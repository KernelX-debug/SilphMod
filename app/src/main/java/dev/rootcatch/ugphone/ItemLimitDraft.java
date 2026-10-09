package dev.rootcatch.ugphone;

import dev.rootcatch.nativeengine.EnginePolicy;
import dev.rootcatch.nativeengine.ItemCatalog;
import java.text.Normalizer;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;

final class ItemLimitDraft {
    private final Map<String, Boolean> selected = new LinkedHashMap<>();
    private final Map<String, String> floors = new LinkedHashMap<>();

    ItemLimitDraft(String limits) {
        Map<String, Integer> current = EnginePolicy.parseLimits(limits);
        for (ItemCatalog.Entry entry : ItemCatalog.ENTRIES) {
            if (!entry.discardable()) continue;
            selected.put(entry.name(), current.containsKey(entry.name()));
            floors.put(entry.name(), String.valueOf(current.getOrDefault(entry.name(), 0)));
        }
    }

    boolean selected(String name) { requireItem(name); return selected.get(name); }
    String floor(String name) { requireItem(name); return floors.get(name); }
    void select(String name, boolean enabled) { requireItem(name); selected.put(name, enabled); }
    void floor(String name, String value) { requireItem(name); floors.put(name, value); }
    int count() { return (int) selected.values().stream().filter(Boolean::booleanValue).count(); }

    String serialize() {
        StringBuilder result = new StringBuilder();
        for (ItemCatalog.Entry entry : ItemCatalog.ENTRIES) {
            if (!entry.discardable() || !selected(entry.name())) continue;
            int amount = FarmPolicy.integer(floor(entry.name()), 0, 10000, entry.label());
            if (result.length() > 0) result.append(',');
            result.append(entry.name()).append('=').append(amount);
        }
        return EnginePolicy.limitsKey(result.toString());
    }

    static boolean matches(ItemCatalog.Entry entry, String query) {
        if (!entry.discardable()) return false;
        String search = normalized(query);
        return search.isEmpty() || normalized(entry.label() + " " + entry.group() + " " + entry.name()).contains(search);
    }

    private static String normalized(String text) {
        return Normalizer.normalize(text == null ? "" : text.trim(), Normalizer.Form.NFD)
                .replaceAll("\\p{M}+", "").toLowerCase(Locale.ROOT);
    }

    private void requireItem(String name) {
        if (!selected.containsKey(name)) throw new IllegalArgumentException("Item not allowed: " + name);
    }
}
