// Item identifiers cross-checked against the 0.433.0 Item enum.
// This allowlist does not authorize unknown or protected items for deletion.
const CATALOG = Object.freeze([
  {
    "name": "PokeBall",
    "id": 1,
    "label": "Poké Ball",
    "group": "Poké Balls",
    "discardable": true,
    "special": false
  },
  {
    "name": "GreatBall",
    "id": 2,
    "label": "Great Ball",
    "group": "Poké Balls",
    "discardable": true,
    "special": false
  },
  {
    "name": "UltraBall",
    "id": 3,
    "label": "Ultra Ball",
    "group": "Poké Balls",
    "discardable": true,
    "special": false
  },
  {
    "name": "Potion",
    "id": 101,
    "label": "Potion",
    "group": "Potions and revives",
    "discardable": true,
    "special": false
  },
  {
    "name": "SuperPotion",
    "id": 102,
    "label": "Super Potion",
    "group": "Potions and revives",
    "discardable": true,
    "special": false
  },
  {
    "name": "HyperPotion",
    "id": 103,
    "label": "Hyper Potion",
    "group": "Potions and revives",
    "discardable": true,
    "special": false
  },
  {
    "name": "MaxPotion",
    "id": 104,
    "label": "Max Potion",
    "group": "Potions and revives",
    "discardable": true,
    "special": false
  },
  {
    "name": "Revive",
    "id": 201,
    "label": "Revive",
    "group": "Potions and revives",
    "discardable": true,
    "special": false
  },
  {
    "name": "MaxRevive",
    "id": 202,
    "label": "Max Revive",
    "group": "Potions and revives",
    "discardable": true,
    "special": false
  },
  {
    "name": "RazzBerry",
    "id": 701,
    "label": "Razz Berry",
    "group": "Berries",
    "discardable": true,
    "special": false
  },
  {
    "name": "NanabBerry",
    "id": 703,
    "label": "Nanab Berry",
    "group": "Berries",
    "discardable": true,
    "special": false
  },
  {
    "name": "PinapBerry",
    "id": 705,
    "label": "Pinap Berry",
    "group": "Berries",
    "discardable": true,
    "special": false
  },
  {
    "name": "GoldenRazzBerry",
    "id": 706,
    "label": "Golden Razz Berry",
    "group": "Berries",
    "discardable": true,
    "special": false
  },
  {
    "name": "GoldenPinapBerry",
    "id": 708,
    "label": "Silver Pinap Berry",
    "group": "Berries",
    "discardable": true,
    "special": false
  },
  {
    "name": "SunStone",
    "id": 1101,
    "label": "Sun Stone",
    "group": "Evolution items",
    "discardable": true,
    "special": true
  },
  {
    "name": "KingsRock",
    "id": 1102,
    "label": "King's Rock",
    "group": "Evolution items",
    "discardable": true,
    "special": true
  },
  {
    "name": "MetalCoat",
    "id": 1103,
    "label": "Metal Coat",
    "group": "Evolution items",
    "discardable": true,
    "special": true
  },
  {
    "name": "DragonScale",
    "id": 1104,
    "label": "Dragon Scale",
    "group": "Evolution items",
    "discardable": true,
    "special": true
  },
  {
    "name": "UpGrade",
    "id": 1105,
    "label": "Up-Grade",
    "group": "Evolution items",
    "discardable": true,
    "special": true
  },
  {
    "name": "Gen4EvolutionStone",
    "id": 1106,
    "label": "Sinnoh Stone",
    "group": "Evolution items",
    "discardable": true,
    "special": true
  },
  {
    "name": "Gen5EvolutionStone",
    "id": 1107,
    "label": "Unova Stone",
    "group": "Evolution items",
    "discardable": true,
    "special": true
  },
  {
    "name": "MoveRerollFastAttack",
    "id": 1201,
    "label": "Fast TM",
    "group": "Technical Machines",
    "discardable": true,
    "special": true
  },
  {
    "name": "MoveRerollSpecialAttack",
    "id": 1202,
    "label": "Charged TM",
    "group": "Technical Machines",
    "discardable": true,
    "special": true
  },
  {
    "name": "LuckyEgg",
    "id": 301,
    "label": "Lucky Egg",
    "group": "Consumables",
    "discardable": true,
    "special": true
  },
  {
    "name": "StarPiece",
    "id": 1404,
    "label": "Star Piece",
    "group": "Consumables",
    "discardable": true,
    "special": true
  },
  {
    "name": "Poffin",
    "id": 709,
    "label": "Poffin",
    "group": "Consumables",
    "discardable": true,
    "special": true
  },
  {
    "name": "IncenseOrdinary",
    "id": 401,
    "label": "Incense",
    "group": "Consumables",
    "discardable": true,
    "special": true
  },
  {
    "name": "TroyDisk",
    "id": 501,
    "label": "Lure Module",
    "group": "Lure Modules",
    "discardable": true,
    "special": true
  },
  {
    "name": "TroyDiskGlacial",
    "id": 502,
    "label": "Glacial Lure Module",
    "group": "Lure Modules",
    "discardable": true,
    "special": true
  },
  {
    "name": "TroyDiskMossy",
    "id": 503,
    "label": "Mossy Lure Module",
    "group": "Lure Modules",
    "discardable": true,
    "special": true
  },
  {
    "name": "TroyDiskMagnetic",
    "id": 504,
    "label": "Magnetic Lure Module",
    "group": "Lure Modules",
    "discardable": true,
    "special": true
  },
  {
    "name": "TroyDiskRainy",
    "id": 505,
    "label": "Rainy Lure Module",
    "group": "Lure Modules",
    "discardable": true,
    "special": true
  },
  {
    "name": "TroyDiskSparkly",
    "id": 506,
    "label": "Sparkly Lure (internal name)",
    "group": "Lure Modules",
    "discardable": true,
    "special": true
  },
  {
    "name": "MasterBall",
    "id": 4,
    "label": "Master Ball",
    "group": "Protected items",
    "discardable": false,
    "special": false
  },
  {
    "name": "PremierBall",
    "id": 5,
    "label": "Premier Ball",
    "group": "Protected items",
    "discardable": false,
    "special": false
  },
  {
    "name": "WildBall",
    "id": 7,
    "label": "Safari Ball / WildBall",
    "group": "Protected items",
    "discardable": false,
    "special": false
  },
  {
    "name": "BlukBerry",
    "id": 702,
    "label": "BlukBerry (internal)",
    "group": "Internal items",
    "discardable": false,
    "special": false
  },
  {
    "name": "WeparBerry",
    "id": 704,
    "label": "WeparBerry (internal)",
    "group": "Internal items",
    "discardable": false,
    "special": false
  },
  {
    "name": "GoldenNanabBerry",
    "id": 707,
    "label": "GoldenNanabBerry (internal)",
    "group": "Internal items",
    "discardable": false,
    "special": false
  },
  {
    "name": "IncenseDailyAdventure",
    "id": 406,
    "label": "Daily Adventure Incense",
    "group": "Protected items",
    "discardable": false,
    "special": false
  },
  {
    "name": "IncenseBelugaBox",
    "id": 405,
    "label": "Mystery Box / BelugaBox",
    "group": "Protected items",
    "discardable": false,
    "special": false
  },
  {
    "name": "IncenseSparkly",
    "id": 407,
    "label": "Coin Bag / IncenseSparkly",
    "group": "Protected items",
    "discardable": false,
    "special": false
  },
  {
    "name": "FreeRaidTicket",
    "id": 1401,
    "label": "Daily Raid Pass",
    "group": "Protected items",
    "discardable": false,
    "special": false
  },
  {
    "name": "PaidRaidTicket",
    "id": 1402,
    "label": "Premium Battle Pass",
    "group": "Protected items",
    "discardable": false,
    "special": false
  },
  {
    "name": "EnhancedCurrency",
    "id": 1410,
    "label": "EnhancedCurrency (internal)",
    "group": "Protected items",
    "discardable": false,
    "special": false
  },
  {
    "name": "EnhancedCurrencyHolder",
    "id": 1411,
    "label": "EnhancedCurrencyHolder (internal)",
    "group": "Protected items",
    "discardable": false,
    "special": false
  },
  {
    "name": "IncubatorBasicUnlimited",
    "id": 901,
    "label": "Unlimited Incubator",
    "group": "Protected items",
    "discardable": false,
    "special": false
  },
  {
    "name": "IncubatorBasic",
    "id": 902,
    "label": "Egg Incubator",
    "group": "Protected items",
    "discardable": false,
    "special": false
  },
  {
    "name": "IncubatorSuper",
    "id": 903,
    "label": "Super Incubator",
    "group": "Protected items",
    "discardable": false,
    "special": false
  },
  {
    "name": "IncubatorTimed",
    "id": 904,
    "label": "Temporary Incubator",
    "group": "Protected items",
    "discardable": false,
    "special": false
  }
].map(Object.freeze));
const ITEMS = Object.freeze(Object.fromEntries(CATALOG.filter(x=>x.discardable).map(x=>[x.name,x.id])));
module.exports={CATALOG,ITEMS};
