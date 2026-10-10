package dev.rootcatch.ugphone;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.os.SystemClock;
import dev.rootcatch.automation.AutomationConfig;
import dev.rootcatch.nativeengine.EnginePolicy;
import dev.rootcatch.nativeengine.NativeEngineRunner;
import dev.rootcatch.nativeengine.RollingLog;
import dev.rootcatch.nativeengine.TransferOptions;
import dev.rootcatch.root.RootShell;
import org.json.JSONObject;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.Locale;

final class FarmSession implements Runnable {
    interface Listener {
        void launchReady(String ticket);
        void status(String message); void ready(); void engineStopped(); void finished(String reason);
        void stats(int caught, int fled, int spun, int discarded); void transfers(int confirmed);
        void rewards(long xp, long stardust, long missing, boolean available); void cleanupPaused(String reason);
    }
    private final Context context;
    private final AutomationConfig config;
    private final TransferOptions transfers;
    private final boolean resolveError12;
    private final boolean userApprovedGameStop;
    private final boolean automaticRestart;
    private final int recoveryAttempt;
    private final String expectedPlayerId;
    private final SessionCounters baseline;
    private volatile boolean recoveryReady;
    private volatile boolean userCancelled, startupRetryReady;
    private boolean startupOpened;
    private volatile boolean retryCandidate;
    private final Listener listener;
    private final RootShell root = new RootShell(), monitorRoot = new RootShell();
    private volatile boolean stopping;
    private volatile String stopReason;
    private volatile NativeEngineRunner runner;
    private volatile long readyAt;
    private Thread monitor;
    private int pid;
    private long startTicks, lastSnapshot;
    private File report;
    private RollingLog trace;
    private CrashCollector collector;
    private SessionCounters counters = new SessionCounters();
    private String ending = "Session ended", lastStatus = "Preparing", lastInspection = "";
    private String cleanupPauseReason = "";

    FarmSession(Context context, AutomationConfig config, TransferOptions transfers, boolean resolveError12,
                boolean userApprovedGameStop, boolean automaticRestart, int recoveryAttempt, String expectedPlayerId,
                SessionCounters baseline, Listener listener) {
        this.context = context.getApplicationContext(); this.config = config;
        this.transfers = transfers; this.resolveError12 = resolveError12;
        this.userApprovedGameStop = resolveError12 && userApprovedGameStop; this.listener = listener;
        this.automaticRestart = automaticRestart; this.expectedPlayerId = expectedPlayerId;
        this.recoveryAttempt = recoveryAttempt;
        this.baseline = baseline; this.counters = baseline;
    }
    boolean recoveryReady() { return recoveryReady && stopReason == null; }
    boolean startupRetryReady() { return startupRetryReady && !userCancelled; }
    SessionCounters totals() { return counters; }
    String playerIdentity() { return runner == null ? "" : runner.playerIdentity(); }
    void noteGameLaunch(String ticket) {
        if (!stopping && report != null && report.getName().equals(ticket))
            record("user_activity_game_launch", detail("visible Activity; one attempt; no automatic restart"));
    }
    boolean isStopping() { return stopping; }
    synchronized void requestStop(String reason) {
        userCancelled = true;
        stopEngine(reason);
    }
    private synchronized void stopEngine(String reason) {
        stopReason = reason;
        if (stopping) return;
        stopping = true;
        listener.launchReady("");
        record("stop_requested", detail(reason));
        NativeEngineRunner active = runner;
        if (active != null) active.requestStop(reason); else root.close();
        monitorRoot.close();
    }
    @Override public void run() {
        String result = "Engine did not start";
        String limitReason = "";
        try {
            report = FarmReports.create(context.getFilesDir());
            trace = new RollingLog(report, "farm-session.jsonl", 1024 * 1024, 2);
            reportStatus(false, "Preparing");
            record("session_start", new JSONObject().put("build", FarmPolicy.BUILD).put("sessionId", report.getName())
                    .put("appPackage", context.getPackageName()).put("android", Build.VERSION.RELEASE)
                    .put("sdk", Build.VERSION.SDK_INT).put("modelReported", Build.MODEL)
                    .put("abisReported", Arrays.toString(Build.SUPPORTED_ABIS)).put("rawLogs", true)
                    .put("automaticPreflight", true).put("singleAttach", true).put("readOnlyObservationMs", 10_000)
                    .put("continuous", true).put("autoCatch", config.autoCatch).put("autoSpin", config.autoSpin)
                    .put("cleanBag", config.cleanBag).put("transfersEnabled", transfers.enabled)
                    .put("transferPolicy", transfers.proofKey()).put("cleanupLimits", EnginePolicy.limitsKey(config.discardLimits))
                    .put("maxDiscardedItems", config.maxDiscardedItems).put("cleanupTrigger", EnginePolicy.CLEANUP_TRIGGER)
                    .put("actionDelayMs", config.actionDelayMs).put("maxThrows", config.maxThrows)
                    .put("throwQuality", config.throwQuality)
                    .put("useBerries", config.useBerries).put("berryOrder", new org.json.JSONArray(config.berryOrder))
                    .put("skipInterruptions", config.skipInterruptions)
                    .put("maxCatches", config.maxCatches).put("maxSpins", config.maxSpins)
                    .put("priorCaught", baseline.caught).put("priorSpun", baseline.spun).put("automaticRestart", automaticRestart)
                    .put("recoveryAttempt", recoveryAttempt).put("maxRecoveryAttempts", RecoveryAttempts.MAX_ATTEMPTS)
                    .put("keepDynamax", transfers.keepDynamax).put("keepHatched", transfers.keepHatched)
                    .put("keepBackground", transfers.keepBackground).put("keepShinyBackground", transfers.keepShinyBackground)
                    .put("expectedAgentSha256", FarmPolicy.EXPECTED_AGENT_SHA).put("externalEnhancerRequired", !resolveError12)
                    .put("edition", "ugphone").put("resolveError12", resolveError12).put("locationImplementation", "rootcatch-profile7-v2")
                    .put("attachPhase", resolveError12 || automaticRestart ? "startup" : "warm").put("rootModulesModified", false)
                    .put("userApprovedGameStop", userApprovedGameStop).put("maxGameStopAttempts", 1)
                    .put("startupAuthorization", userApprovedGameStop ? "USER_START_BUTTON" : "NONE")
                    .put("controlRecovery", true).put("controlStopAfterMs", 10000).put("actionLeaseMs", 2200));
            FarmPolicy.validate(config.autoCatch, config.autoSpin, config.cleanBag, transfers.enabled, config.discardLimits);
            String block = dev.rootcatch.automation.SessionLimits.startBlock(baseline.caught, baseline.spun, config.maxCatches, config.maxSpins);
            if (!block.isEmpty()) throw new IllegalArgumentException(block);
            if(config.autoCatch&&config.useBerries&&config.berryOrder.isEmpty())throw new IllegalArgumentException("Select berries to use");
            listener.status("Requesting root…");
            if (!root.isAvailable()) throw new IllegalStateException("Root permission unavailable");
            if (stopping) return;
            if (!config.internalEngine) throw new IllegalStateException("Internal engine disabled");
            if (!Arrays.asList(Build.SUPPORTED_ABIS).contains("arm64-v8a"))
                throw new IllegalStateException("ARM64 required");
            PackageInfo game = context.getPackageManager().getPackageInfo(EnginePolicy.GAME, 0);
            if (!EnginePolicy.supports(config.gamePackage, game.versionName, game.getLongVersionCode()))
                throw new IllegalStateException("Supported version: GO 0.431.0 / 2026092402");
            try (InputStream asset = context.getAssets().open("engine/agent.js")) {
                if (!FarmPolicy.EXPECTED_AGENT_SHA.equals(hash(asset))) throw new IllegalStateException("Engine integrity check failed");
            }
            if (resolveError12 && Build.VERSION.SDK_INT != 29)
                throw new IllegalStateException("Fix error 12: UGPhone Android 10 profile");
            RootShell.Result security = root.execute("getenforce", 2000);
            record("environment", new JSONObject().put("rootGranted", true).put("selinuxReported", security.output).put("exit", security.code));
            runner = new NativeEngineRunner(context, root, config, false, new NativeEngineRunner.Listener() {
                @Override public void onStatus(String message) {
                    if ("Internal engine active".equals(message) && readyAt == 0) {
                        readyAt = SystemClock.elapsedRealtime(); listener.ready();
                    }
                    if (!message.startsWith("Internal · catches ")) { lastStatus = message; listener.status(compactStatus(message)); }
                    snapshot();
                }
                @Override public void onStopped(String reason) { ending = reason; }
                @Override public void onStatistics(int caught, int fled, int spun, int discarded) {
                    counters = counters.update(SessionCounters.add(baseline.caught, caught), SessionCounters.add(baseline.fled, fled),
                            SessionCounters.add(baseline.spun, spun), SessionCounters.add(baseline.discarded, discarded));
                    listener.stats(counters.caught, counters.fled, counters.spun, counters.discarded); snapshot();
                }
                @Override public void onTransferStatistics(int confirmed) {
                    counters = counters.transfers(SessionCounters.add(baseline.transferred, confirmed)); listener.transfers(counters.transferred); snapshot();
                }
                @Override public void onRewards(long xp, long stardust, long missing, boolean available) {
                    counters = counters.rewards(SessionCounters.add(baseline.xp, xp), SessionCounters.add(baseline.stardust, stardust),
                            SessionCounters.add(baseline.missingRewardReceipts, missing), baseline.rewardsAvailable || available);
                    listener.rewards(counters.xp, counters.stardust, counters.missingRewardReceipts, counters.rewardsAvailable); snapshot();
                }
                @Override public void onCleanupPaused(String reason) {
                    cleanupPauseReason = reason; listener.cleanupPaused(reason); snapshot();
                }
                @Override public void onInspection(String summary) { lastInspection = summary; snapshot(); }
            }, true, false, 0, report, transfers.enabled, transfers, expectedPlayerId, true);
            runner.accumulatedCounts(baseline.caught, baseline.spun, true);
            runner.enableNativeLocation(resolveError12);
            listener.status("Preparing connection…");
            runner.prepareAssets(FarmPolicy.EXPECTED_AGENT_SHA);
            if (stopping) return;
            if (resolveError12 || automaticRestart) {
                collector = new CrashCollector(report, 0, 0, true);
                collector.start(true);
                listener.status("Checking game startup…");
                GameStartCheck startCheck = new GameStartCheck((command, timeout) -> {
                    RootShell.Result reply = root.execute(command, timeout);
                    return new GameStartCheck.Reply(reply.code, reply.output);
                }, (event, values) -> record(event, new JSONObject(values)),
                        SystemClock::elapsedRealtime, Thread::sleep, () -> stopping);
                startCheck.prepare(!automaticRestart && userApprovedGameStop);
                if (stopping) return;
                record("early_armed", new JSONObject().put("assetsAlreadyVerified", true)
                        .put("gameAbsentConfirmed", true).put("automaticKill", false).put("singleAttach", true)
                        .put("userApprovedGameStop", userApprovedGameStop).put("automaticRestart", automaticRestart));
                listener.status("Opening Pokémon GO…");
                if (automaticRestart) launchRecoveryGame(); else listener.launchReady(report.getName());
                awaitEarlyProcess();
                listener.launchReady("");
                if (stopping) return;
                collector.bindTarget(pid, startTicks);
            } else {
                awaitProcess();
                if (stopping) return;
                listener.status("Preparing logs…");
                collector = new CrashCollector(report, pid, startTicks, true);
                collector.start(false);
            }
            if (stopping) return;
            record("target", new JSONObject().put("pid", pid).put("startTicks", startTicks)
                    .put("gameVersion", game.versionName).put("gameVersionCode", game.getLongVersionCode()));
            checkIdentity(root);
            RootShell.Result processStatus = root.execute("cat /proc/" + pid + "/status", 2500);
            if (processStatus.code != 0 || AttachGuard.tracerPid(processStatus.output) != 0)
                throw new IllegalStateException("The game is already traced; engine not connected");
            runner.expectProcess(pid, startTicks);
            if (stopping) { runner.requestStop(stopReason); return; }
            long started = SystemClock.elapsedRealtime();
            monitor = new Thread(() -> supervise(started), "rootcatch-farm-guard");
            monitor.setDaemon(true); monitor.start();
            runner.run();
            result = stopReason != null ? stopReason : ending;
            if (stopReason == null && !runner.sessionLimitReached().isEmpty()) {
                limitReason = dev.rootcatch.automation.SessionLimits.reason(runner.sessionLimitReached());
                result = limitReason;
                stopping = true;
                monitorRoot.close();
                if (monitor != null) { monitor.interrupt(); monitor.join(1500); }
                listener.status("Closing Pokémon GO…");
                SessionLimitStop limitStop = new SessionLimitStop((command, timeout) -> {
                    RootShell.Result reply = root.execute(command, timeout);
                    return new GameStartCheck.Reply(reply.code, reply.output);
                }, (event, values) -> record(event, new JSONObject(values)), Thread::sleep);
                SessionLimitStop.Result closure = limitStop.close(pid, startTicks);
                record("session_limit_completed", new JSONObject().put("kind", runner.sessionLimitReached())
                        .put("maxCatches", config.maxCatches).put("maxSpins", config.maxSpins)
                        .put("caught", counters.caught).put("spun", counters.spun)
                        .put("gameClosed", closure.gameClosed).put("closureCommandAccepted", closure.commandAccepted)
                        .put("closureStatus", closure.status).put("closureError", closure.error)
                        .put("closureObservations", closure.observations).put("engineStoppedBeforeGameClose", true));
            } else if (stopReason == null && runner.emptyMapRecoveryReady()) {
                stopping = true;
                monitorRoot.close();
                if (monitor != null) { monitor.interrupt(); monitor.join(1500); }
                listener.status("Restarting Pokémon GO…");
                SessionLimitStop gameStop = new SessionLimitStop((command, timeout) -> {
                    RootShell.Result reply = root.execute(command, timeout);
                    return new GameStartCheck.Reply(reply.code, reply.output);
                }, (event, values) -> record("empty_map_" + event, new JSONObject(values)), Thread::sleep);
                if (stopReason == null) {
                    SessionLimitStop.Result closure = gameStop.close(pid, startTicks);
                    recoveryReady = closure.gameClosed && !runner.playerIdentity().isEmpty() && stopReason == null;
                    result = recoveryReady ? "Restarting after 5 minutes without nearby Pokémon" : "Map recovery could not close the game";
                    record("empty_map_recovery_closed", new JSONObject().put("gameClosed", closure.gameClosed)
                            .put("status", closure.status).put("error", closure.error).put("recoveryReady", recoveryReady)
                            .put("caught", counters.caught).put("spun", counters.spun));
                }
            }
        } catch (Exception error) {
            result = stopReason != null ? stopReason : limitReason.isEmpty() ? "Engine: " + error.getMessage() : limitReason;
            if (limitReason.isEmpty()) record("session_error", detail(result));
            else record("session_limit_finalize_error", detail(error.toString()));
        } finally {
            stopping = true;
            listener.launchReady("");
            if (runner != null) runner.requestStop();
            monitorRoot.close();
            if (monitor != null) {
                monitor.interrupt();
                try { monitor.join(1500); } catch (InterruptedException error) { Thread.currentThread().interrupt(); }
            }
            prepareStartupRetry();
            root.close();
            listener.engineStopped();
            if (collector != null) {
                listener.status("Saving logs…");
                try { collector.finish(); } catch (Exception error) { record("collector_error", detail(error.toString())); }
            }
            result = stopReason != null ? stopReason : result;
            if (startupRetryReady() && recoveryAttempt == RecoveryAttempts.MAX_ATTEMPTS)
                result = "Startup failed after " + RecoveryAttempts.MAX_ATTEMPTS + " attempts";
            record("session_end", detail(result));
            reportStatus(true, result);
            try { if (trace != null) trace.close(); } catch (Exception ignored) { }
            listener.finished(result);
        }
    }
    private void launchRecoveryGame() throws Exception {
        if (stopping) return;
        Intent launch = context.getPackageManager().getLaunchIntentForPackage(EnginePolicy.GAME);
        if (launch == null || launch.getComponent() == null
                || !EnginePolicy.GAME.equals(launch.getComponent().getPackageName()))
            throw new IllegalStateException("Could not resolve game launch");
        String component = launch.getComponent().flattenToString();
        startupOpened = true;
        RootShell.Result reply = root.execute("am start --user current -n " + EnginePolicy.quote(component)
                + " -a android.intent.action.MAIN -c android.intent.category.LAUNCHER", 8000);
        record("recovery_game_launch", new JSONObject().put("component", component).put("exit", reply.code).put("output", reply.output));
        if (!stopping && (reply.code != 0 || reply.output.contains("Error:")))
            throw startupFailure("Automatic game launch failed");
    }
    private IllegalStateException startupFailure(String message) {
        retryCandidate = true; return new IllegalStateException(message);
    }
    private void prepareStartupRetry() {
        if (!automaticRestart || !startupOpened || readyAt != 0 || userCancelled
                || !(retryCandidate || runner != null && runner.startupRetryCandidate())) return;
        try {
            if (runner != null && !runner.startupTransportClosed()) {
                record("recovery_retry_blocked", detail("Previous injector exit unconfirmed")); return;
            }
            if (pid > 1 && startTicks > 0) {
                RootShell.Result ids = root.execute(GameStartCheck.framed("PIDOF", GameStartCheck.PIDOF), 2000);
                RootShell.Result target = root.execute(GameStartCheck.framed("TARGET", SessionLimitStop.targetCommand(pid)), 2000);
                GameStartCheck.State state = SessionLimitStop.absence(new GameStartCheck.Reply(ids.code, ids.output),
                        new GameStartCheck.Reply(target.code, target.output), startTicks);
                if (state == GameStartCheck.State.UNVERIFIED) {
                    record("recovery_retry_blocked", detail("Game identity unconfirmed")); return;
                }
                if (state == GameStartCheck.State.RUNNING && !userCancelled) {
                    SessionLimitStop gameStop = new SessionLimitStop((command, timeout) -> {
                        RootShell.Result reply = root.execute(command, timeout);
                        return new GameStartCheck.Reply(reply.code, reply.output);
                    }, (event, values) -> record("recovery_retry_" + event, new JSONObject(values)), Thread::sleep);
                    if (!gameStop.close(pid, startTicks).gameClosed) return;
                }
            }
            GameStartCheck absence = new GameStartCheck((command, timeout) -> {
                RootShell.Result reply = root.execute(command, timeout);
                return new GameStartCheck.Reply(reply.code, reply.output);
            }, (event, values) -> record("recovery_retry_" + event, new JSONObject(values)),
                    SystemClock::elapsedRealtime, Thread::sleep, () -> userCancelled);
            absence.prepare(false);
            startupRetryReady = !userCancelled;
            record("recovery_retry_ready", new JSONObject().put("attempt", recoveryAttempt)
                    .put("maxAttempts", RecoveryAttempts.MAX_ATTEMPTS).put("retryReady", startupRetryReady));
        } catch (Exception error) { record("recovery_retry_blocked", detail(error.toString())); }
    }
    private void awaitEarlyProcess() throws Exception {
        long waitingSince = SystemClock.elapsedRealtime(), until = waitingSince + 150_000;
        int observedPid = 0; long observedTicks = 0;
        while (!stopping && SystemClock.elapsedRealtime() < until) {
            RootShell.Result found = root.execute("pidof " + EnginePolicy.GAME, 2000);
            int candidate = found.code == 0 ? EnginePolicy.uniquePid(found.output) : -1;
            if (candidate > 1) {
                long ticks;
                try { ticks = readStart(root, candidate); }
                catch (Exception error) { throw startupFailure("Game identity unavailable during startup: " + error.getMessage()); }
                if (observedPid > 1 && (candidate != observedPid || ticks != observedTicks))
                    throw startupFailure("The game changed during startup; start again");
                if (observedPid == 0) {
                    observedPid = candidate; observedTicks = ticks;
                    pid = candidate; startTicks = ticks;
                    record("game_observed", new JSONObject().put("pid", candidate).put("startTicks", ticks));
                }
                RootShell.Result status = root.execute("cat /proc/" + candidate + "/status", 2000);
                if (status.code == 0 && AttachGuard.tracerPid(status.output) == 0
                        && NativeEngineRunner.gameFocused(root, () -> stopping, sample -> {
                    try { record("startup_focus", new JSONObject(sample).put("pid", candidate).put("startTicks", ticks)
                            .put("startupWaitMs", SystemClock.elapsedRealtime() - waitingSince)); }
                    catch (Exception ignored) { }
                })) {
                    pid = candidate; startTicks = ticks; return;
                }
            } else if (observedPid > 1) throw startupFailure("The game ended during startup");
            Thread.sleep(250);
        }
        if (!stopping) throw startupFailure("Game not found during startup");
    }
    private void awaitProcess() throws Exception {
        long until = SystemClock.elapsedRealtime() + 90_000;
        int consecutive = 0, previousPid = 0; long previousTicks = 0;
        listener.status("Waiting for the game…");
        while (!stopping && SystemClock.elapsedRealtime() < until) {
            RootShell.Result found = root.execute("pidof " + EnginePolicy.GAME, 2500);
            int candidate = EnginePolicy.uniquePid(found.output);
            if (found.code == 0 && candidate > 1) {
                long ticks = readStart(root, candidate);
                RootShell.Result status = root.execute("cat /proc/" + candidate + "/status", 2500);
                RootShell.Result maps = root.execute("cat /proc/" + candidate + "/maps", 2500);
                boolean untraced = status.code == 0 && AttachGuard.tracerPid(status.output) == 0;
                boolean loaded = maps.code == 0 && maps.output.contains("libil2cpp.so") && maps.output.contains("libunity.so");
                record("attach_guard", new JSONObject().put("pid", candidate).put("startTicks", ticks)
                        .put("status", status.output).put("tracerFree", untraced).put("librariesLoaded", loaded));
                consecutive = untraced && loaded ? (candidate == previousPid && ticks == previousTicks ? consecutive + 1 : 1) : 0;
                previousPid = candidate; previousTicks = ticks;
                if (consecutive >= 3) { pid = candidate; startTicks = ticks; return; }
            } else consecutive = 0;
            Thread.sleep(1000);
        }
        if (!stopping) throw new IllegalStateException("No game ready to connect");
    }
    private void supervise(long started) {
        try {
            while (!stopping) {
                if (readyAt == 0 && SystemClock.elapsedRealtime() - started >= FarmPolicy.STARTUP_LIMIT_MS) {
                    retryCandidate = true; stopEngine("Startup incomplete; export logs"); return;
                }
                checkIdentity(monitorRoot);
                Thread.sleep(1000);
            }
        } catch (InterruptedException error) { Thread.currentThread().interrupt(); }
        catch (Exception error) {
            if (!stopping) { retryCandidate = readyAt == 0; stopEngine("Game disconnected: " + error.getMessage()); }
        }
    }
    private void checkIdentity(RootShell shell) throws Exception {
        RootShell.Result found = shell.execute("pidof " + EnginePolicy.GAME, 2500);
        if (found.code != 0 || EnginePolicy.uniquePid(found.output) != pid || readStart(shell, pid) != startTicks)
            throw new IllegalStateException("Game process changed");
    }
    private long readStart(RootShell shell, int target) throws Exception {
        RootShell.Result identity = shell.execute("cat /proc/" + target + "/stat", 2500);
        if (identity.code != 0) throw new IllegalStateException("Could not verify the process");
        record("process_identity", new JSONObject().put("pid", target).put("stat", identity.output));
        return FarmPolicy.processStartTicks(identity.output);
    }
    private synchronized void record(String event, JSONObject detail) {
        if (trace == null) return;
        try { trace.write(detail.put("farmEvent", event).put("time", System.currentTimeMillis())
                .put("elapsedMs", SystemClock.elapsedRealtime()).toString()); } catch (Exception ignored) { }
    }
    private void snapshot() {
        long now = SystemClock.elapsedRealtime();
        if (now - lastSnapshot >= 5000) { lastSnapshot = now; reportStatus(false, lastStatus); }
    }
    private void reportStatus(boolean completed, String reason) {
        if (report == null) return;
        try (FileOutputStream output = new FileOutputStream(new File(report, "report-status.json"))) {
            output.write(new JSONObject().put("sessionId", report.getName()).put("workerFinished", completed)
                    .put("build", FarmPolicy.BUILD).put("result", reason).put("agentSha256", FarmPolicy.EXPECTED_AGENT_SHA)
                    .put("pid", pid).put("startTicks", startTicks).put("continuous", true).put("automaticPreflight", true)
                    .put("singleAttach", true).put("caught", counters.caught).put("fled", counters.fled)
                    .put("recoveryAttempt", recoveryAttempt).put("maxRecoveryAttempts", RecoveryAttempts.MAX_ATTEMPTS)
                    .put("startupRetryReady", startupRetryReady())
                    .put("resolveError12", resolveError12).put("locationImplementation", "rootcatch-profile7-v2")
                    .put("spun", counters.spun).put("transferred", counters.transferred).put("discarded", counters.discarded)
                    .put("xp", counters.xp).put("stardust", counters.stardust)
                    .put("missingRewardReceipts", counters.missingRewardReceipts).put("rewardsAvailable", counters.rewardsAvailable)
                    .put("cleanupPaused", !cleanupPauseReason.isEmpty()).put("cleanupPauseReason", cleanupPauseReason)
                    .put("lastInspection", lastInspection).put("transferPolicy", transfers.proofKey())
                    .put("cleanupLimits", EnginePolicy.limitsKey(config.discardLimits)).put("maxDiscardedItems", config.maxDiscardedItems)
                    .put("cleanupTrigger", EnginePolicy.CLEANUP_TRIGGER).put("activeSinceElapsedMs", readyAt)
                    .put("wrapperLogBytes", trace == null ? 0 : trace.totalBytes()).put("wrapperLogArchives", 2)
                    .put("wrapperLogRotations", trace == null ? 0 : trace.rotations()).put("time", System.currentTimeMillis())
                    .toString().getBytes(StandardCharsets.UTF_8));
        } catch (Exception error) { record("report_status_error", detail(error.toString())); }
    }
    static String hash(InputStream input) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        byte[] bytes = new byte[32768]; int count;
        while ((count = input.read(bytes)) != -1) digest.update(bytes, 0, count);
        StringBuilder text = new StringBuilder();
        for (byte value : digest.digest()) text.append(String.format(Locale.ROOT, "%02x", value & 255));
        return text.toString();
    }
    private static JSONObject detail(String value) {
        JSONObject object = new JSONObject();
        try { object.put("message", value); } catch (Exception ignored) { }
        return object;
    }
    static String compactStatus(String message) {
        if ("Internal engine active".equals(message) || message.startsWith("Internal resumed")) return "Active";
        if (message.startsWith("Internal paused")) return "Paused";
        if (message.startsWith("Internal: open")) return "Waiting for the map…";
        if (message.startsWith("Catches paused: Pokémon storage")) return "Pokémon storage full";
        if (message.startsWith("Catches paused: no ordinary") || message.startsWith("No Poké Balls")) return "No Poké Balls";
        if (message.startsWith("Catches resumed")) return "Active";
        if (message.startsWith("Item bag full") || message.startsWith("Bag full WITHOUT")) return "Bag full";
        if (message.startsWith("Initial cleanup ·")) return "Cleaning item bag…";
        if (message.startsWith("Initial cleanup finished")) return "Active";
        if (message.startsWith("Cleanup paused:")) return "Active";
        if (message.startsWith("Initial cleanup: quota") || message.startsWith("TOTAL cleanup quota")) return "Cleanup quota reached";
        if (message.startsWith("Transfer quota reached")) return "Transfer quota reached";
        if (message.startsWith("Queue empty:")) return "Preparing…";
        return message;
    }
}
