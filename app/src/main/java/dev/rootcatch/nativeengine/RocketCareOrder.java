package dev.rootcatch.nativeengine;

public final class RocketCareOrder {
    private RocketCareOrder() { }
    public static int[] parse(String value, boolean revives) {
        if (value == null || value.isBlank()) return new int[0];
        String[] parts = value.split(",", -1);
        int count = revives ? 2 : 4, first = revives ? 201 : 101;
        if (parts.length != count) throw new IllegalArgumentException("Invalid medicine priority");
        int[] order = new int[count];
        for (int i = 0; i < count; i++) {
            order[i] = Integer.parseInt(parts[i].trim());
            if (order[i] < first || order[i] >= first + count) throw new IllegalArgumentException("Invalid medicine priority");
            for (int j = 0; j < i; j++) if (order[i] == order[j]) throw new IllegalArgumentException("Duplicate medicine priority");
        }
        return order;
    }
    public static String encode(int[] order) {
        StringBuilder text = new StringBuilder();
        for (int item : order) { if (text.length() != 0) text.append(','); text.append(item); }
        return text.toString();
    }
}
