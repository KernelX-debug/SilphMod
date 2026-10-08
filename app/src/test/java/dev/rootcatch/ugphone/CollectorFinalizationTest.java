package dev.rootcatch.ugphone;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.junit.Test;
import static org.junit.Assert.*;

public final class CollectorFinalizationTest {
    @Test public void unboundTargetKeepsSystemSourcesWithoutReadingAnInvalidPid() {
        for (long[] identity : new long[][]{{0, 0}, {1, 99}, {99, 0}}) {
            Map<String, String> sources = DiagnosticCommands.snapshots((int) identity[0], identity[1]);
            assertEquals(8, sources.size()); assertFalse(sources.containsKey("game-process"));
            assertTrue(sources.containsKey("logcat")); assertTrue(sources.containsKey("process-list"));
            assertTrue(sources.containsKey("activity"));
            for (String command : sources.values()) { assertFalse(command.contains("/proc/0/")); assertFalse(command.contains("/proc/1/")); }
        }
    }
    @Test public void boundTargetRetainsStartTickValidationAndAllSources() {
        Map<String, String> sources = DiagnosticCommands.snapshots(123, 456);
        assertEquals(9, sources.size()); assertTrue(sources.get("game-process").contains("EXPECTED_START=456"));
        assertTrue(sources.get("game-process").contains("/proc/123/status"));
        try { DiagnosticCommands.process(0, 0, true); fail("Invalid direct process query accepted"); }
        catch (IllegalArgumentException expected) { }
    }
    @Test public void normalFinalizationRunsInOrder() {
        List<String> steps = new ArrayList<>();
        CollectorFinalization.run(() -> steps.add("gather"), () -> steps.add("stop"), () -> steps.add("end"), () -> steps.add("close"));
        assertEquals(List.of("gather", "stop", "end", "close"), steps);
    }
    @Test public void eachFailureStillRunsAllLaterCleanupSteps() {
        for (String failing : List.of("gather", "stop", "end")) {
            List<String> steps = new ArrayList<>();
            try {
                CollectorFinalization.run(() -> step(steps, "gather", failing), () -> step(steps, "stop", failing),
                        () -> step(steps, "end", failing), () -> step(steps, "close", failing));
                fail("Failure not propagated");
            } catch (IllegalStateException expected) { assertEquals(failing, expected.getMessage()); }
            assertEquals(List.of("gather", "stop", "end", "close"), steps);
        }
    }
    private static void step(List<String> steps, String step, String failing) {
        steps.add(step); if (step.equals(failing)) throw new IllegalStateException(step);
    }
}
