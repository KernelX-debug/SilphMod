package dev.rootcatch.nativeengine;

public enum ConnectionStrategy {
    ATTACH("librootcatch-inject.so", "17.22.1",
            "fde5dd3d72cdc062e31c9f262038d9bfb637b6ac9665131341cfddd1fb7f05c0"),
    EARLY_SPAWN("libsilph-spawn.so", "17.23.3",
            "bc4e1a0c7ede20af387e9e002a535cb3ff0072f523c460692820473b3f07e122");

    public final String library, version, sha256;
    public static final String LOADER_LIBRARY = "libsilph-loader.so";
    public static final String LOADER_SHA256 = "0215d7f3f5ded7104932ec9e28653d40f50576b1dc88ed6b9eef91bce5f0e7d8";

    ConnectionStrategy(String library, String version, String sha256) {
        this.library = library; this.version = version; this.sha256 = sha256;
    }

    public static ConnectionStrategy forSdk(int sdk) {
        if (sdk < 29) throw new IllegalArgumentException("Android 10 or newer required");
        return sdk == 29 ? ATTACH : EARLY_SPAWN;
    }

    public boolean earlySpawn() { return this == EARLY_SPAWN; }

    public String spawnCommand(String binary, String guard, String agent, String directory, String json) {
        if (!earlySpawn()) throw new IllegalStateException("Spawn backend not selected");
        for (String path : new String[]{binary, guard, agent, directory}) {
            if (path == null || !path.startsWith("/data/") || path.contains("/../"))
                throw new IllegalArgumentException("Invalid runtime path");
        }
        return "silph_parent_ns=$(readlink /proc/self/ns/mnt) || exit 90; "
                + "exec unshare -m -- /system/bin/sh " + EnginePolicy.quote(guard)
                + " \"$silph_parent_ns\" " + EnginePolicy.quote(binary)
                + " " + EnginePolicy.quote(directory)
                + " " + EnginePolicy.quote(binary.substring(0, binary.lastIndexOf('/') + 1) + LOADER_LIBRARY)
                + " -f " + EnginePolicy.quote(EnginePolicy.GAME)
                + " -s " + EnginePolicy.quote(agent) + " -R qjs -i -P " + EnginePolicy.quote(json);
    }
}
