package dev.rootcatch.nativeengine;

import java.util.Locale;

public enum ConnectionFailure {
    NONE, SPAWN_SIGNAL, AGENT_SYNC, BROKEN_CHANNEL, PROCESS_EXIT, NATIVE_CRASH, POLICY, VALIDATION, UNKNOWN;
    public static ConnectionFailure classify(String text) {
        String value = text == null ? "" : text.replaceAll("\\u001B\\[[0-?]*[ -/]*[@-~]", "").trim().toLowerCase(Locale.ROOT);
        if (value.contains("silph_guard_error=") || value.contains("agent loader failed:")
                || value.contains("permission denied") || value.contains("operation not permitted")
                || value.contains("avc: denied")) return POLICY;
        if (value.contains("frida_sigchain_compat_validate_signal")) return SPAWN_SIGNAL;
        if (value.contains("timed out trying to sync up with agent")
                || value.contains("timed out waiting for the injected agent to connect back")) return AGENT_SYNC;
        if (value.contains("error sending message: broken pipe") || value.contains("control transport failed")
                || value.equals("agent connection closed unexpectedly") || value.equals("the connection is closed")) return BROKEN_CHANNEL;
        if (value.startsWith("process crashed:")) return NATIVE_CRASH;
        if (value.contains("bound_process_exited") || value.contains("game ended during startup")) return PROCESS_EXIT;
        return UNKNOWN;
    }
    public boolean retryable() {
        return this == SPAWN_SIGNAL || this == AGENT_SYNC || this == BROKEN_CHANNEL
                || this == PROCESS_EXIT || this == NATIVE_CRASH;
    }
    public ConnectionStrategy alternative(ConnectionStrategy route) {
        if (!retryable()) return null;
        if (route == ConnectionStrategy.EARLY_SPAWN || route == ConnectionStrategy.ATTACH)
            return ConnectionStrategy.DIRECT_ATTACH;
        if (route == ConnectionStrategy.DIRECT_ATTACH) return ConnectionStrategy.COMPAT_ATTACH;
        return route == ConnectionStrategy.COMPAT_ATTACH ? route : null;
    }
}
