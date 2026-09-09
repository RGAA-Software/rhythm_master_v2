package org.rhythmmaster.player;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.SeekBar;
import android.widget.CheckBox;
import android.widget.ScrollView;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;
import android.content.Context;
import android.view.Surface;
import android.view.SurfaceHolder;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Native controls and storage only; decoding and playback remain in the shared core. */
public final class PlayerActivity extends SDLActivity {
    private static final int kOpenPackage = 10;
    private static final int kOpenMusic = 11;
    private static final int kImportProgram = 12;
    private final Handler handler_ = new Handler(Looper.getMainLooper());
    private final ExecutorService importer_ = Executors.newSingleThreadExecutor();
    private TextView status_ = null;
    private boolean importing_ = false;
    private boolean active_ = false;
    private int render_quality_ = 1;
    private SeekBar music_position_ = null;
    private TextView music_time_ = null;
    private boolean seeking_ = false;
    private CheckBox repeat_ = null;
    private AudioManager audio_manager_ = null;
    private AudioFocusRequest audio_focus_ = null;
    private ScrollView controls_container_ = null;
    private EffectCatalog effects_ = null;
    private TextView effect_title_ = null;
    private PlayerPresentation presentation_ = null;
    private static native void nativeCommand(int command);
    private static native boolean nativeOpen(String path);
    private static native String nativeStatus();
    private static native void nativeSurface(Surface surface);
    private static native boolean nativeMusic(String path);
    private static native void nativeSeek(double seconds);
    private static native double nativePosition();
    private static native double nativeDuration();
    private static native boolean nativeMusicLoop();
    private static native int nativeSceneOrientation();
    private static native String nativeSceneTitle();

    @Override protected SDLSurface createSDLSurface(Context context) {
        return new SDLSurface(context) {
            @Override public void surfaceCreated(SurfaceHolder holder) {
                nativeSurface(holder.getSurface());
                super.surfaceCreated(holder);
            }
            @Override public void surfaceDestroyed(SurfaceHolder holder) {
                nativeSurface(null);
                super.surfaceDestroyed(holder);
            }
        };
    }

    private final Runnable update_ = new Runnable() {
        @Override public void run() {
            if (!active_ || status_ == null) return;
            ApplySceneOrientation();
            effect_title_.setText(nativeSceneTitle());
            String text = nativeStatus().replace("playing", getString(R.string.playing))
                    .replace("paused", getString(R.string.paused))
                    .replace("audio_error", getString(R.string.audio_error))
                    .replace("package_error", getString(R.string.package_error))
                    .replace("event_rejections", getString(R.string.event_rejections));
            status_.setText(text);
            double duration = nativeDuration();
            double position = nativePosition();
            repeat_.setChecked(nativeMusicLoop());
            music_position_.setEnabled(duration > 0);
            if (!seeking_ && duration > 0)
                music_position_.setProgress((int) Math.min(10000, position / duration * 10000));
            music_time_.setText(String.format(java.util.Locale.ROOT, "%.2f / %.2f s", position, duration));
            handler_.postDelayed(this, 500);
        }
    };

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (mLayout == null) return;
        audio_manager_ = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        audio_focus_ = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(new AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
                .setOnAudioFocusChangeListener(change -> {
                    if (change < 0) nativeCommand(3);
                }, handler_).build();
        LinearLayout controls = new LinearLayout(this);
        controls.setOrientation(LinearLayout.VERTICAL);
        controls.setBackgroundColor(0xee172230);
        LinearLayout buttons = new LinearLayout(this);
        AddButton(buttons, R.string.choose_effect, () -> ChooseEffect());
        AddButton(buttons, R.string.pause_resume, () -> {
            if (RequestAudioFocus()) nativeCommand(1);
        });
        AddButton(buttons, R.string.restart, () -> nativeCommand(2));
        controls.addView(buttons);
        effect_title_ = new TextView(this);
        effect_title_.setTextColor(0xffeeeeee);
        effect_title_.setPadding(16, 4, 16, 4);
        controls.addView(effect_title_);
        LinearLayout settings = new LinearLayout(this);
        AddButton(settings, R.string.render_quality, () -> ChooseQuality());
        AddButton(settings, R.string.open_package, () -> OpenPackage());
        AddButton(settings, R.string.performance_controls, () -> PerformanceControls.Show(this));
        LinearLayout scenes = new LinearLayout(this);
        AddButton(scenes, R.string.scene_queue, () -> SceneQueueDialog.Show(this, () -> ChooseQueuedEffect()));
        AddButton(scenes, R.string.program_title, () -> PerformanceProgramDialog.Show(this,
                () -> ChooseProgramEffect(), () -> ImportProgram(),
                () -> SceneQueueDialog.Show(this, () -> ChooseQueuedEffect())));
        AddButton(scenes, R.string.fullscreen, () -> presentation_.Enter());
        controls.addView(settings);
        controls.addView(scenes);
        LinearLayout music = new LinearLayout(this);
        AddButton(music, R.string.open_music, () -> OpenMusic());
        AddButton(music, R.string.demo_music, () -> StartImport(null, true));
        repeat_ = new CheckBox(this);
        repeat_.setText(R.string.loop_music);
        repeat_.setTextColor(0xffeeeeee);
        // Programmatic state refresh never dispatches another playback command.
        repeat_.setOnClickListener(button -> nativeCommand(repeat_.isChecked() ? 21 : 20));
        music.addView(repeat_, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        controls.addView(music);
        music_position_ = new SeekBar(this);
        music_position_.setMax(10000);
        music_position_.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar bar, int progress, boolean fromUser) {}
            @Override public void onStartTrackingTouch(SeekBar bar) { seeking_ = true; }
            @Override public void onStopTrackingTouch(SeekBar bar) {
                double duration = nativeDuration();
                if (duration > 0) nativeSeek(duration * bar.getProgress() / 10000.0);
                seeking_ = false;
            }
        });
        controls.addView(music_position_);
        music_time_ = new TextView(this);
        music_time_.setTextColor(0xffeeeeee);
        controls.addView(music_time_);
        render_quality_ = Math.max(0, Math.min(2, getSharedPreferences("player", MODE_PRIVATE)
                .getInt("render_quality", 1)));
        nativeCommand(10 + render_quality_);
        status_ = new TextView(this);
        status_.setTextColor(0xffeeeeee);
        status_.setPadding(16, 4, 16, 12);
        controls.addView(status_);
        controls_container_ = new ScrollView(this);
        controls_container_.setId(View.generateViewId());
        controls_container_.setBackgroundColor(0xff172230);
        controls_container_.addView(controls);
        mLayout.addView(controls_container_);
        presentation_ = new PlayerPresentation(this, mLayout, mSurface, controls_container_);
        boolean chinese = getResources().getConfiguration().getLocales().get(0).getLanguage().equals("zh");
        importer_.execute(() -> {
            try {
                EffectCatalog catalog = EffectCatalog.Load(getAssets(), chinese);
                handler_.post(() -> effects_ = catalog);
            } catch (Exception error) {
                handler_.post(() -> Toast.makeText(this, R.string.package_error, Toast.LENGTH_LONG).show());
            }
        });
    }

    private void ChooseEffect() {
        if (effects_ == null || importing_) {
            Toast.makeText(this, R.string.loading_effects, Toast.LENGTH_SHORT).show();
            return;
        }
        effects_.Show(this, asset -> StartImport(null, false, asset));
    }

    private void ChooseQueuedEffect() {
        if (effects_ == null || importing_) {
            Toast.makeText(this, R.string.loading_effects, Toast.LENGTH_SHORT).show();
            return;
        }
        effects_.Show(this, asset -> StartImport(null, false, asset, effects_.Title(asset)));
    }
    private void ChooseProgramEffect() {
        if (effects_ == null) return;
        effects_.Show(this, asset -> {
            if (!PerformanceProgramDialog.AddBuiltin(effects_.ContentId(asset), effects_.Title(asset)))
                Toast.makeText(this, R.string.program_rejected, Toast.LENGTH_SHORT).show();
        });
    }
    private void ImportProgram() {
        if (importing_) return;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/octet-stream");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"application/octet-stream", "application/zip"});
        startActivityForResult(intent, kImportProgram);
    }

    private void ApplySceneOrientation() {
        // This is a snapshot of the accepted package, independent of render quality
        // and the current SurfaceView dimensions. Reuse SDL's orientation policy.
        int shape = nativeSceneOrientation();
        int requested;
        String hint;
        if (shape == 1) {
            requested = ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE;
            hint = "LandscapeLeft LandscapeRight";
        } else if (shape == 2) {
            requested = ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT;
            hint = "Portrait PortraitUpsideDown";
        } else if (shape == 3) {
            requested = ActivityInfo.SCREEN_ORIENTATION_FULL_USER;
            hint = "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown";
        } else {
            return;
        }
        if (getRequestedOrientation() != requested)
            super.setOrientationBis(0, 0, true, hint);
    }

    @Override public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        if (presentation_ != null) presentation_.ApplyLayout();
    }

    @Override public void onBackPressed() {
        if (presentation_ != null && presentation_.Exit()) return;
        super.onBackPressed();
    }

    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        if (presentation_ != null && presentation_.HandleBack(event)) return true;
        return super.dispatchKeyEvent(event);
    }

    @Override public boolean dispatchTouchEvent(MotionEvent event) {
        if (presentation_ != null) presentation_.ObserveTouch(event);
        return super.dispatchTouchEvent(event);
    }

    private void AddButton(LinearLayout row, int label, Runnable action) {
        Button button = new Button(this);
        button.setText(label);
        button.setOnClickListener(view -> action.run());
        row.addView(button, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
    }

    private void OpenPackage() {
        if (importing_) return;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityForResult(intent, kOpenPackage);
    }

    private boolean RequestAudioFocus() {
        return audio_manager_ != null && audio_focus_ != null &&
                audio_manager_.requestAudioFocus(audio_focus_) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
    }

    private void OpenMusic() {
        if (importing_) return;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("audio/*");
        startActivityForResult(intent, kOpenMusic);
    }

    private void ChooseQuality() {
        String[] choices = {getString(R.string.quality_original), getString(R.string.quality_balanced),
                getString(R.string.quality_economy)};
        new AlertDialog.Builder(this).setTitle(R.string.render_quality)
                .setSingleChoiceItems(choices, render_quality_, (dialog, selected) -> {
                    render_quality_ = selected;
                    getSharedPreferences("player", MODE_PRIVATE).edit().putInt("render_quality", selected).apply();
                    nativeCommand(10 + selected);
                    dialog.dismiss();
                }).setNegativeButton(android.R.string.cancel, null).show();
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if ((request != kOpenPackage && request != kOpenMusic && request != kImportProgram) || result != Activity.RESULT_OK ||
                data == null || data.getData() == null || importing_) return;
        if (request == kImportProgram) StartImport(data.getData(), false, null, null, true);
        else StartImport(data.getData(), request == kOpenMusic);
    }

    private void StartImport(Uri uri, boolean music) {
        StartImport(uri, music, music ? "resonance_demo.wav" : null);
    }

    private void StartImport(Uri uri, boolean music, String asset) {
        StartImport(uri, music, asset, null);
    }

    private void StartImport(Uri uri, boolean music, String asset, String queue_title) {
        StartImport(uri, music, asset, queue_title, false);
    }
    private void StartImport(Uri uri, boolean music, String asset, String queue_title, boolean program) {
        if (importing_ || (music && !RequestAudioFocus())) return;
        importing_ = true;
        importer_.execute(() -> {
            File staging = null;
            try {
                staging = File.createTempFile(music ? "music-" : "incoming-",
                        music ? ".media" : ".rhythmpack", getCacheDir());
                try (InputStream input = uri == null ? getAssets().open(asset) :
                        getContentResolver().openInputStream(uri);
                     FileOutputStream output = new FileOutputStream(staging)) {
                    if (input == null) throw new java.io.IOException("No document stream");
                    byte[] buffer = new byte[64 * 1024];
                    long total = 0;
                    int count;
                    while ((count = input.read(buffer)) != -1) {
                        total += count;
                        if (total > (music ? 64L : 272L) * 1024 * 1024 || Thread.currentThread().isInterrupted())
                            throw new java.io.IOException("Import limit");
                        output.write(buffer, 0, count);
                    }
                    output.getFD().sync();
                }
                boolean accepted = program ? PerformanceProgramDialog.Import(staging.getAbsolutePath()) :
                        queue_title != null ? SceneQueueDialog.Enqueue(staging.getAbsolutePath(), queue_title) :
                        music ? nativeMusic(staging.getAbsolutePath()) : nativeOpen(staging.getAbsolutePath());
                if (!accepted) throw new java.io.IOException("Import queue full");
            } catch (Exception error) {
                android.util.Log.w("RhythmImport", "Import failed", error);
                if (staging != null) staging.delete();
                handler_.post(() -> Toast.makeText(this, music ? R.string.audio_error : R.string.package_error,
                        Toast.LENGTH_LONG).show());
            } finally {
                handler_.post(() -> importing_ = false);
            }
        });
    }

    @Override protected void onResume() {
        super.onResume();
        active_ = true;
        if (presentation_ != null) presentation_.Resume();
        handler_.post(update_);
    }

    @Override protected void onPause() {
        active_ = false;
        handler_.removeCallbacks(update_);
        if (presentation_ != null) presentation_.Pause();
        super.onPause();
    }

    @Override protected void onDestroy() {
        active_ = false;
        handler_.removeCallbacks(update_);
        importer_.shutdownNow();
        if (presentation_ != null) presentation_.Pause();
        if (audio_manager_ != null && audio_focus_ != null) audio_manager_.abandonAudioFocusRequest(audio_focus_);
        super.onDestroy();
    }
}
