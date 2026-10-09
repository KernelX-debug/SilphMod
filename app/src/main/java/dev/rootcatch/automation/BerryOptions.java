package dev.rootcatch.automation;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;

public final class BerryOptions {
    public static final String DEFAULT_ORDER = "RazzBerry,PinapBerry,NanabBerry";
    public static final List<String> ALLOWED = Collections.unmodifiableList(Arrays.asList(
            "RazzBerry", "PinapBerry", "NanabBerry", "GoldenRazzBerry", "GoldenPinapBerry"));
    private BerryOptions() { }
    public static List<String> parse(String value) {
        if (value == null) throw new IllegalArgumentException("Invalid berry priority");
        if (value.isEmpty()) return Collections.emptyList();
        List<String> result = new ArrayList<>(Arrays.asList(value.split(",", -1)));
        if (result.size() > 5 || new HashSet<>(result).size() != result.size() || !ALLOWED.containsAll(result))
            throw new IllegalArgumentException("Invalid berry priority");
        return Collections.unmodifiableList(result);
    }
    public static String fromSavedValue(String value) {
        try { return String.join(",", parse(value)); }
        catch (IllegalArgumentException ignored) { return DEFAULT_ORDER; }
    }
}
