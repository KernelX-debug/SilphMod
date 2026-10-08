package dev.rootcatch.ugphone;

import org.junit.Test;
import static org.junit.Assert.*;

public final class EnglishStatusTest {
    @Test public void translatedStatusPrefixesStillReportStorageAndBallPauses() {
        assertEquals("Pokémon storage full", FarmSession.compactStatus("Catches paused: Pokémon storage 300/300; existing collection unchanged"));
        assertEquals("No Poké Balls", FarmSession.compactStatus("Catches paused: no ordinary Poké Balls; spinning and cleanup remain available"));
        assertEquals("Bag full", FarmSession.compactStatus("Item bag full: spins limited, catches remain available; check cleanup"));
    }
    @Test public void translatedStatusPrefixesStillResumeAndFinishCleanup() {
        assertEquals("Active", FarmSession.compactStatus("Internal engine active"));
        assertEquals("Active", FarmSession.compactStatus("Internal resumed: game in foreground"));
        assertEquals("Paused", FarmSession.compactStatus("Internal paused: game not in foreground"));
        assertEquals("Cleaning item bag…", FarmSession.compactStatus("Initial cleanup · checking per-type surplus · items 50"));
        assertEquals("Active", FarmSession.compactStatus("Initial cleanup finished: selected types within their limits"));
        assertEquals("Cleanup quota reached", FarmSession.compactStatus("TOTAL cleanup quota reached (20); catches/spins continue"));
        assertEquals("Transfer quota reached", FarmSession.compactStatus("Transfer quota reached (3); catches and spins continue"));
    }
    @Test public void notificationCountersUseEnglishLabels() {
        SessionCounters counts = new SessionCounters(4, 1, 2, 50, 3).rewards(1000, 400, 0, true);
        assertEquals("Catches 4 · Spins 2 · Items 50 · Transferred 3", counts.compact());
        assertEquals("XP 1,000 · Stardust 400", counts.rewardLine());
        assertTrue(counts.detailed().contains("Fled 1 · confirmed results only"));
    }
}
