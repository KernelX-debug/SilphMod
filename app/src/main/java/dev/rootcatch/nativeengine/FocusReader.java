package dev.rootcatch.nativeengine;

import java.nio.charset.StandardCharsets;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class FocusReader {
    static final int MAX_BYTES = 32768;
    static final long TIMEOUT_MS = 3500;
    static final String EXIT_MARKER = "RC_FOCUS_SOURCE_EXIT:";
    private static final Pattern EXIT = Pattern.compile("^RC_FOCUS_SOURCE_EXIT:([0-9]+)$", Pattern.MULTILINE);
    private static final Pattern ERROR = Pattern.compile(
            "\\b(?:permission denial|permission denied|access denied|[a-z0-9_.$]*exception\\b|error\\s*:|error dumping|dump timed out|timeout\\b|not found|unknown command|unrecognized command)",
            Pattern.CASE_INSENSITIVE);
    interface Shell { Reply read(String command, long timeoutMs) throws Exception; }
    static final class Reply {
        final int code; final String output;
        Reply(int code, String output) { this.code = code; this.output = output == null ? "" : output; }
    }
    static final class Reading {
        final ForegroundFocus focus; final String source;
        Reading(ForegroundFocus focus, String source) { this.focus = focus; this.source = source; }
    }
    static final class Sample {
        final String stage, source, command, response, error;
        final int shellExitCode, sourceExitCode;
        final boolean truncated;
        final long durationMs;
        final ForegroundFocus focus;
        Sample(String stage, String source, String command, String response, String error, int shellExit,
                int sourceExit, boolean truncated, long durationMs, ForegroundFocus focus) {
            this.stage = stage; this.source = source; this.command = command; this.response = response; this.error = error;
            this.shellExitCode = shellExit; this.sourceExitCode = sourceExit; this.truncated = truncated;
            this.durationMs = durationMs; this.focus = focus;
        }
    }
    private final Shell shell;
    private final BooleanSupplier stopping;
    private final Consumer<Sample> observer;
    private String preferred = "displays";
    FocusReader(Shell shell, BooleanSupplier stopping, Consumer<Sample> observer) {
        this.shell = shell; this.stopping = stopping; this.observer = observer;
    }
    Reading read(String stage) {
        if (stopping.getAsBoolean()) return new Reading(ForegroundFocus.unavailable("stopped"), "none");
        Sample first = query(preferred, stage);
        if (first.focus.known() || stopping.getAsBoolean()) return new Reading(first.focus, first.source);
        Sample fallback = query(preferred.equals("displays") ? "full" : "displays", stage);
        if (fallback.focus.known()) preferred = fallback.source;
        return new Reading(fallback.focus, fallback.source);
    }
    static String command(String source) {
        if (!source.equals("displays") && !source.equals("full")) throw new IllegalArgumentException("Invalid source");
        // Preserve dumpsys's exit status despite the filtering pipeline. Always drain the pipe.
        return "{ dumpsys -t 2 window" + (source.equals("displays") ? " displays" : "")
                + " 2>&1; rc_focus_exit=$?; printf '\\nRC_FOCUS_SOURCE_EXIT:%s\\n' \"$rc_focus_exit\"; }"
                + " | grep -E '^WINDOW MANAGER |(^|[[:space:]])(Display:|Display #[0-9]|mCurrentFocus=|mFocusedApp=|mTopFocusedDisplayId=)"
                + "|RC_FOCUS_SOURCE_EXIT:|[Pp]ermission|[Dd]enied|[Ee]rror|[Ee]xception|[Tt][Ii][Mm][Ee][Oo][Uu][Tt]"
                + "|[Tt]imed out|not found|[Uu]nknown command|[Uu]nrecognized command'"
                + " | { head -c " + (MAX_BYTES + 1) + "; cat >/dev/null; }";
    }
    private Sample query(String source, String stage) {
        String command = command(source), output = "", error = null;
        int code = -1, sourceCode = -1;
        boolean truncated = false;
        long began = System.nanoTime();
        ForegroundFocus focus;
        try {
            Reply reply = shell.read(command, TIMEOUT_MS);
            code = reply.code; output = reply.output;
            byte[] bytes = output.getBytes(StandardCharsets.UTF_8);
            truncated = bytes.length >= MAX_BYTES;
            if (truncated) {
                output = new String(bytes, 0, Math.min(MAX_BYTES, bytes.length), StandardCharsets.UTF_8);
                focus = ForegroundFocus.unavailable("response_truncated");
            } else {
                Matcher exit = EXIT.matcher(output);
                if (!exit.find()) focus = ForegroundFocus.unavailable("source_exit_marker_absent");
                else {
                    sourceCode = Integer.parseInt(exit.group(1));
                    int markerStart = exit.start(), markerEnd = exit.end();
                    if (exit.find() || !output.substring(markerEnd).trim().isEmpty())
                        focus = ForegroundFocus.unavailable("invalid_source_exit_marker");
                    else if (code != 0 || sourceCode != 0)
                        focus = ForegroundFocus.unavailable("command_failed");
                    else if (sourceReportedError(output.substring(0, markerStart)))
                        focus = ForegroundFocus.unavailable("source_reported_error");
                    else focus = ForegroundFocus.parse(output.substring(0, markerStart));
                }
            }
        } catch (Exception failed) {
            error = failed.toString(); focus = ForegroundFocus.unavailable("shell_exception");
        }
        Sample sample = new Sample(stage, source, command, output, error, code, sourceCode, truncated,
                (System.nanoTime() - began) / 1_000_000, focus);
        observer.accept(sample);
        return sample;
    }
    private static boolean sourceReportedError(String output) {
        for (String raw : output.split("\\R")) {
            String line = raw.trim();
            if (line.startsWith("mCurrentFocus=") || line.startsWith("mFocusedApp=")
                    || line.startsWith("mTopFocusedDisplayId=") || line.startsWith("Display: mDisplayId=")
                    || line.startsWith("Display #")) continue;
            if (ERROR.matcher(line).find()) return true;
        }
        return false;
    }
}
