package org.rhythmmaster.player;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Locale;
import org.json.JSONArray;
import org.json.JSONObject;

/** Native value snapshots and gestures; persistence and resolution use the shared core. */
final class PerformanceProgramDialog {
    private static double last_duration = 1;
    private static int last_mode = 0;
    private static boolean last_follow = true;
    private final Activity activity_;
    private final Handler handler_ = new Handler(Looper.getMainLooper());
    private final ArrayList<String> ids_ = new ArrayList<>();
    private final ArrayList<Button> buttons_ = new ArrayList<>();
    private AlertDialog dialog_ = null;
    private TextView status_ = null;
    private TextView duration_text_ = null;
    private Spinner rows_ = null;
    private Spinner mode_ = null;
    private SeekBar duration_ = null;
    private CheckBox follow_ = null;
    private Button cancel_ = null;
    private JSONArray entries_ = new JSONArray();
    private String rows_json_ = "";
    private final Runnable update_ = new Runnable() {
        @Override public void run() {
            if (dialog_ == null || !dialog_.isShowing()) return;
            Refresh();
            handler_.postDelayed(this, 200);
        }
    };
    private static native String nativeDescribe();
    private static native boolean nativeAction(byte[] request);
    private static boolean Request(JSONObject request) {
        return nativeAction(request.toString().getBytes(StandardCharsets.UTF_8));
    }
    static boolean AddBuiltin(String content_id, String title) {
        try {
            return Request(new JSONObject().put("action", 9).put("content_id", content_id)
                    .put("title", title).put("duration", last_duration).put("mode", last_mode)
                    .put("follow", last_follow));
        } catch (Exception error) { return false; }
    }
    static boolean Import(String path) {
        try {
            return Request(new JSONObject().put("action", 10).put("path", path)
                    .put("duration", last_duration).put("mode", last_mode));
        } catch (Exception error) { return false; }
    }
    private PerformanceProgramDialog(Activity activity) { activity_ = activity; }
    static void Show(Activity activity, Runnable add, Runnable imported, Runnable queue) {
        new PerformanceProgramDialog(activity).Open(add, imported, queue);
    }
    private String Selected() {
        int index = rows_.getSelectedItemPosition();
        return index >= 0 && index < ids_.size() ? ids_.get(index) : "0";
    }
    private Button Button(LinearLayout row, int text, Runnable action) {
        Button button = new Button(activity_);
        button.setText(text);
        button.setOnClickListener(view -> action.run());
        row.addView(button, new LinearLayout.LayoutParams(0, -2, 1));
        buttons_.add(button);
        return button;
    }
    private void Send(int action) {
        try {
            if (!Request(new JSONObject().put("action", action).put("id", Selected())
                    .put("duration", duration_.getProgress() / 20.0)
                    .put("mode", mode_.getSelectedItemPosition()).put("follow", follow_.isChecked())))
                Toast.makeText(activity_, R.string.program_rejected, Toast.LENGTH_SHORT).show();
        } catch (Exception error) {
            Toast.makeText(activity_, R.string.program_rejected, Toast.LENGTH_SHORT).show();
        }
    }
    private void SelectSettings(int index) {
        try {
            if (index < 0 || index >= entries_.length()) return;
            JSONObject entry = entries_.getJSONObject(index);
            duration_.setProgress((int) Math.round(entry.getDouble("transition_seconds") * 20));
            String mode = entry.getString("quantization");
            mode_.setSelection(mode.equals("bar") ? 2 : mode.equals("beat") ? 1 : 0);
            follow_.setChecked(entry.getJSONObject("work").getString("policy").equals("current_builtin"));
        } catch (Exception error) { status_.setText(R.string.program_rejected); }
    }
    private void Refresh() {
        try {
            JSONObject data = new JSONObject(nativeDescribe());
            JSONArray entries = data.optJSONArray("entries");
            if (entries == null) return;
            if (!entries.toString().equals(rows_json_)) {
                String selected = Selected();
                boolean added = entries.length() > entries_.length();
                entries_ = entries;
                ids_.clear();
                ArrayList<String> labels = new ArrayList<>();
                for (int index = 0; index < entries.length(); ++index) {
                    JSONObject entry = entries.getJSONObject(index);
                    ids_.add(entry.getString("id"));
                    labels.add((index + 1) + ". " + entry.getString("title"));
                }
                ArrayAdapter<String> adapter = new ArrayAdapter<>(activity_, android.R.layout.simple_spinner_item, labels);
                adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
                rows_.setAdapter(adapter);
                int restored = added ? ids_.size() - 1 : ids_.indexOf(selected);
                if (restored >= 0) rows_.setSelection(restored);
                rows_json_ = entries.toString();
            }
            boolean busy = data.optBoolean("busy");
            for (Button button : buttons_) button.setEnabled(!busy);
            cancel_.setEnabled(busy);
            rows_.setEnabled(!busy);
            duration_.setEnabled(!busy);
            mode_.setEnabled(!busy);
            follow_.setEnabled(!busy);
            String error = data.optString("error");
            status_.setText(!error.isEmpty() ? activity_.getString(R.string.program_failed) + "\n" + error :
                    activity_.getString(busy ? R.string.program_busy : data.optBoolean("dirty") ?
                            R.string.program_unsaved : data.optBoolean("saved") ? R.string.program_saved : R.string.program_new));
        } catch (Exception error) { status_.setText(R.string.program_rejected); }
    }
    private void Open(Runnable add, Runnable imported, Runnable queue) {
        LinearLayout content = new LinearLayout(activity_);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(24, 8, 24, 8);
        TextView help = new TextView(activity_);
        help.setText(R.string.program_help);
        content.addView(help);
        status_ = new TextView(activity_);
        content.addView(status_);
        LinearLayout persistence = new LinearLayout(activity_);
        Button(persistence, R.string.program_save, () -> Send(2));
        Button(persistence, R.string.program_reopen, () -> Send(1));
        Button(persistence, R.string.program_prepare, () -> Send(3));
        content.addView(persistence);
        LinearLayout sources = new LinearLayout(activity_);
        Button(sources, R.string.program_add, add);
        Button(sources, R.string.program_import, imported);
        content.addView(sources);
        rows_ = new Spinner(activity_);
        content.addView(rows_);
        LinearLayout edit = new LinearLayout(activity_);
        Button(edit, R.string.program_up, () -> Send(5));
        Button(edit, R.string.program_down, () -> Send(6));
        Button(edit, R.string.program_duplicate, () -> Send(7));
        Button(edit, R.string.program_remove, () -> Send(4));
        content.addView(edit);
        duration_text_ = new TextView(activity_);
        duration_text_.setText(activity_.getString(R.string.scene_duration, last_duration));
        content.addView(duration_text_);
        duration_ = new SeekBar(activity_);
        duration_.setContentDescription(activity_.getString(R.string.program_duration));
        duration_.setMax(100);
        duration_.setProgress((int) Math.round(last_duration * 20));
        duration_.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar bar, int value, boolean from_user) {
                duration_text_.setText(activity_.getString(R.string.scene_duration, value / 20.0));
                if (from_user) last_duration = value / 20.0;
            }
            @Override public void onStartTrackingTouch(SeekBar bar) {}
            @Override public void onStopTrackingTouch(SeekBar bar) {}
        });
        content.addView(duration_);
        mode_ = new Spinner(activity_);
        String[] modes = {activity_.getString(R.string.beat_immediate), activity_.getString(R.string.beat_next_beat), activity_.getString(R.string.beat_next_bar)};
        ArrayAdapter<String> mode_adapter = new ArrayAdapter<>(activity_, android.R.layout.simple_spinner_item, modes);
        mode_adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mode_.setAdapter(mode_adapter);
        mode_.setSelection(last_mode);
        mode_.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(AdapterView<?> parent, View view, int index, long id) { last_mode = index; }
            @Override public void onNothingSelected(AdapterView<?> parent) {}
        });
        content.addView(mode_);
        follow_ = new CheckBox(activity_);
        follow_.setText(R.string.program_follow);
        follow_.setChecked(last_follow);
        follow_.setOnClickListener(view -> last_follow = follow_.isChecked());
        content.addView(follow_);
        rows_.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(AdapterView<?> parent, View view, int index, long id) { SelectSettings(index); }
            @Override public void onNothingSelected(AdapterView<?> parent) {}
        });
        LinearLayout apply = new LinearLayout(activity_);
        Button(apply, R.string.program_apply, () -> Send(8));
        Button(apply, R.string.scene_queue, () -> { dialog_.dismiss(); queue.run(); });
        cancel_ = Button(apply, R.string.program_cancel, () -> Send(11));
        buttons_.remove(cancel_);
        content.addView(apply);
        ScrollView scroll = new ScrollView(activity_);
        scroll.addView(content);
        dialog_ = new AlertDialog.Builder(activity_).setTitle(R.string.program_title).setView(scroll)
                .setNegativeButton(android.R.string.cancel, null).create();
        dialog_.setOnDismissListener(dialog -> handler_.removeCallbacks(update_));
        dialog_.show();
        handler_.post(update_);
    }
}
