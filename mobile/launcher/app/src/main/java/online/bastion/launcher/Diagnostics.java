package online.bastion.launcher;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Build;
import android.view.Display;
import android.view.WindowManager;

import java.io.File;

/** Диагностика устройства и папки ресурсов Bastion. Не проверяет наличие сторонней внешней игры. */
public final class Diagnostics {
    private Diagnostics() { }

    private static final String CRASH_FILE = "last_crash.txt";

    /**
     * Записывает последнее необработанное исключение процесса (лаунчер и встроенный
     * клиент работают в одном процессе) в files/last_crash.txt, затем передаёт его
     * стандартному обработчику — приложение закрывается как обычно.
     */
    public static void installCrashLog(Context ctx) {
        final File file = new File(ctx.getApplicationContext().getFilesDir(), CRASH_FILE);
        final Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        if (previous != null && previous.getClass().getName().startsWith(Diagnostics.class.getName())) return;
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override public void uncaughtException(Thread thread, Throwable error) {
                try {
                    java.io.StringWriter text = new java.io.StringWriter();
                    java.io.PrintWriter out = new java.io.PrintWriter(text);
                    out.println(new java.util.Date() + " · поток " + thread.getName());
                    error.printStackTrace(out);
                    out.flush();
                    java.io.FileOutputStream f = new java.io.FileOutputStream(file);
                    try { f.write(text.toString().getBytes("UTF-8")); } finally { f.close(); }
                } catch (Throwable ignored) { }
                if (previous != null) previous.uncaughtException(thread, error);
            }
        });
    }

    private static String lastCrash(Context ctx) {
        File file = new File(ctx.getFilesDir(), CRASH_FILE);
        if (!file.isFile()) return null;
        try {
            java.io.FileInputStream in = new java.io.FileInputStream(file);
            try {
                byte[] data = new byte[(int) Math.min(file.length(), 6000)];
                int n = in.read(data);
                return n <= 0 ? null : new String(data, 0, n, "UTF-8");
            } finally { in.close(); }
        } catch (Exception e) { return null; }
    }

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

        r.append("\n[Файлы Bastion]\n");
        File client = ClientInstaller.gameDir(ctx);
        if (client == null) client = new File(ctx.getFilesDir(), "game");
        r.append("Папка игры: ").append(client.getAbsolutePath()).append('\n');
        r.append("Установлено: ").append(ClientInstaller.isGameInstalled(ctx) ? "да" : "нет").append('\n');
        File sampRoot = ClientInstaller.findSampStorageRoot(ctx);
        r.append("Корень SAMP-Mobile: ").append(sampRoot == null ? "не найден" : sampRoot.getAbsolutePath()).append('\n');
        String warnings = ClientInstaller.installWarnings(ctx);
        if (warnings.length() > 0) r.append(warnings).append('\n');
        String[] names = client.list();
        if (names == null) {
            r.append(client.exists() ? "Папка есть, но нет доступа\n" : "Ресурсы ещё не загружены\n");
        } else {
            r.append("Элементов: ").append(names.length).append('\n');
            for (int i = 0; i < names.length && i < 30; i++) {
                File item = new File(client, names[i]);
                r.append("  ").append(names[i]);
                if (item.isFile()) r.append(" — ").append(item.length() / (1024 * 1024)).append(" МБ");
                r.append('\n');
            }
        }
        if (sampRoot != null) {
            r.append("\n[Файлы SAMP-Mobile]\n");
            for (String rel : ClientInstaller.REQUIRED_FILES) {
                File f = ClientInstaller.resolveIgnoreCase(sampRoot, rel);
                r.append(f != null && f.isFile() ? "ok   " : "НЕТ  ").append(rel).append('\n');
            }
            File texdb = ClientInstaller.resolveIgnoreCase(sampRoot, "texdb");
            r.append(texdb != null ? "ok   " : "НЕТ  ").append("texdb/\n");
            File samp = new File(sampRoot, ClientInstaller.SAMP_DIR);
            String[] sampNames = samp.list();
            if (sampNames != null) {
                java.util.Arrays.sort(sampNames);
                r.append("SAMP/: ");
                for (int i = 0; i < sampNames.length && i < 60; i++) r.append(i == 0 ? "" : ", ").append(sampNames[i]);
                r.append('\n');
            }
        }
        String game = GameCrashReport.last(ctx);
        r.append("\n[Последнее падение игры]\n").append(game == null ? "нет\n" : game);
        String crash = lastCrash(ctx);
        r.append("\n[Последний вылет лаунчера]\n").append(crash == null ? "нет\n" : crash);
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
