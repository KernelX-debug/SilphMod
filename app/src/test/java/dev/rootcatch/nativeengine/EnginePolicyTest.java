package dev.rootcatch.nativeengine;

import org.junit.Test;
import static org.junit.Assert.*;

public final class EnginePolicyTest {
    @Test public void normalStopPipeCloseIsDistinctFromUnexpectedConnectionFailure() {
        assertTrue(EnginePolicy.expectedPipeClose(true, new java.io.IOException("Stream Closed")));
        assertTrue(EnginePolicy.expectedPipeClose(true, new java.io.IOException("Broken pipe")));
        assertFalse(EnginePolicy.expectedPipeClose(false, new java.io.IOException("Stream Closed")));
        assertFalse(EnginePolicy.expectedPipeClose(true, new java.io.IOException("No space left on device")));
        assertFalse(EnginePolicy.expectedPipeClose(true, new IllegalStateException("Stream Closed")));
    }
    @Test public void briefOrEmptyReadingCannotProveStability() {
        assertTrue(EnginePolicy.readStabilityValid(true, 20, 9500, 4));
        assertFalse(EnginePolicy.readStabilityValid(false, 20, 9500, 4));
        assertFalse(EnginePolicy.readStabilityValid(true, 0, 9500, 4));
        assertFalse(EnginePolicy.readStabilityValid(true, 20, 30, 4));
        assertFalse(EnginePolicy.readStabilityValid(true, 20, 9500, 3));
    }
    @Test public void requiresExactKnownGameBuild() {
        assertTrue(EnginePolicy.supports(EnginePolicy.GAME, "0.431.0", 2026092402L));
        assertFalse(EnginePolicy.supports(EnginePolicy.GAME + ".ares", "0.431.0", 2026092402L));
        assertFalse(EnginePolicy.supports(EnginePolicy.GAME, "0.432.0", 2026092402L));
        assertFalse(EnginePolicy.supports(EnginePolicy.GAME, "0.431.0", 1));
    }
    @Test public void parsesExplicitKeeps() {
        assertEquals(Integer.valueOf(50), EnginePolicy.parseLimits(EnginePolicy.DEFAULT_LIMITS).get("Potion"));
        assertEquals(Integer.valueOf(0), EnginePolicy.parseLimits("PokeBall=0").get("PokeBall"));
        assertTrue(EnginePolicy.parseLimits("").isEmpty());
    }
    @Test public void catalogIncludesFiveBerriesButNeverSafariOrMasterBall() {
        assertEquals(50, ItemCatalog.ENTRIES.size()); assertEquals(33, EnginePolicy.ITEMS.size());
        assertEquals(Integer.valueOf(706), EnginePolicy.ITEMS.get("GoldenRazzBerry"));
        assertEquals(Integer.valueOf(708), EnginePolicy.ITEMS.get("GoldenPinapBerry"));
        for (String name : new String[]{"MasterBall", "PremierBall", "WildBall", "BlukBerry", "WeparBerry", "GoldenNanabBerry", "IncubatorBasic", "FreeRaidTicket"})
            assertFalse(EnginePolicy.ITEMS.containsKey(name));
    }
    @Test public void specialConsumablesRequireExplicitTypeInLimits() {
        assertFalse(EnginePolicy.parseLimits(EnginePolicy.DEFAULT_LIMITS).containsKey("LuckyEgg"));
        assertEquals(Integer.valueOf(5), EnginePolicy.parseLimits("LuckyEgg=5,SunStone=10").get("LuckyEgg"));
        assertEquals(Integer.valueOf(10), EnginePolicy.parseLimits("LuckyEgg=5,SunStone=10").get("SunStone"));
    }
    @Test public void canonicalCleanupProofKeyIgnoresOrderNotAmounts() {
        assertEquals(EnginePolicy.limitsKey("Revive=10,Potion=20"), EnginePolicy.limitsKey(" Potion = 20 , Revive = 10 "));
        assertNotEquals(EnginePolicy.limitsKey("Potion=20"), EnginePolicy.limitsKey("Potion=0"));
        assertEquals("", EnginePolicy.limitsKey(""));
    }
    @Test public void rejectsProtectedItemsAndMalformedLimits() {
        for (String text : new String[]{"MasterBall=0", "RareCandy=0", "Potion=-1", "Potion=10001",
                "Potion=3,Potion=4", "Potion=3,", "Potion=3=4", "Potion=no"}) {
            try { EnginePolicy.parseLimits(text); fail("Accepted " + text); }
            catch (IllegalArgumentException expected) { }
        }
    }
    @Test public void acceptsOnlySingleNonSystemPid() {
        assertEquals(1234, EnginePolicy.uniquePid("1234\n"));
        for (String output : new String[]{"", "1", "0", "-9", "1234 1235", "12;reboot", "9999999999"})
            assertEquals(-1, EnginePolicy.uniquePid(output));
    }
    @Test public void shellQuoteKeepsMetacharactersLiteral() {
        assertEquals("'a'\\''b'", EnginePolicy.quote("a'b"));
        assertEquals("'$(reboot); x'", EnginePolicy.quote("$(reboot); x"));
        try { EnginePolicy.quote("a\nb"); fail(); } catch (IllegalArgumentException expected) { }
    }
    @Test public void neverKillsGameOrBroadProcessMatches() {
        try { EnginePolicy.killCommand(123, 123, "/data/app/helper.so"); fail(); }
        catch (IllegalArgumentException expected) { }
        String command = EnginePolicy.killCommand(124, 123, "/data/app/helper.so");
        assertTrue(command.contains("/proc/124/exe"));
        assertTrue(command.contains("kill -TERM 124"));
        assertFalse(command.contains("pkill"));
    }
    @Test public void attachIsInteractiveAndNeverEternalized() {
        String cmd = EnginePolicy.attachCommand("/data/app/helper.so", "/data/user/0/agent.js", 123, "{\"key\":\"'x\"}");
        assertTrue(cmd.contains(" -i -P "));
        assertFalse(cmd.contains(" -e "));
        assertFalse(cmd.contains(" -H "));
    }
    @Test public void requiresCompletePointerWidthValidation() {
        assertTrue(EnginePolicy.gcValidationValid(true, 64, 4, 4, true, true, true));
        assertFalse(EnginePolicy.gcValidationValid(true, 32, 4, 4, true, true, true));
        assertFalse(EnginePolicy.gcValidationValid(false, 64, 4, 4, true, true, true));
        assertFalse(EnginePolicy.gcValidationValid(true, 64, 3, 4, true, true, true));
        assertFalse(EnginePolicy.gcValidationValid(true, 64, 4, 3, true, true, true));
    }
    @Test public void requiresBothReferenceProbesAndSuccessfulFree() {
        assertFalse(EnginePolicy.gcValidationValid(true, 64, 4, 4, false, true, true));
        assertFalse(EnginePolicy.gcValidationValid(true, 64, 4, 4, true, false, true));
        assertFalse(EnginePolicy.gcValidationValid(true, 64, 4, 4, true, true, false));
    }
    @Test public void cleanupMustConfirmZeroRemainingReferences() {
        assertTrue(EnginePolicy.gcCleanupValid(true, 64, 0));
        assertFalse(EnginePolicy.gcCleanupValid(false, 64, 0));
        assertFalse(EnginePolicy.gcCleanupValid(true, 32, 0));
        assertFalse(EnginePolicy.gcCleanupValid(true, 64, 1));
        assertFalse(EnginePolicy.gcCleanupValid(true, 64, -1));
    }
}
