package dev.rootcatch.nativeengine;

public enum ConnectionStrategy {
    ATTACH("librootcatch-inject.so", "17.22.1",
            "fde5dd3d72cdc062e31c9f262038d9bfb637b6ac9665131341cfddd1fb7f05c0"),
    EARLY_SPAWN("libsilph-spawn.so", "17.23.3",
            "bc4e1a0c7ede20af387e9e002a535cb3ff0072f523c460692820473b3f07e122"),
    DIRECT_ATTACH("libsilph-spawn.so", "17.23.3",
            "bc4e1a0c7ede20af387e9e002a535cb3ff0072f523c460692820473b3f07e122"),
    COMPAT_ATTACH("librootcatch-inject.so", "17.22.1",
            "fde5dd3d72cdc062e31c9f262038d9bfb637b6ac9665131341cfddd1fb7f05c0");

    public final String library, version, sha256;
    public static final String LOADER_LIBRARY = "libsilph-loader.so";
    public static final String LOADER_SHA256 = "45e5b68538d166afbf28157c5a52ca44a5b896bac105c19c8e189553fd475703";

    ConnectionStrategy(String library, String version, String sha256) {
        this.library = library; this.version = version; this.sha256 = sha256;
    }

    public static ConnectionStrategy forSdk(int sdk) {
        if (sdk < 29) throw new IllegalArgumentException("Android 10 or newer required");
        return sdk == 29 ? ATTACH : EARLY_SPAWN;
    }

    public boolean earlySpawn() { return this == EARLY_SPAWN; }
    public boolean guarded() { return this != ATTACH; }

    public String spawnCommand(String binary, String guard, String agent, String directory, String json) {
        if (!earlySpawn()) throw new IllegalStateException("Spawn backend not selected");
        return guardedCommand(binary, guard, agent, directory, json, "-f " + EnginePolicy.quote(EnginePolicy.GAME));
    }
    public String directCommand(String binary, String guard, String agent, String directory, int pid, String json) {
        if (earlySpawn() || !guarded() || pid <= 1) throw new IllegalArgumentException("Invalid direct connection");
        return guardedCommand(binary, guard, agent, directory, json, "-p " + pid);
    }
    public static String probeCommand(String guard) {
        runtimePath(guard);
        return "( silph_parent_ns=$(readlink /proc/self/ns/mnt) || exit 90; "
                + "exec unshare -m -- /system/bin/sh " + EnginePolicy.quote(guard)
                + " --probe \"$silph_parent_ns\" )";
    }
    private static void runtimePath(String path) {
        if (path == null || !path.startsWith("/data/") || path.contains("/../") || path.endsWith("/.."))
            throw new IllegalArgumentException("Invalid runtime path");
        EnginePolicy.quote(path);
    }
    private String guardedCommand(String binary, String guard, String agent, String directory, String json, String target) {
        for (String path : new String[]{binary, guard, agent, directory}) {
            runtimePath(path);
        }
        return "silph_parent_ns=$(readlink /proc/self/ns/mnt) || exit 90; "
                + "exec unshare -m -- /system/bin/sh " + EnginePolicy.quote(guard)
                + " \"$silph_parent_ns\" " + EnginePolicy.quote(binary)
                + " " + EnginePolicy.quote(directory)
                + " " + EnginePolicy.quote(binary.substring(0, binary.lastIndexOf('/') + 1) + LOADER_LIBRARY)
                + " " + target
                + " -s " + EnginePolicy.quote(agent) + " -R qjs -i -P " + EnginePolicy.quote(json);
    }
}
