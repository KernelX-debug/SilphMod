package dev.rootcatch.ugphone;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.io.IOException;
import org.junit.Test;
import static org.junit.Assert.*;

public final class SessionLimitStopTest {
    private static GameStartCheck.Reply reply(String name, int code, String output) {
        return new GameStartCheck.Reply(code, "SILPH_" + name + "_BEGIN\n" + output + "\nSILPH_" + name + "_END:" + code);
    }
    private static String identity(long ticks) { return "1234 (game process) S " + "0 ".repeat(18) + ticks; }
    @Test public void stopCommandTargetsOnlyTheExpectedGameIdentity() {
        String command = SessionLimitStop.command(1234, 56789);
        assertTrue(command.contains("[ \"$rc_pid\" = \"1234\" ] || exit 71"));
        assertTrue(command.contains("/proc/1234/stat"));
        assertTrue(command.contains("[ \"${20}\" = \"56789\" ] || exit 73"));
        assertTrue(command.endsWith(GameStartCheck.STOP_GAME));
        assertThrows(IllegalArgumentException.class, () -> SessionLimitStop.command(1, 1));
        assertThrows(IllegalArgumentException.class, () -> SessionLimitStop.command(100, 0));
        assertFalse(command.contains("clear")); assertFalse(command.contains("kill -9"));
    }
    @Test public void confirmedCloseNeedsTwoIndependentAbsentObservationsAndOneStop() throws Exception {
        ArrayDeque<GameStartCheck.Reply> replies = new ArrayDeque<>();
        replies.add(reply("LIMIT_STOP", 0, ""));
        for (int i = 0; i < 2; i++) {
            replies.add(reply("PIDOF", 0, ""));
            replies.add(reply("TARGET", 0, "ABSENT"));
        }
        List<String> commands = new ArrayList<>(); List<Long> waits = new ArrayList<>();
        SessionLimitStop stop = new SessionLimitStop((command, timeout) -> {
            commands.add(command); return replies.remove();
        }, (event, values) -> assertEquals("session_limit_game_close", event), waits::add);
        SessionLimitStop.Result result = stop.close(1234, 56789);
        assertTrue(result.gameClosed); assertTrue(result.commandAccepted); assertEquals("confirmed", result.status);
        assertEquals(2, result.observations); assertTrue(replies.isEmpty());
        assertEquals(1, commands.stream().filter(s -> s.contains("am force-stop")).count());
        assertFalse(commands.stream().anyMatch(s -> s.contains("/proc/[0-9]*")));
        assertEquals(List.of(250L), waits);
    }
    @Test public void changedIdentityOrFailedCommandDoesNotRetryOrClaimClosure() throws Exception {
        for (int code : new int[]{71, 72, 73, 1}) {
            List<String> commands = new ArrayList<>();
            SessionLimitStop stop = new SessionLimitStop((command, timeout) -> {
                commands.add(command); return reply("LIMIT_STOP", code, "not stopped");
            }, (event, values) -> { }, millis -> fail("No wait after rejected stop"));
            SessionLimitStop.Result result = stop.close(1234, 56789);
            assertFalse(result.gameClosed); assertFalse(result.commandAccepted); assertEquals(1, commands.size());
        }
    }
    @Test public void unverifiedAbsenceDoesNotBecomeASuccessfulClose() throws Exception {
        ArrayDeque<GameStartCheck.Reply> replies = new ArrayDeque<>(List.of(reply("LIMIT_STOP", 0, ""),
                reply("PIDOF", 0, ""), reply("TARGET", 0, "UNVERIFIED")));
        SessionLimitStop stop = new SessionLimitStop((command, timeout) -> replies.remove(),
                (event, values) -> { }, millis -> fail("No wait after unverified observation"));
        SessionLimitStop.Result result = stop.close(1234, 56789);
        assertFalse(result.gameClosed); assertTrue(result.commandAccepted);
        assertEquals("verification_unconfirmed", result.status); assertTrue(replies.isEmpty());
    }
    @Test public void cheapTargetProbeAndPidofMustAgreeBeforeConfirmingAbsence() {
        GameStartCheck.Reply absent = reply("PIDOF", 1, "");
        assertEquals(GameStartCheck.State.ABSENT, SessionLimitStop.absence(absent, reply("TARGET", 0, "ABSENT"), 56789));
        assertEquals(GameStartCheck.State.RUNNING, SessionLimitStop.absence(absent, reply("TARGET", 0, identity(56789)), 56789));
        assertEquals(GameStartCheck.State.ABSENT, SessionLimitStop.absence(absent, reply("TARGET", 0, identity(56790)), 56789));
        assertEquals(GameStartCheck.State.RUNNING, SessionLimitStop.absence(reply("PIDOF", 0, "2222"), reply("TARGET", 0, "ABSENT"), 56789));
        assertEquals(GameStartCheck.State.UNVERIFIED, SessionLimitStop.absence(reply("PIDOF", 127, ""), reply("TARGET", 0, "ABSENT"), 56789));
        assertEquals(GameStartCheck.State.UNVERIFIED, SessionLimitStop.absence(absent, reply("TARGET", 1, "Permission denied"), 56789));
        assertEquals(GameStartCheck.State.UNVERIFIED, SessionLimitStop.absence(absent, new GameStartCheck.Reply(0, "ABSENT"), 56789));
        assertEquals(GameStartCheck.State.UNVERIFIED, SessionLimitStop.absence(absent, reply("TARGET", 0, "malformed"), 56789));
        assertFalse(SessionLimitStop.targetCommand(1234).contains("for "));
        assertThrows(IllegalArgumentException.class, () -> SessionLimitStop.targetCommand(1));
    }
    @Test public void pendingShutdownCanSettleWithoutSendingAnotherStop() {
        ArrayDeque<GameStartCheck.Reply> replies = new ArrayDeque<>(List.of(reply("LIMIT_STOP", 0, ""),
                reply("PIDOF", 0, "1234"), reply("TARGET", 0, identity(56789)),
                reply("PIDOF", 1, ""), reply("TARGET", 0, "ABSENT"),
                reply("PIDOF", 1, ""), reply("TARGET", 0, "ABSENT")));
        List<String> commands = new ArrayList<>();
        SessionLimitStop stop = new SessionLimitStop((command, timeout) -> {
            commands.add(command); return replies.remove();
        }, (event, values) -> { }, millis -> assertEquals(250, millis));
        SessionLimitStop.Result result = stop.close(1234, 56789);
        assertTrue(result.gameClosed); assertEquals(3, result.observations);
        assertEquals(1, commands.stream().filter(s -> s.contains("am force-stop")).count());
    }
    @Test public void stillRunningProcessHasBoundedObservationsAndNoRepeatedWrite() {
        List<String> commands = new ArrayList<>();
        SessionLimitStop stop = new SessionLimitStop((command, timeout) -> {
            commands.add(command);
            return command.contains("LIMIT_STOP") ? reply("LIMIT_STOP", 0, "")
                    : command.contains("PIDOF") ? reply("PIDOF", 0, "1234") : reply("TARGET", 0, identity(56789));
        }, (event, values) -> { }, millis -> assertEquals(250, millis));
        SessionLimitStop.Result result = stop.close(1234, 56789);
        assertFalse(result.gameClosed); assertEquals("process_still_present", result.status);
        assertEquals(4, result.observations); assertEquals(9, commands.size());
        assertEquals(1, commands.stream().filter(s -> s.contains("am force-stop")).count());
    }
    @Test public void timeoutsAreRecordedAsClosureDiagnosticsRatherThanThrownAsEngineFailure() {
        for (String failed : List.of("LIMIT_STOP", "PIDOF", "TARGET")) {
            List<Map<String, Object>> events = new ArrayList<>(); List<String> commands = new ArrayList<>();
            SessionLimitStop stop = new SessionLimitStop((command, timeout) -> {
                commands.add(command);
                if (command.contains("SILPH_" + failed + "_BEGIN")) throw new IOException("Root command timed out (" + timeout + " ms)");
                return command.contains("LIMIT_STOP") ? reply("LIMIT_STOP", 0, "") : reply("PIDOF", 1, "");
            }, (event, values) -> events.add(new LinkedHashMap<>(values)), millis -> fail("No retry after a timeout"));
            SessionLimitStop.Result result = stop.close(1234, 56789);
            assertFalse(result.gameClosed); assertEquals("query_failed", result.status);
            assertEquals(!failed.equals("LIMIT_STOP"), result.commandAccepted);
            assertTrue(result.error.contains("Root command timed out"));
            Map<String, Object> diagnostic = events.get(events.size() - 1);
            assertEquals(failed, diagnostic.get("probe")); assertEquals("java.io.IOException", diagnostic.get("errorClass"));
            assertTrue(diagnostic.containsKey("timeoutMs")); assertTrue(diagnostic.containsKey("durationMs"));
            assertEquals(1, commands.stream().filter(s -> s.contains("am force-stop")).count());
        }
    }
    @Test public void interruptedClosureKeepsItsDiagnosticAndRestoresInterruption() {
        SessionLimitStop stop = new SessionLimitStop((command, timeout) -> {
            if (command.contains("LIMIT_STOP")) return reply("LIMIT_STOP", 0, "");
            return command.contains("PIDOF") ? reply("PIDOF", 1, "") : reply("TARGET", 0, "ABSENT");
        }, (event, values) -> { }, millis -> { throw new InterruptedException("Stopped"); });
        try {
            SessionLimitStop.Result result = stop.close(1234, 56789);
            assertFalse(result.gameClosed); assertEquals("interrupted", result.status);
            assertTrue(Thread.currentThread().isInterrupted());
        } finally { Thread.interrupted(); }
    }
}
