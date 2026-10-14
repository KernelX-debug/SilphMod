package dev.rootcatch.nativeengine;

import java.util.Objects;

final class FocusLogPolicy {
    static final long QUERY_INTERVAL_MS = 800, SNAPSHOT_MS = 15000;
    private FocusReader.Sample previous;
    private long lastLoggedAt;

    boolean record(FocusReader.Sample sample, long now, boolean active) {
        FocusReader.Sample old = previous;
        boolean changed = old == null || !sample.stage.equals(old.stage) || !sample.source.equals(old.source)
                || !sample.response.equals(old.response) || !Objects.equals(sample.error, old.error)
                || sample.shellExitCode != old.shellExitCode || sample.sourceExitCode != old.sourceExitCode
                || sample.truncated != old.truncated || sample.focus.state != old.focus.state
                || !sample.focus.reason.equals(old.focus.reason);
        previous = sample;
        boolean abnormal = sample.error != null || sample.shellExitCode != 0 || sample.sourceExitCode != 0
                || sample.truncated || !sample.focus.known() || sample.durationMs >= 1000;
        if (!active || changed || abnormal || now < lastLoggedAt || now - lastLoggedAt >= SNAPSHOT_MS) {
            lastLoggedAt = now;
            return true;
        }
        return false;
    }
}
