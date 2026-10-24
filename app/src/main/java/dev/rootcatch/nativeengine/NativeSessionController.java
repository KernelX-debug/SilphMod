package dev.rootcatch.nativeengine;

import java.util.function.Consumer;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class NativeSessionController {
    public interface Listener {
        void status(String message);
        void update(NativeSessionWire.Snapshot snapshot,NativeSessionState.Totals totals);
        void finished(String reason,boolean cleanupConfirmed);
        default void candidate(NativeKecleonWire.Event event){ }
        default void reply(long executed){ }
    }
    private static final Pattern CONNECTED=Pattern.compile("\\{\"event\":\"native_control_connected\",\"pid\":([0-9]+),\"serverUid\":([0-9]+)\\}");
    private static final Pattern REPLY=Pattern.compile("\\{\"event\":\"native_control_reply\",\"sequence\":\"([0-9]+)\",\"command\":([0-9]+),\"status\":([0-9]+),\"state\":([0-9]+),\"reason\":([0-9]+),\"queued\":([0-9]+),\"readOnly\":([01]),\"executed\":\"([0-9]+)\",\"canceled\":\"([0-9]+)\",\"pulses\":\"([0-9]+)\"\\}");
    private static final Pattern SNAPSHOT=Pattern.compile("\\{\"event\":\"native_engine_snapshot\",\"bytes\":\"([a-f0-9]{512})\"\\}");
    private final NativeSessionWire.Config config;
    private final NativeSessionState state;
    private final Consumer<String> sender;
    private final Listener listener;
    private boolean connected,configured,finished,foreground=true;
    private int pending;
    private final java.util.ArrayDeque<String> commands=new java.util.ArrayDeque<>();
    private String inflight;
    private boolean pendingConfig;
    private long eventSequence;
    private String lastEvent="";
    private long sequence;
    private String stopReason="",lastStatus="";
    public NativeSessionController(NativeSessionWire.Config config,NativeSessionState.Totals baseline,Consumer<String> sender,Listener listener){
        if(sender==null||listener==null)throw new IllegalArgumentException("Missing native controller callbacks");
        this.config=NativeSessionWire.decodeConfig(config.encode());this.state=new NativeSessionState(this.config,baseline);this.sender=sender;this.listener=listener;
    }
    public synchronized void accept(String line){
        if(finished)return;if(line==null||line.length()>4096)throw new IllegalStateException("Invalid native controller output");
        Matcher match=CONNECTED.matcher(line);
        if(match.matches()){
            if(connected||Integer.parseInt(match.group(1))!=config.pid)throw new IllegalStateException("Native connection identity changed");
            connected=true;pendingConfig=true;send(config.command(),4);status("Configuring native engine…");return;
        }
        match=REPLY.matcher(line);
        if(match.matches()){
            long next=Long.parseLong(match.group(1));int command=Integer.parseInt(match.group(2)),result=Integer.parseInt(match.group(3));
            if(!connected||next!=sequence+1||command!=pending||command<1||command>4)throw new IllegalStateException("Unexpected native control response");
            sequence=next;pending=0;
            if(command==4){
                if(result==2){if(pendingConfig)throw new IllegalStateException("Native configuration busy");commands.addFirst(inflight);}
                else if(result!=0)throw new IllegalStateException("Native command rejected: "+result);
                else if(pendingConfig){configured=true;pendingConfig=false;}inflight=null;
            }
            else if(result!=0&&result!=2&&result!=3)throw new IllegalStateException("Native control rejected: "+result);
            listener.reply(Long.parseUnsignedLong(match.group(8)));
            return;
        }
        match=SNAPSHOT.matcher(line);
        if(match.matches()){
            if(!configured)throw new IllegalStateException("Native snapshot before configuration");
            if(state.accept(NativeSessionWire.unhex(match.group(1))))listener.update(state.snapshot(),state.totals());
            if(state.stopped()){
                finished=true;NativeSessionWire.Snapshot value=state.snapshot();
                String reason=value.reason==10?"No spawns for 5 minutes":value.reason==11?"Buddy removed":value.mode==1?(value.reason==1?"Session catch limit reached":value.reason==2?"Session spin limit reached":"Session finished"):(value.reason==5?"Maximum encounters reached":value.reason==6?"No pending Kecleon candidates":"Session finished");
                listener.finished(!stopReason.isEmpty()?stopReason:value.error!=0?"Native engine error: "+value.error:reason,state.cleanupConfirmed());
            }else if(state.ready())status(!stopReason.isEmpty()?"Stopping…":!foreground?"Paused":state.snapshot().reason==12?"Checking buddy…":state.snapshot().reason==13?"Removing buddy…":"Active");
            return;
        }
        String prefix="{\"event\":\"native_kecleon_event\",\"bytes\":\"";
        if(line.startsWith(prefix)&&line.endsWith("\"}")){
            String hex=line.substring(prefix.length(),line.length()-2);
            NativeKecleonWire.Event event=NativeKecleonWire.decode(NativeSessionWire.unhex(hex));event.bind(config);
            NativeSessionWire.Snapshot latest=state.snapshot();if(latest==null||event.accountHi!=latest.accountHi||event.accountLo!=latest.accountLo)throw new IllegalStateException("Kecleon account mismatch");
            if(event.sequence<eventSequence||event.sequence==eventSequence&&!lastEvent.equals(hex))throw new IllegalStateException("Stale Kecleon event");
            if(event.sequence>eventSequence){eventSequence=event.sequence;lastEvent=hex;listener.candidate(event);}return;
        }
        throw new IllegalStateException("Unexpected native controller output");
    }
    public synchronized void pulse(boolean foreground){
        this.foreground=foreground;if(finished||!connected||!configured||pending!=0)return;
        String command=state.command(foreground);if(command==null)return;
        if(stopReason.isEmpty()&&foreground&&!commands.isEmpty()){inflight=commands.removeFirst();send(inflight,4);return;}
        send(command,command.equals("stop")?2:command.equals("status")?3:1);
        if(!stopReason.isEmpty())status("Stopping…");else if(state.ready())status(!foreground?"Paused":state.snapshot().reason==12?"Checking buddy…":state.snapshot().reason==13?"Removing buddy…":"Active");
    }
    public synchronized void requestStop(String reason){if(!finished){if(stopReason.isEmpty())stopReason=reason==null||reason.isEmpty()?"Stopped by user":reason;state.requestStop();}}
    public synchronized void disconnected(){if(!finished){finished=true;listener.finished(!stopReason.isEmpty()?stopReason:"Native control disconnected",false);}}
    public synchronized boolean finished(){return finished;}
    public synchronized NativeSessionState.Totals totals(){return state.totals();}
    public synchronized void enqueue(String command){if(finished||!stopReason.isEmpty())return;if(command==null||!command.matches("engine [a-f0-9]{1024}")||commands.size()>=8)throw new IllegalStateException("Native command queue full or invalid");commands.addLast(command);}
    private void send(String text,int command){if(pending!=0)throw new IllegalStateException("Native request still pending");pending=command;sender.accept(text);}
    private void status(String value){if(!lastStatus.equals(value)){lastStatus=value;listener.status(value);}}
}
