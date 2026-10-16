package dev.rootcatch.ugphone;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class ProcessReadiness {
    static final long STABLE_MS = 12_000;
    private static final Pattern MAPPING = Pattern.compile(
            "^[0-9a-f]+-[0-9a-f]+ r[-w]xp [0-9a-f]+ [0-9a-f]+:[0-9a-f]+ [0-9]+ +\\S*(libunity\\.so|libil2cpp\\.so|libNianticLabsPlugin\\.so)(?: \\(deleted\\))?$");
    private int pid;
    private long ticks, since = -1;
    private String previous = "";
    static String command(int pid) {
        if (pid <= 1) throw new IllegalArgumentException("Invalid game PID");
        return "grep '^TracerPid:' /proc/" + pid + "/status && grep -E 'r[-w]xp .*"
                + "(libunity\\.so|libil2cpp\\.so|libNianticLabsPlugin\\.so)( \\(deleted\\))?$' /proc/" + pid + "/maps";
    }
    boolean observe(int observedPid, long start, int code, String output, long now) {
        List<String> libraries = new ArrayList<>(), mappings = new ArrayList<>();
        int tracerFields = 0; boolean untraced = false;
        if (output != null && output.length() <= 32768) for (String raw : output.split("\\R")) {
            String line = raw.trim();
            if (line.startsWith("TracerPid:")) { tracerFields++; untraced = line.matches("TracerPid:\\s+0"); continue; }
            Matcher mapping = MAPPING.matcher(line);
            if (mapping.matches()) { libraries.add(mapping.group(1)); mappings.add(line); }
            else if (!line.isEmpty()) { reset(); return false; }
        }
        if (observedPid <= 1 || start <= 0 || code != 0 || now < 0 || tracerFields != 1 || !untraced
                || !libraries.contains("libunity.so") || !libraries.contains("libil2cpp.so")
                || !libraries.contains("libNianticLabsPlugin.so")) { reset(); return false; }
        Collections.sort(mappings); String identity = String.join("\n", mappings);
        if (pid != observedPid || ticks != start || !previous.equals(identity) || now < since) {
            pid = observedPid; ticks = start; previous = identity; since = now;
        }
        return since >= 0 && now - since >= STABLE_MS;
    }
    void reset() { pid = 0; ticks = 0; since = -1; previous = ""; }
}
