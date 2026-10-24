package dev.silphmod.nativecheck;

import dev.rootcatch.nativeengine.ItemCatalog;
import java.text.Normalizer;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;

final class ItemLimitDraft {
    private final Map<String, Boolean> selected = new LinkedHashMap<>();
    private final Map<String, String> floors = new LinkedHashMap<>();

    ItemLimitDraft(String limits) {
        Map<String, Integer> current = new LinkedHashMap<>();
        if(limits!=null&&!limits.isBlank())for(String part:limits.split(",")){
            String[] pair=part.split(":");if(pair.length!=2)throw new IllegalArgumentException("Invalid item limits");
            int id=Integer.parseInt(pair[0]),floor=Integer.parseInt(pair[1]);
            ItemCatalog.Entry item=ItemCatalog.ENTRIES.stream().filter(e->e.id()==id&&e.discardable()).findFirst().orElseThrow();
            if(floor<0||floor>100000||current.put(item.name(),floor)!=null)throw new IllegalArgumentException("Invalid item limits");
        }
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
            int amount = Integer.parseInt(floor(entry.name()));
            if(amount<0||amount>100000)throw new IllegalArgumentException("Invalid item limit: "+entry.label());
            if (result.length() > 0) result.append(',');
            result.append(entry.id()).append(':').append(amount);
        }
        return result.toString();
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
