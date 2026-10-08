package dev.rootcatch.ugphone;

import org.junit.Test;
import static org.junit.Assert.*;

public final class SessionRewardCountersTest {
    @Test public void rewardTotalsSurviveOtherCountersAndUseLong() {
        SessionCounters counters = new SessionCounters().rewards(5_000_000_000L, 1500, 0, true)
                .update(5, 0, 1, 2).transfers(3).rewards(100, 100, 0, true);
        assertEquals(5_000_000_000L, counters.xp); assertEquals(1500, counters.stardust);
        assertEquals(5, counters.caught); assertEquals(3, counters.transferred);
        assertEquals("5,000,000,000", counters.rewardText(counters.xp));
    }
    @Test public void missingAndUnavailableAreNotPresentedAsExactZero() {
        SessionCounters counters = new SessionCounters(); assertEquals("—", counters.rewardText(0));
        counters = counters.rewards(1000, 100, 1, true).update(1, 0, 0, 0);
        assertEquals("1,000+", counters.rewardText(counters.xp));
        assertTrue(counters.rewardLine().contains("Stardust 100+"));
    }
    @Test public void aNewSessionDoesNotRestorePreviousRewards() {
        SessionCounters prior = new SessionCounters().rewards(9999, 9999, 1, true);
        SessionCounters next = new SessionCounters();
        assertEquals(9999, prior.xp); assertEquals(0, next.xp); assertEquals(0, next.stardust);
        assertEquals(0, next.missingRewardReceipts);
    }
}
