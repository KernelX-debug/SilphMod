package dev.silphmod.nativecheck;

import android.content.Context;
import android.content.SharedPreferences;
import dev.rootcatch.nativeengine.*;
import java.util.ArrayList;

final class NativeSettings {
    final SharedPreferences prefs;
    NativeSettings(Context context){prefs=context.getSharedPreferences("native-settings",0);migrate(context);}
    private void migrate(Context context){
        if(prefs.getBoolean("imported",false))return;
        SharedPreferences old=context.getSharedPreferences("rootcatch",0);java.util.Map<String,?> values=old.getAll();
        SharedPreferences.Editor e=prefs.edit();
        String[][] booleans={{"catches","catch"},{"spins","spin"},{"removeBuddy","remove_buddy"},{"transfer","transfer"},{"fixLocation","resolve_error_12"},{"berries","use_berries"},{"showIvNames","show_iv_names"},{"hideTrainerName","hide_trainer_name"},{"extendedManualTransfer","extended_manual_transfer"},{"fastGiftOpening","fast_gift_opening"}};
        for(String[] keys:booleans)if(!prefs.contains(keys[0])&&values.get(keys[1]) instanceof Boolean)e.putBoolean(keys[0],(Boolean)values.get(keys[1]));
        String[][] numbers={{"delay","delay"},{"catchesMax","max_catches"},{"spinsMax","max_spins"}};
        for(String[] keys:numbers)if(!prefs.contains(keys[0])&&values.get(keys[1]) instanceof Number)e.putInt(keys[0],((Number)values.get(keys[1])).intValue());
        if(!prefs.contains("species")&&values.get("only_catch") instanceof String)e.putString("species",(String)values.get("only_catch"));
        if(!values.isEmpty()&&!prefs.contains("keep")){String[] keys={"keep_shiny","keep_hundo","keep_dynamax","keep_hatched","keep_background","keep_shiny_background","keep_field_research"};int keep=0;for(int i=0;i<keys.length;i++)if(old.getBoolean(keys[i],i!=5))keep|=1<<i;e.putInt("keep",keep);}
        if(!prefs.contains("quality")&&values.get("throw_quality") instanceof String){String[] q={"NORMAL","NICE","GREAT","EXCELLENT"};for(int i=0;i<q.length;i++)if(q[i].equals(values.get("throw_quality")))e.putInt("quality",i+1);}
        if(!prefs.contains("berriesOrder")&&values.get("berry_order") instanceof String){ArrayList<String> ids=new ArrayList<>();for(String name:dev.rootcatch.automation.BerryOptions.parse((String)values.get("berry_order")))for(ItemCatalog.Entry item:ItemCatalog.ENTRIES)if(item.name().equals(name))ids.add(Integer.toString(item.id()));e.putString("berriesOrder",String.join(",",ids));}
        if(!prefs.contains("limits")&&values.get("discard_limits") instanceof String){ArrayList<String> limits=new ArrayList<>();for(String pair:((String)values.get("discard_limits")).split(",")){String[] fields=pair.split("=");if(fields.length!=2)continue;for(ItemCatalog.Entry item:ItemCatalog.ENTRIES)if(item.discardable()&&item.name().equals(fields[0].trim()))limits.add(item.id()+":"+fields[1].trim());}if(!limits.isEmpty()){e.putString("limits",String.join(",",limits));if(!prefs.contains("clean"))e.putBoolean("clean",old.getBoolean("clean",false));}}
        for(int mode=1;mode<=2;mode++){
            SharedPreferences stats=context.getSharedPreferences(mode==1?"farm_last_result":"kecleon_last_result",0);
            for(String key:new String[]{"caught","fled","spun","discarded","transferred","encounters","shinies","xp","stardust","elapsed"})if(!prefs.contains("stats"+mode+"."+key)&&stats.getAll().get(key) instanceof Number)e.putLong("stats"+mode+"."+key,Math.max(0,((Number)stats.getAll().get(key)).longValue()));
        }
        SharedPreferences k=context.getSharedPreferences("silph_kecleon",0);if(!prefs.contains("encountersMax")&&k.contains("maximum"))e.putInt("encountersMax",k.getInt("maximum",1000));
        if(!prefs.contains("filters")&&!k.getAll().isEmpty()){int filter=0;String[] keys={"hundo","nundo","shiny","shundo"};for(int i=0;i<4;i++)if(k.getBoolean(keys[i],false))filter|=1<<i;e.putInt("filters",filter);}
        if(!e.putBoolean("imported",true).commit())throw new IllegalStateException("Cannot save settings");
    }
    private long clock;
    private boolean timing;
    long elapsed(int mode){return prefs.getLong("stats"+mode+".elapsed",0);}
    void elapsedPulse(int mode,boolean active){long now=android.os.SystemClock.elapsedRealtime();if(clock==0){clock=now;timing=active;return;}long delta=now-clock;if(delta>=1000||!active){if(timing&&delta>0&&delta<=10000){long before=elapsed(mode);prefs.edit().putLong("stats"+mode+".elapsed",before>Long.MAX_VALUE-delta?Long.MAX_VALUE:before+delta).apply();}clock=now;}timing=active;}

    String berryNames(){ArrayList<String> names=new ArrayList<>();for(int id:integers(prefs.getString("berriesOrder","701,705,703")))names.add(dev.rootcatch.nativeengine.ItemCatalog.ENTRIES.stream().filter(e->e.id()==id).findFirst().orElseThrow().name());return String.join(",",names);}
    void saveBerries(String names){ArrayList<String> ids=new ArrayList<>();for(String name:dev.rootcatch.automation.BerryOptions.parse(names))ids.add(Integer.toString(dev.rootcatch.nativeengine.ItemCatalog.ENTRIES.stream().filter(e->e.name().equals(name)).findFirst().orElseThrow().id()));prefs.edit().putString("berriesOrder",String.join(",",ids)).putBoolean("berries",!ids.isEmpty()).apply();}
    NativeSessionState.Totals totals(int mode){String p="stats"+mode+".";return new NativeSessionState.Totals(prefs.getLong(p+"caught",0),prefs.getLong(p+"fled",0),prefs.getLong(p+"spun",0),prefs.getLong(p+"discarded",0),prefs.getLong(p+"transferred",0),prefs.getLong(p+"encounters",0),prefs.getLong(p+"shinies",0),prefs.getLong(p+"xp",0),prefs.getLong(p+"stardust",0));}
    void save(int mode,NativeSessionState.Totals t){String p="stats"+mode+".";prefs.edit().putLong(p+"caught",t.caught).putLong(p+"fled",t.fled).putLong(p+"spun",t.spun).putLong(p+"discarded",t.discarded).putLong(p+"transferred",t.transferred).putLong(p+"encounters",t.encounters).putLong(p+"shinies",t.shinies).putLong(p+"xp",t.xp).putLong(p+"stardust",t.stardust).apply();}
    NativeSessionWire.Config config(int mode,boolean readonly){
        NativeSessionWire.Config c=new NativeSessionWire.Config();c.mode=mode;c.delayMs=prefs.getInt("delay",100);c.durationMs=readonly?300000:0;
        if(readonly){if(prefs.getBoolean("fixLocation",false))throw new IllegalArgumentException("Turn off Read-only check to use Fix error 12");c.flags=NativeSessionWire.READ_ONLY;return c;}
        c.flags=mode==2?NativeSessionWire.CATCH:(prefs.getBoolean("catches",true)?1:0)|(prefs.getBoolean("spins",true)?2:0);
        if((c.flags&1)!=0&&prefs.getBoolean("berries",true)){c.flags|=16;c.berries=integers(prefs.getString("berriesOrder","701,705,703"));}
        if(prefs.getBoolean("curve",true))c.flags|=32;c.quality=prefs.getInt("quality",4);c.keep=prefs.getInt("keep",95);
        if(prefs.getBoolean("hideTrainerName",true))c.flags|=NativeSessionWire.TRAINER_NAME;
        if(prefs.getBoolean("showIvNames",true))c.flags|=NativeSessionWire.IV_NAMES;
        if(prefs.getBoolean("fixLocation",false))c.flags|=NativeSessionWire.FIX_LOCATION;
        c.flags|=NativeSessionWire.SCENES;
        if(prefs.getBoolean("extendedManualTransfer",false))c.flags|=NativeSessionWire.MANUAL_TRANSFER;
        if(prefs.getBoolean("fastGiftOpening",true))c.flags|=NativeSessionWire.FAST_GIFTS;
        if(prefs.getBoolean("removeBuddy",false))c.flags|=NativeSessionWire.REMOVE_BUDDY;
        if(mode==1&&(c.flags&1)!=0&&prefs.getBoolean("transfer",false))c.flags|=8;
        if(prefs.getBoolean("clean",false)){String text=prefs.getString("limits","").trim();ArrayList<int[]> limits=new ArrayList<>();
            if(!text.isEmpty())for(String item:text.split(",")){String[] pair=item.trim().split(":");if(pair.length!=2)throw new IllegalArgumentException("Item limits: item ID:keep count");limits.add(new int[]{Integer.parseInt(pair[0].trim()),Integer.parseInt(pair[1].trim())});}
            if(limits.isEmpty())throw new IllegalArgumentException("Add item limits first");c.flags|=4;c.limits=limits.toArray(new int[0][]);
        }
        NativeSessionState.Totals t=totals(mode);
        if(mode==1){c.maxCatches=(c.flags&1)!=0?remaining(prefs.getInt("catchesMax",4800),t.caught,"Catch"):0;c.maxSpins=(c.flags&2)!=0?remaining(prefs.getInt("spinsMax",2400),t.spun,"Spin"):0;
            for(int id:integers(prefs.getString("species","")))c.select(id);
        }else{c.maxEncounters=remaining(prefs.getInt("encountersMax",1000),t.encounters,"Encounter");c.filters=prefs.getInt("filters",0);}
        return c;
    }
    static int[] integers(String s){if(s==null||s.trim().isEmpty())return new int[0];String[] values=s.split(",");int[] out=new int[values.length];for(int i=0;i<values.length;i++)out[i]=Integer.parseInt(values[i].trim());return out;}
    private static int remaining(int maximum,long current,String name){if(maximum<1||maximum>50000)throw new IllegalArgumentException(name+" limit must be 1–50000");if(current>=maximum)throw new IllegalArgumentException(name+" limit reached; clear statistics or increase limit");return(int)(maximum-current);}
}
