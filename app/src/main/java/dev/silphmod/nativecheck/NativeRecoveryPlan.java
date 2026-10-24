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
}
