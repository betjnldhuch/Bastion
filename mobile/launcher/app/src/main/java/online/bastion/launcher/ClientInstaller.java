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


    /** Download a user-supplied HTTPS ZIP and safely extract it into the app's client folder. */
    public void installArchive(final String address, final Callback callback) {
        new Thread(new Runnable() {
            @Override public void run() {
                File archive = null, staging = null;
                try {
                    if (address == null || !address.trim().startsWith("https://"))
                        throw new IllegalArgumentException("Нужна прямая HTTPS-ссылка на ZIP-архив клиента.");
                    File external = context.getExternalFilesDir(null);
                    if (external == null) throw new IllegalStateException("Android не предоставил папку для файлов.");
                    if (!external.exists() && !external.mkdirs()) throw new IllegalStateException("Не удалось создать папку загрузки.");
                    archive = new File(external, "bastion-client-download.zip");
                    staging = new File(external, "client-installing");
                    deleteTree(staging);
                    if (!staging.mkdirs()) throw new IllegalStateException("Не удалось подготовить установку.");
                    report(callback, "Скачивание архива клиента…");
                    downloadArchive(address.trim(), archive, callback);
                    report(callback, "Распаковка файлов клиента…");
                    int count = extractZip(archive, staging);
                    if (count == 0) throw new IllegalStateException("Архив пустой — файлов для установки нет.");
                    File root = new File(external, "client");
                    File backup = new File(external, "client-previous");
                    deleteTree(backup);
                    if (root.exists() && !root.renameTo(backup)) throw new IllegalStateException("Не удалось сохранить предыдущую установку.");
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
            long expected = (long) connection.getContentLength();
            if (expected > 2147483648L) throw new IllegalStateException("Архив превышает лимит 2 ГБ.");
            InputStream in = new BufferedInputStream(connection.getInputStream());
            FileOutputStream out = new FileOutputStream(target);
            try {
                byte[] buffer = new byte[32768];
                long total = 0;
                int n;
                while ((n = in.read(buffer)) != -1) {
                    total += n;
                    if (total > 2147483648L) throw new IllegalStateException("Архив превышает лимит 2 ГБ.");
                    out.write(buffer, 0, n);
                    if (expected > 0) {
                        final int percent = (int) Math.min(99, total * 100 / expected);
                        report(callback, "Скачивание архива клиента… " + percent + "%");
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

    private static int extractZip(File archive, File destination) throws Exception {
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
                if (name == null || name.length() == 0 || name.startsWith("/")
                        || name.indexOf('\\') >= 0 || name.matches("^[A-Za-z]:.*"))
                    throw new SecurityException("Недопустимый путь в архиве: " + name);
                File outFile = new File(destination, name);
                String canonical = outFile.getCanonicalPath();
                if (!canonical.startsWith(base) && !canonical.equals(destination.getCanonicalPath()))
                    throw new SecurityException("Архив пытается записать файл вне папки клиента: " + name);
                if (entry.isDirectory()) {
                    if (!outFile.exists() && !outFile.mkdirs()) throw new IllegalStateException("Не удалось создать папку: " + name);
                } else {
                    File parent = outFile.getParentFile();
                    if (parent != null && !parent.exists() && !parent.mkdirs()) throw new IllegalStateException("Не удалось создать папку: " + name);
                    FileOutputStream out = new FileOutputStream(outFile);
                    try {
                        int n;
                        while ((n = zip.read(buffer)) != -1) {
                            total += n;
                            if (total > 4294967296L) throw new IllegalStateException("Распакованные файлы превышают лимит 4 ГБ.");
                            out.write(buffer, 0, n);
                        }
                    } finally { out.close(); }
                    count++;
                }
                zip.closeEntry();
            }
        } finally { zip.close(); }
        return count;
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