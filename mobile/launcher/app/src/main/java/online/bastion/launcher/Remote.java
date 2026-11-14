package online.bastion.launcher;

import android.content.Context;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Данные из сети: новости, список серверов и номер свежей версии.
 *
 * Лежат в папке launcher/ на сервере раздачи игры (cloud/download-worker):
 * news.txt, servers.ini, version.ini. Адрес выводится из адреса архива игры,
 * вшитого при сборке, поэтому отдельно его настраивать не нужно.
 * Скачанное сохраняется в памяти приложения: без сети лаунчер показывает
 * последнее скачанное, а если и его нет — файлы из assets.
 *
 * version.ini:
 *   android_version_code = 15
 *   android_url          = Bastion.apk     (имя в той же папке или полная https-ссылка)
 *   android_notes        = что нового, одной строкой
 */
public final class Remote {
    private static final int LIMIT = 256 * 1024;
    private static final int TIMEOUT = 6000;

    /** Что вернула проверка версии. */
    public static final class Update {
        public int versionCode;
        public String url = "";
        public String notes = "";
    }

    private Remote() { }

    /** Папка лаунчеров на сервере раздачи или пусто, если адрес архива не https. */
    public static String baseUrl() {
        // Первый https-источник, который раздаёт папку launcher/ (сервер раздачи).
        // Яндекс Диск и GitHub её не раздают — их пропускаем.
        for (String src : ClientInstaller.mirrors(ClientInstaller.GAME_ARCHIVE_URL)) {
            try {
                URL u = new URL(src);
                if (!"https".equals(u.getProtocol()) || ClientInstaller.isPublicHost(u.getHost())) continue;
                return "https://" + u.getAuthority() + "/launcher/";
            } catch (Exception ignored) { }
        }
        return "";
    }

    private static File cache(Context ctx, String name) {
        return new File(ctx.getFilesDir(), "remote-" + name);
    }

    /** Строки последнего скачанного файла или null, если его нет. */
    public static List<String> cached(Context ctx, String name) {
        File f = cache(ctx, name);
        if (!f.isFile()) return null;
        try {
            FileInputStream in = new FileInputStream(f);
            try {
                return lines(readAll(in));
            } finally {
                in.close();
            }
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Скачать news.txt и servers.ini. Вызывать не из главного потока.
     * true — что-то изменилось и экран стоит перерисовать.
     */
    public static boolean refresh(Context ctx) {
        String base = baseUrl();
        if (base.length() == 0) return false;
        boolean changed = false;
        changed |= fetch(ctx, base, "news.txt", false);
        changed |= fetch(ctx, base, "servers.ini", true);
        return changed;
    }

    private static boolean fetch(Context ctx, String base, String name, boolean servers) {
        try {
            byte[] data = read(base + name);
            if (!looksValid(lines(data), servers)) return false;
            File f = cache(ctx, name);
            if (f.isFile() && f.length() == data.length) {
                FileInputStream in = new FileInputStream(f);
                try {
                    if (Arrays.equals(readAll(in), data)) return false;
                } finally {
                    in.close();
                }
            }
            File tmp = new File(f.getPath() + ".tmp");
            FileOutputStream out = new FileOutputStream(tmp);
            try {
                out.write(data);
            } finally {
                out.close();
            }
            return tmp.renameTo(f);
        } catch (Exception e) {
            return false;
        }
    }

    /** Это наш файл, а не страница ошибки: есть хотя бы одна строка нужного вида. */
    private static boolean looksValid(List<String> lines, boolean servers) {
        for (String raw : lines) {
            String line = raw.trim();
            if (line.length() == 0 || line.startsWith(";") || line.startsWith("#")) continue;
            if (line.startsWith("<")) return false;
            if (servers) {
                int eq = line.indexOf('=');
                if (eq > 0 && line.substring(0, eq).trim().equalsIgnoreCase("server")
                        && line.substring(eq + 1).split("\\|").length >= 3) return true;
            } else if (line.split("\\|").length >= 3) {
                return true;
            }
        }
        return false;
    }

    /** Есть ли APK новее установленного. null — нет или узнать не удалось. */
    public static Update checkUpdate() {
        String base = baseUrl();
        if (base.length() == 0) return null;
        try {
            Update u = new Update();
            for (String raw : lines(read(base + "version.ini"))) {
                String line = raw.trim();
                if (line.length() == 0 || line.startsWith(";") || line.startsWith("#")) continue;
                int eq = line.indexOf('=');
                if (eq <= 0) continue;
                String key = line.substring(0, eq).trim().toLowerCase();
                String val = line.substring(eq + 1).trim();
                if (key.equals("android_version_code")) {
                    try { u.versionCode = Integer.parseInt(val); } catch (NumberFormatException ignored) { }
                } else if (key.equals("android_url")) {
                    u.url = val;
                } else if (key.equals("android_notes")) {
                    u.notes = val;
                }
            }
            if (u.versionCode <= BuildConfig.VERSION_CODE || u.url.length() == 0) return null;
            URL full = new URL(new URL(base), u.url);       // имя файла или полная ссылка
            if (!"https".equals(full.getProtocol())) return null;
            u.url = full.toString();
            return u;
        } catch (Exception e) {
            return null;
        }
    }

    private static byte[] read(String address) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(address).openConnection();
        c.setConnectTimeout(TIMEOUT);
        c.setReadTimeout(TIMEOUT);
        c.setInstanceFollowRedirects(true);
        c.setRequestProperty("User-Agent", "Bastion-Launcher-Android");
        try {
            int code = c.getResponseCode();
            if (code < 200 || code >= 300) throw new IllegalStateException("HTTP " + code);
            InputStream in = new BufferedInputStream(c.getInputStream());
            try {
                return readAll(in);
            } finally {
                in.close();
            }
        } finally {
            c.disconnect();
        }
    }

    private static byte[] readAll(InputStream in) throws Exception {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buf = new byte[8192];
        int n;
        while ((n = in.read(buf)) > 0) {
            out.write(buf, 0, n);
            if (out.size() > LIMIT) throw new IllegalStateException("file too large");
        }
        return out.toByteArray();
    }

    private static List<String> lines(byte[] data) throws Exception {
        String text = new String(data, "UTF-8");
        if (text.length() > 0 && text.charAt(0) == '﻿') text = text.substring(1);
        List<String> out = new ArrayList<String>();
        for (String l : text.replace("\r", "").split("\n")) out.add(l);
        return out;
    }
}
