package online.bastion.launcher;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;

import java.io.File;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.util.List;

/**
 * Отчёт о падении встроенного клиента. Игра работает в процессе ":game";
 * когда он падает, Android сохраняет причину (ApplicationExitInfo, Android 11+),
 * а для нативного падения — tombstone. Плюс хвост files/game/.../logcat.txt,
 * который пишет сам клиент, и Java-исключение из files/last_crash.txt.
 */
public final class GameCrashReport {
    private GameCrashReport() { }

    private static final String PREFS = "bastion_game";
    private static final String KEY_LAUNCH = "launch_time";
    private static final String KEY_SHOWN = "shown_exit";
    static final String REPORT_FILE = "last_game_crash.txt";

    /** Вызывается перед запуском игры. */
    public static void markLaunch(Context ctx) {
        prefs(ctx).edit().putLong(KEY_LAUNCH, System.currentTimeMillis()).apply();
        new File(ctx.getFilesDir(), "last_crash.txt").delete();
    }

    /**
     * Если после последнего запуска процесс игры упал — собирает отчёт, сохраняет
     * его в files/last_game_crash.txt и возвращает (иначе null). Каждое падение — один раз.
     */
    public static String collectNew(Context ctx) {
        SharedPreferences p = prefs(ctx);
        long launch = p.getLong(KEY_LAUNCH, 0);
        if (launch == 0) return null;
        StringBuilder r = new StringBuilder();
        boolean crashed = false;
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                ActivityManager am = (ActivityManager) ctx.getSystemService(Context.ACTIVITY_SERVICE);
                List<ApplicationExitInfo> exits = am.getHistoricalProcessExitReasons(ctx.getPackageName(), 0, 10);
                for (ApplicationExitInfo e : exits) {
                    if (e.getProcessName() == null || !e.getProcessName().endsWith(":game")) continue;
                    if (e.getTimestamp() < launch) break;
                    if (p.getLong(KEY_SHOWN, 0) == e.getTimestamp()) return null;
                    int reason = e.getReason();
                    if (reason == ApplicationExitInfo.REASON_EXIT_SELF && e.getStatus() == 0) return null;
                    if (reason == ApplicationExitInfo.REASON_USER_REQUESTED
                            || reason == ApplicationExitInfo.REASON_USER_STOPPED) return null;
                    crashed = true;
                    p.edit().putLong(KEY_SHOWN, e.getTimestamp()).apply();
                    r.append("Причина: ").append(reasonName(reason))
                            .append(", статус ").append(e.getStatus()).append('\n');
                    if (e.getDescription() != null) r.append("Описание: ").append(e.getDescription()).append('\n');
                    r.append("Через ").append((e.getTimestamp() - launch) / 1000).append(" с после запуска\n");
                    if (reason == ApplicationExitInfo.REASON_CRASH_NATIVE || reason == ApplicationExitInfo.REASON_ANR) {
                        String trace = readTrace(e);
                        if (trace != null) r.append("\n[Трасса]\n").append(trace).append('\n');
                    }
                    break;
                }
            } catch (Exception ex) {
                r.append("Нет доступа к причине выхода: ").append(ex).append('\n');
            }
        }
        String javaCrash = readTail(new File(ctx.getFilesDir(), "last_crash.txt"), 4000);
        if (javaCrash != null) { crashed = true; r.append("\n[Java-исключение]\n").append(javaCrash).append('\n'); }
        if (!crashed) return null;
        File root = ClientInstaller.findSampStorageRoot(ctx);
        if (root != null) {
            String log = readTail(new File(root, "logcat.txt"), 5000);
            if (log != null) r.append("\n[logcat.txt клиента, конец]\n").append(log).append('\n');
        }
        String text = r.toString();
        try {
            java.io.FileOutputStream f = new java.io.FileOutputStream(new File(ctx.getFilesDir(), REPORT_FILE));
            try { f.write(text.getBytes("UTF-8")); } finally { f.close(); }
        } catch (Exception ignored) { }
        return text;
    }

    /** Последний сохранённый отчёт (для «Диагностики»). */
    public static String last(Context ctx) {
        return readTail(new File(ctx.getFilesDir(), REPORT_FILE), 12000);
    }

    private static String readTrace(ApplicationExitInfo e) {
        try {
            InputStream in = e.getTraceInputStream();
            if (in == null) return null;
            try { return Tombstone.format(in, 60); } finally { in.close(); }
        } catch (Exception ex) { return "трасса недоступна: " + ex; }
    }

    static String readTail(File f, int max) {
        if (f == null || !f.isFile() || f.length() == 0) return null;
        try {
            RandomAccessFile raf = new RandomAccessFile(f, "r");
            try {
                long start = Math.max(0, raf.length() - max);
                byte[] data = new byte[(int) (raf.length() - start)];
                raf.seek(start);
                raf.readFully(data);
                return new String(data, "UTF-8");
            } finally { raf.close(); }
        } catch (Exception e) { return null; }
    }

    private static String reasonName(int reason) {
        // Значения ApplicationExitInfo.REASON_*.
        switch (reason) {
            case 1: return "выход с кодом (EXIT_SELF)";
            case 2: return "сигнал (SIGNALED)";
            case 3: return "нехватка памяти (LOW_MEMORY)";
            case 4: return "Java-исключение (CRASH)";
            case 5: return "нативное падение (CRASH_NATIVE)";
            case 6: return "зависание (ANR)";
            case 7: return "ошибка инициализации (INITIALIZATION_FAILURE)";
            default: return "код " + reason;
        }
    }

    private static SharedPreferences prefs(Context ctx) {
        return ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }
}
