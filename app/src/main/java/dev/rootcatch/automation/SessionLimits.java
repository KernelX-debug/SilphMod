package dev.rootcatch.automation;

public final class SessionLimits {
    public static final int DEFAULT_CATCHES = 4800;
    public static final int DEFAULT_SPINS = 2400;

    private SessionLimits() { }

    public static int savedLimit(int value, int fallback) {
        return value > 0 ? value : fallback;
    }

    public static String status(boolean catches, boolean spins) {
        if (catches && spins) return "Session capture and spin limits reached";
        if (catches) return "Session capture limit reached";
        if (spins) return "Session spin limit reached";
        return "";
    }

    public static String reason(String kind) {
        if ("capture".equals(kind)) return "Session capture limit reached";
        if ("spin".equals(kind)) return "Session spin limit reached";
        throw new IllegalArgumentException("Invalid session limit kind");
    }

    public static String startBlock(int caught, int spun, int maxCatches, int maxSpins) {
        if (maxCatches <= 0 || maxSpins <= 0) return "Set valid limits";
        if (caught >= maxCatches) return "Capture limit reached";
        if (spun >= maxSpins) return "Spin limit reached";
        return "";
    }

    public static boolean reached(String kind, int caught, int spun, int maxCatches, int maxSpins,
                                  boolean autoCatch, boolean autoSpin) {
        return maxCatches > 0 && maxSpins > 0 && caught >= 0 && spun >= 0
                && (("capture".equals(kind) && autoCatch && caught >= maxCatches)
                || ("spin".equals(kind) && autoSpin && spun >= maxSpins));
    }
}
