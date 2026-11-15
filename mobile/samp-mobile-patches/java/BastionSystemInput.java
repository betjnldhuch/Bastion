package com.gta.game;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.text.InputFilter;
import android.text.InputType;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Bastion: chat input with the phone's own keyboard (launcher setting
 * "Клавиатура в чате: Телефона"). A bar at the top of the screen, so the IME
 * never covers it. The text goes back to the native chat as cp1251.
 */
public class BastionSystemInput {
    private static final String TAG = "BastionSystemInput";
    private static final int ORANGE = 0xFFFF7A1A;

    /** Native side: text != null and sent -> chat message/command; otherwise closed. */
    public static native void onInput(byte[] text, boolean sent);

    private final Activity activity;
    private LinearLayout bar;
    private EditText edit;
    private boolean open;

    public BastionSystemInput(Activity activity) {
        this.activity = activity;
    }

    public static boolean enabled(Activity activity) {
        try {
            return activity.getIntent() != null && activity.getIntent().getBooleanExtra("bastion.system_keyboard", false);
        } catch (Exception e) {
            return false;
        }
    }

    private int dp(float v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, activity.getResources().getDisplayMetrics());
    }

    private GradientDrawable box(int fill, int stroke) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(fill);
        d.setCornerRadius(dp(6));
        if (stroke != 0) d.setStroke(dp(1), stroke);
        return d;
    }

    private void ensureView() {
        if (bar != null) return;
        bar = new LinearLayout(activity);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setGravity(Gravity.CENTER_VERTICAL);
        bar.setPadding(dp(8), dp(6), dp(8), dp(6));
        bar.setBackgroundColor(0xE60E1013);

        edit = new EditText(activity);
        edit.setSingleLine(true);
        edit.setTextColor(0xFFECECEC);
        edit.setHintTextColor(0x66ECECEC);
        edit.setHint("Сообщение или /команда");
        edit.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        edit.setPadding(dp(10), dp(6), dp(10), dp(6));
        edit.setBackground(box(0xFF1B1E23, 0xFF2B3440));
        edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_SENTENCES);
        edit.setImeOptions(EditorInfo.IME_ACTION_SEND | EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_FLAG_NO_FULLSCREEN);
        edit.setFilters(new InputFilter[]{new InputFilter.LengthFilter(128)});
        edit.setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_SEND || actionId == EditorInfo.IME_ACTION_DONE
                    || (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER && event.getAction() == KeyEvent.ACTION_DOWN)) {
                send();
                return true;
            }
            return false;
        });
        bar.addView(edit, new LinearLayout.LayoutParams(0, dp(40), 1f));

        TextView sendBtn = button("➤", ORANGE, Color.WHITE);
        sendBtn.setOnClickListener(v -> send());
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(dp(48), dp(40));
        lp.leftMargin = dp(6);
        bar.addView(sendBtn, lp);

        TextView closeBtn = button("✕", 0xFF1B1E23, 0xFFECECEC);
        closeBtn.setOnClickListener(v -> close(true));
        LinearLayout.LayoutParams lp2 = new LinearLayout.LayoutParams(dp(40), dp(40));
        lp2.leftMargin = dp(6);
        bar.addView(closeBtn, lp2);

        FrameLayout.LayoutParams flp = new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.TOP);
        activity.addContentView(bar, flp);
        bar.setVisibility(View.GONE);
    }

    private TextView button(String text, int fill, int color) {
        TextView b = new TextView(activity);
        b.setText(text);
        b.setGravity(Gravity.CENTER);
        b.setTextColor(color);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        b.setBackground(box(fill, 0));
        return b;
    }

    /** UI thread. */
    public void show() {
        try {
            ensureView();
            open = true;
            edit.setText("");
            bar.setVisibility(View.VISIBLE);
            bar.bringToFront();
            edit.requestFocus();
            edit.postDelayed(() -> {
                InputMethodManager imm = (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) imm.showSoftInput(edit, InputMethodManager.SHOW_IMPLICIT);
            }, 50);
        } catch (Exception e) {
            Log.e(TAG, "show failed", e);
            onInput(null, false);
        }
    }

    private void send() {
        String text = edit.getText().toString().trim();
        byte[] bytes = null;
        try {
            bytes = text.getBytes("windows-1251");
        } catch (Exception e) {
            Log.e(TAG, "cp1251", e);
        }
        hide();
        onInput(bytes, bytes != null && bytes.length > 0);
    }

    private void close(boolean notify) {
        hide();
        if (notify) onInput(null, false);
    }

    private void hide() {
        if (!open) return;
        open = false;
        InputMethodManager imm = (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null && edit != null) imm.hideSoftInputFromWindow(edit.getWindowToken(), 0);
        if (bar != null) bar.setVisibility(View.GONE);
        // back to immersive fullscreen
        activity.getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    /** Game paused (app in background): close without sending. */
    public void setPaused(boolean pause) {
        if (pause && open) close(true);
    }
}
