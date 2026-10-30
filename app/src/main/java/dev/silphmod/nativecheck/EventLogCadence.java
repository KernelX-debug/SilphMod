package dev.silphmod.nativecheck;

final class EventLogCadence {
    private long next;
    private int state,flags,error,reason,rewards,visualFailure,location,research;
    private boolean initialized;
    boolean due(long now,int state,int flags,int error,int reason,int rewards,int visualFailure,int location,int research){
        boolean changed=!initialized||this.state!=state||this.flags!=flags||this.error!=error||this.reason!=reason
            ||this.rewards!=rewards||this.visualFailure!=visualFailure||this.location!=location||this.research!=research;
        if(!changed&&now<next)return false;
        initialized=true;this.state=state;this.flags=flags;this.error=error;this.reason=reason;
        this.rewards=rewards;this.visualFailure=visualFailure;this.location=location;this.research=research;
        next=now+5000;return true;
    }
    void reset(){initialized=false;next=0;}
}
