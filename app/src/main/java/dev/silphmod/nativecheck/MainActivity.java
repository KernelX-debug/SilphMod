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
        if(mode!=1&&mode!=2){
            ui.title("SilphMod",BuildConfig.VERSION_NAME);
            LinearLayout farmer=ui.card("FARMER");farmer.addView(ui.text("Autocatch & autospin",23,ModeUi.TEXT));
            farmer.addView(ui.text("Wild catches · PokéStops · Inventory",14,ModeUi.MUTED));
            ui.button(farmer,"Open Farmer",true).setOnClickListener(v->open(1));
            LinearLayout kec=ui.card("KECLEON (BETA)");kec.addView(ui.text("Automatic Kecleon search & catch",23,ModeUi.TEXT));
            kec.addView(ui.text("Candidate queue · Automatic jumps · IV & shiny filters",14,ModeUi.MUTED));
            ui.button(kec,"Open Kecleon (BETA)",false).setOnClickListener(v->open(2));ui.show();return;
        }
        ui.button(ui.page,"‹  Modes",false).setOnClickListener(v->{mode=0;show();});
        ui.title(mode==1?"Farmer":"Kecleon (BETA)","SilphMod");
        status=ui.text(CheckService.view.status,17,ModeUi.ACCENT);ui.page.addView(status);
        elapsed=ui.text("00:00:00",16,ModeUi.MUTED);ui.page.addView(elapsed);
        details=ui.text("",13,ModeUi.MUTED);ui.page.addView(details);
        String[] labels=mode==1?new String[]{"Catches","PokéStops","Transferred","Items deleted","XP","Stardust"}:new String[]{"Encounters","Catches","Shinies","Items deleted","XP","Stardust"};
        for(int row=0;row<3;row++){LinearLayout line=ui.row();ui.page.addView(line);for(int col=0;col<2;col++)metrics[row*2+col]=ui.metric(line,labels[row*2+col]);}
        clear=ui.button(ui.page,"Clear statistics",false);clear.setOnClickListener(v->{if(CheckService.view.running)return;
            inputError="";settings.save(mode,new NativeSessionState.Totals());settings.prefs.edit().putLong("stats"+mode+".elapsed",0).apply();if(mode==2)dev.rootcatch.ugphone.KecleonHost.clearHistory(this);render();});ui.space(ui.page,16);
        SettingsDialogs dialogs=new SettingsDialogs(this);
        LinearLayout features=ui.card("Features");
        check(features,"Fix error 12","fixLocation",false);
        if(mode==1){
            toggle(features,"Catches","catches",true);
            setting(features,"Only catch",speciesSummary()).setOnClickListener(v->new PokemonPickerDialog(this,settings.prefs.getString("species","")).show(value->{settings.prefs.edit().putString("species",value).apply();show();}));
        }else{
            String[] names={"Only catch 15/15/15","Only catch 0/0/0","Only catch shiny","Only catch shiny + 15/15/15"};
            for(int i=0;i<names.length;i++)bit(features,names[i],"filters",1<<i,0);
        }
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
            transfer.setOnCheckedChangeListener((v,on)->{settings.prefs.edit().putBoolean("transfer",on).apply();options.setVisibility(on?View.VISIBLE:View.GONE);});options.setVisibility(transfer.isChecked()?View.VISIBLE:View.GONE);
        }
        CompoundButton clean=toggle(features,"Clean item bag","clean",false);
        LinearLayout itemOptions=ui.column();features.addView(itemOptions);
        setting(itemOptions,"Item limits",itemSummary()).setOnClickListener(v->dialogs.itemsDialog(settings.prefs.getString("limits",""),value->{settings.prefs.edit().putString("limits",value).apply();show();}));
        clean.setOnCheckedChangeListener((v,on)->{settings.prefs.edit().putBoolean("clean",on).apply();itemOptions.setVisibility(on?View.VISIBLE:View.GONE);});itemOptions.setVisibility(clean.isChecked()?View.VISIBLE:View.GONE);
        LinearLayout inventory=ui.card("Inventory tools");
        toggle(inventory,"Show IV names","showIvNames",true);
        toggle(inventory,"Extended manual transfer","extendedManualTransfer",false);
        LinearLayout game=ui.card("Game tools");
        toggle(game,"Fast gift opening","fastGiftOpening",true);
        toggle(game,"Auto-open gifts","autoOpenGifts",false);
        toggle(game,"Auto-send gifts","autoSendGifts",false);
        toggle(game,"Hide trainer name","hideTrainerName",true);
        Button engine=ui.button(ui.page,"Engine settings",false);edits.add(engine);engine.setOnClickListener(v->engineDialog());
        start=ui.button(ui.controls,"Start service",true);start.setOnClickListener(v->{if(CheckService.view.running){startService(new Intent(this,CheckService.class).setAction(CheckService.STOP));return;}
            try{inputError="";settings.config(mode,false);getPreferences(0).edit().putInt("mode",mode).apply();startForegroundService(new Intent(this,CheckService.class).setAction(CheckService.START).putExtra("mode",mode));}
            catch(Exception error){inputError=error.getMessage();status.setText(inputError);Toast.makeText(this,inputError,Toast.LENGTH_LONG).show();}
        });
        export=ui.button(ui.controls,"Export logs",false);export.setOnClickListener(v->export());ui.show();render();
    }
    private void open(int selected){mode=selected;getPreferences(0).edit().putInt("mode",mode).apply();show();}
    private void render(){
        if(status==null)return;CheckService.View value=CheckService.view;var t=settings.totals(mode);
        set(status,value.running||inputError.isEmpty()?value.status:inputError);set(details,value.running?value.details:"");
        long[] values=mode==1?new long[]{t.caught,t.spun,t.transferred,t.discarded,t.xp,t.stardust}:new long[]{t.encounters,t.caught,t.shinies,t.discarded,t.xp,t.stardust};
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
    private String itemSummary(){String text=settings.prefs.getString("limits","");return text.isBlank()?"Not configured":text.split(",").length+" item types";}
    private void export(){export.setEnabled(false);export.setText("Exporting…");new Thread(()->{String result;try{CheckLog.export(this);result="Saved to Downloads/SilphMod";}catch(Exception error){result=error.getMessage();}String message=result;runOnUiThread(()->{export.setEnabled(true);export.setText("Export logs");Toast.makeText(this,message==null?"Export failed":message,Toast.LENGTH_LONG).show();});},"silph-export").start();}
    private void engineDialog(){
        Dialog dialog=new Dialog(this,R.style.SettingsDialogTheme);LinearLayout page=ui.column();page.setPadding(ui.dp(22),ui.dp(20),ui.dp(22),ui.dp(20));page.setBackgroundColor(ModeUi.BG);
        page.addView(ui.text("Engine settings",23,ModeUi.TEXT));
        LinkedHashMap<String,EditText> inputs=new LinkedHashMap<>();input(page,inputs,"delay","Action interval (ms)",100);
        if(mode==1){input(page,inputs,"catchesMax","Maximum catches",4800);input(page,inputs,"spinsMax","Maximum spins",2400);}else input(page,inputs,"encountersMax","Maximum encounters",1000);
        ui.button(page,"Save",true).setOnClickListener(v->{try{var editor=settings.prefs.edit();for(var pair:inputs.entrySet()){int n=Integer.parseInt(pair.getValue().getText().toString());if(pair.getKey().equals("delay")?(n<100||n>3000):(n<1||n>50000))throw new IllegalArgumentException("Invalid value");editor.putInt(pair.getKey(),n);}editor.apply();inputError="";dialog.dismiss();}catch(Exception error){Toast.makeText(this,"Invalid value",Toast.LENGTH_SHORT).show();}});
        ui.button(page,"Cancel",false).setOnClickListener(v->dialog.dismiss());dialog.setContentView(page);dialog.show();if(dialog.getWindow()!=null)dialog.getWindow().setLayout(Math.min(getResources().getDisplayMetrics().widthPixels-ui.dp(32),ui.dp(420)),-2);
    }
    private void input(LinearLayout page,Map<String,EditText> inputs,String key,String title,int fallback){page.addView(ui.text(title,14,ModeUi.MUTED));EditText edit=new EditText(this);edit.setSingleLine(true);edit.setInputType(InputType.TYPE_CLASS_NUMBER);edit.setTextColor(ModeUi.TEXT);edit.setText(Integer.toString(settings.prefs.getInt(key,fallback)));edit.setContentDescription(title);page.addView(edit);inputs.put(key,edit);}
}
