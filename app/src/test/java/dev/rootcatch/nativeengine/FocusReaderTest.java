package dev.rootcatch.nativeengine;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import org.junit.Test;
import static org.junit.Assert.*;

public class FocusReaderTest {
    private static final String GAME = "Display: mDisplayId=0\nmCurrentFocus=Window{abc u0 com.nianticlabs.pokemongo/Main}";
    private static FocusReader.Reply reply(String output, int exit) {
        return new FocusReader.Reply(0, output + "\n" + FocusReader.EXIT_MARKER + exit);
    }
    private static final class Fake implements FocusReader.Shell {
        final List<String> commands = new ArrayList<>();
        final List<FocusReader.Reply> replies = new ArrayList<>();
        Fake(FocusReader.Reply... values) { java.util.Collections.addAll(replies, values); }
        @Override public FocusReader.Reply read(String command, long timeout) {
            assertEquals(3500, timeout); commands.add(command);
            assertFalse("No unexpected extra query", replies.isEmpty()); return replies.remove(0);
        }
    }
    @Test public void primaryUsesDisplaysAndLogsActualResponse() {
        Fake shell = new Fake(reply(GAME, 0)); List<FocusReader.Sample> logs = new ArrayList<>();
        FocusReader.Reading result = new FocusReader(shell, () -> false, logs::add).read("warm_guard");
        assertTrue(result.focus.isGame()); assertEquals("displays", result.source);
        assertTrue(shell.commands.get(0).contains("dumpsys -t 2 window displays"));
        assertFalse(shell.commands.get(0).contains("window windows"));
        assertEquals(1, logs.size()); assertEquals("warm_guard", logs.get(0).stage);
        assertEquals(GAME + "\n" + FocusReader.EXIT_MARKER + "0", logs.get(0).response);
        assertEquals(0, logs.get(0).sourceExitCode);
    }
    @Test public void absentFieldFallsBackToFullAndRemembersSuccessfulSource() {
        Fake shell = new Fake(reply("mTopFocusedDisplayId=0", 0), reply(GAME, 0), reply(GAME, 0));
        List<FocusReader.Sample> logs = new ArrayList<>();
        FocusReader reader = new FocusReader(shell, () -> false, logs::add);
        assertTrue(reader.read("warm_guard").focus.isGame());
        assertEquals(ForegroundFocus.State.UNAVAILABLE, logs.get(0).focus.state);
        assertTrue(shell.commands.get(1).contains("dumpsys -t 2 window 2>&1"));
        assertEquals("full", reader.read("before_attach").source); assertEquals(3, shell.commands.size());
    }
    @Test public void otherWindowDoesNotFallBackToPotentiallyStaleGame() {
        Fake shell = new Fake(reply("mCurrentFocus=Window{abc u0 com.other/Main}", 0));
        assertEquals(ForegroundFocus.State.OTHER, new FocusReader(shell, () -> false, ignored -> { }).read("warm_guard").focus.state);
        assertEquals(1, shell.commands.size());
    }
    @Test public void explicitNullDoesNotFallBack() {
        Fake shell = new Fake(reply("mCurrentFocus=null", 0));
        assertEquals(ForegroundFocus.State.NO_WINDOW, new FocusReader(shell, () -> false, ignored -> { }).read("warm_guard").focus.state);
        assertEquals(1, shell.commands.size());
    }
    @Test public void nativeExitIsPreservedDespiteSuccessfulShellPipeline() {
        Fake shell = new Fake(reply(GAME, 124), reply(GAME, 124)); List<FocusReader.Sample> logs = new ArrayList<>();
        assertFalse(new FocusReader(shell, () -> false, logs::add).read("warm_guard").focus.isGame());
        assertEquals("command_failed", logs.get(0).focus.reason); assertEquals(124, logs.get(0).sourceExitCode);
    }
    @Test public void permissionErrorsCannotAuthorizeAttachEvenWithValidWindow() {
        for (String error : new String[]{"Permission Denial: requires android.permission.DUMP", "Error dumping service", "DUMP TIMEOUT"}) {
            Fake shell = new Fake(reply(GAME + "\n" + error, 0), reply(error, 0));
            assertFalse(new FocusReader(shell, () -> false, ignored -> { }).read("warm_guard").focus.isGame());
        }
    }
    @Test public void missingMalformedOrRepeatedExitMarkerFailsClosed() {
        for (String value : new String[]{GAME, GAME + "\nRC_FOCUS_SOURCE_EXIT:0\nRC_FOCUS_SOURCE_EXIT:0",
                GAME + "\nRC_FOCUS_SOURCE_EXIT:0\npartial_output", GAME + "\nRC_FOCUS_SOURCE_EXIT:not_a_code"}) {
            Fake shell = new Fake(new FocusReader.Reply(0, value), new FocusReader.Reply(0, value));
            assertFalse(new FocusReader(shell, () -> false, ignored -> { }).read("warm_guard").focus.isGame());
        }
    }
    @Test public void truncatedReplyIsRecordedAndRejected() {
        String large = GAME + "\n" + "x".repeat(FocusReader.MAX_BYTES);
        Fake shell = new Fake(reply(large, 0), reply(large, 0)); List<FocusReader.Sample> logs = new ArrayList<>();
        assertFalse(new FocusReader(shell, () -> false, logs::add).read("warm_guard").focus.isGame());
        assertTrue(logs.get(0).truncated); assertEquals("response_truncated", logs.get(0).focus.reason);
        assertTrue(logs.get(0).response.getBytes(StandardCharsets.UTF_8).length <= FocusReader.MAX_BYTES);
    }
    @Test public void shellFailureIsRecordedWithoutClaimingAnotherApp() {
        List<FocusReader.Sample> logs = new ArrayList<>();
        FocusReader reader = new FocusReader((command, timeout) -> { throw new java.io.IOException("simulated timeout"); }, () -> false, logs::add);
        assertEquals(ForegroundFocus.State.UNAVAILABLE, reader.read("warm_guard").focus.state);
        assertTrue(logs.get(0).error.contains("simulated timeout")); assertNull(logs.get(0).focus.focusedValue());
    }
    @Test public void stopPreventsQueriesAndPreventsFallback() {
        Fake shell = new Fake(); AtomicBoolean stop = new AtomicBoolean(true);
        assertEquals("stopped", new FocusReader(shell, stop::get, ignored -> { }).read("before_attach").focus.reason);
        assertTrue(shell.commands.isEmpty());
        Fake first = new Fake(reply("", 0));
        // First query runs; a stop arriving in the observer must prevent a second query.
        stop.set(false);
        FocusReader reader = new FocusReader(first, stop::get, sample -> stop.set(true));
        assertFalse(reader.read("before_attach").focus.isGame()); assertEquals(1, first.commands.size());
    }
    @Test public void shellExitAndMultiDisplayAmbiguityCannotAuthorizeAttach() {
        Fake shell = new Fake(new FocusReader.Reply(1, GAME + "\nRC_FOCUS_SOURCE_EXIT:0"),
                reply("Display: mDisplayId=0\n" + GAME.substring(GAME.indexOf("mCurrentFocus="))
                        + "\nDisplay: mDisplayId=1\nmCurrentFocus=null", 0));
        assertEquals(ForegroundFocus.State.AMBIGUOUS, new FocusReader(shell, () -> false, ignored -> { }).read("warm_guard").focus.state);
    }
    @Test public void queryShellSyntaxAndNativeExitEnvelopeWorkWithFakeDumpsysOnly() throws Exception {
        String bash = dev.rootcatch.testutil.TestShell.bash();
        for (String source : new String[]{"displays", "full"}) {
            String fake = "dumpsys() { printf '%s\\n' 'Display: mDisplayId=0' "
                    + "'mCurrentFocus=Window{abc u0 com.nianticlabs.pokemongo/Main}'; return 7; }; ";
            Process process = new ProcessBuilder(bash, "--noprofile", "--norc", "-c", fake + FocusReader.command(source))
                    .redirectErrorStream(true).start();
            assertTrue(process.waitFor(5, TimeUnit.SECONDS));
            String output = new String(process.getInputStream().readAllBytes(), StandardCharsets.UTF_8);
            assertEquals(output, 0, process.exitValue()); assertTrue(output.contains("RC_FOCUS_SOURCE_EXIT:7"));
            assertTrue(output.contains("mCurrentFocus=Window"));
        }
        assertThrows(IllegalArgumentException.class, () -> FocusReader.command("windows; am start"));
    }
    @Test public void shadeContextIsReadFreshAndMissingContextFallsBackWithoutReusingGame() {
        String shade = "Display: mDisplayId=0\nmCurrentFocus=Window{abc u0 StatusBar}";
        Fake shell = new Fake(reply(shade, 0), reply(shade + "\nmFocusedApp=ActivityRecord{cc u0 "
                + EnginePolicy.GAME + "/Main t1}", 0), reply(shade + "\nmFocusedApp=ActivityRecord{dd u0 com.other/Main t2}", 0));
        FocusReader reader = new FocusReader(shell, () -> false, ignored -> { });
        assertTrue(reader.read("focus_sample").focus.notificationShadeAllowed());
        assertFalse(reader.read("focus_sample").focus.isGame());
        assertEquals(3, shell.commands.size()); assertTrue(shell.commands.get(0).contains("mFocusedApp="));
    }
    @Test public void unavailableOrFailedUnderlyingAppProofNeverAuthorizesShade() {
        String shade = "mCurrentFocus=Window{abc u0 StatusBar}";
        for (int exit : new int[]{0, 124}) {
            Fake shell = new Fake(reply(shade, exit), reply(shade, exit));
            assertFalse(new FocusReader(shell, () -> false, ignored -> { }).read("focus_sample").focus.isGame());
        }
    }
    @Test public void ugphoneNormalFocusIsAcceptedImmediatelyWithoutAnUnnecessaryFallback() {
        String output = dev.rootcatch.testutil.UgphoneFocusFixture.displays(false);
        Fake shell = new Fake(reply(output, 0)); List<FocusReader.Sample> logs = new ArrayList<>();
        FocusReader.Reading result = new FocusReader(shell, () -> false, logs::add).read("early_startup");
        assertTrue(result.focus.initialAttachAllowed()); assertEquals(1, shell.commands.size());
        assertEquals(output + "\nRC_FOCUS_SOURCE_EXIT:0", logs.get(0).response);
        assertEquals("early_startup", logs.get(0).stage);
    }
    @Test public void ugphoneFullFallbackAcceptsLockscreenTimeoutPropertyAndDisplayAlias() {
        Fake shell = new Fake(reply("mTopFocusedDisplayId=0", 0),
                reply(dev.rootcatch.testutil.UgphoneFocusFixture.full(false), 0));
        List<FocusReader.Sample> logs = new ArrayList<>();
        FocusReader.Reading result = new FocusReader(shell, () -> false, logs::add).read("early_startup");
        assertTrue(result.focus.initialAttachAllowed()); assertEquals("full", result.source);
        assertEquals(2, shell.commands.size()); assertEquals("current_window", logs.get(1).focus.reason);
    }
    @Test public void genuineTimeoutExceptionsAndPermissionErrorsRemainFailures() {
        for (String error : new String[]{"DUMP TIMEOUT", "Dump timed out", "Error dumping service", "ERROR: permission denied",
                "java.lang.SecurityException: requires permission", "Permission Denial: requires DUMP", "unknown command", "dumpsys: not found"}) {
            String value = dev.rootcatch.testutil.UgphoneFocusFixture.full(false) + "\n" + error;
            Fake shell = new Fake(reply(value, 0), reply(value, 0)); List<FocusReader.Sample> logs = new ArrayList<>();
            assertFalse(error, new FocusReader(shell, () -> false, logs::add).read("early_startup").focus.isGame());
            assertEquals(error, "source_reported_error", logs.get(0).focus.reason);
        }
    }
    @Test public void fieldValuesContainingDiagnosticWordsAreNotErrorMessages() {
        String output = "Display: mDisplayId=0\nmCurrentFocus=Window{abc u0 com.other/TimeoutExceptionActivity}\n"
                + "mFocusedApp=ActivityRecord{cc u0 com.other/TimeoutExceptionActivity t1}\n"
                + "mLockScreenTimeout=2147483647 mLockScreenTimerActive=false";
        Fake shell = new Fake(reply(output, 0));
        assertEquals(ForegroundFocus.State.OTHER, new FocusReader(shell, () -> false, ignored -> { }).read("early_startup").focus.state);
    }
    @Test public void shadeContinuityAndSwitchingToAnotherAppUseFreshSamplesOnly() {
        Fake shell = new Fake(reply(dev.rootcatch.testutil.UgphoneFocusFixture.displays(false), 0),
                reply(dev.rootcatch.testutil.UgphoneFocusFixture.displays(true), 0),
                reply("mCurrentFocus=Window{abc u0 com.other/Main}", 0));
        FocusReader reader = new FocusReader(shell, () -> false, ignored -> { });
        assertTrue(reader.read("early_startup").focus.initialAttachAllowed());
        ForegroundFocus shade = reader.read("focus_sample").focus;
        assertTrue(shade.notificationShadeAllowed()); assertFalse(shade.initialAttachAllowed());
        assertFalse(reader.read("focus_sample").focus.isGame()); assertEquals(3, shell.commands.size());
    }
    @Test public void recordedUgphoneFocusSamplesAreAcceptedWithoutFallback() {
        String encoded = System.getenv("SILPH_FOCUS_REPLAY");
        List<String> values = new ArrayList<>();
        if (encoded != null && !encoded.isBlank()) {
            for (String value : encoded.split(":"))
                values.add(new String(java.util.Base64.getDecoder().decode(value), StandardCharsets.UTF_8));
        } else {
            for (boolean shade : new boolean[]{false, true})
                values.add(dev.rootcatch.testutil.UgphoneFocusFixture.full(shade) + "\nRC_FOCUS_SOURCE_EXIT:0");
        }
        assertFalse(values.isEmpty());
        for (String value : values) {
            Fake shell = new Fake(new FocusReader.Reply(0, value));
            List<FocusReader.Sample> logs = new ArrayList<>();
            ForegroundFocus focus = new FocusReader(shell, () -> false, logs::add).read("early_startup").focus;
            assertTrue(logs.get(0).focus.reason, focus.isGame());
            assertEquals(1, shell.commands.size());
            assertEquals(!focus.notificationShadeAllowed(), focus.initialAttachAllowed());
        }
        System.out.println("Ugphone focus replay: " + values.size() + " samples accepted without fallback");
    }
}
