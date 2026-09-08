package org.rhythmmaster.player;

import android.app.Activity;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Collections;
import java.util.Locale;
import org.json.JSONArray;
import org.json.JSONObject;

/** Immutable APK catalog loaded on the import worker; no storage picker or graph logic. */
final class EffectCatalog {
    interface Selection { void Select(String asset); }

    static final class Entry {
        String title_ = "";
        String asset_ = "";
        String description_ = "";
        String search_ = "";
        String tier_ = "example";
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
            JSONObject titles = record.getJSONObject("titles");
            JSONObject descriptions = record.getJSONObject("descriptions");
            entry.description_ = descriptions.getString(chinese ? "zh-CN" : "en-US");
            entry.tier_ = record.getString("tier");
            entry.search_ = (record.getString("id") + " " + titles.getString("zh-CN") + " " +
                    titles.getString("en-US") + " " + descriptions.getString("zh-CN") + " " +
                    descriptions.getString("en-US")).toLowerCase(Locale.ROOT);
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
        EffectPicker.Show(activity, Collections.unmodifiableList(entries_), selection);
    }
}
