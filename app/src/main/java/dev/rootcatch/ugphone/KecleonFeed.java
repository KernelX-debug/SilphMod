package dev.rootcatch.ugphone;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.io.Reader;
import java.util.function.BooleanSupplier;

public final class KecleonFeed {
    public static final String URL = "https://moonani.com/PokeList/quest.php";
    private static final int MAX_ROW_CHARS=65536,MAX_TAG_CHARS=4096;
    private static final Pattern TAG=Pattern.compile("^<\\s*(/?)\\s*([a-zA-Z0-9]+)(?=[\\s/>])");
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
    public interface Sink {void accept(Candidate candidate) throws IOException;}
    public static final class Scan {
        public long characters,rows,candidates,oversizedRows;
    }
    private static void row(String row,Sink sink,Scan scan) throws IOException {
            Matcher hidden = HIDDEN.matcher(row);
            String id = hidden.find() ? hidden.group(1).toLowerCase(Locale.ROOT) : "";
            String clean = row.replaceAll("(?s)<!--.*?-->", "");
            Matcher cells = CELL.matcher(clean); List<String> columns = new ArrayList<>();
            while (cells.find() && columns.size() < 8) columns.add(cells.group(1));
            if (columns.size() != 7 || !text(columns.get(0)).equalsIgnoreCase("Kecleon")
                    || !text(columns.get(1)).equals("352") || !text(columns.get(2)).equalsIgnoreCase("Kecleon")) return;
            Matcher copy = COPY.matcher(columns.get(3)); String coords = copy.find() ? copy.group(1) : text(columns.get(3));
            Candidate candidate;
            try {
                candidate = new Candidate(id, coords, text(columns.get(4)), text(columns.get(5)), text(columns.get(6)));
            } catch (IllegalArgumentException ignored) {return;}
            if(candidate.start.length()>128||candidate.end.length()>128||candidate.country.length()>128)return;
            sink.accept(candidate);scan.candidates++;
    }
    public static Scan read(Reader input,Sink sink,BooleanSupplier cancelled) throws IOException {
        if(input==null||sink==null||cancelled==null)throw new IllegalArgumentException("Invalid candidate reader");
        Scan scan=new Scan();StringBuilder tag=new StringBuilder(),row=null;char[] buffer=new char[8192];
        boolean inTag=false,comment=false,tableSeen=false,largeTag=false;int quote=0,dashes=0,tables=0,raw=0,count;
        while(true){
            if(cancelled.getAsBoolean()||Thread.currentThread().isInterrupted())throw new InterruptedIOException("Stopped");
            count=input.read(buffer);if(count==-1)break;
            if(cancelled.getAsBoolean()||Thread.currentThread().isInterrupted())throw new InterruptedIOException("Stopped");
            scan.characters+=count;
            for(int i=0;i<count;i++){
                char c=buffer[i];
                if(row!=null){if(row.length()<MAX_ROW_CHARS)row.append(c);else{row=null;scan.oversizedRows++;}}
                if(raw!=0){
                    if(c=='<'){tag.setLength(0);tag.append(c);inTag=true;largeTag=false;}
                    else if(inTag){
                        if(tag.length()<MAX_TAG_CHARS)tag.append(c);else largeTag=true;
                        if(c=='>'){inTag=false;Matcher match=TAG.matcher(tag);if(!largeTag&&match.find()&&!match.group(1).isEmpty()&&match.group(2).equalsIgnoreCase(raw==1?"script":"style"))raw=0;}
                    }
                    continue;
                }
                if(comment){if(c=='>'&&dashes>=2){comment=false;dashes=0;}else dashes=c=='-'?Math.min(2,dashes+1):0;continue;}
                if(!inTag){if(c=='<'){inTag=true;tag.setLength(0);tag.append(c);largeTag=false;quote=0;}continue;}
                if(tag.length()<MAX_TAG_CHARS)tag.append(c);else largeTag=true;
                if(tag.length()==4&&tag.toString().equals("<!--")){comment=true;dashes=0;inTag=false;continue;}
                if(quote!=0){if(c==quote)quote=0;continue;}
                if(c=='\''||c=='"'){quote=c;continue;}
                if(c!='>')continue;
                inTag=false;if(largeTag)continue;
                Matcher match=TAG.matcher(tag);if(!match.find())continue;boolean end=!match.group(1).isEmpty();String name=match.group(2);
                if(!end&&(name.equalsIgnoreCase("script")||name.equalsIgnoreCase("style"))){raw=name.equalsIgnoreCase("script")?1:2;continue;}
                if(name.equalsIgnoreCase("table")){if(end){tables=Math.max(0,tables-1);if(tables==0)row=null;}else{tables++;tableSeen=true;}}
                else if(name.equalsIgnoreCase("tr")&&tables>0){
                    if(end){if(row!=null){scan.rows++;row(row.toString(),sink,scan);row=null;}}
                    else row=new StringBuilder(tag);
                }
            }
        }
        if(cancelled.getAsBoolean()||Thread.currentThread().isInterrupted())throw new InterruptedIOException("Stopped");
        if(!tableSeen)throw new IOException("Moonani returned an unexpected page");
        return scan;
    }
}
