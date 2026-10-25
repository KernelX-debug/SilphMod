package dev.rootcatch.nativeengine;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class ForegroundFocus {
    enum State { GAME, OTHER, NOTIFICATION_SHADE, NO_WINDOW, UNAVAILABLE, AMBIGUOUS }
    private static final Pattern DISPLAY = Pattern.compile("^Display: mDisplayId=([0-9]+)(?:\\s.*)?$");
    private static final Pattern LEGACY_DISPLAY = Pattern.compile("^Display #([0-9]+)(?::|\\s.*)?$");
    private static final Pattern JOINED_DISPLAY = Pattern.compile(
            "^mInputMethodWindow=(?:null|Window\\{[^{}]*\\})\\h+(Display: mDisplayId=.*)$");
    private static final Pattern HISTORICAL_DISPLAY = Pattern.compile(
            "^Display #[0-9]+ currentFocus=(?:null|Window\\{[^{}]*\\}) focusedApp=(?:null|ActivityRecord\\{[^{}]*\\})$");
    private static final Pattern TOP = Pattern.compile("^mTopFocusedDisplayId=([0-9]+)$");
    private static final Pattern WINDOW = Pattern.compile(
            "^Window\\{[^\\s{}]+\\s+u[0-9]+\\s+([^\\s/{}]+)(?:/[^\\s{}]+)?(?:\\s+[^{}]*)?\\}$");
    private static final Pattern ACTIVITY = Pattern.compile(
            "ActivityRecord\\{([^\\s{}]+)\\s+u([0-9]+)\\s+([^\\s/{}]+)/([^\\s{}]+)(?:\\s+[^{}]*)?\\}");
    private static final class AppProof {
        final String identity, user, owner, component;
        AppProof(Matcher match) {
            identity = match.group(1); user = match.group(2); owner = match.group(3); component = match.group(4);
        }
        boolean sameActivity(AppProof other) {
            return other != null && identity.equals(other.identity) && user.equals(other.user)
                    && owner.equals(other.owner) && component.equals(other.component);
        }
    }
    final State state;
    final String windowOwner, reason;
    final int displayId;

    private ForegroundFocus(State state, String owner, int display, String reason) {
        this.state = state; this.windowOwner = owner; this.displayId = display; this.reason = reason;
    }
    static ForegroundFocus unavailable(String reason) { return new ForegroundFocus(State.UNAVAILABLE, null, -1, reason); }
    private static ForegroundFocus ambiguous(String reason) { return new ForegroundFocus(State.AMBIGUOUS, null, -1, reason); }
    boolean isGame() { return state == State.GAME; }
    boolean notificationShadeAllowed() { return isGame() && reason.equals("notification_shade_over_game"); }
    boolean initialAttachAllowed() { return isGame() && !notificationShadeAllowed(); }
    boolean known() { return state == State.GAME || state == State.OTHER || state == State.NO_WINDOW; }
    Boolean focusedValue() { return known() ? isGame() : null; }

    static ForegroundFocus parse(String output) {
        if (output == null || output.trim().isEmpty()) return unavailable("empty_response");
        List<ForegroundFocus> entries = new ArrayList<>();
        Set<Integer> displays = new HashSet<>(), entryDisplays = new HashSet<>();
        Set<Integer> modernDisplays = new HashSet<>(), legacyDisplays = new HashSet<>();
        Map<Integer, AppProof> focusedApps = new HashMap<>();
        Set<Integer> appDisplays = new HashSet<>(), conflictingApps = new HashSet<>();
        int current = -1, top = -1;
        boolean topSeen = false, historical = false;
        for (String raw : output.split("\\R")) {
            String line = raw.trim();
            if (line.startsWith("WINDOW MANAGER ")) {
                historical = line.equals("WINDOW MANAGER LAST ANR (dumpsys window lastanr)");
                continue;
            }
            if (historical || HISTORICAL_DISPLAY.matcher(line).matches()) continue;
            Matcher joined = JOINED_DISPLAY.matcher(line);
            if (joined.matches()) line = joined.group(1);
            Matcher display = DISPLAY.matcher(line), legacy = LEGACY_DISPLAY.matcher(line);
            if (display.matches() || legacy.matches()) {
                boolean modern = display.matches();
                current = number(modern ? display.group(1) : legacy.group(1));
                if (current < 0 || !(modern ? modernDisplays : legacyDisplays).add(current))
                    return ambiguous("invalid_or_duplicate_display");
                displays.add(current);
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
            if (line.startsWith("mFocusedApp=")) {
                boolean repeated = !appDisplays.add(current);
                Matcher app = ACTIVITY.matcher(line.substring("mFocusedApp=".length()));
                if (app.find()) {
                    AppProof proof = new AppProof(app);
                    if (app.find() || repeated && !proof.sameActivity(focusedApps.get(current))) conflictingApps.add(current);
                    else focusedApps.put(current, proof);
                } else if (repeated) conflictingApps.add(current);
                continue;
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
            boolean shade = owner.equals("StatusBar") || owner.equals("NotificationShade")
                    || owner.equals("com.android.systemui") && value.matches(
                    "Window\\{[^\\s{}]+\\s+u[0-9]+\\s+com\\.android\\.systemui/NotificationShade\\}");
            entries.add(new ForegroundFocus(EnginePolicy.GAME.equals(owner) ? State.GAME : shade ? State.NOTIFICATION_SHADE : State.OTHER,
                    owner, current, "current_window"));
        }
        if (entries.isEmpty()) return unavailable("current_focus_field_absent");
        if (topSeen && !displays.isEmpty()) {
            for (ForegroundFocus entry : entries) if (entry.displayId == top) return resolveShade(entry, focusedApps, conflictingApps);
            return unavailable("top_display_focus_absent");
        }
        if (entries.size() != 1 || displays.size() > 1) return ambiguous("top_focused_display_unknown");
        return resolveShade(entries.get(0), focusedApps, conflictingApps);
    }
    private static ForegroundFocus resolveShade(ForegroundFocus entry, Map<Integer, AppProof> apps, Set<Integer> conflicts) {
        if (entry.state != State.NOTIFICATION_SHADE) return entry;
        if (conflicts.contains(entry.displayId)) return ambiguous("conflicting_focused_app");
        AppProof proof = apps.get(entry.displayId);
        if (proof == null) return new ForegroundFocus(State.NOTIFICATION_SHADE,
                entry.windowOwner, entry.displayId, "notification_shade_app_unavailable");
        String underlying = proof.owner;
        return new ForegroundFocus(EnginePolicy.GAME.equals(underlying) ? State.GAME : State.OTHER,
                entry.windowOwner, entry.displayId, EnginePolicy.GAME.equals(underlying)
                ? "notification_shade_over_game" : "notification_shade_over_other");
    }
    private static int number(String value) {
        try { return Integer.parseInt(value); } catch (NumberFormatException error) { return -1; }
    }
}
