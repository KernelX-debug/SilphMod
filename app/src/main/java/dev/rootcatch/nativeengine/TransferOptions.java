package dev.rootcatch.nativeengine;

/** Explicit consent for current-session captures only. Never persists a queue. */
public final class TransferOptions {
    public static final TransferOptions OFF = new TransferOptions(false, true, true, 0);
    public final boolean enabled, keepShiny, keepHundo, keepDynamax, keepHatched, keepBackground, keepShinyBackground, keepFieldResearch;
    public final int maximum;
    public TransferOptions(boolean enabled, boolean keepShiny, boolean keepHundo, int maximum) {
        this(enabled, keepShiny, keepHundo, false, false, false, false, maximum);
    }
    public TransferOptions(boolean enabled, boolean keepShiny, boolean keepHundo, boolean keepDynamax,
                           boolean keepHatched, boolean keepBackground, boolean keepShinyBackground, int maximum) {
        this(enabled, keepShiny, keepHundo, keepDynamax, keepHatched, keepBackground, keepShinyBackground, true, maximum);
    }
    public TransferOptions(boolean enabled, boolean keepShiny, boolean keepHundo, boolean keepDynamax,
                           boolean keepHatched, boolean keepBackground, boolean keepShinyBackground, boolean keepFieldResearch, int maximum) {
        if (maximum < 0 || maximum > 1000) throw new IllegalArgumentException("Transfer limit: 0–1000 (0 = unlimited)");
        this.enabled = enabled; this.keepShiny = keepShiny; this.keepHundo = keepHundo; this.maximum = maximum;
        this.keepDynamax = keepDynamax; this.keepHatched = keepHatched;
        this.keepBackground = keepBackground; this.keepShinyBackground = keepShinyBackground;
        this.keepFieldResearch = keepFieldResearch;
    }
    public String proofKey() { return enabled + ":" + keepShiny + ":" + keepHundo + ":" + maximum
            + ":" + keepDynamax + ":" + keepHatched + ":" + keepBackground + ":" + keepShinyBackground + ":" + keepFieldResearch; }
}
