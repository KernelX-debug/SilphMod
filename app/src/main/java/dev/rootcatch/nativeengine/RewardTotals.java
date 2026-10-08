package dev.rootcatch.nativeengine;

/** Only exact JSON integer totals are accepted; no int32 truncation or string parsing. */
public final class RewardTotals {
    private static final long MAX_EXACT = 9_007_199_254_740_991L;
    public final long xp, stardust, missing;
    public final boolean available;
    public RewardTotals() { this(0, 0, 0, false); }
    private RewardTotals(long xp, long stardust, long missing, boolean available) {
        this.xp = xp; this.stardust = stardust; this.missing = missing; this.available = available;
    }
    public RewardTotals update(Object xpValue, Object dustValue, Object missingValue, boolean available) {
        long experience = exact(xpValue), dust = exact(dustValue), omitted = exact(missingValue);
        return new RewardTotals(Math.max(xp, experience), Math.max(stardust, dust), Math.max(missing, omitted), available);
    }
    private static long exact(Object value) {
        if (!(value instanceof Number)) throw new IllegalArgumentException("Non-numeric reward");
        double number = ((Number) value).doubleValue();
        if (!Double.isFinite(number) || number < 0 || number > MAX_EXACT || number != Math.rint(number))
            throw new IllegalArgumentException("Reward outside exact integer range");
        return (long) number;
    }
}
