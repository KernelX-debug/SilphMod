package dev.rootcatch.ugphone;

final class RecoveryAttempts {
    static final int MAX_ATTEMPTS = 10;
    private int attempt;
    private String playerId = "";
    void beginStartup() { attempt = 1; playerId = ""; }

    void begin(String player) {
        if (player == null || player.isEmpty()) throw new IllegalArgumentException("Missing recovery player");
        playerId = player; attempt = 1;
    }
    boolean retry() {
        if (attempt < 1 || attempt >= MAX_ATTEMPTS) return false;
        attempt++; return true;
    }
    int attempt() { return attempt; }
    String playerId() { return playerId; }
    long delayMillis() { return attempt < 2 ? 0 : Math.min(30_000L, (attempt - 1) * 5_000L); }
    void clear() { attempt = 0; playerId = ""; }
}
