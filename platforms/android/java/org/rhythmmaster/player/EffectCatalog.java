package org.rhythmmaster.player;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import org.json.JSONArray;
import org.json.JSONObject;

/** Immutable APK catalog loaded on the import worker; no storage picker or graph logic. */
final class EffectCatalog {
    interface Selection { void Select(String asset); }

    private static final class Entry {
        String title_ = "";
        String asset_ = "";
        int width_ = 0;
        int height_ = 0;
        boolean audio_ = false;
        Bitmap thumbnail_ = null;
    }

    private final List<Entry> entries_ = new ArrayList<>();

    static EffectCatalog Load(AssetManager assets, boolean chinese) throws Exception {
        EffectCatalog catalog = new EffectCatalog();
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (InputStream input = assets.open("effects/catalog.json")) {
            byte[] buffer = new byte[4096];
            int count;
            while ((count = input.read(buffer)) != -1) {
                if (bytes.size() + count > 256 * 1024) throw new java.io.IOException("Catalog limit");
                bytes.write(buffer, 0, count);
            }
        }
        JSONArray records = new JSONArray(new String(bytes.toByteArray(), StandardCharsets.UTF_8));
        if (records.length() == 0 || records.length() > 256) throw new java.io.IOException("Catalog size");
        for (int index = 0; index < records.length(); ++index) {
            JSONObject record = records.getJSONObject(index);
            Entry entry = new Entry();
            entry.title_ = record.getJSONObject("titles").getString(chinese ? "zh-CN" : "en-US");
            entry.asset_ = record.getString("package");
            entry.width_ = record.getJSONObject("canvas").getInt("width");
            entry.height_ = record.getJSONObject("canvas").getInt("height");
            entry.audio_ = record.getBoolean("audio");
            if (record.has("thumbnail")) {
                try (InputStream input = assets.open(record.getString("thumbnail"))) {
                    entry.thumbnail_ = BitmapFactory.decodeStream(input);
                }
            }
            catalog.entries_.add(entry);
        }
        return catalog;
    }

    String Title(String asset) {
        for (Entry entry : entries_) if (entry.asset_.equals(asset)) return entry.title_;
        return asset;
    }

    void Show(Activity activity, Selection selection) {
        final int preview_width = (int) (96 * activity.getResources().getDisplayMetrics().density);
        BaseAdapter adapter = new BaseAdapter() {
            @Override public int getCount() { return entries_.size(); }
            @Override public Object getItem(int position) { return entries_.get(position); }
            @Override public long getItemId(int position) { return position; }
            @Override public View getView(int position, View recycled, ViewGroup parent) {
                LinearLayout row;
                if (recycled instanceof LinearLayout) {
                    row = (LinearLayout) recycled;
                } else {
                    row = new LinearLayout(activity);
                    row.setGravity(android.view.Gravity.CENTER_VERTICAL);
                    row.setPadding(16, 8, 16, 8);
                    ImageView preview = new ImageView(activity);
                    preview.setScaleType(ImageView.ScaleType.FIT_CENTER);
                    row.addView(preview, new LinearLayout.LayoutParams(preview_width, preview_width * 9 / 16));
                    TextView text = new TextView(activity);
                    text.setTextSize(16);
                    text.setPadding(20, 0, 0, 0);
                    row.addView(text, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
                }
                Entry entry = entries_.get(position);
                ((ImageView) row.getChildAt(0)).setImageBitmap(entry.thumbnail_);
                int shape = entry.width_ > entry.height_ ? R.string.landscape_effect :
                        entry.width_ < entry.height_ ? R.string.portrait_effect : R.string.square_effect;
                ((TextView) row.getChildAt(1)).setText(entry.title_ + "\n" + activity.getString(shape) +
                        (entry.audio_ ? " · " + activity.getString(R.string.music_driven) : ""));
                return row;
            }
        };
        new AlertDialog.Builder(activity).setTitle(R.string.choose_effect)
                .setAdapter(adapter, (dialog, index) -> selection.Select(entries_.get(index).asset_))
                .setNegativeButton(android.R.string.cancel, null).show();
    }
}
