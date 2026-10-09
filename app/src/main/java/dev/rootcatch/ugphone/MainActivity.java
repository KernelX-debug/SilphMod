package dev.rootcatch.ugphone;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.WindowInsets;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;
import dev.rootcatch.R;
import dev.rootcatch.automation.AutomationConfig;
import dev.rootcatch.automation.BerryOptions;
import dev.rootcatch.automation.SessionLimits;
import dev.rootcatch.automation.ThrowQuality;
import dev.rootcatch.nativeengine.EnginePolicy;
import java.util.ArrayList;
import java.util.List;

public final class MainActivity extends Activity {
    private static final int BACKGROUND = Color.rgb(16, 21, 30), PANEL = Color.rgb(26, 35, 48),
            TEXT = Color.rgb(242, 247, 251), MUTED = Color.rgb(153, 170, 189), ACCENT = Color.rgb(97, 233, 182);
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final List<View> settings = new ArrayList<>();
    private SharedPreferences prefs;
    private Switch capture, spin, transfer, clean, useBerries, skipInterruptions;
    private CheckBox keepShiny, keepHundo, resolveError12;
    private EditText delay, maxCatches, maxSpins;
    private LinearLayout transferOptions, cleanOptions, advancedOptions, berryOptions;
    private TextView status, elapsed, caught, spun, transferred, discarded, xp, stardust;
    private Button start, export, items, advanced, throwSelector, berrySelector;
    private ThrowQuality throwQuality;
    private String limits;
    private String berryOrder;
    private boolean starting;
    private boolean visible;
    private long startingAt;
    private final Runnable refresh = new Runnable() {
        @Override public void run() { renderState(); openArmedGame(); handler.postDelayed(this, 250); }
    };

    @Override public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        prefs = getSharedPreferences(AutomationConfig.PREFS, MODE_PRIVATE);
        throwQuality = ThrowQuality.fromSavedValue(prefs.getString("throw_quality", "GREAT"));
        limits = prefs.getString("discard_limits", EnginePolicy.DEFAULT_LIMITS);
        berryOrder = BerryOptions.fromSavedValue(prefs.getString("berry_order", BerryOptions.DEFAULT_ORDER));
        FarmService.restoreLast(this);
        ScrollView scroll = new ScrollView(this); scroll.setFillViewport(true); scroll.setBackgroundColor(BACKGROUND);
        LinearLayout page = column(); page.setPadding(dp(22), dp(20), dp(22), dp(26)); scroll.addView(page);
        LinearLayout screen = column(); screen.setBackgroundColor(BACKGROUND);
        screen.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
        screen.setOnApplyWindowInsetsListener((view, insets) -> {
            if (Build.VERSION.SDK_INT >= 30) {
                android.graphics.Insets bars = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
                screen.setPadding(bars.left, bars.top, bars.right, bars.bottom);
            } else screen.setPadding(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                    insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
            return insets;
        });
        LinearLayout header = row();
        ImageView icon = new ImageView(this); icon.setImageResource(R.drawable.ic_launcher); icon.setContentDescription("SilphMod");
        header.addView(icon, new LinearLayout.LayoutParams(dp(48), dp(48)));
        TextView title = text("SilphMod", 26, TEXT); title.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        LinearLayout.LayoutParams titleParams = new LinearLayout.LayoutParams(0, -2, 1); titleParams.setMarginStart(dp(12));
        header.addView(title, titleParams); page.addView(header);
        status = text("Ready", 14, ACCENT); status.setPadding(0, dp(16), 0, dp(6)); page.addView(status);
        elapsed = text("00:00:00", 16, MUTED); elapsed.setTypeface(Typeface.MONOSPACE); page.addView(elapsed); space(page, 18);
        LinearLayout metrics = column();
        LinearLayout first = row(); caught = metric(first, "Catches"); spun = metric(first, "Spins"); metrics.addView(first);
        space(metrics, 10); LinearLayout second = row(); transferred = metric(second, "Transferred"); discarded = metric(second, "Items deleted"); metrics.addView(second);
        space(metrics, 10); LinearLayout rewards = row(); xp = metric(rewards, "XP"); stardust = metric(rewards, "Stardust"); metrics.addView(rewards);
        page.addView(metrics); space(page, 24);
        section(page, "Features");
        LinearLayout features = card(); page.addView(features);
        resolveError12 = check(features, "Fix error 12", prefs.getBoolean("resolve_error_12", false));
        resolveError12.setTextSize(16);
        capture = toggle(features, "Catches", prefs.getBoolean("catch", true));
        throwSelector = button(getString(R.string.throw_quality_selector, throwQuality.label), false);
        settings.add(throwSelector); features.addView(throwSelector);
        throwSelector.setOnClickListener(view -> {
            if (FarmService.isBusy() || starting) return;
            new SettingsDialogs(this).throwsDialog(throwQuality, selected -> {
                throwQuality = selected;
                prefs.edit().putString("throw_quality", selected.name()).apply();
                throwSelector.setText(getString(R.string.throw_quality_selector, selected.label));
            });
        });
        useBerries = toggle(features, getString(R.string.use_berries), prefs.getBoolean("use_berries", true));
        berryOptions = column(); features.addView(berryOptions);
        berrySelector = button("", false); settings.add(berrySelector); berryOptions.addView(berrySelector);
        updateBerryLabel();
        berrySelector.setOnClickListener(view -> {
            if (FarmService.isBusy() || starting) return;
            new SettingsDialogs(this).berriesDialog(berryOrder, selected -> {
                berryOrder = selected;
                prefs.edit().putString("berry_order", selected).apply();
                updateBerryLabel();
            });
        });
        useBerries.setOnCheckedChangeListener((view, checked) -> berryOptions.setVisibility(checked ? View.VISIBLE : View.GONE));
        berryOptions.setVisibility(useBerries.isChecked() ? View.VISIBLE : View.GONE);
        spin = toggle(features, "Spin PokéStops", prefs.getBoolean("spin", true));
        skipInterruptions = toggle(features, getString(R.string.skip_interruptions), prefs.getBoolean("skip_interruptions", true));
        transfer = toggle(features, "Transfer new catches", prefs.getBoolean("transfer", false));
        transferOptions = column(); transferOptions.setPadding(dp(8), 0, dp(8), dp(12)); features.addView(transferOptions);
        keepShiny = check(transferOptions, "Keep shiny", prefs.getBoolean("keep_shiny", true));
        keepHundo = check(transferOptions, "Keep 15/15/15", prefs.getBoolean("keep_hundo", true));
        clean = toggle(features, "Clean item bag", prefs.getBoolean("clean", false));
        cleanOptions = column(); cleanOptions.setPadding(dp(8), 0, dp(8), dp(10)); features.addView(cleanOptions);
        items = button("Items to keep", false); settings.add(items); cleanOptions.addView(items); updateItemLabel();
        items.setOnClickListener(view -> itemDialog());
        transfer.setOnCheckedChangeListener((view, selected) -> transferOptions.setVisibility(selected ? View.VISIBLE : View.GONE));
        clean.setOnCheckedChangeListener((view, selected) -> cleanOptions.setVisibility(selected ? View.VISIBLE : View.GONE));
        transferOptions.setVisibility(transfer.isChecked() ? View.VISIBLE : View.GONE);
        cleanOptions.setVisibility(clean.isChecked() ? View.VISIBLE : View.GONE);
        space(page, 14);
        advanced = button("Engine settings ▾", false); page.addView(advanced); settings.add(advanced);
        advancedOptions = card(); advancedOptions.setVisibility(View.GONE); page.addView(advancedOptions);
        delay = numeric(advancedOptions, "Action interval · ms", prefs.getInt("delay", 150));
        section(advancedOptions, getString(R.string.session_limits));
        maxCatches = numeric(advancedOptions, getString(R.string.maximum_catches),
                SessionLimits.savedLimit(prefs.getInt("max_catches", SessionLimits.DEFAULT_CATCHES), SessionLimits.DEFAULT_CATCHES));
        maxSpins = numeric(advancedOptions, getString(R.string.maximum_spins),
                SessionLimits.savedLimit(prefs.getInt("max_spins", SessionLimits.DEFAULT_SPINS), SessionLimits.DEFAULT_SPINS));
        advanced.setOnClickListener(view -> {
            boolean visible = advancedOptions.getVisibility() != View.VISIBLE;
            advancedOptions.setVisibility(visible ? View.VISIBLE : View.GONE);
            advanced.setText(visible ? "Engine settings ▴" : "Engine settings ▾");
        });
        LinearLayout controls = column(); controls.setPadding(dp(22), dp(12), dp(22), dp(16)); screen.addView(controls);
        start = button("Start service", true); controls.addView(start);
        start.setOnClickListener(view -> {
            if (FarmService.isRunning()) { startService(new Intent(this, FarmService.class).setAction(FarmService.STOP)); return; }
            if (FarmService.isBusy() || starting) return;
            if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, 601); return;
            }
            begin();
        });
        space(controls, 10);
        export = button("Export logs", false); controls.addView(export); export.setOnClickListener(view -> exportLogs());
        setContentView(screen);
        screen.requestApplyInsets();
    }
    @Override protected void onResume() { super.onResume(); visible = true; handler.removeCallbacks(refresh); refresh.run(); }
    @Override protected void onPause() { visible = false; saveDraft(); handler.removeCallbacks(refresh); super.onPause(); }
    @Override public void onRequestPermissionsResult(int request, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(request, permissions, results);
        if (request == 601 && results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) begin();
        else if (request == 601) { status.setText(R.string.enable_notifications_start);
            Toast.makeText(this, R.string.enable_notifications_start, Toast.LENGTH_LONG).show(); }
    }
    private void begin() {
        if (FarmService.isBusy() || starting || !visible) return;
        try {
            saveSettings();
            if (!FarmService.notificationsVisible(this)) {
                status.setText(R.string.enable_notifications_service);
                startActivity(new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE, getPackageName()));
                return;
            }
            PackageInfo game = getPackageManager().getPackageInfo(EnginePolicy.GAME, 0);
            if (!EnginePolicy.supports(EnginePolicy.GAME, game.versionName, game.getLongVersionCode()))
                throw new IllegalArgumentException("Supported version: GO 0.431.0 / 2026092402");
            Intent service = new Intent(this, FarmService.class).setAction(FarmService.START)
                    .putExtra("resolveError12", resolveError12.isChecked())
                    .putExtra("userApprovedGameStop", resolveError12.isChecked())
                    .putExtra("startupAuthorization", "USER_START_BUTTON")
                    .putExtra("transfer", transfer.isChecked()).putExtra("keepShiny", keepShiny.isChecked())
                    .putExtra("keepHundo", keepHundo.isChecked());
            starting = true; startingAt = android.os.SystemClock.elapsedRealtime(); renderState();
            startForegroundService(service);
            if (!resolveError12.isChecked()) moveTaskToBack(true);
        } catch (Exception error) { starting = false; Toast.makeText(this, error.getMessage(), Toast.LENGTH_LONG).show(); }
    }
    private void saveSettings() {
        FarmPolicy.validate(capture.isChecked(), spin.isChecked(), clean.isChecked(), transfer.isChecked(), limits);
        if (capture.isChecked() && useBerries.isChecked() && BerryOptions.parse(berryOrder).isEmpty())
            throw new IllegalArgumentException(getString(R.string.select_berries));
        int actionDelay = read(delay, 100, 3000, "Interval");
        int catchLimit = read(maxCatches, 1, Integer.MAX_VALUE, getString(R.string.maximum_catches));
        int spinLimit = read(maxSpins, 1, Integer.MAX_VALUE, getString(R.string.maximum_spins));
        prefs.edit().putString("package", EnginePolicy.GAME).putBoolean("internal_engine", true)
                .putString("throw_quality", throwQuality.name())
                .putBoolean("use_berries", useBerries.isChecked()).putString("berry_order", berryOrder)
                .putBoolean("skip_interruptions", skipInterruptions.isChecked())
                .putInt("max_catches", catchLimit).putInt("max_spins", spinLimit)
                .putBoolean("catch", capture.isChecked()).putBoolean("spin", spin.isChecked()).putBoolean("clean", clean.isChecked())
                .putBoolean("resolve_error_12", resolveError12.isChecked())
                .putBoolean("transfer", transfer.isChecked()).putBoolean("keep_shiny", keepShiny.isChecked()).putBoolean("keep_hundo", keepHundo.isChecked())
                .putInt("max_transfers", 0).putString("discard_limits", EnginePolicy.limitsKey(limits))
                .putInt("max_discarded_items", 0).putInt("delay", actionDelay).putInt("max_throws", 0).apply();
    }
    private void saveDraft() {
        if (!FarmService.isBusy() && !starting && prefs != null && capture != null) {
            try { saveSettings(); } catch (IllegalArgumentException ignored) { }
        }
    }
    private static int read(EditText input, int min, int max, String label) { return FarmPolicy.integer(input.getText().toString(), min, max, label); }
    private void openArmedGame() {
        if (!visible || !FarmService.isRunning() || FarmService.isClosing()) return;
        String ticket = FarmService.consumeLaunchTicket();
        if (ticket.isEmpty()) return;
        try {
            Intent launch = getPackageManager().getLaunchIntentForPackage(EnginePolicy.GAME);
            if (launch == null) throw new IllegalStateException("Could not open Pokémon GO");
            FarmService.noteGameLaunch(ticket);
            startActivity(launch);
        } catch (Exception error) {
            startService(new Intent(this, FarmService.class).setAction(FarmService.STOP)
                    .putExtra("reason", "Could not open the game: " + error.getMessage()));
        }
    }
    private void renderState() {
        if (FarmService.isRunning() || (starting && android.os.SystemClock.elapsedRealtime() - startingAt > 3000)) starting = false;
        SessionCounters values = FarmService.counters();
        caught.setText(String.valueOf(values.caught)); spun.setText(String.valueOf(values.spun));
        transferred.setText(String.valueOf(values.transferred)); discarded.setText(String.valueOf(values.discarded));
        xp.setText(values.rewardText(values.xp)); stardust.setText(values.rewardText(values.stardust));
        elapsed.setText(SessionCounters.elapsed(FarmService.elapsed()));
        status.setText(starting ? "Preparing…" : FarmService.status());
        boolean busy = FarmService.isBusy() || starting, closing = FarmService.isClosing();
        for (View setting : settings) { setting.setEnabled(!busy); setting.setAlpha(busy ? 0.5f : 1); }
        start.setText(closing ? "Saving logs…" : FarmService.isRunning() ? "Stop service" : "Start service");
        start.setEnabled(!closing && !starting && (!busy || FarmService.isRunning()));
        export.setEnabled(!busy);
        start.setAlpha(start.isEnabled() ? 1 : 0.5f); export.setAlpha(export.isEnabled() ? 1 : 0.5f);
    }
    private void exportLogs() {
        if (!FarmService.beginExport()) return;
        renderState(); export.setText(R.string.exporting);
        new Thread(() -> {
            String result;
            try { result = LogExporter.export(getApplicationContext()); }
            catch (Exception error) { result = "Logs: " + error.getMessage(); }
            finally { FarmService.endExport(); }
            String message = result;
            handler.post(() -> {
                if (isFinishing() || isDestroyed()) return;
                export.setText(R.string.export_logs); renderState(); Toast.makeText(this, message, Toast.LENGTH_LONG).show();
            });
        }, "rootcatch-export").start();
    }
    private void itemDialog() {
        if (FarmService.isBusy() || starting) return;
        new SettingsDialogs(this).itemsDialog(limits, selected -> {
            limits = selected;
            prefs.edit().putString("discard_limits", selected).apply();
            updateItemLabel();
        });
    }
    private void updateItemLabel() {
        int count = EnginePolicy.parseLimits(limits).size();
        items.setText(getResources().getQuantityString(R.plurals.item_types, count, count));
    }
    private void updateBerryLabel() {
        int count = BerryOptions.parse(berryOrder).size();
        berrySelector.setText(getResources().getQuantityString(R.plurals.berry_types, count, count));
    }
    private Switch toggle(LinearLayout parent, String label, boolean value) {
        Switch control = new Switch(this); control.setText(label); control.setTextColor(TEXT); control.setTextSize(16);
        control.setPadding(dp(8), dp(6), dp(8), dp(6)); control.setMinHeight(dp(56)); control.setChecked(value);
        control.setThumbTintList(new android.content.res.ColorStateList(new int[][]{{android.R.attr.state_checked}, {}}, new int[]{ACCENT, MUTED}));
        control.setTrackTintList(new android.content.res.ColorStateList(new int[][]{{android.R.attr.state_checked}, {}}, new int[]{Color.rgb(34, 104, 85), Color.rgb(62, 74, 91)}));
        parent.addView(control); settings.add(control); return control;
    }
    private CheckBox check(LinearLayout parent, String label, boolean value) {
        CheckBox control = new CheckBox(this); control.setText(label); control.setTextColor(TEXT); control.setTextSize(14);
        control.setMinHeight(dp(48)); control.setChecked(value); control.setButtonTintList(android.content.res.ColorStateList.valueOf(ACCENT));
        parent.addView(control); settings.add(control); return control;
    }
    private EditText numeric(LinearLayout parent, String label, int value) {
        TextView caption = text(label, 13, MUTED); caption.setPadding(dp(6), dp(10), 0, 0); parent.addView(caption);
        EditText input = new EditText(this); input.setInputType(InputType.TYPE_CLASS_NUMBER); input.setSingleLine(true);
        input.setTextColor(TEXT); input.setTextSize(16); input.setText(String.valueOf(value)); input.setContentDescription(label);
        parent.addView(input, new LinearLayout.LayoutParams(-1, dp(52))); settings.add(input); return input;
    }
    private TextView metric(LinearLayout parent, String label) {
        LinearLayout box = card(); LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, -2, 1);
        if (parent.getChildCount() > 0) params.setMarginStart(dp(10)); parent.addView(box, params);
        TextView value = text("0", 30, TEXT); value.setTypeface(Typeface.DEFAULT, Typeface.BOLD); box.addView(value);
        box.addView(text(label, 12, MUTED)); return value;
    }
    private Button button(String label, boolean primary) {
        Button button = new Button(this); button.setText(label); button.setAllCaps(false); button.setTextSize(16);
        button.setTextColor(primary ? BACKGROUND : TEXT); button.setBackground(shape(primary ? ACCENT : PANEL, 14));
        button.setMinHeight(dp(54)); button.setLayoutParams(new LinearLayout.LayoutParams(-1, -2));
        button.setPadding(dp(14), dp(12), dp(14), dp(12)); return button;
    }
    private void section(LinearLayout parent, String label) {
        TextView title = text(label, 15, MUTED); title.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        title.setPadding(0, dp(10), 0, dp(12)); parent.addView(title);
    }
    private TextView text(String label, float size, int color) {
        TextView view = new TextView(this); view.setText(label); view.setTextSize(size); view.setTextColor(color); return view;
    }
    private LinearLayout card() {
        LinearLayout view = column(); view.setBackground(shape(PANEL, 16)); view.setPadding(dp(14), dp(12), dp(14), dp(12)); return view;
    }
    private LinearLayout column() { LinearLayout view = new LinearLayout(this); view.setOrientation(LinearLayout.VERTICAL); return view; }
    private LinearLayout row() { LinearLayout view = new LinearLayout(this); view.setOrientation(LinearLayout.HORIZONTAL); view.setGravity(Gravity.CENTER_VERTICAL); return view; }
    private GradientDrawable shape(int color, int radius) { GradientDrawable drawable = new GradientDrawable(); drawable.setColor(color); drawable.setCornerRadius(dp(radius)); return drawable; }
    private void space(LinearLayout parent, int height) { View gap = new View(this); parent.addView(gap, new LinearLayout.LayoutParams(1, dp(height))); }
    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
}
