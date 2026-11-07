package online.bastion.launcher;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.util.HashSet;
import java.util.Set;

/** Downloads Bastion-owned assets listed in the public manifest. */
public final class ClientInstaller {
    private static final String MANIFEST_URL = "https://raw.githubusercontent.com/the-gta-man/Bastion/main/mobile/installer/manifest.json";
    private static final int CONNECT_TIMEOUT = 15000;
    private static final int READ_TIMEOUT = 30000;
    private static final long MAX_FILE_SIZE = 512L * 1024L * 1024L;

    public interface Callback {
        void onProgress(String message);
        void onComplete(int count, File directory);
        void onError(String message);
    }

    private final Context context;
    private final Handler main = new Handler(Looper.getMainLooper());

    public ClientInstaller(Context context) { this.context = context.getApplicationContext(); }

    public void install(final Callback callback) {
        new Thread(new Runnable() {
            @Override public void run() {
                try {
                    report(callback, "Загрузка списка файлов…");
                    JSONObject manifest = new JSONObject(new String(readUrl(MANIFEST_URL, 1024 * 1024), "UTF-8"));
                    if (manifest.optInt("version", -1) != 1)
                        throw new IllegalStateException("Неподдерживаемая версия manifest.json.");
                    JSONArray files = manifest.optJSONArray("files");
                    if (files == null) throw new IllegalStateException("В manifest.json отсутствует список files.");
                    if (files.length() == 0)
                        throw new IllegalStateException("В manifest.json пока нет файлов клиента. Загрузчик готов, но ресурсы ещё не опубликованы.");

                    Set<String> seenPaths = new HashSet<String>();
                    File external = context.getExternalFilesDir(null);
                    if (external == null) throw new IllegalStateException("Android не предоставил папку для файлов.");
                    final File root = new File(external, "client");
                    if (!root.exists() && !root.mkdirs())
                        throw new IllegalStateException("Не удалось создать папку клиента.");

                    int installed = 0;
                    for (int i = 0; i < files.length(); i++) {
                        JSONObject item = files.getJSONObject(i);
                        String path = item.getString("path");
                        if (!seenPaths.add(path))
                            throw new IllegalArgumentException("Повторяющийся путь в manifest: " + path);
                        String address = item.getString("url");
                        String expectedHash = item.getString("sha256").toLowerCase();
                        long expectedSize = item.getLong("size");
                        if (!path.matches("[A-Za-z0-9._/-]+") || path.startsWith("/") || path.contains("..")
                                || !expectedHash.matches("[0-9a-f]{64}"))
                            throw new IllegalArgumentException("Некорректная запись manifest: " + path);
                        if (!address.startsWith("https://") || expectedSize < 0 || expectedSize > MAX_FILE_SIZE)
                            throw new IllegalArgumentException("Недопустимый URL или размер: " + path);

                        File target = new File(root, path);
                        String canonicalRoot = root.getCanonicalPath() + File.separator;
                        if (!target.getCanonicalPath().startsWith(canonicalRoot))
                            throw new SecurityException("Недопустимый путь: " + path);
                        File parent = target.getParentFile();
                        if (parent != null && !parent.exists() && !parent.mkdirs())
                            throw new IllegalStateException("Не удалось создать папку: " + path);

                        report(callback, "Загрузка " + (i + 1) + "/" + files.length() + ": " + path);
                        File temp = new File(target.getAbsolutePath() + ".part");
                        downloadAndVerify(address, temp, expectedSize, expectedHash);
                        File backup = new File(target.getAbsolutePath() + ".bak");
                        if (backup.exists() && !backup.delete()) {
                            temp.delete();
                            throw new IllegalStateException("Не удалось очистить резервный файл: " + path);
                        }
                        boolean hadOldFile = target.exists();
                        if (hadOldFile && !target.renameTo(backup)) {
                            temp.delete();
                            throw new IllegalStateException("Не удалось подготовить замену файла: " + path);
                        }
                        if (!temp.renameTo(target)) {
                            if (hadOldFile) backup.renameTo(target);
                            temp.delete();
                            throw new IllegalStateException("Не удалось сохранить файл: " + path);
                        }
                        if (backup.exists()) backup.delete();
                        installed++;
                    }
                    final int count = installed;
                    main.post(new Runnable() {
                        @Override public void run() { callback.onComplete(count, root); }
                    });
                } catch (final Exception e) {
                    main.post(new Runnable() {
                        @Override public void run() {
                            callback.onError(e.getMessage() == null ? e.toString() : e.getMessage());
                        }
                    });
                }
            }
        }, "bastion-client-installer").start();
    }


    // ------------------------------------------------------------------
    //  Установка игры из ZIP-архива (логика рабочего BastionLauncher3)
    // ------------------------------------------------------------------

    /** Архив игры по умолчанию — тот же, что в рабочем установщике. */
    public static final String GAME_ARCHIVE_URL = "http://192.168.31.182:8000/game.zip";
    /** MD5 архива по умолчанию. Для своей ссылки проверка MD5 не выполняется. */
    public static final String GAME_ARCHIVE_MD5 = "57A0F9765B2C229FD814E11D36F5EF00";
    /** Из архива берётся только эта папка, её содержимое кладётся в files/game. */
    private static final String GAME_ARCHIVE_PREFIX = "Data file Lite (samp)/ru.unisamp_mobile.game/files/";
    private static final String INSTALL_MARKER = ".install_complete";
    private static final long MAX_ARCHIVE_SIZE = 2147483648L;     // 2 ГБ
    private static final long MAX_EXTRACTED_SIZE = 4294967296L;   // 4 ГБ

    /** Папка установленной игры: Android/data/online.bastion.launcher/files/game. */
    public static File gameDir(Context context) {
        File external = context.getExternalFilesDir(null);
        return external == null ? null : new File(external, "game");
    }

    /** Игра считается установленной, если установка дошла до конца и есть папка SAMP. */
    public static boolean isGameInstalled(Context context) {
        File dir = gameDir(context);
        return dir != null && new File(dir, INSTALL_MARKER).exists() && new File(dir, "SAMP").isDirectory();
    }

    /**
     * Корень игровых данных для встроенного клиента: папка, где лежит data/gta.dat
     * (ищется внутри files/game не глубже 3 уровней, без учёта регистра).
     * Если gta.dat не найден, возвращается сама папка game — клиент покажет, чего не хватает.
     */
    public static File findGameDataRoot(Context context) {
        File game = gameDir(context);
        if (game == null || !game.isDirectory()) return null;
        File found = findGtaDatRoot(game, 0);
        return found != null ? found : game;
    }

    private static File findGtaDatRoot(File dir, int depth) {
        File[] children = dir.listFiles();
        if (children == null) return null;
        for (File child : children) {
            if (child.isFile() && child.getName().equalsIgnoreCase("gta.dat")) return dir;
            if (child.isDirectory() && child.getName().equalsIgnoreCase("data")) {
                File[] data = child.listFiles();
                if (data != null) for (File f : data)
                    if (f.isFile() && f.getName().equalsIgnoreCase("gta.dat")) return dir;
            }
        }
        if (depth >= 3) return null;
        for (File child : children) {
            if (!child.isDirectory()) continue;
            File found = findGtaDatRoot(child, depth + 1);
            if (found != null) return found;
        }
        return null;
    }

    /**
     * Скачивает ZIP (http или https), сверяет MD5 (если он передан), распаковывает
     * папку GAME_ARCHIVE_PREFIX во временную папку и только после успеха
     * заменяет ею files/game. При ошибке старая установка остаётся.
     */
    public void installArchive(final String address, final String expectedMd5, final Callback callback) {
        new Thread(new Runnable() {
            @Override public void run() {
                File archive = null, part = null, staging = null;
                try {
                    String url = address == null ? "" : address.trim();
                    if (!url.startsWith("http://") && !url.startsWith("https://"))
                        throw new IllegalArgumentException("Нужна прямая ссылка на ZIP-архив (http:// или https://).");
                    File external = context.getExternalFilesDir(null);
                    if (external == null) throw new IllegalStateException("Android не предоставил папку для файлов.");
                    File downloads = new File(external, "downloads");
                    if (!downloads.exists() && !downloads.mkdirs())
                        throw new IllegalStateException("Не удалось создать папку загрузки.");
                    archive = new File(downloads, "bastion_cache.zip");
                    part = new File(downloads, "bastion_cache.zip.part");
                    staging = new File(external, "game-installing");
                    deleteTree(staging);
                    if (!staging.mkdirs()) throw new IllegalStateException("Не удалось подготовить установку.");

                    report(callback, "Подключение к серверу…");
                    downloadArchive(url, part, callback);
                    if (archive.exists() && !archive.delete())
                        throw new IllegalStateException("Не удалось заменить старый архив.");
                    if (!part.renameTo(archive)) throw new IllegalStateException("Не удалось сохранить архив.");

                    if (expectedMd5 != null && expectedMd5.trim().length() > 0) {
                        report(callback, "Проверка архива (MD5)…");
                        String actual = fileDigest(archive, "MD5");
                        if (!actual.equalsIgnoreCase(expectedMd5.trim()))
                            throw new SecurityException("MD5 не совпадает! Архив повреждён или это другая версия.");
                    }

                    report(callback, "Распаковка файлов игры…");
                    int count = extractZip(archive, staging, GAME_ARCHIVE_PREFIX, callback);
                    if (count == 0)
                        throw new IllegalStateException("В архиве нет папки «" + GAME_ARCHIVE_PREFIX + "».");
                    if (!new File(staging, INSTALL_MARKER).createNewFile())
                        throw new IllegalStateException("Не удалось отметить завершение установки.");

                    File root = new File(external, "game");
                    File backup = new File(external, "game-previous");
                    deleteTree(backup);
                    if (root.exists() && !root.renameTo(backup))
                        throw new IllegalStateException("Не удалось сохранить предыдущую установку.");
                    if (!staging.renameTo(root)) {
                        if (backup.exists()) backup.renameTo(root);
                        throw new IllegalStateException("Не удалось завершить установку файлов.");
                    }
                    deleteTree(backup);
                    archive.delete();
                    final int installed = count;
                    final File result = root;
                    main.post(new Runnable() {
                        @Override public void run() { callback.onComplete(installed, result); }
                    });
                } catch (final Exception e) {
                    if (staging != null) deleteTree(staging);
                    if (part != null) part.delete();
                    if (archive != null) archive.delete();
                    main.post(new Runnable() {
                        @Override public void run() { callback.onError(e.getMessage() == null ? e.toString() : e.getMessage()); }
                    });
                }
            }
        }, "bastion-archive-installer").start();
    }

    private void downloadArchive(String address, File target, Callback callback) throws Exception {
        HttpURLConnection connection = (HttpURLConnection) new URL(address).openConnection();
        connection.setConnectTimeout(CONNECT_TIMEOUT);
        connection.setReadTimeout(READ_TIMEOUT);
        connection.setInstanceFollowRedirects(true);
        connection.setRequestProperty("User-Agent", "Bastion-Launcher/0.4");
        try {
            int code = connection.getResponseCode();
            if (code < 200 || code >= 300) throw new IllegalStateException("Ошибка загрузки архива: HTTP " + code);
            long expected = connection.getContentLength();
            if (expected > MAX_ARCHIVE_SIZE) throw new IllegalStateException("Архив превышает лимит 2 ГБ.");
            InputStream in = new BufferedInputStream(connection.getInputStream());
            FileOutputStream out = new FileOutputStream(target);
            try {
                byte[] buffer = new byte[32768];
                long total = 0;
                int lastPercent = -1;
                int n;
                while ((n = in.read(buffer)) != -1) {
                    total += n;
                    if (total > MAX_ARCHIVE_SIZE) throw new IllegalStateException("Архив превышает лимит 2 ГБ.");
                    out.write(buffer, 0, n);
                    if (expected > 0) {
                        int percent = (int) Math.min(100, total * 100 / expected);
                        if (percent != lastPercent) {
                            lastPercent = percent;
                            report(callback, "Скачивание файлов: " + percent + "%  (" + (total / 1048576) + " / " + (expected / 1048576) + " МБ)");
                        }
                    } else if (total / 1048576 != (total - n) / 1048576) {
                        report(callback, "Скачано: " + (total / 1048576) + " МБ");
                    }
                }
                out.getFD().sync();
            } finally { try { in.close(); } finally { out.close(); } }
            if (target.length() == 0) throw new IllegalStateException("Сервер вернул пустой архив.");
        } catch (Exception e) {
            target.delete();
            throw e;
        } finally { connection.disconnect(); }
    }

    /** Распаковывает только записи внутри prefix (сам prefix отрезается). Возвращает число файлов. */
    private int extractZip(File archive, File destination, String prefix, Callback callback) throws Exception {
        java.util.zip.ZipInputStream zip = new java.util.zip.ZipInputStream(
                new BufferedInputStream(new java.io.FileInputStream(archive)));
        int count = 0;
        long total = 0;
        try {
            java.util.zip.ZipEntry entry;
            byte[] buffer = new byte[32768];
            String base = destination.getCanonicalPath() + File.separator;
            while ((entry = zip.getNextEntry()) != null) {
                String name = entry.getName();
                if (name == null || !name.startsWith(prefix)) { zip.closeEntry(); continue; }
                String relative = name.substring(prefix.length());
                if (relative.length() == 0) { zip.closeEntry(); continue; }
                if (relative.startsWith("/") || relative.indexOf('\\') >= 0 || relative.matches("^[A-Za-z]:.*"))
                    throw new SecurityException("Недопустимый путь в архиве: " + name);
                File outFile = new File(destination, relative);
                if (!outFile.getCanonicalPath().startsWith(base))
                    throw new SecurityException("Архив пытается записать файл вне папки игры: " + name);
                if (entry.isDirectory()) {
                    if (!outFile.exists() && !outFile.mkdirs()) throw new IllegalStateException("Не удалось создать папку: " + relative);
                } else {
                    File parent = outFile.getParentFile();
                    if (parent != null && !parent.exists() && !parent.mkdirs()) throw new IllegalStateException("Не удалось создать папку: " + relative);
                    FileOutputStream out = new FileOutputStream(outFile);
                    try {
                        int n;
                        while ((n = zip.read(buffer)) != -1) {
                            total += n;
                            if (total > MAX_EXTRACTED_SIZE) throw new IllegalStateException("Распакованные файлы превышают лимит 4 ГБ.");
                            out.write(buffer, 0, n);
                        }
                    } finally { out.close(); }
                    count++;
                    if (count % 50 == 0) report(callback, "Распаковка файлов игры… " + count);
                }
                zip.closeEntry();
            }
        } finally { zip.close(); }
        return count;
    }

    private static String fileDigest(File file, String algorithm) throws Exception {
        MessageDigest digest = MessageDigest.getInstance(algorithm);
        InputStream in = new BufferedInputStream(new java.io.FileInputStream(file));
        try {
            byte[] buffer = new byte[32768];
            int n;
            while ((n = in.read(buffer)) != -1) digest.update(buffer, 0, n);
        } finally { in.close(); }
        return hex(digest.digest());
    }

    private static void deleteTree(File file) {
        if (file == null || !file.exists()) return;
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) for (File child : children) deleteTree(child);
        }
        file.delete();
    }

    private static void downloadAndVerify(String address, File target, long expectedSize, String expectedHash) throws Exception {
        HttpURLConnection connection = (HttpURLConnection) new URL(address).openConnection();
        connection.setConnectTimeout(CONNECT_TIMEOUT);
        connection.setReadTimeout(READ_TIMEOUT);
        connection.setInstanceFollowRedirects(true);
        connection.setRequestProperty("User-Agent", "Bastion-Launcher/0.3");
        try {
            int code = connection.getResponseCode();
            if (code < 200 || code >= 300) throw new IllegalStateException("HTTP " + code);
            InputStream in = new BufferedInputStream(connection.getInputStream());
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            FileOutputStream out = new FileOutputStream(target);
            long total = 0;
            try {
                byte[] buffer = new byte[16384];
                int n;
                while ((n = in.read(buffer)) != -1) {
                    total += n;
                    if (total > expectedSize || total > MAX_FILE_SIZE)
                        throw new IllegalStateException("Размер файла больше указанного: " + target.getName());
                    digest.update(buffer, 0, n);
                    out.write(buffer, 0, n);
                }
                out.getFD().sync();
            } finally {
                try { in.close(); } finally { out.close(); }
            }
            if (total != expectedSize) throw new IllegalStateException("Размер файла не совпадает: " + target.getName());
            if (!hex(digest.digest()).equals(expectedHash)) throw new SecurityException("SHA-256 не совпадает: " + target.getName());
        } catch (Exception e) {
            target.delete();
            throw e;
        } finally {
            connection.disconnect();
        }
    }

    private static byte[] readUrl(String address, int limit) throws Exception {
        HttpURLConnection connection = (HttpURLConnection) new URL(address).openConnection();
        connection.setConnectTimeout(CONNECT_TIMEOUT);
        connection.setReadTimeout(READ_TIMEOUT);
        connection.setInstanceFollowRedirects(true);
        connection.setRequestProperty("User-Agent", "Bastion-Launcher/0.3");
        try {
            int code = connection.getResponseCode();
            if (code < 200 || code >= 300) throw new IllegalStateException("HTTP " + code);
            InputStream in = new BufferedInputStream(connection.getInputStream());
            try {
                java.io.ByteArrayOutputStream buffer = new java.io.ByteArrayOutputStream();
                byte[] chunk = new byte[8192];
                int total = 0, n;
                while ((n = in.read(chunk)) != -1) {
                    total += n;
                    if (total > limit) throw new IllegalStateException("Манифест слишком большой.");
                    buffer.write(chunk, 0, n);
                }
                return buffer.toByteArray();
            } finally { in.close(); }
        } finally { connection.disconnect(); }
    }

    private static String hex(byte[] bytes) {
        StringBuilder result = new StringBuilder();
        for (byte b : bytes) result.append(String.format("%02x", b & 0xff));
        return result.toString();
    }

    private void report(final Callback callback, final String message) {
        main.post(new Runnable() {
            @Override public void run() { callback.onProgress(message); }
        });
    }
}