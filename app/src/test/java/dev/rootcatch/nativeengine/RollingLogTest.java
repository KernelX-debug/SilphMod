package dev.rootcatch.nativeengine;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import static org.junit.Assert.*;

public class RollingLogTest {
    @Rule public TemporaryFolder folder = new TemporaryFolder();
    private String text(String name) throws Exception { return new String(Files.readAllBytes(new File(folder.getRoot(), name).toPath()), StandardCharsets.UTF_8); }
    @Test public void rotationRetainsCompleteRecordsAndOnlyTwoArchives() throws Exception {
        try (RollingLog log = new RollingLog(folder.getRoot(), "latest.jsonl", 6, 2)) {
            for (String line : new String[]{"11111", "22222", "33333", "44444"}) log.write(line);
            assertEquals(3, log.rotations()); assertEquals(24, log.totalBytes());
        }
        assertEquals("44444\n", text("latest.jsonl"));
        assertEquals("33333\n", text("latest-1.jsonl")); assertEquals("22222\n", text("latest-2.jsonl"));
        assertEquals(3, folder.getRoot().list().length);
    }
    @Test public void exactBudgetDoesNotRotateEarly() throws Exception {
        try (RollingLog log = new RollingLog(folder.getRoot(), "sources.jsonl", 4, 2)) {
            log.write("a"); log.write("b"); assertEquals(0, log.rotations());
            log.write("c"); assertEquals(1, log.rotations());
        }
        assertEquals("a\nb\n", text("sources-1.jsonl")); assertEquals("c\n", text("sources.jsonl"));
    }
    @Test public void utf8BudgetCountsBytesAndNeverSplitsRecord() throws Exception {
        try (RollingLog log = new RollingLog(folder.getRoot(), "sources.jsonl", 5, 2)) {
            log.write("🚀"); log.write("á"); assertEquals(8, log.totalBytes());
        }
        assertEquals(5, text("sources-1.jsonl").getBytes(StandardCharsets.UTF_8).length);
        assertEquals("á\n", text("sources.jsonl"));
    }
    @Test public void oversizedRecordDoesNotWritePartialData() throws Exception {
        try (RollingLog log = new RollingLog(folder.getRoot(), "latest.jsonl", 4, 2)) {
            log.write("ok"); assertThrows(java.io.IOException.class, () -> log.write("large"));
            assertEquals(0, log.rotations());
        }
        assertEquals("ok\n", text("latest.jsonl"));
    }
    @Test public void invalidPathsAndBudgetsFailBeforeCreatingFiles() {
        for (String name : new String[]{"../latest.jsonl", "nested/latest.jsonl", "secrets.db", "/latest.jsonl"})
            assertThrows(IllegalArgumentException.class, () -> new RollingLog(folder.getRoot(), name, 4, 2));
        assertThrows(IllegalArgumentException.class, () -> new RollingLog(folder.getRoot(), "latest.jsonl", 0, 2));
        assertThrows(IllegalArgumentException.class, () -> new RollingLog(folder.getRoot(), "latest.jsonl", 4, 3));
        assertEquals(0, folder.getRoot().list().length);
    }
    @Test public void closeIsIdempotentAndNoWriteAfterClose() throws Exception {
        RollingLog log = new RollingLog(folder.getRoot(), "latest.jsonl", 8, 2);
        log.write("ok"); log.close(); log.close();
        assertThrows(java.io.IOException.class, () -> log.write("no")); assertEquals("ok\n", text("latest.jsonl"));
    }
}
