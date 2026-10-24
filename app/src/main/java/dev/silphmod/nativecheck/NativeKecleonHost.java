package dev.silphmod.nativecheck;

import android.content.Context;
import dev.rootcatch.nativeengine.*;
import dev.rootcatch.ugphone.KecleonHost;
import java.util.function.BooleanSupplier;
import org.json.JSONObject;

final class NativeKecleonHost implements AutoCloseable {
    private final KecleonHost host;
    private final NativeSessionController controller;
    private final CheckLog log;
    private NativeKecleonWire.Event event;
    private long commandSequence,retryAt;
    private boolean closed;
    NativeKecleonHost(Context context,NativeSessionController controller,CheckLog log,BooleanSupplier active){
        this.controller=controller;this.log=log;host=new KecleonHost(context,this::reply,row->log.write("kecleon_host",row.toString()),active);
    }
    synchronized void accept(NativeKecleonWire.Event value) throws Exception {
        if(closed)return;event=value;retryAt=0;host.bindAccount("native:"+Long.toUnsignedString(value.accountHi,16)+":"+Long.toUnsignedString(value.accountLo,16));
        JSONObject row=new JSONObject().put("requestId",value.request).put("token","kecleon_"+value.request).put("moveId",value.move);
        String kind=switch(value.phase){case 1->"kecleon_need_target";case 2->"kecleon_teleport_request";case 4->"kecleon_stop_resolved";case 6->"kecleon_nudge_request";case 7->"kecleon_candidate_done";default->"";};
        if(kind.isEmpty())return;row.put("event",kind).put("fortId",value.fort).put("incidentId",value.incident).put("verified",value.verified==1).put("reason","native:"+value.reason);
        log.write("kecleon_phase","request="+value.request+" phase="+value.phase+" reason="+value.reason+" verified="+value.verified);host.accept(row);
    }
    synchronized void tick() throws Exception {if(!closed&&retryAt>0&&System.currentTimeMillis()>=retryAt){retryAt=0;host.accept(new JSONObject().put("event","kecleon_need_target").put("requestId",event.request));}}
    private synchronized void reply(JSONObject row) throws Exception {
        if(closed||event==null)return;String type=row.getString("type");int kind,move=0;boolean accepted=false;double lat=0,lon=0;String source="";
        switch(type){
            case "kecleon_target"->{if(row.getLong("requestId")!=event.request||event.phase!=1)return;JSONObject candidate=row.getJSONObject("candidate");kind=1;lat=candidate.getDouble("latitude");lon=candidate.getDouble("longitude");source=candidate.getString("sourceKey");}
            case "kecleon_navigation_done","kecleon_nudge_done"->{if(!row.getString("token").equals("kecleon_"+event.request))return;kind=2;move=row.getInt("moveId");accepted=row.getBoolean("ok");}
            case "kecleon_stop_authorized"->{if(!row.getString("token").equals("kecleon_"+event.request))return;kind=3;accepted=row.getBoolean("ok");}
            case "kecleon_result_saved"->{if(row.getLong("requestId")!=event.request)return;kind=4;}
            case "kecleon_empty"->kind=5;
            case "kecleon_retry_wait"->{retryAt=Math.max(System.currentTimeMillis()+1000,row.getLong("retryAt"));return;}
            default->{controller.requestStop("Kecleon host: "+row.optString("message",type));return;}
        }
        controller.enqueue(NativeKecleonWire.command(event,++commandSequence,kind,move,accepted,lat,lon,source));
    }
    @Override public void close(){synchronized(this){closed=true;}host.close();}
}
