package dev.rootcatch.nativeengine;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.zip.CRC32;

public final class NativeKecleonWire {
    public static final class Event {
        public long ticks,hi,lo,accountHi,accountLo,sequence,request;
        public int pid,phase,move,reason,verified;
        public double latitude,longitude;
        public String source,fort,incident;
        public void bind(NativeSessionWire.Config c){require(c.mode==2&&pid==c.pid&&ticks==c.ticks&&hi==c.sessionHi&&lo==c.sessionLo&&(accountHi!=0||accountLo!=0));}
    }
    public static Event decode(byte[] bytes){
        require(bytes!=null&&bytes.length==640);ByteBuffer b=ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        require(b.getLong(0)==0x3156454b48504c53L&&b.getInt(8)==1&&b.getInt(12)==640);CRC32 crc=new CRC32();crc.update(bytes,0,636);require(b.getInt(636)==(int)crc.getValue());
        for(int i=92;i<96;i++)require(bytes[i]==0);for(int i=531;i<636;i++)require(bytes[i]==0);
        Event e=new Event();e.pid=b.getInt(16);e.phase=b.getInt(20);e.ticks=b.getLong(24);e.hi=b.getLong(32);e.lo=b.getLong(40);e.accountHi=b.getLong(48);e.accountLo=b.getLong(56);
        e.sequence=b.getLong(64);e.request=b.getLong(72);e.move=b.getInt(80);e.reason=b.getInt(84);e.verified=b.getInt(88);e.latitude=b.getDouble(96);e.longitude=b.getDouble(104);
        require(e.pid>1&&e.ticks>0&&(e.hi!=0||e.lo!=0)&&(e.accountHi!=0||e.accountLo!=0)&&e.sequence>0&&e.request>0&&e.phase>=1&&e.phase<=8&&e.move>=0&&e.move<=2&&e.reason>=0&&e.reason<=10&&e.verified>=0&&e.verified<=1);
        require(Double.isFinite(e.latitude)&&Double.isFinite(e.longitude)&&Math.abs(e.latitude)<=90&&Math.abs(e.longitude)<=180);
        e.source=text(bytes,112,97);e.fort=text(bytes,209,161);e.incident=text(bytes,370,161);
        require(e.phase==1||e.phase==8||!e.source.isEmpty());require(e.phase!=4&&e.phase!=5||!e.fort.isEmpty()&&!e.incident.isEmpty());return e;
    }
    public static String command(Event e,long sequence,int kind,int move,boolean accepted,double latitude,double longitude,String source){
        require(sequence>0&&kind>=1&&kind<=6&&move>=0&&move<=2&&e!=null);
        require(Double.isFinite(latitude)&&Double.isFinite(longitude)&&Math.abs(latitude)<=90&&Math.abs(longitude)<=180);
        require(kind==1?source!=null&&source.matches("[a-zA-Z0-9_.:-]{1,96}"):latitude==0&&longitude==0&&(source==null||source.isEmpty()));
        require(kind==2||move==0);require(kind==2||kind==3||!accepted);
        ByteBuffer b=ByteBuffer.allocate(512).order(ByteOrder.LITTLE_ENDIAN);
        b.putLong(0x314d434b48504c53L).putInt(1).putInt(512).putInt(e.pid).putInt(kind).putLong(e.ticks).putLong(e.hi).putLong(e.lo).putLong(e.accountHi).putLong(e.accountLo)
            .putLong(sequence).putLong(e.request).putInt(move).putInt(accepted?1:0).putDouble(latitude).putDouble(longitude);
        if(source!=null)b.put(source.getBytes(StandardCharsets.US_ASCII));CRC32 crc=new CRC32();crc.update(b.array(),0,508);b.putInt(508,(int)crc.getValue());return "engine "+NativeSessionWire.hex(b.array());
    }
    private static String text(byte[] b,int start,int size){int end=start;while(end<start+size&&b[end]!=0){require(b[end]>=33&&b[end]<=126);end++;}require(end<start+size);for(int i=end;i<start+size;i++)require(b[i]==0);return new String(b,start,end-start,StandardCharsets.US_ASCII);}
    private static void require(boolean condition){if(!condition)throw new IllegalArgumentException("Invalid native Kecleon frame");}
    private NativeKecleonWire(){ }
}
