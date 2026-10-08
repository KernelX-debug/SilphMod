package dev.rootcatch.nativeengine;

import org.junit.Test;
import static org.junit.Assert.*;

public class ForegroundFocusTest {
    private static final String GAME = "mCurrentFocus=Window{abc u0 com.nianticlabs.pokemongo/com.nianticlabs.Main}";
    private static final String OTHER = "mCurrentFocus=Window{def u0 com.android.systemui/NotificationShade}";

    @Test public void android15WindowsOnlyRegressionIsMissingNotAnotherApp() {
        // Only the header and focus-related line from the friend's windows dump; no personal data.
        ForegroundFocus result = ForegroundFocus.parse("WINDOW MANAGER WINDOWS (dumpsys window windows)\n  mTopFocusedDisplayId=0\n");
        assertEquals(ForegroundFocus.State.UNAVAILABLE, result.state);
        assertEquals("current_focus_field_absent", result.reason);
        assertNull(result.focusedValue()); assertFalse(result.isGame());
    }
    @Test public void modernSingleDisplayRecognizesGame() {
        ForegroundFocus result = ForegroundFocus.parse("  Display: mDisplayId=0 (organized)\n    " + GAME);
        assertTrue(result.isGame()); assertEquals(0, result.displayId);
        assertEquals(EnginePolicy.GAME, result.windowOwner); assertEquals(Boolean.TRUE, result.focusedValue());
    }
    @Test public void legacyGlobalFocusWorksWithoutDisplayHeaders() {
        assertTrue(ForegroundFocus.parse(GAME).isGame());
        assertTrue(ForegroundFocus.parse(GAME + "\nmTopFocusedDisplayId=0").isGame());
    }
    @Test public void anotherWindowIsAConfirmedNegative() {
        ForegroundFocus result = ForegroundFocus.parse(OTHER);
        assertEquals(ForegroundFocus.State.OTHER, result.state);
        assertEquals(Boolean.FALSE, result.focusedValue()); assertEquals("com.android.systemui", result.windowOwner);
    }
    @Test public void explicitNullIsNotMissingData() {
        ForegroundFocus result = ForegroundFocus.parse("mCurrentFocus=null");
        assertEquals(ForegroundFocus.State.NO_WINDOW, result.state);
        assertEquals(Boolean.FALSE, result.focusedValue());
    }
    @Test public void resumedActivityAndFocusedAppAreNotCurrentWindow() {
        assertEquals(ForegroundFocus.State.UNAVAILABLE, ForegroundFocus.parse(
                "mResumedActivity=" + EnginePolicy.GAME + "\nmFocusedApp=" + EnginePolicy.GAME).state);
    }
    @Test public void malformedWindowCannotAuthorizeAttach() {
        for (String value : new String[]{"", "Window{abc u0}", "Window{abc u0 " + EnginePolicy.GAME,
                "Window{abc u0 " + EnginePolicy.GAME + "/Main} trailing", "garbage"}) {
            assertEquals(value, ForegroundFocus.State.AMBIGUOUS, ForegroundFocus.parse("mCurrentFocus=" + value).state);
        }
    }
    @Test public void duplicateCurrentFocusIsAmbiguousEvenIfBothMatch() {
        assertEquals(ForegroundFocus.State.AMBIGUOUS, ForegroundFocus.parse(GAME + "\n" + GAME).state);
        assertEquals(ForegroundFocus.State.AMBIGUOUS, ForegroundFocus.parse(GAME + "\n" + OTHER).state);
    }
    @Test public void multipleDisplaysWithoutTopIdentityAreAmbiguous() {
        assertEquals(ForegroundFocus.State.AMBIGUOUS, ForegroundFocus.parse(
                "Display: mDisplayId=1\n" + OTHER + "\nDisplay: mDisplayId=0\n" + GAME).state);
    }
    @Test public void topFocusedDisplaySelectsItsWindowNotFirstOrLastWindow() {
        String displays = "Display: mDisplayId=1\n" + GAME + "\nDisplay: mDisplayId=0\n" + OTHER;
        assertEquals(ForegroundFocus.State.GAME, ForegroundFocus.parse(displays + "\nmTopFocusedDisplayId=1").state);
        assertEquals(ForegroundFocus.State.OTHER, ForegroundFocus.parse(displays + "\nmTopFocusedDisplayId=0").state);
    }
    @Test public void missingTopDisplayWindowCannotAuthorizeAttach() {
        assertEquals(ForegroundFocus.State.UNAVAILABLE, ForegroundFocus.parse(
                "Display: mDisplayId=0\n" + GAME + "\nmTopFocusedDisplayId=2").state);
    }
    @Test public void multipleHeadersAndOnlyOneWindowNeedTopIdentity() {
        assertEquals(ForegroundFocus.State.AMBIGUOUS, ForegroundFocus.parse(
                "Display: mDisplayId=0\n" + GAME + "\nDisplay: mDisplayId=1").state);
    }
    @Test public void duplicateOrOverflowingDisplayIdsFailClosed() {
        for (String output : new String[]{"Display: mDisplayId=2147483648\n" + GAME,
                "Display: mDisplayId=unknown\n" + GAME,
                "Display: mDisplayId=0\n" + GAME + "\nDisplay: mDisplayId=0"})
            assertEquals(ForegroundFocus.State.AMBIGUOUS, ForegroundFocus.parse(output).state);
    }
    @Test public void invalidAndDuplicateTopIdsFailClosed() {
        for (String value : new String[]{"-1", "2147483648", "zero", "0\nmTopFocusedDisplayId=0"})
            assertEquals(ForegroundFocus.State.AMBIGUOUS, ForegroundFocus.parse(GAME + "\nmTopFocusedDisplayId=" + value).state);
    }
    @Test public void packagePrefixAndWindowTitleDoNotMasqueradeAsGame() {
        assertFalse(ForegroundFocus.parse(GAME.replace(EnginePolicy.GAME, EnginePolicy.GAME + ".fake")).isGame());
        assertFalse(ForegroundFocus.parse("mCurrentFocus=Window{abc u0 com.other/Main " + EnginePolicy.GAME + "}").isGame());
    }
    @Test public void legacyDisplayHeaderAndCrLfWork() {
        ForegroundFocus result = ForegroundFocus.parse("Display #0:\r\n  " + GAME + "\r\n");
        assertTrue(result.isGame()); assertEquals(0, result.displayId);
    }
    @Test public void emptyAndNullAreUnavailable() {
        assertEquals(ForegroundFocus.State.UNAVAILABLE, ForegroundFocus.parse(null).state);
        assertEquals(ForegroundFocus.State.UNAVAILABLE, ForegroundFocus.parse(" \n").state);
    }
}
