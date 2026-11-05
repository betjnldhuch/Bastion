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