package org.rhythmmaster.player;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;
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
    private final Handler handler_ = new Handler(Looper.getMainLooper());
    private final ExecutorService importer_ = Executors.newSingleThreadExecutor();
    private TextView status_ = null;
    private boolean importing_ = false;
    private boolean active_ = false;
    private static native void nativeCommand(int command);
    private static native boolean nativeOpen(String path);
    private static native String nativeStatus();
    private static native void nativeSurface(Surface surface);

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
            String text = nativeStatus().replace("playing", getString(R.string.playing))
                    .replace("paused", getString(R.string.paused))
                    .replace("package_error", getString(R.string.package_error));
            status_.setText(text);
            handler_.postDelayed(this, 500);
        }
    };

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (mLayout == null) return;
        LinearLayout controls = new LinearLayout(this);
        controls.setOrientation(LinearLayout.VERTICAL);
        controls.setBackgroundColor(0xee172230);
        LinearLayout buttons = new LinearLayout(this);
        AddButton(buttons, R.string.open_package, () -> OpenPackage());
        AddButton(buttons, R.string.pause_resume, () -> nativeCommand(1));
        AddButton(buttons, R.string.restart, () -> nativeCommand(2));
        controls.addView(buttons);
        status_ = new TextView(this);
        status_.setTextColor(0xffeeeeee);
        status_.setPadding(16, 4, 16, 12);
        controls.addView(status_);
        android.widget.RelativeLayout.LayoutParams layout = new android.widget.RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        layout.addRule(android.widget.RelativeLayout.ALIGN_PARENT_BOTTOM);
        mLayout.addView(controls, layout);
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

    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != kOpenPackage || result != Activity.RESULT_OK || data == null || data.getData() == null || importing_) return;
        importing_ = true;
        Uri uri = data.getData();
        importer_.execute(() -> {
            File staging = null;
            try {
                staging = File.createTempFile("incoming-", ".rhythmpack", getCacheDir());
                try (InputStream input = getContentResolver().openInputStream(uri);
                     FileOutputStream output = new FileOutputStream(staging)) {
                    if (input == null) throw new java.io.IOException("No document stream");
                    byte[] buffer = new byte[64 * 1024];
                    long total = 0;
                    int count;
                    while ((count = input.read(buffer)) != -1) {
                        total += count;
                        if (total > 16 * 1024 * 1024 || Thread.currentThread().isInterrupted())
                            throw new java.io.IOException("Package import limit");
                        output.write(buffer, 0, count);
                    }
                    output.getFD().sync();
                }
                if (!nativeOpen(staging.getAbsolutePath())) throw new java.io.IOException("Package queue full");
            } catch (Exception error) {
                if (staging != null) staging.delete();
                handler_.post(() -> Toast.makeText(this, R.string.package_error, Toast.LENGTH_LONG).show());
            } finally {
                handler_.post(() -> importing_ = false);
            }
        });
    }

    @Override protected void onResume() {
        super.onResume();
        active_ = true;
        handler_.post(update_);
    }

    @Override protected void onPause() {
        active_ = false;
        handler_.removeCallbacks(update_);
        super.onPause();
    }

    @Override protected void onDestroy() {
        active_ = false;
        handler_.removeCallbacks(update_);
        importer_.shutdownNow();
        super.onDestroy();
    }
}
