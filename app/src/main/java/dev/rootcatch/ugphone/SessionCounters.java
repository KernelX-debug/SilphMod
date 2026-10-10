package dev.rootcatch.ugphone;

import java.util.Locale;

final class SessionCounters {
    final int caught, fled, spun, discarded, transferred;
    final long xp, stardust, missingRewardReceipts;
    final boolean rewardsAvailable;
    SessionCounters() { this(0, 0, 0, 0); }
    SessionCounters(int caught, int fled, int spun, int discarded) { this(caught, fled, spun, discarded, 0); }
    SessionCounters(int caught, int fled, int spun, int discarded, int transferred) {
        this(caught, fled, spun, discarded, transferred, 0, 0, 0, false);
    }
    SessionCounters(int caught, int fled, int spun, int discarded, int transferred,
                    long xp, long stardust, long missing, boolean available) {
        this.caught = Math.max(0, caught); this.fled = Math.max(0, fled);
        this.spun = Math.max(0, spun); this.discarded = Math.max(0, discarded);
        this.transferred = Math.max(0, transferred);
        this.xp = Math.max(0, xp); this.stardust = Math.max(0, stardust);
        this.missingRewardReceipts = Math.max(0, missing); this.rewardsAvailable = available;
    }
    SessionCounters update(int caught, int fled, int spun, int discarded) {
        return new SessionCounters(Math.max(this.caught, caught), Math.max(this.fled, fled),
                Math.max(this.spun, spun), Math.max(this.discarded, discarded), transferred,
                xp, stardust, missingRewardReceipts, rewardsAvailable);
    }
    SessionCounters transfers(int value) {
        return new SessionCounters(caught, fled, spun, discarded, Math.max(transferred, value),
                xp, stardust, missingRewardReceipts, rewardsAvailable);
    }
    SessionCounters rewards(long experience, long dust, long missing, boolean available) {
        return new SessionCounters(caught, fled, spun, discarded, transferred,
                Math.max(xp, experience), Math.max(stardust, dust), Math.max(missingRewardReceipts, missing), rewardsAvailable || available);
    }
    static int add(int base, int value) { return (int) Math.min(Integer.MAX_VALUE, (long) base + Math.max(0, value)); }
    static long add(long base, long value) { return base > Long.MAX_VALUE - Math.max(0, value) ? Long.MAX_VALUE : base + Math.max(0, value); }
    SessionCounters merge(SessionCounters value) {
        return update(value.caught, value.fled, value.spun, value.discarded).transfers(value.transferred)
                .rewards(value.xp, value.stardust, value.missingRewardReceipts, value.rewardsAvailable);
    }
    String rewardText(long value) {
        return !rewardsAvailable ? "—" : String.format(Locale.ROOT, "%,d", value) + (missingRewardReceipts > 0 ? "+" : "");
    }
    String rewardLine() { return "XP " + rewardText(xp) + " · Stardust " + rewardText(stardust); }
    String compact() { return "Catches " + caught + " · Spins " + spun + " · Items " + discarded + " · Transferred " + transferred; }
    String detailed() { return compact() + "\n" + rewardLine() + "\nFled " + fled + " · confirmed results only"; }
    static String elapsed(long milliseconds) {
        long seconds = Math.max(0, milliseconds) / 1000;
        return String.format(Locale.ROOT, "%02d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
    }
}
