package dev.rootcatch.nativeengine;

public final class EmptyMapWatchdog {
    public static final long EMPTY_MS = 300_000L;
    private static final long SAMPLE_GAP_MS = 15_000L;
    private long emptySince = -1, lastSample = -1;

    public void pause() { emptySince = -1; lastSample = -1; }

    public boolean observe(long now, int nearby, boolean complete, boolean focused) {
        if (!focused || !complete || nearby < 0 || now < 0) { pause(); return false; }
        if (lastSample < 0 || now < lastSample || now - lastSample > SAMPLE_GAP_MS) emptySince = -1;
        lastSample = now;
        if (nearby > 0) { emptySince = -1; return false; }
        if (emptySince < 0) emptySince = now;
        return now - emptySince >= EMPTY_MS;
    }
}
