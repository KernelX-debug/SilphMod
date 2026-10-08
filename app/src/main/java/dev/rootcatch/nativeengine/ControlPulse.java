package dev.rootcatch.nativeengine;

import java.util.function.LongSupplier;

/** Pipe-only worker. A stale focus sample never authorizes actions, but does not mean a dead controller. */
final class ControlPulse implements AutoCloseable {
    static final long INTERVAL_MS = 500, FOCUS_FRESH_MS = 1500, REPORT_MS = 15000;
    interface Sender { void send(Pulse pulse) throws Exception; }
    interface Observer { void sample(Pulse pulse, long writeMs); }
    static final class Pulse {
        final long sequence, sentAt, focusAgeMs, gapMs, maxGapMs;
        final boolean foreground, focusFresh;
        Pulse(long sequence, long sentAt, long focusAge, long gap, long maximum, boolean foreground, boolean fresh) {
            this.sequence = sequence; this.sentAt = sentAt; this.focusAgeMs = focusAge;
            this.gapMs = gap; this.maxGapMs = maximum; this.foreground = foreground; this.focusFresh = fresh;
        }
    }
    private static final class Focus {
        final boolean game; final long observedAt;
        Focus(boolean game, long observedAt) { this.game = game; this.observedAt = observedAt; }
    }
    private final LongSupplier clock;
    private final Sender sender;
    private final Observer observer;
    private volatile Focus focus;
    private volatile boolean stopped;
    private volatile Exception failure;
    private volatile Pulse lastPulse;
    private Thread worker;
    private long sequence, lastSentAt, maxGap, nextReport;
    private boolean previousForeground, previousFresh;
    ControlPulse(LongSupplier clock, Sender sender, Observer observer) {
        this.clock = clock; this.sender = sender; this.observer = observer;
    }
    void publishFocus(boolean game, long queryStartedAt) { if (!stopped) focus = new Focus(game, queryStartedAt); }
    boolean focusAllowed() {
        Focus current = focus; long now = clock.getAsLong();
        return !stopped && current != null && current.game && now >= current.observedAt && now - current.observedAt <= FOCUS_FRESH_MS;
    }
    synchronized void start() {
        if (worker != null || stopped) throw new IllegalStateException("Control duplicated or stopped");
        worker = new Thread(() -> {
            try {
                while (!stopped) { pulseOnce(); Thread.sleep(INTERVAL_MS); }
            } catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
            catch (Exception error) { if (!stopped) failure = error; }
        }, "silph-control-pulse");
        worker.setDaemon(true); worker.start();
    }
    void pulseOnce() throws Exception {
        if (stopped) return;
        long now = clock.getAsLong();
        Focus current = focus;
        long age = current == null || now < current.observedAt ? -1 : now - current.observedAt;
        boolean fresh = age >= 0 && age <= FOCUS_FRESH_MS;
        long gap = sequence == 0 || now < lastSentAt ? 0 : now - lastSentAt;
        maxGap = Math.max(maxGap, gap);
        if (sequence == Long.MAX_VALUE) throw new IllegalStateException("Control sequence exhausted");
        Pulse pulse = new Pulse(++sequence, now, age, gap, maxGap, fresh && current.game, fresh);
        sender.send(pulse);
        lastSentAt = now; lastPulse = pulse;
        // Diagnostics happen after flushing control; callers must enqueue, not perform file IO here.
        if (now >= nextReport || gap > FOCUS_FRESH_MS || pulse.foreground != previousForeground || fresh != previousFresh) {
            try { observer.sample(pulse, Math.max(0, clock.getAsLong() - now)); }
            catch (RuntimeException ignored) { /* Nonessential diagnostics must not stop control. */ }
            nextReport = now + REPORT_MS;
        }
        previousForeground = pulse.foreground; previousFresh = fresh;
    }
    Exception failure() { return failure; }
    Pulse lastPulse() { return lastPulse; }
    synchronized boolean workerAlive() { return worker != null && worker.isAlive(); }
    @Override public void close() {
        stopped = true;
        Thread current;
        synchronized (this) { current = worker; }
        if (current != null && current != Thread.currentThread()) {
            current.interrupt();
            try { current.join(1500); } catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
        }
    }
}
