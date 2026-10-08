package dev.rootcatch.nativeengine;

/** Explicit consent for current-session captures only. Never persists a queue. */
public final class TransferOptions {
    public static final TransferOptions OFF = new TransferOptions(false, true, true, 0);
    public final boolean enabled, keepShiny, keepHundo;
    public final int maximum;
    public TransferOptions(boolean enabled, boolean keepShiny, boolean keepHundo, int maximum) {
        if (maximum < 0 || maximum > 1000) throw new IllegalArgumentException("Transfer limit: 0–1000 (0 = unlimited)");
        this.enabled = enabled; this.keepShiny = keepShiny; this.keepHundo = keepHundo; this.maximum = maximum;
    }
    public String proofKey() { return enabled + ":" + keepShiny + ":" + keepHundo + ":" + maximum; }
}
