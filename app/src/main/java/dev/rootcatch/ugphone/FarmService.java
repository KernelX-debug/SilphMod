package dev.rootcatch.ugphone;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.PowerManager;
import android.os.SystemClock;
import android.widget.Toast;
import dev.rootcatch.R;
import dev.rootcatch.automation.AutomationConfig;
import dev.rootcatch.automation.SessionLimits;
import dev.rootcatch.nativeengine.TransferOptions;

public final class FarmService extends Service {
    static final String START = "dev.rootcatch.ugphone.START", STOP = "dev.rootcatch.ugphone.STOP";
    private static final String CHANNEL = "rootcatch-farm";
    private static final int NOTIFICATION = 601;
    private static volatile boolean running, exporting, closing, workerAlive;
    private static volatile String status = "Ready";
    private static volatile String cleanupNote = "";
    private static volatile SessionCounters counters = new SessionCounters();
    private static volatile long readyAt, stoppedAt, savedElapsed;
    private static volatile String launchReadySession = "";
    private static volatile FarmSession activeSession;
    private static long epoch;
    private long ownEpoch, nextWakeRenew, lastUpdate;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private FarmSession session;
    private AutomationConfig config;
    private TransferOptions transfers;
    private boolean resolveError12, approvedGameStop, userStopped;
    private boolean buddyRemovalHandled;
    private boolean buddyNoticeShown;
    private String userStopReason = "Stopped by user";
    private long cycle;
    private final RecoveryAttempts recovery = new RecoveryAttempts();
    private CompatibilitySession compatibility;
    private boolean verificationOnly;
    private Runnable pendingRecovery;
    private PowerManager.WakeLock wakeLock;

    static boolean isRunning() { return running; }
    static boolean isBusy() { return running || workerAlive || exporting; }
    static boolean isClosing() { return closing; }
    static String status() { return status + (running && !closing && !cleanupNote.isEmpty() ? "\n" + cleanupNote : ""); }
    static SessionCounters counters() { return counters; }
    static synchronized String consumeLaunchTicket() {
        if (!running || closing || activeSession == null || activeSession.isStopping()) return "";
        String ticket = launchReadySession; launchReadySession = ""; return ticket;
    }
    static void noteGameLaunch(String ticket) {
        FarmSession current = activeSession;
        if (current != null) current.noteGameLaunch(ticket);
    }
    static long elapsed() {
        return SessionCounters.add(savedElapsed, readyAt == 0 ? 0 : Math.max(0, SystemClock.elapsedRealtime() - readyAt));
    }
    static synchronized boolean clearStatistics(Context context) {
        if (isBusy()) return false;
        counters = new SessionCounters(); savedElapsed = 0; readyAt = 0; stoppedAt = 0; status = "Ready";
        persist(context);
        context.getSystemService(NotificationManager.class).cancel(NOTIFICATION);
        return true;
    }
    static synchronized boolean beginExport() { if (isBusy()) return false; exporting = true; return true; }
    static synchronized void endExport() { exporting = false; }
    static synchronized void restoreLast(Context context) {
        if (running || workerAlive) return;
        restoreFarmer(context);
    }
    private static SharedPreferences saved(Context context) {
        return context.getSharedPreferences("farm_last_result", 0);
    }
    private static SessionCounters readCounters(SharedPreferences saved) {
        return new SessionCounters(saved.getInt("caught", 0), saved.getInt("fled", 0),
                saved.getInt("spun", 0), saved.getInt("discarded", 0), saved.getInt("transferred", 0),
                saved.getLong("xp", 0), saved.getLong("stardust", 0), saved.getLong("missing_rewards", 0),
                saved.getBoolean("rewards_available", false));
    }
    private static void restoreFarmer(Context context) {
        SharedPreferences saved = saved(context);
        status = "en".equals(saved.getString("status_language", "")) ? saved.getString("status", "Ready") : "Ready";
        counters = readCounters(saved);
        savedElapsed = saved.getLong("elapsed", 0); readyAt = 0; stoppedAt = 0;
    }
    private String limitBlock() {
        return SessionLimits.startBlock(counters.caught, counters.spun, config.maxCatches, config.maxSpins);
    }
    private final Runnable tick = new Runnable() {
        @Override public void run() {
            if (!running) return;
            if (!userStopped && !notificationsVisible(FarmService.this)) stopSession("Notifications disabled");
            if (wakeLock != null && SystemClock.elapsedRealtime() >= nextWakeRenew) {
                wakeLock.acquire(120_000L); nextWakeRenew = SystemClock.elapsedRealtime() + 60_000;
            }
            update(true); handler.postDelayed(this, 2000);
        }
    };
    @Override public void onCreate() {
        super.onCreate();
        restoreLast(this);
        getSystemService(NotificationManager.class).createNotificationChannel(new NotificationChannel(CHANNEL,
                "SilphMod service", NotificationManager.IMPORTANCE_LOW));
    }
    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) { if (!running) stopSelf(); return START_NOT_STICKY; }
        if (STOP.equals(intent.getAction())) {
            if (running || session != null) stopSession(intent.getStringExtra("reason") == null ? "Stopped by user" : intent.getStringExtra("reason"));
            else { getSystemService(NotificationManager.class).cancel(NOTIFICATION); stopSelf(); }
            return START_NOT_STICKY;
        }
        if (!START.equals(intent.getAction())) { if (!running) stopSelf(); return START_NOT_STICKY; }
        if (running) return START_NOT_STICKY;
        buddyRemovalHandled = false;
        buddyNoticeShown = false;
        // Meet Android's foreground-service deadline before any root prompt, file IO or validation.
        startForeground(NOTIFICATION, notification());
        synchronized (FarmService.class) {
            if (exporting || workerAlive) { stopForeground(STOP_FOREGROUND_REMOVE); stopSelf(); return START_NOT_STICKY; }
            running = true; closing = false; ownEpoch = ++epoch;
        }
        config = AutomationConfig.load(this);
        restoreFarmer(this);
        try {
            transfers = new TransferOptions(intent.getBooleanExtra("transfer", false), intent.getBooleanExtra("keepShiny", true),
                    intent.getBooleanExtra("keepHundo", true), intent.getBooleanExtra("keepDynamax", true),
                    intent.getBooleanExtra("keepHatched", true), intent.getBooleanExtra("keepBackground", true),
                    intent.getBooleanExtra("keepShinyBackground", false), intent.getBooleanExtra("keepFieldResearch", true), 0);
            FarmPolicy.validate(config.autoCatch, config.autoSpin, config.cleanBag, transfers.enabled, config.discardLimits);
            String block = limitBlock();
            if (!block.isEmpty()) throw new IllegalArgumentException(block);
            if (!notificationsVisible(this)) throw new IllegalArgumentException("Enable service notifications");
        } catch (IllegalArgumentException error) { finish(error.getMessage()); return START_NOT_STICKY; }
        readyAt = 0; stoppedAt = 0; launchReadySession = ""; userStopped = false; userStopReason = "Stopped by user";
        cancelRecovery(); recovery.beginStartup(); compatibility = new CompatibilitySession();
        resolveError12 = intent.getBooleanExtra("resolveError12", false);
        verificationOnly = dev.rootcatch.BuildConfig.DEBUG && intent.getBooleanExtra("verifyStartupOnly", false);
        compatibility.simulateSpawnFailure(verificationOnly && intent.getBooleanExtra("simulateSpawnFailure", false));
        approvedGameStop = intent.getBooleanExtra("userApprovedGameStop", false)
                && "USER_START_BUTTON".equals(intent.getStringExtra("startupAuthorization"));
        status = "Preparing…"; cleanupNote = ""; update(true);
        wakeLock = getSystemService(PowerManager.class).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "SilphMod:Farm");
        wakeLock.setReferenceCounted(false); wakeLock.acquire(120_000L);
        nextWakeRenew = SystemClock.elapsedRealtime() + 60_000;
        startCycle(false, "");
        handler.postDelayed(tick, 2000);
        return START_NOT_STICKY;
    }
    private void startCycle(boolean automaticRestart, String expectedPlayerId) {
        if (userStopped || !running) return;
        long token = ownEpoch, round = ++cycle;
        closing = false; cleanupNote = "";
        FarmSession current = new FarmSession(this, config, transfers, resolveError12, approvedGameStop,
                automaticRestart, recovery.attempt(), expectedPlayerId, counters, compatibility, new FarmSession.Listener() {
            @Override public void launchReady(String ticket) { handler.post(() -> {
                if (currentCycle(token, round) && !userStopped) launchReadySession = ticket; }); }
            @Override public void status(String message) { handler.post(() -> {
                if (currentCycle(token, round) && !userStopped) {
                    if ("Removing buddy…".equals(message) && !buddyNoticeShown) {
                        buddyNoticeShown = true; Toast.makeText(FarmService.this, R.string.removing_buddy, Toast.LENGTH_LONG).show();
                    }
                    status = message; update(false);
                } }); }
            @Override public void ready() { handler.post(() -> {
                if (currentCycle(token, round) && !userStopped && readyAt == 0) { readyAt = SystemClock.elapsedRealtime(); status = "Active"; update(true); } }); }
            @Override public void stats(int caught, int fled, int spun, int discarded) { handler.post(() -> {
                if (currentCycle(token, round)) { counters = counters.update(caught, fled, spun, discarded); persistLast(); update(false); } }); }
            @Override public void transfers(int confirmed) { handler.post(() -> {
                if (currentCycle(token, round)) { counters = counters.transfers(confirmed); persistLast(); update(false); } }); }
            @Override public void rewards(long xp, long stardust, long missing, boolean available) { handler.post(() -> {
                if (currentCycle(token, round)) { counters = counters.rewards(xp, stardust, missing, available); persistLast(); update(false); } }); }
            @Override public void cleanupPaused(String reason) { handler.post(() -> {
                if (currentCycle(token, round)) { cleanupNote = "Item cleanup paused"; update(true); } }); }
            @Override public void engineStopped() { handler.post(() -> {
                if (currentCycle(token, round)) { freezeElapsed();
                    status = session != null && session.buddyRestartPending() && !userStopped ? "Removing buddy…" : "Saving logs…";
                    persistLast(); update(true); } }); }
            @Override public void finished(String reason) { handler.post(() -> {
                FarmSession ended = session;
                if (ended != null) counters = counters.merge(ended.totals());
                if (ended != null && ended.buddyWasRemoved()) buddyRemovalHandled = true;
                synchronized (FarmService.class) { workerAlive = false; launchReadySession = ""; activeSession = null; }
                if (currentCycle(token, round)) {
                    boolean recover = !userStopped && ended != null && ended.recoveryReady();
                    String block = limitBlock();
                    if (recover && block.isEmpty()) {
                        recovery.begin(ended.playerIdentity());
                        status = "Restarting Pokémon GO…"; update(true); persistLast();
                        startCycle(true, ended.playerIdentity());
                    } else if (!userStopped && ended != null && ended.startupRetryReady() && block.isEmpty()) {
                        if (recovery.retry()) scheduleRecovery(token, round);
                        else finish("Startup failed after " + RecoveryAttempts.MAX_ATTEMPTS + " attempts");
                    } else finish(userStopped ? userStopReason : recover && !block.isEmpty() ? block : reason);
                }
                else { closing = false; status = reason; persistLast(); }
            }); }
        });
        current.skipBuddyRemoval(buddyRemovalHandled);
        current.verifyStartupOnly(verificationOnly);
        session = current; activeSession = current; workerAlive = true;
        new Thread(current, "rootcatch-farm").start();
    }
    private void scheduleRecovery(long token, long round) {
        session = null;
        status = "Restarting Pokémon GO · " + recovery.attempt() + "/" + RecoveryAttempts.MAX_ATTEMPTS;
        persistLast(); update(true);
        pendingRecovery = () -> {
            pendingRecovery = null;
            if (!userStopped && currentCycle(token, round)) startCycle(true, recovery.playerId());
        };
        handler.postDelayed(pendingRecovery, Math.min(3000L, (recovery.attempt() - 1) * 500L));
    }
    private void cancelRecovery() {
        if (pendingRecovery != null) handler.removeCallbacks(pendingRecovery);
        pendingRecovery = null;
    }
    private void stopSession(String reason) {
        userStopReason = reason; userStopped = true; closing = true; launchReadySession = ""; status = "Stopping…"; update(true);
        cancelRecovery();
        if (session != null) session.requestStop(reason); else finish(reason);
    }
    private boolean currentCycle(long token, long round) { return currentEpoch(token) && round == cycle; }
    private static void freezeElapsed() { savedElapsed = elapsed(); readyAt = 0; stoppedAt = 0; }
    private void finish(String reason) {
        handler.removeCallbacks(tick); cancelRecovery(); recovery.clear();
        status = reason; running = false; closing = false; session = null; activeSession = null; launchReadySession = "";
        freezeElapsed();
        persistLast();
        releaseWake(); stopForeground(STOP_FOREGROUND_DETACH); update(true); stopSelf();
    }
    private void persistLast() {
        persist(this);
    }
    private static void persist(Context context) {
        saved(context).edit().putString("status", status).putString("status_language", "en")
                .putInt("caught", counters.caught).putInt("fled", counters.fled).putInt("spun", counters.spun)
                .putInt("discarded", counters.discarded).putInt("transferred", counters.transferred)
                .putLong("xp", counters.xp).putLong("stardust", counters.stardust)
                .putLong("missing_rewards", counters.missingRewardReceipts).putBoolean("rewards_available", counters.rewardsAvailable)
                .putLong("elapsed", elapsed()).apply();
    }
    private boolean currentEpoch(long token) { synchronized (FarmService.class) { return running && token == epoch; } }
    static boolean notificationsVisible(Context context) {
        NotificationManager manager = context.getSystemService(NotificationManager.class);
        NotificationChannel channel = manager.getNotificationChannel(CHANNEL);
        return manager.areNotificationsEnabled() && (channel == null || channel.getImportance() != NotificationManager.IMPORTANCE_NONE);
    }
    private void update(boolean force) {
        long now = SystemClock.elapsedRealtime();
        if (!force && now - lastUpdate < 1000) return;
        lastUpdate = now;
        if (notificationsVisible(this)) getSystemService(NotificationManager.class).notify(NOTIFICATION, notification());
    }
    private Notification notification() {
        PendingIntent open = PendingIntent.getActivity(this, 0, new Intent(this, MainActivity.class),
                PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        PendingIntent stop = PendingIntent.getService(this, 1, new Intent(this, FarmService.class).setAction(STOP),
                PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        Notification.Builder builder = new Notification.Builder(this, CHANNEL).setSmallIcon(R.drawable.ic_notification)
                .setContentTitle("SilphMod · " + "Farmer" + " · " + (running ? status : "Finished"))
                .setContentText(counters.compact()).setContentIntent(open).setOnlyAlertOnce(true)
                .setSubText(counters.rewardLine())
                .setStyle(new Notification.BigTextStyle().bigText(counters.compact() + "\n" + (counters.rewardLine())
                        + "\nTime " + SessionCounters.elapsed(elapsed()) + "\n" + status()))
                .setOngoing(running).setAutoCancel(!running).setShowWhen(readyAt > 0)
                .setWhen(System.currentTimeMillis() - elapsed()).setUsesChronometer(running && readyAt > 0 && stoppedAt == 0);
        if (running && !closing) builder.addAction(new Notification.Action.Builder(null, "Stop", stop).build());
        return builder.build();
    }
    private void releaseWake() {
        if (wakeLock != null && wakeLock.isHeld()) wakeLock.release();
        wakeLock = null;
    }
    @Override public void onDestroy() {
        handler.removeCallbacks(tick);
        cancelRecovery();
        userStopped = true;
        launchReadySession = ""; activeSession = null;
        if (session != null) session.requestStop("Service stopped");
        synchronized (FarmService.class) {
            if (running && ownEpoch == epoch) {
                epoch++; running = false; closing = session != null; status = session != null ? "Saving logs…" : "Service stopped";
                freezeElapsed(); persistLast();
            }
        }
        releaseWake(); super.onDestroy();
    }
    @Override public IBinder onBind(Intent intent) { return null; }
}
