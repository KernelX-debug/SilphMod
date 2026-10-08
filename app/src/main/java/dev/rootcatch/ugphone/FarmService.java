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
import dev.rootcatch.R;
import dev.rootcatch.automation.AutomationConfig;
import dev.rootcatch.nativeengine.TransferOptions;

/** Explicit user start, accessible stop, no sticky restart or persisted capture queue. */
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
        return readyAt == 0 ? savedElapsed : Math.max(0, (stoppedAt > 0 ? stoppedAt : SystemClock.elapsedRealtime()) - readyAt);
    }
    static synchronized boolean beginExport() { if (isBusy()) return false; exporting = true; return true; }
    static synchronized void endExport() { exporting = false; }
    static synchronized void restoreLast(Context context) {
        if (running || workerAlive) return;
        SharedPreferences saved = context.getSharedPreferences("farm_last_result", 0);
        status = "en".equals(saved.getString("status_language", "")) ? saved.getString("status", "Ready") : "Ready";
        counters = new SessionCounters(saved.getInt("caught", 0), saved.getInt("fled", 0),
                saved.getInt("spun", 0), saved.getInt("discarded", 0), saved.getInt("transferred", 0),
                saved.getLong("xp", 0), saved.getLong("stardust", 0), saved.getLong("missing_rewards", 0),
                saved.getBoolean("rewards_available", false));
        savedElapsed = saved.getLong("elapsed", 0); readyAt = 0; stoppedAt = 0;
    }
    private final Runnable tick = new Runnable() {
        @Override public void run() {
            if (!running) return;
            if (session != null && !session.isStopping() && !notificationsVisible(FarmService.this)) stopSession("Notifications disabled");
            if (wakeLock != null && SystemClock.elapsedRealtime() >= nextWakeRenew) {
                wakeLock.acquire(120_000L); nextWakeRenew = SystemClock.elapsedRealtime() + 60_000;
            }
            update(true); handler.postDelayed(this, 2000);
        }
    };
    @Override public void onCreate() {
        super.onCreate();
        getSystemService(NotificationManager.class).createNotificationChannel(new NotificationChannel(CHANNEL,
                "SilphMod service", NotificationManager.IMPORTANCE_LOW));
    }
    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) { if (!running) stopSelf(); return START_NOT_STICKY; }
        if (STOP.equals(intent.getAction())) {
            if (session != null) stopSession(intent.getStringExtra("reason") == null ? "Stopped by user" : intent.getStringExtra("reason"));
            else { getSystemService(NotificationManager.class).cancel(NOTIFICATION); stopSelf(); }
            return START_NOT_STICKY;
        }
        if (!START.equals(intent.getAction())) { if (!running) stopSelf(); return START_NOT_STICKY; }
        if (running) return START_NOT_STICKY;
        // Meet Android's foreground-service deadline before any root prompt, file IO or validation.
        startForeground(NOTIFICATION, notification());
        synchronized (FarmService.class) {
            if (exporting || workerAlive) { stopForeground(STOP_FOREGROUND_REMOVE); stopSelf(); return START_NOT_STICKY; }
            running = true; closing = false; ownEpoch = ++epoch;
        }
        TransferOptions transfers;
        AutomationConfig config = AutomationConfig.load(this);
        try {
            transfers = new TransferOptions(intent.getBooleanExtra("transfer", false), intent.getBooleanExtra("keepShiny", true),
                    intent.getBooleanExtra("keepHundo", true), 0);
            FarmPolicy.validate(config.autoCatch, config.autoSpin, config.cleanBag, transfers.enabled, config.discardLimits);
            if (!notificationsVisible(this)) throw new IllegalArgumentException("Enable service notifications");
        } catch (IllegalArgumentException error) { finish(error.getMessage()); return START_NOT_STICKY; }
        counters = new SessionCounters(); readyAt = 0; stoppedAt = 0; savedElapsed = 0; launchReadySession = "";
        status = "Preparing…"; cleanupNote = ""; update(true);
        wakeLock = getSystemService(PowerManager.class).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "SilphMod:Farm");
        wakeLock.setReferenceCounted(false); wakeLock.acquire(120_000L);
        nextWakeRenew = SystemClock.elapsedRealtime() + 60_000;
        long token = ownEpoch;
        FarmSession current = new FarmSession(this, config, transfers, intent.getBooleanExtra("resolveError12", false),
                intent.getBooleanExtra("userApprovedGameStop", false)
                        && "USER_START_BUTTON".equals(intent.getStringExtra("startupAuthorization")), new FarmSession.Listener() {
            @Override public void launchReady(String ticket) { handler.post(() -> {
                if (currentEpoch(token) && !closing) launchReadySession = ticket; }); }
            @Override public void status(String message) { handler.post(() -> {
                if (currentEpoch(token)) { status = message; update(false); } }); }
            @Override public void ready() { handler.post(() -> {
                if (currentEpoch(token) && readyAt == 0) { readyAt = SystemClock.elapsedRealtime(); status = "Active"; update(true); } }); }
            @Override public void stats(int caught, int fled, int spun, int discarded) { handler.post(() -> {
                if (currentEpoch(token)) { counters = counters.update(caught, fled, spun, discarded); update(false); } }); }
            @Override public void transfers(int confirmed) { handler.post(() -> {
                if (currentEpoch(token)) { counters = counters.transfers(confirmed); update(false); } }); }
            @Override public void rewards(long xp, long stardust, long missing, boolean available) { handler.post(() -> {
                if (currentEpoch(token)) { counters = counters.rewards(xp, stardust, missing, available); update(false); } }); }
            @Override public void cleanupPaused(String reason) { handler.post(() -> {
                if (currentEpoch(token)) { cleanupNote = "Item cleanup paused"; update(true); } }); }
            @Override public void engineStopped() { handler.post(() -> {
                if (currentEpoch(token)) { closing = true; stoppedAt = SystemClock.elapsedRealtime(); status = "Saving logs…"; update(true); } }); }
            @Override public void finished(String reason) { handler.post(() -> {
                // onDestroy may have invalidated UI callbacks; the worker still owns the export lock until here.
                synchronized (FarmService.class) { workerAlive = false; launchReadySession = ""; activeSession = null; }
                if (currentEpoch(token)) finish(reason);
                else { closing = false; status = reason; persistLast(); }
            }); }
        });
        session = current; activeSession = current; workerAlive = true; handler.postDelayed(tick, 2000);
        new Thread(current, "rootcatch-farm").start();
        return START_NOT_STICKY;
    }
    private void stopSession(String reason) {
        if (session == null || session.isStopping()) return;
        closing = true; launchReadySession = ""; status = "Stopping…"; update(true); session.requestStop(reason);
    }
    private void finish(String reason) {
        handler.removeCallbacks(tick); status = reason; running = false; closing = false; session = null; activeSession = null; launchReadySession = "";
        if (stoppedAt == 0) stoppedAt = SystemClock.elapsedRealtime();
        savedElapsed = elapsed();
        persistLast();
        releaseWake(); stopForeground(STOP_FOREGROUND_DETACH); update(true); stopSelf();
    }
    private void persistLast() {
        getSharedPreferences("farm_last_result", 0).edit().putString("status", status).putString("status_language", "en")
                .putInt("caught", counters.caught).putInt("fled", counters.fled).putInt("spun", counters.spun)
                .putInt("discarded", counters.discarded).putInt("transferred", counters.transferred)
                .putLong("xp", counters.xp).putLong("stardust", counters.stardust)
                .putLong("missing_rewards", counters.missingRewardReceipts).putBoolean("rewards_available", counters.rewardsAvailable)
                .putLong("elapsed", savedElapsed).apply();
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
                .setContentTitle("SilphMod · " + (running ? status : "Finished"))
                .setContentText(counters.compact()).setContentIntent(open).setOnlyAlertOnce(true)
                .setSubText(counters.rewardLine())
                .setStyle(new Notification.BigTextStyle().bigText(counters.compact() + "\n" + counters.rewardLine()
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
        launchReadySession = ""; activeSession = null;
        if (session != null) session.requestStop("Service stopped");
        synchronized (FarmService.class) {
            if (session != null && ownEpoch == epoch) {
                epoch++; running = false; closing = true; status = "Saving logs…";
                if (stoppedAt == 0) stoppedAt = SystemClock.elapsedRealtime();
                savedElapsed = elapsed(); persistLast();
            }
        }
        releaseWake(); super.onDestroy();
    }
    @Override public IBinder onBind(Intent intent) { return null; }
}
