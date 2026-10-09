package dev.rootcatch.ugphone;

import java.util.LinkedHashMap;
import java.util.Map;

final class SessionLimitStop {
    static final class Result {
        final boolean commandAccepted, gameClosed;
        final String status, error;
        final int observations;

        Result(boolean commandAccepted, boolean gameClosed, String status, String error, int observations) {
            this.commandAccepted = commandAccepted; this.gameClosed = gameClosed;
            this.status = status; this.error = error; this.observations = observations;
        }
    }
    private final GameStartCheck.Executor executor;
    private final GameStartCheck.Journal journal;
    private final GameStartCheck.Sleeper sleeper;

    SessionLimitStop(GameStartCheck.Executor executor, GameStartCheck.Journal journal, GameStartCheck.Sleeper sleeper) {
        this.executor = executor; this.journal = journal; this.sleeper = sleeper;
    }

    static String command(int pid, long startTicks) {
        if (pid <= 1 || startTicks <= 0) throw new IllegalArgumentException("Invalid process identity");
        return "rc_pid=$(pidof " + GameStartCheck.GAME + "); [ \"$rc_pid\" = \"" + pid + "\" ] || exit 71; "
                + "rc_stat=$(cat /proc/" + pid + "/stat) || exit 72; rc_tail=${rc_stat##*) }; set -- $rc_tail; "
                + "[ \"${20}\" = \"" + startTicks + "\" ] || exit 73; " + GameStartCheck.STOP_GAME;
    }

    static String targetCommand(int pid) {
        if (pid <= 1) throw new IllegalArgumentException("Invalid process identity");
        return "if [ ! -d /proc/" + pid + " ]; then printf 'ABSENT\\n'; "
                + "elif rc_stat=$(cat /proc/" + pid + "/stat 2>/dev/null); then printf '%s\\n' \"$rc_stat\"; "
                + "elif [ ! -d /proc/" + pid + " ]; then printf 'ABSENT\\n'; else printf 'UNVERIFIED\\n'; fi";
    }

    static GameStartCheck.State absence(GameStartCheck.Reply pidof, GameStartCheck.Reply target, long startTicks) {
        GameStartCheck.Frame pids = GameStartCheck.decode("PIDOF", pidof);
        GameStartCheck.Frame process = GameStartCheck.decode("TARGET", target);
        if (!pids.valid || !process.valid || target.code != 0) return GameStartCheck.State.UNVERIFIED;
        String ids = pids.payload.trim();
        if (pidof.code == 0 && ids.matches("[0-9]+(?:\\s+[0-9]+)*")) return GameStartCheck.State.RUNNING;
        if ((pidof.code != 0 && pidof.code != 1) || !ids.isEmpty()) return GameStartCheck.State.UNVERIFIED;
        String identity = process.payload.trim();
        if ("ABSENT".equals(identity)) return GameStartCheck.State.ABSENT;
        try {
            return FarmPolicy.processStartTicks(identity) == startTicks ? GameStartCheck.State.RUNNING : GameStartCheck.State.ABSENT;
        } catch (IllegalArgumentException invalid) { return GameStartCheck.State.UNVERIFIED; }
    }

    private GameStartCheck.Reply query(String name, String command, long timeout) throws Exception {
        Map<String, Object> values = new LinkedHashMap<>();
        values.put("probe", name); values.put("command", command); values.put("timeoutMs", timeout);
        long began = System.nanoTime();
        try {
            GameStartCheck.Reply reply = executor.execute(GameStartCheck.framed(name, command), timeout);
            values.put("exitCode", reply.code); values.put("output", reply.output);
            return reply;
        } catch (Exception error) {
            values.put("error", error.toString()); values.put("errorClass", error.getClass().getName());
            throw error;
        } finally {
            values.put("durationMs", (System.nanoTime() - began) / 1_000_000);
            journal.record("session_limit_game_close", values);
        }
    }

    Result close(int pid, long startTicks) {
        boolean accepted = false;
        int observations = 0, consecutiveAbsent = 0;
        try {
            GameStartCheck.Reply stop = query("LIMIT_STOP", command(pid, startTicks), 5000);
            if (!GameStartCheck.decode("LIMIT_STOP", stop).valid || stop.code != 0)
                return new Result(false, false, "stop_unconfirmed", "", 0);
            accepted = true;
            for (int attempt = 0; attempt < 4; attempt++) {
                GameStartCheck.Reply pidof = query("PIDOF", GameStartCheck.PIDOF, 1500);
                GameStartCheck.Reply target = query("TARGET", targetCommand(pid), 1500);
                observations++;
                GameStartCheck.State state = absence(pidof, target, startTicks);
                if (state == GameStartCheck.State.UNVERIFIED)
                    return new Result(true, false, "verification_unconfirmed", "", observations);
                consecutiveAbsent = state == GameStartCheck.State.ABSENT ? consecutiveAbsent + 1 : 0;
                if (consecutiveAbsent == 2) return new Result(true, true, "confirmed", "", observations);
                if (attempt < 3) sleeper.sleep(250);
            }
            return new Result(true, false, "process_still_present", "", observations);
        } catch (InterruptedException error) {
            Thread.currentThread().interrupt();
            return new Result(accepted, false, "interrupted", error.toString(), observations);
        } catch (Exception error) {
            return new Result(accepted, false, "query_failed", error.toString(), observations);
        }
    }
}
