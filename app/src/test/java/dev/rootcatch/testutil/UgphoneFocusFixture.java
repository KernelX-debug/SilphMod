package dev.rootcatch.testutil;

public final class UgphoneFocusFixture {
    private static final String COMPONENT = "com.nianticlabs.pokemongo/com.nianticproject.holoholo.libholoholo.unity.UnityMainActivity";
    private UgphoneFocusFixture() { }
    public static String displays(boolean shade) {
        return "Display: mDisplayId=0\n  mCurrentFocus=Window{a1 u0 " + (shade ? "StatusBar" : COMPONENT) + "}\n"
                + "  mFocusedApp=AppWindowToken{b1 token=Token{c1 ActivityRecord{d1 u0 " + COMPONENT + " t1}}}\n"
                + "    mFocusedApp=Token{c1 ActivityRecord{d1 u0 " + COMPONENT + " t1}}";
    }
    public static String full(boolean shade) {
        return "mAllowLockscreenWhenOnDisplays=false mLockScreenTimeout=2147483647 mLockScreenTimerActive=false\n  "
                + displays(shade) + "\n  Display #0\n  mTopFocusedDisplayId=0";
    }
}
