package dev.rootcatch.testutil;

import java.io.File;
import java.util.concurrent.TimeUnit;
import org.junit.Assume;

/** Optional Bash used only for harmless shell-syntax fixtures. */
public final class TestShell {
    private TestShell() { }
    public static String bash() throws Exception {
        String executable = System.getenv("SILPH_TEST_BASH");
        if (executable == null || executable.isBlank()) {
            executable = "bash";
            String programs = System.getenv("ProgramFiles");
            if (programs != null) {
                File gitBash = new File(programs, "Git/bin/bash.exe");
                if (gitBash.isFile()) executable = gitBash.getAbsolutePath();
            }
        }
        Process probe;
        try { probe = new ProcessBuilder(executable, "--version").redirectErrorStream(true).start(); }
        catch (java.io.IOException missing) { Assume.assumeNoException(missing); return "bash"; }
        try {
            Assume.assumeTrue("Bash unavailable for optional shell fixtures", probe.waitFor(5, TimeUnit.SECONDS) && probe.exitValue() == 0);
        } finally { probe.destroyForcibly(); }
        return executable;
    }
}
