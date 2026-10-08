package dev.rootcatch.ugphone;

import java.io.File;
import java.nio.file.Files;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import static org.junit.Assert.*;

/** Upgrade compatibility without reading any game data. */
public final class UpgradeReportsTest {
    @Rule public TemporaryFolder temporary = new TemporaryFolder();
    @Test public void oldAndNewSessionsShareTheThreeRecentSlotsWithoutDeletingEither() throws Exception {
        File files = temporary.getRoot();
        File old = new File(files, "test-reports"), current = new File(files, "farm-reports");
        assertTrue(old.mkdir()); assertTrue(current.mkdir());
        assertTrue(new File(old, "1791292927440_68326d35").mkdir());
        assertTrue(new File(old, "1791292927442_68326d35").mkdir());
        assertTrue(new File(current, "1791292927441_68326d35").mkdir());
        assertTrue(new File(current, "1791292927443_68326d35").mkdir());
        File[] selected = FarmReports.recent(files);
        assertEquals(3, selected.length);
        assertEquals("1791292927443_68326d35", selected[0].getName());
        assertEquals("test-reports", selected[1].getParentFile().getName());
        assertEquals(2, old.list().length); assertEquals(2, current.list().length);
    }
    @Test public void legacySessionLogsAndHealthRotationsRemainExportable() {
        for (String valid : new String[]{"test-session.jsonl", "test-session-1.jsonl", "test-session-2.jsonl",
                "health.jsonl", "health-2.jsonl", "farm-session-2.jsonl"}) assertTrue(valid, FarmReports.allowedFile(valid));
        for (String invalid : new String[]{"test-session-3.jsonl", "../test-session.jsonl", "shared_prefs.xml",
                "script.bin", "injector.bin"}) assertFalse(invalid, FarmReports.allowedFile(invalid));
    }
    @Test public void invalidSessionDirectoriesNeverEnterTheExport() throws Exception {
        File root = new File(temporary.getRoot(), "test-reports"); assertTrue(root.mkdir());
        assertTrue(new File(root, "not-a-session").mkdir());
        assertTrue(new File(root, "1791292927440_68326d35.zip").createNewFile());
        assertEquals(0, FarmReports.recent(temporary.getRoot()).length);
    }
    @Test public void duplicatedLegacyIdentityCannotCreateDuplicateZipEntries() throws Exception {
        File old = new File(temporary.getRoot(), "test-reports"), current = new File(temporary.getRoot(), "farm-reports");
        assertTrue(old.mkdir()); assertTrue(current.mkdir());
        String name = "1791292927440_68326d35";
        assertTrue(new File(old, name).mkdir()); assertTrue(new File(current, name).mkdir());
        File[] selected = FarmReports.recent(temporary.getRoot());
        assertEquals(1, selected.length); assertEquals("farm-reports", selected[0].getParentFile().getName());
        assertTrue(new File(old, name).exists());
    }
    @Test public void continuousClockAndCountersRemainMonotonicAcrossLongSessions() {
        assertEquals("48:00:01", SessionCounters.elapsed(172_801_000L));
        SessionCounters counts = new SessionCounters();
        for (int i = 1; i <= 10_000; i++) counts = counts.update(i, i / 7, i / 3, i * 2).transfers(i - 1);
        counts = counts.update(1, 1, 1, 1).transfers(0);
        assertEquals(10_000, counts.caught); assertEquals(9_999, counts.transferred);
        assertEquals(20_000, counts.discarded);
    }
    @Test public void legacyRootSymlinkCannotReadAnotherAppDirectory() throws Exception {
        File outside = temporary.newFolder("outside");
        assertTrue(new File(outside, "1791292927440_68326d35").mkdir());
        try { Files.createSymbolicLink(new File(temporary.getRoot(), "test-reports").toPath(), outside.toPath()); }
        catch (java.io.IOException | UnsupportedOperationException error) {
            org.junit.Assume.assumeNoException("OS did not permit symlinks", error);
        }
        assertEquals(0, FarmReports.recent(temporary.getRoot()).length);
    }
}
