package online.bastion.launcher.core;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.Reader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * Настройки сборки из assets/launcher.ini. Игрок их не меняет — их меняет разработчик
 * перед сборкой APK. Формат: «ключ = значение», строки с ; — комментарии.
 */
public final class LauncherConfig {

    public final String title;
    public final String subtitle;
    public final boolean requireRpName;
    /** true — адреса серверов в Настройках только для чтения, берутся из launcher.ini. */
    public final boolean serversLocked;
    public final List<ServerEntry> servers;
    /** Как часто опрашивать сервера, секунды. */
    public final int pollIntervalSec;

    /** Пакет игры. Пусто — игра встроена в этот же APK. */
    public final String gamePackage;
    /** Activity игры (полное имя класса). */
    public final String gameActivity;
    /** Файлы кеша относительно папки игры, без которых запускать нельзя. */
    public final List<String> requiredFiles;

    /** Список ошибок разбора: показываем в диагностике, а не молча игнорируем. */
    public final List<String> warnings;

    private LauncherConfig(Builder b) {
        title = b.title;
        subtitle = b.subtitle;
        requireRpName = b.requireRpName;
        serversLocked = b.serversLocked;
        servers = Collections.unmodifiableList(new ArrayList<ServerEntry>(b.servers));
        pollIntervalSec = b.pollIntervalSec;
        gamePackage = b.gamePackage;
        gameActivity = b.gameActivity;
        requiredFiles = Collections.unmodifiableList(new ArrayList<String>(b.requiredFiles));
        warnings = Collections.unmodifiableList(new ArrayList<String>(b.warnings));
    }

    private static final class Builder {
        String title = "BASTION";
        String subtitle = "ONLINE";
        boolean requireRpName = true;
        boolean serversLocked = false;
        final List<ServerEntry> servers = new ArrayList<ServerEntry>();
        int pollIntervalSec = 10;
        String gamePackage = "";
        String gameActivity = "";
        final List<String> requiredFiles = new ArrayList<String>();
        final List<String> warnings = new ArrayList<String>();
    }

    public static LauncherConfig parse(Reader source) throws IOException {
        Builder b = new Builder();
        Set<String> ids = new HashSet<String>();
        BufferedReader r = new BufferedReader(source);
        String line;
        int no = 0;
        while ((line = r.readLine()) != null) {
            no++;
            if (no == 1 && line.length() > 0 && line.charAt(0) == '﻿') line = line.substring(1);
            line = line.trim();
            if (line.length() == 0 || line.startsWith(";") || line.startsWith("#")) continue;
            int eq = line.indexOf('=');
            if (eq < 0) { b.warnings.add("Строка " + no + ": нет знака ="); continue; }
            String key = line.substring(0, eq).trim().toLowerCase(Locale.ROOT);
            String val = line.substring(eq + 1).trim();

            if (key.equals("title")) b.title = val;
            else if (key.equals("subtitle")) b.subtitle = val;
            else if (key.equals("require_rp_name")) b.requireRpName = parseBool(val, true, no, b);
            else if (key.equals("lock_servers")) b.serversLocked = parseBool(val, false, no, b);
            else if (key.equals("poll_interval")) b.pollIntervalSec = parseInt(val, 10, 3, 600, no, b);
            else if (key.equals("game_package")) b.gamePackage = val;
            else if (key.equals("game_activity")) b.gameActivity = val;
            else if (key.equals("require_file")) addRequiredFile(val, no, b);
            else if (key.equals("server")) addServer(val, no, b, ids);
            else b.warnings.add("Строка " + no + ": неизвестный ключ «" + key + "»");
        }
        if (b.servers.isEmpty()) b.warnings.add("В launcher.ini нет ни одного server = …");
        if (b.gameActivity.length() == 0) b.warnings.add("В launcher.ini не указан game_activity");
        return new LauncherConfig(b);
    }

    /** server = Название | адрес | порт | описание | пароль (описание и пароль необязательны). */
    private static void addServer(String val, int no, Builder b, Set<String> ids) {
        String[] p = val.split("\\|", -1);
        if (p.length < 3) { b.warnings.add("Строка " + no + ": нужно «Название | адрес | порт»"); return; }
        String name = p[0].trim();
        if (name.length() == 0) { b.warnings.add("Строка " + no + ": пустое название сервера"); return; }
        ServerAddress address;
        try {
            address = ServerAddress.parse(p[1], p[2]);
        } catch (ServerAddress.InvalidException e) {
            b.warnings.add("Строка " + no + ": " + e.getMessage());
            return;
        }
        String description = p.length > 3 ? p[3].trim() : "";
        String password = p.length > 4 ? p[4].trim() : "";
        if (!SampSettingsIni.isSafeValue(password)) {
            b.warnings.add("Строка " + no + ": пароль содержит ; или перевод строки");
            return;
        }
        ServerEntry entry = new ServerEntry(name, description, address, password);
        if (!ids.add(entry.id)) { b.warnings.add("Строка " + no + ": сервер «" + name + "» уже есть"); return; }
        b.servers.add(entry);
    }

    private static void addRequiredFile(String val, int no, Builder b) {
        String path = val.replace('\\', '/');
        while (path.startsWith("/")) path = path.substring(1);
        if (path.length() == 0 || path.contains("..")) {
            b.warnings.add("Строка " + no + ": некорректный путь require_file");
            return;
        }
        b.requiredFiles.add(path);
    }

    private static boolean parseBool(String v, boolean def, int no, Builder b) {
        String s = v.toLowerCase(Locale.ROOT);
        if (s.equals("true") || s.equals("1") || s.equals("yes")) return true;
        if (s.equals("false") || s.equals("0") || s.equals("no")) return false;
        b.warnings.add("Строка " + no + ": ожидается true или false");
        return def;
    }

    private static int parseInt(String v, int def, int min, int max, int no, Builder b) {
        try {
            int x = Integer.parseInt(v);
            if (x >= min && x <= max) return x;
        } catch (NumberFormatException ignored) { }
        b.warnings.add("Строка " + no + ": ожидается число от " + min + " до " + max);
        return def;
    }
}
