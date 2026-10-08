package dev.rootcatch.ugphone;

import dev.rootcatch.nativeengine.EnginePolicy;
import java.util.Map;

/** Pure input policies, shared by UI, worker and tests. */
final class FarmPolicy {
    static final String BUILD = dev.rootcatch.BuildConfig.VERSION_NAME;
    static final String EXPECTED_AGENT_SHA = AgentBuild.SHA256;
    static final long STARTUP_LIMIT_MS = 180_000L;
    private FarmPolicy() { }
    static int integer(String text, int min, int max, String label) {
        if (text == null || !text.trim().matches("[0-9]{1,9}"))
            throw new IllegalArgumentException(label + ": enter a number");
        int value = Integer.parseInt(text.trim());
        if (value < min || value > max) throw new IllegalArgumentException(label + ": " + min + "–" + max);
        return value;
    }
    static void validate(boolean capture, boolean spin, boolean clean, boolean transfer, String limits) {
        if (!capture && !spin && !clean) throw new IllegalArgumentException("Select a feature");
        if (transfer && !capture) throw new IllegalArgumentException("Enable catches to transfer new catches");
        Map<String, Integer> parsed = EnginePolicy.parseLimits(limits);
        if (clean && parsed.isEmpty()) throw new IllegalArgumentException("Select items to clean");
    }
    static long processStartTicks(String stat) {
        int closing = stat.lastIndexOf(')');
        if (closing < 1) throw new IllegalArgumentException("Invalid process identity");
        String[] fields = stat.substring(closing + 1).trim().split("\\s+");
        if (fields.length <= 19 || !fields[19].matches("[0-9]+"))
            throw new IllegalArgumentException("Could not read process start");
        long ticks = Long.parseLong(fields[19]);
        if (ticks <= 0) throw new IllegalArgumentException("Invalid process start");
        return ticks;
    }
}
