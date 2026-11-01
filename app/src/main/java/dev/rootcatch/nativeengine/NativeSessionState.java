package dev.rootcatch.nativeengine;

import java.util.Arrays;

public final class NativeSessionState {
    public static final class Totals {
        public final long caught,fled,spun,discarded,transferred,encounters,shinies,xp,stardust;
        public Totals(long caught,long fled,long spun,long discarded,long transferred,long encounters,long shinies,long xp,long stardust){
            if(caught<0||fled<0||spun<0||discarded<0||transferred<0||encounters<0||shinies<0||xp<0||stardust<0)throw new IllegalArgumentException("Invalid native session totals");
            this.caught=caught;this.fled=fled;this.spun=spun;this.discarded=discarded;this.transferred=transferred;this.encounters=encounters;this.shinies=shinies;this.xp=xp;this.stardust=stardust;
        }
        public Totals(){this(0,0,0,0,0,0,0,0,0);}
    }
    private final NativeSessionWire.Config config;
    private final Totals baseline;
    private NativeSessionWire.Snapshot latest;
    private byte[] lastFrame;
    private boolean stopRequested,stopSent;
    private long accountHi,accountLo;
    private long observedXp,observedDust;
    public NativeSessionState(NativeSessionWire.Config config,Totals baseline){
        if(baseline==null)throw new IllegalArgumentException("Missing baseline");this.config=NativeSessionWire.decodeConfig(config.encode());this.baseline=baseline;
    }
    public synchronized boolean accept(byte[] frame){
        NativeSessionWire.Snapshot next=NativeSessionWire.decodeSnapshot(frame);next.bind(config);
        if(next.sequence<0)throw new IllegalStateException("Native sequence exhausted");
        if(latest!=null){
            if(next.sequence<latest.sequence)throw new IllegalStateException("Stale native snapshot");
            if(next.sequence==latest.sequence){if(Arrays.equals(lastFrame,frame))return false;if(next.sequence!=0)throw new IllegalStateException("Native snapshot changed without sequence");}
            if(stopped()&&!Arrays.equals(lastFrame,frame))throw new IllegalStateException("Native session already stopped");
            monotonic(latest.caught,next.caught);monotonic(latest.fled,next.fled);monotonic(latest.spun,next.spun);monotonic(latest.discarded,next.discarded);
            monotonic(latest.transferred,next.transferred);monotonic(latest.encounters,next.encounters);monotonic(latest.shinies,next.shinies);
            monotonic(latest.xp,next.xp);monotonic(latest.stardust,next.stardust);
        }
        long[] counters={next.caught,next.fled,next.spun,next.discarded,next.transferred,next.encounters,next.shinies,next.berries,next.throwsCount,next.xp,next.stardust};
        for(long count:counters)if(count<0)throw new IllegalStateException("Native counter overflow");
        if(next.transferred>next.caught||next.caught>next.encounters||next.shinies>next.encounters)throw new IllegalStateException("Inconsistent native counts");
        if(config.mode==NativeSessionWire.FARMER&&config.maxCatches>0&&next.caught>config.maxCatches||config.maxSpins>0&&next.spun>config.maxSpins
                ||config.mode==NativeSessionWire.KECLEON&&config.maxEncounters>0&&next.encounters>config.maxEncounters||config.mode==NativeSessionWire.ROCKET&&config.maxEncounters>0&&next.spun>config.maxEncounters)throw new IllegalStateException("Native limit exceeded");
        if((config.flags&NativeSessionWire.READ_ONLY)!=0){
            if((next.flags&1)==0&&next.state!=0)throw new IllegalStateException("Native read-only state changed");
            for(long count:counters)if(count!=0)throw new IllegalStateException("Native read-only actions reported");
        }
        if((next.flags&2)!=0&&(next.accountHi==0&&next.accountLo==0))throw new IllegalStateException("Native account not bound");
        if(accountHi!=0||accountLo!=0){if(next.accountHi!=accountHi||next.accountLo!=accountLo)throw new IllegalStateException("Native account changed");}
        else if(next.accountHi!=0||next.accountLo!=0){accountHi=next.accountHi;accountLo=next.accountLo;}
        if((next.flags&8)!=0&&next.state!=5&&next.state!=6||(next.state==5||next.state==6)&&(next.flags&8)==0)throw new IllegalStateException("Invalid native stop state");
        if((next.rewardsAvailable&1)!=0)observedXp=next.xp;if((next.rewardsAvailable&2)!=0)observedDust=next.stardust;
        latest=next;lastFrame=frame.clone();return true;
    }
    public synchronized Totals totals(){
        if(latest==null)return baseline;
        return new Totals(add(baseline.caught,latest.caught),add(baseline.fled,latest.fled),add(baseline.spun,latest.spun),add(baseline.discarded,latest.discarded),
                add(baseline.transferred,latest.transferred),add(baseline.encounters,latest.encounters),add(baseline.shinies,latest.shinies),
                add(baseline.xp,observedXp),add(baseline.stardust,observedDust));
    }
    public synchronized NativeSessionWire.Snapshot snapshot(){return latest;}
    public synchronized void requestStop(){stopRequested=true;}
    public synchronized String command(boolean foreground){
        if(stopped())return null;if(stopRequested){if(stopSent)return "status";stopSent=true;return "stop";}return foreground?"active":"paused";
    }
    public synchronized boolean ready(){return latest!=null&&(latest.flags&2)!=0;}
    public synchronized boolean stopped(){return latest!=null&&(latest.flags&8)!=0;}
    public synchronized boolean cleanupConfirmed(){return stopped()&&(latest.flags&4)!=0;}
    private static void monotonic(long before,long after){if(after<before)throw new IllegalStateException("Native statistics regressed");}
    private static long add(long a,long b){return a>Long.MAX_VALUE-b?Long.MAX_VALUE:a+b;}
}
