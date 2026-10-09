package dev.rootcatch.nativeengine;

import dev.rootcatch.automation.BerryOptions;
import java.util.Arrays;
import org.junit.Test;
import static org.junit.Assert.*;

public final class BerryOptionsTest {
    @Test public void defaultsUseCommonBerriesAndKeepPremiumTypesOptIn() {
        assertEquals(Arrays.asList("RazzBerry", "PinapBerry", "NanabBerry"), BerryOptions.parse(BerryOptions.DEFAULT_ORDER));
        assertEquals(5, BerryOptions.ALLOWED.size());
        assertFalse(BerryOptions.parse(BerryOptions.DEFAULT_ORDER).contains("GoldenRazzBerry"));
        assertFalse(BerryOptions.parse(BerryOptions.DEFAULT_ORDER).contains("GoldenPinapBerry"));
    }
    @Test public void explicitPriorityAndEmptySelectionRoundTrip() {
        assertEquals(Arrays.asList("GoldenPinapBerry", "PinapBerry"), BerryOptions.parse("GoldenPinapBerry,PinapBerry"));
        assertEquals("GoldenPinapBerry,PinapBerry", BerryOptions.fromSavedValue("GoldenPinapBerry,PinapBerry"));
        assertEquals("", BerryOptions.fromSavedValue(""));
        assertTrue(BerryOptions.parse("").isEmpty());
    }
    @Test public void invalidSavedPreferencesReturnOnlyCommonDefaults() {
        for (String value : new String[]{null, "unknown", "Poffin", "RazzBerry,RazzBerry", "RazzBerry,", "RazzBerry "}) {
            assertThrows(IllegalArgumentException.class, () -> BerryOptions.parse(value));
            assertEquals(BerryOptions.DEFAULT_ORDER, BerryOptions.fromSavedValue(value));
        }
    }
    @Test public void selectedTypesAreAllKnownBerriesWithCatalogIcons() {
        int[] expected = {701,705,703,706,708};
        for (int i = 0; i < BerryOptions.ALLOWED.size(); i++) {
            String name = BerryOptions.ALLOWED.get(i);
            assertEquals(Integer.valueOf(expected[i]), EnginePolicy.ITEMS.get(name));
            assertTrue(ItemCatalog.ENTRIES.stream().anyMatch(entry -> entry.name().equals(name) && entry.group().equals("Berries")));
        }
    }
    @Test public void parsedSelectionAndAllowlistAreImmutable() {
        assertThrows(UnsupportedOperationException.class, () -> BerryOptions.parse("RazzBerry").add("PinapBerry"));
        assertThrows(UnsupportedOperationException.class, () -> BerryOptions.ALLOWED.clear());
    }
}
