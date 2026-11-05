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

/**
 * Downloads Bastion client assets described by mobile/installer/manifest.json.
 * Files are stored inside the launcher's private app directory.
 */
public final class ClientInstaller {
    private static final String MANIFEST_URL =
            "https://raw.githubusercontent.com/the-gta-man/Bastion/main/mobile/installer/manifest.json";
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

    public ClientInstaller(Context context) {
        this.context = context.getApplicationContext();
    }

    public void install(final Callback callback) {
        new Thread(new Runnable() {
            @Override public void run() {
                try {
                    report(callback, "Загрузка списка файлов…");
                    JSONObject manifest = new JSONObject(new String(
                            readUrl(MANIFEST_URL, 1024 * 1024), "UTF-8"));
                    JSONArray files = manifest.optJSONArray("files");
                    if (files == null || files.length() == 0) {
                        throw new IllegalStateException(
                                "Список файлов пуст. Сначала добавьте клиентские ресурсы в manifest.json.");
                    }

                    File root = new File(context.getExternalFilesDir(null), "client");
                    if (!root.exists() && !root.mkdirs()) {
                        throw new IllegalStateException("Не удалось создать папку клиента.");
                    }

                    int installed = 0;
                    for (int i = 0; i < files.length(); i++) {
                        JSONObject item = files.getJSONObject(i);
                        String path = item.getString("path");
                        String url = item.getString("url");
                        String expectedHash = item.getString("sha256").toLowerCase();
                        long expectedSize = item.optLong("size", -1);

                        if (!path.matches("[A-Za-z0-9._/-]+") || path.startsWith("/")
                                || path.contains("..") || expectedHash.length() != 64) {
                            throw new IllegalArgumentException("Некорректная запись в manifest: " + path);
                        }
                        if (!url.startsWith("https://") || expectedSize < 0
                                || expectedSize > MAX_FILE_SIZE) {
                            throw new IllegalArgumentException("Небезопасный URL или размер файла: " + path);
                        }

                        report(callback, "Загрузка " + (i + 1) + "/" + files.length() + ": " + path);
                        byte[] data = readUrl(url, (int) Math.min(MAX_FILE_SIZE, expectedSize + 1));
                        if (data.length != expectedSize) {
                            throw new IllegalStateException("Размер файла не совпадает: " + path);
                        }
                        String actualHash = sha256(data);
                        if (!actualHash.equals(expectedHash)) {
                            throw new SecurityException("SHA-256 не совпадает: " + path);
                        }

                        File target = new File(root, path);
                        File parent = target.getParentFile();
                        if (parent != null && !parent.exists() && !parent.mkdirs()) {
                            throw new IllegalStateException("Не удалось создать папку: " + path);
                        }
                        File temp = new File(target.getAbsolutePath() + ".part");
                        FileOutputStream out = new FileOutputStream(temp);
                        try { out.write(data); out.getFD().sync(); }
                        finally { out.close(); }
                        if (target.exists() && !target.delete()) {
                            temp.delete();
                            throw new IllegalStateException("Не удалось заменить файл: " + path);
                        }
                        if (!temp.renameTo(target)) {
                            temp.delete();
                            throw new IllegalStateException("Не удалось сохранить файл: " + path);
                        }
                        installed++;
                    }
                    final int count = installed;
                    final File destination = root;
                    main.post(new Runnable() {
                        @Override public void run() { callback.onComplete(count, destination); }
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

    private static byte[] readUrl(String address, int limit) throws Exception {
        HttpURLConnection connection = (HttpURLConnection) new URL(address).openConnection();
        connection.setConnectTimeout(CONNECT_TIMEOUT);
        connection.setReadTimeout(READ_TIMEOUT);
        connection.setInstanceFollowRedirects(true);
        connection.setRequestProperty("User-Agent", "Bastion-Launcher/0.2");
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
                    if (total > limit) throw new IllegalStateException("Файл превышает допустимый размер.");
                    buffer.write(chunk, 0, n);
                }
                return buffer.toByteArray();
            } finally { in.close(); }
        } finally { connection.disconnect(); }
    }

    private static String sha256(byte[] data) throws Exception {
        byte[] hash = MessageDigest.getInstance("SHA-256").digest(data);
        StringBuilder result = new StringBuilder();
        for (byte b : hash) result.append(String.format("%02x", b & 0xff));
        return result.toString();
    }

    private void report(final Callback callback, final String message) {
        main.post(new Runnable() {
            @Override public void run() { callback.onProgress(message); }
        });
    }
}
