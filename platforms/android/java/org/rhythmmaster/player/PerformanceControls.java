package org.rhythmmaster.player;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Handler;
import android.os.Looper;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import org.json.JSONArray;
import org.json.JSONObject;
import java.util.ArrayList;
import java.util.Locale;

/** A bounded UI view of the current work's public controls. Native core validates edits. */
final class PerformanceControls {
    private final Activity activity_;
    private final ArrayList<SeekBar> sliders_ = new ArrayList<>();
    private final ArrayList<TextView> labels_ = new ArrayList<>();
    private long generation_ = 0;
    private AlertDialog dialog_ = null;
    private TextView cue_ = null;
    private final Handler handler_ = new Handler(Looper.getMainLooper());
    private final Runnable update_ = new Runnable() {
        @Override public void run() {
            if (dialog_ == null || !dialog_.isShowing()) return;
            Refresh();
            if (dialog_.isShowing()) handler_.postDelayed(this, 200);
        }
    };
    private static native String nativeDescribe();
    private static native boolean nativeValue(long generation, long id, double value);
    private static native boolean nativeBlend(long generation, long first, long second, double amount);
    private static native boolean nativeFollow(long generation);

    private PerformanceControls(Activity activity) { activity_ = activity; }
    static void Show(Activity activity) { new PerformanceControls(activity).Open(); }

    private void Stale() {
        if (dialog_ != null) dialog_.dismiss();
        Toast.makeText(activity_, R.string.controls_changed, Toast.LENGTH_SHORT).show();
    }
    private void Refresh() {
        try {
            JSONObject data = new JSONObject(nativeDescribe());
            if (data.getLong("generation") != generation_) { Stale(); return; }
            if (cue_ != null) cue_.setText(data.optString("cue") +
                    (data.optBoolean("overridden") ? " · " + activity_.getString(R.string.controls_override) : ""));
            JSONArray controls = data.getJSONArray("controls");
            for (int i = 0; i < controls.length(); ++i) {
                JSONObject control = controls.getJSONObject(i);
                double minimum = control.getDouble("minimum");
                double maximum = control.getDouble("maximum");
                double value = control.getDouble("value");
                sliders_.get(i).setProgress((int) Math.round((value - minimum) / (maximum - minimum) * 10000));
                labels_.get(i).setText(String.format(Locale.ROOT, "%s: %.3f", control.getString("title"), value));
            }
        } catch (Exception error) { Stale(); }
    }
    private void Open() {
        try {
            JSONObject data = new JSONObject(nativeDescribe());
            generation_ = data.getLong("generation");
            JSONArray controls = data.getJSONArray("controls");
            if (controls.length() == 0) {
                Toast.makeText(activity_, R.string.controls_empty, Toast.LENGTH_SHORT).show();
                return;
            }
            LinearLayout content = new LinearLayout(activity_);
            content.setOrientation(LinearLayout.VERTICAL);
            content.setPadding(24, 8, 24, 8);
            if (data.optBoolean("automated")) {
                cue_ = new TextView(activity_);
                content.addView(cue_);
                Button follow = new Button(activity_);
                follow.setText(R.string.controls_follow);
                follow.setOnClickListener(view -> {
                    if (!nativeFollow(generation_)) Stale();
                    else Refresh();
                });
                content.addView(follow);
            }
            for (int i = 0; i < controls.length(); ++i) {
                JSONObject control = controls.getJSONObject(i);
                final long id = Long.parseUnsignedLong(control.getString("id"));
                final String title = control.getString("title");
                final double minimum = control.getDouble("minimum");
                final double maximum = control.getDouble("maximum");
                TextView label = new TextView(activity_);
                labels_.add(label);
                content.addView(label);
                SeekBar slider = new SeekBar(activity_);
                slider.setMax(10000);
                slider.setContentDescription(title);
                slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                    @Override public void onProgressChanged(SeekBar bar, int progress, boolean user) {
                        if (!user) return;
                        double value = minimum + (maximum - minimum) * progress / 10000.0;
                        if (!nativeValue(generation_, id, value)) { Stale(); return; }
                        label.setText(String.format(Locale.ROOT, "%s: %.3f", title, value));
                    }
                    @Override public void onStartTrackingTouch(SeekBar bar) {}
                    @Override public void onStopTrackingTouch(SeekBar bar) {}
                });
                sliders_.add(slider);
                content.addView(slider);
            }
            JSONArray snapshots = data.getJSONArray("snapshots");
            if (snapshots.length() > 0) {
                String[] names = new String[snapshots.length()];
                long[] ids = new long[snapshots.length()];
                for (int i = 0; i < snapshots.length(); ++i) {
                    names[i] = snapshots.getJSONObject(i).getString("title");
                    ids[i] = Long.parseUnsignedLong(snapshots.getJSONObject(i).getString("id"));
                }
                Spinner first = SnapshotPicker(content, names, R.string.controls_first);
                Spinner second = SnapshotPicker(content, names, R.string.controls_second);
                if (names.length > 1) second.setSelection(1);
                SeekBar blend = new SeekBar(activity_);
                blend.setMax(10000);
                Button recall = new Button(activity_);
                recall.setText(R.string.controls_recall);
                recall.setOnClickListener(view -> {
                    long id = ids[first.getSelectedItemPosition()];
                    if (!nativeBlend(generation_, id, id, 0)) Stale();
                    else { blend.setProgress(0); Refresh(); }
                });
                content.addView(recall);
                TextView blend_label = new TextView(activity_);
                blend_label.setText(R.string.controls_blend);
                content.addView(blend_label);
                blend.setContentDescription(activity_.getString(R.string.controls_blend));
                blend.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                    @Override public void onProgressChanged(SeekBar bar, int progress, boolean user) {
                        if (!user) return;
                        if (!nativeBlend(generation_, ids[first.getSelectedItemPosition()],
                                         ids[second.getSelectedItemPosition()], progress / 10000.0)) Stale();
                        else Refresh();
                    }
                    @Override public void onStartTrackingTouch(SeekBar bar) {}
                    @Override public void onStopTrackingTouch(SeekBar bar) {}
                });
                content.addView(blend);
            }
            ScrollView scroll = new ScrollView(activity_);
            scroll.addView(content);
            dialog_ = new AlertDialog.Builder(activity_).setTitle(R.string.performance_controls)
                    .setView(scroll).setPositiveButton(android.R.string.ok, null).create();
            dialog_.show();
            dialog_.setOnDismissListener(ignored -> handler_.removeCallbacks(update_));
            Refresh();
            handler_.postDelayed(update_, 200);
        } catch (Exception error) { Stale(); }
    }
    private Spinner SnapshotPicker(LinearLayout content, String[] names, int title) {
        TextView label = new TextView(activity_);
        label.setText(title);
        content.addView(label);
        Spinner picker = new Spinner(activity_);
        ArrayAdapter<String> adapter = new ArrayAdapter<>(activity_, android.R.layout.simple_spinner_item, names);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        picker.setAdapter(adapter);
        content.addView(picker);
        return picker;
    }
}
