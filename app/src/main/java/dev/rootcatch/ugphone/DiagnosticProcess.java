package dev.rootcatch.ugphone;

import dev.rootcatch.root.RootShell;
import java.io.ByteArrayOutputStream;
import java.io.Closeable;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Bounded byte capture; native child also has a timeout, including if this app dies. */
final class DiagnosticProcess implements Closeable {
    interface Launcher { Process start(String command) throws IOException; }
    interface Cleanup { void stop(int pid, long ticks); }
    private final Launcher launcher;
    private final Cleanup cleanup;
    private volatile Process active;
    private volatile Capture activeCapture;
    private volatile boolean cancelled;
    private volatile int ownerPid = -1;
    private volatile long ownerTicks;
    DiagnosticProcess() { this(command -> new ProcessBuilder("su", "-c", command).redirectErrorStream(true).start(),
            (pid, ticks) -> { try (RootShell shell = new RootShell()) { shell.execute(DiagnosticCommands.stopOwned(pid, ticks), 2000); }
                catch (Exception ignored) { } }); }
    DiagnosticProcess(Launcher launcher, Cleanup cleanup) { this.launcher = launcher; this.cleanup = cleanup; }
    Result run(String command, File file, int seconds, int maxBytes) throws IOException {
        if (cancelled) throw new IOException("Collector cancelled");
        if (maxBytes < 1) throw new IllegalArgumentException("Invalid limit");
        long began = System.nanoTime();
        Capture capture = new Capture(file, maxBytes);
        activeCapture = capture;
        Process local;
        try { local = launcher.start(DiagnosticCommands.bounded(command, seconds)); }
        catch (IOException failure) { capture.close(); activeCapture = null; throw failure; }
        active = local;
        if (cancelled) local.destroyForcibly();
        Thread reader = new Thread(() -> capture.read(local.getInputStream()), "ugphone-diagnostic-output");
        reader.setDaemon(true); reader.start();
        boolean timeout = false; int exit = -1;
        try {
            if (!local.waitFor(seconds * 1000L + 1000, TimeUnit.MILLISECONDS)) { timeout = true; close(); }
            if (!local.isAlive()) exit = local.exitValue();
            if (!capture.done.await(750, TimeUnit.MILLISECONDS)) {
                try { local.getInputStream().close(); } catch (IOException ignored) { }
                capture.done.await(250, TimeUnit.MILLISECONDS);
            }
        } catch (InterruptedException interrupted) {
            Thread.currentThread().interrupt(); close(); throw new IOException("Collector interrupted", interrupted);
        } finally {
            capture.close(); local.destroyForcibly(); active = null; activeCapture = null;
        }
        return new Result(exit, capture.bytes, capture.observed, capture.truncated, timeout,
                cancelled, capture.error, TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - began), ownerPid, ownerTicks);
    }
    @Override public void close() {
        cancelled = true;
        Capture capture = activeCapture;
        if (capture != null) try { capture.close(); } catch (IOException ignored) { }
        Process local = active;
        if (local == null) return;
        // Verify our timeout wrapper's identity; never kill the game or another app.
        if (ownerPid > 1 && ownerTicks > 0) cleanup.stop(ownerPid, ownerTicks);
        local.destroyForcibly();
    }
    private final class Capture implements Closeable {
        final CountDownLatch done = new CountDownLatch(1);
        final FileOutputStream output;
        final int limit;
        long bytes, observed;
        boolean truncated, closed;
        String error;
        Capture(File file, int limit) throws IOException { this.output = new FileOutputStream(file); this.limit = limit; }
        void read(InputStream input) {
            try {
                ByteArrayOutputStream header = new ByteArrayOutputStream(); int value;
                while (header.size() < 256 && (value = input.read()) != -1) {
                    header.write(value); if (value == '\n') break;
                }
                String line = header.toString(StandardCharsets.US_ASCII.name()).trim();
                if (line.matches("RC_DIAG_OWNER:[1-9][0-9]{0,9}:[1-9][0-9]{0,19}")) {
                    String[] parts = line.split(":"); ownerPid = Integer.parseInt(parts[1]); ownerTicks = Long.parseLong(parts[2]);
                } else write(header.toByteArray(), header.size());
                byte[] buffer = new byte[8192]; int count;
                while ((count = input.read(buffer)) != -1) write(buffer, count);
            } catch (Exception exception) { synchronized (this) { error = exception.toString(); } }
            finally { try { close(); input.close(); } catch (IOException ignored) { } done.countDown(); }
        }
        synchronized void write(byte[] buffer, int count) throws IOException {
            if (closed) return;
            observed += count; int keep = (int) Math.min(count, limit - bytes);
            output.write(buffer, 0, keep); bytes += keep;
            if (count > keep) truncated = true;
        }
        @Override public synchronized void close() throws IOException { if (!closed) { closed = true; output.close(); } }
    }
    static final class Result {
        final int exitCode, ownerPid;
        final long bytes, observedBytes, durationMs, ownerTicks;
        final boolean truncated, timedOut, cancelled;
        final String error;
        Result(int code, long bytes, long observed, boolean truncated, boolean timeout, boolean cancelled,
               String error, long duration, int pid, long ticks) {
            this.exitCode = code; this.bytes = bytes; this.observedBytes = observed; this.truncated = truncated;
            this.timedOut = timeout; this.cancelled = cancelled; this.error = error;
            this.durationMs = duration; this.ownerPid = pid; this.ownerTicks = ticks;
        }
    }
}
