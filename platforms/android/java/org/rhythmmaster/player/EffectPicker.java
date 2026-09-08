package org.rhythmmaster.player;

import android.app.Activity;
import android.app.AlertDialog;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.SearchView;
import android.widget.Spinner;
import android.widget.TextView;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Native offline catalog UI. Filtering changes only visible immutable entries. */
final class EffectPicker {
    private final Activity activity_;
    private final List<EffectCatalog.Entry> entries_;
    private final List<EffectCatalog.Entry> visible_ = new ArrayList<>();
    private final EffectCatalog.Selection selection_;
    private AlertDialog dialog_ = null;
    private BaseAdapter adapter_ = null;
    private TextView summary_ = null;
    private ListView list_ = null;
    private String query_ = "";
    private int tier_ = 0;
    private int shape_ = 0;

    private EffectPicker(Activity activity, List<EffectCatalog.Entry> entries,
                         EffectCatalog.Selection selection) {
        activity_ = activity;
        entries_ = entries;
        selection_ = selection;
    }
    static void Show(Activity activity, List<EffectCatalog.Entry> entries,
                     EffectCatalog.Selection selection) {
        new EffectPicker(activity, entries, selection).Open();
    }
    private int Pixels(int dp) {
        return Math.round(dp * activity_.getResources().getDisplayMetrics().density);
    }
    private void Refresh() {
        visible_.clear();
        String[] words = query_.trim().toLowerCase(Locale.ROOT).split("\\s+");
        String[] tiers = {"", "basic", "advanced", "example"};
        for (EffectCatalog.Entry entry : entries_) {
            int shape = entry.width_ > entry.height_ ? 1 : entry.width_ < entry.height_ ? 2 : 3;
            if (tier_ != 0 && !tiers[tier_].equals(entry.tier_)) continue;
            if (shape_ != 0 && shape_ != shape) continue;
            boolean matches = true;
            for (String word : words) if (!entry.search_.contains(word)) matches = false;
            if (matches) visible_.add(entry);
        }
        if (adapter_ == null) return;
        adapter_.notifyDataSetChanged();
        list_.setSelection(0);
        summary_.setText(activity_.getString(R.string.catalog_count, visible_.size(), entries_.size()));
    }
    private Spinner Filter(LinearLayout row, int description, int[] labels, boolean tier) {
        Spinner spinner = new Spinner(activity_);
        spinner.setContentDescription(activity_.getString(description));
        List<String> names = new ArrayList<>();
        for (int label : labels) names.add(activity_.getString(label));
        ArrayAdapter<String> choices = new ArrayAdapter<>(activity_, android.R.layout.simple_spinner_item, names);
        choices.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(choices);
        spinner.setMinimumHeight(Pixels(44));
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onNothingSelected(AdapterView<?> parent) {}
            @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                if (tier) tier_ = position; else shape_ = position;
                Refresh();
            }
        });
        row.addView(spinner, new LinearLayout.LayoutParams(0, -2, 1));
        return spinner;
    }
    private int TierLabel(String tier) {
        return "basic".equals(tier) ? R.string.catalog_basic :
                "advanced".equals(tier) ? R.string.catalog_advanced : R.string.catalog_example;
    }
    private View Row(int position, View recycled) {
        LinearLayout row;
        if (recycled instanceof LinearLayout) {
            row = (LinearLayout) recycled;
        } else {
            row = new LinearLayout(activity_);
            row.setGravity(Gravity.CENTER_VERTICAL);
            row.setPadding(Pixels(4), Pixels(6), Pixels(4), Pixels(6));
            ImageView image = new ImageView(activity_);
            image.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            image.setScaleType(ImageView.ScaleType.FIT_CENTER);
            row.addView(image, new LinearLayout.LayoutParams(Pixels(96), Pixels(54)));
            LinearLayout labels = new LinearLayout(activity_);
            labels.setOrientation(LinearLayout.VERTICAL);
            labels.setPadding(Pixels(12), 0, 0, 0);
            TextView title = new TextView(activity_);
            title.setTextSize(16);
            labels.addView(title);
            TextView description = new TextView(activity_);
            description.setTextSize(12);
            description.setMaxLines(2);
            description.setEllipsize(android.text.TextUtils.TruncateAt.END);
            labels.addView(description);
            row.addView(labels, new LinearLayout.LayoutParams(0, -2, 1));
        }
        EffectCatalog.Entry entry = visible_.get(position);
        ((ImageView) row.getChildAt(0)).setImageBitmap(entry.thumbnail_);
        LinearLayout labels = (LinearLayout) row.getChildAt(1);
        int shape = entry.width_ > entry.height_ ? R.string.landscape_effect :
                entry.width_ < entry.height_ ? R.string.portrait_effect : R.string.square_effect;
        ((TextView) labels.getChildAt(0)).setText(entry.title_ + " · " + activity_.getString(TierLabel(entry.tier_)));
        ((TextView) labels.getChildAt(1)).setText(activity_.getString(shape) +
                (entry.audio_ ? " · " + activity_.getString(R.string.music_driven) : "") +
                (entry.description_.isEmpty() ? "" : "\n" + entry.description_));
        return row;
    }
    private void Open() {
        LinearLayout body = new LinearLayout(activity_);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(Pixels(12), 0, Pixels(12), 0);
        body.setFocusableInTouchMode(true);
        SearchView search = new SearchView(activity_);
        search.setIconifiedByDefault(false);
        search.setQueryHint(activity_.getString(R.string.catalog_search));
        search.setImeOptions(android.view.inputmethod.EditorInfo.IME_ACTION_SEARCH |
                android.view.inputmethod.EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        search.setMaxWidth(Integer.MAX_VALUE);
        body.addView(search, new LinearLayout.LayoutParams(-1, -2));
        LinearLayout filters = new LinearLayout(activity_);
        Filter(filters, R.string.catalog_tier, new int[]{R.string.catalog_all_tiers,
                R.string.catalog_basic, R.string.catalog_advanced, R.string.catalog_example}, true);
        Filter(filters, R.string.catalog_shape, new int[]{R.string.catalog_all_shapes,
                R.string.landscape_effect, R.string.portrait_effect, R.string.square_effect}, false);
        body.addView(filters, new LinearLayout.LayoutParams(-1, -2));
        summary_ = new TextView(activity_);
        summary_.setTextSize(12);
        body.addView(summary_, new LinearLayout.LayoutParams(-1, -2));
        LinearLayout results = new LinearLayout(activity_);
        results.setGravity(Gravity.CENTER);
        list_ = new ListView(activity_);
        adapter_ = new BaseAdapter() {
            @Override public int getCount() { return visible_.size(); }
            @Override public Object getItem(int position) { return visible_.get(position); }
            @Override public long getItemId(int position) { return position; }
            @Override public View getView(int position, View recycled, ViewGroup parent) {
                return Row(position, recycled);
            }
        };
        list_.setAdapter(adapter_);
        results.addView(list_, new LinearLayout.LayoutParams(-1, -1));
        TextView empty = new TextView(activity_);
        empty.setGravity(Gravity.CENTER);
        empty.setText(R.string.catalog_empty);
        results.addView(empty, new LinearLayout.LayoutParams(-1, -1));
        list_.setEmptyView(empty);
        body.addView(results, new LinearLayout.LayoutParams(-1, 0, 1));
        search.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            @Override public boolean onQueryTextSubmit(String value) {
                search.clearFocus();
                android.view.inputmethod.InputMethodManager keyboard = activity_.getSystemService(
                        android.view.inputmethod.InputMethodManager.class);
                if (keyboard != null) keyboard.hideSoftInputFromWindow(search.getWindowToken(), 0);
                return true;
            }
            @Override public boolean onQueryTextChange(String value) {
                query_ = value;
                Refresh();
                return true;
            }
        });
        dialog_ = new AlertDialog.Builder(activity_).setTitle(R.string.choose_effect).setView(body)
                .setNegativeButton(android.R.string.cancel, null).create();
        list_.setOnItemClickListener((parent, view, position, id) -> {
            String asset = visible_.get(position).asset_;
            dialog_.dismiss();
            selection_.Select(asset);
        });
        dialog_.show();
        android.graphics.Rect available = new android.graphics.Rect();
        activity_.getWindow().getDecorView().getWindowVisibleDisplayFrame(available);
        dialog_.getWindow().setLayout(Math.min(Pixels(880), available.width() - Pixels(24)),
                available.height() - Pixels(24));
        dialog_.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE |
                WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
        body.requestFocus();
        Refresh();
    }
}
