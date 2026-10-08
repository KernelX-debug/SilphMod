package dev.rootcatch.nativeengine;
import org.junit.Test;
import static org.junit.Assert.*;

public class TransferOptionsTest {
    @Test public void defaultIsOffAndBothRetentionChecksAreOn() {
        assertFalse(TransferOptions.OFF.enabled); assertTrue(TransferOptions.OFF.keepShiny);
        assertTrue(TransferOptions.OFF.keepHundo); assertEquals(0, TransferOptions.OFF.maximum);
    }
    @Test public void proofBindsEveryOption() {
        TransferOptions standard = new TransferOptions(true, true, true, 3);
        assertNotEquals(standard.proofKey(), TransferOptions.OFF.proofKey());
        assertNotEquals(standard.proofKey(), new TransferOptions(true,false,true,3).proofKey());
        assertNotEquals(standard.proofKey(), new TransferOptions(true,true,false,3).proofKey());
        assertNotEquals(standard.proofKey(), new TransferOptions(true,true,true,0).proofKey());
    }
    @Test public void invalidLimitsFailClosed() {
        assertThrows(IllegalArgumentException.class, () -> new TransferOptions(true,true,true,-1));
        assertThrows(IllegalArgumentException.class, () -> new TransferOptions(true,true,true,1001));
        assertEquals(0, new TransferOptions(true,true,true,0).maximum);
    }
}
