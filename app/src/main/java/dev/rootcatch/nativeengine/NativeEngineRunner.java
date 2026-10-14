package dev.rootcatch.nativeengine;

import android.content.Context;
import android.app.KeyguardManager;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.os.PowerManager;
import android.os.SystemClock;

import dev.rootcatch.automation.AutomationConfig;
import dev.rootcatch.root.RootShell;

import org.json.JSONObject;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

/** Local root runtime controlled through pipes. */
public final class NativeEngineRunner implements Runnable {
    public interface Listener {
        void onStatus(String message);
        void onStopped(String reason);
        default void onStatistics(int caught, int fled, int spun, int discarded) { }
        default void onTransferStatistics(int transferred) { }
        default void onRewards(long xp, long stardust, long missing, boolean available) { }
        default void onCleanupPaused(String reason) { }
        default void onInspection(String summary) { }
        default void onTargetProcess(int pid, long startTicks) { }
        default void onGameForeground() { }
    }
    private static final int MAX_LINE = 16384;
    private static final AtomicBoolean SESSION = new AtomicBoolean();
    private final Context context;
    private final RootShell shell;
    private final AutomationConfig config;
    private final boolean readOnly;
    private final boolean diagnosticTrace, encounterOnly;
    private final boolean inspectOwned;
    private final TransferOptions transfers;
    private final String sessionId;
    private final String expectedPlayerId;
    private final boolean automaticPreflight;
    private final ConnectionStrategy connection;
    private final int observeReadOnlyMs;
    private final File diagnosticLogDirectory;
    private final Listener listener;
    private final BlockingQueue<String> lines = new ArrayBlockingQueue<>(256);
    private final Object stopLock = new Object();
    private volatile boolean stopping, overflow, eof, requestedStop;
    private volatile Process process;
    private volatile BufferedWriter control;
    private File logFile;
    private RollingLog log;
    private RollingLog healthLog;
    private FileOutputStream initialLog;
    private long initialLogged;
    private int helperPid = -1, gamePid = -1;
    private String binary;
    private boolean attached, ready, gcAbi, gcValidated, cleanupConfirmed, agentStopped, gameForegroundConfirmed;
    private boolean identityValidated;
    private String validatedPlayerId = "", activationToken = "";
    private boolean preflightAnnounced, activationSent;
    private int expectedGamePid;
    private long expectedGameStart;
    private boolean nativeCrashReported;
    private boolean startupInterrupted, processStarted, transportClosed = true;
    private boolean nativeLocationJNI, nativeLocationReady;
    private boolean nativeLocationCleanupConfirmed = true;
    private File preparedAgent;
    private File preparedGuard, runtimeDirectory;
    private boolean policyGuardConfirmed;
    private final LoaderState loaderState = new LoaderState();
    private String preparedAgentSha;
    private Thread outputReader;
    private volatile ControlPulse controlPulse;
    private long readOnlyStopAt, readyAt;
    private boolean stabilityValidated;
    private long started, lastStatusTime;
    private volatile String ending = "Internal engine stopped";
    private int caught, fled, spun, discarded, transferred;
    private RewardTotals rewards = new RewardTotals();
    private String sessionLimitKind = "";
    private boolean sessionLimitAcknowledged;
    private int priorCaught, priorSpun;
    private boolean emptyMapRecoveryEnabled, emptyMapRecoveryRequested, emptyMapRecoveryAcknowledged, recoveryFailed;
    private long lastPresenceTime;
    private static final String EMPTY_MAP_REASON = "No nearby Pokémon for 5 minutes";
    private final EmptyMapWatchdog emptyMapWatchdog = new EmptyMapWatchdog();
    private static final String BUDDY_RESTART_REASON = "Buddy removed; restarting Pokémon GO";
    private boolean buddyRemovalHandled, buddyRemovalConfirmed, buddyRemovalAcknowledged;

    public void skipBuddyRemoval(boolean handled) {
        if (process != null || attached) throw new IllegalStateException("Session already started");
        buddyRemovalHandled = handled;
    }
    public boolean buddyWasRemoved() { return buddyRemovalConfirmed; }
    public boolean buddyRemovalRecoveryReady() {
        return buddyRemovalConfirmed && buddyRemovalAcknowledged && !requestedStop && !recoveryFailed
                && agentStopped && cleanupConfirmed && nativeLocationCleanupConfirmed
                && !nativeCrashReported && sessionLimitKind.isEmpty() && identityValidated;
    }

    public void accumulatedCounts(int caught, int spun, boolean recoverEmptyMap) {
        if (process != null || attached || caught < 0 || spun < 0
                || caught >= config.maxCatches || spun >= config.maxSpins)
            throw new IllegalArgumentException("Invalid accumulated statistics");
        priorCaught = caught; priorSpun = spun;
        emptyMapRecoveryEnabled = recoverEmptyMap && dev.rootcatch.automation.PokemonSelection.recoverEmptyMap(config.onlyCatch);
    }
    private int captureBudget() { return config.maxCatches - priorCaught; }
    private int spinBudget() { return config.maxSpins - priorSpun; }
    public String playerIdentity() { return validatedPlayerId; }
    public boolean usesEarlySpawn() { return connection.earlySpawn(); }
    public boolean startupRetryCandidate() {
        return loaderState.failure().isEmpty() && !ready && (startupInterrupted || nativeCrashReported);
    }
    public boolean startupTransportClosed() throws Exception {
        if (!transportClosed || process != null || outputReader != null && outputReader.isAlive()) return false;
        if (!processStarted) return true;
        if (helperPid <= 1 || helperPid == gamePid) return false;
        RootShell.Result reply = shell.execute("if [ ! -d /proc/" + helperPid + " ]; then printf 'ABSENT\\n'; "
                + "elif rc_exe=$(readlink /proc/" + helperPid + "/exe); then "
                + "if [ \"$rc_exe\" = " + EnginePolicy.quote(binary) + " ]; then printf 'ACTIVE\\n'; else printf 'REPLACED\\n'; fi; "
                + "elif [ ! -d /proc/" + helperPid + " ]; then printf 'ABSENT\\n'; else printf 'UNVERIFIED\\n'; fi", 2500);
        return reply.code == 0 && ("ABSENT".equals(reply.output) || "REPLACED".equals(reply.output));
    }
    public boolean emptyMapRecoveryReady() {
        return emptyMapRecoveryRequested && emptyMapRecoveryAcknowledged && !recoveryFailed && agentStopped && cleanupConfirmed && nativeLocationCleanupConfirmed
                && !nativeCrashReported && sessionLimitKind.isEmpty() && identityValidated;
    }

    public String sessionLimitReached() {
        return agentStopped && sessionLimitAcknowledged && cleanupConfirmed && nativeLocationCleanupConfirmed
                && (!requestedStop || emptyMapRecoveryRequested) ? sessionLimitKind : "";
    }

    /** Process-local location option, restricted to this edition. */
    public void enableNativeLocation(boolean enabled) {
        if (process != null || attached) throw new IllegalStateException("Session already started");
        if (enabled && (!automaticPreflight || !"dev.rootcatch.ugphonetest".equals(context.getPackageName())))
            throw new IllegalArgumentException("Location treatment requires the automatic engine");
        nativeLocationJNI = enabled;
        nativeLocationCleanupConfirmed = !enabled;
    }

    /** Called before launch: the successful trace must not copy/hash resources after observing a new PID. */
    public void prepareAssets(String expectedAgentSha) throws Exception {
        if (preparedAgent != null) return;
        if (stopping) throw new IllegalStateException("Startup stopped");
        binary = new File(context.getApplicationInfo().nativeLibraryDir, connection.library).getAbsolutePath();
        if (!new File(binary).isFile() || !connection.sha256.equals(hash(new File(binary))))
            throw new IllegalStateException("Injector integrity check failed; reinstall the APK");
        File directory = new File(context.getFilesDir(), "engine");
        if (!directory.exists() && !directory.mkdirs()) throw new IllegalStateException("Could not prepare agent");
        File agent = new File(directory, "agent.js");
        try (InputStream input = context.getAssets().open("engine/agent.js"); FileOutputStream output = new FileOutputStream(agent)) {
            byte[] buffer = new byte[32768]; int count;
            while ((count = input.read(buffer)) != -1) {
                if (stopping) throw new IllegalStateException("Startup stopped");
                output.write(buffer, 0, count);
            }
        }
        String actual = hash(agent);
        if (expectedAgentSha != null && !expectedAgentSha.equals(actual))
            throw new IllegalStateException("Agent integrity check failed");
        if (connection.earlySpawn()) {
            File loader = new File(context.getApplicationInfo().nativeLibraryDir, ConnectionStrategy.LOADER_LIBRARY);
            if (!loader.isFile() || !ConnectionStrategy.LOADER_SHA256.equals(hash(loader)))
                throw new IllegalStateException("Loader integrity check failed; reinstall the APK");
            File guard = new File(directory, "spawn-guard.sh");
            try (InputStream input = context.getAssets().open("engine/spawn-guard.sh");
                 FileOutputStream output = new FileOutputStream(guard)) {
                byte[] bytes = new byte[4096]; int count;
                while ((count = input.read(bytes)) != -1) output.write(bytes, 0, count);
            }
            preparedGuard = guard;
            runtimeDirectory = new File(context.getCacheDir(), "native-runtime");
            if (!runtimeDirectory.exists() && !runtimeDirectory.mkdirs())
                throw new IllegalStateException("Could not prepare runtime directory");
        }
        preparedAgentSha = actual; preparedAgent = agent;
    }

    public static boolean gameFocused(RootShell root) {
        return gameFocused(root, () -> false, ignored -> { });
    }
    public static boolean gameFocused(RootShell root, BooleanSupplier stopping, Consumer<String> observer) {
        return new FocusReader((command, timeout) -> {
            RootShell.Result reply = root.execute(command, timeout);
            return new FocusReader.Reply(reply.code, reply.output);
        }, stopping, sample -> {
            try { observer.accept(focusRecord(sample).toString()); } catch (Exception ignored) { }
        }).read("early_startup").focus.initialAttachAllowed();
    }
    private static JSONObject focusRecord(FocusReader.Sample sample) throws Exception {
        return new JSONObject().put("event", "host_focus").put("source", sample.source)
                .put("state", sample.focus.state.name()).put("reason", sample.focus.reason).put("stage", sample.stage)
                .put("sourceExit", sample.sourceExitCode).put("durationMs", sample.durationMs)
                .put("shellExit", sample.shellExitCode).put("error", sample.error == null ? JSONObject.NULL : sample.error)
                .put("raw", sample.response).put("command", sample.command).put("truncated", sample.truncated)
                .put("displayId", sample.focus.displayId).put("initialAttachAllowed", sample.focus.initialAttachAllowed())
                .put("notificationShade", sample.focus.notificationShadeAllowed()).put("time", System.currentTimeMillis());
    }
    private boolean screenAllowsActions() {
        PowerManager power = context.getSystemService(PowerManager.class);
        KeyguardManager keyguard = context.getSystemService(KeyguardManager.class);
        return power != null && keyguard != null && power.isInteractive() && !keyguard.isKeyguardLocked();
    }

    public NativeEngineRunner(Context context, RootShell shell, AutomationConfig config,
                              boolean readOnly, Listener listener) {
        this(context, shell, config, readOnly, listener, false, false, 0, null);
    }
    public NativeEngineRunner(Context context, RootShell shell, AutomationConfig config, boolean readOnly,
                              Listener listener, boolean trace, boolean encounterOnly, int observeReadOnlyMs, File diagnosticLogDirectory) {
        this(context, shell, config, readOnly, listener, trace, encounterOnly, observeReadOnlyMs, diagnosticLogDirectory, false);
    }
    public NativeEngineRunner(Context context, RootShell shell, AutomationConfig config, boolean readOnly,
                              Listener listener, boolean trace, boolean encounterOnly, int observeReadOnlyMs,
                              File diagnosticLogDirectory, boolean inspectOwned) {
        this(context, shell, config, readOnly, listener, trace, encounterOnly, observeReadOnlyMs,
                diagnosticLogDirectory, inspectOwned, TransferOptions.OFF);
    }
    public NativeEngineRunner(Context context, RootShell shell, AutomationConfig config, boolean readOnly,
                              Listener listener, boolean trace, boolean encounterOnly, int observeReadOnlyMs,
                              File diagnosticLogDirectory, boolean inspectOwned, TransferOptions transfers) {
        this(context, shell, config, readOnly, listener, trace, encounterOnly, observeReadOnlyMs,
                diagnosticLogDirectory, inspectOwned, transfers, "");
    }
    public NativeEngineRunner(Context context, RootShell shell, AutomationConfig config, boolean readOnly,
                              Listener listener, boolean trace, boolean encounterOnly, int observeReadOnlyMs,
                              File diagnosticLogDirectory, boolean inspectOwned, TransferOptions transfers, String expectedPlayerId) {
        this(context, shell, config, readOnly, listener, trace, encounterOnly, observeReadOnlyMs,
                diagnosticLogDirectory, inspectOwned, transfers, expectedPlayerId, false);
    }
    public NativeEngineRunner(Context context, RootShell shell, AutomationConfig config, boolean readOnly,
                              Listener listener, boolean trace, boolean encounterOnly, int observeReadOnlyMs,
                              File diagnosticLogDirectory, boolean inspectOwned, TransferOptions transfers,
                              String expectedPlayerId, boolean automaticPreflight) {
        if (automaticPreflight && (readOnly || encounterOnly)) throw new IllegalArgumentException("Automatic startup requires the full engine");
        this.context = context.getApplicationContext(); this.shell = shell;
        this.config = config; this.readOnly = readOnly; this.listener = listener;
        this.diagnosticTrace = trace; this.encounterOnly = encounterOnly;
        this.observeReadOnlyMs = Math.max(0, Math.min(15_000, observeReadOnlyMs));
        this.diagnosticLogDirectory = diagnosticLogDirectory;
        this.inspectOwned = inspectOwned || transfers.enabled;
        this.transfers = transfers;
        this.sessionId = diagnosticLogDirectory == null ? UUID.randomUUID().toString() : diagnosticLogDirectory.getName();
        this.expectedPlayerId = expectedPlayerId;
        this.automaticPreflight = automaticPreflight;
        this.connection = automaticPreflight ? ConnectionStrategy.forSdk(Build.VERSION.SDK_INT) : ConnectionStrategy.ATTACH;
    }

    /** Optional binding for a new host; legacy manual-test callers are unchanged. */
    public void expectProcess(int pid, long startTicks) {
        if (started != 0 || process != null || pid <= 1 || startTicks <= 0)
            throw new IllegalArgumentException("Invalid process identity");
        expectedGamePid = pid; expectedGameStart = startTicks;
    }

    public void requestStop() {
        requestStop("Stopped by user");
    }
    public void requestStop(String reason) {
        synchronized (stopLock) {
            if (stopping) return;
            requestedStop = true; ending = reason; stopping = true;
        }
        // Never block the service's main thread on a pipe or a root command.
        Thread stopper = new Thread(() -> {
            try { command(new JSONObject().put("type", "stop").put("reason", reason)); }
            catch (Exception ignored) { }
            closeControl();
        }, "rootcatch-native-stop");
        stopper.setDaemon(true); stopper.start();
    }

    @Override public void run() {
        if (!SESSION.compareAndSet(false, true)) {
            listener.onStopped("The previous internal session is closing; wait a few seconds");
            return;
        }
        try {
            File logs = diagnosticLogDirectory != null ? diagnosticLogDirectory : new File(context.getFilesDir(), "engine-logs");
            if (!logs.exists() && !logs.mkdirs()) throw new IllegalStateException("Could not create log");
            // Keep just the latest session; export it before starting another run.
            logFile = new File(logs, "latest.jsonl");
            log = new RollingLog(logs, "latest.jsonl", 4L * 1024 * 1024, 2);
            healthLog = new RollingLog(logs, "health.jsonl", 256L * 1024, 2);
            initialLog = new FileOutputStream(new File(logs, "engine-start.jsonl"));
            record(new JSONObject().put("event", "host_start").put("readOnly", readOnly)
                    .put("automaticPreflight", automaticPreflight).put("singleAttach", automaticPreflight)
                    .put("diagnosticTrace", diagnosticTrace).put("encounterOnly", encounterOnly).put("observeReadOnlyMs", observeReadOnlyMs)
                    .put("android", Build.VERSION.RELEASE).put("package", config.gamePackage).toString());
            if (!Arrays.asList(Build.SUPPORTED_ABIS).contains("arm64-v8a"))
                throw new IllegalStateException("The internal engine requires ARM64");
            PackageInfo game = context.getPackageManager().getPackageInfo(config.gamePackage, 0);
            if (!EnginePolicy.supports(config.gamePackage, game.versionName, game.getLongVersionCode()))
                throw new IllegalStateException("Supported version: GO " + EnginePolicy.VERSION + " / " + EnginePolicy.VERSION_CODE);
            Map<String, Integer> limits = EnginePolicy.parseLimits(config.discardLimits);
            if (config.cleanBag && limits.isEmpty()) throw new IllegalStateException("Set item limits before cleanup");
            JSONObject params = new JSONObject().put("gamePackage", config.gamePackage).put("sessionId", sessionId)
                    .put("expectedPlayerId", expectedPlayerId)
                    .put("mode", "FARMER")
                    .put("version", EnginePolicy.VERSION).put("autoCatch", config.autoCatch)
                    .put("onlyCatch", new org.json.JSONArray(config.onlyCatch))
                    .put("emptyMapRecoveryEnabled", emptyMapRecoveryEnabled)
                    .put("autoSpin", config.autoSpin).put("cleanBag", config.cleanBag)
                    .put("removeBuddy", config.removeBuddy && !buddyRemovalHandled)
                    .put("readOnly", readOnly).put("maxThrows", config.maxThrows)
                    .put("throwQuality", config.throwQuality).put("notificationShadeSupport", true)
                    .put("useBerries", config.useBerries).put("berryOrder", new org.json.JSONArray(config.berryOrder))
                    .put("skipInterruptions", config.skipInterruptions)
                    .put("showIvNames", config.showIvNames).put("extendedManualTransfer", config.extendedManualTransfer)
                    .put("fieldResearch", config.fieldResearch).put("discardUnsupportedResearch", config.discardUnsupportedResearch)
                    .put("maxCatches", captureBudget()).put("maxSpins", spinBudget())
                    .put("automaticPreflight", automaticPreflight)
                    .put("nativeLocationJNI", false).put("edition", "ugphone")
                    .put("resolveError12", nativeLocationJNI).put("sdk", Build.VERSION.SDK_INT)
                    .put("diagnosticTrace", diagnosticTrace).put("encounterOnly", encounterOnly)
                    .put("activeTrace", false)
                    .put("inspectOwned", inspectOwned)
                    .put("transferEnabled", transfers.enabled).put("keepShiny", transfers.keepShiny)
                    .put("keepHundo", transfers.keepHundo).put("maxTransfers", transfers.maximum)
                    .put("keepDynamax", transfers.keepDynamax).put("keepHatched", transfers.keepHatched)
                    .put("keepBackground", transfers.keepBackground).put("keepShinyBackground", transfers.keepShinyBackground)
                    .put("keepFieldResearch", transfers.keepFieldResearch)
                    .put("maxDiscardedItems", config.maxDiscardedItems)
                    .put("cleanupTrigger", EnginePolicy.CLEANUP_TRIGGER)
                    .put("actionDelayMs", config.actionDelayMs).put("discard", new JSONObject(limits))
                    .put("sessionAccounting", true).put("controlRecovery", true);
            record(new JSONObject().put("event", "configuration").put("parameters", params).toString());
            prepareAssets(null);
            File agent = preparedAgent;
            record(new JSONObject().put("event", "integrity").put("helperSha256", connection.sha256)
                    .put("connection", connection.name()).put("runtimeVersion", connection.version)
                    .put("loaderAdapter", connection.earlySpawn() ? LoaderState.ADAPTER : "none")
                    .put("loaderSha256", connection.earlySpawn() ? ConnectionStrategy.LOADER_SHA256 : JSONObject.NULL)
                    .put("agentSha256", preparedAgentSha).toString());
            FocusLogPolicy focusLogs = new FocusLogPolicy();
            FocusReader focusReader = new FocusReader((command, timeout) -> {
                RootShell.Result reply = shell.execute(command, timeout);
                return new FocusReader.Reply(reply.code, reply.output);
            }, () -> stopping, sample -> {
                try { if (focusLogs.record(sample, SystemClock.elapsedRealtime(), ready && !readOnly))
                    record(focusRecord(sample).toString()); }
                catch (Exception ignored) { }
            });
            started = SystemClock.elapsedRealtime();
            if (expectedGamePid > 1) gamePid = expectedGamePid;
            listener.onStatus("Connecting SilphMod…");
            while (!connection.earlySpawn() && !stopping && SystemClock.elapsedRealtime() - started < 90000) {
                if (gamePid > 1) break;
                if (focusReader.read("before_attach").focus.initialAttachAllowed()) {
                    gamePid = EnginePolicy.uniquePid(shell.execute("pidof " + EnginePolicy.GAME, 3000).output);
                    if (gamePid > 1) break;
                }
                Thread.sleep(700);
            }
            if (stopping) return;
            if (!connection.earlySpawn()) {
                if (gamePid <= 1) throw new IllegalStateException("No foreground game main process found");
                verifyGameIdentity(gamePid);
            } else {
                if (expectedGamePid > 1) throw new IllegalStateException("Spawn requires a new game process");
                RootShell.Result absent = shell.execute("pidof " + EnginePolicy.GAME, 2500);
                if (!(absent.code == 0 || absent.code == 1) || !absent.output.isEmpty())
                    throw new IllegalStateException("Game startup changed before connecting");
            }
            if (expectedGamePid > 1) {
                RootShell.Result stat = shell.execute("cat /proc/" + gamePid + "/stat", 2500);
                if (gamePid != expectedGamePid || stat.code != 0 || !StartupProof.sameProcess(stat.output, expectedGameStart))
                    throw new IllegalStateException("The process changed before connecting");
            }
            listener.onStatus(readOnly ? "Checking engine (read-only)…" : "Connecting internal engine…");
            record(new JSONObject().put("event", connection.earlySpawn() ? "host_spawn_start" : "host_attach_start").put("pid", gamePid)
                    .put("time", System.currentTimeMillis()).put("elapsedMs", SystemClock.elapsedRealtime())
                    .put("readOnly", readOnly).put("inspectOwned", inspectOwned).toString());
            String launchCommand = connection.earlySpawn()
                    ? connection.spawnCommand(binary, preparedGuard.getAbsolutePath(), agent.getAbsolutePath(),
                            runtimeDirectory.getAbsolutePath(), params.toString())
                    : EnginePolicy.attachCommand(binary, agent.getAbsolutePath(), gamePid, params.toString());
            Process local = new ProcessBuilder("su", "-c", launchCommand).redirectErrorStream(true).start();
            process = local;
            processStarted = true; transportClosed = false;
            control = new BufferedWriter(new OutputStreamWriter(local.getOutputStream(), StandardCharsets.UTF_8));
            Thread reader = new Thread(() -> readLines(local), "rootcatch-native-reader");
            outputReader = reader;
            reader.setDaemon(true); reader.start();
            ControlPulse pulses = new ControlPulse(SystemClock::elapsedRealtime, pulse ->
                    command(new JSONObject().put("type", "heartbeat").put("foreground", pulse.foreground)
                            .put("notificationShade", pulse.notificationShade)
                            .put("sequence", pulse.sequence).put("sentAtElapsedMs", pulse.sentAt)), (pulse, writeMs) -> {
                try {
                    // Nonessential diagnostics may be skipped if the output queue is full;
                    // never block the control worker on storage or on log consumption.
                    lines.offer(new JSONObject().put("event", "host_control_pulse").put("time", System.currentTimeMillis())
                            .put("elapsedMs", SystemClock.elapsedRealtime()).put("sequence", pulse.sequence)
                            .put("foreground", pulse.foreground).put("focusFresh", pulse.focusFresh)
                            .put("notificationShade", pulse.notificationShade)
                            .put("focusAgeMs", pulse.focusAgeMs).put("gapMs", pulse.gapMs)
                            .put("maxGapMs", pulse.maxGapMs).put("writeMs", writeMs).toString());
                } catch (Exception ignored) { }
            });
            controlPulse = pulses; pulses.start();
            started = SystemClock.elapsedRealtime();
            long nextFocus = 0;
            boolean previousFocus = false;
            while (!stopping) {
                long now = SystemClock.elapsedRealtime();
                if (overflow) throw new IllegalStateException("Agent output exceeded limit; stopping");
                if (pulses.failure() != null) {
                    startupInterrupted = !ready;
                    throw new IllegalStateException("Control transport failed", pulses.failure());
                }
                if (now >= nextFocus) {
                    long focusStarted = SystemClock.elapsedRealtime();
                    ForegroundFocus observedFocus = focusReader.read("focus_sample").focus;
                    boolean observedGame = observedFocus.isGame() && screenAllowsActions();
                    if (stopping) break;
                    if (attached && observedGame && observedFocus.initialAttachAllowed() && !gameForegroundConfirmed) {
                        gameForegroundConfirmed = true;
                        listener.onGameForeground();
                    }
                    pulses.publishFocus(observedGame, observedFocus.notificationShadeAllowed(), focusStarted);
                    boolean focus = pulses.focusAllowed();
                    if (!focus) emptyMapWatchdog.pause();
                    nextFocus = focusStarted + FocusLogPolicy.QUERY_INTERVAL_MS;
                    if (previousFocus && !focus) listener.onStatus("Internal paused: game not in foreground");
                    if (!previousFocus && focus && ready) listener.onStatus("Internal resumed: game in foreground");
                    previousFocus = focus;
                }
                String line = lines.poll(100, TimeUnit.MILLISECONDS);
                if (line != null) handle(line);
                // Drain bounded bursts without starving heartbeat or Stop.
                for (int i = 0; i < 31 && !stopping; i++) {
                    line = lines.poll(); if (line == null) break; handle(line);
                }
                if (eof && lines.isEmpty() && !stopping) {
                    startupInterrupted = !ready;
                    record(new JSONObject().put("event", "host_transport_exit").put("attached", attached)
                            .put("ready", ready).put("loaderStage", loaderState.stage())
                            .put("loaderAdapted", loaderState.adapted()).put("loaderErrno", loaderState.error())
                            .put("helperExit", local.isAlive() ? JSONObject.NULL : local.exitValue())
                            .put("time", System.currentTimeMillis()).toString());
                    throw new IllegalStateException(LoaderState.connectionEnded(attached, nativeCrashReported,
                            connection.earlySpawn(), loaderState.ready()));
                }
                if (!ready && now - started > (automaticPreflight ? 180000 : 45000)) {
                    startupInterrupted = true;
                    throw new IllegalStateException("Could not validate map/services; export logs");
                }
                if (readOnly && ready && readOnlyStopAt == 0 && now - readyAt >= observeReadOnlyMs) {
                    if (diagnosticTrace && observeReadOnlyMs >= 10_000 && !stabilityValidated)
                        throw new IllegalStateException("Insufficient stable readings with the map visible; actions disabled");
                    readOnlyStopAt = SystemClock.elapsedRealtime();
                    listener.onStatus("Read-only validated; checking reference release…");
                    command(new JSONObject().put("type", "stop").put("reason", "Read-only complete; releasing references"));
                }
                if (readOnlyStopAt > 0 && !stopping && now - readOnlyStopAt > 5000)
                    throw new IllegalStateException("Reference release not confirmed within 5 s; actions disabled");
            }
        } catch (InterruptedException e) {
            recoveryFailed = true;
            Thread.currentThread().interrupt(); ending = "Stopped by user";
        } catch (Exception e) {
            boolean expected = EnginePolicy.expectedPipeClose(requestedStop, e);
            if (!expected) recoveryFailed = true;
            if (!expected) ending = "Internal engine: " + e.getMessage();
            try { record(new JSONObject().put("event", expected ? "host_stop_transport_closed" : "host_error")
                    .put("message", e.toString()).put("reason", ending).toString()); }
            catch (Exception ignored) { }
        } finally {
            stopping = true;
            ControlPulse pulses = controlPulse;
            if (pulses != null) pulses.close();
            shutdown();
            if (pulses != null) {
                pulses.close();
                try { record(new JSONObject().put("event", "host_control_end").put("time", System.currentTimeMillis())
                        .put("workerFinished", !pulses.workerAlive())
                        .put("lastSequence", pulses.lastPulse() == null ? 0 : pulses.lastPulse().sequence)
                        .put("maxGapMs", pulses.lastPulse() == null ? 0 : pulses.lastPulse().maxGapMs)
                        .put("failure", pulses.failure() == null ? JSONObject.NULL : pulses.failure().toString()).toString()); }
                catch (Exception ignored) { }
            }
            try { record(new JSONObject().put("event", "host_stop").put("reason", ending)
                    .put("cleanupConfirmed", cleanupConfirmed).put("agentStopped", agentStopped)
                    .put("resolveError12", nativeLocationJNI).put("locationCleanupConfirmed", nativeLocationCleanupConfirmed)
                    .put("loaderStage", loaderState.stage()).put("loaderAdapted", loaderState.adapted())
                    .put("loaderErrno", loaderState.error()).put("loaderOriginalErrno", loaderState.originalError())
                    .put("caught", caught).put("fled", fled).put("spun", spun).put("discarded", discarded)
                    .put("transferred", transferred)
                    .put("priorCaught", priorCaught).put("priorSpun", priorSpun)
                    .put("emptyMapRecoveryRequested", emptyMapRecoveryRequested)
                    .put("emptyMapRecoveryAcknowledged", emptyMapRecoveryAcknowledged).put("recoveryFailed", recoveryFailed)
                    .put("xp", rewards.xp).put("stardust", rewards.stardust)
                    .put("missingRewardReceipts", rewards.missing).put("rewardsAvailable", rewards.available)
                    .put("logRotations", log == null ? 0 : log.rotations()).toString());
            } catch (Exception ignored) { }
            try { if (log != null) log.close(); } catch (Exception ignored) { }
            try { if (healthLog != null) healthLog.close(); } catch (Exception ignored) { }
            try { if (initialLog != null) initialLog.close(); } catch (Exception ignored) { }
            SESSION.set(false);
            listener.onStopped(ending);
        }
    }

    private synchronized void command(JSONObject json) throws Exception {
        if (stopping && !"stop".equals(json.optString("type"))) return;
        BufferedWriter writer = control;
        if (writer == null) return;
        json.put("sessionId", sessionId);
        writer.write(json.toString()); writer.newLine(); writer.flush();
    }

    private synchronized void closeControl() {
        BufferedWriter writer = control; control = null;
        if (writer != null) try { writer.close(); } catch (Exception ignored) { }
    }

    private void statistics(JSONObject payload) {
        int confirmedCaught = payload.optInt("caught", caught);
        if (confirmedCaught > caught) emptyMapWatchdog.pokemonObserved(SystemClock.elapsedRealtime());
        caught = confirmedCaught; fled = payload.optInt("fled", fled);
        spun = payload.optInt("spun", spun); discarded = payload.optInt("discarded", discarded);
        listener.onStatistics(caught, fled, spun, discarded);
        transferred = payload.optInt("transferred", transferred);
        listener.onTransferStatistics(transferred);
        if (payload.has("xp")) {
            rewards = rewards.update(payload.opt("xp"), payload.opt("stardust"), payload.opt("missingRewardReceipts"),
                    payload.optBoolean("rewardsAvailable"));
            listener.onRewards(rewards.xp, rewards.stardust, rewards.missing, rewards.available);
        }
    }

    private void handle(String line) throws Exception {
        record(line);
        if ("SILPH_POLICY_WRITE_GUARD=isolated_mount_namespace".equals(line)) {
            policyGuardConfirmed = true; return;
        }
        if (line.startsWith("ROOTCATCH_HELPER_PID=")) {
            helperPid = EnginePolicy.uniquePid(line.substring("ROOTCATCH_HELPER_PID=".length())); return;
        }
        if (line.startsWith("Process crashed:")) { nativeCrashReported = true; return; }
        if (!line.startsWith("{")) return;
        JSONObject message;
        try { message = new JSONObject(line); } catch (Exception ignored) { return; }
        if ("host_loader".equals(message.optString("event"))) {
            if (!connection.earlySpawn() || !policyGuardConfirmed
                    || !loaderState.observe(message.optString("adapter"), message.optString("stage"),
                    message.optInt("errno", -1), message.optInt("originalErrno", -1),
                    message.optInt("pid", -1), helperPid))
                throw new IllegalStateException("Invalid loader confirmation");
            if (!loaderState.failure().isEmpty()) throw new IllegalStateException(loaderState.failure());
            return;
        }
        if ("error".equals(message.optString("type")))
            throw new IllegalStateException(message.optString("description", message.toString()));
        if (!"send".equals(message.optString("type"))) return;
        JSONObject payload = message.optJSONObject("payload");
        if (payload == null || payload.optInt("protocol") != 1) return;
        String event = payload.optString("event");
        if ("attached".equals(event)) {
            if (connection.earlySpawn() && !attached) {
                if (!policyGuardConfirmed) throw new IllegalStateException("Runtime isolation not confirmed");
                if (!loaderState.ready()) throw new IllegalStateException("Runtime loader not confirmed");
                int observedPid = payload.optInt("pid", -1);
                long observedTicks = verifyGameIdentity(observedPid);
                gamePid = observedPid; expectedGamePid = observedPid; expectedGameStart = observedTicks;
                listener.onTargetProcess(observedPid, observedTicks);
                record(new JSONObject().put("event", "host_spawn_bound").put("pid", gamePid)
                        .put("startTicks", observedTicks).put("policyGuardConfirmed", true).toString());
            }
            if (payload.optInt("pid") != gamePid || !"arm64".equals(payload.optString("arch")))
                throw new IllegalStateException("Process or architecture mismatch");
            attached = true;
        } else if ("native_location_ready".equals(event)) {
            if (!nativeLocationJNI || !attached || nativeLocationReady
                    || !"rootcatch-profile7-v2".equals(payload.optString("implementation"))
                    || payload.optJSONArray("methods") == null
                    || !payload.optJSONArray("methods").toString().contains("\"isFromMockProvider\"")
                    || payload.optInt("sdk", -1) != Build.VERSION.SDK_INT
                    || Build.VERSION.SDK_INT >= 31 && !payload.optJSONArray("methods").toString().contains("\"isMock\"")
                    || payload.optInt("hookCount") < 1 || payload.optInt("hookCount") > 3
                    || payload.optBoolean("coordinateMutation", true) || payload.optBoolean("systemMutation", true))
                throw new IllegalStateException("JNI treatment not validated; stopping");
            nativeLocationReady = true;
            listener.onStatus("Location treatment active · preparing engine…");
        } else if ("native_location_cleanup".equals(event)) {
            nativeLocationCleanupConfirmed = locationCleanup(payload);
            if (!nativeLocationCleanupConfirmed) throw new IllegalStateException("Location cleanup unconfirmed");
        } else if ("gc_abi".equals(event)) {
            gcAbi = payload.optInt("handleBits") == 64 && payload.optInt("pointerSize") == 8
                    && "rootcatch-gc64".equals(payload.optString("adapter"));
            if (!gcAbi) throw new IllegalStateException("Incompatible reference ABI");
        } else if ("gc_validation".equals(event)) {
            gcValidated = gcAbi && gcValidation(payload);
            if (!gcValidated) throw new IllegalStateException("64-bit references not validated");
        } else if ("session_identity".equals(event)) {
            String player = payload.optString("playerId");
            identityValidated = player.matches("[^\\x00-\\x20\\x7f]{1,256}") && payload.optBoolean("passed")
                    && payload.optInt("gameRequestsSent", -1) == 0
                    && (expectedPlayerId.isEmpty() || expectedPlayerId.equals(player));
            if (!identityValidated) throw new IllegalStateException("Player identity differs from preflight");
            if (!validatedPlayerId.isEmpty() && !validatedPlayerId.equals(player))
                throw new IllegalStateException("Player identity changed during startup");
            validatedPlayerId = player;
            healthLog.write(payload.toString());
        } else if ("target_selected".equals(event) || "encounter".equals(event) || "capture".equals(event)) {
            long observedAt = payload.optLong("time", 0);
            double distance = payload.optDouble("distanceMeters", Double.NaN);
            boolean targetInRange = !"target_selected".equals(event)
                    || Double.isFinite(distance) && distance >= 0 && distance <= 40;
            if (ready && !readOnly && !requestedStop && targetInRange
                    && payload.optString("id").matches("[0-9]{1,20}")
                    && Math.abs(System.currentTimeMillis() - observedAt) <= 10_000)
                emptyMapWatchdog.pokemonObserved(SystemClock.elapsedRealtime());
        } else if ("map_presence".equals(event)) {
            ControlPulse pulses = controlPulse;
            boolean focused = pulses != null && pulses.focusAllowed();
            long observedAt = payload.optLong("time", 0);
            boolean fresh = observedAt > lastPresenceTime && Math.abs(System.currentTimeMillis() - observedAt) <= 10_000;
            lastPresenceTime = Math.max(lastPresenceTime, observedAt);
            if (!payload.optBoolean("active") || !fresh) emptyMapWatchdog.pause();
            if (emptyMapRecoveryEnabled && ready && !readOnly && !requestedStop && sessionLimitKind.isEmpty()
                    && fresh && payload.optInt("radiusMeters", -1) == 40
                    && emptyMapWatchdog.observe(SystemClock.elapsedRealtime(), payload.optInt("nearby", -1),
                    payload.optBoolean("complete") && payload.optBoolean("active"), focused)) {
                emptyMapRecoveryRequested = true;
                record(new JSONObject().put("event", "empty_map_recovery_requested").put("emptyMs", EmptyMapWatchdog.EMPTY_MS)
                        .put("pid", gamePid).put("playerId", validatedPlayerId).toString());
                listener.onStatus("Restarting Pokémon GO: no nearby Pokémon");
                requestStop(EMPTY_MAP_REASON);
            }
        } else if ("buddy_removal_request".equals(event)) {
            if (config.removeBuddy && !buddyRemovalHandled && !requestedStop)
                listener.onStatus("Removing buddy…");
        } else if ("buddy_removed".equals(event)) {
            if (!ready || !identityValidated || !config.removeBuddy || buddyRemovalHandled
                    || !payload.optBoolean("confirmed") || !"0".equals(payload.optString("pokemonId"))
                    || payload.optBoolean("transferred", true) || payload.optBoolean("restoreOnStop", true)
                    || !payload.optBoolean("restartRequired"))
                throw new IllegalStateException("Invalid buddy removal confirmation");
            buddyRemovalConfirmed = true;
            listener.onStatus("Removing buddy…");
        } else if ("buddy_removal_failed".equals(event)) {
            listener.onStatus("Buddy removal failed · farming continues");
        } else if ("health".equals(event)) {
            healthLog.write(payload.toString());
            statistics(payload);
        } else if ("reward_profile".equals(event)) {
            rewards = rewards.update(rewards.xp, rewards.stardust, rewards.missing, payload.optBoolean("available"));
            listener.onRewards(rewards.xp, rewards.stardust, rewards.missing, rewards.available);
        } else if ("reward_receipt".equals(event) || "reward_unavailable".equals(event)) {
            statistics(payload);
        } else if ("cleanup_paused".equals(event)) {
            listener.onCleanupPaused(payload.optString("reason", "unconfirmed_response"));
            listener.onStatus("Cleanup paused: unconfirmed response; other features continue");
        } else if ("control_delay".equals(event)) {
            listener.onStatus("Internal paused: delayed control");
        } else if ("control_recovered".equals(event)) {
            listener.onStatus(payload.optBoolean("foregroundAllowed") && ready
                    ? "Internal resumed: control restored and fresh focus" : "Internal paused: waiting for game focus");
        } else if ("capture_paused".equals(event)) {
            listener.onStatus("pokemon_storage_full".equals(payload.optString("reason"))
                    ? "Catches paused: Pokémon storage " + payload.optInt("pokemonCount") + "/" + payload.optInt("maxPokemon")
                            + "; existing collection unchanged"
                    : "Catches paused: no ordinary Poké Balls; spinning and cleanup remain available");
        } else if ("capture_resumed".equals(event)) {
            listener.onStatus("Catches resumed: searching near the player");
        } else if ("session_limit".equals(event)) {
            if (!ready || readOnly || !"SESSION".equals(payload.optString("scope"))
                    || !payload.optBoolean("stopSession") || !payload.optBoolean("closeGameRequested")
                    || payload.optInt("maxCatches") != captureBudget() || payload.optInt("maxSpins") != spinBudget()
                    || !dev.rootcatch.automation.SessionLimits.reached(payload.optString("kind"),
                    payload.optInt("caught", -1), payload.optInt("spun", -1), captureBudget(), spinBudget(),
                    config.autoCatch, config.autoSpin)) throw new IllegalStateException("Invalid session limit completion");
            sessionLimitKind = payload.optString("kind");
            listener.onStatus(sessionLimitStatus(payload));
        } else if ("ready".equals(event)) {
            if (nativeLocationJNI && !nativeLocationReady)
                throw new IllegalStateException("Engine did not confirm the JNI location connection");
            if (!attached || !gcValidated || !identityValidated) throw new IllegalStateException("Incomplete handshake: references/player not validated");
            if (automaticPreflight && ready) throw new IllegalStateException("Duplicate activation");
            if (automaticPreflight && "PREFLIGHT".equals(payload.optString("mode"))) {
                if (preflightAnnounced || ready) throw new IllegalStateException("Duplicate startup");
                preflightAnnounced = true; listener.onStatus("Checking engine…"); return;
            }
            if (automaticPreflight && (!"INTERNAL".equals(payload.optString("mode")) || !activationSent || !payload.optBoolean("automaticPreflightPassed")
                    || !payload.optBoolean("singleAttach") || !sessionId.equals(payload.optString("sessionId"))
                    || !activationToken.equals(payload.optString("activationToken"))
                    || !validatedPlayerId.equals(payload.optString("playerId")) || payload.optInt("gameRequestsSent", -1) != 0))
                throw new IllegalStateException("Automatic activation not validated; stopping");
            ready = true; readyAt = SystemClock.elapsedRealtime();
            listener.onStatus(readOnly ? "Engine recognized (read-only)" : "Internal engine active");
        } else if ("preflight_validated".equals(event)) {
            if (!automaticPreflight || !preflightAnnounced || !stabilityValidated || ready || !attached || !gcValidated
                    || !identityValidated || !StartupProof.valid(sessionId, gamePid, validatedPlayerId,
                    payload.optString("sessionId"), payload.optInt("pid"), payload.optString("playerId"),
                    payload.optString("token"), payload.optInt("samples"), payload.optLong("elapsedMs"),
                    payload.optInt("cachedReferences"), payload.optInt("gameRequestsSent", -1),
                    payload.optBoolean("automatic"), payload.optBoolean("singleAttach")))
                throw new IllegalStateException("Automatic preflight incomplete; stopping");
            activationToken = payload.optString("token"); activationSent = true;
            command(new JSONObject().put("type", "activate").put("token", activationToken)
                    .put("sessionId", sessionId).put("playerId", validatedPlayerId));
        } else if ("activation_rejected".equals(event) && automaticPreflight) {
            if (ready) throw new IllegalStateException("Inconsistent activation state");
            activationSent = false; activationToken = "";
            listener.onStatus("Repeating map check…");
        } else if ("read_stability".equals(event)) {
            stabilityValidated = EnginePolicy.readStabilityValid(payload.optBoolean("passed"), payload.optInt("samples"),
                    payload.optLong("elapsedMs"), payload.optInt("cachedReferences"));
        } else if ("gc_cleanup".equals(event)) {
            cleanupConfirmed = gcCleanup(payload);
            if (!cleanupConfirmed) throw new IllegalStateException("Reference release unconfirmed: " + payload);
        } else if ("error".equals(event)) {
            recoveryFailed = true;
            String stage = payload.optString("stage");
            throw new IllegalStateException((stage.isEmpty() ? "" : "[" + stage + "] ") + payload.optString("message"));
        } else if ("stopped".equals(event)) {
            agentStopped = true;
            buddyRemovalAcknowledged = buddyRemovalConfirmed && BUDDY_RESTART_REASON.equals(payload.optString("reason"))
                    && payload.optString("limitStopKind").isEmpty();
            emptyMapRecoveryAcknowledged = emptyMapRecoveryRequested && EMPTY_MAP_REASON.equals(payload.optString("reason"))
                    && payload.optString("limitStopKind").isEmpty();
            if (!cleanupConfirmed || !nativeLocationCleanupConfirmed || !payload.optBoolean("cleanupPassed"))
                throw new IllegalStateException("Agent ended without confirming reference release");
            statistics(payload);
            if (!sessionLimitKind.equals(payload.optString("limitStopKind")))
                throw new IllegalStateException("Session limit stop was not acknowledged");
            sessionLimitAcknowledged = !sessionLimitKind.isEmpty()
                    && dev.rootcatch.automation.SessionLimits.reached(sessionLimitKind, caught, spun,
                    captureBudget(), spinBudget(), config.autoCatch, config.autoSpin);
            ending = readOnly && ready && readOnlyStopAt > 0
                    ? "Check passed. Export logs; no actions sent"
                    : payload.optString("reason", "Agent stopped");
            stopping = true;
        } else if ("stats".equals(event)) {
            statistics(payload);
            if (SystemClock.elapsedRealtime() - lastStatusTime >= 1800) {
                String limits = sessionLimitStatus(payload);
                listener.onStatus(limits.isEmpty() ? "Internal · catches " + caught + " · spins " + spun + " · items " + discarded : limits);
                lastStatusTime = SystemClock.elapsedRealtime();
            }
        } else if ("bag_full".equals(event) || "no_balls".equals(event)) {
            listener.onStatus("bag_full".equals(event)
                    ? "Item bag full: spins limited, catches remain available; check cleanup"
                    : "No Poké Balls available");
        } else if ("bag_cleanup_blocked".equals(event)) {
            listener.onStatus("Bag full WITHOUT configured surplus: check item limits; nothing deleted");
        } else if ("inventory_check".equals(event) && payload.optBoolean("startup")) {
            listener.onStatus("Initial cleanup · checking per-type surplus · items " + discarded);
        } else if ("inventory_startup_complete".equals(event)) {
            listener.onStatus("quota_reached".equals(payload.optString("reason"))
                    ? "Initial cleanup: quota reached; surplus may remain"
                    : "Initial cleanup finished: selected types within their limits");
        } else if ("discard_limit".equals(event)) {
            listener.onStatus("TOTAL cleanup quota reached (" + payload.optInt("maximum") + "); catches/spins continue");
        } else if ("pokemon_inspection".equals(event)) {
            String decision = payload.optString("decision", "unknown");
            String label = transfers.enabled && !readOnly ? "READ" : "retain".equals(decision) ? "KEEP" : "review".equals(decision) ? "REVIEW" : "NO DATA";
            listener.onInspection(("sample".equals(payload.optString("origin")) ? "Sample" : "Caught")
                    + " #" + payload.optInt("dexId") + " · IV " + payload.optInt("attack", -1) + "/"
                    + payload.optInt("defense", -1) + "/" + payload.optInt("stamina", -1)
                    + " · shiny " + (payload.has("shiny") ? payload.optBoolean("shiny") ? "yes" : "no" : "?")
                    + " · " + label + (transfers.enabled && !readOnly ? " · protections pending" : " · SIMULATION, no transfer"));
        } else if ("transfer_decision".equals(event)) {
            listener.onInspection("Caught #" + payload.optInt("dexId") + " · "
                    + (payload.optBoolean("transferAllowed") ? "Current-session CANDIDATE" : "KEEP / NOT authorized")
                    + " · " + payload.optJSONArray("reasons"));
        } else if ("transfer".equals(event) && payload.optBoolean("confirmed")) {
            listener.onInspection("Transfer confirmed · ID " + payload.optString("id") + " · current-session catch only");
        } else if ("transfer_limit".equals(event)) {
            listener.onStatus("Transfer quota reached (" + payload.optInt("limit") + "); catches and spins continue");
        } else if ("transfer_scope".equals(event)) {
            listener.onStatus("Queue empty: current-session catches only; no collection scan");
        } else if ("capture_scope_result".equals(event) && !payload.optBoolean("accepted")) {
            listener.onInspection("Catch NOT authorized for transfer · " + payload.optString("reason"));
        }
    }

    private long verifyGameIdentity(int pid) throws Exception {
        if (pid <= 1) throw new IllegalStateException("Invalid game process");
        long until = SystemClock.elapsedRealtime() + (connection.earlySpawn() ? 4000 : 0);
        long observedTicks = 0;
        do {
            RootShell.Result identity = shell.execute("tr '\\000' '\\n' </proc/" + pid + "/cmdline", 2500);
            RootShell.Result stat = shell.execute("cat /proc/" + pid + "/stat", 2500);
            record(new JSONObject().put("event", "host_process_identity").put("pid", pid)
                    .put("cmdlineExit", identity.code).put("cmdline", identity.output)
                    .put("statExit", stat.code).put("stat", stat.output).toString());
            if (stat.code != 0) throw new IllegalStateException("Game process identity unavailable");
            long ticks = StartupProof.processStartTicks(stat.output);
            if (observedTicks != 0 && observedTicks != ticks)
                throw new IllegalStateException("Game process changed during startup");
            observedTicks = ticks;
            if (identity.code == 0 && EnginePolicy.GAME.equals(identity.output.split("\\R", -1)[0])) return ticks;
            if (!connection.earlySpawn() || stopping) break;
            Thread.sleep(150);
        } while (SystemClock.elapsedRealtime() < until);
        throw new IllegalStateException("PID does not belong to the game");
    }

    private static boolean gcValidation(JSONObject payload) {
        return EnginePolicy.gcValidationValid(payload.optBoolean("passed"), payload.optInt("handleBits"),
                payload.optInt("cachedReferences"), payload.optInt("roundTrips"),
                payload.optBoolean("strongProbe"), payload.optBoolean("weakProbe"), payload.optBoolean("probeFreed"));
    }
    private static boolean locationCleanup(JSONObject payload) {
        return payload.optBoolean("passed") && payload.optInt("remainingHooks", -1) == 0
                && payload.optInt("remainingClassRefs", -1) == 0;
    }

    private static String sessionLimitStatus(JSONObject payload) {
        return dev.rootcatch.automation.SessionLimits.status(payload.optBoolean("catchLimitReached"), payload.optBoolean("spinLimitReached"));
    }

    private static boolean gcCleanup(JSONObject payload) {
        return EnginePolicy.gcCleanupValid(payload.optBoolean("passed"), payload.optInt("handleBits"),
                payload.optInt("remaining", -1));
    }

    private void readLines(Process local) {
        // readLine() would allocate an unbounded line. Enforce a hard cap instead.
        try (InputStreamReader input = new InputStreamReader(local.getInputStream(), StandardCharsets.UTF_8)) {
            StringBuilder line = new StringBuilder(); int value;
            while ((value = input.read()) != -1) {
                if (value == '\n') {
                    if (!lines.offer(line.toString())) { overflow = true; break; }
                    line.setLength(0);
                } else if (value != '\r') {
                    if (line.length() >= MAX_LINE) { overflow = true; break; }
                    line.append((char)value);
                }
            }
            if (line.length() > 0 && !lines.offer(line.toString())) overflow = true;
        } catch (Exception ignored) { }
        finally { eof = true; }
    }

    private void shutdown() {
        Process local = process;
        if (local == null) return;
        // Clear interruption only for bounded cleanup, then restore it.
        boolean interrupted = Thread.interrupted();
        try {
            try { command(new JSONObject().put("type", "stop").put("reason", ending)); } catch (Exception ignored) { }
            // Record cleanup/last counters even on an externally requested stop.
            // Do not overwrite the original error or wait indefinitely.
            long drainUntil = SystemClock.elapsedRealtime() + 10000;
            while (!agentStopped && SystemClock.elapsedRealtime() < drainUntil && (!eof || !lines.isEmpty())) {
                String line = lines.poll(100, TimeUnit.MILLISECONDS);
                if (line == null) continue;
                try {
                    handle(line);
                } catch (Exception error) {
                    recoveryFailed = true;
                    record(new JSONObject().put("event", "host_shutdown_error").put("message", error.toString()).toString());
                }
            }
            closeControl();
            if (!local.waitFor(1200, TimeUnit.MILLISECONDS) && helperPid > 1 && helperPid != gamePid) {
                try (RootShell cleanup = new RootShell()) {
                    cleanup.execute(EnginePolicy.killCommand(helperPid, gamePid, binary), 2000);
                } catch (Exception ignored) { }
            }
            if (local.isAlive()) local.destroy();
            if (outputReader != null) {
                outputReader.join(1200);
                if (outputReader.isAlive()) { local.getInputStream().close(); outputReader.interrupt(); outputReader.join(500); }
            }
            if (!agentStopped || !cleanupConfirmed || !nativeLocationCleanupConfirmed)
                ending += " · cleanup unconfirmed; restart Pokémon GO";
        } catch (Exception ignored) { local.destroy(); }
        finally {
            if (local.isAlive()) {
                local.destroy();
                try { local.waitFor(1000, TimeUnit.MILLISECONDS); } catch (InterruptedException error) { interrupted = true; }
            }
            transportClosed = !local.isAlive() && (outputReader == null || !outputReader.isAlive());
            if (interrupted) Thread.currentThread().interrupt(); control = null; process = null;
        }
    }

    private void record(String text) throws Exception {
        if (log == null) return;
        byte[] data = (text + "\n").getBytes(StandardCharsets.UTF_8);
        if (initialLog != null && initialLogged + data.length <= 64L * 1024) {
            initialLog.write(data); initialLog.flush(); initialLogged += data.length;
        }
        log.write(text);
    }

    private static String hash(File file) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try (FileInputStream input = new FileInputStream(file)) {
            byte[] bytes = new byte[65536]; int count;
            while ((count = input.read(bytes)) != -1) digest.update(bytes, 0, count);
        }
        StringBuilder hex = new StringBuilder();
        for (byte value : digest.digest()) hex.append(String.format(java.util.Locale.ROOT, "%02x", value & 255));
        return hex.toString();
    }
}
