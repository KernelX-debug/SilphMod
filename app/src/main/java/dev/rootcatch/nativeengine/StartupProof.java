package dev.rootcatch.nativeengine;

/** Pure verification of the automatic, same-attach observation. */
public final class StartupProof {
    private StartupProof() { }
    public static boolean sameProcess(String stat, long expectedStart) {
        if (stat == null || expectedStart <= 0) return false;
        int closing = stat.lastIndexOf(')');
        if (closing < 1) return false;
        String[] fields = stat.substring(closing + 1).trim().split("\\s+");
        if (fields.length <= 19 || !fields[19].matches("[0-9]+")) return false;
        try { return Long.parseLong(fields[19]) == expectedStart; }
        catch (NumberFormatException error) { return false; }
    }
    public static boolean valid(String expectedSession, int expectedPid, String expectedPlayer,
            String session, int pid, String player, String token, int samples, long elapsed,
            int references, int gameRequests, boolean automatic, boolean singleAttach) {
        return expectedSession != null && expectedSession.equals(session) && expectedPid > 1 && expectedPid == pid
                && expectedPlayer != null && expectedPlayer.matches("[^\\x00-\\x20\\x7f]{1,256}") && expectedPlayer.equals(player)
                && token != null && token.matches(java.util.regex.Pattern.quote(expectedSession) + ":[1-9][0-9]{0,9}")
                && samples >= 20 && elapsed >= 10_000 && references >= 4 && gameRequests == 0 && automatic && singleAttach;
    }
}
