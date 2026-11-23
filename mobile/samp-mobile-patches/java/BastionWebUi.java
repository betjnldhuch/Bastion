package com.gta.game;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Color;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.webkit.JavascriptInterface;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;

import com.gta.game.ui.dialog.DialogManager;

import org.json.JSONObject;

/**
 * Bastion: HTML windows over the game.
 *
 * The server sends an ordinary SA-MP dialog whose caption is "[ui:NAME]" and whose
 * text is JSON. Instead of the stock dialog we open NAME in assets/ui/index.html
 * and send the player's choice back as the normal dialog response, so the
 * gamemode handles it in OnDialogResponse like any other dialog.
 * Every other dialog is drawn by the same page in the same style (window "samp").
 *
 * A dialog with caption "[hud]" is not a window: its JSON goes to BastionUI.hud
 * (health, armour, hunger, cash, bank panel). While only the panel is shown the
 * page lets every touch through to the game.
 */
public class BastionWebUi {
    private static final String TAG = "BastionWebUi";
    private static final String PREFIX = "[ui:";

    private final Activity activity;
    private final DialogManager dialogs;
    private WebView web;
    private boolean pageReady;
    private String pendingJs;
    private int dialogId = -1;
    private boolean shown;
    private boolean hudOn;
    private boolean paused;

    public BastionWebUi(Activity activity, DialogManager dialogs) {
        this.activity = activity;
        this.dialogs = dialogs;
    }

    /** True when the caption asks for an HTML window. */
    public static boolean wants(String caption) {
        return caption != null && caption.startsWith(PREFIX) && caption.indexOf(']') > PREFIX.length();
    }

    /** True for the hidden HUD data dialog from hud.inc. */
    public static boolean isHud(String caption) {
        return caption != null && caption.startsWith("[hud]");
    }

    /** Called on the UI thread. HUD panel data; {"off":1} hides the panel. */
    public void hud(String json) {
        ensureView();
        String data = json == null || json.isEmpty() ? "{}" : json;
        hudOn = !data.contains("\"off\"");
        String js = "window.BastionUI && BastionUI.hud && BastionUI.hud(" + JSONObject.quote(data) + ")";
        if (pageReady) web.evaluateJavascript(js, null);
        else pendingHud = js;
        updateVisibility();
    }

    private String pendingHud;

    private void updateVisibility() {
        if (web == null) return;
        web.setVisibility(!paused && (shown || hudOn) ? View.VISIBLE : View.GONE);
    }

    /** Called on the UI thread. A "[ui:NAME]" window with JSON data from ui.inc. */
    public void show(int id, String caption, String json, String button1, String button2) {
        String name = caption.substring(PREFIX.length(), caption.indexOf(']'));
        open(id, name, json == null || json.isEmpty() ? "{}" : json, button1, button2);
    }

    /**
     * Called on the UI thread. Any ordinary SA-MP dialog (MSGBOX/INPUT/LIST/PASSWORD/
     * TABLIST/TABLIST_HEADERS) drawn in the Bastion style; {RRGGBB} colours kept.
     */
    public void showDialog(int id, int style, String caption, String text, String button1, String button2) {
        try {
            JSONObject data = new JSONObject();
            data.put("style", style);
            data.put("caption", caption == null ? "" : caption);
            data.put("text", text == null ? "" : text);
            open(id, "samp", data, button1, button2);
        } catch (Exception e) {
            Log.e(TAG, "bad dialog " + id, e);
        }
    }

    private void open(int id, String name, Object data, String button1, String button2) {
        ensureView();
        dialogId = id;
        shown = true;
        String js;
        try {
            JSONObject msg = new JSONObject();
            msg.put("window", name);
            msg.put("button1", button1 == null ? "" : button1);
            msg.put("button2", button2 == null ? "" : button2);
            msg.put("data", data);
            js = "window.BastionUI && BastionUI.open(" + msg + ")";
        } catch (Exception e) {
            Log.e(TAG, "bad window " + name, e);
            return;
        }
        web.setFocusable(true);
        web.setFocusableInTouchMode(true);
        updateVisibility();
        web.requestFocus();
        if (pageReady) web.evaluateJavascript(js, null);
        else pendingJs = js;
    }

    public void hide() {
        shown = false;
        if (web == null) return;
        web.clearFocus();
        web.setFocusable(false);
        web.setFocusableInTouchMode(false);
        updateVisibility();
        InputMethodManager imm = (InputMethodManager) activity.getSystemService(Activity.INPUT_METHOD_SERVICE);
        if (imm != null) imm.hideSoftInputFromWindow(web.getWindowToken(), 0);
    }

    /** Pause menu hides windows; they come back on resume. */
    public void setPaused(boolean paused) {
        this.paused = paused;
        updateVisibility();
    }

    @SuppressLint({"SetJavaScriptEnabled", "AddJavascriptInterface"})
    private void ensureView() {
        if (web != null) return;
        // Without an open window (HUD panel only) touches go to the game below
        web = new WebView(activity) {
            @Override
            public boolean dispatchTouchEvent(MotionEvent ev) {
                return shown && super.dispatchTouchEvent(ev);
            }
        };
        web.setBackgroundColor(Color.TRANSPARENT);
        web.setFocusable(false);
        web.setFocusableInTouchMode(false);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setAllowFileAccess(true);
        s.setTextZoom(100);
        web.addJavascriptInterface(new Bridge(), "BastionNative");
        web.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageFinished(WebView view, String url) {
                pageReady = true;
                if (pendingHud != null) {
                    view.evaluateJavascript(pendingHud, null);
                    pendingHud = null;
                }
                if (pendingJs != null) {
                    view.evaluateJavascript(pendingJs, null);
                    pendingJs = null;
                }
            }
        });
        web.setVisibility(View.GONE);
        activity.addContentView(web, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        web.loadUrl("file:///android_asset/ui/index.html");
    }

    private class Bridge {
        /** button: 1 = left button, 0 = right button / close. */
        @JavascriptInterface
        public void respond(int button, int listitem, String text) {
            activity.runOnUiThread(() -> {
                if (dialogId < 0) return;
                int id = dialogId;
                dialogId = -1;
                hide();
                try {
                    byte[] bytes = (text == null ? "" : text).getBytes("windows-1251");
                    dialogs.sendDialogResponse(button, id, listitem, bytes);
                } catch (Exception e) {
                    Log.e(TAG, "response failed", e);
                }
            });
        }
    }
}
