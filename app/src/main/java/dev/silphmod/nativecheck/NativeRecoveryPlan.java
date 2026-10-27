package dev.silphmod.nativecheck;

final class NativeRecoveryPlan {
    static boolean startupFailure(boolean readonly,boolean stopped,boolean workStarted,int error,int reason,boolean transportFailure){
        return !readonly&&!stopped&&!workStarted&&error==0&&reason==0&&transportFailure;
    }
    static boolean startupRetry(boolean readonly,boolean stopped,boolean workStarted,int attempts,int error,int reason,boolean transportFailure){
        return attempts>0&&attempts<10&&startupFailure(readonly,stopped,workStarted,error,reason,transportFailure);
    }
    static boolean restart(boolean readonly,boolean stopped,boolean cleanup,int error,int reason,boolean buddyHandled){
        return !readonly&&!stopped&&cleanup&&error==0&&(reason==10||reason==11&&!buddyHandled);
    }
    static boolean progressRetry(boolean readonly,boolean stopped,int error,int reason,int recoveries){
        return !readonly&&!stopped&&error==0&&reason==0&&recoveries>=0&&recoveries<3;
    }
    static boolean progressLive(int state,int stopped,int removed,int stopReason,int sessionReason){
        return state==2&&stopped==0&&removed==0&&stopReason==0&&sessionReason==0;
    }
    static boolean transportFailure(String message,String finished){
        return "Native control disconnected".equals(finished)||"Game process changed".equals(message)
            ||"Native snapshots timed out".equals(message)||"Native control pulse failed".equals(message)
            ||"Native control stopped responding".equals(message);
    }
    static int transportFailures(int previous,long lifetime,boolean confirmedWork){
        if(previous<0||previous>10||lifetime<0)return 10;
        return confirmedWork&&lifetime>=120000?1:previous>=9?10:previous+1;
    }
    static boolean transportRetry(boolean readonly,boolean stopped,int error,int reason,int attempts,boolean recoverable,boolean recentlyActive){
        return !readonly&&!stopped&&error==0&&reason==0&&attempts>0&&attempts<10&&recoverable&&recentlyActive;
    }
    static boolean unreportedStartupExit(boolean alive,boolean intentional,boolean emptyReport,boolean confirmedWork,long lifetime){
        return !alive&&!intentional&&emptyReport&&!confirmedWork&&lifetime>=0&&lifetime<=30000;
    }
}
