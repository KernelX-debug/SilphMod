package dev.rootcatch.ugphone;

/** A read-only ptrace concurrency guard, not a crash fix or a protection bypass. */
final class AttachGuard {
    private AttachGuard() { }
    static int tracerPid(String status) {
        Integer tracer = null;
        for (String line : status.split("\\R")) {
            if (!line.startsWith("TracerPid:")) continue;
            String value = line.substring("TracerPid:".length()).trim();
            if (tracer != null || !value.matches("[0-9]{1,10}"))
                throw new IllegalArgumentException("Invalid TracerPid state");
            try { tracer = Integer.valueOf(value); }
            catch (NumberFormatException error) { throw new IllegalArgumentException("TracerPid out of range"); }
        }
        if (tracer == null) throw new IllegalArgumentException("Could not read TracerPid");
        return tracer;
    }
}
