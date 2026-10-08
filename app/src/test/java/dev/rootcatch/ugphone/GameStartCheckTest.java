package dev.rootcatch.ugphone;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.junit.Test;
import static org.junit.Assert.*;

public final class GameStartCheckTest {
    private static GameStartCheck.Reply reply(String name, int code, String payload) {
        return new GameStartCheck.Reply(code, "SILPH_" + name + "_BEGIN\n" + payload + "\nSILPH_" + name + "_END:" + code);
    }
    private static GameStartCheck.Reply absentProc() { return reply("PROC", 0, "SUMMARY:200:0:0\n"); }
    private static GameStartCheck.Reply presentProc() {
        return reply("PROC", 0, "PROCESS:23673:12345:" + GameStartCheck.GAME + "\nSUMMARY:200:0:1\n");
    }
    @Test public void normalPidofAbsenceNeedsIndependentInventory() {
        assertEquals(GameStartCheck.State.ABSENT, GameStartCheck.decide(reply("PIDOF", 1, ""), absentProc()).state);
        assertEquals(GameStartCheck.State.UNVERIFIED, GameStartCheck.decide(reply("PIDOF", 1, ""), reply("PROC", 1, "Permission denied\n")).state);
    }
    @Test public void ugphoneEmptySuccessfulPidofIsNotMistakenForRunning() {
        assertEquals(GameStartCheck.State.ABSENT, GameStartCheck.decide(reply("PIDOF", 0, ""), absentProc()).state);
    }
    @Test public void missingPidofHasAnExplicitCompleteProcFallback() {
        GameStartCheck.Observation result = GameStartCheck.decide(reply("PIDOF", 127, "pidof: not found\n"), absentProc());
        assertEquals(GameStartCheck.State.ABSENT, result.state);
        assertEquals("complete_proc_fallback_pidof_anomaly", result.reason);
    }
    @Test public void noiseAndTrailingNewlinesArePreservedNotSilentlyParsedAsPids() {
        String text = "pidof: warning\n\n";
        assertEquals(text, GameStartCheck.decode("PIDOF", reply("PIDOF", 1, text)).payload);
        assertEquals(GameStartCheck.State.ABSENT, GameStartCheck.decide(reply("PIDOF", 1, text), absentProc()).state);
        assertEquals(GameStartCheck.State.UNVERIFIED, GameStartCheck.decide(new GameStartCheck.Reply(1, "su noise\n" + reply("PIDOF", 1, "").output), absentProc()).state);
    }
    @Test public void contradictoryPidofAndProcCannotAuthorizeLaunch() {
        assertEquals(GameStartCheck.State.UNVERIFIED, GameStartCheck.decide(reply("PIDOF", 0, "23673\n"), absentProc()).state);
    }
    @Test public void procEvidenceDetectsRunningGameDespiteBrokenPidof() {
        assertEquals(GameStartCheck.State.RUNNING, GameStartCheck.decide(reply("PIDOF", 127, "not found\n"), presentProc()).state);
    }
    @Test public void auxProcessAlsoRequiresACleanStart() {
        GameStartCheck.Reply auxiliary = reply("PROC", 0, "PROCESS:555:44:" + GameStartCheck.GAME + ":worker\nSUMMARY:200:0:1\n");
        assertEquals(GameStartCheck.State.RUNNING, GameStartCheck.decide(reply("PIDOF", 1, ""), auxiliary).state);
    }
    @Test public void incompleteOrMalformedProcNeverMeansAbsence() {
        for (String payload : new String[]{"SUMMARY:200:1:0\n", "SUMMARY:0:0:0\n", "SUMMARY:2:0:1\n",
                "PROCESS:1:99:" + GameStartCheck.GAME + "\nSUMMARY:200:0:1\n",
                "PROCESS:55:0:" + GameStartCheck.GAME + "\nSUMMARY:200:0:1\n",
                "PROCESS:55:44:other.app\nSUMMARY:200:0:1\n",
                "SUMMARY:2147483648:0:0\n", "Permission denied\nSUMMARY:200:0:0\n"}) {
            assertEquals(payload, GameStartCheck.State.UNVERIFIED, GameStartCheck.decide(reply("PIDOF", 1, ""), reply("PROC", 0, payload)).state);
        }
    }
    @Test public void duplicateAndOverflowProcessIdentitiesAreRejected() {
        String row = "PROCESS:55:44:" + GameStartCheck.GAME + "\n";
        assertEquals(GameStartCheck.State.UNVERIFIED, GameStartCheck.decide(reply("PIDOF", 0, "55\n"), reply("PROC", 0, row + row + "SUMMARY:200:0:2\n")).state);
        assertEquals(GameStartCheck.State.UNVERIFIED, GameStartCheck.decide(reply("PIDOF", 0, "55\n"), reply("PROC", 0, "PROCESS:55:9223372036854775808:" + GameStartCheck.GAME + "\nSUMMARY:200:0:1\n")).state);
    }
    @Test public void stopTargetsOnlyFixedGamePackageAndLeavesPersistentShellAlive() {
        assertEquals("am force-stop --user current com.nianticlabs.pokemongo", GameStartCheck.STOP_GAME);
        String command = GameStartCheck.framed("STOP_GAME", GameStartCheck.STOP_GAME);
        assertTrue(command.startsWith("( printf ")); assertTrue(command.endsWith(" )"));
        assertFalse(command.contains("kill ")); assertFalse(command.contains("clear "));
        assertFalse(GameStartCheck.PROC.contains("force-stop"));
    }
    private static final class Harness {
        final List<String> commands = new ArrayList<>(), events = new ArrayList<>();
        final List<Map<String, Object>> details = new ArrayList<>();
        long now;
        boolean running, stuckRunning, uncertain, cancelled, stopFails;
        int stops, procReads;
        final GameStartCheck check = new GameStartCheck((command, timeout) -> {
            commands.add(command); now += 40;
            if (command.equals(GameStartCheck.framed("PIDOF", GameStartCheck.PIDOF)))
                return reply("PIDOF", running ? 0 : 1, running ? "23673\n" : "");
            if (command.equals(GameStartCheck.framed("PROC", GameStartCheck.PROC))) {
                procReads++;
                return uncertain ? reply("PROC", 0, "SUMMARY:200:1:0\n") : running ? presentProc() : absentProc();
            }
            if (command.equals(GameStartCheck.framed("STOP_GAME", GameStartCheck.STOP_GAME))) {
                stops++; if (!stuckRunning && !stopFails) running = false;
                return reply("STOP_GAME", stopFails ? 1 : 0, stopFails ? "Error: not allowed\n" : "");
            }
            throw new AssertionError("Unexpected command: " + command);
        }, (event, values) -> { events.add(event); details.add(values); }, () -> now, millis -> now += millis, () -> cancelled);
        void expectFailure(boolean approved, String expected) throws Exception {
            try { check.prepare(approved); fail("Expected guard failure"); }
            catch (IllegalStateException failure) { assertTrue(failure.getMessage(), failure.getMessage().contains(expected)); }
            assertFalse(events.contains("game_start_ready"));
        }
    }
    @Test public void absentGameRequiresTwoChecksAndDoesNotIssueStopEvenWithConsent() throws Exception {
        Harness h = new Harness(); h.check.prepare(true);
        assertEquals(2, h.procReads); assertEquals(0, h.stops);
        assertTrue(h.events.contains("game_start_ready"));
        assertEquals(4, h.commands.size());
        Map<String, Object> raw = h.details.get(h.events.indexOf("game_start_probe"));
        assertEquals(1, raw.get("exitCode")); assertEquals("", raw.get("payload"));
        assertEquals(reply("PIDOF", 1, "").output, raw.get("output"));
        assertEquals(GameStartCheck.PIDOF, raw.get("command"));
    }
    @Test public void runningGameWithoutConsentCannotBeStopped() throws Exception {
        Harness h = new Harness(); h.running = true; h.expectFailure(false, "start a new session");
        assertEquals(0, h.stops); assertEquals(1, h.procReads);
    }
    @Test public void consentPermitsOneStopFollowedByTwoAbsenceChecks() throws Exception {
        Harness h = new Harness(); h.running = true; h.check.prepare(true);
        assertEquals(1, h.stops); assertEquals(3, h.procReads);
        assertTrue(h.events.indexOf("game_stop_requested") < h.events.indexOf("game_start_ready"));
        assertEquals(7, h.commands.size());
    }
    @Test public void rejectedStopNeverLaunchesOrRetriesStop() throws Exception {
        Harness h = new Harness(); h.running = true; h.stopFails = true; h.expectFailure(true, "Could not stop");
        assertEquals(1, h.stops); assertEquals(1, h.procReads);
        int last = h.events.lastIndexOf("game_start_probe");
        assertEquals("Error: not allowed\n", h.details.get(last).get("payload"));
    }
    @Test public void unknownIdentityFailsClosedAndCannotStopAnyApp() throws Exception {
        Harness h = new Harness(); h.uncertain = true; h.expectFailure(true, "Could not verify");
        assertEquals(0, h.stops); assertEquals(2, h.procReads);
    }
    @Test public void respawnedGameIsBoundedWithoutRepeatedStops() throws Exception {
        Harness h = new Harness(); h.running = true; h.stuckRunning = true; h.expectFailure(true, "Could not confirm");
        assertEquals(1, h.stops); assertTrue(h.now >= 20_000); assertTrue(h.now < 21_000);
    }
    @Test public void cancellationBeforeProbeSendsNoCommands() throws Exception {
        Harness h = new Harness(); h.cancelled = true;
        try { h.check.prepare(true); fail("Expected cancellation"); } catch (InterruptedException expected) { }
        assertTrue(h.commands.isEmpty());
    }
    @Test public void shellExceptionIsRecordedWithCommandTimeoutAndError() throws Exception {
        List<Map<String, Object>> records = new ArrayList<>();
        GameStartCheck check = new GameStartCheck((command, timeout) -> { throw new java.io.IOException("root timeout"); },
                (event, values) -> { assertEquals("game_start_probe_error", event); records.add(values); },
                () -> 0, millis -> { }, () -> false);
        try { check.prepare(true); fail("Expected root error"); } catch (java.io.IOException expected) { }
        assertEquals(1, records.size()); assertEquals(2500L, records.get(0).get("timeoutMs"));
        assertTrue(records.get(0).get("error").toString().contains("root timeout"));
    }
}
