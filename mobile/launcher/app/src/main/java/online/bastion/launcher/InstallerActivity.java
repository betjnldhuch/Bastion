package online.bastion.launcher;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

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
 * Downloads Bastion-owned client assets listed in the remote manifest.
 * It deliberately does not download or modify proprietary GTA:SA game files.
 */
public final class InstallerActivity extends Activity {
    private static final String MANIFEST =
            "https://raw.githubusercontent.com/the-gta-man/Bastion/main/mobile/installer/manifest.json";
    private final Handler main = new Handler(Looper.getMainLooper());
    private TextView status;
    private ProgressBar progress;
    private Button install;
    private Button launchClient;
    private volatile boolean running;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setGravity(Gravity.CENTER);
        int pad = (int)(24 * getResources().getDisplayMetrics().density);
        page.setPadding(pad, pad, pad, pad);
        page.setBackgroundColor(0xff101216);

        TextView title = new TextView(this);
        title.setText("BASTION · УСТАНОВКА");
        title.setTextColor(0xffffa52f);
        title.setTextSize(22);
        page.addView(title);

        status = new TextView(this);
        status.setText("Готов к установке файлов Bastion");
        status.setTextColor(0xffeeeeee);
        status.setPadding(0, pad, 0, pad);
        page.addView(status);

        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(100);
        page.addView(progress, new LinearLayout.LayoutParams(-1, -2));

        install = new Button(this);
        install.setText("УСТАНОВИТЬ / ОБНОВИТЬ");
        install.setOnClickListener(v -> startInstall());
        LinearLayout.LayoutParams bp = new LinearLayout.LayoutParams(-1, -2);
        bp.topMargin = pad;
        page.addView(install, bp);

        launchClient = new Button(this);
        launchClient.setText("ЗАПУСТИТЬ ИГРУ");
        launchClient.setOnClickListener(v -> openGameClient());
        LinearLayout.LayoutParams launchLp = new LinearLayout.LayoutParams(-1, -2);
        launchLp.topMargin = pad / 2;
        page.addView(launchClient, launchLp);

        TextView hint = new TextView(this);
        hint.setText("Игровой модуль Bastion запускается в этом же приложении. Выбранные ник и сервер будут переданы в игровой экран.");
        hint.setTextColor(0xffaeb4bf);
        hint.setTextSize(12);
        hint.setPadding(0, pad / 2, 0, 0);
        page.addView(hint);
        setContentView(page);
    }

    private void openGameClient() {
        try {
            // The game client is compiled into this APK. Persist Bastion's
            // current profile into the embedded client's own stores before
            // opening its Activity inside the same application.
            Config config = new Config(this);
            if (config.servers.isEmpty()) throw new IllegalStateException("Список серверов пуст");
            if (config.selected < 0 || config.selected >= config.servers.size()) config.selected = 0;
            Config.Server server = config.servers.get(config.selected);
            String nickname = config.nickname == null ? "" : config.nickname.trim();
            if (!nickname.matches("[ -~]{3,24}")) {
                throw new IllegalStateException("Сначала укажи корректный ник в лаунчере Bastion");
            }
            if (server.host == null || !server.host.trim().matches("[A-Za-z0-9.-]{1,253}") || server.port < 1 || server.port > 65535) {
                throw new IllegalStateException("Проверь адрес и порт выбранного сервера");
            }

            getSharedPreferences("player_profile", MODE_PRIVATE).edit()
                    .putString("nickname", nickname)
                    .apply();
            getSharedPreferences("server_favorites", MODE_PRIVATE).edit()
                    .putString("primary_host", server.host.trim())
                    .putInt("primary_port", server.port)
                    .putString("primary_label", server.name == null || server.name.trim().isEmpty() ? "Bastion" : server.name.trim())
                    .apply();
            getSharedPreferences("bastion_bridge", MODE_PRIVATE).edit()
                    .putBoolean("open_server_on_launch", true)
                    .apply();

            Intent launch = new Intent();
            launch.setClassName(getPackageName(), "com.xykal.sampdroid.ui.MainActivity");
            startActivity(launch);
        } catch (Exception e) {
            new android.app.AlertDialog.Builder(this)
                    .setTitle("Не удалось запустить игру")
                    .setMessage(e.getMessage() == null ? "Проверь настройки Bastion." : e.getMessage())
                    .setPositiveButton("Понятно", null)
                    .show();
        }
    }

    private void startInstall() {
        if (running) return;
        running = true;
        install.setEnabled(false);
        status.setText("Проверяю список файлов…");
        new Thread(() -> {
            try {
                JSONObject manifest = getJson(MANIFEST);
                JSONArray files = manifest.optJSONArray("files");
                if (files == null || files.length() == 0) {
                    finishOnUi("Манифест пока пуст. Добавь файлы мода в mobile/installer/manifest.json.");
                    return;
                }
                File root = new File(getFilesDir(), "bastion-mod");
                if (!root.exists() && !root.mkdirs()) throw new Exception("Не удалось создать папку мода");
                for (int i = 0; i < files.length(); i++) {
                    JSONObject item = files.getJSONObject(i);
                    String relative = item.getString("path");
                    String url = item.getString("url");
                    String expected = item.getString("sha256").toLowerCase();
                    File target = safeTarget(root, relative);
                    File parent = target.getParentFile();
                    if (!parent.exists() && !parent.mkdirs()) throw new Exception("Не удалось создать папку " + relative);
                    if (target.isFile() && expected.equals(sha256(target))) {
                        updateUi("Уже установлено: " + relative, (i + 1) * 100 / files.length());
                        continue;
                    }
                    File temp = new File(target.getAbsolutePath() + ".download");
                    download(url, temp);
                    if (!expected.equals(sha256(temp))) {
                        temp.delete();
                        throw new Exception("Контрольная сумма не совпала: " + relative);
                    }
                    if (target.exists() && !target.delete()) throw new Exception("Не удалось заменить " + relative);
                    if (!temp.renameTo(target)) throw new Exception("Не удалось сохранить " + relative);
                    updateUi("Установлено: " + relative, (i + 1) * 100 / files.length());
                }
                finishOnUi("Файлы Bastion установлены. Папка: Android/data/online.bastion.launcher/files/bastion-mod");
            } catch (Exception e) {
                finishOnUi("Ошибка установки: " + e.getMessage());
            }
        }, "bastion-installer").start();
    }

    private JSONObject getJson(String address) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(address).openConnection();
        c.setConnectTimeout(15000);
        c.setReadTimeout(20000);
        c.setInstanceFollowRedirects(true);
        try (InputStream in = new BufferedInputStream(c.getInputStream())) {
            byte[] data = readAll(in);
            return new JSONObject(new String(data, "UTF-8"));
        } finally { c.disconnect(); }
    }

    private void download(String address, File output) throws Exception {
        if (!address.startsWith("https://")) throw new Exception("Разрешены только HTTPS-ссылки");
        HttpURLConnection c = (HttpURLConnection) new URL(address).openConnection();
        c.setConnectTimeout(15000);
        c.setReadTimeout(30000);
        c.setInstanceFollowRedirects(true);
        try (InputStream in = new BufferedInputStream(c.getInputStream());
             FileOutputStream out = new FileOutputStream(output)) {
            byte[] buffer = new byte[32768];
            int n;
            while ((n = in.read(buffer)) != -1) out.write(buffer, 0, n);
            out.getFD().sync();
        } finally { c.disconnect(); }
    }

    private byte[] readAll(InputStream in) throws Exception {
        java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
        byte[] b = new byte[8192]; int n;
        while ((n = in.read(b)) != -1) out.write(b, 0, n);
        return out.toByteArray();
    }

    private File safeTarget(File root, String relative) throws Exception {
        if (relative.length() == 0 || relative.startsWith("/") || relative.contains("\\") ||
                relative.contains("..") || relative.contains(":"))
            throw new Exception("Недопустимый путь в манифесте: " + relative);
        File target = new File(root, relative).getCanonicalFile();
        if (!target.getPath().startsWith(root.getCanonicalPath() + File.separator))
            throw new Exception("Путь выходит за папку мода");
        return target;
    }

    private String sha256(File file) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try (InputStream in = new BufferedInputStream(new java.io.FileInputStream(file))) {
            byte[] b = new byte[32768]; int n;
            while ((n = in.read(b)) != -1) digest.update(b, 0, n);
        }
        StringBuilder result = new StringBuilder();
        for (byte x : digest.digest()) result.append(String.format("%02x", x & 255));
        return result.toString();
    }

    private void updateUi(String text, int value) {
        main.post(() -> { status.setText(text); progress.setProgress(value); });
    }

    private void finishOnUi(String message) {
        main.post(() -> {
            running = false;
            status.setText(message);
            install.setEnabled(true);
        });
    }
}
