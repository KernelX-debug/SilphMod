package dev.rootcatch.ugphone;

import dev.rootcatch.automation.SessionLimits;
import org.junit.Test;
import static org.junit.Assert.*;

public final class SessionLimitsTest {
    @Test public void defaultsAndInvalidSavedValues() {
        assertEquals(4800, SessionLimits.DEFAULT_CATCHES);
        assertEquals(2400, SessionLimits.DEFAULT_SPINS);
        for (int value : new int[]{0, -1, Integer.MIN_VALUE}) {
            assertEquals(4800, SessionLimits.savedLimit(value, SessionLimits.DEFAULT_CATCHES));
            assertEquals(2400, SessionLimits.savedLimit(value, SessionLimits.DEFAULT_SPINS));
        }
        assertEquals(1, SessionLimits.savedLimit(1, 4800));
        assertEquals(Integer.MAX_VALUE, SessionLimits.savedLimit(Integer.MAX_VALUE, 2400));
    }
    @Test public void inputsAcceptPositiveLimitsAndRejectInvalidNumbers() {
        assertEquals(4800, FarmPolicy.integer("4800", 1, Integer.MAX_VALUE, "Maximum catches"));
        assertEquals(1, FarmPolicy.integer("1", 1, Integer.MAX_VALUE, "Maximum catches"));
        for (String value : new String[]{"", "0", "-1", "1.5", "2147483648", "not a number"})
            assertThrows(IllegalArgumentException.class, () -> FarmPolicy.integer(value, 1, Integer.MAX_VALUE, "Maximum catches"));
    }
    @Test public void eachLimitHasAnEnglishStatus() {
        assertEquals("", SessionLimits.status(false, false));
        assertEquals("Session capture limit reached", SessionLimits.status(true, false));
        assertEquals("Session spin limit reached", SessionLimits.status(false, true));
        assertEquals("Session capture and spin limits reached", SessionLimits.status(true, true));
    }
    @Test public void finalLimitReasonsMatchTheAgentRegardlessOfClosureVerification() {
        assertEquals("Session capture limit reached", SessionLimits.reason("capture"));
        assertEquals("Session spin limit reached", SessionLimits.reason("spin"));
        assertThrows(IllegalArgumentException.class, () -> SessionLimits.reason("discard"));
        assertThrows(IllegalArgumentException.class, () -> SessionLimits.reason(""));
    }
    @Test public void onlyEnabledConfirmedPositiveLimitsCanAuthorizeGameClosure() {
        assertTrue(SessionLimits.reached("capture", 1, 0, 1, 2, true, true));
        assertTrue(SessionLimits.reached("spin", 0, 2, 1, 2, true, true));
        assertFalse(SessionLimits.reached("capture", 0, 0, 1, 2, true, true));
        assertFalse(SessionLimits.reached("capture", 1, 0, 1, 2, false, true));
        assertFalse(SessionLimits.reached("spin", 0, 2, 1, 2, true, false));
        assertFalse(SessionLimits.reached("discard", 1, 2, 1, 2, true, true));
        assertFalse(SessionLimits.reached("capture", -1, 2, 1, 2, true, true));
        assertFalse(SessionLimits.reached("spin", 1, 2, 0, 2, true, true));
    }
}
