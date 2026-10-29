package dev.silphmod.nativecheck;

import android.Manifest;
import android.app.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.os.*;
import android.text.InputType;
import android.view.View;
import android.widget.*;
import dev.rootcatch.automation.ThrowQuality;
import dev.rootcatch.nativeengine.*;
import java.util.*;

public final class MainActivity extends Activity {
    private final Handler handler=new Handler(Looper.getMainLooper());
    private NativeSettings settings;
    private ModeUi ui;
    private int mode;
    private TextView status,details,elapsed;
    private final TextView[] metrics=new TextView[6];
    private Button start,export,clear;
    private String inputError="";
    private final ArrayList<View> edits=new ArrayList<>();
    private final Runnable refresh=new Runnable(){public void run(){render();handler.postDelayed(this,750);}};
    @Override public void onCreate(Bundle state){
        super.onCreate(state);settings=new NativeSettings(this);
        getWindow().setStatusBarColor(ModeUi.BG);getWindow().setNavigationBarColor(ModeUi.BG);
        mode=state==null?getIntent().getIntExtra("mode",0):state.getInt("mode",0);show();
        if(Build.VERSION.SDK_INT>=33&&checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED)requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS},10);
    }
    @Override protected void onNewIntent(Intent intent){super.onNewIntent(intent);setIntent(intent);mode=intent.getIntExtra("mode",mode);show();}
    @Override protected void onSaveInstanceState(Bundle state){state.putInt("mode",mode);super.onSaveInstanceState(state);}
    @Override protected void onResume(){super.onResume();handler.removeCallbacks(refresh);handler.post(refresh);}
    @Override protected void onPause(){handler.removeCallbacks(refresh);super.onPause();}
    private void show(){
        edits.clear();inputError="";ui=new ModeUi(this);status=null;start=null;
        if(mode!=1&&mode!=2&&mode!=3&&mode!=4){
            ui.title("SilphMod",BuildConfig.VERSION_NAME);
            LinearLayout farmer=ui.card("FARMER");farmer.addView(ui.text("Autocatch & autospin",23,ModeUi.TEXT));
            farmer.addView(ui.text("Wild catches · PokéStops · Inventory",14,ModeUi.MUTED));
            ui.button(farmer,"Open Farmer",true).setOnClickListener(v->open(1));
            LinearLayout kec=ui.card("KECLEON (BETA)");kec.addView(ui.text("Automatic Kecleon search & catch",23,ModeUi.TEXT));
            kec.addView(ui.text("Candidate queue · Automatic jumps · IV & shiny filters",14,ModeUi.MUTED));
            ui.button(kec,"Open Kecleon (BETA)",false).setOnClickListener(v->open(2));
            LinearLayout rocket=ui.card("AUTOROCKET (BETA)");rocket.addView(ui.text("Automatic grunt battles & shadow catches",23,ModeUi.TEXT));
            rocket.addView(ui.text("Rocket queue · Team healing · IV & shiny filters",14,ModeUi.MUTED));ui.button(rocket,"Open AutoRocket (BETA)",false).setOnClickListener(v->open(3));
            LinearLayout events=ui.card("EVENT TRICKS");events.addView(ui.text("Automatic event box collection",23,ModeUi.TEXT));
            events.addView(ui.text("Pokémon Candy · Rare Candy · No catches or spins",14,ModeUi.MUTED));
            ui.button(events,"Open Event tricks",false).setOnClickListener(v->open(4));ui.show();return;
        }
        ui.button(ui.page,"‹  Modes",false).setOnClickListener(v->{mode=0;show();});
        ui.title(mode==4?"Event tricks":mode==1?"Farmer":mode==2?"Kecleon (BETA)":"AutoRocket (BETA)","SilphMod");
        status=ui.text(CheckService.view.status,17,ModeUi.ACCENT);ui.page.addView(status);
        elapsed=ui.text("00:00:00",16,ModeUi.MUTED);ui.page.addView(elapsed);
        details=ui.text("",13,ModeUi.MUTED);ui.page.addView(details);
        String[] labels=mode==4?new String[]{"Boxes opened","Pokémon Candy","Rare Candy / XL","Candy XL","XP","Stardust"}:mode==1?new String[]{"Catches","PokéStops","Transferred","Items deleted","XP","Stardust"}:new String[]{mode==3?"Rockets":"Encounters","Catches","Shinies","Items deleted","XP","Stardust"};
        for(int row=0;row<3;row++){LinearLayout line=ui.row();ui.page.addView(line);for(int col=0;col<2;col++)metrics[row*2+col]=ui.metric(line,labels[row*2+col]);}
        clear=ui.button(ui.page,"Clear statistics",false);clear.setOnClickListener(v->{if(CheckService.view.running)return;
            inputError="";settings.save(mode,new NativeSessionState.Totals());settings.prefs.edit().putLong("stats"+mode+".elapsed",0).apply();if(mode==2)dev.rootcatch.ugphone.KecleonHost.clearHistory(this);if(mode==3)NativeRocketHost.clear(this);render();});ui.space(ui.page,16);
        if(mode==4){
            LinearLayout event=ui.card("Event boxes");
            event.addView(ui.text("Collects box rewards as you follow your route. Pokémon encounters are left uncaught.",14,ModeUi.MUTED));
            check(event,"Fix error 12","eventFixLocation",settings.prefs.getBoolean("fixLocation",false));
            check(event,"Hide trainer name","eventHideTrainerName",true);
            serviceControls();ui.show();render();return;
        }
        SettingsDialogs dialogs=new SettingsDialogs(this);
        LinearLayout features=ui.card("Features");
        check(features,"Fix error 12","fixLocation",false);
        if(mode==1){
            toggle(features,"Catches","catches",true);
            toggle(features,"Automatic event actions","automaticEventActions",true);
            setting(features,"Only catch",speciesSummary()).setOnClickListener(v->new PokemonPickerDialog(this,settings.prefs.getString("species","")).show(value->{settings.prefs.edit().putString("species",value).apply();show();}));
        }else{
            LinearLayout filters=ui.column();
            if(mode==3){CompoundButton all=toggle(features,"Catch all","rocketCatchAll",true);features.addView(filters);filters.setVisibility(all.isChecked()?View.GONE:View.VISIBLE);
                all.setOnCheckedChangeListener((v,on)->{settings.prefs.edit().putBoolean("rocketCatchAll",on).apply();filters.setVisibility(on?View.GONE:View.VISIBLE);});
            }else features.addView(filters);
            String[] names=mode==3?new String[]{"Only catch 15/15/15","Only catch 0/0/0","Only catch shiny","Only catch shiny + 15/15/15","Only catch 13/13/13 or above"}:new String[]{"Only catch 15/15/15","Only catch 0/0/0","Only catch shiny","Only catch shiny + 15/15/15"};
            for(int i=0;i<names.length;i++)bit(filters,names[i],mode==3?"rocketFilters":"filters",1<<i,0);
        }
        if(mode==3)check(features,"Use recommended team","rocketRecommendedTeam",true);
        int q=Math.max(1,Math.min(4,settings.prefs.getInt("quality",4)));
        setting(features,"Throw quality",ThrowQuality.values()[q-1].label).setOnClickListener(v->dialogs.throwsDialog(ThrowQuality.values()[q-1],value->{settings.prefs.edit().putInt("quality",value.ordinal()+1).apply();show();}));
        toggle(features,"Curve ball","curve",true);
        CompoundButton berries=toggle(features,"Use berries","berries",true);
        LinearLayout berryOptions=ui.column();features.addView(berryOptions);
        setting(berryOptions,"Berry priority",settings.berryNames().replace(","," · ")).setOnClickListener(v->dialogs.berriesDialog(settings.berryNames(),value->{settings.saveBerries(value);show();}));
        berries.setOnCheckedChangeListener((v,on)->{settings.prefs.edit().putBoolean("berries",on).apply();berryOptions.setVisibility(on?View.VISIBLE:View.GONE);});berryOptions.setVisibility(berries.isChecked()?View.VISIBLE:View.GONE);
        check(features,"Remove buddy","removeBuddy",false);
        if(mode==1){
            toggle(features,"Spin PokéStops","spins",true);
            CompoundButton transfer=toggle(features,"Transfer new catches","transfer",false);
            LinearLayout options=ui.column();features.addView(options);options.setPadding(ui.dp(8),0,0,ui.dp(12));
            String[] names={"Keep shiny","Keep 15/15/15","Keep Dynamax","Keep hatched","Keep background","Keep shiny + background","Keep field research encounters"};
            for(int i=0;i<names.length;i++)bit(options,names[i],"keep",1<<i,95);
            check(options,"Keep GO Pass encounters","keepGoPass",true);
            String kept=settings.prefs.getString("keepSpecies",dev.rootcatch.automation.PokemonSelection.KEEP_DEFAULT);
            int keptCount=dev.rootcatch.automation.PokemonSelection.parse(kept).size();
            setting(options,"Select Pokémon to keep",keptCount==0?"None selected":keptCount+" selected").setOnClickListener(v->new PokemonPickerDialog(this,kept,true).show(value->{settings.prefs.edit().putString("keepSpecies",value).apply();show();}));
            transfer.setOnCheckedChangeListener((v,on)->{settings.prefs.edit().putBoolean("transfer",on).apply();options.setVisibility(on?View.VISIBLE:View.GONE);});options.setVisibility(transfer.isChecked()?View.VISIBLE:View.GONE);
        }
        CompoundButton clean=toggle(features,"Clean item bag","clean",false);
        LinearLayout itemOptions=ui.column();features.addView(itemOptions);
        setting(itemOptions,"Item limits",itemSummary()).setOnClickListener(v->dialogs.itemsDialog(settings.prefs.getString("limits",""),value->{settings.prefs.edit().putString("limits",value).apply();show();}));
        clean.setOnCheckedChangeListener((v,on)->{settings.prefs.edit().putBoolean("clean",on).apply();itemOptions.setVisibility(on?View.VISIBLE:View.GONE);});itemOptions.setVisibility(clean.isChecked()?View.VISIBLE:View.GONE);
        if(mode==1){
            LinearLayout buddy=ui.card("Buddy");
            check(buddy,"Feed buddy","feedBuddy",false);
            setting(buddy,"Berry priority",settings.buddyBerryNames().replace(","," · ")).setOnClickListener(v->dialogs.berriesDialog(settings.buddyBerryNames(),value->{settings.saveBuddyBerries(value);show();}));
            check(buddy,"Mega Evolve buddy","megaBuddy",false);
            buddy.addView(ui.text(settings.prefs.getBoolean("removeBuddy",false)?"Disabled while Remove buddy is enabled":"Uses the available Mega Evolution with the lowest energy cost",13,ModeUi.MUTED));
        }
        if(mode==3){
            LinearLayout care=ui.card("Team care");
            setting(care,"Potion priority",medicineSummary(false)).setOnClickListener(v->dialogs.medicineDialog("Potion priority",settings.prefs.getString("rocketPotionOrder",""),false,value->{settings.prefs.edit().putString("rocketPotionOrder",value).apply();show();}));
            setting(care,"Revive priority",medicineSummary(true)).setOnClickListener(v->dialogs.medicineDialog("Revive priority",settings.prefs.getString("rocketReviveOrder",""),true,value->{settings.prefs.edit().putString("rocketReviveOrder",value).apply();show();}));
        }
        LinearLayout inventory=ui.card("Inventory tools");
        toggle(inventory,"Show IV names","showIvNames",true);
        toggle(inventory,"Extended manual transfer","extendedManualTransfer",false);
        LinearLayout game=ui.card("Game tools");
        toggle(game,"Fast gift opening","fastGiftOpening",true);
        if(mode!=3){toggle(game,"Auto-open gifts","autoOpenGifts",false);toggle(game,"Auto-send gifts","autoSendGifts",false);}
        toggle(game,"Hide trainer name","hideTrainerName",true);
        Button engine=ui.button(ui.page,"Engine settings",false);edits.add(engine);engine.setOnClickListener(v->engineDialog());
        serviceControls();ui.show();render();
    }
    private void serviceControls(){
        start=ui.button(ui.controls,"Start service",true);start.setOnClickListener(v->{if(CheckService.view.running){startService(new Intent(this,CheckService.class).setAction(CheckService.STOP));return;}
            try{inputError="";settings.config(mode,false);getPreferences(0).edit().putInt("mode",mode).apply();startForegroundService(new Intent(this,CheckService.class).setAction(CheckService.START).putExtra("mode",mode));}
            catch(Exception error){inputError=error.getMessage();status.setText(inputError);Toast.makeText(this,inputError,Toast.LENGTH_LONG).show();}
        });
        export=ui.button(ui.controls,"Export logs",false);export.setOnClickListener(v->export());
    }
    private void open(int selected){mode=selected;getPreferences(0).edit().putInt("mode",mode).apply();show();}
    private void render(){
        if(status==null)return;CheckService.View value=CheckService.view;var t=settings.totals(mode);
        set(status,value.running||inputError.isEmpty()?value.status:inputError);set(details,value.running?value.details:"");
        long[] values=mode==4?new long[]{t.spun,t.caught,t.shinies,t.discarded,t.xp,t.stardust}:mode==1?new long[]{t.caught,t.spun,t.transferred,t.discarded,t.xp,t.stardust}:new long[]{mode==3?t.spun:t.encounters,t.caught,t.shinies,t.discarded,t.xp,t.stardust};
        long seconds=settings.elapsed(mode)/1000;set(elapsed,String.format(Locale.US,"%02d:%02d:%02d",seconds/3600,seconds/60%60,seconds%60));
        for(int i=0;i<6;i++)set(metrics[i],String.format(Locale.US,"%,d",values[i]));
        set(start,value.running?"Stop service":"Start service");clear.setEnabled(!value.running);for(View edit:edits)edit.setEnabled(!value.running);
    }
    private static void set(TextView view,String value){if(!view.getText().toString().equals(value))view.setText(value);}
    private void check(LinearLayout parent,String title,String key,boolean fallback){CheckBox b=ui.check(parent,title,settings.prefs.getBoolean(key,fallback));edits.add(b);b.setOnCheckedChangeListener((v,on)->settings.prefs.edit().putBoolean(key,on).apply());}
    private CompoundButton toggle(LinearLayout parent,String title,String key,boolean fallback){
        Switch b=ui.toggle(parent,title,settings.prefs.getBoolean(key,fallback));edits.add(b);b.setOnCheckedChangeListener((v,on)->settings.prefs.edit().putBoolean(key,on).apply());return b;
    }
    private void bit(LinearLayout parent,String title,String key,int bit,int fallback){CheckBox b=ui.check(parent,title,(settings.prefs.getInt(key,fallback)&bit)!=0);edits.add(b);b.setOnCheckedChangeListener((v,on)->{int n=settings.prefs.getInt(key,fallback);settings.prefs.edit().putInt(key,on?n|bit:n&~bit).apply();});}
    private Button setting(LinearLayout parent,String label,String value){Button b=ui.button(parent,label+"\n"+value,false);edits.add(b);return b;}
    private String speciesSummary(){int n=dev.rootcatch.automation.PokemonSelection.parse(settings.prefs.getString("species","")).size();return n==0?"All Pokémon":n+" selected";}
    private String medicineSummary(boolean revives){int[] ids=RocketCareOrder.parse(settings.prefs.getString(revives?"rocketReviveOrder":"rocketPotionOrder",""),revives);if(ids.length==0)return "Automatic";java.util.ArrayList<String> names=new java.util.ArrayList<>();for(int id:ids)for(ItemCatalog.Entry item:ItemCatalog.ENTRIES)if(item.id()==id)names.add(item.label());return String.join(" · ",names);}
    private String itemSummary(){String text=settings.prefs.getString("limits","");return text.isBlank()?"Not configured":text.split(",").length+" item types";}
    private void export(){export.setEnabled(false);export.setText("Exporting…");new Thread(()->{String result;try{CheckLog.export(this);result="Saved to Downloads/SilphMod";}catch(Exception error){result=error.getMessage();}String message=result;runOnUiThread(()->{export.setEnabled(true);export.setText("Export logs");Toast.makeText(this,message==null?"Export failed":message,Toast.LENGTH_LONG).show();});},"silph-export").start();}
    private void engineDialog(){
        Dialog dialog=new Dialog(this,R.style.SettingsDialogTheme);LinearLayout page=ui.column();page.setPadding(ui.dp(22),ui.dp(20),ui.dp(22),ui.dp(20));page.setBackgroundColor(ModeUi.BG);
        page.addView(ui.text("Engine settings",23,ModeUi.TEXT));
        LinkedHashMap<String,EditText> inputs=new LinkedHashMap<>();input(page,inputs,"delay","Action interval (ms)",100);
        if(mode==1){input(page,inputs,"catchesMax","Maximum catches",4800);input(page,inputs,"spinsMax","Maximum spins",2400);}else if(mode==3)input(page,inputs,"rocketsMax","Maximum Rockets",700);else input(page,inputs,"encountersMax","Maximum encounters",1000);
        EditText location=new EditText(this);if(mode==3){page.addView(ui.text("Last cooldown location",14,ModeUi.MUTED));location.setSingleLine(true);location.setTextColor(ModeUi.TEXT);location.setInputType(InputType.TYPE_CLASS_TEXT);
            location.setHint("Latitude,longitude (optional)");location.setText(settings.prefs.getString("rocketCooldownLocation",""));page.addView(location);}
        ui.button(page,"Save",true).setOnClickListener(v->{try{var editor=settings.prefs.edit();for(var pair:inputs.entrySet()){int n=Integer.parseInt(pair.getValue().getText().toString());if(pair.getKey().equals("delay")?(n<100||n>3000):(n<1||n>50000))throw new IllegalArgumentException("Invalid value");editor.putInt(pair.getKey(),n);}if(mode==3){String value=location.getText().toString().trim();if(!value.isEmpty())dev.rootcatch.ugphone.KecleonFeed.parseCoordinates(value);editor.putString("rocketCooldownLocation",value);}editor.apply();inputError="";dialog.dismiss();}catch(Exception error){Toast.makeText(this,"Invalid value",Toast.LENGTH_SHORT).show();}});
        ui.button(page,"Cancel",false).setOnClickListener(v->dialog.dismiss());dialog.setContentView(page);dialog.show();if(dialog.getWindow()!=null)dialog.getWindow().setLayout(Math.min(getResources().getDisplayMetrics().widthPixels-ui.dp(32),ui.dp(420)),-2);
    }
    private void input(LinearLayout page,Map<String,EditText> inputs,String key,String title,int fallback){page.addView(ui.text(title,14,ModeUi.MUTED));EditText edit=new EditText(this);edit.setSingleLine(true);edit.setInputType(InputType.TYPE_CLASS_NUMBER);edit.setTextColor(ModeUi.TEXT);edit.setText(Integer.toString(settings.prefs.getInt(key,fallback)));edit.setContentDescription(title);page.addView(edit);inputs.put(key,edit);}
}
