package online.bastion.launcher.game;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.List;

import online.bastion.launcher.core.LauncherConfig;
import online.bastion.launcher.core.SampSettingsIni;
import online.bastion.launcher.core.ServerAddress;
import online.bastion.launcher.core.ServerEntry;
import online.bastion.launcher.data.LauncherPrefs;

/**
 * Запуск игры: проверка кеша → запись SAMP/settings.ini → старт Activity игры.
 *
 * Папка игры — это getExternalFilesDir() приложения:
 *   Android/data/&lt;пакет&gt;/files/
 * Туда же GTA SA (libGTASA) кладёт свои файлы, и оттуда мод читает SAMP/settings.ini
 * (g_pszStorage + "SAMP/settings.ini"). Поэтому игра должна быть в том же APK, что и лаунчер,
 * либо в game_package указан пакет, который сам читает настройки из Intent.
 */
public final class GameLauncher {

    /** Путь к настройкам мода внутри папки игры. */
    public static final String SETTINGS_PATH = "SAMP/settings.ini";

    /** Дополнительно передаём параметры в Intent — мод может взять их и оттуда. */
    public static final String EXTRA_NAME = "online.bastion.extra.NAME";
    public static final String EXTRA_HOST = "online.bastion.extra.HOST";
    public static final String EXTRA_PORT = "online.bastion.extra.PORT";

    /**
     * settings.ini пишем и читаем как ISO-8859-1: так любые байты (например, комментарии
     * в cp1251) сохраняются без изменений, а наши значения — чистый ASCII.
     */
    private static final Charset FILE_CHARSET = Charset.forName("ISO-8859-1");
    private static final long MAX_SETTINGS_SIZE = 256 * 1024;

    /** Ошибка запуска с заголовком и текстом для игрока. */
    public static final class LaunchException extends Exception {
        private static final long serialVersionUID = 1L;
        public final String title;
        public LaunchException(String title, String message) {
            super(message);
            this.title = title;
        }
    }

    private final Context context;
    private final LauncherConfig config;

    public GameLauncher(Context context, LauncherConfig config) {
        this.context = context.getApplicationContext();
        this.config = config;
    }

    /** Папка игры или null, если общее хранилище сейчас недоступно. */
    public File storageDir() {
        return context.getExternalFilesDir(null);
    }

    /** Файлы кеша из require_file, которых нет (или они пустые). */
    public List<String> missingCacheFiles() {
        List<String> missing = new ArrayList<String>();
        File root = storageDir();
        for (String path : config.requiredFiles) {
            File f = root == null ? null : new File(root, path);
            if (f == null || !f.isFile() || f.length() == 0) missing.add(path);
        }
        return missing;
    }

    /** Intent игры или null, если Activity игры нет в сборке / на устройстве. */
    public Intent gameIntent() {
        if (config.gameActivity.length() == 0) return null;
        String pkg = config.gamePackage.length() == 0 ? context.getPackageName() : config.gamePackage;
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClassName(pkg, config.gameActivity);
        return intent.resolveActivity(context.getPackageManager()) == null ? null : intent;
    }

    /** Полная проверка и запуск. Вызывать из главного потока. */
    public void launch(Activity activity, String nickname, ServerEntry server, ServerAddress address,
                       LauncherPrefs prefs) throws LaunchException {
        File root = storageDir();
        if (root == null) {
            throw new LaunchException("Нет доступа к памяти",
                    "Android сейчас не даёт доступ к папке игры. Проверь, что память телефона не переполнена, и попробуй ещё раз.");
        }

        List<String> missing = missingCacheFiles();
        if (!missing.isEmpty()) {
            throw new LaunchException("Кеш игры не найден",
                    "Не хватает файлов:\n" + join(missing, 6) + "\n\nПапка игры:\n" + root.getAbsolutePath());
        }

        Intent intent = gameIntent();
        if (intent == null) {
            throw new LaunchException("Игра не найдена",
                    "В сборке нет Activity игры «" + config.gameActivity + "». Проверь game_activity в launcher.ini "
                            + "и что эта Activity объявлена в AndroidManifest.xml.");
        }

        writeSettings(new File(root, SETTINGS_PATH), nickname, server, address, prefs);

        intent.putExtra(EXTRA_NAME, nickname);
        intent.putExtra(EXTRA_HOST, address.host);
        intent.putExtra(EXTRA_PORT, address.port);
        try {
            activity.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            throw new LaunchException("Игра не найдена", "Не удалось открыть " + config.gameActivity + ".");
        } catch (SecurityException e) {
            throw new LaunchException("Запуск запрещён",
                    "Activity игры не экспортирована для запуска из лаунчера: " + config.gameActivity + ".");
        }
    }

    /** Обновляет SAMP/settings.ini, сохраняя ключи, которые лаунчер не трогает. */
    void writeSettings(File file, String nickname, ServerEntry server, ServerAddress address,
                       LauncherPrefs prefs) throws LaunchException {
        SampSettingsIni ini = new SampSettingsIni()
                .set("client", "name", nickname)
                .set("client", "host", address.host)
                .set("client", "port", String.valueOf(address.port))
                .set("client", "password", server.password)
                .set("debug", "online", "true")
                // Если debug не задан, мод по умолчанию включает отладочное окно — выключаем.
                .setIfMissing("debug", "debug", "false")
                .set("gui", "ChatMaxMessages", String.valueOf(prefs.chatLines()))
                .set("gui", "FontSize", String.valueOf(prefs.fontSize()))
                // Мод пока не читает эту секцию; ключ для будущего ограничителя FPS.
                .set("launcher", "fps_limit", String.valueOf(prefs.fpsLimit()));

        try {
            File dir = file.getParentFile();
            if (dir != null && !dir.isDirectory() && !dir.mkdirs())
                throw new IOException("не удалось создать " + dir);
            String existing = file.isFile() ? readSmallFile(file) : null;
            writeAtomically(file, ini.apply(existing).getBytes(FILE_CHARSET));
        } catch (IOException e) {
            throw new LaunchException("Не удалось сохранить настройки", SETTINGS_PATH + ": " + e.getMessage());
        }
    }

    private static String readSmallFile(File f) throws IOException {
        if (f.length() > MAX_SETTINGS_SIZE) throw new IOException("файл слишком большой");
        InputStream in = new FileInputStream(f);
        try {
            ByteArrayOutputStream out = new ByteArrayOutputStream((int) f.length());
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) != -1) out.write(buf, 0, n);
            return new String(out.toByteArray(), FILE_CHARSET);
        } finally {
            in.close();
        }
    }

    /** Пишем во временный файл и переименовываем: при сбое старый файл остаётся целым. */
    private static void writeAtomically(File target, byte[] data) throws IOException {
        File tmp = new File(target.getPath() + ".tmp");
        FileOutputStream out = new FileOutputStream(tmp);
        try {
            out.write(data);
            out.getFD().sync();
        } finally {
            out.close();
        }
        if (!tmp.renameTo(target)) {
            tmp.delete();
            throw new IOException("не удалось заменить " + target.getName());
        }
    }

    private static String join(List<String> items, int max) {
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < items.size() && i < max; i++) b.append("• ").append(items.get(i)).append('\n');
        if (items.size() > max) b.append("… и ещё ").append(items.size() - max);
        return b.toString().trim();
    }
}
