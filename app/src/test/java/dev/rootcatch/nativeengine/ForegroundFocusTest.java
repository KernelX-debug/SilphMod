package dev.rootcatch.nativeengine;

import org.junit.Test;
import static org.junit.Assert.*;

public class ForegroundFocusTest {
    private static final String GAME = "mCurrentFocus=Window{abc u0 com.nianticlabs.pokemongo/com.nianticlabs.Main}";
    private static final String OTHER = "mCurrentFocus=Window{def u0 com.other/Main}";

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
        assertEquals(Boolean.FALSE, result.focusedValue()); assertEquals("com.other", result.windowOwner);
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
    @Test public void statusBarOverFreshGameAppIsNotAnotherApplication() {
        String output = "Display: mDisplayId=0\nmCurrentFocus=Window{7e87d3 u0 StatusBar}\n"
                + "mFocusedApp=AppWindowToken{aa token=Token{bb ActivityRecord{cc u0 "
                + EnginePolicy.GAME + "/.Main t81}}}";
        ForegroundFocus result = ForegroundFocus.parse(output);
        assertTrue(result.isGame()); assertTrue(result.notificationShadeAllowed());
        assertEquals("notification_shade_over_game", result.reason); assertEquals("StatusBar", result.windowOwner);
    }
    @Test public void shadeNeverUsesPreviousGameOrAnAppOnAnotherDisplay() {
        for (String app : new String[]{"null", "ActivityRecord{cc u0 com.other/.Main t81}",
                "ActivityRecord{cc u0 " + EnginePolicy.GAME + ".fake/.Main t81}", EnginePolicy.GAME}) {
            ForegroundFocus result = ForegroundFocus.parse("mCurrentFocus=Window{abc u0 StatusBar}\nmFocusedApp=" + app);
            assertFalse(result.isGame()); assertFalse(result.notificationShadeAllowed());
        }
        ForegroundFocus result = ForegroundFocus.parse("Display: mDisplayId=1\n" + GAME
                + "\nmFocusedApp=ActivityRecord{cc u0 " + EnginePolicy.GAME + "/.Main t81}\n"
                + "Display: mDisplayId=0\nmCurrentFocus=Window{abc u0 StatusBar}\nmTopFocusedDisplayId=0");
        assertFalse(result.isGame());
    }
    @Test public void onlyNotificationWindowsMayUseTheUnderlyingAppProof() {
        String app = "\nmFocusedApp=ActivityRecord{cc u0 " + EnginePolicy.GAME + "/.Main t81}";
        for (String window : new String[]{"StatusBar", "NotificationShade", "com.android.systemui/NotificationShade"})
            assertTrue(ForegroundFocus.parse("mCurrentFocus=Window{abc u0 " + window + "}" + app).notificationShadeAllowed());
        for (String window : new String[]{"NavigationBar", "Keyguard", "com.android.systemui/RecentsActivity", "com.other/Main"})
            assertFalse(ForegroundFocus.parse("mCurrentFocus=Window{abc u0 " + window + "}" + app).isGame());
    }
    @Test public void gamePackageInAnotherActivityTitleDoesNotAuthorizeShade() {
        ForegroundFocus result = ForegroundFocus.parse("mCurrentFocus=Window{abc u0 StatusBar}\n"
                + "mFocusedApp=ActivityRecord{cc u0 com.other/.Main " + EnginePolicy.GAME + " t81}");
        assertFalse(result.isGame());
    }
    @Test public void conflictingFocusedAppRecordsCannotAuthorizeShade() {
        ForegroundFocus result = ForegroundFocus.parse("mCurrentFocus=Window{abc u0 StatusBar}\n"
                + "mFocusedApp=ActivityRecord{cc u0 " + EnginePolicy.GAME + "/.Main t81}\n"
                + "mFocusedApp=ActivityRecord{dd u0 com.other/.Main t82}");
        assertEquals(ForegroundFocus.State.AMBIGUOUS, result.state);
    }
    @Test public void actualUgphoneShapeKeepsTheVisibleGameDespiteNestedDuplicateAppDescriptions() {
        ForegroundFocus result = ForegroundFocus.parse(dev.rootcatch.testutil.UgphoneFocusFixture.displays(false));
        assertTrue(result.isGame()); assertTrue(result.initialAttachAllowed());
        assertFalse(result.notificationShadeAllowed()); assertEquals("current_window", result.reason);
    }
    @Test public void actualUgphoneShadeShapeAllowsContinuityButNeverInitialAttach() {
        ForegroundFocus result = ForegroundFocus.parse(dev.rootcatch.testutil.UgphoneFocusFixture.displays(true));
        assertTrue(result.isGame()); assertTrue(result.notificationShadeAllowed());
        assertFalse(result.initialAttachAllowed());
    }
    @Test public void actualFullDumpMayDescribeOneDisplayInTwoDifferentHeaderFormats() {
        for (boolean shade : new boolean[]{false, true}) {
            ForegroundFocus result = ForegroundFocus.parse(dev.rootcatch.testutil.UgphoneFocusFixture.full(shade));
            assertTrue(result.isGame()); assertEquals(0, result.displayId);
            assertEquals(shade, result.notificationShadeAllowed());
        }
        assertEquals(ForegroundFocus.State.AMBIGUOUS, ForegroundFocus.parse(
                dev.rootcatch.testutil.UgphoneFocusFixture.full(false) + "\nDisplay #0").state);
    }
    @Test public void repeatedAppProofMustMatchIdentityUserOwnerAndComponentNotJustPackage() {
        String first = "ActivityRecord{cc u0 " + EnginePolicy.GAME + "/.Main t81}";
        for (String second : new String[]{first.replace("{cc", "{dd"), first.replace("u0", "u1"),
                first.replace("/.Main", "/.AnotherActivity"), first.replace(EnginePolicy.GAME, "com.other"), "null", "invalid"}) {
            ForegroundFocus result = ForegroundFocus.parse("mCurrentFocus=Window{abc u0 StatusBar}\n"
                    + "mFocusedApp=" + first + "\nmFocusedApp=" + second);
            assertEquals(second, ForegroundFocus.State.AMBIGUOUS, result.state);
            assertEquals("conflicting_focused_app", result.reason); assertFalse(result.initialAttachAllowed());
        }
    }
    @Test public void focusedAppAmbiguityCannotHideAConfirmedCurrentWindowOrAuthorizeAnotherWindow() {
        String conflict = "\nmFocusedApp=ActivityRecord{cc u0 " + EnginePolicy.GAME + "/.Main t81}\n"
                + "mFocusedApp=ActivityRecord{dd u0 com.other/.Main t82}";
        assertTrue(ForegroundFocus.parse(GAME + conflict).initialAttachAllowed());
        assertEquals(ForegroundFocus.State.OTHER, ForegroundFocus.parse(OTHER + conflict).state);
    }
    @Test public void invalidFirstAppAndMultipleRecordsCannotBeRepairedByAnotherRecordForTheShade() {
        String valid = "ActivityRecord{cc u0 " + EnginePolicy.GAME + "/.Main t81}";
        for (String first : new String[]{"null", "invalid", valid + " " + valid}) {
            assertFalse(ForegroundFocus.parse("mCurrentFocus=Window{abc u0 StatusBar}\nmFocusedApp="
                    + first + "\nmFocusedApp=" + valid).isGame());
        }
    }
}
