package dev.rootcatch.nativeengine;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Parses WindowManager focus, never inferring focus from a resumed activity. */
final class ForegroundFocus {
    enum State { GAME, OTHER, NO_WINDOW, UNAVAILABLE, AMBIGUOUS }
    private static final Pattern DISPLAY = Pattern.compile("^Display: mDisplayId=([0-9]+)(?:\\s.*)?$");
    private static final Pattern LEGACY_DISPLAY = Pattern.compile("^Display #([0-9]+)(?::|\\s.*)?$");
    private static final Pattern TOP = Pattern.compile("^mTopFocusedDisplayId=([0-9]+)$");
    private static final Pattern WINDOW = Pattern.compile(
            "^Window\\{[^\\s{}]+\\s+u[0-9]+\\s+([^\\s/{}]+)(?:/[^\\s{}]+)?(?:\\s+[^{}]*)?\\}$");
    final State state;
    final String windowOwner, reason;
    final int displayId;

    private ForegroundFocus(State state, String owner, int display, String reason) {
        this.state = state; this.windowOwner = owner; this.displayId = display; this.reason = reason;
    }
    static ForegroundFocus unavailable(String reason) { return new ForegroundFocus(State.UNAVAILABLE, null, -1, reason); }
    private static ForegroundFocus ambiguous(String reason) { return new ForegroundFocus(State.AMBIGUOUS, null, -1, reason); }
    boolean isGame() { return state == State.GAME; }
    boolean known() { return state == State.GAME || state == State.OTHER || state == State.NO_WINDOW; }
    Boolean focusedValue() { return known() ? isGame() : null; }

    static ForegroundFocus parse(String output) {
        if (output == null || output.trim().isEmpty()) return unavailable("empty_response");
        List<ForegroundFocus> entries = new ArrayList<>();
        Set<Integer> displays = new HashSet<>(), entryDisplays = new HashSet<>();
        int current = -1, top = -1;
        boolean topSeen = false;
        for (String raw : output.split("\\R")) {
            String line = raw.trim();
            Matcher display = DISPLAY.matcher(line), legacy = LEGACY_DISPLAY.matcher(line);
            if (display.matches() || legacy.matches()) {
                current = number(display.matches() ? display.group(1) : legacy.group(1));
                if (current < 0 || !displays.add(current)) return ambiguous("invalid_or_duplicate_display");
                continue;
            }
            if (line.startsWith("Display: mDisplayId=") || line.startsWith("Display #"))
                return ambiguous("unrecognized_display_header");
            if (line.startsWith("mTopFocusedDisplayId=")) {
                Matcher match = TOP.matcher(line);
                if (!match.matches() || topSeen) return ambiguous("invalid_or_duplicate_top_display");
                top = number(match.group(1)); topSeen = true;
                if (top < 0) return ambiguous("invalid_top_display");
            }
            if (!line.startsWith("mCurrentFocus=")) continue;
            if (!entryDisplays.add(current)) return ambiguous("duplicate_current_focus");
            String value = line.substring("mCurrentFocus=".length()).trim();
            if (value.equals("null")) {
                entries.add(new ForegroundFocus(State.NO_WINDOW, null, current, "explicit_null_window"));
                continue;
            }
            Matcher match = WINDOW.matcher(value);
            if (!match.matches()) return ambiguous("unrecognized_window_format");
            String owner = match.group(1);
            entries.add(new ForegroundFocus(EnginePolicy.GAME.equals(owner) ? State.GAME : State.OTHER,
                    owner, current, "current_window"));
        }
        if (entries.isEmpty()) return unavailable("current_focus_field_absent");
        if (topSeen && !displays.isEmpty()) {
            for (ForegroundFocus entry : entries) if (entry.displayId == top) return entry;
            return unavailable("top_display_focus_absent");
        }
        if (entries.size() != 1 || displays.size() > 1) return ambiguous("top_focused_display_unknown");
        return entries.get(0);
    }
    private static int number(String value) {
        try { return Integer.parseInt(value); } catch (NumberFormatException error) { return -1; }
    }
}
