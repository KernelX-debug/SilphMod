package dev.silphmod.nativecheck;

import android.app.Activity;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.content.res.ColorStateList;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;
import android.widget.*;

final class ModeUi {
    static final int BG=0xff10151e,PANEL=0xff1a2330,TEXT=0xfff2f7fb,MUTED=0xff99aabd,ACCENT=0xff61e9b6;
    final Activity activity;
    final LinearLayout screen,page,controls;
    ModeUi(Activity activity){
        this.activity=activity;screen=column();screen.setBackgroundColor(BG);
        ScrollView scroll=new ScrollView(activity);scroll.setFillViewport(true);page=column();page.setPadding(dp(22),dp(18),dp(22),dp(24));scroll.addView(page);
        screen.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));controls=column();controls.setPadding(dp(22),dp(10),dp(22),dp(16));screen.addView(controls);
        screen.setOnApplyWindowInsetsListener((view,insets)->{
            if(Build.VERSION.SDK_INT>=30){android.graphics.Insets bars=insets.getInsets(WindowInsets.Type.systemBars()|WindowInsets.Type.displayCutout());view.setPadding(bars.left,bars.top,bars.right,bars.bottom);}
            else view.setPadding(insets.getSystemWindowInsetLeft(),insets.getSystemWindowInsetTop(),insets.getSystemWindowInsetRight(),insets.getSystemWindowInsetBottom());return insets;
        });
    }
    void show(){activity.setContentView(screen);screen.requestApplyInsets();}
    int dp(int value){return Math.round(value*activity.getResources().getDisplayMetrics().density);}
    LinearLayout column(){LinearLayout value=new LinearLayout(activity);value.setOrientation(LinearLayout.VERTICAL);return value;}
    LinearLayout row(){LinearLayout value=new LinearLayout(activity);value.setOrientation(LinearLayout.HORIZONTAL);return value;}
    TextView text(String value,int size,int color){TextView view=new TextView(activity);view.setText(value);view.setTextSize(size);view.setTextColor(color);view.setPadding(0,dp(7),0,dp(7));return view;}
    void title(String title,String caption){LinearLayout header=row();ImageView logo=new ImageView(activity);logo.setImageResource(R.drawable.ic_launcher);logo.setContentDescription("SilphMod");header.addView(logo,new LinearLayout.LayoutParams(dp(48),dp(48)));
        TextView value=text(title,26,TEXT);value.setTypeface(null,Typeface.BOLD);LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,-2,1);p.setMarginStart(dp(12));header.addView(value,p);page.addView(header);page.addView(text(caption,14,MUTED));space(page,14);}
    void space(LinearLayout parent,int height){View gap=new View(activity);parent.addView(gap,new LinearLayout.LayoutParams(1,dp(height)));}
    LinearLayout card(String title){LinearLayout card=column();card.setPadding(dp(16),dp(12),dp(16),dp(14));
        GradientDrawable shape=new GradientDrawable();shape.setColor(PANEL);shape.setCornerRadius(dp(20));card.setBackground(shape);
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,-2);p.bottomMargin=dp(14);page.addView(card,p);
        if(!title.isEmpty()){TextView label=text(title,15,MUTED);label.setTypeface(null,Typeface.BOLD);card.addView(label);}return card;
    }
    Button button(LinearLayout parent,String label,boolean primary){Button view=new Button(activity);view.setText(label);view.setAllCaps(false);
        view.setTextSize(15);view.setTextColor(primary?BG:TEXT);view.setMinHeight(dp(52));view.setBackgroundTintList(ColorStateList.valueOf(primary?ACCENT:PANEL));parent.addView(view);return view;}
    CheckBox check(LinearLayout parent,String label,boolean selected){CheckBox view=new CheckBox(activity);view.setText(label);view.setTextColor(TEXT);view.setTextSize(15);
        view.setButtonTintList(ColorStateList.valueOf(ACCENT));view.setMinHeight(dp(48));view.setChecked(selected);parent.addView(view);return view;}
    Switch toggle(LinearLayout parent,String label,boolean selected){Switch view=new Switch(activity);view.setText(label);view.setTextColor(TEXT);view.setTextSize(16);
        view.setThumbTintList(new ColorStateList(new int[][]{new int[]{android.R.attr.state_checked},new int[]{}},new int[]{ACCENT,MUTED}));view.setMinHeight(dp(54));view.setChecked(selected);parent.addView(view);return view;}
    TextView metric(LinearLayout parent,String label){LinearLayout box=column();box.setPadding(dp(14),dp(12),dp(14),dp(12));
        GradientDrawable shape=new GradientDrawable();shape.setColor(PANEL);shape.setCornerRadius(dp(18));box.setBackground(shape);
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,-2,1);p.setMargins(dp(3),dp(4),dp(3),dp(4));parent.addView(box,p);
        TextView value=text("0",27,TEXT);value.setTypeface(null,Typeface.BOLD);box.addView(value);box.addView(text(label,13,MUTED));return value;
    }
}
