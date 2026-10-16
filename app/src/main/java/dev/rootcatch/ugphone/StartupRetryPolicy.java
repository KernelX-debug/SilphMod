package dev.rootcatch.ugphone;

final class StartupRetryPolicy {
    private StartupRetryPolicy() { }
    static boolean mayPlan(boolean opened, boolean cancelled, long readyAt, int attempt) {
        return opened && !cancelled && readyAt == 0 && attempt >= 1 && attempt < RecoveryAttempts.MAX_ATTEMPTS;
    }
}
