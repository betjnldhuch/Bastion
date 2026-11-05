package online.bastion.launcher.ui;

import android.content.Context;
import android.graphics.Typeface;

/**
 * Весь внешний вид в одном месте: цвета, шрифты, размеры.
 * Чтобы перекрасить лаунчер, достаточно поменять константы здесь.
 */
public final class Theme {

    // Цвета: чёрный основной, оранжевый — только акценты.
    public static final int BLACK = 0xFF000000;
    public static final int SURFACE = 0xFF0C0C0C;     // карточки и панели
    public static final int SURFACE_HI = 0xFF151515;  // выбранная карточка
    public static final int LINE = 0xFF262626;        // тонкие рамки
    public static final int TEXT = 0xFFF2F2F2;
    public static final int MUTED = 0xFF8A8A8A;
    public static final int DIM = 0xFF4D4D4D;
    public static final int ORANGE = 0xFFFF6A00;
    public static final int ORANGE_PRESSED = 0xFFCC5500;
    public static final int ERROR = 0xFFFF3B30;
    public static final int SCRIM = 0xD9000000;       // затемнение под окнами

    // Шрифты (assets/fonts). Оба — SIL Open Font License, лицензии лежат рядом.
    private static final String TITLE_FONT = "fonts/BlackOpsOne-Regular.ttf"; // трафарет, только латиница
    private static final String HEAVY_FONT = "fonts/RussoOne-Regular.ttf";    // с кириллицей

    /** Крупный заголовок-логотип (трафарет). */
    public static Typeface title = Typeface.DEFAULT_BOLD;
    /** Кнопки, подписи, названия серверов. */
    public static Typeface heavy = Typeface.DEFAULT_BOLD;
    /** Обычный текст. */
    public static Typeface body = Typeface.create("sans-serif", Typeface.NORMAL);
    public static Typeface bodyMedium = Typeface.create("sans-serif-medium", Typeface.NORMAL);

    private static float density = 1f;
    private static boolean loaded;

    private Theme() { }

    public static void init(Context context) {
        density = context.getResources().getDisplayMetrics().density;
        if (loaded) return;
        title = load(context, TITLE_FONT, Typeface.DEFAULT_BOLD);
        heavy = load(context, HEAVY_FONT, Typeface.DEFAULT_BOLD);
        loaded = true;
    }

    private static Typeface load(Context c, String path, Typeface fallback) {
        try {
            return Typeface.createFromAsset(c.getAssets(), path);
        } catch (RuntimeException e) {
            return fallback;  // шрифт не попал в сборку — работаем на системном
        }
    }

    public static int dp(float v) { return Math.round(v * density); }
}
