package dev.silphmod.nativecheck;

final class NativeStartupPolicy {
    static String statCommand(int pid){
        if(pid<=1)throw new IllegalArgumentException("Invalid process");
        return "silph_stat=$(cat /proc/"+pid+"/stat 2>&1); silph_stat_status=$?; if [ \"$silph_stat_status\" -eq 0 ]; then printf '%s\\n' \"$silph_stat\"; elif [ ! -d /proc/"+pid+" ]; then :; else printf '%s\\n' \"$silph_stat\"; (exit \"$silph_stat_status\"); fi";
    }
    static boolean gameStage(int sdk,String kernel,boolean learned){
        return learned||(sdk==29&&kernel!=null&&kernel.toLowerCase(java.util.Locale.ROOT).matches(".*(?:[.-])lxc(?:[.-].*|$)"));
    }
    static boolean live(String stat,int pid,long ticks){
        if(stat==null||pid<=1||ticks<=0)return false;
        int end=stat.lastIndexOf(')');
        if(end<0||!stat.startsWith(pid+" ("))return false;
        String[] fields=stat.substring(end+1).trim().split("\\s+");
        if(fields.length<20||fields[0].length()!=1||"ZXx".contains(fields[0]))return false;
        try{return Long.parseLong(fields[19])==ticks;}catch(NumberFormatException error){return false;}
    }
}
