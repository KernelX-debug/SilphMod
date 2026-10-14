package dev.rootcatch.nativeengine;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;

/** Pure policy: no Android/game dependency, independently unit-testable. */
public final class EnginePolicy {
    public static final String GAME = "com.nianticlabs.pokemongo";
    public static final String VERSION = "0.433.0";
    public static final long VERSION_CODE = 2026100702L;
    public static final String CLEANUP_TRIGGER = "EXCESS_FROM_START";
    public static final String DEFAULT_LIMITS = "Potion=50,SuperPotion=50,Revive=30,NanabBerry=30";
    public static final Map<String, Integer> ITEMS;
    static {
        Map<String, Integer> items = new LinkedHashMap<>();
        for (ItemCatalog.Entry entry : ItemCatalog.ENTRIES)
            if (entry.discardable()) items.put(entry.name(), entry.id());
        ITEMS = Collections.unmodifiableMap(items);
    }
    private EnginePolicy() { }

    public static Map<String, Integer> parseLimits(String text) {
        Map<String, Integer> limits = new LinkedHashMap<>();
        if (text == null || text.trim().isEmpty()) return limits;
        for (String entry : text.split(",", -1)) {
            String[] parts = entry.trim().split("=", -1);
            if (parts.length != 2 || !ITEMS.containsKey(parts[0].trim()))
                throw new IllegalArgumentException("Item not allowed or invalid format: " + entry);
            String name = parts[0].trim();
            int keep;
            try { keep = Integer.parseInt(parts[1].trim()); }
            catch (NumberFormatException e) { throw new IllegalArgumentException("Invalid amount: " + entry); }
            if (keep < 0 || keep > 10000 || limits.containsKey(name))
                throw new IllegalArgumentException("Invalid or duplicate limit: " + entry);
            limits.put(name, keep);
        }
        return limits;
    }
    public static String limitsKey(String text) {
        return parseLimits(text).entrySet().stream().sorted(Map.Entry.comparingByKey())
                .map(entry -> entry.getKey() + "=" + entry.getValue()).collect(java.util.stream.Collectors.joining(","));
    }

    public static boolean supports(String name, String version, long code) {
        return GAME.equals(name) && VERSION.equals(version) && code == VERSION_CODE;
    }

    public static boolean gcValidationValid(boolean passed, int bits, int cached, int roundTrips,
                                             boolean strong, boolean weak, boolean freed) {
        return passed && bits == 64 && cached >= 4 && roundTrips >= 4 && strong && weak && freed;
    }

    public static boolean gcCleanupValid(boolean passed, int bits, int remaining) {
        return passed && bits == 64 && remaining == 0;
    }
    public static boolean readStabilityValid(boolean passed, int samples, long elapsedMs, int cached) {
        return passed && samples >= 10 && elapsedMs >= 9000 && cached >= 4;
    }

    public static boolean expectedPipeClose(boolean stopRequested, Exception error) {
        if (!stopRequested || !(error instanceof java.io.IOException)) return false;
        String message = error.getMessage();
        return message != null && (message.equalsIgnoreCase("Stream closed")
                || message.equalsIgnoreCase("Pipe closed") || message.equalsIgnoreCase("Broken pipe"));
    }

    public static int uniquePid(String output) {
        if (output == null || !output.trim().matches("[1-9][0-9]{0,9}")) return -1;
        try { int value = Integer.parseInt(output.trim()); return value > 1 ? value : -1; }
        catch (NumberFormatException ignored) { return -1; }
    }

    public static String quote(String value) {
        if (value == null || value.indexOf('\0') >= 0 || value.indexOf('\n') >= 0 || value.indexOf('\r') >= 0)
            throw new IllegalArgumentException("Invalid shell argument");
        return "'" + value.replace("'", "'\\''") + "'";
    }

    public static String attachCommand(String binary, String script, int pid, String json) {
        if (pid <= 1 || !binary.startsWith("/data/") || !script.startsWith("/data/"))
            throw new IllegalArgumentException("Invalid process or path");
        // Report ONLY our helper PID, then replace that shell; never eternalize.
        return "echo ROOTCATCH_HELPER_PID=$$; exec " + quote(binary) + " -p " + pid
                + " -s " + quote(script) + " -i -P " + quote(json);
    }

    public static String killCommand(int helperPid, int gamePid, String binary) {
        if (helperPid <= 1 || helperPid == gamePid || !binary.startsWith("/data/"))
            throw new IllegalArgumentException("This process cannot be terminated");
        return "if [ \"$(readlink /proc/" + helperPid + "/exe)\" = " + quote(binary)
                + " ]; then kill -TERM " + helperPid + "; fi";
    }
}
