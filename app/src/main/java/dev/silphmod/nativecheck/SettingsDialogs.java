package dev.silphmod.nativecheck;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import dev.silphmod.nativecheck.R;
import dev.rootcatch.automation.ThrowQuality;
import dev.rootcatch.automation.BerryOptions;
import dev.rootcatch.nativeengine.ItemCatalog;
import java.util.ArrayList;
import java.util.List;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.function.Consumer;

final class SettingsDialogs {
    private static final int BACKGROUND = Color.rgb(16, 21, 30), PANEL = Color.rgb(26, 35, 48),
            TEXT = Color.rgb(242, 247, 251), MUTED = Color.rgb(153, 170, 189),
            ACCENT = Color.rgb(97, 233, 182), BORDER = Color.rgb(49, 65, 82),
            SELECTED = Color.rgb(28, 53, 48);
    private final Activity activity;

    SettingsDialogs(Activity activity) { this.activity = activity; }

    void medicineDialog(String title, String current, boolean revives, Consumer<String> save) {
        Dialog dialog = dialog();
        int[] stored = dev.rootcatch.nativeengine.RocketCareOrder.parse(current, revives);
        List<Integer> priority = new ArrayList<>();
        for (int id : stored) priority.add(id);
        if (priority.isEmpty()) for (int id : revives ? new int[]{202,201} : new int[]{104,103,102,101}) priority.add(id);
        LinearLayout page = page();
        page.addView(text(title, 23, TEXT));
        TextView caption = text("Uses the first available item in this order. Missing items are skipped.", 13, MUTED);
        caption.setPadding(0, dp(10), 0, dp(14)); page.addView(caption);
        LinearLayout body = column();
        Runnable[] render = new Runnable[1];
        render[0] = () -> {
            body.removeAllViews();
            for (int i = 0; i < priority.size(); i++) {
                int id = priority.get(i), index = i;
                ItemCatalog.Entry entry = ItemCatalog.ENTRIES.stream().filter(item -> item.id() == id).findFirst().orElseThrow();
                LinearLayout line = row(); line.setPadding(dp(10), dp(10), dp(6), dp(10));
                line.setBackground(shape(PANEL, BORDER, 14));
                ImageView image = new ImageView(activity); image.setImageResource(ItemArtwork.resource(id));
                image.setScaleType(ImageView.ScaleType.FIT_CENTER); line.addView(image, new LinearLayout.LayoutParams(dp(34), dp(44)));
                TextView label = text((i + 1) + ". " + entry.label(), 14, TEXT); label.setPadding(dp(10), 0, dp(4), 0);
                line.addView(label, new LinearLayout.LayoutParams(0, -2, 1));
                Button up = button(R.string.move_up_symbol, false), down = button(R.string.move_down_symbol, false);
                up.setContentDescription("Move " + entry.label() + " up"); down.setContentDescription("Move " + entry.label() + " down");
                up.setEnabled(i > 0); down.setEnabled(i + 1 < priority.size());
                up.setAlpha(up.isEnabled() ? 1 : .3f); down.setAlpha(down.isEnabled() ? 1 : .3f);
                line.addView(up, new LinearLayout.LayoutParams(dp(48), dp(48))); line.addView(down, new LinearLayout.LayoutParams(dp(48), dp(48)));
                up.setOnClickListener(v -> { Collections.swap(priority, index, index - 1); render[0].run(); });
                down.setOnClickListener(v -> { Collections.swap(priority, index, index + 1); render[0].run(); });
                LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2); params.bottomMargin = dp(8); body.addView(line, params);
            }
        };
        render[0].run(); scroll(page, body);
        Button automatic = button(R.string.automatic_medicine_priority, false);
        automatic.setOnClickListener(v -> { save.accept(""); dialog.dismiss(); }); page.addView(automatic);
        footer(page, dialog, R.string.apply, () -> {
            int[] ids = priority.stream().mapToInt(Integer::intValue).toArray();
            save.accept(dev.rootcatch.nativeengine.RocketCareOrder.encode(ids)); dialog.dismiss();
        });
        show(dialog, page, revives ? 450 : 620);
    }

    void berriesDialog(String current, Consumer<String> save) {
        Dialog dialog = dialog();
        List<String> priority = new ArrayList<>(BerryOptions.parse(current));
        Set<String> selected = new HashSet<>(priority);
        for (String name : BerryOptions.ALLOWED) if (!priority.contains(name)) priority.add(name);
        LinearLayout page = page();
        header(page, R.string.berry_priority_title);
        TextView summary = text(activity.getString(R.string.berry_priority_caption), 13, MUTED);
        summary.setPadding(0, 0, 0, dp(12)); page.addView(summary);
        LinearLayout body = column();
        Runnable[] render = new Runnable[1];
        render[0] = () -> {
            body.removeAllViews();
            int rank = 0;
            for (int i = 0; i < priority.size(); i++) {
                String name = priority.get(i);
                ItemCatalog.Entry entry = ItemCatalog.ENTRIES.stream().filter(item -> item.name().equals(name)).findFirst()
                        .orElseThrow(() -> new IllegalStateException("Invalid berry catalog"));
                LinearLayout line = row();
                boolean enabled = selected.contains(name);
                line.setPadding(dp(4), dp(10), dp(6), dp(10));
                line.setMinimumHeight(dp(76));
                line.setBackground(ripple(shape(enabled ? SELECTED : PANEL, enabled ? ACCENT : BORDER, 14)));
                CheckBox check = new CheckBox(activity); check.setMinWidth(0); check.setMinHeight(dp(48));
                check.setButtonTintList(ColorStateList.valueOf(ACCENT)); check.setChecked(enabled);
                check.setContentDescription(entry.label());
                line.addView(check, new LinearLayout.LayoutParams(dp(44), dp(48)));
                ImageView image = new ImageView(activity); image.setImageResource(ItemArtwork.resource(entry.id()));
                image.setScaleType(ImageView.ScaleType.FIT_CENTER);
                image.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
                line.addView(image, new LinearLayout.LayoutParams(dp(32), dp(40)));
                TextView label = text((enabled ? ++rank + ". " : "") + entry.label(), 14, TEXT);
                label.setPadding(dp(8), 0, dp(4), 0);
                line.addView(label, new LinearLayout.LayoutParams(0, -2, 1));
                int index = i;
                Button up = button(R.string.move_up_symbol, false), down = button(R.string.move_down_symbol, false);
                up.setTextSize(20); down.setTextSize(20);
                up.setContentDescription(activity.getString(R.string.move_berry_up, entry.label()));
                down.setContentDescription(activity.getString(R.string.move_berry_down, entry.label()));
                up.setEnabled(i > 0); down.setEnabled(i + 1 < priority.size());
                up.setAlpha(up.isEnabled() ? 1 : 0.3f); down.setAlpha(down.isEnabled() ? 1 : 0.3f);
                line.addView(up, new LinearLayout.LayoutParams(dp(48), dp(48)));
                LinearLayout.LayoutParams downParams = new LinearLayout.LayoutParams(dp(48), dp(48)); downParams.setMarginStart(dp(2));
                line.addView(down, downParams);
                up.setOnClickListener(view -> { Collections.swap(priority, index, index - 1); render[0].run(); });
                down.setOnClickListener(view -> { Collections.swap(priority, index, index + 1); render[0].run(); });
                check.setOnCheckedChangeListener((button, checked) -> {
                    if (checked) selected.add(name); else selected.remove(name); render[0].run();
                });
                line.setOnClickListener(view -> check.toggle());
                LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2); params.bottomMargin = dp(8);
                body.addView(line, params);
            }
        };
        render[0].run();
        scroll(page, body);
        footer(page, dialog, R.string.apply, () -> {
            List<String> enabled = new ArrayList<>();
            for (String name : priority) if (selected.contains(name)) enabled.add(name);
            save.accept(String.join(",", enabled)); dialog.dismiss();
        });
        show(dialog, page, 660);
    }

    void throwsDialog(ThrowQuality current, Consumer<ThrowQuality> save) {
        Dialog dialog = dialog();
        LinearLayout page = page();
        header(page, R.string.throw_quality_title);
        LinearLayout body = column();
        List<LinearLayout> rows = new ArrayList<>();
        ThrowQuality[] draft = {current};
        for (ThrowQuality quality : ThrowQuality.values()) {
            LinearLayout line = row();
            line.setPadding(dp(14), dp(12), dp(14), dp(12));
            line.setMinimumHeight(dp(76));
            line.setFocusable(true);
            ThrowTarget target = new ThrowTarget(activity, quality);
            line.addView(target, new LinearLayout.LayoutParams(dp(48), dp(48)));
            TextView label = text(quality.label, 18, TEXT);
            label.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
            LinearLayout.LayoutParams labelParams = new LinearLayout.LayoutParams(0, -2, 1);
            labelParams.setMarginStart(dp(14));
            line.addView(label, labelParams);
            SelectionMark mark = new SelectionMark(activity);
            line.addView(mark, new LinearLayout.LayoutParams(dp(26), dp(26)));
            rows.add(line);
            line.setOnClickListener(view -> {
                draft[0] = quality;
                refreshThrows(rows, draft[0]);
            });
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2);
            params.bottomMargin = dp(10);
            body.addView(line, params);
        }
        refreshThrows(rows, current);
        scroll(page, body);
        footer(page, dialog, R.string.apply, () -> { save.accept(draft[0]); dialog.dismiss(); });
        show(dialog, page, 520);
    }

    void itemsDialog(String limits, Consumer<String> save) {
        Dialog dialog = dialog();
        ItemLimitDraft draft = new ItemLimitDraft(limits);
        LinearLayout page = page();
        header(page, R.string.item_limits_title);
        TextView summary = text("", 13, MUTED);
        summary.setPadding(0, 0, 0, dp(12));
        page.addView(summary);
        updateCount(summary, draft);
        EditText search = new EditText(activity);
        search.setSingleLine(true);
        search.setTextColor(TEXT);
        search.setHintTextColor(MUTED);
        search.setTextSize(16);
        search.setHint(R.string.search_items);
        search.setContentDescription(activity.getString(R.string.search_items));
        search.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        search.setImeOptions(EditorInfo.IME_ACTION_DONE);
        search.setPadding(dp(14), dp(12), dp(14), dp(12));
        search.setBackground(shape(PANEL, BORDER, 14));
        LinearLayout.LayoutParams searchParams = new LinearLayout.LayoutParams(-1, dp(52));
        searchParams.bottomMargin = dp(10);
        page.addView(search, searchParams);
        LinearLayout body = column();
        List<ItemRow> rows = new ArrayList<>();
        List<ItemGroup> groups = new ArrayList<>();
        ItemGroup group = null;
        for (ItemCatalog.Entry entry : ItemCatalog.ENTRIES) {
            if (!entry.discardable()) continue;
            if (group == null || !group.name.equals(entry.group())) {
                TextView caption = text(entry.group(), 13, MUTED);
                caption.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
                caption.setPadding(dp(2), dp(14), 0, dp(10));
                body.addView(caption);
                group = new ItemGroup(entry.group(), caption);
                groups.add(group);
            }
            ItemRow item = itemRow(entry, draft, summary);
            rows.add(item); group.items.add(item);
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2);
            params.bottomMargin = dp(8);
            body.addView(item.view, params);
        }
        TextView empty = text(activity.getString(R.string.no_matching_items), 16, MUTED);
        empty.setGravity(Gravity.CENTER);
        empty.setPadding(0, dp(28), 0, dp(28));
        empty.setVisibility(View.GONE);
        body.addView(empty);
        watch(search, query -> {
            int visible = 0;
            for (ItemRow item : rows) {
                boolean matches = ItemLimitDraft.matches(item.entry, query);
                item.view.setVisibility(matches ? View.VISIBLE : View.GONE);
                if (matches) visible++;
            }
            for (ItemGroup category : groups)
                category.header.setVisibility(category.items.stream().anyMatch(item -> item.view.getVisibility() == View.VISIBLE)
                        ? View.VISIBLE : View.GONE);
            empty.setVisibility(visible == 0 ? View.VISIBLE : View.GONE);
        });
        scroll(page, body);
        footer(page, dialog, R.string.save, () -> {
            try { save.accept(draft.serialize()); dialog.dismiss(); }
            catch (IllegalArgumentException error) { Toast.makeText(activity, error.getMessage(), Toast.LENGTH_SHORT).show(); }
        });
        show(dialog, page, 780);
    }

    private ItemRow itemRow(ItemCatalog.Entry entry, ItemLimitDraft draft, TextView summary) {
        LinearLayout line = row();
        line.setMinimumHeight(dp(76));
        line.setPadding(dp(4), dp(10), dp(10), dp(10));
        CheckBox select = new CheckBox(activity);
        select.setMinWidth(0); select.setMinHeight(dp(48));
        select.setButtonTintList(new ColorStateList(new int[][]{{android.R.attr.state_checked}, {}},
                new int[]{ACCENT, MUTED}));
        select.setContentDescription(entry.label());
        select.setChecked(draft.selected(entry.name()));
        line.addView(select, new LinearLayout.LayoutParams(dp(44), dp(48)));
        ImageView image = new ImageView(activity);
        image.setImageResource(ItemArtwork.resource(entry.id()));
        image.setScaleType(ImageView.ScaleType.FIT_CENTER);
        image.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        line.addView(image, new LinearLayout.LayoutParams(dp(32), dp(40)));
        TextView label = text(entry.label(), 14, TEXT);
        label.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        label.setPadding(dp(10), 0, dp(8), 0);
        line.addView(label, new LinearLayout.LayoutParams(0, -2, 1));
        LinearLayout amount = column();
        TextView caption = text(activity.getString(R.string.keep), 11, MUTED);
        caption.setGravity(Gravity.CENTER); amount.addView(caption);
        EditText keep = new EditText(activity);
        keep.setSingleLine(true);
        keep.setInputType(InputType.TYPE_CLASS_NUMBER);
        keep.setFilters(new InputFilter[]{new InputFilter.LengthFilter(5)});
        keep.setTextSize(16); keep.setTextColor(TEXT); keep.setGravity(Gravity.CENTER);
        keep.setContentDescription(activity.getString(R.string.keep_item, entry.label()));
        keep.setImeOptions(EditorInfo.IME_ACTION_DONE);
        keep.setPadding(dp(4), 0, dp(4), 0);
        keep.setBackground(shape(BACKGROUND, BORDER, 10));
        keep.setText(draft.floor(entry.name()));
        amount.addView(keep, new LinearLayout.LayoutParams(-1, dp(48)));
        line.addView(amount, new LinearLayout.LayoutParams(dp(72), -2));
        Runnable refresh = () -> {
            keep.setEnabled(select.isChecked());
            amount.setAlpha(select.isChecked() ? 1 : 0.45f);
            line.setBackground(ripple(shape(select.isChecked() ? SELECTED : PANEL,
                    select.isChecked() ? ACCENT : BORDER, 14)));
        };
        select.setOnCheckedChangeListener((button, checked) -> {
            draft.select(entry.name(), checked); refresh.run(); updateCount(summary, draft);
        });
        line.setOnClickListener(view -> select.toggle());
        refresh.run();
        watch(keep, value -> draft.floor(entry.name(), value));
        return new ItemRow(entry, line);
    }

    private void refreshThrows(List<LinearLayout> rows, ThrowQuality selected) {
        for (int i = 0; i < rows.size(); i++) {
            LinearLayout line = rows.get(i);
            boolean checked = ThrowQuality.values()[i] == selected;
            line.setSelected(checked);
            line.setContentDescription(ThrowQuality.values()[i].label + (checked
                    ? activity.getString(R.string.selected_suffix) : ""));
            line.setBackground(ripple(shape(checked ? SELECTED : PANEL, checked ? ACCENT : BORDER, 16)));
            ((SelectionMark) line.getChildAt(2)).setChecked(checked);
        }
    }

    private void updateCount(TextView view, ItemLimitDraft draft) {
        view.setText(activity.getResources().getQuantityString(R.plurals.selected_item_types, draft.count(), draft.count()));
    }

    private Dialog dialog() {
        Dialog dialog = new Dialog(activity, R.style.SettingsDialogTheme);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        dialog.setCanceledOnTouchOutside(true);
        return dialog;
    }

    private LinearLayout page() {
        LinearLayout page = column();
        page.setPadding(dp(18), dp(8), dp(18), dp(12));
        page.setBackground(shape(BACKGROUND, BORDER, 24));
        page.setFocusableInTouchMode(true);
        return page;
    }

    private void header(LinearLayout page, int title) {
        TextView header = text(activity.getString(title), 22, TEXT);
        header.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        header.setPadding(0, dp(16), 0, dp(16));
        header.setAccessibilityHeading(true);
        page.addView(header);
    }

    private void scroll(LinearLayout page, LinearLayout body) {
        ScrollView scroll = new ScrollView(activity);
        scroll.setFillViewport(false);
        scroll.setClipToPadding(false);
        scroll.setPadding(0, 0, 0, dp(4));
        scroll.addView(body);
        page.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
    }

    private void footer(LinearLayout page, Dialog dialog, int label, Runnable save) {
        LinearLayout footer = row();
        footer.setPadding(0, dp(12), 0, 0);
        Button cancel = button(R.string.cancel, false);
        cancel.setOnClickListener(view -> dialog.dismiss());
        footer.addView(cancel, new LinearLayout.LayoutParams(0, -2, 1));
        Button apply = button(label, true);
        apply.setOnClickListener(view -> save.run());
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, -2, 1);
        params.setMarginStart(dp(10)); footer.addView(apply, params);
        page.addView(footer);
    }

    private Button button(int label, boolean primary) {
        Button button = new Button(activity);
        button.setText(label); button.setAllCaps(false);
        button.setTextColor(primary ? BACKGROUND : TEXT);
        button.setTextSize(16); button.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        button.setMinWidth(0); button.setMinimumHeight(dp(52));
        button.setPadding(dp(8), dp(12), dp(8), dp(12));
        button.setBackground(ripple(shape(primary ? ACCENT : PANEL, primary ? ACCENT : BORDER, 14)));
        return button;
    }

    private void show(Dialog dialog, LinearLayout page, int height) {
        dialog.setContentView(page);
        dialog.show();
        Window window = dialog.getWindow();
        if (window == null) return;
        android.util.DisplayMetrics metrics = activity.getResources().getDisplayMetrics();
        window.setLayout(Math.min(dp(560), metrics.widthPixels - dp(24)),
                Math.min(dp(height), (int) (metrics.heightPixels * 0.88f)));
        window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE
                | WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
    }

    private TextView text(String value, float size, int color) {
        TextView text = new TextView(activity);
        text.setText(value); text.setTextColor(color); text.setTextSize(size);
        return text;
    }
    private LinearLayout column() { LinearLayout view = new LinearLayout(activity); view.setOrientation(LinearLayout.VERTICAL); return view; }
    private LinearLayout row() { LinearLayout view = new LinearLayout(activity); view.setGravity(Gravity.CENTER_VERTICAL); return view; }
    private GradientDrawable shape(int color, int stroke, int radius) {
        GradientDrawable shape = new GradientDrawable(); shape.setColor(color);
        shape.setCornerRadius(dp(radius)); shape.setStroke(dp(1), stroke); return shape;
    }
    private RippleDrawable ripple(GradientDrawable shape) {
        return new RippleDrawable(ColorStateList.valueOf(Color.argb(32, 97, 233, 182)), shape, null);
    }
    private int dp(int value) { return Math.round(value * activity.getResources().getDisplayMetrics().density); }
    private static void watch(EditText input, Consumer<String> changed) {
        input.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence text, int start, int count, int after) { }
            @Override public void onTextChanged(CharSequence text, int start, int before, int count) { changed.accept(text.toString()); }
            @Override public void afterTextChanged(Editable text) { }
        });
    }

    private static final class ItemRow {
        final ItemCatalog.Entry entry; final LinearLayout view;
        ItemRow(ItemCatalog.Entry entry, LinearLayout view) { this.entry = entry; this.view = view; }
    }
    private static final class ItemGroup {
        final String name; final TextView header; final List<ItemRow> items = new ArrayList<>();
        ItemGroup(String name, TextView header) { this.name = name; this.header = header; }
    }

    private static final class SelectionMark extends View {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private boolean checked;
        SelectionMark(Context context) { super(context); setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO); }
        void setChecked(boolean checked) { this.checked = checked; invalidate(); }
        @Override protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            float center = getWidth() / 2f, radius = Math.min(getWidth(), getHeight()) * 0.36f;
            paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(getResources().getDisplayMetrics().density * 2);
            paint.setColor(checked ? ACCENT : MUTED);
            canvas.drawCircle(center, getHeight() / 2f, radius, paint);
            if (checked) { paint.setStyle(Paint.Style.FILL); canvas.drawCircle(center, getHeight() / 2f, radius * 0.5f, paint); }
        }
    }

    private static final class ThrowTarget extends View {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final ThrowQuality quality;
        ThrowTarget(Context context, ThrowQuality quality) {
            super(context); this.quality = quality; setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        }
        @Override protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            float x = getWidth() / 2f, y = getHeight() / 2f, radius = Math.min(getWidth(), getHeight()) * 0.4f;
            paint.setStyle(Paint.Style.FILL); paint.setColor(PANEL); canvas.drawCircle(x, y, radius * 1.12f, paint);
            paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(getResources().getDisplayMetrics().density * 2);
            paint.setColor(MUTED); canvas.drawCircle(x, y, radius, paint);
            float[] ratios = {0.91f, 0.73f, 0.48f, 0.2f};
            paint.setColor(ACCENT); canvas.drawCircle(x, y, radius * ratios[quality.ordinal()], paint);
            paint.setStyle(Paint.Style.FILL); canvas.drawCircle(x, y, getResources().getDisplayMetrics().density * 2, paint);
        }
    }
}
