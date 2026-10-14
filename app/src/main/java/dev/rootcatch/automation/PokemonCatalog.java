package dev.rootcatch.automation;

import android.content.Context;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import java.nio.charset.StandardCharsets;
import java.text.Normalizer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

public final class PokemonCatalog {
    private static List<Entry> cached;
    public static final class Entry {
        private final int id;
        private final String name, searchName, searchNumber;
        public Entry(int id, String name) {
            this.id = id; this.name = name; searchName = normalize(name); searchNumber = String.format(Locale.ROOT, "%04d", id);
        }
        public int id() { return id; }
        public String name() { return name; }
        public boolean matches(String query) {
            return matchesKey(normalize(query));
        }
        public boolean matchesKey(String key) { return key.isEmpty() || searchName.contains(key) || searchNumber.contains(key); }
        public String artwork() { return String.format(Locale.ROOT, "pokemon/icons/%04d.png", id); }
    }
    private PokemonCatalog() { }
    public static synchronized List<Entry> load(Context context) throws IOException {
        if (cached == null) try (Reader reader = new InputStreamReader(context.getAssets().open("pokemon/catalog.tsv"), StandardCharsets.UTF_8)) {
            cached = read(reader);
        }
        return cached;
    }
    public static List<Entry> read(Reader input) throws IOException {
        List<Entry> entries = new ArrayList<>();
        BufferedReader reader = new BufferedReader(input);
        String line; int previous = 0;
        while ((line = reader.readLine()) != null) {
            String[] fields = line.split("\t", -1);
            if (fields.length != 2 || !fields[0].matches("[0-9]{1,4}") || fields[1].trim().isEmpty()) throw new IOException("Invalid Pokémon catalog");
            int id = Integer.parseInt(fields[0]);
            if (id <= previous || id > PokemonSelection.MAX_DEX) throw new IOException("Invalid Pokédex order");
            entries.add(new Entry(id, fields[1])); previous = id;
        }
        if (entries.isEmpty()) throw new IOException("Empty Pokémon catalog");
        return Collections.unmodifiableList(entries);
    }
    public static String normalize(String value) {
        return Normalizer.normalize(value == null ? "" : value, Normalizer.Form.NFD).replaceAll("\\p{M}", "")
                .replaceAll("[^\\p{L}\\p{N}♀♂]", "").toLowerCase(Locale.ROOT);
    }
}
