package dev.rootcatch.ugphone;

import dev.rootcatch.nativeengine.StartupProof;

final class BoundGameProcess {
    enum State { MATCH, CHANGED, UNAVAILABLE }
    static final class Result {
        final State state;
        final String reason;
        Result(State state, String reason) { this.state = state; this.reason = reason; }
        boolean matches() { return state == State.MATCH; }
    }

    static String command(int pid) {
        if (pid <= 1) throw new IllegalArgumentException("Invalid bound PID");
        String directory = "/proc/" + pid;
        return "printf 'SILPH_BOUND_BEGIN\\n'; "
                + "rc_before=$(cat " + directory + "/stat 2>&1); rc_before_exit=$?; "
                + "rc_name=$(tr '\\000' ' ' < " + directory + "/cmdline 2>&1); rc_name_exit=$?; "
                + "rc_name=${rc_name%% *}; "
                + "rc_after=$(cat " + directory + "/stat 2>&1); rc_after_exit=$?; "
                + "printf 'BEFORE:%s:%s\\nCMDLINE:%s:%s\\nAFTER:%s:%s\\nSILPH_BOUND_END\\n' "
                + "\"$rc_before_exit\" \"$rc_before\" \"$rc_name_exit\" \"$rc_name\" \"$rc_after_exit\" \"$rc_after\"";
    }

    static Result verify(int shellExit, String output, int pid, long expectedStart) {
        if (pid <= 1 || expectedStart <= 0) return unavailable("invalid_bound_identity");
        if (shellExit != 0 || output == null) return unavailable("identity_command_failed");
        String[] lines = output.trim().split("\\R", -1);
        if (lines.length != 5 || !lines[0].equals("SILPH_BOUND_BEGIN") || !lines[4].equals("SILPH_BOUND_END"))
            return unavailable("identity_frame_invalid");
        String before = value(lines[1], "BEFORE"), name = value(lines[2], "CMDLINE"), after = value(lines[3], "AFTER");
        if (before == null || name == null || after == null) return unavailable("identity_read_failed");
        try {
            for (String stat : new String[]{before, after}) {
                int opening = stat.indexOf('('), closing = stat.lastIndexOf(')');
                if (opening <= 1 || closing <= opening) return unavailable("identity_stat_invalid");
                int observedPid = Integer.parseInt(stat.substring(0, opening).trim());
                if (observedPid != pid) return changed("bound_pid_differs");
                String state = stat.substring(closing + 1).trim().split("\\s+")[0];
                if (state.equals("Z") || state.equals("X") || state.equals("x")) return changed("bound_process_exited");
                if (!state.matches("[RSDTtWIKP]")) return unavailable("identity_state_invalid");
                if (StartupProof.processStartTicks(stat) != expectedStart) return changed("bound_start_time_differs");
            }
        } catch (IllegalArgumentException error) { return unavailable("identity_stat_invalid"); }
        if (!GameStartCheck.GAME.equals(name)) return changed("bound_cmdline_differs");
        return new Result(State.MATCH, "bound_pid_cmdline_and_start_time_match");
    }

    private static String value(String line, String prefix) {
        String expected = prefix + ":0:";
        return line.startsWith(expected) ? line.substring(expected.length()) : null;
    }
    private static Result unavailable(String reason) { return new Result(State.UNAVAILABLE, reason); }
    private static Result changed(String reason) { return new Result(State.CHANGED, reason); }
}
