package dev.rootcatch.nativeengine;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;
import org.junit.Test;
import static org.junit.Assert.*;

public final class ControlPulseTest {
    private static final class Harness {
        long now = 10000;
        final List<ControlPulse.Pulse> sent = new ArrayList<>(), diagnostics = new ArrayList<>();
        final ControlPulse channel = new ControlPulse(() -> now, sent::add, (pulse, duration) -> diagnostics.add(pulse));
    }
    @Test public void noFocusReadMaintainsTransportButNeverAuthorizesActions() throws Exception {
        Harness h = new Harness(); h.channel.pulseOnce();
        assertFalse(h.sent.get(0).foreground); assertFalse(h.sent.get(0).focusFresh);
        assertEquals(-1, h.sent.get(0).focusAgeMs); assertFalse(h.channel.focusAllowed());
    }
    @Test public void onlyFreshPositiveFocusCanAuthorizeActions() throws Exception {
        Harness h = new Harness(); h.channel.publishFocus(true, h.now); h.channel.pulseOnce();
        assertTrue(h.sent.get(0).foreground); assertTrue(h.channel.focusAllowed());
        h.channel.publishFocus(false, h.now); h.channel.pulseOnce();
        assertFalse(h.sent.get(1).foreground); assertFalse(h.channel.focusAllowed());
    }
    @Test public void stalledFocusReadDoesNotStallControlOrReuseStaleAuthorization() throws Exception {
        Harness h = new Harness(); h.channel.publishFocus(true, h.now); h.channel.pulseOnce();
        for (int i = 1; i <= 14; i++) { h.now += 500; h.channel.pulseOnce(); }
        assertEquals(15, h.sent.size()); assertFalse(h.sent.get(14).foreground);
        assertEquals(7000, h.sent.get(14).focusAgeMs); assertEquals(500, h.sent.get(14).gapMs);
        assertFalse(h.channel.focusAllowed());
    }
    @Test public void longReadUsesQueryStartNotCompletionAsFreshnessEvidence() throws Exception {
        Harness h = new Harness(); long started = h.now; h.now += 3500;
        h.channel.publishFocus(true, started); h.channel.pulseOnce();
        assertFalse(h.sent.get(0).foreground); assertEquals(3500, h.sent.get(0).focusAgeMs);
    }
    @Test public void returningFocusRequiresANewPositiveSample() throws Exception {
        Harness h = new Harness(); h.channel.publishFocus(true, h.now); h.now += 5000; h.channel.pulseOnce();
        assertFalse(h.sent.get(0).foreground);
        h.channel.publishFocus(true, h.now); h.channel.pulseOnce(); assertTrue(h.sent.get(1).foreground);
    }
    @Test public void pulseSequencesAndSchedulingGapsAreExplicitAndMonotonic() throws Exception {
        Harness h = new Harness(); h.channel.pulseOnce(); h.now += 500; h.channel.pulseOnce(); h.now += 4900; h.channel.pulseOnce();
        assertEquals(1, h.sent.get(0).sequence); assertEquals(3, h.sent.get(2).sequence);
        assertEquals(4900, h.sent.get(2).gapMs); assertEquals(4900, h.sent.get(2).maxGapMs);
        assertEquals(15400, h.sent.get(2).sentAt); assertEquals(2, h.diagnostics.size());
    }
    @Test public void regressedMonotonicClockDoesNotAuthorizeStaleFocus() throws Exception {
        Harness h = new Harness(); h.channel.publishFocus(true, h.now); h.now -= 1000; h.channel.pulseOnce();
        assertFalse(h.sent.get(0).foreground); assertFalse(h.channel.focusAllowed());
    }
    @Test public void closeIsIdempotentAndCannotRenewControlOrFocus() throws Exception {
        Harness h = new Harness(); h.channel.close(); h.channel.close(); h.channel.publishFocus(true, h.now); h.channel.pulseOnce();
        assertTrue(h.sent.isEmpty()); assertFalse(h.channel.focusAllowed()); assertFalse(h.channel.workerAlive());
    }
    @Test public void nonessentialDiagnosticFailureCannotStopHeartbeatSending() throws Exception {
        AtomicLong clock = new AtomicLong(10000); List<ControlPulse.Pulse> sent = new ArrayList<>();
        ControlPulse channel = new ControlPulse(clock::get, sent::add, (pulse, duration) -> { throw new IllegalStateException("log full"); });
        channel.pulseOnce(); clock.addAndGet(500); channel.pulseOnce(); assertEquals(2, sent.size());
    }
    @Test public void workerKeepsSendingWhileCallerCannotRefreshFocus() throws Exception {
        CountDownLatch sent = new CountDownLatch(3);
        ControlPulse channel = new ControlPulse(() -> System.nanoTime() / 1000000,
                pulse -> { assertFalse(pulse.foreground); sent.countDown(); }, (pulse, duration) -> { });
        try { channel.start(); assertTrue(sent.await(3, TimeUnit.SECONDS)); assertNull(channel.failure()); }
        finally { channel.close(); }
        assertFalse(channel.workerAlive());
    }
    @Test public void transportFailureIsObservableRatherThanSilentlyRetryingWrites() throws Exception {
        CountDownLatch attempted = new CountDownLatch(1);
        ControlPulse channel = new ControlPulse(() -> 10000, pulse -> {
            attempted.countDown(); throw new java.io.IOException("broken pipe");
        }, (pulse, duration) -> { });
        try {
            channel.start(); assertTrue(attempted.await(1, TimeUnit.SECONDS));
            long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(1);
            while (channel.failure() == null && System.nanoTime() < deadline) Thread.yield();
            assertNotNull(channel.failure()); assertEquals("broken pipe", channel.failure().getMessage());
        } finally { channel.close(); }
    }
    @Test public void duplicateWorkerStartIsRejected() {
        ControlPulse channel = new ControlPulse(() -> 10000, pulse -> { }, (pulse, duration) -> { });
        try { channel.start(); try { channel.start(); fail("duplicate worker"); } catch (IllegalStateException expected) { } }
        finally { channel.close(); }
    }
}
