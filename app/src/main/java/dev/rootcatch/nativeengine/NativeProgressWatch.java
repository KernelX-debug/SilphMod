package dev.rootcatch.nativeengine;

public final class NativeProgressWatch {
    public static final int WAIT=0,CHANNEL_STALLED=1,PROGRESS_STALLED=2;
    private long lastSnapshot,lastReply,lastCheck,activeWait,executed;
    public NativeProgressWatch(long now){lastSnapshot=lastReply=lastCheck=now;}
    public synchronized void snapshot(long now){lastSnapshot=now;activeWait=0;}
    public synchronized void reply(long now,long executed){lastReply=now;this.executed=executed;}
    public synchronized int check(long now,boolean active){
        long elapsed=Math.max(0,Math.min(2500,now-lastCheck));lastCheck=now;
        if(!active){activeWait=0;return WAIT;}
        activeWait+=elapsed;
        if(now-lastSnapshot<65_000||activeWait<65_000)return WAIT;
        return now-lastReply<=5000?PROGRESS_STALLED:CHANNEL_STALLED;
    }
    public synchronized String details(long now){return "snapshotAgeMs="+(now-lastSnapshot)+" replyAgeMs="+(now-lastReply)+" foregroundWaitMs="+activeWait+" executed="+executed;}
}
