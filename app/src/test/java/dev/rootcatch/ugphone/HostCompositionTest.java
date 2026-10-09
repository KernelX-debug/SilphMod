package dev.rootcatch.ugphone;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.charset.StandardCharsets;
import org.junit.Test;
import static org.junit.Assert.*;

/** Source-level regression checks complement, but do not replace, Android execution. */
public final class HostCompositionTest {
    private String read(String path) throws Exception {
        return new String(Files.readAllBytes(Path.of(System.getProperty("user.dir"), path)), StandardCharsets.UTF_8);
    }
    @Test public void throwLimitIsNotEditableAndLegacyLimitCannotRestrictTheEngine() throws Exception {
        String ui = read("src/main/java/dev/rootcatch/ugphone/MainActivity.java");
        String config = read("src/main/java/dev/rootcatch/automation/AutomationConfig.java");
        assertFalse(ui.contains("Throws per encounter")); assertFalse(ui.contains("throwsCount"));
        assertTrue(ui.contains(".putInt(\"max_throws\", 0)"));
        assertFalse(config.contains("p.getInt(\"max_throws\""));
        assertTrue(config.contains("clamp(p.getInt(\"delay\", 150), 100, 3000),"));
        assertTrue(config.contains("SessionLimits.DEFAULT_SPINS),\n                0,"));
        assertTrue(ui.contains("read(delay, 100, 3000, \"Interval\")"));
    }
    @Test public void publishedUiOmitsSessionQuotaInputsAndMigratesOldPreferencesToZero() throws Exception {
        String ui = read("src/main/java/dev/rootcatch/ugphone/MainActivity.java");
        assertFalse(ui.contains("maxTransfers")); assertFalse(ui.contains("maxDiscard"));
        assertFalse(ui.contains("per session")); assertFalse(ui.contains("sin límite"));
        assertTrue(ui.contains(".putInt(\"max_transfers\", 0)"));
        assertTrue(ui.contains(".putInt(\"max_discarded_items\", 0)"));
        assertTrue(ui.contains("Keep shiny")); assertTrue(ui.contains("Keep 15/15/15"));
        assertTrue(ui.contains("Items to keep")); assertTrue(ui.contains("Engine settings"));
    }
    @Test public void productionConfigIgnoresLegacyCapsAndRetainsOnlyCurrentSessionScope() throws Exception {
        String config = read("src/main/java/dev/rootcatch/automation/AutomationConfig.java");
        String service = read("src/main/java/dev/rootcatch/ugphone/FarmService.java");
        String session = read("src/main/java/dev/rootcatch/ugphone/FarmSession.java");
        assertFalse(config.contains("p.getInt(\"max_discarded_items\""));
        assertTrue(config.matches("(?s).*p.getString\\(\"discard_limits\"[^\\n]+\\n\\s*0\\s*\\).*"));
        assertFalse(service.contains("getIntExtra(\"maxTransfers\""));
        assertTrue(service.contains("intent.getBooleanExtra(\"keepHundo\", true), 0)"));
        assertTrue(service.contains(".putString(\"status_language\", \"en\")"));
        assertTrue(session.contains("new JSONObject(values)"));
        assertTrue(session.contains("transfers, \"\", true)"));
    }
    @Test public void silphLaunchesOnlyFromVisibleActivityWithConsumedPreparedTicket() throws Exception {
        String ui = read("src/main/java/dev/rootcatch/ugphone/MainActivity.java");
        assertTrue(ui.contains("Start service")); assertTrue(ui.contains("Export logs"));
        assertTrue(ui.contains("moveTaskToBack(true)"));
        assertTrue(ui.contains("getLaunchIntentForPackage(EnginePolicy.GAME)"));
        assertTrue(ui.contains("if (!visible || !FarmService.isRunning() || FarmService.isClosing()) return"));
        assertTrue(ui.contains("FarmService.consumeLaunchTicket()"));
        assertTrue(ui.contains("if (!resolveError12.isChecked()) moveTaskToBack(true)"));
        assertFalse(ui.contains("Solo lectura")); assertFalse(ui.contains("Iniciar motor"));
        assertTrue(ui.contains("startActivity(launch)"));
    }
    @Test public void generatedSessionUsesTheSameSingleAttachAutomaticGateAsRoot() throws Exception {
        String session = read("src/main/java/dev/rootcatch/ugphone/FarmSession.java");
        assertTrue(session.contains("\"externalEnhancerRequired\", !resolveError12"));
        assertTrue(session.contains("runner.enableNativeLocation("));
        assertTrue(session.contains("runner.enableNativeLocation(resolveError12)"));
        assertTrue(session.indexOf("runner.prepareAssets(FarmPolicy.EXPECTED_AGENT_SHA)") < session.indexOf("record(\"early_armed\""));
        assertTrue(session.contains("startCheck.prepare(userApprovedGameStop)"));
        assertFalse(session.contains("existing.code != 1 || !existing.output.trim().isEmpty()"));
        assertTrue(session.contains("new JSONObject(values)"));
        assertTrue(session.contains("collector.start(true)"));
        assertTrue(session.contains("\"edition\", \"ugphone\""));
        assertTrue(session.contains("\"continuous\", true"));
        assertTrue(session.contains("runner.expectProcess(pid, startTicks)"));
        assertTrue(session.contains("transfers, \"\", true)"));
        assertFalse(session.contains("new TestSession")); assertFalse(session.contains("am start"));
    }
    @Test public void closingGameRequiresPerStartUiConsentAndDoesNotRunFromWarmPath() throws Exception {
        String ui = read("src/main/java/dev/rootcatch/ugphone/MainActivity.java");
        String service = read("src/main/java/dev/rootcatch/ugphone/FarmService.java");
        String session = read("src/main/java/dev/rootcatch/ugphone/FarmSession.java");
        assertFalse(ui.contains("Inicio limpio")); assertFalse(ui.contains("Cerrar juego e iniciar"));
        assertTrue(ui.contains("putExtra(\"userApprovedGameStop\", resolveError12.isChecked())"));
        assertTrue(ui.contains("putExtra(\"startupAuthorization\", \"USER_START_BUTTON\")"));
        assertTrue(service.contains("getBooleanExtra(\"userApprovedGameStop\", false)"));
        assertTrue(service.contains("\"USER_START_BUTTON\".equals(intent.getStringExtra(\"startupAuthorization\"))"));
        assertTrue(session.contains("this.userApprovedGameStop = resolveError12 && userApprovedGameStop"));
        assertTrue(session.indexOf("if (resolveError12) {") < session.indexOf("startCheck.prepare(userApprovedGameStop)"));
        assertFalse(ui.contains("putBoolean(\"userApprovedGameStop\""));
    }
    @Test public void controlWorkerDoesNotDependOnFocusQueriesOrLogFileWrites() throws Exception {
        String runner = read("src/main/java/dev/rootcatch/nativeengine/NativeEngineRunner.java");
        assertTrue(runner.contains("ControlPulse pulses = new ControlPulse"));
        assertTrue(runner.contains("pulses.publishFocus(observedGame, observedFocus.notificationShadeAllowed(), focusStarted)"));
        assertTrue(runner.contains("pulses.failure() != null"));
        assertTrue(runner.contains("\"host_control_pulse\""));
        assertTrue(runner.contains(".put(\"controlRecovery\", true)"));
        assertTrue(runner.contains("synchronized (stopLock)"));
        assertFalse(runner.contains("public synchronized void requestStop(String reason)"));
        assertTrue(runner.contains("\"host_control_end\""));
    }
    @Test public void collectorAlwaysClosesAndReportsUnboundTargetsInsteadOfInventingAPid() throws Exception {
        String collector = read("src/main/java/dev/rootcatch/ugphone/CrashCollector.java");
        assertTrue(collector.contains("CollectorFinalization.run(this::gatherFinalSources, this::stopLive"));
        assertTrue(collector.contains("if (finishing) return"));
        assertTrue(collector.contains("target not bound"));
        assertTrue(collector.contains("worker.interrupt()"));
        assertTrue(collector.contains("\"targetBound\", pid > 1 && ticks > 0"));
    }
    @Test public void jniOptionCanBeDisabledAndIsRecordedByTheHost() throws Exception {
        String ui = read("src/main/java/dev/rootcatch/ugphone/MainActivity.java");
        assertTrue(ui.contains("Fix error 12"));
        assertTrue(ui.contains("putBoolean(\"resolve_error_12\", resolveError12.isChecked())"));
        String runner = read("src/main/java/dev/rootcatch/nativeengine/NativeEngineRunner.java");
        assertTrue(runner.contains("\"dev.rootcatch.ugphonetest\".equals(context.getPackageName())"));
        assertTrue(runner.contains(".put(\"nativeLocationJNI\", false)"));
        assertTrue(runner.contains(".put(\"resolveError12\", nativeLocationJNI)"));
        assertTrue(runner.contains("rootcatch-profile7-v2"));
        assertTrue(runner.contains("json.put(\"sessionId\", sessionId)"));
    }
    @Test public void serviceRetainsVisibleStopWakeRenewalAndNonStickyLifecycle() throws Exception {
        String service = read("src/main/java/dev/rootcatch/ugphone/FarmService.java");
        assertTrue(service.contains("dev.rootcatch.ugphone.STOP"));
        assertTrue(service.contains("SilphMod · "));
        assertTrue(service.contains("START_NOT_STICKY")); assertTrue(service.contains("nextWakeRenew"));
        assertTrue(service.contains("\"Stop\", stop"));
        assertTrue(service.contains("session.requestStop(reason)"));
        assertTrue(service.contains("if (!notificationsVisible(this))"));
    }
    @Test public void exportUsesEditionIdentityAndKeepsRawLogsAndIntegrityIndex() throws Exception {
        String exporter = read("src/main/java/dev/rootcatch/ugphone/LogExporter.java");
        assertTrue(exporter.contains("SilphMod_"));
        assertTrue(exporter.contains("Download/SilphMod"));
        assertTrue(exporter.contains("\"rawLogs\", true")); assertTrue(exporter.contains("bundle-index.json"));
        assertTrue(exporter.contains("\"sha256\", digest"));
    }
    @Test public void shadeExceptionRequiresUnderlyingGameFreshControlAndAnUnlockedInteractiveScreen() throws Exception {
        String runner = read("src/main/java/dev/rootcatch/nativeengine/NativeEngineRunner.java");
        assertTrue(runner.contains("observedFocus.isGame() && screenAllowsActions()"));
        assertTrue(runner.contains("power.isInteractive() && !keyguard.isKeyguardLocked()"));
        assertTrue(runner.contains(".put(\"notificationShade\", pulse.notificationShade)"));
        assertTrue(runner.contains(".put(\"notificationShadeSupport\", true)"));
    }
    @Test public void throwChoiceIsPersistedPassedToTheEngineAndIncludedInSessionLogs() throws Exception {
        String ui = read("src/main/java/dev/rootcatch/ugphone/MainActivity.java");
        String config = read("src/main/java/dev/rootcatch/automation/AutomationConfig.java");
        String runner = read("src/main/java/dev/rootcatch/nativeengine/NativeEngineRunner.java");
        String session = read("src/main/java/dev/rootcatch/ugphone/FarmSession.java");
        assertTrue(ui.contains("R.string.throw_quality_selector")); assertTrue(ui.contains("settings.add(throwSelector)"));
        assertTrue(ui.contains(".putString(\"throw_quality\", throwQuality.name())"));
        assertTrue(config.contains("ThrowQuality.fromSavedValue(p.getString(\"throw_quality\", \"GREAT\")).name()"));
        assertTrue(runner.contains(".put(\"throwQuality\", config.throwQuality)"));
        assertTrue(session.contains(".put(\"throwQuality\", config.throwQuality)"));
    }
    @Test public void initialFocusQueryIsLoggedWithActualSourceResponseIdentityAndWaitTime() throws Exception {
        String session = read("src/main/java/dev/rootcatch/ugphone/FarmSession.java");
        String runner = read("src/main/java/dev/rootcatch/nativeengine/NativeEngineRunner.java");
        assertTrue(session.contains("NativeEngineRunner.gameFocused(root, () -> stopping, sample ->"));
        assertTrue(session.contains("record(\"startup_focus\", new JSONObject(sample).put(\"pid\", candidate).put(\"startTicks\", ticks)"));
        assertTrue(session.contains(".put(\"startupWaitMs\", SystemClock.elapsedRealtime() - waitingSince)"));
        assertTrue(runner.contains(".read(\"early_startup\").focus.initialAttachAllowed()"));
        assertTrue(runner.contains(".put(\"raw\", sample.response).put(\"command\", sample.command)"));
        assertTrue(runner.contains("focusReader.read(\"before_attach\").focus.initialAttachAllowed()"));
    }
    @Test public void berryChoicesArePersistedPassedToTheEngineAndLoggedWithoutChangingDiscardLimits() throws Exception {
        String ui = read("src/main/java/dev/rootcatch/ugphone/MainActivity.java");
        String config = read("src/main/java/dev/rootcatch/automation/AutomationConfig.java");
        String runner = read("src/main/java/dev/rootcatch/nativeengine/NativeEngineRunner.java");
        String session = read("src/main/java/dev/rootcatch/ugphone/FarmSession.java");
        assertTrue(ui.contains("settings.add(berrySelector)"));
        assertTrue(ui.contains(".putBoolean(\"use_berries\", useBerries.isChecked()).putString(\"berry_order\", berryOrder)"));
        assertTrue(ui.contains("new SettingsDialogs(this).berriesDialog(berryOrder"));
        assertTrue(config.contains("p.getBoolean(\"use_berries\", true)"));
        for (String host : new String[]{runner, session}) {
            assertTrue(host.contains(".put(\"useBerries\", config.useBerries).put(\"berryOrder\", new org.json.JSONArray(config.berryOrder))"));
        }
        assertTrue(runner.contains(".put(\"discard\", new JSONObject(limits))"));
        assertFalse(runner.contains("limits.remove("));
    }
    @Test public void sessionLimitsAreEditablePersistedPassedToEngineAndLogged() throws Exception {
        String ui = read("src/main/java/dev/rootcatch/ugphone/MainActivity.java");
        String config = read("src/main/java/dev/rootcatch/automation/AutomationConfig.java");
        assertTrue(ui.contains("section(advancedOptions, getString(R.string.session_limits))"));
        assertTrue(ui.contains(".putInt(\"max_catches\", catchLimit).putInt(\"max_spins\", spinLimit)"));
        assertTrue(config.contains("SessionLimits.DEFAULT_CATCHES"));
        assertTrue(config.contains("SessionLimits.DEFAULT_SPINS"));
        for (String path : new String[]{"nativeengine/NativeEngineRunner.java", "ugphone/FarmSession.java"})
            assertTrue(read("src/main/java/dev/rootcatch/" + path).contains(".put(\"maxCatches\", config.maxCatches).put(\"maxSpins\", config.maxSpins)"));
    }
    @Test public void limitClosureWaitsForEngineCompletionAndStopsIdentityMonitoringBeforeClosingTheGame() throws Exception {
        String session = read("src/main/java/dev/rootcatch/ugphone/FarmSession.java");
        int completed = session.indexOf("runner.run();");
        int limit = session.indexOf("if (!runner.sessionLimitReached().isEmpty())", completed);
        int monitor = session.indexOf("monitorRoot.close();", limit);
        int close = session.indexOf("limitStop.close(pid, startTicks)", limit);
        int collect = session.indexOf("collector.finish();", close);
        assertTrue(completed >= 0 && limit > completed && monitor > limit && close > monitor && collect > close);
        String runner = read("src/main/java/dev/rootcatch/nativeengine/NativeEngineRunner.java");
        assertTrue(runner.contains("agentStopped && sessionLimitAcknowledged && cleanupConfirmed && nativeLocationCleanupConfirmed && !requestedStop"));
        assertTrue(runner.contains("sessionLimitKind.equals(payload.optString(\"limitStopKind\"))"));
        assertTrue(session.contains("record(\"session_limit_completed\""));
    }
    @Test public void interruptionChoiceIsPersistedPassedToTheEngineAndIncludedInSessionLogs() throws Exception {
        String ui = read("src/main/java/dev/rootcatch/ugphone/MainActivity.java");
        String config = read("src/main/java/dev/rootcatch/automation/AutomationConfig.java");
        assertTrue(ui.contains("R.string.skip_interruptions"));
        assertTrue(ui.contains(".putBoolean(\"skip_interruptions\", skipInterruptions.isChecked())"));
        assertTrue(config.contains("p.getBoolean(\"skip_interruptions\", true)"));
        for (String path : new String[]{"nativeengine/NativeEngineRunner.java", "ugphone/FarmSession.java"})
            assertTrue(read("src/main/java/dev/rootcatch/" + path).contains(".put(\"skipInterruptions\", config.skipInterruptions)"));
    }
    @Test public void closureDiagnosticsDoNotReplaceTheConfirmedSessionLimitReason() throws Exception {
        String session = read("src/main/java/dev/rootcatch/ugphone/FarmSession.java");
        assertTrue(session.contains("limitReason = dev.rootcatch.automation.SessionLimits.reason(runner.sessionLimitReached())"));
        assertTrue(session.contains(".put(\"closureStatus\", closure.status).put(\"closureError\", closure.error)"));
        assertTrue(session.contains("limitReason.isEmpty() ? \"Engine: \" + error.getMessage() : limitReason"));
        assertTrue(session.contains("record(\"session_limit_finalize_error\""));
        assertFalse(session.contains("result += \" · game closure unconfirmed\""));
        String close = read("src/main/java/dev/rootcatch/ugphone/SessionLimitStop.java");
        assertFalse(close.contains("GameStartCheck.PROC"));
    }
}
