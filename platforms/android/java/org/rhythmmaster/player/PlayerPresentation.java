package org.rhythmmaster.player;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.Toast;
import org.libsdl.app.SDLActivity;

/** Owns responsive presentation layout and transient fullscreen controls on the UI thread. */
final class PlayerPresentation {
    private final Activity activity_;
    private final View surface_;
    private final ViewGroup root_;
    private final ScrollView controls_;
    private final Button exit_;
    private final Handler handler_ = new Handler(Looper.getMainLooper());
    private boolean fullscreen_ = false;
    private final Runnable hide_;

    PlayerPresentation(Activity activity, ViewGroup root, View surface, ScrollView controls) {
        activity_ = activity;
        surface_ = surface;
        root_ = root;
        controls_ = controls;
        exit_ = new Button(activity);
        hide_ = () -> exit_.setVisibility(View.GONE);
        exit_.setText(R.string.exit_fullscreen);
        exit_.setContentDescription(activity.getString(R.string.exit_fullscreen));
        exit_.setOnClickListener(view -> Exit());
        exit_.setVisibility(View.GONE);
        RelativeLayout.LayoutParams button = new RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        button.addRule(RelativeLayout.ALIGN_PARENT_TOP);
        button.addRule(RelativeLayout.ALIGN_PARENT_END);
        int margin = (int) (16 * activity.getResources().getDisplayMetrics().density);
        button.setMargins(margin, margin, margin, margin);
        root.addView(exit_, button);
        // Window bounds exclude system bars in ordinary mode. Recompute after
        // rotation, fullscreen or inset changes rather than using display size.
        root.addOnLayoutChangeListener((view, left, top, right, bottom,
                                      old_left, old_top, old_right, old_bottom) -> {
            if (right - left != old_right - old_left || bottom - top != old_bottom - old_top)
                ApplyLayout();
        });
        controls.getChildAt(0).addOnLayoutChangeListener((view, left, top, right, bottom,
                                                       old_left, old_top, old_right, old_bottom) -> {
            if (bottom - top != old_bottom - old_top) ApplyLayout();
        });
        ApplyLayout();
    }

    void Enter() {
        if (fullscreen_) return;
        fullscreen_ = true;
        SDLActivity.setWindowStyle(true);
        ApplyLayout();
        RevealExit();
        Toast.makeText(activity_, R.string.fullscreen_help, Toast.LENGTH_SHORT).show();
    }

    boolean Exit() {
        if (!fullscreen_) return false;
        fullscreen_ = false;
        handler_.removeCallbacks(hide_);
        exit_.setVisibility(View.GONE);
        SDLActivity.setWindowStyle(false);
        ApplyLayout();
        return true;
    }

    boolean HandleBack(KeyEvent event) {
        if (!fullscreen_ || event.getKeyCode() != KeyEvent.KEYCODE_BACK) return false;
        if (event.getAction() == KeyEvent.ACTION_UP && !event.isCanceled()) Exit();
        return true;
    }

    void ObserveTouch(MotionEvent event) {
        // SDL restores its surface listener on resume. Observe at the Activity
        // boundary without replacing that listener or consuming scene input.
        if (fullscreen_ && event.getActionMasked() == MotionEvent.ACTION_UP) RevealExit();
    }

    private void RevealExit() {
        handler_.removeCallbacks(hide_);
        exit_.setVisibility(View.VISIBLE);
        exit_.bringToFront();
        handler_.postDelayed(hide_, 3000);
    }

    void ApplyLayout() {
        controls_.setVisibility(fullscreen_ ? View.GONE : View.VISIBLE);
        RelativeLayout.LayoutParams surface = new RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        if (!fullscreen_) {
            android.util.DisplayMetrics metrics = activity_.getResources().getDisplayMetrics();
            boolean landscape = activity_.getResources().getConfiguration().orientation ==
                    Configuration.ORIENTATION_LANDSCAPE;
            int available_width = root_.getWidth() > 0 ? root_.getWidth() : metrics.widthPixels;
            int available_height = root_.getHeight() > 0 ? root_.getHeight() : metrics.heightPixels;
            int panel_width = landscape ? Math.min((int) (280 * metrics.density), available_width / 2) :
                    available_width;
            View content = controls_.getChildAt(0);
            content.measure(View.MeasureSpec.makeMeasureSpec(panel_width, View.MeasureSpec.EXACTLY),
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
            int panel_height = landscape ? available_height :
                    Math.min(content.getMeasuredHeight(), available_height / 2);
            RelativeLayout.LayoutParams controls = new RelativeLayout.LayoutParams(panel_width, panel_height);
            controls.addRule(landscape ? RelativeLayout.ALIGN_PARENT_RIGHT : RelativeLayout.ALIGN_PARENT_BOTTOM);
            ViewGroup.LayoutParams previous = controls_.getLayoutParams();
            if (previous == null || previous.width != panel_width || previous.height != panel_height ||
                    !(previous instanceof RelativeLayout.LayoutParams) ||
                    ((RelativeLayout.LayoutParams) previous).getRule(landscape ?
                            RelativeLayout.ALIGN_PARENT_RIGHT : RelativeLayout.ALIGN_PARENT_BOTTOM) == 0)
                controls_.setLayoutParams(controls);
            surface.addRule(landscape ? RelativeLayout.LEFT_OF : RelativeLayout.ABOVE, controls_.getId());
        }
        surface_.setLayoutParams(surface);
    }

    void Resume() {
        if (fullscreen_) SDLActivity.setWindowStyle(true);
    }

    void Pause() {
        handler_.removeCallbacks(hide_);
        exit_.setVisibility(View.GONE);
    }

}
