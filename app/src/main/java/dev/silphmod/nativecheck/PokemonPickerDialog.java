package dev.silphmod.nativecheck;

import android.app.Activity;
import android.app.Dialog;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.LruCache;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;
import dev.silphmod.nativecheck.R;
import dev.rootcatch.automation.PokemonCatalog;
import dev.rootcatch.automation.PokemonSelection;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.TreeSet;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

final class PokemonPickerDialog {
    private static final int BACKGROUND = Color.rgb(16, 21, 30), PANEL = Color.rgb(26, 35, 48),
            TEXT = Color.rgb(242, 247, 251), MUTED = Color.rgb(153, 170, 189),
            ACCENT = Color.rgb(97, 233, 182), BORDER = Color.rgb(49, 65, 82), SELECTED = Color.rgb(28, 53, 48);
    private final Activity activity;
    private final TreeSet<Integer> selected;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ThreadPoolExecutor images = new ThreadPoolExecutor(1, 1, 10, TimeUnit.SECONDS,
            new LinkedBlockingQueue<>(48), runnable -> {
                Thread worker = new Thread(runnable, "silph-pokemon-icons"); worker.setDaemon(true); return worker;
            }, new ThreadPoolExecutor.DiscardOldestPolicy());
    private final LruCache<Integer, Bitmap> artwork = new LruCache<>(4 * 1024 * 1024) {
        @Override protected int sizeOf(Integer key, Bitmap bitmap) { return bitmap.getAllocationByteCount(); }
    };
    private Dialog dialog;
    private SpeciesAdapter adapter;
    private TextView summary, empty;
    private Button selectedTab, allTab;
    private boolean selectedOnly;
    private String query = "";

    PokemonPickerDialog(Activity activity, String current) {
        this.activity = activity; selected = new TreeSet<>(PokemonSelection.parse(current));
    }
    void show(Consumer<String> save) {
        final List<PokemonCatalog.Entry> catalog;
        try { catalog = PokemonCatalog.load(activity); }
        catch (java.io.IOException error) { images.shutdownNow(); Toast.makeText(activity, R.string.pokemon_catalog_unavailable, Toast.LENGTH_LONG).show(); return; }
        dialog = new Dialog(activity); dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        LinearLayout page = column(); page.setPadding(dp(18), dp(20), dp(18), dp(16));
        page.setBackground(shape(BACKGROUND, BORDER, 22));
        TextView title = text(activity.getString(R.string.only_catch), 22, TEXT); title.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        page.addView(title);
        summary = text("", 13, MUTED); summary.setPadding(0, dp(7), 0, dp(16)); page.addView(summary);
        EditText search = new EditText(activity); search.setSingleLine(true);
        search.setHint(R.string.search_pokemon); search.setContentDescription(activity.getString(R.string.search_pokemon));
        search.setTextColor(TEXT); search.setHintTextColor(MUTED); search.setTextSize(15);
        search.setPadding(dp(14), dp(10), dp(14), dp(10)); search.setBackground(shape(PANEL, BORDER, 13));
        search.setImeOptions(EditorInfo.IME_ACTION_DONE); page.addView(search, new LinearLayout.LayoutParams(-1, dp(52)));
        LinearLayout tabs = row(); tabs.setPadding(0, dp(12), 0, dp(12));
        allTab = button(activity.getString(R.string.all_pokemon), false);
        selectedTab = button("", false);
        Button clear = button(activity.getString(R.string.clear_selection), false);
        tabs.addView(allTab, new LinearLayout.LayoutParams(0, dp(44), 1));
        LinearLayout.LayoutParams tabParams = new LinearLayout.LayoutParams(0, dp(44), 1); tabParams.setMarginStart(dp(7));
        tabs.addView(selectedTab, tabParams);
        LinearLayout.LayoutParams clearParams = new LinearLayout.LayoutParams(-2, dp(44)); clearParams.setMarginStart(dp(7));
        tabs.addView(clear, clearParams); page.addView(tabs);
        ListView list = new ListView(activity); list.setDivider(new ColorDrawable(Color.TRANSPARENT)); list.setDividerHeight(dp(7));
        list.setSelector(new ColorDrawable(Color.TRANSPARENT)); list.setCacheColorHint(Color.TRANSPARENT);
        list.setClipToPadding(false); list.setPadding(0, 0, 0, dp(8));
        adapter = new SpeciesAdapter(catalog); list.setAdapter(adapter);
        page.addView(list, new LinearLayout.LayoutParams(-1, 0, 1));
        empty = text(activity.getString(R.string.no_matching_pokemon), 15, MUTED); empty.setGravity(Gravity.CENTER);
        empty.setPadding(0, dp(14), 0, dp(14)); page.addView(empty, new LinearLayout.LayoutParams(-1, 0, 1)); list.setEmptyView(empty);
        LinearLayout footer = row(); footer.setPadding(0, dp(12), 0, 0);
        Button cancel = button(activity.getString(R.string.cancel), false), apply = button(activity.getString(R.string.save_selection), true);
        footer.addView(cancel, new LinearLayout.LayoutParams(0, dp(52), 1));
        LinearLayout.LayoutParams applyParams = new LinearLayout.LayoutParams(0, dp(52), 1); applyParams.setMarginStart(dp(9));
        footer.addView(apply, applyParams); page.addView(footer);
        cancel.setOnClickListener(view -> dialog.dismiss());
        apply.setOnClickListener(view -> { save.accept(PokemonSelection.encode(selected)); dialog.dismiss(); });
        allTab.setOnClickListener(view -> { selectedOnly = false; adapter.filter(); });
        selectedTab.setOnClickListener(view -> { selectedOnly = true; adapter.filter(); });
        clear.setOnClickListener(view -> { selected.clear(); adapter.filter(); });
        search.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence value, int start, int count, int after) { }
            @Override public void onTextChanged(CharSequence value, int start, int before, int count) { query = value.toString(); adapter.filter(); }
            @Override public void afterTextChanged(Editable value) { }
        });
        dialog.setOnDismissListener(value -> { images.shutdownNow(); artwork.evictAll(); handler.removeCallbacksAndMessages(null); });
        dialog.setContentView(page); dialog.show();
        Window window = dialog.getWindow();
        if (window != null) {
            window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            android.util.DisplayMetrics metrics = activity.getResources().getDisplayMetrics();
            window.setLayout(Math.min(dp(560), metrics.widthPixels - dp(24)), (int)(metrics.heightPixels * 0.88f));
            window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE | WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
        }
        adapter.filter();
    }
    private void updateSummary() {
        summary.setText(selected.isEmpty() ? activity.getString(R.string.only_catch_all_caption)
                : activity.getResources().getQuantityString(R.plurals.pokemon_selected, selected.size(), selected.size()));
        selectedTab.setText(activity.getString(R.string.selected_pokemon, selected.size()));
        allTab.setTextColor(selectedOnly ? MUTED : ACCENT); selectedTab.setTextColor(selectedOnly ? ACCENT : MUTED);
    }
    private final class SpeciesAdapter extends BaseAdapter {
        private final List<PokemonCatalog.Entry> catalog, visible = new ArrayList<>();
        SpeciesAdapter(List<PokemonCatalog.Entry> catalog) { this.catalog = catalog; }
        void filter() {
            visible.clear();
            String key = PokemonCatalog.normalize(query);
            for (PokemonCatalog.Entry entry : catalog) if (entry.matchesKey(key) && (!selectedOnly || selected.contains(entry.id()))) visible.add(entry);
            updateSummary(); notifyDataSetChanged();
        }
        @Override public int getCount() { return visible.size(); }
        @Override public PokemonCatalog.Entry getItem(int position) { return visible.get(position); }
        @Override public long getItemId(int position) { return getItem(position).id(); }
        @Override public boolean hasStableIds() { return true; }
        @Override public View getView(int position, View recycled, ViewGroup parent) {
            Row holder;
            if (recycled == null) { holder = new Row(); recycled = holder.view; recycled.setTag(holder); }
            else holder = (Row)recycled.getTag();
            PokemonCatalog.Entry entry = getItem(position);
            boolean enabled = selected.contains(entry.id());
            holder.view.setBackground(new RippleDrawable(ColorStateList.valueOf(Color.argb(32, 97, 233, 182)),
                    shape(enabled ? SELECTED : PANEL, enabled ? ACCENT : BORDER, 14), null));
            holder.check.setChecked(enabled); holder.check.setContentDescription(entry.name());
            holder.label.setText(activity.getString(R.string.pokemon_catalog_entry, entry.id(), entry.name())); holder.image.setTag(entry.id());
            showArtwork(holder.image, entry);
            holder.view.setOnClickListener(view -> {
                if (!selected.remove(entry.id())) selected.add(entry.id());
                if (selectedOnly) filter(); else { updateSummary(); notifyDataSetChanged(); }
            });
            return recycled;
        }
    }
    private final class Row {
        final LinearLayout view = row();
        final CheckBox check = new CheckBox(activity);
        final ImageView image = new ImageView(activity);
        final TextView label = text("", 16, TEXT);
        Row() {
            view.setPadding(dp(5), dp(9), dp(14), dp(9)); view.setMinimumHeight(dp(72));
            check.setClickable(false); check.setFocusable(false); check.setButtonTintList(ColorStateList.valueOf(ACCENT));
            check.setMinWidth(0); check.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            view.addView(check, new LinearLayout.LayoutParams(dp(42), dp(48)));
            image.setScaleType(ImageView.ScaleType.FIT_CENTER); image.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            view.addView(image, new LinearLayout.LayoutParams(dp(52), dp(52)));
            label.setPadding(dp(12), 0, 0, 0); view.addView(label, new LinearLayout.LayoutParams(0, -2, 1));
        }
    }
    private void showArtwork(ImageView image, PokemonCatalog.Entry entry) {
        Bitmap bitmap = artwork.get(entry.id());
        image.setImageBitmap(bitmap);
        if (bitmap != null || images.isShutdown()) return;
        images.execute(() -> {
            Bitmap loaded = artwork.get(entry.id());
            if (loaded == null) try (InputStream input = activity.getAssets().open(entry.artwork())) {
                BitmapFactory.Options options = new BitmapFactory.Options(); options.inSampleSize = 2;
                loaded = BitmapFactory.decodeStream(input, null, options);
                if (loaded != null && !images.isShutdown()) artwork.put(entry.id(), loaded);
            } catch (java.io.IOException ignored) { }
            Bitmap result = loaded;
            if (!images.isShutdown()) handler.post(() -> {
                if (dialog.isShowing() && Integer.valueOf(entry.id()).equals(image.getTag())) image.setImageBitmap(result);
            });
        });
    }
    private Button button(String value, boolean primary) {
        Button button = new Button(activity); button.setText(value); button.setAllCaps(false); button.setTextSize(14);
        button.setTypeface(Typeface.DEFAULT, Typeface.BOLD); button.setTextColor(primary ? BACKGROUND : TEXT);
        button.setMinWidth(0); button.setMinimumWidth(0); button.setPadding(dp(8), 0, dp(8), 0);
        button.setBackground(new RippleDrawable(ColorStateList.valueOf(Color.argb(32, 97, 233, 182)),
                shape(primary ? ACCENT : PANEL, primary ? ACCENT : BORDER, 12), null));
        return button;
    }
    private TextView text(String value, int size, int color) {
        TextView text = new TextView(activity); text.setText(value); text.setTextSize(size); text.setTextColor(color); return text;
    }
    private LinearLayout column() { LinearLayout column = new LinearLayout(activity); column.setOrientation(LinearLayout.VERTICAL); return column; }
    private LinearLayout row() { LinearLayout row = new LinearLayout(activity); row.setGravity(Gravity.CENTER_VERTICAL); return row; }
    private GradientDrawable shape(int color, int stroke, int radius) {
        GradientDrawable shape = new GradientDrawable(); shape.setColor(color); shape.setCornerRadius(dp(radius)); shape.setStroke(dp(1), stroke); return shape;
    }
    private int dp(int value) { return Math.round(value * activity.getResources().getDisplayMetrics().density); }
}
