package online.bastion.launcher;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RadialGradient;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.TextView;

/** Цвета, шрифты и нарисованные кодом элементы в стиле ПК-лаунчера. */
public final class Ui {
    public static final int BG = 0xFF0B0C0F;
    public static final int PANEL = 0xFF14161A;
    public static final int PANEL_HI = 0xFF1B1E23;
    public static final int LINE = 0xFF2A2D33;
    public static final int TEXT = 0xFFECECEC;
    public static final int MUTED = 0xFF8C9098;
    public static final int DIM = 0xFF5A5F68;
    public static final int ORANGE = 0xFFFF7A1A;
    public static final int ORANGE_DARK = 0xFFC2540A;
    public static final int GREEN = 0xFF4CD07D;
    public static final int RED = 0xFFE5484D;

    public static Typeface heavy;   // Russo One
    public static Typeface body = Typeface.create("sans-serif", Typeface.NORMAL);
    public static Typeface bodyBold = Typeface.create("sans-serif-medium", Typeface.NORMAL);

    private static float density = 1f;

    private Ui() { }

    public static void init(Context ctx) {
        density = ctx.getResources().getDisplayMetrics().density;
        try {
            heavy = Typeface.createFromAsset(ctx.getAssets(), "fonts/RussoOne-Regular.ttf");
        } catch (Exception e) {
            heavy = Typeface.DEFAULT_BOLD;
        }
    }

    public static int dp(float v) { return Math.round(v * density); }

    public static GradientDrawable box(int fill, int stroke, float radiusDp, float strokeDp) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(fill);
        g.setCornerRadius(dp(radiusDp));
        if (strokeDp > 0) g.setStroke(Math.max(1, dp(strokeDp)), stroke);
        return g;
    }

    public static TextView text(Context c, String s, float sp, int color, Typeface tf) {
        TextView t = new TextView(c);
        t.setText(s);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        t.setTypeface(tf);
        t.setIncludeFontPadding(false);
        return t;
    }

    /** Заголовок секции: мелкие разряженные буквы с оранжевой чёрточкой. */
    public static TextView caption(Context c, String s) {
        TextView t = text(c, s.toUpperCase(), 11, MUTED, heavy);
        t.setLetterSpacing(0.15f);
        t.setCompoundDrawablePadding(dp(8));
        GradientDrawable bar = new GradientDrawable();
        bar.setColor(ORANGE);
        bar.setSize(dp(3), dp(12));
        t.setCompoundDrawablesWithIntrinsicBounds(bar, null, null, null);
        t.setGravity(Gravity.CENTER_VERTICAL);
        return t;
    }

    /** Большая оранжевая кнопка (ИГРАТЬ). */
    public static TextView primaryButton(Context c, String s) {
        TextView b = text(c, s, 20, Color.WHITE, heavy);
        b.setGravity(Gravity.CENTER);
        b.setLetterSpacing(0.08f);
        GradientDrawable g = new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                new int[]{0xFFFF9442, ORANGE, ORANGE_DARK});
        g.setCornerRadius(dp(6));
        b.setBackground(g);
        b.setShadowLayer(dp(2), 0, dp(1), 0x99000000);
        b.setPadding(dp(28), 0, dp(28), 0);
        pressFeedback(b);
        return b;
    }

    /** Тихая кнопка с обводкой. */
    public static TextView ghostButton(Context c, String s) {
        TextView b = text(c, s.toUpperCase(), 12, TEXT, heavy);
        b.setGravity(Gravity.CENTER);
        b.setLetterSpacing(0.1f);
        b.setBackground(box(0x00000000, LINE, 6, 1));
        b.setPadding(dp(16), 0, dp(16), 0);
        pressFeedback(b);
        return b;
    }

    public static void pressFeedback(final View v) {
        v.setClickable(true);
        v.setOnTouchListener(new View.OnTouchListener() {
            @Override public boolean onTouch(View view, android.view.MotionEvent e) {
                if (e.getAction() == android.view.MotionEvent.ACTION_DOWN) view.setAlpha(0.75f);
                else if (e.getAction() == android.view.MotionEvent.ACTION_UP
                        || e.getAction() == android.view.MotionEvent.ACTION_CANCEL) view.setAlpha(1f);
                return false;
            }
        });
    }

    // ------------------------------------------------------------------
    //  Фон: случайная сцена по локальному времени суток
    //  (assets/loadscreens/<сцена>_<day|sunset|night>.jpg: день 7–17,
    //  закат 17–20, ночь 20–7) под затемнением. Без картинки — тёмный
    //  градиент, тёплое свечение в углу и тонкие диагонали.
    // ------------------------------------------------------------------
    public static final String[] SCENES = {"pier", "chase", "grove", "lv", "vinewood"};

    public static String period(java.util.Calendar c) {
        int h = c.get(java.util.Calendar.HOUR_OF_DAY);
        if (h >= 7 && h < 17) return "day";
        if (h >= 17 && h < 20) return "sunset";
        return "night";
    }

    public static Bitmap loadScreen(Context c) {
        String name = SCENES[new java.util.Random().nextInt(SCENES.length)] + "_"
                + period(java.util.Calendar.getInstance());
        try (java.io.InputStream in = c.getAssets().open("loadscreens/" + name + ".jpg")) {
            return BitmapFactory.decodeStream(in);
        } catch (Exception | OutOfMemoryError e) {
            return null;
        }
    }

    public static final class Background extends View {
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint img = new Paint(Paint.FILTER_BITMAP_FLAG);
        private final Bitmap bmp;
        private final Rect src = new Rect();
        private final Rect dst = new Rect();
        public Background(Context c) { super(c); bmp = loadScreen(c); }

        @Override protected void onDraw(Canvas cv) {
            int w = getWidth(), h = getHeight();
            if (bmp != null && w > 0 && h > 0) {
                // «cover»: обрезаем по центру под пропорции экрана
                int bw = bmp.getWidth(), bh = bmp.getHeight();
                if ((long) bw * h > (long) bh * w) {
                    int cw = (int) ((long) bh * w / h);
                    src.set((bw - cw) / 2, 0, (bw + cw) / 2, bh);
                } else {
                    int ch = (int) ((long) bw * h / w);
                    src.set(0, (bh - ch) / 2, bw, (bh + ch) / 2);
                }
                dst.set(0, 0, w, h);
                cv.drawBitmap(bmp, src, dst, img);
                // затемнение, чтобы читались панели и текст
                p.setShader(new LinearGradient(0, 0, w, 0, 0x8C0B0C0F, 0xD90B0C0F, Shader.TileMode.CLAMP));
                cv.drawRect(0, 0, w, h, p);
                p.setShader(new LinearGradient(0, h * 0.55f, 0, h, 0x000B0C0F, 0xE60B0C0F, Shader.TileMode.CLAMP));
                cv.drawRect(0, h * 0.55f, w, h, p);
                p.setShader(null);
                return;
            }
            p.setShader(new LinearGradient(0, 0, 0, h, 0xFF101216, 0xFF07080A, Shader.TileMode.CLAMP));
            cv.drawRect(0, 0, w, h, p);
            p.setShader(new RadialGradient(w * 0.08f, -h * 0.1f, Math.max(w, h) * 0.55f,
                    0x33FF7A1A, 0x00FF7A1A, Shader.TileMode.CLAMP));
            cv.drawRect(0, 0, w, h, p);
            p.setShader(null);
            p.setColor(0x08FFFFFF);
            p.setStrokeWidth(dp(1));
            int step = dp(22);
            for (int x = -h; x < w; x += step) cv.drawLine(x, h, x + h, 0, p);
        }
    }

    // ------------------------------------------------------------------
    //  Шапка «BASTION ONLINE» — тяжёлый металл, как в танковых играх.
    // ------------------------------------------------------------------
    public static final class Header extends View {
        private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint small = new Paint(Paint.ANTI_ALIAS_FLAG);

        public Header(Context c) {
            super(c);
            setLayerType(LAYER_TYPE_SOFTWARE, null); // нужен для теней у текста
            fill.setTypeface(heavy);
            stroke.setTypeface(heavy);
            stroke.setStyle(Paint.Style.STROKE);
            stroke.setStrokeJoin(Paint.Join.ROUND);
            small.setTypeface(heavy);
            small.setColor(MUTED);
            small.setLetterSpacing(0.45f);
        }

        @Override protected void onMeasure(int wSpec, int hSpec) {
            setMeasuredDimension(MeasureSpec.getSize(wSpec), dp(58));
        }

        @Override protected void onDraw(Canvas cv) {
            float size = dp(34);
            float base = dp(38);
            fill.setTextSize(size);
            stroke.setTextSize(size);
            stroke.setStrokeWidth(dp(3));

            String a = "BASTION";
            String b = " ONLINE";
            float wa = fill.measureText(a);
            float x = 0;

            // тень + чёрная обводка
            stroke.setColor(0xFF000000);
            stroke.setShadowLayer(dp(6), 0, dp(3), 0xCC000000);
            cv.drawText(a + b, x, base, stroke);
            stroke.clearShadowLayer();

            // BASTION — стальной градиент
            fill.setShader(new LinearGradient(0, base - size, 0, base,
                    new int[]{0xFFFFFFFF, 0xFFD6D9DE, 0xFF8A8F98, 0xFFB9BDC4},
                    new float[]{0f, 0.45f, 0.55f, 1f}, Shader.TileMode.CLAMP));
            cv.drawText(a, x, base, fill);

            // ONLINE — оранжевый
            fill.setShader(new LinearGradient(0, base - size, 0, base,
                    new int[]{0xFFFFB066, ORANGE, ORANGE_DARK}, null, Shader.TileMode.CLAMP));
            cv.drawText(b, x + wa, base, fill);
            fill.setShader(null);

            // блик сверху
            stroke.setColor(0x33FFFFFF);
            stroke.setStrokeWidth(dp(1));
            cv.drawText(a, x, base - dp(1), stroke);

            small.setTextSize(dp(9));
            cv.drawText("SAN ANDREAS  ROLEPLAY", x + dp(2), base + dp(15), small);
        }
    }
}
