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
        private final int id, dex, form;
        private final String name, searchName, searchNumber;
        public Entry(int id, String name) {
            this(id, id, 0, name);
        }
        public Entry(int id, int dex, int form, String name) {
            this.id = id; this.dex = dex; this.form = form; this.name = name; searchName = normalize(name); searchNumber = String.format(Locale.ROOT, "%04d", dex);
        }
        public int id() { return id; }
        public int dex() { return dex; }
        public int form() { return form; }
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
            List<Entry> entries = new ArrayList<>(read(reader));
            try (BufferedReader forms = new BufferedReader(new InputStreamReader(context.getAssets().open("pokemon/forms.tsv"), StandardCharsets.UTF_8))) {
                String line;
                while ((line = forms.readLine()) != null) {
                    String[] fields = line.split("\t", -1);
                    if (fields.length != 4) throw new IOException("Invalid Pokémon forms");
                    int key = Integer.parseInt(fields[0]), dex = Integer.parseInt(fields[1]), form = Integer.parseInt(fields[2]);
                    if (key <= PokemonSelection.MAX_DEX || key > PokemonSelection.MAX_KEY || dex < 1 || dex > PokemonSelection.MAX_DEX || form <= 0) throw new IOException("Invalid Pokémon form");
                    entries.add(new Entry(key, dex, form, fields[3]));
                }
            }
            entries.sort(java.util.Comparator.comparingInt(Entry::dex).thenComparingInt(Entry::form));
            cached = Collections.unmodifiableList(entries);
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
