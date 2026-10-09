package dev.rootcatch.ugphone;

import dev.rootcatch.nativeengine.ItemCatalog;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashSet;
import java.util.Set;
import org.junit.Test;
import static org.junit.Assert.*;

public final class SettingsDialogsTest {
    private String read(String path) throws Exception {
        return new String(Files.readAllBytes(Path.of(System.getProperty("user.dir"), path)), java.nio.charset.StandardCharsets.UTF_8);
    }
    @Test public void everySelectableItemHasOneDistinctPackagedResource() throws Exception {
        Set<Integer> resources = new HashSet<>();
        Path folder = Path.of(System.getProperty("user.dir"), "src/main/res/drawable-nodpi");
        for (ItemCatalog.Entry entry : ItemCatalog.ENTRIES) if (entry.discardable()) {
            assertTrue(resources.add(ItemArtwork.resource(entry.id())));
            String stem = String.format(java.util.Locale.ROOT, "item_%04d", entry.id());
            try (var files = Files.list(folder)) {
                Path file = files.filter(path -> path.getFileName().toString().matches(stem + "\\.(png|webp)")).findFirst().orElseThrow();
                assertTrue(Files.size(file) > 500);
            }
        }
        assertEquals(33, resources.size());
        try (var files = Files.list(folder)) { assertEquals(33, files.count()); }
    }
    @Test public void unknownAndProtectedArtworkCannotSilentlyBecomeAnotherItem() {
        for (int id : new int[]{4, 5, 7, 0, 9999})
            assertThrows(IllegalArgumentException.class, () -> ItemArtwork.resource(id));
    }
    @Test public void selectorsUseCustomDialogsAndKeepSaveControlsOutsideTheirScrollArea() throws Exception {
        String ui = read("src/main/java/dev/rootcatch/ugphone/MainActivity.java");
        String dialogs = read("src/main/java/dev/rootcatch/ugphone/SettingsDialogs.java");
        assertFalse(ui.contains("AlertDialog")); assertFalse(dialogs.contains("AlertDialog"));
        assertTrue(ui.contains("new SettingsDialogs(this).throwsDialog(throwQuality"));
        assertTrue(ui.contains("new SettingsDialogs(this).itemsDialog(limits"));
        assertTrue(dialogs.contains("new Dialog(activity, R.style.SettingsDialogTheme)"));
        assertTrue(dialogs.contains("scroll(page, body);\n        footer(page, dialog"));
        assertTrue(dialogs.contains("new LinearLayout.LayoutParams(-1, 0, 1)"));
        assertTrue(dialogs.contains("metrics.heightPixels * 0.88f"));
        assertTrue(dialogs.contains("SOFT_INPUT_ADJUST_RESIZE"));
    }
    @Test public void filteringUsesTheSameDraftAndNeverRecreatesTheKeepFields() throws Exception {
        String dialogs = read("src/main/java/dev/rootcatch/ugphone/SettingsDialogs.java");
        assertTrue(dialogs.contains("ItemLimitDraft.matches(item.entry, query)"));
        assertTrue(dialogs.contains("item.view.setVisibility(matches ? View.VISIBLE : View.GONE)"));
        assertTrue(dialogs.contains("save.accept(draft.serialize())"));
        assertTrue(dialogs.contains("draft.floor(entry.name(), value)"));
        assertTrue(dialogs.contains("ItemArtwork.resource(entry.id())"));
        assertFalse(dialogs.contains("WebView")); assertFalse(dialogs.contains("http"));
    }
}
