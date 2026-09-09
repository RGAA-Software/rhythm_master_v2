package org.rhythmmaster.player;

import android.app.Activity;
import android.os.SystemClock;
import android.text.InputType;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import java.util.Locale;
import org.json.JSONArray;
import org.json.JSONObject;

/** Native-owned manual timing and bounded requests; this view owns only edits. */
final class BeatControls {
    private static final int[] kModes = {R.string.beat_immediate, R.string.beat_next_beat, R.string.beat_next_bar};
    private static final int[] kUnits = {1, 2, 4, 8, 16, 32};
    private Activity activity_ = null;
    private long generation_ = 0;
    private TextView status_ = null;
    private Spinner mode_ = null;
    private Spinner unit_ = null;
    private CheckBox enabled_ = null;
    private EditText bpm_ = null;
    private EditText beats_ = null;
    private EditText origin_ = null;
    private final Button[] cancel_ = new Button[2];
    private final long[] requests_ = new long[2];

    private static native boolean nativeGrid(long generation, boolean enabled, double bpm,
                                             int beats, int unit, double origin);
    private static native boolean nativeMode(long generation, int mode);
    private static native double nativeTap(long generation, double seconds);
    private static native boolean nativeCancel(long generation, long id);

    BeatControls(Activity activity, long generation, LinearLayout parent, JSONObject data) throws Exception {
        activity_ = activity;
        generation_ = generation;
        status_ = new TextView(activity_);
        parent.addView(status_);
        TextView timing = new TextView(activity_);
        timing.setText(R.string.beat_timing);
        parent.addView(timing);
        String[] modes = new String[kModes.length];
        for (int i = 0; i < modes.length; ++i) modes[i] = activity_.getString(kModes[i]);
        mode_ = Picker(parent, modes);
        mode_.setContentDescription(activity_.getString(R.string.beat_timing));
        mode_.setSelection(data.optInt("mode"));
        mode_.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(AdapterView<?> adapter, View view, int position, long id) {
                if (!nativeMode(generation_, position)) {
                    Toast.makeText(activity_, R.string.beat_enable_first, Toast.LENGTH_SHORT).show();
                    try {
                        JSONObject current = new JSONObject(PerformanceControls.nativeDescribe());
                        mode_.setSelection(current.optInt("mode"));
                    } catch (Exception ignored) {}
                }
            }
            @Override public void onNothingSelected(AdapterView<?> adapter) {}
        });
        LinearLayout cancellations = new LinearLayout(activity_);
        for (int i = 0; i < cancel_.length; ++i) {
            final int index = i;
            cancel_[i] = Button(cancellations, i == 0 ? R.string.beat_cancel_snapshot : R.string.beat_cancel_scene,
                    () -> nativeCancel(generation_, requests_[index]));
        }
        parent.addView(cancellations);
        LinearLayout settings = new LinearLayout(activity_);
        settings.setOrientation(LinearLayout.VERTICAL);
        settings.setVisibility(View.GONE);
        Button show = new Button(activity_);
        show.setText(R.string.beat_settings);
        show.setOnClickListener(view -> settings.setVisibility(settings.getVisibility() == View.GONE ? View.VISIBLE : View.GONE));
        parent.addView(show);
        parent.addView(settings);
        JSONObject grid = data.optJSONObject("grid");
        enabled_ = new CheckBox(activity_);
        enabled_.setText(R.string.beat_enabled);
        enabled_.setChecked(grid != null);
        settings.addView(enabled_);
        bpm_ = Number(settings, R.string.beat_bpm, grid == null ? 120 : grid.getDouble("bpm"), false);
        beats_ = Number(settings, R.string.beat_beats, grid == null ? 4 : grid.getInt("beats"), true);
        TextView denominator = new TextView(activity_);
        denominator.setText(R.string.beat_unit);
        settings.addView(denominator);
        unit_ = Picker(settings, new String[]{"1", "2", "4", "8", "16", "32"});
        int selected_unit = grid == null ? 4 : grid.getInt("unit");
        for (int i = 0; i < kUnits.length; ++i) if (kUnits[i] == selected_unit) unit_.setSelection(i);
        origin_ = Number(settings, R.string.beat_origin, grid == null ? 0 : grid.getDouble("origin"), false);
        LinearLayout commands = new LinearLayout(activity_);
        Button(commands, R.string.beat_apply, () -> Apply());
        Button(commands, R.string.beat_tap, () -> {
            double bpm = nativeTap(generation_, SystemClock.elapsedRealtimeNanos() / 1.0e9);
            if (bpm < 0) Toast.makeText(activity_, R.string.beat_enable_first, Toast.LENGTH_SHORT).show();
            else if (bpm > 0) bpm_.setText(String.format(Locale.ROOT, "%.2f", bpm));
        });
        Button(commands, R.string.beat_mark, () -> {
            try {
                double seconds = new JSONObject(PerformanceControls.nativeDescribe()).getDouble("seconds");
                origin_.setText(String.format(Locale.ROOT, "%.6f", seconds));
                Apply();
            } catch (Exception ignored) {}
        });
        settings.addView(commands);
        TextView help = new TextView(activity_);
        help.setText(R.string.beat_help);
        settings.addView(help);
        Refresh(data);
    }
    private void Apply() {
        try {
            boolean enabled = enabled_.isChecked();
            double bpm = enabled ? Double.parseDouble(bpm_.getText().toString()) : 120;
            int beats = enabled ? Integer.parseInt(beats_.getText().toString()) : 4;
            double origin = enabled ? Double.parseDouble(origin_.getText().toString()) : 0;
            if (!nativeGrid(generation_, enabled, bpm, beats, kUnits[unit_.getSelectedItemPosition()], origin))
                throw new IllegalArgumentException();
            if (!enabled) mode_.setSelection(0);
            Toast.makeText(activity_, R.string.beat_applied, Toast.LENGTH_SHORT).show();
        } catch (Exception error) {
            Toast.makeText(activity_, R.string.beat_invalid, Toast.LENGTH_SHORT).show();
        }
    }
    void Refresh(JSONObject data) throws Exception {
        status_.setText(Summary(activity_, data));
        // Spinner selection callbacks may arrive on the next layout. Polling an
        // older native snapshot must not overwrite the user's uncommitted choice.
        JSONArray actions = data.getJSONArray("actions");
        for (int i = 0; i < 2; ++i) {
            JSONObject action = actions.getJSONObject(i);
            requests_[i] = Long.parseUnsignedLong(action.getString("id"));
            cancel_[i].setVisibility(action.getInt("state") == 1 || action.getInt("state") == 2 ? View.VISIBLE : View.GONE);
        }
    }
    static String Summary(Activity activity, JSONObject data) throws Exception {
        int mode = Math.max(0, Math.min(kModes.length - 1, data.optInt("mode")));
        StringBuilder text = new StringBuilder(activity.getString(kModes[mode]));
        JSONObject grid = data.optJSONObject("grid");
        if (grid != null) text.append(" · ").append(activity.getString(R.string.beat_position,
                grid.getDouble("bpm"), grid.getInt("beats"), grid.getInt("unit"), grid.getLong("bar"), grid.getInt("beat")));
        if (data.optInt("taps") > 0) text.append("\n").append(activity.getString(R.string.beat_taps, data.getInt("taps")));
        int[] states = {R.string.beat_idle, R.string.beat_pending, R.string.beat_dispatched,
                R.string.beat_completed, R.string.beat_cancelled, R.string.beat_failed};
        int[] reasons = {0, R.string.beat_user_cancelled, R.string.beat_source_changed,
                R.string.beat_grid_changed, R.string.beat_enable_first, R.string.beat_no_boundary,
                R.string.beat_target_unavailable};
        JSONArray actions = data.optJSONArray("actions");
        if (actions != null) for (int i = 0; i < actions.length(); ++i) {
            JSONObject action = actions.getJSONObject(i);
            int state = action.getInt("state");
            if (state == 0) continue;
            text.append("\n").append(activity.getString(action.getInt("kind") == 0 ? R.string.beat_snapshot : R.string.beat_scene));
            text.append(" #").append(action.getString("target")).append(": ").append(activity.getString(states[state]));
            if (state == 1 || state == 2) text.append(String.format(Locale.ROOT, " @ %.3f s", action.getDouble("due")));
            int reason = action.getInt("reason");
            if (reason != 0) text.append(" · ").append(activity.getString(reasons[reason]));
        }
        return text.toString();
    }
    static boolean HasPendingScene(JSONObject data) throws Exception {
        JSONObject action = data.getJSONArray("actions").getJSONObject(1);
        return action.getInt("state") == 1 || action.getInt("state") == 2;
    }
    static void CancelScene() {
        try {
            JSONObject data = new JSONObject(PerformanceControls.nativeDescribe());
            JSONObject action = data.getJSONArray("actions").getJSONObject(1);
            nativeCancel(data.getLong("generation"), Long.parseUnsignedLong(action.getString("id")));
        } catch (Exception ignored) {}
    }
    private Spinner Picker(LinearLayout parent, String[] labels) {
        Spinner picker = new Spinner(activity_);
        ArrayAdapter<String> adapter = new ArrayAdapter<>(activity_, android.R.layout.simple_spinner_item, labels);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        picker.setAdapter(adapter);
        parent.addView(picker);
        return picker;
    }
    private EditText Number(LinearLayout parent, int label, double value, boolean integer) {
        TextView title = new TextView(activity_);
        title.setText(label);
        parent.addView(title);
        EditText field = new EditText(activity_);
        field.setSingleLine(true);
        field.setContentDescription(activity_.getString(label));
        field.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_SIGNED |
                (integer ? 0 : InputType.TYPE_NUMBER_FLAG_DECIMAL));
        field.setText(integer ? Integer.toString((int) value) : Double.toString(value));
        parent.addView(field);
        return field;
    }
    private Button Button(LinearLayout row, int label, Runnable action) {
        Button button = new Button(activity_);
        button.setText(label);
        button.setOnClickListener(view -> action.run());
        row.addView(button, new LinearLayout.LayoutParams(0, -2, 1));
        return button;
    }
}
