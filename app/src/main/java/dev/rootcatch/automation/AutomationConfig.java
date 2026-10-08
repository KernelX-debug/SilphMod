package dev.rootcatch.automation;

import android.content.Context;
import android.content.SharedPreferences;

public final class AutomationConfig {
    public static final String PREFS = "rootcatch";

    public final String gamePackage;
    public final boolean autoCatch;
    public final boolean autoSpin;
    public final boolean cleanBag;
    public final int actionDelayMs;
    public final int maxThrows;
    public final boolean internalEngine;
    public final String discardLimits;
    public final int maxDiscardedItems;

    private AutomationConfig(String gamePackage, boolean autoCatch, boolean autoSpin,
                             boolean cleanBag, int actionDelayMs,
                             int maxThrows, boolean internalEngine,
                             String discardLimits, int maxDiscardedItems) {
        this.gamePackage = gamePackage;
        this.autoCatch = autoCatch;
        this.autoSpin = autoSpin;
        this.cleanBag = cleanBag;
        this.actionDelayMs = actionDelayMs;
        this.maxThrows = maxThrows;
        this.internalEngine = internalEngine;
        this.discardLimits = discardLimits;
        this.maxDiscardedItems = maxDiscardedItems;
    }

    public static AutomationConfig load(Context context) {
        SharedPreferences p = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        return new AutomationConfig(
                p.getString("package", "com.nianticlabs.pokemongo"),
                p.getBoolean("catch", true),
                p.getBoolean("spin", true),
                p.getBoolean("clean", false),
                clamp(p.getInt("delay", 650), 250, 3000),
                clamp(p.getInt("max_throws", 4), 1, 10),
                p.getBoolean("internal_engine", true),
                p.getString("discard_limits", dev.rootcatch.nativeengine.EnginePolicy.DEFAULT_LIMITS),
                0
        );
    }

    private static int clamp(int value, int min, int max) {
        return Math.max(min, Math.min(max, value));
    }
}
