package dev.rootcatch.ugphone;

import android.content.Context;
import dev.rootcatch.root.RootShell;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.File;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;
import org.json.JSONObject;

public final class KecleonHost implements AutoCloseable {
    public interface Sender { void send(JSONObject message) throws Exception; }
    private final KecleonHistory history;
    private final Sender sender;
    private final Consumer<JSONObject> logger;
    private final BooleanSupplier active;
    private final File cache;
    private final RootShell joystick=new RootShell();
    private final ThreadPoolExecutor worker=new ThreadPoolExecutor(1,1,0,TimeUnit.SECONDS,new ArrayBlockingQueue<>(8),
            task->{Thread thread=new Thread(task,"silph-kecleon-io");thread.setDaemon(true);return thread;}){
        @Override protected void terminated(){if(candidates!=null)candidates.close();history.close();}
    };
    private volatile HttpURLConnection connection;
    private volatile boolean closed;
    private String account="",token="",key="";
    private KecleonFeed.Candidate current;
    private KecleonQueue candidates;
    private final Map<String,KecleonFeed.Candidate> deferred=new LinkedHashMap<>();
    public KecleonHost(Context context,Sender sender,Consumer<JSONObject> logger,BooleanSupplier active){
        history=new KecleonHistory(context);this.sender=sender;this.logger=logger;this.active=active;
        cache=context.getCacheDir();
    }
    public synchronized void bindAccount(String player){
        String next=KecleonHistory.hash(player);
        if(!account.isEmpty()&&!account.equals(next))throw new IllegalStateException("Kecleon account changed");
        account=next;
    }
    public void accept(JSONObject event){
        if(closed||account.isEmpty())return;
        String type=event.optString("event");
        if(!java.util.Arrays.asList("kecleon_need_target","kecleon_teleport_request","kecleon_nudge_request","kecleon_stop_resolved","kecleon_candidate_done").contains(type))return;
        try{worker.execute(()->{
            try{process(type,event);}catch(Exception error){
                log("kecleon_host_error",error.toString());
                try{if(!closed)sender.send(new JSONObject().put("type","kecleon_host_error").put("message",error.toString()));}catch(Exception ignored){}
            }
        });}catch(java.util.concurrent.RejectedExecutionException error){
            try{sender.send(new JSONObject().put("type","kecleon_host_error").put("message","Candidate controller busy"));}catch(Exception ignored){}
        }
    }
    private void process(String kind,JSONObject event) throws Exception {
        if(closed)return;
        if(kind.equals("kecleon_need_target")){
            long request=event.getLong("requestId");if(request<1)throw new IllegalArgumentException("Invalid candidate request");
            if(current!=null)throw new IllegalStateException("Previous candidate not finalized");
            if(candidates==null){candidates=fetch();log("kecleon_feed_loaded","candidates="+candidates.size());}
            KecleonFeed.Candidate value;
            while((value=candidates.next())!=null){
                String candidate=KecleonHistory.key(value);
                if(!history.claim(account,candidate)){
                    if(history.retryAt(account,candidate)!=Long.MAX_VALUE)deferred.put(candidate,value);
                    continue;
                }
                current=value;key=candidate;token="kecleon_"+request;
                if(!closed)sender.send(new JSONObject().put("type","kecleon_target").put("requestId",request).put("token",token)
                        .put("candidate",new JSONObject().put("pokemonId",352).put("candidateId",value.id).put("latitude",value.latitude)
                                .put("longitude",value.longitude).put("sourceKey",key).put("sourceStart",value.start).put("sourceEnd",value.end)));
                return;
            }
            long retryAt=Long.MAX_VALUE,now=System.currentTimeMillis();
            for(java.util.Iterator<Map.Entry<String,KecleonFeed.Candidate>> it=deferred.entrySet().iterator();it.hasNext();){
                Map.Entry<String,KecleonFeed.Candidate> entry=it.next();long next=history.retryAt(account,entry.getKey());
                if(next==Long.MAX_VALUE){it.remove();continue;}
                if(next>now){retryAt=Math.min(retryAt,next);continue;}
                if(!history.claim(account,entry.getKey())){it.remove();continue;}
                current=entry.getValue();key=entry.getKey();token="kecleon_"+request;it.remove();
                if(!closed)sender.send(new JSONObject().put("type","kecleon_target").put("requestId",request).put("token",token)
                    .put("candidate",new JSONObject().put("pokemonId",352).put("candidateId",current.id).put("latitude",current.latitude)
                        .put("longitude",current.longitude).put("sourceKey",key).put("sourceStart",current.start).put("sourceEnd",current.end)));
                return;
            }
            if(!closed)sender.send(new JSONObject().put("type",retryAt==Long.MAX_VALUE?"kecleon_empty":"kecleon_retry_wait")
                .put("requestId",request).put("retryAt",retryAt));return;
        }
        if(current==null||!event.optString("token").equals(token))throw new IllegalStateException("Stale candidate token");
        if(kind.equals("kecleon_teleport_request")||kind.equals("kecleon_nudge_request")){
            boolean initial=kind.equals("kecleon_teleport_request");int moveId=initial?0:event.getInt("moveId");
            if(!initial&&(moveId<1||moveId>2))throw new IllegalArgumentException("Invalid map movement cycle");
            int steps=0;
            double[][] points=KecleonMovement.points(current.latitude,current.longitude);
            for(double[] point:points){
                while(!closed&&!active.getAsBoolean())Thread.sleep(100);
                if(closed)return;
                RootShell.Result reply=joystick.execute(KecleonMovement.command(point[0],point[1]),3000);
                logger.accept(new JSONObject().put("event","kecleon_navigation_step").put("token",token).put("step",steps)
                        .put("time",System.currentTimeMillis()).put("moveId",moveId).put("initial",initial).put("radiusMeters",KecleonMovement.RADIUS_METERS)
                        .put("latitude",point[0]).put("longitude",point[1]).put("exit",reply.code).put("output",reply.output));
                String lower=reply.output.toLowerCase(Locale.ROOT);
                if(reply.code!=0||lower.contains("error")||lower.contains("exception")||lower.contains("not found"))
                    throw new IllegalStateException("GPS JoyStick teleport rejected: "+reply.output);
                steps++;if(steps<points.length)Thread.sleep(KecleonMovement.STEP_MS);
            }
            if(!closed)sender.send(new JSONObject().put("type",initial?"kecleon_navigation_done":"kecleon_nudge_done").put("token",token).put("moveId",moveId).put("ok",true));
        }else if(kind.equals("kecleon_stop_resolved")){
            String fort=event.getString("fortId"),incident=event.getString("incidentId");
            if(!fort.matches("[a-zA-Z0-9_.:-]{1,160}")||incident.isEmpty()||incident.length()>160)
                throw new IllegalArgumentException("Invalid resolved incidence");
            boolean accepted=history.incident(account,fort,incident,key);
            if(!closed)sender.send(new JSONObject().put("type","kecleon_stop_authorized").put("token",token).put("ok",accepted));
        }else{
            boolean verified=event.optBoolean("verified",false);
            history.finish(account,key,event.optString("reason","processed"),verified);
            if(!verified)deferred.put(key,current);
            current=null;token="";key="";
            if(!closed)sender.send(new JSONObject().put("type","kecleon_result_saved").put("requestId",event.getLong("requestId")));
        }
    }
    private KecleonQueue fetch() throws Exception {
        HttpURLConnection request=(HttpURLConnection)new URL(KecleonFeed.URL).openConnection();connection=request;
        request.setConnectTimeout(15000);request.setReadTimeout(15000);request.setInstanceFollowRedirects(false);
        request.setRequestProperty("User-Agent","SilphMod/1.9.5");
        try{
            int code=request.getResponseCode();if(code!=200)throw new IllegalStateException("Moonani HTTP "+code);
            KecleonQueue queue=new KecleonQueue(cache);boolean complete=false;
            try(InputStream input=request.getInputStream();InputStreamReader reader=new InputStreamReader(input,StandardCharsets.UTF_8)){
                KecleonFeed.Scan scan=KecleonFeed.read(reader,queue::add,()->closed);
                queue.ready();if(closed)throw new InterruptedException("Stopped");
                log("kecleon_feed_scan","characters="+scan.characters+" rows="+scan.rows+" candidates="+scan.candidates+" oversizedRows="+scan.oversizedRows);
                complete=true;return queue;
            }finally{if(!complete)queue.close();}
        }finally{request.disconnect();connection=null;}
    }
    private void log(String event,String detail){try{logger.accept(new JSONObject().put("event",event).put("detail",detail));}catch(Exception ignored){}}
    public static void clearHistory(Context context){try(KecleonHistory history=new KecleonHistory(context)){history.clearAll();}}
    @Override public void close(){
        closed=true;HttpURLConnection request=connection;if(request!=null)request.disconnect();
        worker.shutdownNow();joystick.close();
        try{worker.awaitTermination(2000,TimeUnit.MILLISECONDS);}catch(InterruptedException error){Thread.currentThread().interrupt();}
    }
}
