package dev.rootcatch.ugphone;

import java.io.File;
import java.nio.file.Files;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import static org.junit.Assert.*;

public final class FarmReportsTest {
    @Rule public TemporaryFolder temporary = new TemporaryFolder();
    @Test public void includesHealthAndAllBoundedDiagnosticRotations() {
        for (String valid : new String[]{"health.jsonl", "health-1.jsonl", "health-2.jsonl", "farm-session-2.jsonl",
                "latest.jsonl", "sources-1.jsonl", "before-game-process.log", "after-dropbox-native.log",
                "logcat-live-2.log", "report-status.json", "current-tombstone_01.log"}) assertTrue(valid, FarmReports.allowedFile(valid));
        for (String invalid : new String[]{"../health.jsonl", "latest-3.jsonl", "secret.dat", "agent.js", "/health.jsonl", "health.jsonl.exe"})
            assertFalse(invalid, FarmReports.allowedFile(invalid));
    }
    @Test public void recentExportIsBoundedButDoesNotDeleteOlderSessions() throws Exception {
        File root = new File(temporary.getRoot(), "farm-reports"); assertTrue(root.mkdir());
        for (int i = 0; i < 5; i++) assertTrue(new File(root, "179129292744" + i + "_68326d35").mkdir());
        assertEquals(3, FarmReports.recent(temporary.getRoot()).length);
        assertEquals(5, root.listFiles().length);
    }
    @Test public void exportsOnlyDirectAllowlistedFilesInSession() throws Exception {
        File report = FarmReports.create(temporary.getRoot());
        assertTrue(new File(report, "health.jsonl").createNewFile());
        assertTrue(new File(report, "secret.dat").createNewFile());
        assertTrue(new File(report, "latest.jsonl").mkdir());
        assertEquals(1, FarmReports.files(report).length);
    }
    @Test public void symlinkEscapeCannotExportOtherDirectories() throws Exception {
        File report = FarmReports.create(temporary.getRoot()), outside = temporary.newFile("outside.log");
        try { Files.createSymbolicLink(new File(report, "health.jsonl").toPath(), outside.toPath()); }
        catch (java.io.IOException | UnsupportedOperationException error) {
            org.junit.Assume.assumeNoException("OS did not permit symlinks", error);
        }
        assertEquals(0, FarmReports.files(report).length);
    }
}
