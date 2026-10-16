package dev.rootcatch.ugphone;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.system.Os;
import android.system.OsConstants;
import dev.rootcatch.nativeengine.ConnectionCapabilities;
import dev.rootcatch.nativeengine.ConnectionFailure;
import dev.rootcatch.nativeengine.ConnectionStrategy;
import dev.rootcatch.nativeengine.ConnectionPlan;
import dev.rootcatch.root.RootShell;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.UUID;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;
import org.json.JSONObject;

final class CompatibilitySession {
    final String chain = UUID.randomUUID().toString();
    private ConnectionCapabilities capabilities;
    private ConnectionPlan plan;
    private String fingerprint = "";
    private SharedPreferences profiles;
    private boolean simulation, simulated;
    void simulateSpawnFailure(boolean enabled) { simulation = enabled && dev.rootcatch.BuildConfig.DEBUG; }
    boolean takeSimulatedFailure(ConnectionStrategy route) {
        if (!simulation || simulated || route != ConnectionStrategy.EARLY_SPAWN) return false;
        simulated = true; return true;
    }
    ConnectionStrategy prepare(Context context, RootShell root, boolean location, BooleanSupplier stopped,
                               Consumer<JSONObject> observer) throws Exception {
        if (plan != null) return plan.selected();
        long began = android.os.SystemClock.elapsedRealtime();
        RootShell.Result environment = root.execute(ConnectionCapabilities.COMMAND, 3000);
        if (stopped.getAsBoolean()) throw new InterruptedException("Startup stopped");
        File directory = new File(context.getFilesDir(), "engine");
        if (!directory.exists() && !directory.mkdirs()) throw new IllegalStateException("Could not prepare connection");
        File guard = new File(directory, "compatibility-guard.sh");
        try (InputStream input = context.getAssets().open("engine/spawn-guard.sh"); FileOutputStream output = new FileOutputStream(guard)) {
            byte[] buffer = new byte[4096]; int count;
            while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
        }
        String command = ConnectionStrategy.probeCommand(guard.getAbsolutePath());
        int guardExit = -1; String guardOutput;
        try { RootShell.Result probe = root.execute(command, 3000); guardExit = probe.code; guardOutput = probe.output; }
        catch (java.io.IOException error) { guardOutput = error.toString(); }
        if (stopped.getAsBoolean()) throw new InterruptedException("Startup stopped");
        long pageSize = Os.sysconf(OsConstants._SC_PAGESIZE);
        observer.accept(new JSONObject().put("event", "connection_capabilities").put("environment", environment.output)
                .put("rootGranted", true)
                .put("environmentExit", environment.code).put("guardCommand", command).put("guardExit", guardExit)
                .put("guardOutput", guardOutput).put("sdk", Build.VERSION.SDK_INT).put("pageSize", pageSize)
                .put("durationMs", android.os.SystemClock.elapsedRealtime() - began));
        capabilities = new ConnectionCapabilities(Build.VERSION.SDK_INT, pageSize, environment.code, environment.output, guardExit, guardOutput);
        fingerprint = capabilities.fingerprint(Build.FINGERPRINT + ":" + Build.VERSION.SECURITY_PATCH,
                dev.rootcatch.nativeengine.EnginePolicy.VERSION + ":" + dev.rootcatch.nativeengine.EnginePolicy.VERSION_CODE, location);
        profiles = context.getSharedPreferences("injector_compatibility", 0);
        String prefix = location ? "location_" : "plain_";
        String cachedRoute = fingerprint.equals(profiles.getString(prefix + "fingerprint", ""))
                ? profiles.getString(prefix + "route", "") : "";
        plan = new ConnectionPlan(capabilities, cachedRoute);
        observer.accept(new JSONObject().put("event", "connection_selected").put("route", plan.selected().name())
                .put("fingerprint", fingerprint).put("cached", plan.cached()).put("chain", chain));
        return plan.selected();
    }
    void validated(boolean location) {
        if (simulation || profiles == null || plan == null) return;
        String prefix = location ? "location_" : "plain_";
        profiles.edit().putString(prefix + "fingerprint", fingerprint).putString(prefix + "route", plan.selected().name()).apply();
    }
    ConnectionStrategy alternative(ConnectionFailure failure, boolean location) {
        if (plan == null || failure == null || !failure.retryable() && failure != ConnectionFailure.POLICY) return null;
        String prefix = location ? "location_" : "plain_";
        if (!simulation && profiles != null && fingerprint.equals(profiles.getString(prefix + "fingerprint", "")))
            profiles.edit().remove(prefix + "fingerprint").remove(prefix + "route").apply();
        return plan.next(failure);
    }
    void advance(ConnectionStrategy next) {
        if (plan == null) throw new IllegalStateException("Connection not prepared");
        plan.advance(next);
    }
}
