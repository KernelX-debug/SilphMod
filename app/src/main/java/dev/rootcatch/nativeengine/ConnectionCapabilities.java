package dev.rootcatch.nativeengine;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.LinkedHashMap;
import java.util.Map;

public final class ConnectionCapabilities {
    public static final String COMMAND = "printf 'SILPH_UID='; id -u; printf 'SILPH_CONTEXT='; id -Z; "
            + "printf 'SILPH_SELINUX='; getenforce; printf 'SILPH_KERNEL='; uname -r; "
            + "printf 'SILPH_POLICY='; sha256sum /sys/fs/selinux/policy 2>/dev/null | cut -d ' ' -f 1; printf '\\n'";
    public final int sdk;
    public final long pageSize;
    public final String context, selinux, kernel, policyIdentity;
    public final boolean isolated;
    public ConnectionCapabilities(int sdk, long pageSize, int code, String environment, int guardExit, String guardOutput) {
        if (sdk < 29 || pageSize != 4096 && pageSize != 16384)
            throw new IllegalArgumentException("Unsupported Android or memory page size");
        Map<String, String> fields = new LinkedHashMap<>();
        if (environment != null) for (String line : environment.split("\\R")) {
            int equal = line.indexOf('=');
            if (equal < 0 || !line.startsWith("SILPH_")) continue;
            String key = line.substring(0, equal), value = line.substring(equal + 1).trim();
            if (fields.putIfAbsent(key, value) != null) throw new IllegalArgumentException("Ambiguous root environment");
        }
        if (code != 0 || !"0".equals(fields.get("SILPH_UID")))
            throw new IllegalArgumentException("Root environment could not be verified");
        String rawContext = fields.getOrDefault("SILPH_CONTEXT", "");
        context = rawContext.matches("[A-Za-z0-9_:,.-]{1,256}") ? rawContext : "unavailable";
        selinux = fields.getOrDefault("SILPH_SELINUX", "");
        kernel = fields.getOrDefault("SILPH_KERNEL", "");
        String policy = fields.getOrDefault("SILPH_POLICY", "");
        policyIdentity = policy.matches("[a-f0-9]{64}") ? policy : "unavailable";
        if (kernel.isEmpty() || kernel.length() > 256
                || !(selinux.equals("Enforcing") || selinux.equals("Permissive") || selinux.equals("Disabled")))
            throw new IllegalArgumentException("Incomplete root environment; export logs");
        this.sdk = sdk; this.pageSize = pageSize;
        isolated = guardExit == 0 && "SILPH_GUARD_READY=isolated_mount_namespace".equals(guardOutput == null ? "" : guardOutput.trim());
    }
    public boolean allows(ConnectionStrategy route) {
        return route != null && (route == ConnectionStrategy.ATTACH
                ? sdk == 29 && selinux.equals("Disabled") : isolated);
    }
    public ConnectionStrategy preferred() {
        ConnectionStrategy route = sdk == 29 ? !selinux.equals("Disabled")
                ? ConnectionStrategy.DIRECT_ATTACH : ConnectionStrategy.ATTACH : ConnectionStrategy.EARLY_SPAWN;
        if (!allows(route)) throw new IllegalArgumentException("Injector isolation unavailable; export logs");
        return route;
    }
    public String fingerprint(String system, String game, boolean location) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            for (String part : new String[]{"silph-compat-v1", system, game, Boolean.toString(location), Integer.toString(sdk),
                    Long.toString(pageSize), context, selinux, kernel, policyIdentity, Boolean.toString(isolated),
                    ConnectionStrategy.ATTACH.sha256, ConnectionStrategy.EARLY_SPAWN.sha256, ConnectionStrategy.LOADER_SHA256}) {
                if (part == null || part.length() > 4096) throw new IllegalArgumentException("Invalid environment identity");
                digest.update((part.length() + ":" + part).getBytes(StandardCharsets.UTF_8));
            }
            StringBuilder hash = new StringBuilder();
            for (byte value : digest.digest()) hash.append(String.format(java.util.Locale.ROOT, "%02x", value & 255));
            return hash.toString();
        } catch (java.security.NoSuchAlgorithmException error) { throw new IllegalStateException(error); }
    }
}
