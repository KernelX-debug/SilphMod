package dev.rootcatch.ugphone;

final class StartupOrphans {
    private StartupOrphans() { }
    static boolean onlyChildren(GameStartCheck.Reply pids, GameStartCheck.Reply inventory) {
        if (GameStartCheck.decide(pids, inventory).state != GameStartCheck.State.RUNNING) return false;
        String[] rows = GameStartCheck.decode("PROC", inventory).payload.trim().split("\n");
        String[] summary = rows[rows.length - 1].split(":");
        if (!"0".equals(summary[2])) return false;
        for (int i = 0; i < rows.length - 1; i++) {
            String[] fields = rows[i].split(":", 4);
            if (!fields[3].startsWith(GameStartCheck.GAME + ":")
                    || fields[3].length() <= GameStartCheck.GAME.length() + 1) return false;
        }
        return rows.length > 1;
    }
    static String command(int exitedPid) {
        if (exitedPid <= 1) throw new IllegalArgumentException("Invalid previous game PID");
        return "[ ! -d /proc/" + exitedPid + " ] || exit 71; rc_inventory=$(" + GameStartCheck.FAST_PROC
                + ") || exit 72; printf '%s\\n' \"$rc_inventory\" | awk -F: -v game='" + GameStartCheck.GAME
                + "' '$1 == \"PROCESS\" && $2 > 1 && $3 > 0 && $4 == game && NF >= 5 && length($5) > 0 { children++; next } "
                + "$1 == \"SUMMARY\" && NF == 4 && $2 > 0 && $3 == 0 && $4 == children { summary++; next } "
                + "{ invalid=1 } END { if (invalid || summary != 1 || children < 1) exit 1 }' || exit 73; "
                + "[ ! -d /proc/" + exitedPid + " ] || exit 74; " + GameStartCheck.STOP_GAME;
    }
}
