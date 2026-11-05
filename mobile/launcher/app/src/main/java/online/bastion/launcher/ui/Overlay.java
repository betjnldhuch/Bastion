package online.bastion.launcher.ui;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import static online.bastion.launcher.ui.Theme.dp;
import static online.bastion.launcher.ui.Views.MATCH;
import static online.bastion.launcher.ui.Views.WRAP;

/** Окно-сообщение поверх экрана в стиле лаунчера (вместо системного AlertDialog). */
public final class Overlay {

    private static final String TAG = "bastion-overlay";

    /** Кнопка окна. action может быть null — тогда кнопка просто закрывает окно. */
    public static final class Action {
        final String label;
        final boolean primary;
        final Runnable action;

        public Action(String label, boolean primary, Runnable action) {
            this.label = label;
            this.primary = primary;
            this.action = action;
        }
    }

    private final FrameLayout host;
    private final FrameLayout scrim;

    private Overlay(FrameLayout host, FrameLayout scrim) {
        this.host = host;
        this.scrim = scrim;
    }

    public void dismiss() { host.removeView(scrim); }

    /** Закрыть верхнее окно, если оно есть (кнопка «Назад»). */
    public static boolean dismissTop(FrameLayout host) {
        for (int i = host.getChildCount() - 1; i >= 0; i--) {
            View v = host.getChildAt(i);
            if (TAG.equals(v.getTag())) {
                host.removeViewAt(i);
                return true;
            }
        }
        return false;
    }

    public static Overlay message(FrameLayout host, String title, String message) {
        return show(host, title, message, new Action("Понятно", false, null));
    }

    public static Overlay show(FrameLayout host, String title, String message, Action... actions) {
        Context c = host.getContext();
        FrameLayout scrim = new FrameLayout(c);
        scrim.setTag(TAG);
        scrim.setBackgroundColor(Theme.SCRIM);
        scrim.setClickable(true);  // не пропускаем нажатия на экран под окном
        final Overlay overlay = new Overlay(host, scrim);

        LinearLayout box = Views.vertical(c);
        box.setPadding(dp(24), dp(20), dp(24), dp(18));
        box.setBackground(Views.box(Theme.SURFACE, Theme.LINE, 1));

        // Оранжевая полоса сверху окна.
        View bar = new View(c);
        bar.setBackgroundColor(Theme.ORANGE);
        box.addView(bar, Views.margins(Views.lp(dp(40), dp(3)), 0, 0, 0, 14));

        TextView t = Views.text(c, title.toUpperCase(), 18, Theme.TEXT, Theme.heavy);
        t.setLetterSpacing(0.06f);
        box.addView(t);

        ScrollView scroll = new ScrollView(c);
        scroll.setVerticalScrollBarEnabled(false);
        TextView m = Views.text(c, message, 13, Theme.MUTED, Theme.body);
        m.setLineSpacing(dp(3), 1f);
        m.setTextIsSelectable(true);
        scroll.addView(m);
        // Высота 0 + вес: длинный текст прокручивается, а кнопки всегда видны.
        box.addView(scroll, Views.margins(Views.lp(MATCH, 0, 1), 0, 12, 0, 0));

        LinearLayout buttons = Views.horizontal(c);
        buttons.setGravity(Gravity.END | Gravity.CENTER_VERTICAL);
        for (final Action a : actions) {
            TextView b = a.primary ? Views.primaryButton(c, a.label, 12) : Views.ghostButton(c, a.label);
            b.setOnClickListener(new View.OnClickListener() {
                @Override public void onClick(View v) {
                    overlay.dismiss();
                    if (a.action != null) a.action.run();
                }
            });
            buttons.addView(b, Views.margins(Views.lp(WRAP, dp(40)), 10, 0, 0, 0));
        }
        box.addView(buttons, Views.margins(Views.lp(MATCH, WRAP), 0, 18, 0, 0));

        int maxWidth = Math.min(dp(480), c.getResources().getDisplayMetrics().widthPixels - dp(48));
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(maxWidth, WRAP, Gravity.CENTER);
        lp.topMargin = dp(16);
        lp.bottomMargin = dp(16);
        scrim.addView(box, lp);
        host.addView(scrim, new FrameLayout.LayoutParams(MATCH, MATCH));
        return overlay;
    }
}
