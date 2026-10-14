package dev.rootcatch.nativeengine;

public final class LoaderState {
    public static final String ADAPTER = "readonly-apk-v1";
    private boolean ready, adapted;
    private String stage = "not_started";
    private int error, originalError;

    public boolean observe(String adapter, String observedStage, int observedError, int original, int pid, int helperPid) {
        if (!ADAPTER.equals(adapter) || pid <= 1 || pid != helperPid || observedError < 0 || original < 0 || error != 0) return false;
        if ("ready".equals(observedStage)) {
            if (ready || observedError != 0) return false;
            ready = true;
        } else if ("resolver_failed".equals(observedStage)) {
            if (ready || observedError == 0) return false;
        } else {
            if (!ready) return false;
            switch (observedStage) {
                case "original_memfd":
                    if (observedError != 0 || original != 0) return false;
                    break;
                case "readonly_apk_memfd":
                    if (observedError != 0 || original == 0) return false;
                    adapted = true;
                    break;
                case "read_context": case "read_flags": case "label_readonly":
                case "open_readonly": case "verify_context": case "replace_readonly":
                    if (observedError == 0 || original == 0) return false;
                    break;
                default: return false;
            }
        }
        stage = observedStage; error = observedError; originalError = original;
        return true;
    }
    public boolean ready() { return ready && error == 0; }
    public boolean adapted() { return adapted; }
    public String stage() { return stage; }
    public int error() { return error; }
    public int originalError() { return originalError; }
    public String failure() { return error == 0 ? "" : "Agent loader failed: " + stage + " (errno " + error + ")"; }
    public static String connectionEnded(boolean attached, boolean crash, boolean adapterRequired, boolean adapterReady) {
        if (crash) return "The game reported a native crash; wait for logs to finish, then export the ZIP";
        if (!attached && adapterRequired && !adapterReady) return "Injector loader did not initialize; export logs";
        return attached ? "Injector exited before session completion" : "Injector connection failed before engine startup; export logs";
    }
}
