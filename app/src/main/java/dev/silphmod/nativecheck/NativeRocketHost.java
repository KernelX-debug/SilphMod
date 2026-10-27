package dev.silphmod.nativecheck;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.SystemClock;
import dev.rootcatch.nativeengine.*;
import dev.rootcatch.root.RootShell;
import dev.rootcatch.ugphone.KecleonFeed;
import dev.rootcatch.ugphone.KecleonMovement;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.BooleanSupplier;
import org.json.JSONObject;

final class NativeRocketHost implements AutoCloseable {
    private final SharedPreferences history,settings;
    private final NativeSessionController controller;
    private final CheckLog log;
    private final BooleanSupplier active;
    private final RootShell joystick=new RootShell();
    private final ExecutorService worker=Executors.newSingleThreadExecutor(r->{Thread t=new Thread(r,"silph-rocket-route");t.setDaemon(true);return t;});
    private final ScheduledExecutorService network=Executors.newSingleThreadScheduledExecutor(r->{Thread t=new Thread(r,"silph-rocket-feed");t.setDaemon(true);return t;});
    private volatile RocketFeed.Snapshot feed;
    private volatile HttpURLConnection connection;
    private volatile boolean closed;
    private volatile long retryAt;
    private NativeKecleonWire.Event event;
    private RocketFeed.Candidate candidate;
    private String account="";
    private final HashSet<String> completed=new HashSet<>(),incidents=new HashSet<>();
    private final HashMap<String,Long> retry=new HashMap<>();
    private long commandSequence,lastCatches;
    private double[] anchor;
    private double latitude,longitude;
    NativeRocketHost(Context context,NativeSessionController controller,CheckLog log,BooleanSupplier active){
        this.controller=controller;this.log=log;this.active=active;history=context.getSharedPreferences("rocket-state",0);settings=new NativeSettings(context).prefs;
        String supplied=settings.getString("rocketCooldownLocation","").trim();if(!supplied.isEmpty())anchor=KecleonFeed.parseCoordinates(supplied);
        lastCatches=controller.totals().caught;network.scheduleWithFixedDelay(this::refresh,0,RocketFeed.REFRESH_MS,TimeUnit.MILLISECONDS);
    }
    static void beginSession(Context context){SharedPreferences history=context.getSharedPreferences("rocket-state",0);var edit=history.edit();for(String key:history.getAll().keySet())if(key.startsWith("anchor."))edit.remove(key);edit.apply();}
    static void clear(Context context){context.getSharedPreferences("rocket-state",0).edit().clear().apply();new NativeSettings(context).prefs.edit().remove("rocketCooldownLocation").apply();}
    synchronized void accept(NativeKecleonWire.Event value){
        if(closed)return;
        String next=Long.toUnsignedString(value.accountHi,16)+":"+Long.toUnsignedString(value.accountLo,16);
        if(!account.isEmpty()&&!account.equals(next)){controller.requestStop("Rocket account changed");return;}
        if(account.isEmpty()){
            account=next;completed.addAll(history.getStringSet("done."+account,Collections.emptySet()));incidents.addAll(history.getStringSet("incidents."+account,Collections.emptySet()));
            String saved=history.getString("anchor."+account,"");if(anchor==null&&!saved.isEmpty())anchor=KecleonFeed.parseCoordinates(saved);
        }
        event=value;
        if(value.phase==1){latitude=anchor==null?value.latitude:anchor[0];longitude=anchor==null?value.longitude:anchor[1];}
        submit(()->process(value));
    }
    private void submit(Checked task){if(closed)return;try{worker.execute(()->{try{if(!closed)task.run();}catch(InterruptedException stopped){Thread.currentThread().interrupt();}catch(Exception error){log.write("rocket_host_error",error.toString());controller.requestStop("Rocket controller: "+error.getMessage());}});}catch(RejectedExecutionException ignored){}}
    private interface Checked {void run() throws Exception;}
    synchronized void tick(){
        if(closed)return;long caught=controller.totals().caught;
        if(caught>lastCatches&&candidate!=null){if(anchor==null){anchor=new double[]{candidate.latitude,candidate.longitude};history.edit().putString("anchor."+account,KecleonFeed.formatCoordinates(anchor[0],anchor[1])).apply();}lastCatches=caught;}
        if(event!=null&&event.phase==1&&retryAt>0&&SystemClock.elapsedRealtime()>=retryAt){retryAt=0;NativeKecleonWire.Event value=event;submit(()->process(value));}
    }
    private synchronized void send(NativeKecleonWire.Event e,int kind,int move,boolean accepted,double lat,double lon,String key){
        if(closed||event==null||event.request!=e.request)return;
        controller.enqueue(NativeKecleonWire.command(e,++commandSequence,kind,move,accepted,lat,lon,key));
        log.write("rocket_command","sequence="+commandSequence+" request="+e.request+" kind="+kind+" move="+move);
    }
    private void process(NativeKecleonWire.Event e) throws Exception {
        if(closed)return;log.write("rocket_phase","request="+e.request+" phase="+e.phase+" move="+e.move+" reason="+e.reason);
        if(e.phase==1){
            if(candidate!=null)return;RocketFeed.Snapshot snapshot=feed;
            if(snapshot==null){retryAt=SystemClock.elapsedRealtime()+1000;return;}
            candidate=snapshot.select(latitude,longitude,SystemClock.elapsedRealtime(),RocketFeed.DEFAULT_REMAINING_MS,completed,retry);
            if(candidate==null){retryAt=SystemClock.elapsedRealtime()+RocketFeed.REFRESH_MS;return;}
            send(e,1,0,false,candidate.latitude,candidate.longitude,candidate.key);return;
        }
        if(candidate==null)return;
        if(e.phase==2||e.phase==6){
            if(e.phase==6&&e.move==2){
                if(anchor!=null){send(e,1,0,false,anchor[0],anchor[1],"anchor");move(new double[][]{{anchor[0],anchor[1]}},e,"capture_anchor");}
                log.write("rocket_capture_anchor",anchor==null?"first capture at candidate":KecleonFeed.formatCoordinates(anchor[0],anchor[1]));
            }else move(KecleonMovement.points(candidate.latitude,candidate.longitude),e,e.phase==2?"candidate":"nudge");
            send(e,2,e.move,true,0,0,"");
        }else if(e.phase==4){
            if(e.fort.isBlank()||e.incident.isBlank())throw new IOException("Rocket identifiers unavailable");
            send(e,3,0,!incidents.contains(e.fort+":"+e.incident),0,0,"");
        }else if(e.phase==7){
            long caught=controller.totals().caught;
            if(caught>lastCatches&&anchor==null){anchor=new double[]{candidate.latitude,candidate.longitude};history.edit().putString("anchor."+account,KecleonFeed.formatCoordinates(anchor[0],anchor[1])).apply();}
            lastCatches=caught;
            if(e.verified==1){completed.add(candidate.key);if(!e.fort.isEmpty()&&!e.incident.isEmpty())incidents.add(e.fort+":"+e.incident);
                history.edit().putStringSet("done."+account,new HashSet<>(completed)).putStringSet("incidents."+account,new HashSet<>(incidents)).apply();
            }else retry.put(candidate.key,SystemClock.elapsedRealtime()+60000);
            candidate=null;send(e,4,0,false,0,0,"");
        }
    }
    private void move(double[][] points,NativeKecleonWire.Event e,String reason) throws Exception {
        for(int i=0;i<points.length;i++){
            while(!closed&&!active.getAsBoolean())Thread.sleep(100);if(closed)throw new InterruptedException("Stopped");
            RootShell.Result value=joystick.execute(KecleonMovement.command(points[i][0],points[i][1]),3000);
            String lower=value.output.toLowerCase(Locale.ROOT);if(value.code!=0||lower.contains("error")||lower.contains("exception")||lower.contains("not found"))throw new IOException("GPS JoyStick move rejected: "+value.output);
            log.write("rocket_move",new JSONObject().put("request",e.request).put("reason",reason).put("step",i).put("latitude",points[i][0]).put("longitude",points[i][1]).toString());
            if(i+1<points.length)Thread.sleep(KecleonMovement.STEP_MS);
        }
    }
    private void refresh(){
        if(closed)return;HttpURLConnection request=null;
        try{
            request=(HttpURLConnection)new URL(RocketFeed.url(System.currentTimeMillis())).openConnection();connection=request;
            request.setConnectTimeout(10000);request.setReadTimeout(10000);request.setInstanceFollowRedirects(false);request.setRequestProperty("User-Agent","SilphMod/"+BuildConfig.VERSION_NAME);
            int code=request.getResponseCode();if(code!=200)throw new IOException("Rocket feed HTTP "+code);
            try(InputStream input=request.getInputStream();Reader reader=new InputStreamReader(new FilterInputStream(input){long count;@Override public int read(byte[] b,int o,int n)throws IOException{int r=super.read(b,o,n);if(r>0&&(count+=r)>4*1024*1024)throw new IOException("Rocket feed too large");return r;}},StandardCharsets.UTF_8)){
                long started=SystemClock.elapsedRealtime();RocketFeed.Snapshot next=RocketFeed.read(reader,started);if(closed)return;feed=next;
                log.write("rocket_feed","rows="+next.rows+" grunts="+next.candidates.size()+" unsupported="+next.unsupported+" readMs="+(SystemClock.elapsedRealtime()-started));retryAt=SystemClock.elapsedRealtime()+100;
            }
        }catch(Exception error){if(!closed)log.write("rocket_feed_error",error.toString());}
        finally{if(request!=null)request.disconnect();connection=null;}
    }
    @Override public void close(){closed=true;HttpURLConnection current=connection;if(current!=null)current.disconnect();network.shutdownNow();worker.shutdownNow();joystick.close();}
}
