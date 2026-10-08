package dev.rootcatch.ugphone;

import org.junit.Test;
import static org.junit.Assert.*;

public final class FarmPolicyTest {
    @Test public void boundariesAreExplicitAndNotSilentlyClamped() {
        assertEquals(0, FarmPolicy.integer("0", 0, 1000, "Límite"));
        assertEquals(1000, FarmPolicy.integer(" 1000 ", 0, 1000, "Límite"));
        assertEquals(250, FarmPolicy.integer("250", 250, 3000, "Interval"));
        for (String invalid : new String[]{"", "-1", "1.5", "1e3", "9999999999", "1001"})
            assertThrows(IllegalArgumentException.class, () -> FarmPolicy.integer(invalid, 0, 1000, "Límite"));
    }
    @Test public void transfersRequireCaptureAndCleanupRequiresExplicitSelection() {
        FarmPolicy.validate(true, true, false, false, "");
        FarmPolicy.validate(false, false, true, false, "Potion=0");
        assertThrows(IllegalArgumentException.class, () -> FarmPolicy.validate(false, false, false, false, ""));
        assertThrows(IllegalArgumentException.class, () -> FarmPolicy.validate(false, true, false, true, ""));
        assertThrows(IllegalArgumentException.class, () -> FarmPolicy.validate(true, true, true, false, ""));
        assertThrows(IllegalArgumentException.class, () -> FarmPolicy.validate(true, true, true, false, "MasterBall=0"));
    }
    @Test public void ticksHandleSpacesAndParenthesesInProcessNames() {
        String stat = "34029 (game ) worker) S " + "1 ".repeat(18) + "1406655 0 0";
        assertEquals(1406655L, FarmPolicy.processStartTicks(stat));
        assertThrows(IllegalArgumentException.class, () -> FarmPolicy.processStartTicks("short"));
        assertThrows(IllegalArgumentException.class, () -> FarmPolicy.processStartTicks(stat.replace("1406655", "0")));
    }
}
