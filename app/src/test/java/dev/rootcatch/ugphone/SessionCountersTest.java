package dev.rootcatch.ugphone;
import org.junit.Test;
import static org.junit.Assert.*;
public final class SessionCountersTest {
    @Test public void onlyMonotonicConfirmedCountsAreDisplayed() {
        SessionCounters counts = new SessionCounters().update(3, 1, 2, 5).transfers(2).update(1, 0, 1, 2).transfers(1);
        assertEquals(3, counts.caught); assertEquals(2, counts.spun); assertEquals(5, counts.discarded); assertEquals(2, counts.transferred);
    }
    @Test public void continuousTimeHasNoMinuteOrHourLimit() {
        assertEquals("03:45:01", SessionCounters.elapsed(13_501_000));
        assertEquals("00:00:00", SessionCounters.elapsed(-1));
    }
}
