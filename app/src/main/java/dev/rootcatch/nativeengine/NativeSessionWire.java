package dev.rootcatch.nativeengine;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.zip.CRC32;

public final class NativeSessionWire {
    public static final int CONFIG_BYTES=1024,SNAPSHOT_BYTES=256,FARMER=1,KECLEON=2;
    public static final int CATCH=1,SPIN=2,CLEAN=4,TRANSFER=8,BERRIES=16,CURVE=32,READ_ONLY=64,TRAINER_NAME=128,IV_NAMES=256,FIX_LOCATION=512,SCENES=1024,MANUAL_TRANSFER=2048,FAST_GIFTS=4096,REMOVE_BUDDY=8192,AUTO_OPEN_GIFTS=16384,AUTO_SEND_GIFTS=32768;
    private static final long CONFIG_MAGIC=0x3147464348504c53L,SNAPSHOT_MAGIC=0x3154415348504c53L;
    private NativeSessionWire() { }
    public static final class Config {
        public long ticks,sessionHi,sessionLo;
        public int pid,mode=FARMER,flags=READ_ONLY,quality=3,delayMs=150,keep=127;
        public int maxCatches,maxSpins,maxEncounters,filters,durationMs;
        public int[] berries=new int[0];
        public int[][] limits=new int[0][2];
        public byte[] species=new byte[256];
        public byte[] encode() {
            validate();ByteBuffer b=ByteBuffer.allocate(CONFIG_BYTES).order(ByteOrder.LITTLE_ENDIAN);
            b.putLong(CONFIG_MAGIC).putInt(1).putInt(CONFIG_BYTES).putInt(pid).putInt(mode).putLong(ticks).putLong(sessionHi).putLong(sessionLo);
            b.putInt(flags).putInt(quality).putInt(delayMs).putInt(keep).putInt(maxCatches).putInt(maxSpins).putInt(maxEncounters).putInt(berries.length);
            for(int i=0;i<berries.length;i++)b.putInt(80+i*4,berries[i]);b.putInt(100,limits.length);
            for(int i=0;i<limits.length;i++){b.putInt(104+i*8,limits[i][0]);b.putInt(108+i*8,limits[i][1]);}
            b.position(616);b.put(species);b.putInt(872,filters).putInt(876,durationMs);checksum(b,1020);return b.array();
        }
        public String command(){return "engine "+hex(encode());}
        public void select(int speciesId){if(speciesId<=0||speciesId>=2048)throw new IllegalArgumentException("Invalid species");species[speciesId/8]|=(byte)(1<<(speciesId%8));}
        private void validate() {
            require(pid>1&&ticks>0&&(sessionHi!=0||sessionLo!=0)&&mode>=1&&mode<=2&&flags>=0&&flags<=65535);
            require(quality>=1&&quality<=4&&delayMs>=100&&delayMs<=3000&&keep>=0&&keep<=255&&maxCatches>=0&&maxCatches<=50000
                    &&maxSpins>=0&&maxSpins<=50000&&maxEncounters>=0&&maxEncounters<=50000&&filters>=0&&filters<=15
                    &&durationMs>=0&&durationMs<=86400000&&(durationMs==0||durationMs>=1000));
            require(berries!=null&&berries.length<=5&&limits!=null&&limits.length<=64&&species!=null&&species.length==256&&(species[0]&1)==0);
            if((flags&READ_ONLY)!=0)require(flags==READ_ONLY&&maxCatches==0&&maxSpins==0&&maxEncounters==0&&filters==0);
            else if(mode==FARMER)require((flags&7)!=0&&((flags&CATCH)==0||maxCatches>0)&&maxEncounters==0&&filters==0
                    &&((flags&CATCH)!=0||(flags&(TRANSFER|BERRIES))==0));
            else require((flags&CATCH)!=0&&(flags&(SPIN|TRANSFER))==0&&maxEncounters>0&&maxCatches==0&&maxSpins==0);
            require(((flags&BERRIES)!=0)==(berries.length>0)&&((flags&CLEAN)!=0)==(limits.length>0));
            for(int i=0;i<berries.length;i++){int n=berries[i];require(n==701||n==703||n==705||n==706||n==708);for(int j=0;j<i;j++)require(n!=berries[j]);}
            for(int i=0;i<limits.length;i++){require(limits[i]!=null&&limits[i].length==2&&limits[i][0]>0&&limits[i][0]!=4&&limits[i][0]<=100000&&limits[i][1]>=0&&limits[i][1]<=100000);for(int j=0;j<i;j++)require(limits[j][0]!=limits[i][0]);}
            if(mode==KECLEON)for(int i=0;i<species.length;i++)require(species[i]==0||i==44&&species[i]==1);
        }
    }
    public static Config decodeConfig(byte[] data){
        ByteBuffer b=checked(data,CONFIG_BYTES,CONFIG_MAGIC,1020);for(int i=880;i<1020;i++)require(data[i]==0);
        Config c=new Config();c.pid=b.getInt(16);c.mode=b.getInt(20);c.ticks=b.getLong(24);c.sessionHi=b.getLong(32);c.sessionLo=b.getLong(40);
        c.flags=b.getInt(48);c.quality=b.getInt(52);c.delayMs=b.getInt(56);c.keep=b.getInt(60);c.maxCatches=b.getInt(64);c.maxSpins=b.getInt(68);c.maxEncounters=b.getInt(72);
        int count=b.getInt(76);require(count>=0&&count<=5);c.berries=new int[count];for(int i=0;i<5;i++){int n=b.getInt(80+i*4);if(i<count)c.berries[i]=n;else require(n==0);}
        count=b.getInt(100);require(count>=0&&count<=64);c.limits=new int[count][2];for(int i=0;i<64;i++){int item=b.getInt(104+i*8),floor=b.getInt(108+i*8);if(i<count){c.limits[i][0]=item;c.limits[i][1]=floor;}else require(item==0&&floor==0);}
        c.species=Arrays.copyOfRange(data,616,872);c.filters=b.getInt(872);c.durationMs=b.getInt(876);c.validate();return c;
    }
    public static final class Snapshot {
        public final int mode,state,error,reason,flags,pid,candidateState,rewardsAvailable,visualAvailable,visualFailed,researchStatus,locationStatus;
        public final long ivReads,nameUpdates;
        public final long ticks,sessionHi,sessionLo,sequence,accountHi,accountLo;
        public final long caught,fled,spun,discarded,transferred,encounters,shinies,berries,throwsCount,xp,stardust,encounterId,capturedId,candidateRequest;
        public final double latitude,longitude;
        private Snapshot(ByteBuffer b){
            mode=b.getInt(16);state=b.getInt(20);error=b.getInt(24);reason=b.getInt(28);flags=b.getInt(32);pid=b.getInt(36);
            ticks=b.getLong(40);sessionHi=b.getLong(48);sessionLo=b.getLong(56);sequence=b.getLong(64);accountHi=b.getLong(72);accountLo=b.getLong(80);
            caught=b.getLong(88);fled=b.getLong(96);spun=b.getLong(104);discarded=b.getLong(112);transferred=b.getLong(120);encounters=b.getLong(128);
            shinies=b.getLong(136);berries=b.getLong(144);throwsCount=b.getLong(152);xp=b.getLong(160);stardust=b.getLong(168);encounterId=b.getLong(176);capturedId=b.getLong(184);
            candidateRequest=b.getLong(192);latitude=b.getDouble(200);longitude=b.getDouble(208);candidateState=b.getInt(216);rewardsAvailable=b.getInt(220);
            visualAvailable=b.getInt(224);visualFailed=b.getInt(228);ivReads=b.getLong(232);nameUpdates=b.getLong(240);
            researchStatus=b.getInt(248)&65535;locationStatus=b.getInt(248)>>>16;
            require((visualAvailable&~7552)==0&&(visualFailed&~7552)==0&&(visualAvailable&visualFailed)==0&&ivReads>=0&&nameUpdates>=0);
            require(mode>=1&&mode<=2&&state>=0&&state<=7&&flags>=0&&flags<=15&&pid>1&&ticks>0&&(sessionHi!=0||sessionLo!=0)
                    &&rewardsAvailable>=0&&rewardsAvailable<=3&&Double.isFinite(latitude)&&Double.isFinite(longitude)&&Math.abs(latitude)<=90&&Math.abs(longitude)<=180);
        }
        public void bind(Config config){require(pid==config.pid&&ticks==config.ticks&&sessionHi==config.sessionHi&&sessionLo==config.sessionLo&&mode==config.mode);}
    }
    public static Snapshot decodeSnapshot(byte[] data){ByteBuffer b=checked(data,SNAPSHOT_BYTES,SNAPSHOT_MAGIC,252);return new Snapshot(b);}
    private static ByteBuffer checked(byte[] data,int size,long magic,int checksumAt){
        require(data!=null&&data.length==size);ByteBuffer b=ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN);
        require(b.getLong(0)==magic&&b.getInt(8)==1&&b.getInt(12)==size);CRC32 crc=new CRC32();crc.update(data,0,checksumAt);require(b.getInt(checksumAt)==(int)crc.getValue());return b;
    }
    private static void checksum(ByteBuffer b,int offset){CRC32 crc=new CRC32();crc.update(b.array(),0,offset);b.putInt(offset,(int)crc.getValue());}
    public static String hex(byte[] bytes){StringBuilder out=new StringBuilder(bytes.length*2);for(byte value:bytes){out.append(Character.forDigit((value>>>4)&15,16));out.append(Character.forDigit(value&15,16));}return out.toString();}
    public static byte[] unhex(String text){require(text!=null&&(text.length()&1)==0&&text.length()<=4096);byte[] out=new byte[text.length()/2];for(int i=0;i<out.length;i++){int a=Character.digit(text.charAt(i*2),16),b=Character.digit(text.charAt(i*2+1),16);require(a>=0&&b>=0);out[i]=(byte)((a<<4)|b);}return out;}
    private static void require(boolean condition){if(!condition)throw new IllegalArgumentException("Invalid native session frame");}
}
