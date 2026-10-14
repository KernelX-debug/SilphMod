package dev.rootcatch.ugphone;

import org.json.JSONObject;
import dev.rootcatch.nativeengine.RollingLog;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

/** Bounded Android sources, independent of game/injector lifecycle and game PID filters. */
final class CrashCollector {
    private static final int MIB = 1024 * 1024;
    private final File report;
    private volatile int pid;
    private volatile long ticks;
    private final RollingLog manifest;
    private final boolean continuous;
    private volatile DiagnosticProcess live;
    private volatile boolean liveStopping;
    private Thread liveThread;
    private volatile long finalDeadline;
    private boolean finishing;
    private boolean fullReport = true, restarting;
    private java.util.function.Consumer<String> progress = message -> { };
    CrashCollector(File report, int pid, long ticks, boolean continuous) throws Exception {
        this.report = report; this.pid = pid; this.ticks = ticks;
        this.continuous = continuous;
        manifest = new RollingLog(report, "sources.jsonl", MIB, 2);
    }
    void bindTarget(int target, long startTicks) {
        pid = target; ticks = startTicks;
        note("collector_target", new JSONObjectSafe().put("pid", pid).put("startTicks", ticks));
    }
    void start(boolean early) {
        note("collector", new JSONObjectSafe().put("rawLogs", true).put("pid", pid).put("startTicks", ticks)
                .put("liveBytesLimit", 16 * MIB).put("sourceBytesLimit", 2 * MIB)
                .put("continuousLiveRotation", continuous).put("liveSegmentSeconds", 240).put("liveArchives", 2)
                .put("nativeReportsRequireRootAccess", true).put("allSystemBuffersMayIncludeOtherApps", true)
                .put("liveScope", "SYSTEM_WARNINGS_AND_GAME_CRASH_INFO").put("verboseSystemStream", false));
        liveThread = new Thread(this::liveLoop,
                "ugphone-logcat-live");
        liveThread.setDaemon(true); liveThread.start();
        if (early) {
            note("baseline_deferred", new JSONObjectSafe().put("reason", "startup connection")
                    .put("liveRecordingStartedBeforeLaunch", true));
            return;
        }
        capture("tombstone-inventory-before.log", DiagnosticCommands.tombstoneInventory(), new DiagnosticProcess(), 4, MIB);
        tombstones("tombstone-inventory-before.log", "historical", 2);
        // Small initial sources avoid delaying attachment with all dumpsys sources.
        capture("before-logcat.log", DiagnosticCommands.snapshots(pid, ticks).get("logcat"), new DiagnosticProcess(), 4, 2 * MIB);
        capture("before-game-process.log", DiagnosticCommands.process(pid, ticks, true), new DiagnosticProcess(), 4, 2 * MIB);
        capture("before-process-list.log", DiagnosticCommands.snapshots(pid, ticks).get("process-list"), new DiagnosticProcess(), 4, 2 * MIB);
        capture("before-tools.log", DiagnosticCommands.snapshots(pid, ticks).get("tools"), new DiagnosticProcess(), 4, 2 * MIB);
    }
    void finish() { finish(true, false, message -> { }); }
    void finish(boolean full, boolean restart, java.util.function.Consumer<String> onProgress) {
        synchronized (this) { if (finishing) return; finishing = true; }
        fullReport = full; restarting = restart; progress = onProgress;
        int seconds = restarting ? 3 : fullReport ? 50 : 12;
        finalDeadline = System.nanoTime() + java.util.concurrent.TimeUnit.SECONDS.toNanos(seconds);
        note("finalization_profile", new JSONObjectSafe().put("fullReport", fullReport).put("controlledRestart", restarting)
                .put("sourceBudgetSeconds", seconds));
        if (!fullReport) stopLive();
        CollectorFinalization.run(this::gatherFinalSources, this::stopLive, () -> {
            note("collector_end", new JSONObjectSafe().put("liveWorkerFinished", liveThread == null || !liveThread.isAlive())
                    .put("targetBound", pid > 1 && ticks > 0).put("pid", pid).put("startTicks", ticks)
                    .put("gameRequestsSentByCollector", false).put("systemLoggingConfigurationChanged", false));
        }, () -> { synchronized (this) { try { manifest.close(); } catch (Exception ignored) { } } });
    }
    private void gatherFinalSources() {
        if (restarting) {
            note("source_omitted", new JSONObjectSafe().put("reason", "controlled restart; engine and lifecycle logs retained"));
            return;
        }
        if (fullReport) try { Thread.sleep(2500); } catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
        if (pid <= 1 || ticks <= 0)
            note("source_omitted", new JSONObjectSafe().put("file", "after-game-process.log")
                    .put("reason", "target not bound; collecting system sources without a fabricated PID"));
        Map<String, String> sources = DiagnosticCommands.finalSources(pid, ticks, fullReport);
        int completed = 0;
        for (Map.Entry<String, String> source : sources.entrySet()) {
            progress.accept("Saving logs… " + (++completed) + "/" + (sources.size() + 1));
            capture("after-" + source.getKey() + ".log", source.getValue(), new DiagnosticProcess(), 5, 2 * MIB);
        }
        progress.accept("Saving logs… " + (sources.size() + 1) + "/" + (sources.size() + 1));
        capture("tombstone-inventory-after.log", DiagnosticCommands.tombstoneInventory(), new DiagnosticProcess(), 4, MIB);
        tombstones("tombstone-inventory-after.log", "current", 4);
    }
    private void stopLive() {
        liveStopping = true;
        DiagnosticProcess current = live;
        Thread worker = liveThread;
        try { if (current != null) current.close(); }
        finally {
            if (worker != null) {
                worker.interrupt();
                try { worker.join(2500); } catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
            }
        }
    }
    private void liveLoop() {
        long segment = 0;
        do {
            try {
                if (segment > 0) RollingLog.rotate(report, "logcat-live.log", 2);
                DiagnosticProcess current = new DiagnosticProcess(); live = current;
                if (liveStopping) { current.close(); break; }
                note("live_segment", new JSONObjectSafe().put("sequence", ++segment)
                        .put("file", "logcat-live.log").put("olderSegmentsMayBeOverwritten", true));
                capture("logcat-live.log", DiagnosticCommands.farmerLiveLogcat(), current, 240, 16 * MIB);
                if (!liveStopping && continuous) Thread.sleep(1000);
            } catch (Exception error) {
                note("live_rotation_error", new JSONObjectSafe().put("error", error.toString())); break;
            }
        } while (continuous && !liveStopping);
    }
    private void capture(String name, String command, DiagnosticProcess process, int seconds, int budget) {
        if (finalDeadline > 0) {
            long remaining = java.util.concurrent.TimeUnit.NANOSECONDS.toSeconds(finalDeadline - System.nanoTime());
            if (remaining < 3) {
                note("source_omitted", new JSONObjectSafe().put("file", name).put("reason", "post-stop total time budget")); return;
            }
            seconds = (int) Math.min(seconds, remaining - 2);
        }
        note("source_start", new JSONObjectSafe().put("file", name).put("command", command)
                .put("timeoutSeconds", seconds).put("maxBytes", budget));
        try {
            DiagnosticProcess.Result result = process.run(command, new File(report, name), seconds, budget);
            note("source_end", new JSONObjectSafe().put("file", name).put("exitCode", result.exitCode)
                    .put("bytes", result.bytes).put("observedBytes", result.observedBytes).put("truncated", result.truncated)
                    .put("timedOut", result.timedOut).put("cancelled", result.cancelled).put("error", result.error == null ? JSONObject.NULL : result.error)
                    .put("durationMs", result.durationMs).put("helperPid", result.ownerPid).put("helperStartTicks", result.ownerTicks));
        } catch (Exception exception) { note("source_error", new JSONObjectSafe().put("file", name).put("error", exception.toString())); }
    }
    private void tombstones(String inventory, String prefix, int limit) {
        try {
            List<String[]> candidates = new ArrayList<>();
            for (String line : Files.readAllLines(new File(report, inventory).toPath(), StandardCharsets.UTF_8)) {
                String[] fields = line.trim().split("\\s+");
                if (fields.length == 4 && fields[0].matches("tombstone_[0-9]{2,4}")
                        && fields[1].matches("[0-9]+") && fields[2].matches("[0-9]+") && fields[3].matches("[0-9]+")) candidates.add(fields);
            }
            candidates.sort(Comparator.comparingLong((String[] row) -> Long.parseLong(row[3])).reversed());
            for (int i = 0; i < Math.min(limit, candidates.size()); i++) {
                String[] row = candidates.get(i);
                note("tombstone_selected", new JSONObjectSafe().put("name", row[0]).put("inode", row[1]).put("sourceBytes", row[2])
                        .put("modifiedUnixSeconds", row[3]).put("phase", prefix)
                        .put("historicalOrCurrentDoesNotProveCrashDuringTest", true));
                capture(prefix + "-" + row[0] + ".log", DiagnosticCommands.tombstone(row[0]), new DiagnosticProcess(), 4, 2 * MIB);
            }
            note("tombstone_inventory", new JSONObjectSafe().put("phase", prefix).put("matching", candidates.size())
                    .put("selected", Math.min(limit, candidates.size())).put("selectionLimit", limit));
        } catch (Exception exception) { note("tombstone_error", new JSONObjectSafe().put("phase", prefix).put("error", exception.toString())); }
    }
    private synchronized void note(String event, JSONObject detail) {
        try { detail.put("event", event).put("time", System.currentTimeMillis());
            manifest.write(detail.toString()); } catch (Exception ignored) { }
    }
    private static final class JSONObjectSafe extends JSONObject {
        @Override public JSONObjectSafe put(String key, Object value) { try { super.put(key, value); } catch (Exception ignored) { } return this; }
        @Override public JSONObjectSafe put(String key, boolean value) { try { super.put(key, value); } catch (Exception ignored) { } return this; }
        @Override public JSONObjectSafe put(String key, int value) { try { super.put(key, value); } catch (Exception ignored) { } return this; }
        @Override public JSONObjectSafe put(String key, long value) { try { super.put(key, value); } catch (Exception ignored) { } return this; }
    }
}
