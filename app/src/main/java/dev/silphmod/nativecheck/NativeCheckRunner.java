package dev.silphmod.nativecheck;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.os.SystemClock;
import dev.rootcatch.nativeengine.*;
import dev.rootcatch.root.RootShell;
import org.json.JSONObject;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.*;
import java.util.function.BiConsumer;
import java.util.regex.*;

final class NativeCheckRunner {
    private static final class SymbolResolutionFailure extends IOException {
        SymbolResolutionFailure(int error){super("System library resolution failed (errno "+error+"); export logs");}
    }
    private static final class NamespaceFailure extends IOException {
        NamespaceFailure(){super("Native namespace rejected both loading paths; export logs");}
    }
    private static final class TargetExited extends IOException {
        TargetExited(){super("Pokémon GO exited during native startup");}
    }
    private static final class ProgressStalled extends IOException {
        final boolean workStarted;
        ProgressStalled(boolean workStarted){super("Native engine progress stalled");this.workStarted=workStarted;}
    }
    static final String GAME="com.nianticlabs.pokemongo",GAME_SHA="a98b7107ef60fb91abc1c7cea9678d45e71b715bd53aacaf4b3ff688b99b6cfd";
    static final class Result {final String reason;final boolean cleanup;Result(String reason,boolean cleanup){this.reason=reason;this.cleanup=cleanup;}}
    private final Context context;
    private final int mode;
    private final NativeSettings settings;
    private final boolean readonly;
    private final BlockingQueue<NativeKecleonWire.Event> candidates=new ArrayBlockingQueue<>(16);
    private NativeKecleonHost kecleon;
    private NativeRocketHost rocket;
    private final CheckLog log;
    private final BiConsumer<String,String> display;
    private final RootShell shell=new RootShell();
    private final BlockingQueue<String> lines=new ArrayBlockingQueue<>(128);
    private volatile boolean stopping,focusActive,focusQuit,streamFailed,streamEnded;
    private volatile long focusAt;
    private volatile NativeSessionController controller;
    private volatile NativeProgressWatch progressWatch;
    private volatile String status="Preparing…";
    private volatile long frames,sequence;
    private volatile int rewardsAvailable;
    private volatile long droppedLogs;
    private volatile int gamePid;
    private long ticks;
    private Process client;
    private Thread focusThread,readerThread,pulseThread;
    private volatile boolean pulseQuit;
    private volatile boolean stopAcknowledged;
    private boolean cleanupForeground;
    private volatile Throwable pulseFailure;
    private String statusAddress,nonceText;
    private String snapshotKey="";
    private long snapshotLoggedAt;
    private boolean injected,cleanup;
    private String finishReason="";
    private volatile int nativeReason,nativeError;
    private long accountHi,accountLo;
    private boolean buddyHandled;
    private int startupAttempts;
    private volatile boolean startupWorkStarted;
    private int recoveries,observedPid;
    private int progressRecoveries;
    private int rocketScreenRecoveries;
    private int transportFailures;
    private long connectedAt;
    private volatile long lastGameActiveAt;
    private volatile boolean confirmedWork;
    private boolean connectionDiagnosed,failedProcessAlive,failedWhileActive;
    private NativeExitReport exitReport;
    private String startupStage="prepare";
    private boolean gameNamespace;
    private long launchWallTime;
    private String namespaceCache;
    private String gameComponent;
    private boolean farmerMenuBlocked;
    private final java.util.LinkedHashMap<File,String> gameStageFiles=new java.util.LinkedHashMap<>();
    private File gameStageDirectory;
    NativeCheckRunner(Context context,int mode,CheckLog log,BiConsumer<String,String> display){this.context=context;this.mode=mode;this.log=log;this.display=display;settings=new NativeSettings(context);readonly=false;if(mode==3)NativeRocketHost.beginSession(context);}
    void stop(){stopping=true;NativeSessionController current=controller;if(current!=null)current.requestStop("Stopped by user");}
    String details(){
        NativeSessionController c=controller;NativeSessionState.Totals t=c==null?settings.totals(mode):c.totals();
        String counts=mode==1?"Catches "+t.caught+" · Spins "+t.spun+" · Transferred "+t.transferred:
            (mode==3?"Rockets "+t.spun:"Encounters "+t.encounters)+" · Catches "+t.caught+" · Shinies "+t.shinies;
        return(mode==1?"Farmer":mode==2?"Kecleon (BETA)":"AutoRocket (BETA)")+"\n"+counts+"\nItems deleted "+t.discarded+
            "\nXP "+((rewardsAvailable&1)!=0?t.xp:"—")+" · Stardust "+((rewardsAvailable&2)!=0?t.stardust:"—")+"\nTime "+String.format(java.util.Locale.US,"%02d:%02d:%02d",settings.elapsed(mode)/3600000,settings.elapsed(mode)/60000%60,settings.elapsed(mode)/1000%60);
    }
    private void status(String text){status=text;display.accept(text,details());log.write("status",text);}
    Result run(){
        try{
          while(true){
            prepare();if(stopping)return new Result("Stopped before connection",true);
            Exception connectionError=null;
            try{connect();monitor();verifyCleanup();}
            catch(Exception error){
                if(error instanceof ProgressStalled&&((ProgressStalled)error).workStarted){
                    connectionError=error;log.write("progress_stalled",errorText(error));stopAndDrain("Recovering stalled engine");
                }else{
                    if(!startupTransportFailure(error)&&!(error instanceof IOException&&NativeRecoveryPlan.transportFailure(error.getMessage(),finishReason)))throw error;
                    connectionError=error;log.write("connection_error",errorText(error));
                }
            }
            if(connectionError instanceof ProgressStalled&&((ProgressStalled)connectionError).workStarted){
                if(NativeRecoveryPlan.progressRetry(readonly,stopping,nativeError,nativeReason,progressRecoveries)){
                    log.write("progress_recovery","attempt="+(++progressRecoveries)+" maximum=3 hooksClean="+cleanup+" statisticsPreserved=true pendingActionsReplayed=false");
                    releaseAttempt(false);checkStop();resetAttempt();startupAttempts=0;status("Recovering engine ("+progressRecoveries+"/3)…");continue;
                }
                if(!stopping)throw connectionError;
                finishReason="Stopped by user";
            }
            boolean transportFailure=connectionError!=null||"Native control disconnected".equals(finishReason);
            if(transportFailure&&!(connectionError instanceof ProgressStalled)){
                diagnoseConnection();
                if(stopping)finishReason="Stopped by user";
                else if(!failedProcessAlive&&exitReport!=null&&exitReport.intentional){finishReason=exitReport.summary;transportFailure=false;}
                else if(startupWorkStarted&&nativeError==0&&nativeReason==0){
                    transportFailures=NativeRecoveryPlan.transportFailures(transportFailures,SystemClock.elapsedRealtime()-connectedAt,confirmedWork);
                    boolean recent=failedWhileActive&&(!failedProcessAlive||focusActive&&SystemClock.elapsedRealtime()-focusAt<=2500);
                    boolean recoverable=failedProcessAlive||exitReport!=null&&exitReport.recoverable;
                    if(mode==3&&exitReport!=null&&NativeRecoveryPlan.unreportedStartupExit(failedProcessAlive,exitReport.intentional,exitReport.crash.isEmpty()&&exitReport.history.isEmpty(),confirmedWork,SystemClock.elapsedRealtime()-connectedAt)){
                        recoverable=true;log.write("unreported_startup_exit","No Android exit report; bounded Rocket startup recovery eligible");
                    }
                    if(NativeRecoveryPlan.transportRetry(readonly,stopping,nativeError,nativeReason,transportFailures,recoverable,recent)){
                        log.write("transport_recovery","completedAttempts="+transportFailures+" maximum=10 processAlive="+failedProcessAlive+" statisticsPreserved=true pendingActionsReplayed=false");
                        releaseAttempt(false);checkStop();resetAttempt();startupAttempts=0;status("Reconnecting game ("+(transportFailures+1)+"/10)…");continue;
                    }
                    if(transportFailures>=10)finishReason="Game recovery failed after 10 attempts; export logs";
                    else if(!failedProcessAlive&&exitReport!=null)finishReason=exitReport.summary;
                }
            }
            if(NativeRecoveryPlan.startupFailure(readonly,stopping,startupWorkStarted,nativeError,nativeReason,transportFailure)){
                startupFailureDetails(startupAttempts);
                if(NativeRecoveryPlan.startupRetry(readonly,stopping,startupWorkStarted,startupAttempts,nativeError,nativeReason,transportFailure)){
                    log.write("startup_recovery","completedAttempts="+startupAttempts+" maximum=10 workStarted=false statisticsPreserved=true");
                    releaseAttempt(false);checkStop();resetAttempt();status("Retrying startup ("+(startupAttempts+1)+"/10)…");continue;
                }
                if(!stopping)finishReason="Startup failed after "+startupAttempts+" attempts · Native connection lost before farming";
            }
            if(finishReason.isEmpty())finishReason=stopping?"Stopped by user":"Native connection ended";
            if(mode==3&&!stopping&&cleanup&&nativeError==0&&nativeReason==7){
                if(++rocketScreenRecoveries>3)return new Result("Battle screen recovery failed; export logs",true);
                log.write("rocket_screen_recovery","attempt="+rocketScreenRecoveries+" maximum=3 statisticsPreserved=true candidateSkipped=true");releaseAttempt(false);checkStop();resetAttempt();startupAttempts=0;status("Continuing Rocket search…");continue;
            }
            if(NativeRecoveryPlan.restart(readonly,stopping,cleanup,nativeError,nativeReason,buddyHandled)){
                if(nativeReason==11)buddyHandled=true;
                settings.config(mode,readonly);
                log.write("automatic_recovery","cycle="+(++recoveries)+" reason="+nativeReason+" cleanupConfirmed=true statisticsPreserved=true");
                boolean removed=nativeReason==11;releaseAttempt(false);checkStop();resetAttempt();startupAttempts=0;status(removed?"Buddy removed; restarting Pokémon GO…":"No spawns; restarting Pokémon GO…");continue;
            }
            return new Result(finishReason,cleanup);
          }
        }catch(Exception error){
            log.write("error",errorText(error));
            if(mode==1&&stopping&&"Native cleanup acknowledgement timed out".equals(error.getMessage())){
                try{closeBoundGame();cleanup=readTicks(gamePid)!=ticks;log.write("stop_process_cleanup","originalProcessGone="+cleanup+" nativeAcknowledgement=false");if(cleanup)return new Result("Stopped by user",true);}
                catch(Exception closing){log.write("stop_process_cleanup_failed",closing.toString());}
            }
            try{stopAndDrain("Session error");}catch(Exception closing){log.write("stop_diagnostic_error",closing.toString());}
            if(controller==null&&injected&&gamePid>1&&ticks>0)try{
                closeBoundGame();
                cleanup=readTicks(gamePid)!=ticks;
                log.write("bootstrap_process_closed","pid="+gamePid+" originalProcessGone="+cleanup+" managedEngineStarted=false");
            }catch(Exception closing){log.write("bootstrap_close_failed",closing.toString());}
            String reason=error.getMessage()==null?error.toString():error.getMessage();
            return new Result(stopping&&(cleanup||!injected)?"Stopped by user":stopping?"Stop incomplete: "+reason:reason,cleanup||!injected);
        }finally{
            try{releaseAttempt(true);}catch(Exception error){log.write("finalize_ui",error.toString());}shell.close();
        }
    }
    private void stopAndDrain(String reason) throws Exception {
        if(controller==null)return;
        if(!controller.finished()){
            controller.requestStop(reason);long deadline=SystemClock.elapsedRealtime()+7000;
            while(!controller.finished()&&SystemClock.elapsedRealtime()<deadline){consume();wakeOwnerForCleanup();controller.pulse(false);SystemClock.sleep(100);}
        }
        consume();
        if(cleanup)verifyCleanup();
    }
    private boolean startupTransportFailure(Exception error){
        String message=error.getMessage();
        boolean transport="Native control disconnected".equals(finishReason)||"Game process changed".equals(message)
            ||"Native snapshots timed out".equals(message)||"Native control pulse failed".equals(message)
            ||"Native engine progress stalled".equals(message)||"Native control stopped responding".equals(message);
        return error instanceof IOException&&NativeRecoveryPlan.startupFailure(readonly,stopping,startupWorkStarted,nativeError,nativeReason,transport);
    }
    private void powerState(){
        try{
            android.os.PowerManager power=(android.os.PowerManager)context.getSystemService(Context.POWER_SERVICE);
            android.app.ActivityManager activity=(android.app.ActivityManager)context.getSystemService(Context.ACTIVITY_SERVICE);
            log.write("power_config","batteryOptimizationExempt="+(power!=null&&power.isIgnoringBatteryOptimizations(BuildConfig.APPLICATION_ID))+" gameBatteryOptimizationExempt="+(power!=null&&power.isIgnoringBatteryOptimizations(GAME))+" powerSave="+(power!=null&&power.isPowerSaveMode())+" backgroundRestricted="+(activity!=null&&activity.isBackgroundRestricted()));
        }catch(Exception error){log.write("power_config_error",error.toString());}
    }
    private void diagnoseConnection(){
        if(connectionDiagnosed)return;connectionDiagnosed=true;
        long activeAt=lastGameActiveAt;
        failedWhileActive=activeAt>0&&SystemClock.elapsedRealtime()-activeAt<=2500;
        try{failedProcessAlive=gamePid>1&&readTicks(gamePid)==ticks;if(gamePid>1&&!failedProcessAlive){cleanup=true;log.write("process_cleanup","Original game process exited; native hooks cannot remain active");}}catch(Exception error){log.write("connection_diagnostic_error",error.toString());}
        log.write("connection_lost","pid="+gamePid+" startTicks="+ticks+" processAlive="+failedProcessAlive+" workStarted="+startupWorkStarted+" confirmedWork="+confirmedWork+" recentGameFocus="+failedWhileActive);
        if(stopping)return;
        String since=new java.text.SimpleDateFormat("MM-dd HH:mm:ss.SSS",java.util.Locale.US).format(new java.util.Date(launchWallTime));
        long began=SystemClock.elapsedRealtime();
        for(int attempt=0;attempt<2&&!stopping;attempt++){
            if(attempt>0){if(SystemClock.elapsedRealtime()-began>3500)break;SystemClock.sleep(250);}
            String crash="",history="";
            try{crash=command("logcat -b crash -d -v threadtime -T "+quote(since)+" 2>&1 | tail -c 65536",2500);}catch(Exception error){log.write("connection_diagnostic_error",error.toString());}
            if(!stopping&&android.os.Build.VERSION.SDK_INT>=30)try{history=command("dumpsys activity exit-info "+GAME+" 2>&1 | head -c 65536",2500);}catch(Exception error){log.write("connection_diagnostic_error",error.toString());}
            exitReport=NativeExitReport.inspect(crash,history,gamePid,GAME,launchWallTime);
            if(failedProcessAlive||exitReport.recoverable||exitReport.intentional)break;
        }
        if(exitReport==null)exitReport=NativeExitReport.inspect("","",gamePid,GAME,launchWallTime);
        log.write("failure_crash_log",exitReport.crash.isEmpty()?"No matching native crash record for pid="+gamePid:exitReport.crash);
        log.write("failure_exit_info",exitReport.history.isEmpty()?"No matching exit record for pid="+gamePid:exitReport.history);
        log.write("connection_classification",failedProcessAlive?"Control connection lost; bound game process still alive":exitReport.summary);
        powerState();
    }
    private void releaseAttempt(boolean returnToApp) throws Exception {
            settings.elapsedPulse(mode,false);
            if(mode==3&&gamePid>1)try{log.write("rocket_native_details",command("logcat -d -v brief --pid="+gamePid+" -s SilphRocket:V SilphRuntime:I | tail -c 18000",3000));}catch(Exception error){log.write("rocket_diagnostic_error",error.toString());}
            if(mode==1&&gamePid>1)try{log.write("farmer_native_details",command("logcat -b all -d -v brief --pid="+gamePid+" -s SilphFarmer:I SilphEvent:I SilphRuntime:I | tail -c 16000",3000));}catch(Exception error){log.write("farmer_diagnostic_error",error.toString());}
            if(mode==2&&gamePid>1)try{log.write("kecleon_native_details",command("logcat -b all -d -v brief --pid="+gamePid+" -s SilphRuntime:I SilphFarmer:I | tail -c 12000",3000));}catch(Exception error){log.write("kecleon_diagnostic_error",error.toString());}
            if(kecleon!=null)kecleon.close();if(rocket!=null)rocket.close();
            pulseQuit=true;if(pulseThread!=null)pulseThread.interrupt();focusQuit=true;if(focusThread!=null)focusThread.interrupt();
            if(client!=null){try{client.getOutputStream().close();client.waitFor(1500,TimeUnit.MILLISECONDS);}catch(Exception ignored){ }if(client.isAlive())client.destroyForcibly();}
                closeBoundGame();
                cleanGameStage();
                for(Thread thread:new Thread[]{pulseThread,readerThread,focusThread})if(thread!=null){thread.join(3500);if(thread.isAlive())throw new IOException("Native controller worker did not stop");}
                try{consume();}catch(Exception error){log.write("final_event_error",error.toString());}
                if(controller!=null)settings.save(mode,controller.totals());
                if(returnToApp)command("am start -n "+BuildConfig.APPLICATION_ID+"/dev.silphmod.nativecheck.MainActivity --ei mode "+mode,5000);
    }
    private void closeBoundGame() throws Exception {
        if(gamePid<=1||ticks<=0)return;
        String stat=command(NativeStartupPolicy.statCommand(gamePid),4000);
        if(!NativeStartupPolicy.live(stat,gamePid,ticks))return;
        command("am force-stop "+GAME,5000);
        long until=SystemClock.elapsedRealtime()+5000;
        do{stat=command(NativeStartupPolicy.statCommand(gamePid),4000);if(!NativeStartupPolicy.live(stat,gamePid,ticks))break;SystemClock.sleep(100);}while(SystemClock.elapsedRealtime()<until);
        if(NativeStartupPolicy.live(stat,gamePid,ticks))throw new IOException("Previous game process did not stop; startup not retried");
        log.write("game_closed","pid="+gamePid+" ticks="+ticks+" confirmed=true");
    }
    private void resetAttempt(){
        kecleon=null;rocket=null;controller=null;client=null;pulseThread=readerThread=focusThread=null;lines.clear();candidates.clear();
        focusActive=focusQuit=streamFailed=streamEnded=pulseQuit=stopAcknowledged=cleanupForeground=false;focusAt=0;pulseFailure=null;
        frames=sequence=0;gamePid=0;ticks=0;snapshotKey="";snapshotLoggedAt=0;injected=cleanup=false;finishReason="";statusAddress=nonceText=null;
        startupWorkStarted=false;nativeReason=nativeError=0;progressWatch=null;controlKey=null;
        farmerMenuBlocked=false;
        connectedAt=lastGameActiveAt=0;confirmedWork=connectionDiagnosed=failedProcessAlive=failedWhileActive=false;exitReport=null;
    }
    private void prepare() throws Exception {
        status("Checking root and game…");startupStage="root";
        RootShell.Result root=shell.execute("id",12000);log.write("root_check","exit="+root.code+" "+root.output);
        if(root.code!=0||!root.output.contains("uid=0"))throw new IOException("Root access required");checkStop();
        log.write("session_config","mode="+mode+" fixLocation="+settings.prefs.getBoolean("fixLocation",false));
        powerState();
        String environment=command("getprop ro.build.version.sdk; getprop ro.product.cpu.abilist; getprop ro.dalvik.vm.native.bridge; getconf PAGESIZE; getenforce; uname -r",5000);
        log.write("environment",environment);
        namespaceCache="namespace."+android.os.Build.FINGERPRINT.hashCode()+"."+GAME_SHA.substring(0,16);
        String[] environmentLines=environment.split("\\R");
        gameNamespace=NativeStartupPolicy.gameStage(android.os.Build.VERSION.SDK_INT,environmentLines.length==0?"":environmentLines[environmentLines.length-1],settings.prefs.getBoolean(namespaceCache,false));
        startupStage="game_profile";
        settings.config(mode,readonly);
        PackageInfo info=context.getPackageManager().getPackageInfo(GAME,0);
        if(!"0.433.1".equals(info.versionName))throw new IOException("Pokémon GO 0.433.1 required");
        for(String other:new String[]{"dev.silphmod.nativetest","dev.silphmod.nativecheck"}){
            if(other.equals(BuildConfig.APPLICATION_ID))continue;
            String services=command("dumpsys activity services "+other,8000);
            if(Pattern.compile("(?m)^\\s*packageName="+Pattern.quote(other)+"\\s*$").matcher(services).find())throw new IOException("Stop the other SilphMod service first");
        }
        String paths=command("pm path "+GAME,5000),split=null;
        for(String line:paths.split("\\R"))if(line.startsWith("package:")&&line.endsWith("/split_config.arm64_v8a.apk"))split=line.substring(8);
        log.write("game_package_paths",paths);
        if(split==null||!split.matches("/data/app/[A-Za-z0-9_./+=~-]+/split_config\\.arm64_v8a\\.apk"))throw new IOException("ARM64 game profile missing");
        if(!command("sha256sum "+quote(split),10000).startsWith(GAME_SHA+" "))throw new IOException("Game library profile mismatch");
        startupStage="components";
        File directory=new File(context.getApplicationInfo().nativeLibraryDir).getCanonicalFile();
        log.write("native_library_directory",directory.getAbsolutePath());
        if(!directory.getAbsolutePath().matches("/data/app/[A-Za-z0-9_./+=~-]+/lib/arm64"))throw new IOException("Invalid native library directory");
        for(String name:new String[]{"libsilph-native-probe.so","libsilph-control.so","libsilph-farmer-check.so","libsilph-kecleon-check.so","libsilph-farmer-status.so","libsilph-kecleon-status.so","libsilph-rocket-check.so","libsilph-rocket-status.so","libshadowhook.so","libshadowhook_nothing.so"}){
            File file=new File(directory,name);if(!file.isFile()||!file.getCanonicalFile().getParentFile().equals(directory))throw new IOException("Native component missing: "+name);
            String actual=hash(file);if(!actual.equals(NativeArtifacts.hash(name)))throw new IOException("Native component integrity mismatch");log.write("component",name+" sha256="+actual);
        }
        String existing=command("pidof "+GAME+" || true",5000);
        if(!existing.isEmpty()){
            if(!existing.matches("[0-9]+"))throw new IOException("Ambiguous game process");
            String conflict=command("grep -iE 'rootcatch-inject|silph-minimal' /proc/"+existing+"/maps || true",5000);
            if(!conflict.isEmpty())throw new IOException("Stop the other injector and close Pokémon GO");
        }
        String cache="route."+android.os.Build.FINGERPRINT.hashCode()+"."+GAME_SHA.substring(0,16);
        int preferred=settings.prefs.getInt(cache,2);if(preferred<0||preferred>3)preferred=2;
        Exception startupFailure=null;
        for(;startupAttempts<10;){
            int attempt=startupAttempts++;
            int route=attempt==0?preferred:((preferred+attempt)%4);
            try{launchAndLoad(directory,route,attempt==0&&!settings.prefs.getBoolean("fixLocation",false)&&settings.prefs.getInt("startup",0)!=1&&existing.matches("[0-9]+"));settings.prefs.edit().putInt(cache,route).apply();startupFailure=null;break;}
            catch(Exception error){startupFailure=error;log.write("startup_attempt","attempt="+(attempt+1)+" route="+route+" stage="+startupStage+" error="+errorText(error));checkStop();
                startupFailureDetails(attempt+1);
                if(error instanceof SymbolResolutionFailure||error instanceof NamespaceFailure)throw error;
                if(error.getMessage()!=null&&(error.getMessage().contains("policy denied")||error.getMessage().contains("profile mismatch")||error.getMessage().contains("profile rejected")||error.getMessage().contains("instrumentation conflict")||error.getMessage().contains("Game stage file conflict")))throw error;
                closeBoundGame();cleanGameStage();gamePid=0;ticks=0;injected=false;
            }
        }
        if(startupFailure!=null)throw new IOException("Native startup failed after 10 attempts: "+startupFailure.getMessage(),startupFailure);
        NativeSessionWire.Config config=settings.config(mode,readonly);if(buddyHandled)config.flags&=~NativeSessionWire.REMOVE_BUDDY;config.pid=gamePid;config.ticks=ticks;config.sessionHi=Long.parseUnsignedLong(nonceText);config.sessionLo=1;
        config.encode();
        if(mode==1)log.write("farmer_configuration","delayMs="+config.delayMs+" flags="+config.flags+" quality="+config.quality+" itemLimits="+config.limits.length);
        client=new ProcessBuilder("su","-c",quote(new File(directory,"libsilph-control.so").getAbsolutePath())+" "+gamePid+" "+ticks).redirectErrorStream(true).start();
        BufferedWriter writer=new BufferedWriter(new OutputStreamWriter(client.getOutputStream(),StandardCharsets.UTF_8));
        writer.write(nonceText);writer.write(' ');writer.write(controlKey);writer.newLine();writer.flush();controlKey=null;
        progressWatch=new NativeProgressWatch(SystemClock.elapsedRealtime());
        controller=new NativeSessionController(config,settings.totals(mode),text->{try{writer.write(text);writer.newLine();writer.flush();}catch(IOException error){throw new IllegalStateException("Native controller write failed",error);}},new NativeSessionController.Listener(){
            public void status(String message){status=message;event("status",message);}
            public void update(NativeSessionWire.Snapshot snapshot,NativeSessionState.Totals totals){
                frames++;sequence=snapshot.sequence;rewardsAvailable=snapshot.rewardsAvailable;
                progressWatch.snapshot(SystemClock.elapsedRealtime());
                if(snapshot.caught!=0||snapshot.spun!=0||snapshot.transferred!=0||snapshot.discarded!=0||snapshot.encounters!=0||snapshot.berries!=0||snapshot.throwsCount!=0)startupWorkStarted=true;
                if(snapshot.caught!=0||snapshot.spun!=0||snapshot.transferred!=0||snapshot.discarded!=0)confirmedWork=true;
                nativeReason=snapshot.reason;nativeError=snapshot.error;
                if((snapshot.flags&2)!=0){if(accountHi==0&&accountLo==0){accountHi=snapshot.accountHi;accountLo=snapshot.accountLo;}else if(accountHi!=snapshot.accountHi||accountLo!=snapshot.accountLo)throw new IllegalStateException("Account changed during recovery");}
                String key="state="+snapshot.state+" flags="+snapshot.flags+" error="+snapshot.error+" reason="+snapshot.reason+" catches="+snapshot.caught+" spins="+snapshot.spun+" transferred="+snapshot.transferred+" items="+snapshot.discarded+" encounters="+snapshot.encounters+" candidateState="+snapshot.candidateState+" xp="+snapshot.xp+" stardust="+snapshot.stardust+" rewardsAvailable="+snapshot.rewardsAvailable+" encounterId="+Long.toUnsignedString(snapshot.encounterId)+" capturedId="+Long.toUnsignedString(snapshot.capturedId)+" visualAvailable="+snapshot.visualAvailable+" visualFailed="+snapshot.visualFailed+" ivReads="+snapshot.ivReads+" nameUpdates="+snapshot.nameUpdates;

                key+=" locationStatus="+snapshot.locationStatus;
                if(mode==1)key+=" researchState="+snapshot.candidateState+" researchClaims="+(snapshot.candidateRequest&0xffffffffL)+" researchRemoved="+(snapshot.candidateRequest>>>32)+" researchStatus="+snapshot.researchStatus;
                long now=SystemClock.elapsedRealtime();if(!key.equals(snapshotKey)||now-snapshotLoggedAt>=5000){snapshotKey=key;snapshotLoggedAt=now;event("native_snapshot","sequence="+sequence+" "+key);}
            }
            public void candidate(NativeKecleonWire.Event value){startupWorkStarted=true;event(mode==3?"rocket_native_event":"kecleon_native_event","request="+value.request+" phase="+value.phase+" reason="+value.reason+" verified="+value.verified+" source="+value.source+" fort="+value.fort+" incident="+value.incident);if(!candidates.offer(value)){streamFailed=true;event(mode==3?"rocket_queue":"kecleon_queue","overflow");}}
            public void reply(long executed){progressWatch.reply(SystemClock.elapsedRealtime(),executed);}
            public void finished(String reason,boolean confirmed){finishReason=reason;cleanup=confirmed;event("native_finished",reason+" cleanup="+confirmed);}
        });
        if(mode==3&&!readonly)rocket=new NativeRocketHost(context,controller,log,()->!stopping&&!controller.finished()&&focusActive&&SystemClock.elapsedRealtime()-focusAt<=2500);
        if(mode==2&&!readonly)kecleon=new NativeKecleonHost(context,controller,log,()->!stopping&&!controller.finished()&&focusActive&&SystemClock.elapsedRealtime()-focusAt<=2500);
        startReader();
    }
    private String controlKey;
    private void launchAndLoad(File directory,int route,boolean existing) throws Exception {
        observedPid=0;startupStage="launch";checkStop();if(!existing)command("am force-stop "+GAME,5000);
        log.write("startup_route","trace="+(route<2?"ATTACH":"SEIZE")+" library="+(route%2==0?"FD":"PATH")+" game="+(existing?"EXISTING":"COLD"));
        Intent launch=context.getPackageManager().getLaunchIntentForPackage(GAME);
        if(launch==null||launch.getComponent()==null)throw new IOException("Game launch activity missing");
        String component=launch.getComponent().flattenToString();if(!component.matches("com\\.nianticlabs\\.pokemongo/[A-Za-z0-9_.$]+"))throw new IOException("Invalid game launch activity");
        gameComponent=component;
        File probe=new File(directory,"libsilph-native-probe.so"),source=new File(directory,mode==1?"libsilph-farmer-check.so":mode==2?"libsilph-kecleon-check.so":"libsilph-rocket-check.so");
        File payload=null;int bootstrapPid=0;long bootstrapTicks=0;
        if(settings.prefs.getBoolean("fixLocation",false))payload=gameNamespace?stageForGame(source):stage(directory,source);
        launchWallTime=System.currentTimeMillis();
        status("Opening Pokémon GO…");log.write("game_launch",command("am start -W -n "+quote(component),15000));
        if(settings.prefs.getBoolean("fixLocation",false)){
            payload=bootstrapLocation(probe,payload,source,route);bootstrapPid=gamePid;bootstrapTicks=ticks;
        }
        startupStage="wait_libraries";
        NativeFocus focus=new NativeFocus(shell,()->stopping,s->log.write("focus",s));
        long deadline=SystemClock.elapsedRealtime()+75_000,stable=0;int lastPid=bootstrapPid,missing=0;boolean stableReady=false;
        while(SystemClock.elapsedRealtime()<deadline){
            checkStop();String current=command("pidof "+GAME+" || true",5000);
            if(current.matches("[0-9]+")){
                missing=0;
                int pid=Integer.parseInt(current);observedPid=pid;if(pid!=lastPid){stable=0;lastPid=pid;}
                String maps=command("grep -E 'libil2cpp[.]so|libunity[.]so' /proc/"+pid+"/maps || true",5000);
                if(maps.contains("libil2cpp.so")&&maps.contains("libunity.so")){
                    if(stable==0){stable=SystemClock.elapsedRealtime();log.write("libraries_ready","pid="+pid+" stabilityMs=20000");}
                    startupStage="wait_foreground";
                    if(SystemClock.elapsedRealtime()-stable>=20_000&&focus.active()){gamePid=pid;ticks=readTicks(pid);stableReady=true;break;}
                }else stable=0;
            }else{stable=0;if(lastPid>1&&++missing>=3)throw new IOException("Game exited before native engine connection");}
            SystemClock.sleep(500);
        }
        if(!stableReady)throw new IOException("Game did not reach a stable foreground state");checkStop();
        if(bootstrapPid!=0&&(gamePid!=bootstrapPid||ticks!=bootstrapTicks))throw new IOException("Game process changed during location bootstrap");
        verifyInstrumentation(gamePid,payload);
        log.write("target","pid="+gamePid+" ticks="+ticks+" selinux="+command("getenforce",3000));
        startupStage="load_managed";status("Connecting native engine…");
        if(payload==null)payload=gameNamespace?stageForGame(source):stage(directory,source);
        logStartupMaps();
        injected=true;
        String[] traces={"attach","attach","seize","seize"},routes={"fd","path","fd","path"};
        RootShell.Result loaded=null;JSONObject report=null;
        for(int load=0;load<6;load++){
            int connection=NativeStagePolicy.bootstrapRoute(route,load);
            loaded=runLoader(quote(probe.getAbsolutePath())+" --pid "+gamePid+" --start "+ticks+" --library "+quote(payload.getAbsolutePath())+" --trace "+traces[connection]+" --route "+routes[connection]+" --level managed",30000,true);
            String raw=loaded.output;log.write("loader_exit",Integer.toString(loaded.code));report=null;
            for(String line:raw.split("\\R")){try{JSONObject value=new JSONObject(line);if("native_loader_result".equals(value.optString("event")))report=value;}catch(Exception ignored){ }}
            log.write("loader",raw);log.write("loader_attempt","attempt="+(load+1)+" route="+connection+" sameProcess=true");
            assertStartupTarget(report);
            if(load<5&&retryBoundary(report)){log.write("loader_retry","Safe boundary unavailable; process retained");checkStop();SystemClock.sleep(250);continue;}
            if(load<5&&bootstrapPid==0&&!gameNamespace&&namespaceRejected(report)){rememberGameNamespace();payload=stageForGame(source);verifyInstrumentation(gamePid,null);continue;}
            break;
        }
        if(report==null)throw new IOException("Native loader response missing");
        if(namespaceRejected(report))throw new NamespaceFailure();
        if("SYMBOL_RESOLUTION".equals(report.optString("failure")))throw new SymbolResolutionFailure(report.optInt("errno"));
        if("policy".equalsIgnoreCase(report.optString("failure")))throw new IOException("Native policy denied; export logs");
        JSONObject fault=report.optJSONObject("fault");
        String startupError=NativeStagePolicy.startupFailure(report.optString("failure"),report.optString("stage"),fault==null?0:fault.optInt("signal"),fault==null?0:fault.optInt("code"));
        if(startupError!=null)throw new IOException(startupError);
        JSONObject managed=report.optJSONObject("managed");
        if(loaded.code!=0||!report.optBoolean("passed")||report.optBoolean("restartRequired")||!report.optBoolean("restored")||!report.optBoolean("detached")
                ||managed==null||managed.optInt("created")!=7||managed.optInt("released")!=7||managed.optInt("closed")!=1||managed.optInt("remaining")!=0)throw new IOException("Native loader did not confirm startup; export logs");
        Matcher detail=Pattern.compile("Unity pilot: address=(0x[a-f0-9]+) nonce=([0-9]+) resident=1 readOnly=0 controlKeyLo=([0-9]+)").matcher(report.getJSONObject("observation").getString("detail"));
        if(!detail.matches())throw new IOException("Native control identity missing");
        statusAddress=detail.group(1);nonceText=detail.group(2);
        controlKey=detail.group(3);
    }
    private void verifyInstrumentation(int pid,File ownedPayload) throws Exception {
        String before=command("cat /proc/"+pid+"/status; grep -iE 'silph-native-observer|silph-farmer-check|silph-kecleon-check' /proc/"+pid+"/maps || true",5000);
        if(!Pattern.compile("^TracerPid:\\s+0$",Pattern.MULTILINE).matcher(before).find())throw new IOException("Game instrumentation conflict");
        Pattern foreign=Pattern.compile("silph-.*check|silph-native-observer",Pattern.CASE_INSENSITIVE);
        for(String line:before.split("\\R"))if(foreign.matcher(line).find()&&(ownedPayload==null||!line.endsWith(" "+ownedPayload.getAbsolutePath())))throw new IOException("Game instrumentation conflict");
    }
    private File bootstrapLocation(File probe,File payload,File source,int route) throws Exception {
        long until=SystemClock.elapsedRealtime()+10_000;
        while(gamePid==0&&SystemClock.elapsedRealtime()<until){checkStop();String pid=command("pidof "+GAME+" || true",3000);if(pid.matches("[0-9]+")){gamePid=Integer.parseInt(pid);ticks=readTicks(gamePid);}else SystemClock.sleep(200);}
        if(gamePid<2||ticks<1)throw new IOException("Location bootstrap game identity unavailable");
        observedPid=gamePid;startupStage="location_bootstrap";
        verifyInstrumentation(gamePid,null);status("Preparing native location…");
        String[] traces={"attach","attach","seize","seize"},routes={"fd","path","fd","path"};
        boolean gameStage=gameNamespace;
        for(int attempt=0;attempt<8;attempt++){
            checkStop();assertStartupTarget(null);
            if(attempt==0||attempt==4)logStartupMaps();
            int connection=NativeStagePolicy.bootstrapRoute(route,attempt);
            RootShell.Result result=shell.execute(quote(probe.getAbsolutePath())+" --pid "+gamePid+" --start "+ticks+" --library "+quote(payload.getAbsolutePath())+" --trace "+traces[connection]+" --route "+routes[connection]+" --level bootstrap",15000);
            log.write("location_bootstrap_exit","exit="+result.code+" retry="+(attempt+1)+" route="+connection+" gameStage="+gameStage);log.write("location_bootstrap_loader",result.output);JSONObject report=null,lastStage=null;
            for(String line:result.output.split("\\R"))try{JSONObject value=new JSONObject(line);if("native_loader_result".equals(value.optString("event")))report=value;else if("native_loader_stage".equals(value.optString("event")))lastStage=value;}catch(Exception ignored){ }
            if(report==null)throw new IOException(NativeStagePolicy.bootstrapFailure(result.code,lastStage==null?"":lastStage.optString("stage"),lastStage==null?0:lastStage.optInt("errno")));
            assertStartupTarget(report);
            if("SYMBOL_RESOLUTION".equals(report.optString("failure")))throw new SymbolResolutionFailure(report.optInt("errno"));
            if(result.code==0&&report.optBoolean("passed")&&report.optBoolean("restored")&&report.optBoolean("detached")&&!report.optBoolean("restartRequired")){
                JSONObject observation=report.optJSONObject("observation");
                if(observation==null||!observation.optString("detail").equals("Native location bootstrap: pending deadlineMs=120000 gameActions=0"))throw new IOException("Location bootstrap identity invalid");
                injected=true;log.write("location_bootstrap","Prepared before managed engine; gameActions=0");return payload;
            }
            if(!gameStage&&namespaceRejected(report)){rememberGameNamespace();payload=stageForGame(source);gameStage=true;continue;}
            if(gameStage&&namespaceRejected(report))throw new NamespaceFailure();
            if(!retryBoundary(report))throw new IOException("Location bootstrap failed; export logs");
            SystemClock.sleep(500);
        }
        throw new IOException("Location bootstrap could not reach a safe startup boundary");
    }
    private void rememberGameNamespace(){
        gameNamespace=true;settings.prefs.edit().putBoolean(namespaceCache,true).apply();
        log.write("namespace_route","Game library staging selected after a verified clean namespace rejection");
    }
    private void assertStartupTarget(JSONObject report) throws Exception {
        if(report!=null&&"TARGET_EXIT".equals(report.optString("failure")))throw new TargetExited();
        String stat=command(NativeStartupPolicy.statCommand(gamePid),4000);
        if(!NativeStartupPolicy.live(stat,gamePid,ticks)){
            log.write("startup_target_exited","pid="+gamePid+" startTicks="+ticks+" stat="+stat);throw new TargetExited();
        }
    }
    private void logStartupMaps(){
        try{log.write("startup_memory_maps","pid="+gamePid+" startTicks="+ticks+" boundedBytes=49152\n"+command("grep -E 'libart[.]so|libandroid_runtime[.]so|cfi shadow|silph|shadowhook|libunity[.]so|libil2cpp[.]so|libNianticLabsPlugin[.]so' /proc/"+gamePid+"/maps | head -c 49152",4000));}catch(Exception error){log.write("startup_diagnostic_error",errorText(error));}
    }
    private boolean retryBoundary(JSONObject report){
        return report!=null&&NativeStagePolicy.retryBoundary(report.optString("failure"),report.optString("stage"),report.optInt("errno"),
            report.optBoolean("restored"),report.optBoolean("detached"),report.optBoolean("libraryReleased")&&report.optBoolean("scratchReleased")&&report.optBoolean("fdClosed"),
            report.optBoolean("restartRequired",true),report.optLong("gameActions",-1),report.optInt("controlledReturnStops",-1));
    }
    private RootShell.Result runLoader(String command,long timeout,boolean wakeIdle) throws Exception {
        int pid=gamePid;long identity=ticks;Thread[] wake={null};
        try{return shell.execute(command,timeout,line->{
            if(!wakeIdle||stopping||wake[0]!=null||!line.startsWith("{\"event\":\"native_boundary_wait\""))return;
            try{JSONObject event=new JSONObject(line);if(event.optInt("pid")!=pid||!Long.toString(identity).equals(event.optString("startTicks")))return;}catch(Exception ignored){return;}
            wake[0]=new Thread(()->{
                try(RootShell foreground=new RootShell()){
                    RootShell.Result stat=foreground.execute(NativeStartupPolicy.statCommand(pid),2000);
                    if(stopping||stat.code!=0||!NativeStartupPolicy.live(stat.output,pid,identity))return;
                    RootShell.Result resumed=foreground.execute("am start -n "+quote(gameComponent)+" --activity-single-top",3000);
                    log.write("loader_foreground_wake","pid="+pid+" startTicks="+identity+" exit="+resumed.code+" "+resumed.output);
                }catch(Exception error){log.write("loader_foreground_wake_error",errorText(error));}
            },"native-startup-foreground");wake[0].setDaemon(true);wake[0].start();
        });}finally{if(wake[0]!=null){wake[0].join(5500);if(wake[0].isAlive())throw new IOException("Startup foreground request did not finish");}}
    }
    private void startReader(){
        readerThread=new Thread(()->{
            try(Reader input=new InputStreamReader(client.getInputStream(),StandardCharsets.UTF_8)){
                StringBuilder line=new StringBuilder();int value;
                while((value=input.read())>=0){if(value=='\n'){
                    String text=line.toString().trim();line.setLength(0);if(!text.isEmpty()){
                        try{JSONObject row=new JSONObject(text);String kind=row.optString("event");
                            if(kind.equals("native_control_failure")){event("control_failure",text);continue;}
                            controller.accept(text);
                            if(kind.equals("native_control_reply")&&row.optInt("command")==2&&row.optInt("status")==0)stopAcknowledged=true;
                            if(!kind.equals("native_engine_snapshot")&&!kind.equals("native_kecleon_event")&&(!kind.equals("native_control_reply")||row.optInt("command")==2||row.optInt("command")==4||row.optInt("status")!=0||Long.parseLong(row.optString("sequence","0"))%20==0))event("control",text);
                        }catch(Exception error){event("invalid_control",text);streamFailed=true;break;}
                    }
                }else{if(line.length()>=4096){streamFailed=true;break;}line.append((char)value);}}
                if(line.length()!=0)streamFailed=true;
            }catch(IOException error){streamFailed=true;event("control_reader",error.toString());}finally{streamEnded=true;}
        },"native-check-replies");readerThread.setDaemon(true);readerThread.start();
    }
    private void connect(){
        connectedAt=SystemClock.elapsedRealtime();
        focusThread=new Thread(()->{try(RootShell focusShell=new RootShell()){
            NativeFocus focus=new NativeFocus(focusShell,()->focusQuit,s->log.write("focus",s));
            while(!focusQuit){focusActive=focus.active();focusAt=SystemClock.elapsedRealtime();if(focusActive)lastGameActiveAt=focusAt;SystemClock.sleep(800);}
        }catch(Exception error){focusActive=false;log.write("focus_error",error.toString());}},"native-check-focus");focusThread.setDaemon(true);focusThread.start();
        pulseThread=new Thread(()->{
            long previous=SystemClock.elapsedRealtime();
            try{while(!pulseQuit&&!controller.finished()){
                long now=SystemClock.elapsedRealtime();if(now-previous>700)event("pulse_gap","ms="+(now-previous));previous=now;
                controller.pulse(focusActive&&now-focusAt<=2500);SystemClock.sleep(150);
            }}catch(Throwable error){pulseFailure=error;}
        },"native-check-pulse");pulseThread.setDaemon(true);pulseThread.start();
    }
    private void consume() throws Exception {
        String line;while((line=lines.poll())!=null){
            int separator=line.indexOf('\t');if(separator<0)throw new IOException("Invalid session event");
            String kind=line.substring(0,separator),message=line.substring(separator+1);log.write(kind,message);
            if(kind.equals("native_snapshot"))settings.save(mode,controller.totals());
            if(kind.equals("status")||kind.equals("native_snapshot"))display.accept(status,details());
        }
        NativeKecleonWire.Event candidate;while((candidate=candidates.poll())!=null){if(kecleon!=null)kecleon.accept(candidate);if(rocket!=null)rocket.accept(candidate);}
        if(kecleon!=null)kecleon.tick();if(rocket!=null)rocket.tick();
        if(streamFailed)throw new IOException("Native control output invalid or overflowed");
    }
    private void event(String kind,String message){if(!lines.offer(kind+"\t"+message))droppedLogs++;}
    private void monitor() throws Exception {
        long begin=SystemClock.elapsedRealtime(),lastPid=0,stopAt=0,lastFarmerStatus=begin,lastDisplay=begin;
        while(!controller.finished()){
            consume();if(controller.finished())break;
            long now=SystemClock.elapsedRealtime();settings.elapsedPulse(mode,focusActive&&now-focusAt<=2500&&(mode==3?controller.active():status.equals("Active"))&&!stopping);
            if(stopping){controller.requestStop("Stopped by user");if(stopAt==0)stopAt=now;}
            if(now-lastDisplay>=1000){lastDisplay=now;display.accept(status,details());}
            wakeOwnerForCleanup();
            if(readonly&&now-begin>360_000&&stopAt==0){controller.requestStop("Session time limit reached");stopAt=now;}
            if(now-lastPid>=1000){if(readTicks(gamePid)!=ticks)throw new IOException("Game process changed");lastPid=now;}
            if(mode==1&&!stopping&&stopAt==0&&now-lastFarmerStatus>=15000){lastFarmerStatus=now;try{JSONObject view=readNativeStatus("farmer_performance");farmerMenuBlocked=view.optInt("farmerViewAvailable")==1&&view.optInt("farmerViewKind")==0;}catch(Exception error){log.write("farmer_performance_unavailable",error.toString());}}
            if(stopAt>0&&now-stopAt>10_000)throw new IOException("Native cleanup acknowledgement timed out");
            if(pulseFailure!=null)throw new IOException("Native control pulse failed",pulseFailure);
            if(streamEnded&&lines.isEmpty()){log.write("client_exit",client.isAlive()?"pending":Integer.toString(client.exitValue()));controller.disconnected();break;}
            int progress=progressWatch.check(now,focusActive&&now-focusAt<=2500&&!stopping&&stopAt==0&&!farmerMenuBlocked);
            if(progress!=NativeProgressWatch.WAIT){
                long before=frames;log.write("progress_watch",progressWatch.details(now));
                JSONObject value=readNativeStatus("native_stall_status");
                if(controller.finished())break;
                if(stopping||frames!=before||!focusActive||SystemClock.elapsedRealtime()-focusAt>2500)continue;
                nativeError=value.optInt("error");
                if(nativeError!=0)throw new IOException(nativeErrorText(nativeError));
                if(!NativeRecoveryPlan.progressLive(value.getInt("state"),value.getInt("stopped"),value.getInt("hookRemoved"),value.getInt("stopReason"),value.optInt("sessionReason")))
                    throw new IOException("Native session stopped before final snapshot");
                if(progress==NativeProgressWatch.PROGRESS_STALLED)throw new ProgressStalled(startupWorkStarted);
                throw new IOException("Native control stopped responding");
            }
            SystemClock.sleep(100);
        }
        consume();log.write("telemetry","dropped="+droppedLogs);
    }
    private JSONObject readNativeStatus(String event) throws Exception {
        File reader=new File(context.getApplicationInfo().nativeLibraryDir,mode==1?"libsilph-farmer-status.so":mode==2?"libsilph-kecleon-status.so":"libsilph-rocket-status.so");
        JSONObject value=new JSONObject(command(quote(reader.getAbsolutePath())+" "+gamePid+" "+ticks+" "+statusAddress+" "+nonceText,4000));
        log.write(event,value.toString());return value;
    }
    private void wakeOwnerForCleanup() throws Exception {
        if(!stopAcknowledged||cleanupForeground)return;
        if(readTicks(gamePid)!=ticks)throw new IOException("Game changed before native cleanup");
        cleanupForeground=true;log.write("cleanup_owner_resume","Stop acknowledged; write authorization revoked before resuming Unity");
        command("am start -n com.nianticlabs.pokemongo/com.nianticproject.holoholo.libholoholo.unity.UnityMainActivity",5000);
    }
    private String nativeErrorText(int error){return mode==3&&error==113?"Rocket battle action timed out":"Native engine error "+error;}
    private void verifyCleanup() throws Exception {
        if(!cleanup){
            if(injected&&statusAddress!=null&&readTicks(gamePid)==ticks){
                File diagnostic=new File(context.getApplicationInfo().nativeLibraryDir,mode==1?"libsilph-farmer-status.so":mode==2?"libsilph-kecleon-status.so":"libsilph-rocket-status.so");
                JSONObject value=new JSONObject(command(quote(diagnostic.getAbsolutePath())+" "+gamePid+" "+ticks+" "+statusAddress+" "+nonceText,4000));log.write("native_failure_status",value.toString());
                if(mode==1&&value.optInt("sessionReason")!=0){nativeReason=value.optInt("sessionReason");if(nativeReason==1)finishReason="Session catch limit reached";else if(nativeReason==2)finishReason="Session spin limit reached";}
                cleanup=value.optInt("stopped")==1&&value.optInt("hookRemoved")==1&&value.optInt("remaining")==0&&value.getString("created").equals(value.getString("released"))&&value.optInt("locationRemaining")==0&&value.optInt("locationRefs")==0&&value.optInt("presentationHooks",-1)==0&&value.optInt("presentationRefs",-1)==0;
                if(value.optInt("error")!=0){
                    nativeError=value.optInt("error");finishReason=nativeErrorText(nativeError);
                    if(value.optInt("locationError")!=0)finishReason+=" · Location stage "+value.optInt("locationStage")+", error "+value.optInt("locationError");
                    else if(mode!=3&&value.optInt("actionError")!=0)finishReason+=" · Action "+value.optInt("farmerState");
                }
            }
            return;
        }cleanup=false;
        File reader=new File(context.getApplicationInfo().nativeLibraryDir,mode==1?"libsilph-farmer-status.so":mode==2?"libsilph-kecleon-status.so":"libsilph-rocket-status.so");
        long end=SystemClock.elapsedRealtime()+8000;
        while(SystemClock.elapsedRealtime()<end){
            JSONObject value=new JSONObject(command(quote(reader.getAbsolutePath())+" "+gamePid+" "+ticks+" "+statusAddress+" "+nonceText,4000));
            if(value.optInt("state")==3||value.optInt("state")==4){
                log.write("native_cleanup",value.toString());
                cleanup=value.optInt("stopped")==1&&value.optInt("hookRemoved")==1&&value.optInt("remaining")==0
                        &&value.getString("created").equals(value.getString("released"))&&value.optInt("locationRemaining")==0&&value.optInt("locationRefs")==0&&(!readonly||value.optInt("gameActions")==0);
                cleanup=cleanup&&value.optInt("presentationHooks",-1)==0&&value.optInt("presentationRefs",-1)==0;
                if(!cleanup)throw new IOException("Native hook or reference cleanup incomplete");return;
            }
            SystemClock.sleep(200);
        }
        throw new IOException("Native hook removal not confirmed");
    }
    private long readTicks(int pid) throws Exception {
        if(pid<=1)return 0;
        String stat=command(NativeStartupPolicy.statCommand(pid),4000);int end=stat.lastIndexOf(')');if(end<0)return 0;
        String[] fields=stat.substring(end+1).trim().split("\\s+");if(fields.length<20)return 0;return Long.parseLong(fields[19]);
    }
    private File stage(File directory,File source) throws Exception {
        String expected=NativeArtifacts.hash(source.getName());
        File folder=new File(directory,"silph-native-check-"+mode+"-"+expected.substring(0,16));String path=folder.getAbsolutePath();
        String actual=command("if [ ! -e "+quote(path)+" ]; then mkdir "+quote(path)+" || exit; fi; [ -d "+quote(path)+" ] && [ ! -L "+quote(path)+" ] && readlink -f "+quote(path),5000);
        if(!path.equals(actual))throw new IOException("Native stage directory changed");
        for(String name:new String[]{source.getName(),"libshadowhook.so","libshadowhook_nothing.so"}){
            File from=new File(directory,name),to=new File(folder,name.equals(source.getName())?"libsilph-native-observer.so":name);String destination=to.getAbsolutePath();
            String existing=command("if [ -e "+quote(destination)+" ] || [ -L "+quote(destination)+" ]; then [ -f "+quote(destination)+" ] && [ ! -L "+quote(destination)+" ] && sha256sum "+quote(destination)+"; fi",5000);
            if(existing.isEmpty())command("cp -n "+quote(from.getAbsolutePath())+" "+quote(destination)+" && chmod 755 "+quote(destination),5000);
            else if(!existing.startsWith(NativeArtifacts.hash(name)+" "))throw new IOException("Existing native stage integrity mismatch");
            if(!command("sha256sum "+quote(destination),5000).startsWith(NativeArtifacts.hash(name)+" "))throw new IOException("Staged native component integrity mismatch");
        }
        log.write("stage",path+" retained until game exit; no external runtime");return new File(folder,"libsilph-native-observer.so");
    }
    private boolean namespaceRejected(JSONObject report){
        return report!=null&&NativeStagePolicy.namespaceRejected(report.optString("failure"),report.optString("detail"),
            report.optBoolean("restored"),report.optBoolean("detached"),report.optBoolean("libraryReleased")&&report.optBoolean("scratchReleased")&&report.optBoolean("fdClosed"),
            report.optBoolean("restartRequired",true),report.optLong("gameActions",-1));
    }
    private File stageForGame(File source) throws Exception {
        checkStop();
        File directory=new File(context.getPackageManager().getApplicationInfo(GAME,0).nativeLibraryDir).getCanonicalFile();
        String root=directory.getAbsolutePath();
        if(!NativeStagePolicy.directory(root)||!root.equals(command("[ -d "+quote(root)+" ] && [ ! -L "+quote(root)+" ] && readlink -f "+quote(root),5000)))throw new IOException("Game native directory unavailable");
        gameStageDirectory=directory;
        String observer=NativeStagePolicy.observer(mode,NativeArtifacts.hash(source.getName()));
        for(String name:new String[]{source.getName(),"libshadowhook.so","libshadowhook_nothing.so"}){
            File from=new File(source.getParentFile(),name),to=new File(directory,name.equals(source.getName())?observer:name);
            String path=to.getAbsolutePath(),expected=NativeArtifacts.hash(name);
            String existing=command("if [ -e "+quote(path)+" ] || [ -L "+quote(path)+" ]; then [ -f "+quote(path)+" ] && [ ! -L "+quote(path)+" ] && sha256sum "+quote(path)+"; fi",5000);
            if(existing.isEmpty()){
                command("cp -n "+quote(from.getAbsolutePath())+" "+quote(path)+" && chmod 755 "+quote(path),5000);
                if(!command("[ ! -L "+quote(path)+" ] && sha256sum "+quote(path),5000).startsWith(expected+" "))throw new IOException("Game stage integrity mismatch");
                gameStageFiles.put(to,expected);
            }else if(!existing.startsWith(expected+" "))throw new IOException("Game stage file conflict; export logs");
            if(!to.getParentFile().equals(directory)||!path.equals(command("readlink -f "+quote(path),5000)))throw new IOException("Game stage path changed");
        }
        log.write("namespace_stage","target="+root+" observer="+observer+" gameLibrariesReplaced=false");
        return new File(directory,observer);
    }
    private void cleanGameStage() throws Exception {
        if(gameStageFiles.isEmpty()||!command("pidof "+GAME+" || true",5000).isEmpty())return;
        for(java.util.Map.Entry<File,String> entry:new java.util.ArrayList<>(gameStageFiles.entrySet())){
            File file=entry.getKey();String path=file.getAbsolutePath();
            if(gameStageDirectory==null||!file.getParentFile().equals(gameStageDirectory)||!NativeStagePolicy.directory(gameStageDirectory.getAbsolutePath()))throw new IOException("Game stage cleanup scope invalid");
            String identity=command("[ -f "+quote(path)+" ] && [ ! -L "+quote(path)+" ] && readlink -f "+quote(path)+" && sha256sum "+quote(path),5000);
            if(!identity.startsWith(path+"\n"+entry.getValue()+" "))throw new IOException("Game stage cleanup identity changed");
            command("rm -- "+quote(path),5000);gameStageFiles.remove(file);log.write("namespace_stage_removed",path);
        }
    }
    private void startupFailureDetails(int attempt) {
        if(stopping)return;
        int pid=gamePid>1?gamePid:observedPid;
        String process=pid>1?"cat /proc/"+pid+"/status 2>&1; readlink /proc/"+pid+"/exe 2>&1; grep -E 'libil2cpp[.]so|libunity[.]so|silph|shadowhook' /proc/"+pid+"/maps 2>&1 | head -n 20":"pidof "+GAME+" || true";
        try{log.write("startup_process","attempt="+attempt+" stage="+startupStage+" pid="+pid+"\n"+command("( "+process+" ) | tail -c 16000",2500));}catch(Exception error){log.write("startup_diagnostic_error",errorText(error));}
        if(stopping)return;
        String since=new java.text.SimpleDateFormat("MM-dd HH:mm:ss.SSS",java.util.Locale.US).format(new java.util.Date(launchWallTime));
        try{log.write("failure_crash_log","attempt="+attempt+" pid="+pid+"\n"+command("logcat -b crash -d -v threadtime -T "+quote(since)+" 2>&1 | tail -c 65536",3500));}catch(Exception error){log.write("startup_diagnostic_error",errorText(error));}
    }
    private static String errorText(Throwable error){
        StringWriter text=new StringWriter();error.printStackTrace(new PrintWriter(text));String value=text.toString();return value.length()>12000?value.substring(0,12000):value;
    }
    private void checkStop() throws IOException {if(stopping)throw new IOException("Stopped by user");}
    private String command(String command,long timeout) throws IOException {
        long began=SystemClock.elapsedRealtime();
        try{
            RootShell.Result result=shell.execute(command,timeout);long cost=SystemClock.elapsedRealtime()-began;
            if(cost>750)log.write("command_delay","ms="+cost+" command="+command);
            if(result.code!=0)throw new IOException("Root command exit "+result.code+": "+result.output);
            return result.output;
        }catch(IOException error){log.write("root_command_failure","stage="+startupStage+" timeoutMs="+timeout+" elapsedMs="+(SystemClock.elapsedRealtime()-began)+" command="+command+"\n"+errorText(error));throw error;}
    }
    private static String quote(String value){return "'"+value.replace("'","'\\''")+"'";}
    private static String hash(File file) throws Exception {return NativeComponentIntegrity.hash(file);}
}
