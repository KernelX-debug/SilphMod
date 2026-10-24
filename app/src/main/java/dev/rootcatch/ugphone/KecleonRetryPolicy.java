package dev.rootcatch.ugphone;

final class KecleonRetryPolicy {
    static final int MAX_ATTEMPTS=3;
    static final long DELAY_MS=120000;
    static boolean unverified(String state){
        return state!=null&&(state.startsWith("unverified:")||state.equals("Map loading timed out; candidate not verified"));
    }
    static long retryAt(String state,int attempts,long updated){
        return unverified(state)&&attempts<MAX_ATTEMPTS?updated+DELAY_MS:Long.MAX_VALUE;
    }
    private KecleonRetryPolicy(){}
}
