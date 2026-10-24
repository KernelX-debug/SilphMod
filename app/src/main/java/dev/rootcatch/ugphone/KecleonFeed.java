package dev.rootcatch.ugphone;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class KecleonFeed {
    public static final String URL = "https://moonani.com/PokeList/quest.php";
    private static final Pattern ROW = Pattern.compile("<tr\\b[^>]*>.*?</tr>", Pattern.CASE_INSENSITIVE | Pattern.DOTALL);
    private static final Pattern CELL = Pattern.compile("<td\\b[^>]*>(.*?)</td>", Pattern.CASE_INSENSITIVE | Pattern.DOTALL);
    private static final Pattern HIDDEN = Pattern.compile("<!--\\s*<td>\\d+</td>\\s*<td>([a-fA-F0-9]{32})</td>\\s*-->", Pattern.CASE_INSENSITIVE);
    private static final Pattern COPY = Pattern.compile("data-clipboard-text\\s*=\\s*[\"']([^\"']+)[\"']", Pattern.CASE_INSENSITIVE);
    private static final Pattern COORDS = Pattern.compile("\\s*(-?\\d{1,3}(?:\\.\\d{1,12})?)\\s*,\\s*(-?\\d{1,3}(?:\\.\\d{1,12})?)\\s*");
    public static final class Candidate {
        public final String id, coordinates, start, end, country;
        public final double latitude, longitude;
        Candidate(String id, String coords, String start, String end, String country) {
            this.id = id; this.latitude = parseCoordinates(coords)[0]; this.longitude = parseCoordinates(coords)[1];
            coordinates = formatCoordinates(latitude, longitude);
            this.start = start; this.end = end; this.country = country;
        }
        public String label() { return (country.isEmpty() ? "—" : country.toUpperCase(Locale.ROOT)) + " · " + coordinates + "\nEnd: " + end; }
    }
    private KecleonFeed() { }
    public static String formatCoordinates(double latitude, double longitude) {
        if (!Double.isFinite(latitude) || !Double.isFinite(longitude) || Math.abs(latitude)>90 || Math.abs(longitude)>180)
            throw new IllegalArgumentException("Coordinates out of range");
        return java.math.BigDecimal.valueOf(latitude).stripTrailingZeros().toPlainString()+","
                +java.math.BigDecimal.valueOf(longitude).stripTrailingZeros().toPlainString();
    }
    public static double[] parseCoordinates(String value) {
        if (value == null || value.length() > 64) throw new IllegalArgumentException("Enter latitude,longitude");
        Matcher match = COORDS.matcher(value);
        if (!match.matches()) throw new IllegalArgumentException("Enter latitude,longitude");
        double lat = Double.parseDouble(match.group(1)), lon = Double.parseDouble(match.group(2));
        if (!Double.isFinite(lat) || !Double.isFinite(lon) || Math.abs(lat) > 90 || Math.abs(lon) > 180)
            throw new IllegalArgumentException("Coordinates out of range");
        return new double[]{lat, lon};
    }
    public static String normalizeId(String id) {
        String value = id == null ? "" : id.trim();
        if (!value.isEmpty() && !value.matches("[a-fA-F0-9]{32}")) throw new IllegalArgumentException("Candidate ID must contain 32 hexadecimal characters");
        return value.toLowerCase(Locale.ROOT);
    }
    private static String text(String value) {
        return value.replaceAll("(?s)<[^>]*>", " ").replace("&nbsp;", " ").replace("&#160;", " ")
                .replace("&amp;", "&").replace("&quot;", "\"").replace("&#39;", "'").replaceAll("\\s+", " ").trim();
    }
    public static List<Candidate> parse(String html) {
        if (html == null || html.length() > 1_048_576) throw new IllegalArgumentException("Candidate page too large");
        List<Candidate> result = new ArrayList<>(); LinkedHashSet<String> seen = new LinkedHashSet<>();
        Matcher rows = ROW.matcher(html); int visited = 0;
        while (rows.find() && visited++ < 1500 && result.size() < 500) {
            String row = rows.group(); Matcher hidden = HIDDEN.matcher(row);
            String id = hidden.find() ? hidden.group(1).toLowerCase(Locale.ROOT) : "";
            String clean = row.replaceAll("(?s)<!--.*?-->", "");
            Matcher cells = CELL.matcher(clean); List<String> columns = new ArrayList<>();
            while (cells.find() && columns.size() < 8) columns.add(cells.group(1));
            if (columns.size() != 7 || !text(columns.get(0)).equalsIgnoreCase("Kecleon")
                    || !text(columns.get(1)).equals("352") || !text(columns.get(2)).equalsIgnoreCase("Kecleon")) continue;
            Matcher copy = COPY.matcher(columns.get(3)); String coords = copy.find() ? copy.group(1) : text(columns.get(3));
            try {
                Candidate candidate = new Candidate(id, coords, text(columns.get(4)), text(columns.get(5)), text(columns.get(6)));
                if (seen.add(id + ":" + candidate.coordinates)) result.add(candidate);
            } catch (IllegalArgumentException ignored) { }
        }
        return result;
    }
}
