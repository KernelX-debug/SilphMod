package dev.rootcatch.nativeengine;

import org.junit.Test;
import static org.junit.Assert.*;

public final class RewardTotalsTest {
    @Test public void exactLongTotalsAreMonotonic() {
        RewardTotals totals = new RewardTotals().update(5_000_000_000L, 100L, 1L, true)
                .update(100, 0, 0, true);
        assertEquals(5_000_000_000L, totals.xp); assertEquals(100, totals.stardust);
        assertEquals(1, totals.missing); assertTrue(totals.available);
    }
    @Test public void invalidProtocolNumbersAreRejectedWithoutMutation() {
        RewardTotals totals = new RewardTotals().update(100, 10, 0, true);
        Object[] invalid = {null, "100", -1, Double.NaN, Double.POSITIVE_INFINITY, 0.25, 9_007_199_254_740_992L};
        for (Object value : invalid) {
            try { totals.update(value, 0, 0, true); fail("Accepted invalid reward: " + value); }
            catch (IllegalArgumentException expected) { }
        }
        assertEquals(100, totals.xp); assertEquals(10, totals.stardust);
    }
}
