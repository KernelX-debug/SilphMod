package dev.rootcatch.nativeengine;

import java.util.List;

/** Item identifiers cross-checked against the supported game schema. */
public final class ItemCatalog {
    public static final class Entry {
        private final String name, label, group;
        private final int id;
        private final boolean discardable, special;
        public Entry(String name, int id, String label, String group, boolean discardable, boolean special) {
            this.name=name; this.id=id; this.label=label; this.group=group; this.discardable=discardable; this.special=special;
        }
        public String name() { return name; }
        public int id() { return id; }
        public String label() { return label; }
        public String group() { return group; }
        public boolean discardable() { return discardable; }
        public boolean special() { return special; }
    }
    public static final List<Entry> ENTRIES = java.util.Collections.unmodifiableList(java.util.Arrays.asList(
        new Entry("PokeBall", 1, "Poké Ball", "Poké Balls", true, false),
        new Entry("GreatBall", 2, "Great Ball", "Poké Balls", true, false),
        new Entry("UltraBall", 3, "Ultra Ball", "Poké Balls", true, false),
        new Entry("Potion", 101, "Potion", "Potions and revives", true, false),
        new Entry("SuperPotion", 102, "Super Potion", "Potions and revives", true, false),
        new Entry("HyperPotion", 103, "Hyper Potion", "Potions and revives", true, false),
        new Entry("MaxPotion", 104, "Max Potion", "Potions and revives", true, false),
        new Entry("Revive", 201, "Revive", "Potions and revives", true, false),
        new Entry("MaxRevive", 202, "Max Revive", "Potions and revives", true, false),
        new Entry("RazzBerry", 701, "Razz Berry", "Berries", true, false),
        new Entry("NanabBerry", 703, "Nanab Berry", "Berries", true, false),
        new Entry("PinapBerry", 705, "Pinap Berry", "Berries", true, false),
        new Entry("GoldenRazzBerry", 706, "Golden Razz Berry", "Berries", true, false),
        new Entry("GoldenPinapBerry", 708, "Silver Pinap Berry", "Berries", true, false),
        new Entry("SunStone", 1101, "Sun Stone", "Evolution items", true, true),
        new Entry("KingsRock", 1102, "King's Rock", "Evolution items", true, true),
        new Entry("MetalCoat", 1103, "Metal Coat", "Evolution items", true, true),
        new Entry("DragonScale", 1104, "Dragon Scale", "Evolution items", true, true),
        new Entry("UpGrade", 1105, "Up-Grade", "Evolution items", true, true),
        new Entry("Gen4EvolutionStone", 1106, "Sinnoh Stone", "Evolution items", true, true),
        new Entry("Gen5EvolutionStone", 1107, "Unova Stone", "Evolution items", true, true),
        new Entry("MoveRerollFastAttack", 1201, "Fast TM", "Technical Machines", true, true),
        new Entry("MoveRerollSpecialAttack", 1202, "Charged TM", "Technical Machines", true, true),
        new Entry("LuckyEgg", 301, "Lucky Egg", "Consumables", true, true),
        new Entry("StarPiece", 1404, "Star Piece", "Consumables", true, true),
        new Entry("Poffin", 709, "Poffin", "Consumables", true, true),
        new Entry("IncenseOrdinary", 401, "Incense", "Consumables", true, true),
        new Entry("TroyDisk", 501, "Lure Module", "Lure Modules", true, true),
        new Entry("TroyDiskGlacial", 502, "Glacial Lure Module", "Lure Modules", true, true),
        new Entry("TroyDiskMossy", 503, "Mossy Lure Module", "Lure Modules", true, true),
        new Entry("TroyDiskMagnetic", 504, "Magnetic Lure Module", "Lure Modules", true, true),
        new Entry("TroyDiskRainy", 505, "Rainy Lure Module", "Lure Modules", true, true),
        new Entry("TroyDiskSparkly", 506, "Golden Lure Module", "Lure Modules", true, true),
        new Entry("MasterBall", 4, "Master Ball", "Protected items", false, false),
        new Entry("PremierBall", 5, "Premier Ball", "Protected items", false, false),
        new Entry("WildBall", 7, "Safari Ball / WildBall", "Protected items", false, false),
        new Entry("BlukBerry", 702, "BlukBerry (internal)", "Internal items", false, false),
        new Entry("WeparBerry", 704, "WeparBerry (internal)", "Internal items", false, false),
        new Entry("GoldenNanabBerry", 707, "GoldenNanabBerry (internal)", "Internal items", false, false),
        new Entry("IncenseDailyAdventure", 406, "Daily Adventure Incense", "Protected items", false, false),
        new Entry("IncenseBelugaBox", 405, "Mystery Box / BelugaBox", "Protected items", false, false),
        new Entry("IncenseSparkly", 407, "Coin Bag / IncenseSparkly", "Protected items", false, false),
        new Entry("FreeRaidTicket", 1401, "Daily Raid Pass", "Protected items", false, false),
        new Entry("PaidRaidTicket", 1402, "Premium Battle Pass", "Protected items", false, false),
        new Entry("EnhancedCurrency", 1410, "EnhancedCurrency (internal)", "Protected items", false, false),
        new Entry("EnhancedCurrencyHolder", 1411, "EnhancedCurrencyHolder (internal)", "Protected items", false, false),
        new Entry("IncubatorBasicUnlimited", 901, "Unlimited Incubator", "Protected items", false, false),
        new Entry("IncubatorBasic", 902, "Egg Incubator", "Protected items", false, false),
        new Entry("IncubatorSuper", 903, "Super Incubator", "Protected items", false, false),
        new Entry("IncubatorTimed", 904, "Temporary Incubator", "Protected items", false, false)
    ));
    private ItemCatalog() { }
}
