package dev.rootcatch.nativeengine;
import org.junit.Test;
import static org.junit.Assert.*;
public final class StartupProofTest {
    private boolean valid(String session,int pid,String player,String token,int samples,long elapsed,int refs,int requests) {
        return StartupProof.valid("session_A",42,"player_A",session,pid,player,token,samples,elapsed,refs,requests,true,true);
    }
    @Test public void bindsIdentityAndZeroRequests() {
        assertTrue(valid("session_A",42,"player_A","session_A:1",21,10000,4,0));
        assertFalse(valid("session_B",42,"player_A","session_A:1",21,10000,4,0));
        assertFalse(valid("session_A",43,"player_A","session_A:1",21,10000,4,0));
        assertFalse(valid("session_A",42,"player_B","session_A:1",21,10000,4,0));
        assertFalse(valid("session_A",42,"player_A","session_A:1",21,10000,4,1));
    }
    @Test public void needsFullObservationAndBoundToken() {
        assertFalse(valid("session_A",42,"player_A","session_A:1",19,10000,4,0));
        assertFalse(valid("session_A",42,"player_A","session_A:1",21,9999,4,0));
        assertFalse(valid("session_A",42,"player_A","session_A:1",21,10000,3,0));
        assertFalse(valid("session_A",42,"player_A","wrong:1",21,10000,4,0));
        assertFalse(valid("session_A",42,"player_A","session_A:0",21,10000,4,0));
    }
    @Test public void rejectsReusedPidWithDifferentStartAndMalformedProcStat() {
        String stat = "42 (game ) worker) S " + "1 ".repeat(18) + "1406655 0 0";
        assertTrue(StartupProof.sameProcess(stat, 1406655));
        assertFalse(StartupProof.sameProcess(stat, 1406656));
        assertFalse(StartupProof.sameProcess(stat.replace("1406655", "9999999999999999999999"), 1));
        assertFalse(StartupProof.sameProcess(null, 1));
        assertFalse(StartupProof.sameProcess("short", 1));
        assertFalse(StartupProof.sameProcess(stat, 0));
    }
}
