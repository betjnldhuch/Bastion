package online.bastion.launcher.ui;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.text.InputFilter;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import static online.bastion.launcher.ui.Theme.dp;

/** Фабрика элементов интерфейса. Экраны собираются только из них. */
public final class Views {

    public static final int MATCH = ViewGroup.LayoutParams.MATCH_PARENT;
    public static final int WRAP = ViewGroup.LayoutParams.WRAP_CONTENT;

    private Views() { }

    public static TextView text(Context c, String s, float sp, int color, android.graphics.Typeface tf) {
        TextView t = new TextView(c);
        t.setText(s);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        t.setTypeface(tf);
        t.setIncludeFontPadding(false);
        return t;
    }

    /** Прямоугольник: заливка + рамка. Углы прямые — так строже. */
    public static GradientDrawable box(int fill, int stroke, float strokeDp) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(fill);
        if (strokeDp > 0) g.setStroke(Math.max(1, dp(strokeDp)), stroke);
        return g;
    }

    private static Drawable pressable(Drawable normal, Drawable pressed) {
        StateListDrawable d = new StateListDrawable();
        d.addState(new int[]{android.R.attr.state_pressed}, pressed);
        d.addState(new int[]{-android.R.attr.state_enabled}, box(Theme.SURFACE, Theme.LINE, 1));
        d.addState(new int[]{}, normal);
        return d;
    }

    /** Подпись секции: оранжевый квадрат + разреженные заглавные буквы. */
    public static TextView caption(Context c, String s) {
        TextView t = text(c, s.toUpperCase(), 11, Theme.MUTED, Theme.heavy);
        t.setLetterSpacing(0.18f);
        GradientDrawable mark = box(Theme.ORANGE, 0, 0);
        mark.setSize(dp(6), dp(6));
        t.setCompoundDrawablesWithIntrinsicBounds(mark, null, null, null);
        t.setCompoundDrawablePadding(dp(10));
        t.setGravity(Gravity.CENTER_VERTICAL);
        return t;
    }

    /** Главная кнопка: оранжевый блок с чёрным текстом. */
    public static TextView primaryButton(Context c, String s, float sp) {
        TextView b = text(c, s.toUpperCase(), sp, Theme.BLACK, Theme.heavy);
        b.setGravity(Gravity.CENTER);
        b.setLetterSpacing(0.12f);
        b.setPadding(dp(24), 0, dp(24), 0);
        b.setBackground(pressable(box(Theme.ORANGE, 0, 0), box(Theme.ORANGE_PRESSED, 0, 0)));
        b.setClickable(true);
        b.setFocusable(true);
        return b;
    }

    /** Второстепенная кнопка: тонкая рамка на чёрном. */
    public static TextView ghostButton(Context c, String s) {
        TextView b = text(c, s.toUpperCase(), 11, Theme.TEXT, Theme.heavy);
        b.setGravity(Gravity.CENTER);
        b.setLetterSpacing(0.12f);
        b.setPadding(dp(16), 0, dp(16), 0);
        b.setBackground(pressable(box(Theme.BLACK, Theme.LINE, 1), box(Theme.SURFACE_HI, Theme.ORANGE, 1)));
        b.setClickable(true);
        b.setFocusable(true);
        return b;
    }

    /** Поле ввода в рамке. */
    public static EditText field(Context c, String value, String hint, int maxLength) {
        EditText e = new EditText(c);
        e.setText(value);
        e.setHint(hint);
        e.setHintTextColor(Theme.DIM);
        e.setTextColor(Theme.TEXT);
        e.setTypeface(Theme.bodyMedium);
        e.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        e.setSingleLine(true);
        e.setFilters(new InputFilter[]{new InputFilter.LengthFilter(maxLength)});
        // VISIBLE_PASSWORD отключает автозамену и подсказки клавиатуры — для ников и адресов это нужно.
        e.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        e.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_ACTION_DONE);
        e.setPadding(dp(12), 0, dp(12), 0);
        e.setGravity(Gravity.CENTER_VERTICAL);
        setFieldState(e, false);
        return e;
    }

    /** Подсветить поле с ошибкой (или снять подсветку). */
    public static void setFieldState(EditText e, boolean error) {
        if (!e.isEnabled()) {
            e.setBackground(box(Theme.SURFACE, Theme.SURFACE, 1));
            e.setTextColor(Theme.MUTED);
            return;
        }
        e.setTextColor(Theme.TEXT);
        StateListDrawable d = new StateListDrawable();
        d.addState(new int[]{android.R.attr.state_focused}, box(Theme.SURFACE, error ? Theme.ERROR : Theme.ORANGE, 1));
        d.addState(new int[]{}, box(Theme.SURFACE, error ? Theme.ERROR : Theme.LINE, 1));
        e.setBackground(d);
    }

    /** Горизонтальная линия: оранжевый отрезок слева, дальше серая. */
    public static View rule(Context c) {
        View v = new View(c);
        v.setBackground(new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,
                new int[]{Theme.ORANGE, Theme.LINE, Theme.LINE, 0x00262626}));
        return v;
    }

    public interface OnPick { void onPick(int value); }

    /** Строка «подпись + переключатель из нескольких вариантов». */
    public static View segmented(Context c, String label, final String[] names, final int[] values,
                                 int current, final OnPick onPick) {
        LinearLayout row = new LinearLayout(c);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, dp(10), 0, 0);
        row.addView(text(c, label, 13, Theme.TEXT, Theme.body), new LinearLayout.LayoutParams(0, WRAP, 1));

        LinearLayout seg = new LinearLayout(c);
        seg.setOrientation(LinearLayout.HORIZONTAL);
        seg.setBackground(box(Theme.BLACK, Theme.LINE, 1));
        seg.setPadding(dp(2), dp(2), dp(2), dp(2));
        final TextView[] items = new TextView[names.length];
        int selected = 0;
        for (int i = 0; i < names.length; i++) {
            if (values[i] == current) selected = i;
            final int index = i;
            TextView t = text(c, names[i], 12, Theme.MUTED, Theme.heavy);
            t.setGravity(Gravity.CENTER);
            t.setMinWidth(dp(46));
            t.setPadding(dp(10), 0, dp(10), 0);
            t.setOnClickListener(new View.OnClickListener() {
                @Override public void onClick(View v) {
                    paintSegments(items, index);
                    onPick.onPick(values[index]);
                }
            });
            items[i] = t;
            seg.addView(t, new LinearLayout.LayoutParams(WRAP, dp(30)));
        }
        paintSegments(items, selected);
        row.addView(seg);
        return row;
    }

    private static void paintSegments(TextView[] items, int selected) {
        for (int i = 0; i < items.length; i++) {
            boolean on = i == selected;
            items[i].setBackground(on ? box(Theme.ORANGE, 0, 0) : null);
            items[i].setTextColor(on ? Theme.BLACK : Theme.MUTED);
        }
    }

    public static LinearLayout vertical(Context c) {
        LinearLayout l = new LinearLayout(c);
        l.setOrientation(LinearLayout.VERTICAL);
        return l;
    }

    public static LinearLayout horizontal(Context c) {
        LinearLayout l = new LinearLayout(c);
        l.setOrientation(LinearLayout.HORIZONTAL);
        l.setGravity(Gravity.CENTER_VERTICAL);
        return l;
    }

    public static LinearLayout.LayoutParams lp(int w, int h) { return new LinearLayout.LayoutParams(w, h); }

    public static LinearLayout.LayoutParams lp(int w, int h, float weight) {
        return new LinearLayout.LayoutParams(w, h, weight);
    }

    public static LinearLayout.LayoutParams margins(LinearLayout.LayoutParams p, int l, int t, int r, int b) {
        p.setMargins(dp(l), dp(t), dp(r), dp(b));
        return p;
    }
}
