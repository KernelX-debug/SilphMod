package dev.rootcatch.automation;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.TreeSet;

public final class PokemonSelection {
    public static final String KEY = "only_catch";
    public static final int MAX_DEX = 1025, MAX_KEY = 1063;
    public static final String KEEP_DEFAULT = "808,999,1052,1053,1054";
    private PokemonSelection() { }
    public static List<Integer> parse(String value) {
        if (value == null || value.trim().isEmpty()) return Collections.emptyList();
        String[] fields = value.split(",", -1);
        if (fields.length > MAX_KEY) throw new IllegalArgumentException("Invalid Only catch selection");
        TreeSet<Integer> ids = new TreeSet<>();
        for (String field : fields) {
            String text = field.trim();
            if (!text.matches("[0-9]{1,4}")) throw new IllegalArgumentException("Invalid Only catch selection");
            int id = Integer.parseInt(text);
            if (id < 1 || id > MAX_KEY) throw new IllegalArgumentException("Invalid Only catch selection");
            ids.add(id);
        }
        return Collections.unmodifiableList(new ArrayList<>(ids));
    }
    public static String encode(Iterable<Integer> selected) {
        TreeSet<Integer> ids = new TreeSet<>();
        for (Integer id : selected) {
            if (id == null || id < 1 || id > MAX_KEY) throw new IllegalArgumentException("Invalid Only catch selection");
            ids.add(id);
        }
        StringBuilder value = new StringBuilder();
        for (int id : ids) { if (value.length() > 0) value.append(','); value.append(id); }
        return value.toString();
    }
    public static boolean recoverEmptyMap(List<Integer> selected) { return selected.isEmpty(); }
}
