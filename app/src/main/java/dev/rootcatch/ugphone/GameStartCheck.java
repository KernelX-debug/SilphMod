package dev.rootcatch.ugphone;

import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;
import java.util.function.BooleanSupplier;
import java.util.function.LongSupplier;

/** Read-only identity checks; one package stop only for the user's explicit cold-launch start. */
final class GameStartCheck {
    static final String GAME = "com.nianticlabs.pokemongo";
    static final String PIDOF = "pidof " + GAME;
    static final String STOP_GAME = "am force-stop --user current " + GAME;
    static final String PROC =
            "rc_seen=0; rc_unread=0; rc_match=0; "
            + "for rc_dir in /proc/[0-9]*; do [ -d \"$rc_dir\" ] || continue; "
            + "rc_seen=$((rc_seen+1)); "
            + "rc_name=$(tr '\\000' ' ' < \"$rc_dir/cmdline\" 2>/dev/null); rc_read=$?; "
            + "if [ \"$rc_read\" != 0 ]; then [ ! -d \"$rc_dir\" ] || rc_unread=$((rc_unread+1)); continue; fi; "
            + "rc_name=${rc_name%% *}; "
            + "case \"$rc_name\" in '" + GAME + "'|'" + GAME + ":'*) "
            + "rc_stat=$(cat \"$rc_dir/stat\" 2>/dev/null); rc_read=$?; "
            + "if [ \"$rc_read\" != 0 ]; then [ ! -d \"$rc_dir\" ] || rc_unread=$((rc_unread+1)); continue; fi; "
            + "rc_tail=${rc_stat##*) }; set -- $rc_tail; "
            + "case \"${20}\" in ''|*[!0-9]*|0) rc_unread=$((rc_unread+1)); continue;; esac; "
            + "rc_match=$((rc_match+1)); printf 'PROCESS:%s:%s:%s\\n' \"${rc_dir##*/}\" \"${20}\" \"$rc_name\";; esac; "
            + "done; printf 'SUMMARY:%s:%s:%s\\n' \"$rc_seen\" \"$rc_unread\" \"$rc_match\"";

    interface Executor { Reply execute(String command, long timeoutMs) throws Exception; }
    interface Journal { void record(String event, Map<String, Object> values); }
    interface Sleeper { void sleep(long millis) throws InterruptedException; }
    static final class Reply {
        final int code;
        final String output;
        Reply(int code, String output) { this.code = code; this.output = output; }
    }
    enum State { ABSENT, RUNNING, UNVERIFIED }
    static final class Observation {
        final State state;
        final String reason;
        Observation(State state, String reason) { this.state = state; this.reason = reason; }
    }
    static final class Frame {
        final boolean valid;
        final String payload;
        Frame(boolean valid, String payload) { this.valid = valid; this.payload = payload; }
    }

    private final Executor executor;
    private final Journal journal;
    private final LongSupplier clock;
    private final Sleeper sleeper;
    private final BooleanSupplier cancelled;
    GameStartCheck(Executor executor, Journal journal, LongSupplier clock, Sleeper sleeper, BooleanSupplier cancelled) {
        this.executor = executor; this.journal = journal; this.clock = clock;
        this.sleeper = sleeper; this.cancelled = cancelled;
    }
    static String framed(String name, String command) {
        if (!name.matches("[A-Z_]+")) throw new IllegalArgumentException("Invalid probe name");
        // The inner output remains between sentinels, including its trailing newlines.
        // The subshell prevents `exit` from terminating our persistent root shell.
        return "( printf 'SILPH_" + name + "_BEGIN\\n'; " + command
                + "; rc_code=$?; printf '\\nSILPH_" + name + "_END:%s\\n' \"$rc_code\"; exit \"$rc_code\" )";
    }
    static Frame decode(String name, Reply reply) {
        String prefix = "SILPH_" + name + "_BEGIN\n", suffix = "\nSILPH_" + name + "_END:" + reply.code;
        if (reply.output == null || !reply.output.startsWith(prefix) || !reply.output.endsWith(suffix))
            return new Frame(false, "");
        return new Frame(true, reply.output.substring(prefix.length(), reply.output.length() - suffix.length()));
    }
    static Observation decide(Reply pidof, Reply proc) {
        Frame pids = decode("PIDOF", pidof), inventory = decode("PROC", proc);
        if (!inventory.valid || proc.code != 0) return new Observation(State.UNVERIFIED, "proc_query_failed_or_unframed");
        String[] rows = inventory.payload.trim().split("\n");
        if (rows.length < 1 || !rows[rows.length - 1].matches("SUMMARY:[0-9]+:[0-9]+:[0-9]+"))
            return new Observation(State.UNVERIFIED, "proc_summary_invalid");
        try {
            String[] summary = rows[rows.length - 1].split(":");
            int seen = Integer.parseInt(summary[1]), unread = Integer.parseInt(summary[2]), matched = Integer.parseInt(summary[3]);
            if (seen < 1 || unread > seen || matched > seen || matched != rows.length - 1)
                return new Observation(State.UNVERIFIED, "proc_counts_invalid");
            Set<Integer> verified = new HashSet<>();
            for (int i = 0; i < rows.length - 1; i++) {
                String[] fields = rows[i].split(":", 4);
                if (fields.length != 4 || !"PROCESS".equals(fields[0])
                        || !fields[1].matches("[0-9]+") || !fields[2].matches("[0-9]+"))
                    return new Observation(State.UNVERIFIED, "proc_identity_invalid");
                int pid = Integer.parseInt(fields[1]); long ticks = Long.parseLong(fields[2]);
                if (pid <= 1 || ticks <= 0 || !verified.add(pid)
                        || !(GAME.equals(fields[3]) || fields[3].startsWith(GAME + ":")))
                    return new Observation(State.UNVERIFIED, "proc_identity_invalid");
            }
            if (!verified.isEmpty()) return new Observation(State.RUNNING, "verified_game_process");
            if (unread != 0) return new Observation(State.UNVERIFIED, "proc_read_incomplete");
            if (!pids.valid) return new Observation(State.UNVERIFIED, "pidof_unframed");
            String text = pids.payload.trim();
            if (pidof.code == 0 && text.matches("[0-9]+(?:\\s+[0-9]+)*"))
                return new Observation(State.UNVERIFIED, "pidof_proc_disagree");
            if ((pidof.code == 0 || pidof.code == 1) && text.isEmpty())
                return new Observation(State.ABSENT, "empty_pidof_and_complete_proc");
            // A missing/broken pidof is not itself evidence of absence. A complete
            // independent /proc inventory, repeated before launch, is the evidence.
            return new Observation(State.ABSENT, "complete_proc_fallback_pidof_anomaly");
        } catch (NumberFormatException invalid) { return new Observation(State.UNVERIFIED, "proc_number_invalid"); }
    }
    private Reply query(String name, String command, long timeout, String phase, int attempt) throws Exception {
        if (cancelled.getAsBoolean()) throw new InterruptedException("Startup cancelled");
        long began = clock.getAsLong();
        String wireCommand = framed(name, command);
        Map<String, Object> values = values("phase", phase, "attempt", attempt, "probe", name,
                "command", command, "shellCommand", wireCommand, "timeoutMs", timeout,
                "stderrMerged", true, "transport", "RootShell UTF-8 line reader; outer frame trimmed only");
        try {
            Reply reply = executor.execute(wireCommand, timeout);
            Frame frame = decode(name, reply);
            values.put("exitCode", reply.code); values.put("output", reply.output);
            values.put("frameValid", frame.valid); values.put("payload", frame.payload);
            values.put("durationMs", clock.getAsLong() - began);
            journal.record("game_start_probe", values);
            return reply;
        } catch (Exception error) {
            values.put("error", error.toString()); values.put("durationMs", clock.getAsLong() - began);
            journal.record("game_start_probe_error", values); throw error;
        }
    }
    /** Two independent absence observations, no automatic repeated stop/relaunch. */
    void prepare(boolean allowGameStop) throws Exception {
        long deadline = clock.getAsLong() + 20_000;
        int attempt = 0, absent = 0, uncertain = 0;
        boolean stopIssued = false;
        do {
            if (cancelled.getAsBoolean()) throw new InterruptedException("Startup cancelled");
            String phase = stopIssued ? "after_user_approved_stop" : "before_launch";
            Reply pidof = query("PIDOF", PIDOF, 2500, phase, ++attempt);
            Reply proc = query("PROC", PROC, 8000, phase, attempt);
            Observation observation = decide(pidof, proc);
            journal.record("game_start_decision", values("phase", phase, "attempt", attempt,
                    "state", observation.state.name(), "reason", observation.reason,
                    "userApprovedGameStop", allowGameStop, "stopAlreadyIssued", stopIssued));
            if (observation.state == State.ABSENT) {
                uncertain = 0;
                if (++absent >= 2) {
                    journal.record("game_start_ready", values("gameAbsentConfirmed", true,
                            "consecutiveAbsenceChecks", absent, "userApprovedStopIssued", stopIssued));
                    return;
                }
            } else {
                absent = 0;
                if (observation.state == State.UNVERIFIED) {
                    if (++uncertain >= 2) throw new IllegalStateException("Could not verify the Pokémon GO process; export logs");
                } else {
                    uncertain = 0;
                    if (!allowGameStop) throw new IllegalStateException("Pokémon GO is still running; start a new session from SilphMod");
                    if (!stopIssued) {
                        journal.record("game_stop_requested", values("package", GAME, "userApproved", true,
                                "maxStopAttempts", 1, "gameDataCleared", false, "otherAppsStopped", false));
                        stopIssued = true;
                        Reply stopped = query("STOP_GAME", STOP_GAME, 5000, "user_approved_stop", attempt);
                        if (stopped.code != 0 || !decode("STOP_GAME", stopped).valid)
                            throw new IllegalStateException("Could not stop Pokémon GO; export logs");
                    }
                }
            }
            sleeper.sleep(350);
        } while (clock.getAsLong() < deadline);
        throw new IllegalStateException("Could not confirm a clean Pokémon GO startup; export logs");
    }
    static Map<String, Object> values(Object... pairs) {
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < pairs.length; i += 2) result.put((String) pairs[i], pairs[i + 1]);
        return result;
    }
}
