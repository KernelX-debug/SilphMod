package dev.rootcatch.automation;

import android.content.Context;
import android.content.SharedPreferences;

public final class AutomationConfig {
    public static final String PREFS = "rootcatch";

    public final String gamePackage;
    public final boolean autoCatch;
    public final java.util.List<Integer> onlyCatch;
    public final boolean autoSpin;
    public final boolean removeBuddy;
    public final boolean cleanBag;
    public final int actionDelayMs;
    public final int maxCatches;
    public final int maxSpins;
    public final int maxThrows;
    public final String throwQuality;
    public final boolean useBerries;
    public final java.util.List<String> berryOrder;
    public final boolean skipInterruptions;
    public final boolean fieldResearch;
    public final boolean discardUnsupportedResearch;
    public final boolean showIvNames;
    public final boolean extendedManualTransfer;
    public final boolean fastGiftOpening;
    public final boolean hideTrainerName;
    public final boolean internalEngine;
    public final String discardLimits;
    public final int maxDiscardedItems;

    private AutomationConfig(String gamePackage,
                             boolean autoCatch, java.util.List<Integer> onlyCatch, boolean autoSpin, boolean removeBuddy,
                             boolean cleanBag, int actionDelayMs, int maxCatches, int maxSpins,
                             int maxThrows, String throwQuality, boolean useBerries, java.util.List<String> berryOrder,
                             boolean showIvNames, boolean extendedManualTransfer, boolean fastGiftOpening, boolean hideTrainerName, boolean internalEngine,
                             String discardLimits, int maxDiscardedItems) {
        this.gamePackage = gamePackage;
        this.autoCatch = autoCatch;
        this.onlyCatch = onlyCatch;
        this.autoSpin = autoSpin;
        this.removeBuddy = removeBuddy;
        this.cleanBag = cleanBag;
        this.actionDelayMs = actionDelayMs;
        this.maxCatches = maxCatches;
        this.maxSpins = maxSpins;
        this.maxThrows = maxThrows;
        this.throwQuality = throwQuality;
        this.useBerries = useBerries;
        this.berryOrder = berryOrder;
        this.skipInterruptions = true;
        this.fieldResearch = true;
        this.discardUnsupportedResearch = true;
        this.showIvNames = showIvNames;
        this.extendedManualTransfer = extendedManualTransfer;
        this.fastGiftOpening = fastGiftOpening;
        this.hideTrainerName = hideTrainerName;
        this.internalEngine = internalEngine;
        this.discardLimits = discardLimits;
        this.maxDiscardedItems = maxDiscardedItems;
    }

    public static AutomationConfig load(Context context) {
        SharedPreferences p = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        return new AutomationConfig(
                p.getString("package", "com.nianticlabs.pokemongo"),
                p.getBoolean("catch", true),
                PokemonSelection.parse(p.getString(PokemonSelection.KEY, "")),
                p.getBoolean("spin", true),
                p.getBoolean("remove_buddy", false),
                p.getBoolean("clean", false),
                clamp(p.getInt("delay", 150), 100, 3000),
                SessionLimits.savedLimit(p.getInt("max_catches", SessionLimits.DEFAULT_CATCHES), SessionLimits.DEFAULT_CATCHES),
                SessionLimits.savedLimit(p.getInt("max_spins", SessionLimits.DEFAULT_SPINS), SessionLimits.DEFAULT_SPINS),
                0,
                ThrowQuality.fromSavedValue(p.getString("throw_quality", "GREAT")).name(),
                p.getBoolean("use_berries", true),
                BerryOptions.parse(BerryOptions.fromSavedValue(p.getString("berry_order", BerryOptions.DEFAULT_ORDER))),
                p.getBoolean("show_iv_names", true),
                p.getBoolean("extended_manual_transfer", false),
                p.getBoolean("fast_gift_opening", true),
                p.getBoolean("hide_trainer_name", true),
                p.getBoolean("internal_engine", true),
                p.getString("discard_limits", dev.rootcatch.nativeengine.EnginePolicy.DEFAULT_LIMITS),
                0
        );
    }

    private static int clamp(int value, int min, int max) {
        return Math.max(min, Math.min(max, value));
    }
}
