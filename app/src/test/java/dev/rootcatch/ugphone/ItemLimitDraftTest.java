package dev.rootcatch.ugphone;

import dev.rootcatch.nativeengine.EnginePolicy;
import dev.rootcatch.nativeengine.ItemCatalog;
import java.util.Locale;
import org.junit.Test;
import static org.junit.Assert.*;

public final class ItemLimitDraftTest {
    private static ItemCatalog.Entry item(String name) {
        return ItemCatalog.ENTRIES.stream().filter(entry -> entry.name().equals(name)).findFirst().orElseThrow();
    }
    @Test public void existingLimitsRoundTripWithoutChangingFloors() {
        ItemLimitDraft draft = new ItemLimitDraft(EnginePolicy.DEFAULT_LIMITS);
        assertEquals(4, draft.count());
        assertTrue(draft.selected("Potion"));
        assertEquals("50", draft.floor("Potion"));
        assertEquals(EnginePolicy.limitsKey(EnginePolicy.DEFAULT_LIMITS), draft.serialize());
    }
    @Test public void missingSelectionsHaveZeroFloorButDoNotAuthorizeDeletion() {
        ItemLimitDraft draft = new ItemLimitDraft("");
        assertEquals(0, draft.count()); assertEquals("", draft.serialize());
        assertFalse(draft.selected("Poffin")); assertEquals("0", draft.floor("Poffin"));
    }
    @Test public void selectingThenEditingPersistsOnlyThatItem() {
        ItemLimitDraft draft = new ItemLimitDraft("");
        draft.select("GoldenPinapBerry", true); draft.floor("GoldenPinapBerry", " 27 ");
        assertEquals(1, draft.count()); assertEquals("GoldenPinapBerry=27", draft.serialize());
    }
    @Test public void deselectionRetainsLocalDraftWithoutAuthorizingItsDiscard() {
        ItemLimitDraft draft = new ItemLimitDraft("Potion=50");
        draft.floor("Potion", "83"); draft.select("Potion", false);
        assertEquals("", draft.serialize()); assertEquals("83", draft.floor("Potion"));
        draft.select("Potion", true); assertEquals("Potion=83", draft.serialize());
    }
    @Test public void searchNeverChangesSelectionOrHiddenItemAmounts() {
        ItemLimitDraft draft = new ItemLimitDraft("Potion=50,RazzBerry=15");
        String expected = draft.serialize();
        for (String query : new String[]{"berry", "poké", "nothing-matches", "", null})
            for (ItemCatalog.Entry entry : ItemCatalog.ENTRIES) ItemLimitDraft.matches(entry, query);
        assertEquals(expected, draft.serialize()); assertEquals(2, draft.count());
    }
    @Test public void searchMatchesDisplayNameGroupAndInternalNameWithoutCaseOrAccentSensitivity() {
        assertTrue(ItemLimitDraft.matches(item("PokeBall"), "  POKÉ BALL "));
        assertTrue(ItemLimitDraft.matches(item("GreatBall"), "poke balls"));
        assertTrue(ItemLimitDraft.matches(item("GoldenPinapBerry"), "silver pinap"));
        assertTrue(ItemLimitDraft.matches(item("Gen4EvolutionStone"), "gen4"));
        assertTrue(ItemLimitDraft.matches(item("Poffin"), "consumables"));
        assertFalse(ItemLimitDraft.matches(item("Poffin"), "Potion"));
    }
    @Test public void searchIsIndependentOfDeviceLocale() {
        Locale previous = Locale.getDefault();
        try { Locale.setDefault(Locale.forLanguageTag("tr-TR")); assertTrue(ItemLimitDraft.matches(item("PinapBerry"), "PINAP")); }
        finally { Locale.setDefault(previous); }
    }
    @Test public void blankSearchIncludesAll33SelectableItemsAndNoProtectedItems() {
        int included = 0;
        for (ItemCatalog.Entry entry : ItemCatalog.ENTRIES) {
            assertEquals(entry.discardable(), ItemLimitDraft.matches(entry, ""));
            if (ItemLimitDraft.matches(entry, null)) included++;
        }
        assertEquals(33, included);
    }
    @Test public void invalidSelectedFloorIsRejectedBeforeSaving() {
        ItemLimitDraft draft = new ItemLimitDraft("Potion=50");
        for (String bad : new String[]{"", "-1", "10001", "NaN", "1.5", null}) {
            draft.floor("Potion", bad);
            assertThrows(IllegalArgumentException.class, draft::serialize);
        }
    }
    @Test public void invalidUnselectedFloorDoesNotPreventSavingOtherItems() {
        ItemLimitDraft draft = new ItemLimitDraft("Potion=50");
        draft.floor("Poffin", ""); assertEquals("Potion=50", draft.serialize());
    }
    @Test public void allowedFloorBoundariesAndCanonicalOrderArePreserved() {
        ItemLimitDraft draft = new ItemLimitDraft("Potion=0,Poffin=10000");
        assertEquals("Poffin=10000,Potion=0", draft.serialize());
    }
    @Test public void protectedAndUnknownItemsCannotEnterTheDraft() {
        ItemLimitDraft draft = new ItemLimitDraft("");
        for (String name : new String[]{"MasterBall", "WildBall", "PremierBall", "unknown", null}) {
            assertThrows(IllegalArgumentException.class, () -> draft.select(name, true));
            assertThrows(IllegalArgumentException.class, () -> draft.floor(name, "0"));
        }
        assertEquals("", draft.serialize());
    }
    @Test public void everySelectableItemCanRoundTripWithoutRenamingItsEngineIdentifier() {
        ItemLimitDraft draft = new ItemLimitDraft("");
        for (ItemCatalog.Entry entry : ItemCatalog.ENTRIES) if (entry.discardable()) {
            draft.select(entry.name(), true); draft.floor(entry.name(), String.valueOf(entry.id()));
        }
        assertEquals(33, EnginePolicy.parseLimits(draft.serialize()).size());
        for (ItemCatalog.Entry entry : ItemCatalog.ENTRIES) if (entry.discardable())
            assertEquals(Integer.valueOf(entry.id()), EnginePolicy.parseLimits(draft.serialize()).get(entry.name()));
    }
}
