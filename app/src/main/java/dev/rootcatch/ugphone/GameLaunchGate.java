package dev.rootcatch.ugphone;

final class GameLaunchGate {
    private final boolean targetRequired;
    private boolean issued, stopped;

    GameLaunchGate(boolean targetRequired) { this.targetRequired = targetRequired; }

    synchronized boolean request(boolean targetBound) {
        if (stopped || issued || targetRequired && !targetBound) return false;
        issued = true;
        return true;
    }

    synchronized void stop() { stopped = true; }
}
