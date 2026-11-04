package online.bastion.launcher;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.os.Environment;
import android.view.Display;
import android.view.WindowManager;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.security.MessageDigest;
import java.util.Enumeration;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * Отчёт о телефоне и установленной GTA: San Andreas. Нужен для этапа 1:
 * по версии и SHA-256 libGTASA.so мы подбираем адреса функций игры.
 * Только читает, ничего не меняет и никуда не отправляет сам.
 */
public final class Diagnostics {
    private Diagnostics() { }

    /** Долгий вызов (хэширует библиотеку): только из фонового потока. */
    @SuppressWarnings("deprecation")
    public static String report(Context ctx) {
        StringBuilder r = new StringBuilder();
        r.append("=== Bastion Online: диагностика ===\n");
        try {
            r.append("Лаунчер: ").append(ctx.getPackageManager().getPackageInfo(ctx.getPackageName(), 0).versionName).append('\n');
        } catch (Exception ignored) { }

        r.append("\n[Телефон]\n");
        r.append("Модель: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
                .append(" (").append(Build.DEVICE).append(")\n");
        r.append("Android: ").append(Build.VERSION.RELEASE).append(" (API ").append(Build.VERSION.SDK_INT).append(")\n");
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

        r.append("\n[GTA: San Andreas]\n");
        PackageInfo pi;
        try {
            pi = ctx.getPackageManager().getPackageInfo(MainActivity.GTA_PACKAGE, 0);
        } catch (Exception e) {
            r.append("Не установлена (").append(MainActivity.GTA_PACKAGE).append(")\n");
            return r.toString();
        }
        r.append("Версия: ").append(pi.versionName).append(" (code ").append(pi.versionCode).append(")\n");
        ApplicationInfo ai = pi.applicationInfo;
        r.append("APK: ").append(ai.sourceDir).append('\n');
        if (ai.splitSourceDirs != null)
            for (String s : ai.splitSourceDirs) r.append("Split: ").append(s).append('\n');
        r.append("Native libs: ").append(ai.nativeLibraryDir).append('\n');

        boolean found = false;
        File libDir = new File(ai.nativeLibraryDir);
        File[] libs = libDir.listFiles();
        if (libs != null && libs.length > 0) {
            for (File f : libs) {
                r.append("  ").append(f.getName()).append("  ").append(f.length()).append(" байт");
                if (f.getName().equals("libGTASA.so")) {
                    r.append("  sha256=").append(sha256File(f));
                    found = true;
                }
                r.append('\n');
            }
        } else {
            r.append("  (папка пустая или нет доступа: библиотеки внутри APK)\n");
        }

        // Библиотеки могут лежать несжатыми прямо в APK (extractNativeLibs=false).
        if (!found) {
            String[] apks = concat(ai.sourceDir, ai.splitSourceDirs);
            for (String apk : apks) {
                ZipFile z = null;
                try {
                    z = new ZipFile(apk);
                    Enumeration<? extends ZipEntry> en = z.entries();
                    while (en.hasMoreElements()) {
                        ZipEntry e = en.nextElement();
                        String n = e.getName();
                        if (n.startsWith("lib/") && n.endsWith(".so")) {
                            r.append("  [").append(new File(apk).getName()).append("] ").append(n)
                                    .append("  ").append(e.getSize()).append(" байт")
                                    .append(e.getMethod() == ZipEntry.STORED ? " stored" : " deflated");
                            if (n.endsWith("/libGTASA.so")) {
                                r.append("  sha256=").append(sha256Stream(z.getInputStream(e)));
                                found = true;
                            }
                            r.append('\n');
                        }
                    }
                } catch (Exception ex) {
                    r.append("  не читается ").append(apk).append(": ").append(ex.getClass().getSimpleName()).append('\n');
                } finally {
                    if (z != null) try { z.close(); } catch (Exception ignored) { }
                }
            }
        }
        if (!found) r.append("libGTASA.so не найдена\n");

        r.append("\n[Файлы игры]\n");
        File ext = Environment.getExternalStorageDirectory();
        dir(r, new File(ext, "Android/obb/" + MainActivity.GTA_PACKAGE));
        dir(r, new File(ext, "Android/data/" + MainActivity.GTA_PACKAGE + "/files"));
        return r.toString();
    }

    private static void dir(StringBuilder r, File d) {
        r.append(d.getAbsolutePath()).append(": ");
        String[] names = d.list();
        if (names == null) {
            r.append(d.exists() ? "есть, но нет доступа" : "нет доступа или нет папки").append('\n');
            return;
        }
        r.append(names.length).append(" файлов\n");
        for (int i = 0; i < names.length && i < 20; i++) {
            File f = new File(d, names[i]);
            r.append("  ").append(names[i]);
            if (f.isFile()) r.append("  ").append(f.length() / (1024 * 1024)).append(" МБ");
            r.append('\n');
        }
    }

    private static String[] concat(String a, String[] rest) {
        int n = rest == null ? 0 : rest.length;
        String[] out = new String[n + 1];
        out[0] = a;
        for (int i = 0; i < n; i++) out[i + 1] = rest[i];
        return out;
    }

    private static String join(String[] a) {
        if (a == null || a.length == 0) return "-";
        StringBuilder b = new StringBuilder();
        for (String s : a) { if (b.length() > 0) b.append(", "); b.append(s); }
        return b.toString();
    }

    private static String sha256File(File f) {
        try { return sha256Stream(new FileInputStream(f)); } catch (Exception e) { return "ошибка"; }
    }

    private static String sha256Stream(InputStream in) {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) md.update(buf, 0, n);
            in.close();
            StringBuilder h = new StringBuilder();
            for (byte b : md.digest()) h.append(String.format("%02x", b));
            return h.toString();
        } catch (Exception e) {
            return "ошибка";
        }
    }
}
