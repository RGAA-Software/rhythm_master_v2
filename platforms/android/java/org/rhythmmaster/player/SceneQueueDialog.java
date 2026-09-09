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
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Locale;
import org.json.JSONArray;
import org.json.JSONObject;

/** Snapshot view of the host-owned bounded performance queue. */
final class SceneQueueDialog {
    private final Activity activity_;
    private final Runnable add_;
    private final Handler handler_ = new Handler(Looper.getMainLooper());
    private final ArrayList<Long> ids_ = new ArrayList<>();
    private AlertDialog dialog_ = null;
    private Spinner rows_ = null;
    private TextView status_ = null;
    private Button go_ = null;
    private Button cancel_ = null;
    private Button cancel_pending_ = null;
    private TextView timing_ = null;
    private TextView duration_label_ = null;
    private SeekBar duration_slider_ = null;
    private boolean program_entry_ = false;
    private long head_ = 0;
    // Main UI thread preference, retained when the dialog is reopened.
    private static double last_duration = 1;
    private double duration_ = last_duration;
    private String rows_json_ = "";
    private final Runnable update_ = new Runnable() {
        @Override public void run() {
            if (dialog_ == null || !dialog_.isShowing()) return;
            Refresh();
            handler_.postDelayed(this, 200);
        }
    };
    private static native String nativeDescribe();
    private static native boolean nativeEnqueue(byte[] request);
    private static native void nativeAction(int action, long id, double duration);

    static boolean Enqueue(String path, String title) throws Exception {
        return nativeEnqueue(new JSONObject().put("path", path).put("title", title)
                .toString().getBytes(StandardCharsets.UTF_8));
    }
    private SceneQueueDialog(Activity activity, Runnable add) { activity_ = activity; add_ = add; }
    static void Show(Activity activity, Runnable add) { new SceneQueueDialog(activity, add).Open(); }
    private Button Button(LinearLayout row, int label, Runnable action) {
        Button button = new Button(activity_);
        button.setText(label);
        button.setOnClickListener(view -> action.run());
        row.addView(button, new LinearLayout.LayoutParams(0, -2, 1));
        return button;
    }
    private long Selected() {
        int index = rows_.getSelectedItemPosition();
        return index >= 0 && index < ids_.size() ? ids_.get(index) : 0;
    }
    private void Refresh() {
        try {
            JSONObject data = new JSONObject(nativeDescribe());
            JSONObject timing = new JSONObject(PerformanceControls.nativeDescribe());
            timing_.setText(BeatControls.Summary(activity_, timing));
            cancel_pending_.setEnabled(BeatControls.HasPendingScene(timing));
            JSONArray items = data.optJSONArray("items");
            if (items == null) return;
            if (!items.toString().equals(rows_json_)) {
                long selected = Selected();
                ids_.clear();
                ArrayList<String> labels = new ArrayList<>();
                int[] states = {R.string.scene_waiting, R.string.scene_loading,
                        R.string.scene_cpu_ready, R.string.scene_failed,
                        R.string.scene_gpu_preparing, R.string.scene_ready};
                int[] resolutions = {R.string.program_exact, R.string.program_updated,
                        R.string.program_missing, R.string.program_changed, R.string.program_ambiguous};
                for (int i = 0; i < items.length(); ++i) {
                    JSONObject item = items.getJSONObject(i);
                    ids_.add(Long.parseLong(item.getString("id")));
                    String label = item.getString("title") + " · " + activity_.getString(states[item.getInt("state")]);
                    if (item.optBoolean("program_entry")) label += " · " + activity_.getString(resolutions[item.getInt("resolution")]);
                    if (!item.optString("resolution_error").isEmpty()) label += " · " + item.getString("resolution_error");
                    if (!item.optString("preparation_error").isEmpty()) label += " · " + item.getString("preparation_error");
                    labels.add(label);
                }
                ArrayAdapter<String> adapter = new ArrayAdapter<>(activity_, android.R.layout.simple_spinner_item, labels);
                adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
                rows_.setAdapter(adapter);
                int restored = ids_.indexOf(selected);
                if (restored >= 0) rows_.setSelection(restored);
                rows_json_ = items.toString();
            }
            head_ = ids_.isEmpty() ? 0 : ids_.get(0);
            boolean program_entry = data.has("entry_duration");
            duration_slider_.setEnabled(!program_entry);
            if (program_entry) {
                duration_slider_.setProgress((int) Math.round(data.getDouble("entry_duration") * 20));
                int[] modes = {R.string.beat_immediate, R.string.beat_next_beat, R.string.beat_next_bar};
                duration_label_.setText(activity_.getString(R.string.program_entry_settings) + " · " +
                        activity_.getString(R.string.scene_duration, data.getDouble("entry_duration")) + " · " +
                        activity_.getString(modes[data.getInt("entry_mode")]));
            } else if (program_entry_) {
                duration_slider_.setProgress((int) Math.round(duration_ * 20));
                duration_label_.setText(activity_.getString(R.string.scene_duration, duration_));
            }
            program_entry_ = program_entry;
            go_.setEnabled(data.optBoolean("can_go"));
            cancel_.setEnabled(data.optBoolean("transitioning") || data.optBoolean("gpu_preparing"));
            int error = data.optInt("error");
            status_.setText(error != 0 ? activity_.getString(error == 3 ? R.string.scene_budget :
                    error == 6 ? R.string.scene_audio_failed : R.string.scene_interrupted) +
                    (data.optString("error_detail").isEmpty() ? "" : "\n" + data.optString("error_detail")) :
                    data.optBoolean("gpu_preparing") ? activity_.getString(R.string.scene_gpu_preparing) +
                    " · " + data.optInt("prepared_nodes") + " / " + data.optInt("total_nodes") :
                    data.optBoolean("audio_pending") && !data.optBoolean("transitioning") ?
                    activity_.getString(R.string.scene_audio_recover) :
                    data.optBoolean("audio_pending") && data.optDouble("progress") == 0 ?
                    activity_.getString(R.string.scene_audio_wait) :
                    data.optBoolean("transitioning") ? String.format(Locale.ROOT, "%s · %.0f%%",
                    data.optString("incoming"), data.optDouble("progress") * 100) :
                    activity_.getString(R.string.scene_help));
        } catch (Exception error) { status_.setText(R.string.scene_interrupted); }
    }
    private void Open() {
        LinearLayout content = new LinearLayout(activity_);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(24, 8, 24, 8);
        status_ = new TextView(activity_);
        status_.setMinLines(2);
        content.addView(status_);
        timing_ = new TextView(activity_);
        content.addView(timing_);
        LinearLayout timing_actions = new LinearLayout(activity_);
        Button(timing_actions, R.string.beat_settings, () -> {
            dialog_.dismiss();
            PerformanceControls.Show(activity_);
        });
        cancel_pending_ = Button(timing_actions, R.string.beat_cancel_scene, () -> BeatControls.CancelScene());
        content.addView(timing_actions);
        LinearLayout add = new LinearLayout(activity_);
        Button(add, R.string.scene_enqueue, add_);
        content.addView(add);
        rows_ = new Spinner(activity_);
        content.addView(rows_);
        LinearLayout edit = new LinearLayout(activity_);
        Button(edit, R.string.scene_remove, () -> nativeAction(2, Selected(), duration_));
        Button(edit, R.string.scene_clear, () -> nativeAction(3, 0, duration_));
        Button(edit, R.string.scene_retry, () -> nativeAction(4, head_, duration_));
        content.addView(edit);
        TextView duration = new TextView(activity_);
        duration_label_ = duration;
        duration.setText(activity_.getString(R.string.scene_duration, duration_));
        content.addView(duration);
        SeekBar slider = new SeekBar(activity_);
        duration_slider_ = slider;
        slider.setMax(100);
        slider.setProgress((int) Math.round(duration_ * 20));
        slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar bar, int progress, boolean fromUser) {
                if (fromUser) {
                    duration_ = progress / 20.0;
                    last_duration = duration_;
                    duration.setText(activity_.getString(R.string.scene_duration, duration_));
                }
            }
            @Override public void onStartTrackingTouch(SeekBar bar) {}
            @Override public void onStopTrackingTouch(SeekBar bar) {}
        });
        content.addView(slider);
        LinearLayout actions = new LinearLayout(activity_);
        go_ = Button(actions, R.string.scene_go, () -> nativeAction(1, head_, duration_));
        cancel_ = Button(actions, R.string.scene_cancel, () -> nativeAction(5, 0, duration_));
        content.addView(actions);
        ScrollView scroll = new ScrollView(activity_);
        scroll.addView(content);
        dialog_ = new AlertDialog.Builder(activity_).setTitle(R.string.scene_queue).setView(scroll)
                .setNegativeButton(android.R.string.cancel, null).create();
        dialog_.setOnDismissListener(dialog -> handler_.removeCallbacks(update_));
        dialog_.show();
        handler_.post(update_);
    }
}
