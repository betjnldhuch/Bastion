package online.bastion.launcher;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Build;
import android.view.Display;
import android.view.WindowManager;

/** Device-only diagnostics. Does not inspect or require a particular GTA installation. */
public final class Diagnostics {
    private Diagnostics() { }

    @SuppressWarnings("deprecation")
    public static String report(Context ctx) {
        StringBuilder r = new StringBuilder();
        r.append("=== Bastion Online: диагностика ===\n");
        try {
            r.append("Лаунчер: ").append(ctx.getPackageManager()
                    .getPackageInfo(ctx.getPackageName(), 0).versionName).append('\n');
        } catch (Exception ignored) { }

        r.append("\n[Телефон]\n");
        r.append("Модель: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
                .append(" (").append(Build.DEVICE).append(")\n");
        r.append("Android: ").append(Build.VERSION.RELEASE).append(" (API ")
                .append(Build.VERSION.SDK_INT).append(")\n");
        r.append("ABI: ").append(join(Build.SUPPORTED_ABIS)).append('\n');
        r.append("32-bit ABI: ").append(join(Build.SUPPORTED_32_BIT_ABIS)).append('\n');
        try {
            ActivityManager am = (ActivityManager) ctx.getSystemService(Context.ACTIVITY_SERVICE);
            ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
            am.getMemoryInfo(mi);
            r.append("RAM: ").append(mi.totalMem / (1024 * 1024)).append(" МБ\n");
        } catch (Exception ignored) { }
        try {
            Display d = ((WindowManager) ctx.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();
            r.append("Экран: ").append(Math.round(d.getRefreshRate())).append(" Гц сейчас");
            if (Build.VERSION.SDK_INT >= 23) {
                StringBuilder modes = new StringBuilder();
                for (Display.Mode m : d.getSupportedModes()) {
                    String hz = String.valueOf(Math.round(m.getRefreshRate()));
                    if (modes.indexOf(hz + " ") < 0 && modes.indexOf(hz + ",") < 0) {
                        if (modes.length() > 0) modes.append(", ");
                        modes.append(hz);
                    }
                }
                r.append(", поддерживает: ").append(modes).append(" Гц");
            }
            r.append('\n');
        } catch (Exception ignored) { }
        return r.toString();
    }

    private static String join(String[] values) {
        if (values == null || values.length == 0) return "-";
        StringBuilder out = new StringBuilder();
        for (String value : values) {
            if (out.length() > 0) out.append(", ");
            out.append(value);
        }
        return out.toString();
    }
}
