package dev.rootcatch.nativeengine;

import dev.rootcatch.automation.ThrowQuality;
import org.junit.Test;
import static org.junit.Assert.*;

public final class ThrowQualityTest {
    @Test public void supportedChoicesHaveStableSavedValuesAndEnglishLabels() {
        String[] labels = {"Normal", "Nice", "Great", "Excellent"};
        assertEquals(4, ThrowQuality.values().length);
        for (ThrowQuality quality : ThrowQuality.values()) {
            assertEquals(quality, ThrowQuality.fromSavedValue(quality.name()));
            assertEquals(labels[quality.ordinal()], quality.label);
        }
    }
    @Test public void missingOrInvalidOldPreferencesPreserveThePreviousGreatThrow() {
        for (String value : new String[]{null, "", "invalid", "EXCELLENT ", "excellent", "5"})
            assertEquals(ThrowQuality.GREAT, ThrowQuality.fromSavedValue(value));
    }
}
