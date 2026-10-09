package dev.rootcatch.automation;

public enum ThrowQuality {
    NORMAL("Normal"), NICE("Nice"), GREAT("Great"), EXCELLENT("Excellent");
    public final String label;
    ThrowQuality(String label) { this.label = label; }
    public static ThrowQuality fromSavedValue(String value) {
        for (ThrowQuality quality : values()) if (quality.name().equals(value)) return quality;
        return GREAT;
    }
}
