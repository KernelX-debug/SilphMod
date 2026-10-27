package dev.silphmod.nativecheck;

import android.util.JsonReader;
import android.util.JsonToken;
import java.io.IOException;
import java.io.Reader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

public final class RocketFeed {
    public static final long REFRESH_MS=30000;
    public static final long DEFAULT_REMAINING_MS=180000;
    public static final String URL="https://nycpokemap.com/pokestop.php";

    public static final class Candidate {
        public final double latitude,longitude;
        public final long startsMs,expiresMs;
        public final int character;
        public final String key;
        private Candidate(double lat,double lon,long start,long end,int character) {
            latitude=lat;longitude=lon;startsMs=start;expiresMs=end;this.character=character;
            key="nyc:"+Math.round(lat*1000000)+":"+Math.round(lon*1000000)+":"+start+":"+end+":"+character;
        }
    }
    public static final class Snapshot {
        public final long serverTimeMs,receivedElapsedMs;
        public final int rows,unsupported,invalid,duplicates;
        public final List<Candidate> candidates;
        private Snapshot(long serverTime,long received,int rows,int unsupported,int invalid,int duplicates,List<Candidate> candidates) {
            serverTimeMs=serverTime;receivedElapsedMs=received;this.rows=rows;this.unsupported=unsupported;
            this.invalid=invalid;this.duplicates=duplicates;
            this.candidates=Collections.unmodifiableList(new ArrayList<>(candidates));
        }
        public long nowMs(long elapsedMs) {
            if(elapsedMs<receivedElapsedMs)return -1;
            long age=elapsedMs-receivedElapsedMs;
            return age>Long.MAX_VALUE-serverTimeMs?-1:serverTimeMs+age;
        }
        public Candidate select(double latitude,double longitude,long elapsedMs,long minimumRemainingMs,
            Set<String> completed,Map<String,Long> retryAfter) {
            if(!coordinate(latitude,longitude)||minimumRemainingMs<0||completed==null||retryAfter==null)return null;
            long now=nowMs(elapsedMs);if(now<0)return null;
            Candidate best=null;double distance=Double.POSITIVE_INFINITY;
            for(Candidate candidate:candidates) {
                Long retry=retryAfter.get(candidate.key);
                if(candidate.startsMs>now||candidate.expiresMs<=now||candidate.expiresMs-now<minimumRemainingMs
                    ||completed.contains(candidate.key)||(retry!=null&&retry>elapsedMs))continue;
                double value=distanceSquared(latitude,longitude,candidate.latitude,candidate.longitude);
                if(value<distance||(value==distance&&(best==null||candidate.expiresMs<best.expiresMs))) {
                    best=candidate;distance=value;
                }
            }
            return best;
        }
    }
    private RocketFeed() {}
    public static String url(long wallTimeMs) {
        if(wallTimeMs<0)throw new IllegalArgumentException("Invalid time");
        return URL+"?time="+wallTimeMs;
    }
    public static boolean normalGrunt(int character) {
        return (character>=4&&character<=39)||(character>=47&&character<=50);
    }
    private static boolean coordinate(double lat,double lon) {
        return Double.isFinite(lat)&&Double.isFinite(lon)&&lat>=-90&&lat<=90&&lon>=-180&&lon<=180;
    }
    private static double distanceSquared(double lat,double lon,double targetLat,double targetLon) {
        double y=Math.toRadians(targetLat-lat),x=Math.toRadians(targetLon-lon)*Math.cos(Math.toRadians((targetLat+lat)*0.5));
        return x*x+y*y;
    }
    private static double number(JsonReader reader) throws IOException {
        if(reader.peek()!=JsonToken.NUMBER){reader.skipValue();return Double.NaN;}
        return reader.nextDouble();
    }
    private static long integer(JsonReader reader) throws IOException {
        double value=number(reader);
        return Double.isFinite(value)&&value>=0&&value<=4102444800L&&value==Math.rint(value)?(long)value:-1;
    }
    public static Snapshot read(Reader input,long receivedElapsedMs) throws IOException {
        if(input==null||receivedElapsedMs<0)throw new IllegalArgumentException("Invalid reader");
        JsonReader reader=new JsonReader(input);reader.setLenient(false);
        ArrayList<Candidate> candidates=new ArrayList<>();HashSet<String> keys=new HashSet<>();
        long server=-1;int rows=0,unsupported=0,invalid=0,duplicates=0;boolean seenMeta=false,seenRows=false;
        reader.beginObject();
        while(reader.hasNext()) {
            String field=reader.nextName();
            if(field.equals("meta")) {
                if(seenMeta)throw new IOException("Duplicate metadata");seenMeta=true;
                reader.beginObject();
                while(reader.hasNext()){if(reader.nextName().equals("time"))server=integer(reader);else reader.skipValue();}
                reader.endObject();
            }else if(field.equals("invasions")) {
                if(seenRows)throw new IOException("Duplicate candidates");seenRows=true;
                reader.beginArray();
                while(reader.hasNext()) {
                    if(++rows>25000)throw new IOException("Candidate limit exceeded");
                    if(reader.peek()!=JsonToken.BEGIN_OBJECT){reader.skipValue();invalid++;continue;}
                    double lat=Double.NaN,lon=Double.NaN;long start=-1,end=-1,character=-1,type=-1;
                    reader.beginObject();
                    while(reader.hasNext()) {
                        switch(reader.nextName()) {
                            case "lat":lat=number(reader);break;
                            case "lng":lon=number(reader);break;
                            case "invasion_start":start=integer(reader);break;
                            case "invasion_end":end=integer(reader);break;
                            case "character":character=integer(reader);break;
                            case "type":type=integer(reader);break;
                            default:reader.skipValue();
                        }
                    }
                    reader.endObject();
                    if(type!=1||character>Integer.MAX_VALUE||!normalGrunt((int)character)){unsupported++;continue;}
                    if(!coordinate(lat,lon)||start<1577836800||end<=start){invalid++;continue;}
                    Candidate candidate=new Candidate(lat,lon,start*1000,end*1000,(int)character);
                    if(keys.add(candidate.key))candidates.add(candidate);else duplicates++;
                }
                reader.endArray();
            }else reader.skipValue();
        }
        reader.endObject();
        if(reader.peek()!=JsonToken.END_DOCUMENT||!seenRows||!seenMeta||server<1577836800)throw new IOException("Invalid feed metadata");
        return new Snapshot(server*1000,receivedElapsedMs,rows,unsupported,invalid,duplicates,candidates);
    }
}
