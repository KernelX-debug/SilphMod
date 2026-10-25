package dev.silphmod.nativecheck;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class NativeExitReport {
    final String summary,crash,history;
    final boolean recoverable,intentional;
    private NativeExitReport(String summary,String crash,String history,boolean recoverable,boolean intentional){
        this.summary=summary;this.crash=crash;this.history=history;this.recoverable=recoverable;this.intentional=intentional;
    }
    static NativeExitReport inspect(String logcat,String exits,int pid,String process,long launched){
        if(pid<=1||process==null||process.isEmpty()||launched<=0)return new NativeExitReport("Game exit not identified","","",false,false);
        String crash=crash(logcat==null?"":logcat,pid),history="";int reason=-1;
        Pattern identity=Pattern.compile("timestamp=([^\\r\\n]+?) pid="+pid+"(?:\\s|$)");
        Pattern name=Pattern.compile("(?:^|\\s)process="+Pattern.quote(process)+"(?:\\s|$)");
        Pattern value=Pattern.compile("(?:^|\\s)reason=(\\d+)(?:\\s|$)");
        if(exits!=null)for(String entry:exits.split("ApplicationExitInfo #")){
            Matcher match=identity.matcher(entry);if(!match.find()||!name.matcher(entry).find())continue;
            try{
                SimpleDateFormat format=new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS",Locale.US);format.setLenient(false);
                Date time=format.parse(match.group(1).trim());
                if(time==null||time.getTime()<launched-5000)continue;
                Matcher number=value.matcher(entry);if(!number.find())continue;
                reason=Integer.parseInt(number.group(1));history=entry.length()>12000?entry.substring(0,12000):entry;break;
            }catch(Exception ignored){ }
        }
        if(reason==1||reason==10||reason==11)return new NativeExitReport("Game closed by user",crash,history,false,true);
        Matcher signal=Pattern.compile("(?:signal\\s+\\d+\\s*\\(|Fatal signal\\s+\\d+\\s*\\()(SIG[A-Z0-9]+)\\)").matcher(crash);
        if(signal.find()){
            String detail=signal.group(1);Matcher frame=Pattern.compile("#00[^\\r\\n]*?([^/\\s]+\\.so)(?:[\\s(]|$)").matcher(crash);
            if(frame.find())detail+=" · "+frame.group(1);
            return new NativeExitReport("Native crash: "+detail,crash,history,true,false);
        }
        String summary=reason==3?"Game stopped: low memory":reason==4?"Game crashed":reason==5?"Native game crash":reason==6?"Game stopped: ANR":reason==9?"Game stopped: resource limit":reason==12?"Game dependency exited":reason==14?"Game stopped: Android freezer":"Game process exited; export logs";
        return new NativeExitReport(summary,crash,history,reason==3||reason==4||reason==5||reason==6||reason==9||reason==12||reason==14,false);
    }
    private static String crash(String text,int pid){
        Pattern target=Pattern.compile("\\bpid:\\s*"+pid+"(?:,|\\s|$)");
        Matcher header=target.matcher(text);int selected=-1;while(header.find())selected=header.start();
        if(selected>=0){
            int start=text.lastIndexOf("*** ***",selected);if(start<0)start=text.lastIndexOf('\n',selected)+1;else start=text.lastIndexOf('\n',start)+1;
            int end=text.indexOf("*** ***",selected);if(end<0)end=text.length();else end=text.lastIndexOf('\n',end)+1;
            if(end<=start)end=text.length();return text.substring(start,Math.min(end,start+24000));
        }
        Pattern fatal=Pattern.compile("[^\\r\\n]*Fatal signal[^\\r\\n]*\\bpid\\s+"+pid+"(?:\\s|,|\\().*");
        Matcher match=fatal.matcher(text);String line="";while(match.find())line=match.group();return line;
    }
}
