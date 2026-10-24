package dev.rootcatch.root;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.Closeable;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;
import java.util.function.Consumer;

/** Persistent root shell. Commands are created exclusively by this application. */
public final class RootShell implements Closeable {
    private static final long DEFAULT_TIMEOUT_MS = 15_000;
    private static final long ROOT_CHECK_TIMEOUT_MS = 12_000;
    private static final long POLL_INTERVAL_MS = 100;

    private final AtomicLong sequence = new AtomicLong();
    private final Object commandLock = new Object();
    private final String[] shellCommand;
    private volatile Session session;
    private volatile boolean closed;

    public RootShell() { this("su"); }

    RootShell(String... shellCommand) {
        if (shellCommand == null || shellCommand.length == 0) throw new IllegalArgumentException("Empty shell command");
        this.shellCommand = shellCommand.clone();
    }

    public Result execute(String command) throws IOException {
        return execute(command, DEFAULT_TIMEOUT_MS);
    }

    public Result execute(String command, long timeoutMs) throws IOException {
        return execute(command,timeoutMs,null);
    }

    public Result execute(String command, long timeoutMs, Consumer<String> observer) throws IOException {
        if (command == null || command.isEmpty()) {
            throw new IllegalArgumentException("Empty command");
        }
        if (timeoutMs <= 0) {
            throw new IllegalArgumentException("Timeout must be positive");
        }

        synchronized (commandLock) {
            if (closed) throw new IOException("Root shell is closed");
            Session active = ensureStarted();
            long deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(timeoutMs);
            String marker = "__ROOTCATCH_" + sequence.incrementAndGet() + "__";

            try {
                active.writer.write(command);
                active.writer.newLine();
                active.writer.write("printf '\\n" + marker + "%s\\n' \"$?\"");
                active.writer.newLine();
                active.writer.flush();

                StringBuilder output = new StringBuilder();
                while (true) {
                    if (closed) throw new IOException("Root shell is closed");
                    long remaining = deadline - System.nanoTime();
                    if (remaining <= 0) {
                        throw new IOException("Root command timed out (" + timeoutMs + " ms)");
                    }

                    LineEvent event;
                    try {
                        event = active.lines.poll(Math.min(remaining,
                                TimeUnit.MILLISECONDS.toNanos(POLL_INTERVAL_MS)),
                                TimeUnit.NANOSECONDS);
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                        throw new IOException("Root command interrupted", e);
                    }
                    if (event == null) continue;
                    if (event.endOfStream) {
                        String tail = output.length() > 2048 ? output.substring(output.length() - 2048) : output.toString();
                        String exit = active.process.isAlive() ? "pending" : Integer.toString(active.process.exitValue());
                        throw new IOException("The su process ended unexpectedly (exit " + exit + "): " + tail);
                    }
                    if (event.line.startsWith(marker)) {
                        if (closed) throw new IOException("Root shell is closed");
                        int code;
                        try {
                            code = Integer.parseInt(event.line.substring(marker.length()));
                        } catch (NumberFormatException e) {
                            code = -1;
                        }
                        return new Result(code, output.toString().trim());
                    }
                    if(output.length()+event.line.length()>131072)throw new IOException("Root response exceeded diagnostic limit");
                    output.append(event.line).append('\n');
                    if(observer!=null)observer.accept(event.line);
                }
            } catch (IOException e) {
                discard(active);
                throw e;
            } catch (RuntimeException e) {
                discard(active);
                throw new IOException("Root output observer failed",e);
            }
        }
    }

    public boolean isAvailable() {
        try {
            Result result = execute("id", ROOT_CHECK_TIMEOUT_MS);
            return result.code == 0 && result.output.contains("uid=0");
        } catch (IOException e) {
            return false;
        }
    }

    private Session ensureStarted() throws IOException {
        if (closed) throw new IOException("Root shell is closed");
        Session current = session;
        if (current != null && current.process.isAlive()) return current;
        if (current != null) discard(current);

        Process process = new ProcessBuilder(shellCommand).redirectErrorStream(true).start();
        if (closed) {
            process.destroyForcibly();
            throw new IOException("Root shell is closed");
        }
        Session started = new Session(process);
        session = started;
        // close() may have run after the first check but before session was assigned.
        if (closed) {
            discard(started);
            throw new IOException("Root shell is closed");
        }
        return started;
    }

    private void discard(Session active) {
        if (session == active) session = null;
        active.process.destroyForcibly();
    }

    @Override
    public void close() {
        // Never wait for commandLock: execute() may be waiting for su or screencap.
        closed = true;
        Session active = session;
        if (active != null) active.process.destroyForcibly();
    }

    private static final class Session {
        final Process process;
        final BufferedWriter writer;
        final BlockingQueue<LineEvent> lines = new LinkedBlockingQueue<>(4096);

        Session(Process process) {
            this.process = process;
            writer = new BufferedWriter(new OutputStreamWriter(process.getOutputStream(),
                    StandardCharsets.UTF_8));
            Thread reader = new Thread(() -> readOutput(process), "rootcatch-shell-reader");
            reader.setDaemon(true);
            reader.start();
        }

        private void readOutput(Process process) {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                    process.getInputStream(), StandardCharsets.UTF_8))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    if(line.length()>32768||!lines.offer(new LineEvent(line, false))){process.destroyForcibly();break;}
                }
            } catch (IOException ignored) {
                // The command thread observes the end-of-stream event below.
            } finally {
                lines.offer(new LineEvent(null, true));
            }
        }
    }

    private static final class LineEvent {
        final String line;
        final boolean endOfStream;

        LineEvent(String line, boolean endOfStream) {
            this.line = line;
            this.endOfStream = endOfStream;
        }
    }

    public static final class Result {
        public final int code;
        public final String output;

        Result(int code, String output) {
            this.code = code;
            this.output = output;
        }
    }
}
