package dev.rootcatch.ugphone;

import dev.rootcatch.R;

final class ItemArtwork {
    private ItemArtwork() { }
    static int resource(int id) {
        return switch (id) {
            case 1 -> R.drawable.item_0001;
            case 2 -> R.drawable.item_0002;
            case 3 -> R.drawable.item_0003;
            case 101 -> R.drawable.item_0101;
            case 102 -> R.drawable.item_0102;
            case 103 -> R.drawable.item_0103;
            case 104 -> R.drawable.item_0104;
            case 201 -> R.drawable.item_0201;
            case 202 -> R.drawable.item_0202;
            case 301 -> R.drawable.item_0301;
            case 401 -> R.drawable.item_0401;
            case 501 -> R.drawable.item_0501;
            case 502 -> R.drawable.item_0502;
            case 503 -> R.drawable.item_0503;
            case 504 -> R.drawable.item_0504;
            case 505 -> R.drawable.item_0505;
            case 506 -> R.drawable.item_0506;
            case 701 -> R.drawable.item_0701;
            case 703 -> R.drawable.item_0703;
            case 705 -> R.drawable.item_0705;
            case 706 -> R.drawable.item_0706;
            case 708 -> R.drawable.item_0708;
            case 709 -> R.drawable.item_0709;
            case 1101 -> R.drawable.item_1101;
            case 1102 -> R.drawable.item_1102;
            case 1103 -> R.drawable.item_1103;
            case 1104 -> R.drawable.item_1104;
            case 1105 -> R.drawable.item_1105;
            case 1106 -> R.drawable.item_1106;
            case 1107 -> R.drawable.item_1107;
            case 1201 -> R.drawable.item_1201;
            case 1202 -> R.drawable.item_1202;
            case 1404 -> R.drawable.item_1404;
            default -> throw new IllegalArgumentException("No artwork for item: " + id);
        };
    }
}
