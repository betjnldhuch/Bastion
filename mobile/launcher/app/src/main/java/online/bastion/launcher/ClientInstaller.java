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

    /**
     * Архив игры по умолчанию. Задаётся при сборке (секреты GitHub BASTION_GAME_URL,
     * BASTION_GAME_MD5), без них — домашний сервер 192.168.31.182.
     */
    public static final String GAME_ARCHIVE_URL = BuildConfig.GAME_ARCHIVE_URL;
    /** MD5 архива по умолчанию. Для своей ссылки проверка MD5 не выполняется. */
    public static final String GAME_ARCHIVE_MD5 = BuildConfig.GAME_ARCHIVE_MD5;
    /**
     * Ключ закрытой раздачи (cloud/download-worker), секрет BASTION_DOWNLOAD_KEY.
     * Отправляется только на https-сервер архива по умолчанию, не на чужие ссылки.
     */
    private static final String DOWNLOAD_KEY = BuildConfig.DOWNLOAD_KEY;
    /**
     * Ключ Backblaze B2 только на чтение (секреты BASTION_B2_KEY_ID, BASTION_B2_KEY).
     * Используется для ссылок вида b2://бакет/путь/к/файлу.zip.
     */
    private static final String B2_KEY_ID = BuildConfig.B2_KEY_ID;
    private static final String B2_KEY = BuildConfig.B2_KEY;
    /** Токен GitHub только на чтение для приватного релиза (секрет BASTION_GH_TOKEN). */
    private static final String GH_TOKEN = BuildConfig.GH_TOKEN;
    /**
     * Папка внутри архива, содержимое которой кладётся в files/game. Если её нет,
     * корень игры ищется автоматически: папка, в которой лежит SAMP/ (так устроен
     * кэш SAMP-Mobile GTA-2.11 — содержимое /storage/emulated/0/GTA/).
     */
    private static final String GAME_ARCHIVE_PREFIX = "Data file Lite (samp)/ru.unisamp_mobile.game/files/";
    /** Папка встроенного клиента SAMP-Mobile внутри корня игры. */
    public static final String SAMP_DIR = "SAMP";
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
        return dir != null && new File(dir, INSTALL_MARKER).exists() && findSampStorageRoot(context) != null;
    }

    /**
     * Корень хранилища для SAMP-Mobile — папка, где лежит SAMP/ (аналог
     * /storage/emulated/0/GTA/ в оригинальном клиенте). Ищется в files/game
     * не глубже 3 уровней. Возвращает null, если SAMP/ нет.
     */
    public static File findSampStorageRoot(Context context) {
        File game = gameDir(context);
        if (game == null || !game.isDirectory()) return null;
        return findSampRoot(game, 0);
    }

    private static File findSampRoot(File dir, int depth) {
        if (new File(dir, SAMP_DIR).isDirectory()) return dir;
        if (depth >= 3) return null;
        File[] children = dir.listFiles();
        if (children == null) return null;
        for (File child : children) {
            if (!child.isDirectory()) continue;
            File found = findSampRoot(child, depth + 1);
            if (found != null) return found;
        }
        return null;
    }

    /**
     * Записывает выбранный сервер, ник и настройки в SAMP/settings.ini клиента.
     * Остальные ключи файла (из кэша игры) сохраняются. Без этого файла
     * SAMP-Mobile завершается сразу после запуска.
     */
    public static File writeSampSettings(File storageRoot, String nick, String host, int port,
                                         int fpsLimit, int chatLines) throws java.io.IOException {
        File samp = new File(storageRoot, SAMP_DIR);
        if (!samp.isDirectory() && !samp.mkdirs()) throw new java.io.IOException("Нет папки " + samp);
        File ini = new File(samp, "settings.ini");
        java.util.List<String> lines = new java.util.ArrayList<String>();
        if (ini.exists()) {
            java.io.BufferedReader r = new java.io.BufferedReader(new java.io.InputStreamReader(
                    new java.io.FileInputStream(ini), "UTF-8"));
            try { String l; while ((l = r.readLine()) != null) lines.add(l); } finally { r.close(); }
        }
        setIni(lines, "client", "name", nick);
        setIni(lines, "client", "host", host);
        setIni(lines, "client", "port", String.valueOf(port));
        // "-bui1": клиент умеет HTML-окна (сервер смотрит GetPlayerVersion, ui.inc).
        setIni(lines, "client", "version", "0.3.7-bui1");
        setIni(lines, "gui", "FPSLimit", String.valueOf(fpsLimit));
        setIni(lines, "gui", "ChatMaxMessages", String.valueOf(chatLines));
        File tmp = new File(samp, "settings.ini.tmp");
        java.io.Writer w = new java.io.OutputStreamWriter(new FileOutputStream(tmp), "UTF-8");
        try { for (String l : lines) { w.write(l); w.write('\n'); } } finally { w.close(); }
        if (ini.exists() && !ini.delete()) throw new java.io.IOException("Не удалось обновить " + ini);
        if (!tmp.renameTo(ini)) throw new java.io.IOException("Не удалось сохранить " + ini);
        return ini;
    }

    /** Файлы, без которых SAMP-Mobile GTA-2.11 падает при старте (пути от корня игры). */
    static final String[] REQUIRED_FILES = {
            "SAMP/main.scm", "SAMP/script.img", "SAMP/gta.dat", "SAMP/peds.ide",
            "SAMP/vehicles.ide", "SAMP/handling.cfg", "SAMP/weapon.dat", "SAMP/fonts/arial_bold.ttf"
    };
    /** Шрифт интерфейса клиента: без него ImGui завершает процесс. */
    static final String SAMP_FONT = "SAMP/fonts/arial_bold.ttf";

    /**
     * Подготовка к запуску: если в кэше нет шрифта клиента, кладёт шрифт лаунчера
     * (Russo One, с кириллицей) под именем arial_bold.ttf. Возвращает список
     * недостающих обязательных файлов (пустой — можно запускать).
     */
    public static java.util.List<String> prepareForLaunch(Context ctx, File root) {
        File font = resolveIgnoreCase(root, SAMP_FONT);
        if (font == null) {
            try {
                File target = new File(root, SAMP_FONT);
                File dir = target.getParentFile();
                if (dir != null && !dir.isDirectory()) dir.mkdirs();
                InputStream in = ctx.getAssets().open("fonts/RussoOne-Regular.ttf");
                FileOutputStream out = new FileOutputStream(target);
                try {
                    byte[] buffer = new byte[16384];
                    int n;
                    while ((n = in.read(buffer)) != -1) out.write(buffer, 0, n);
                } finally { try { in.close(); } finally { out.close(); } }
            } catch (Exception ignored) { }
        }
        java.util.List<String> missing = new java.util.ArrayList<String>();
        for (String rel : REQUIRED_FILES) {
            File f = resolveIgnoreCase(root, rel);
            if (f == null || !f.isFile() || f.length() == 0) missing.add(rel);
        }
        File texdb = resolveIgnoreCase(root, "texdb");
        if (texdb == null || !texdb.isDirectory()) missing.add("texdb/");
        if (chooseGameLanguage(root) == null) missing.add("text/russian.gxt или text/american.gxt");
        return missing;
    }

    /** Языки движка GTA SA (libGame.so) и их файлы текстов. */
    static final String[][] GAME_LANGUAGES = {
            {"ru", "text/russian.gxt"}, {"en", "text/american.gxt"}, {"fr", "text/french.gxt"},
            {"de", "text/german.gxt"}, {"it", "text/italian.gxt"}, {"es", "text/spanish.gxt"},
            {"ja", "text/japanese.gxt"}
    };

    /**
     * Язык игры, для которого в установке есть .gxt: язык телефона, иначе русский,
     * иначе английский, иначе любой установленный. null — текстов игры нет вообще.
     * Передаётся клиенту как bastion.language (без этого движок открывает файл по
     * языку телефона и падает, если его нет в кэше).
     */
    public static String chooseGameLanguage(File root) {
        String device = java.util.Locale.getDefault().getLanguage();
        String[] order = {device, "ru", "en", "fr", "de", "it", "es", "ja"};
        for (String want : order)
            for (String[] lang : GAME_LANGUAGES)
                if (lang[0].equals(want)) {
                    File f = resolveIgnoreCase(root, lang[1]);
                    if (f != null && f.isFile() && f.length() > 0) return lang[0];
                }
        return null;
    }

    static String gxtFor(String language) {
        for (String[] lang : GAME_LANGUAGES) if (lang[0].equals(language)) return lang[1];
        return "text/american.gxt";
    }

    /** Находит файл по относительному пути без учёта регистра. */
    static File resolveIgnoreCase(File root, String rel) {
        File current = root;
        for (String part : rel.split("/")) {
            if (part.length() == 0) continue;
            File exact = new File(current, part);
            if (exact.exists()) { current = exact; continue; }
            String[] names = current.list();
            File found = null;
            if (names != null) for (String n : names) if (n.equalsIgnoreCase(part)) { found = new File(current, n); break; }
            if (found == null) return null;
            current = found;
        }
        return current;
    }

    /** Ставит key=value в секции [section] (без учёта регистра, как inih); создаёт при отсутствии. */
    static void setIni(java.util.List<String> lines, String section, String key, String value) {
        int sectionStart = -1, sectionEnd = lines.size();
        for (int i = 0; i < lines.size(); i++) {
            String t = lines.get(i).trim();
            if (t.startsWith("[") && t.endsWith("]")) {
                if (sectionStart >= 0) { sectionEnd = i; break; }
                if (t.substring(1, t.length() - 1).trim().equalsIgnoreCase(section)) sectionStart = i;
            }
        }
        if (sectionStart < 0) {
            if (!lines.isEmpty() && lines.get(lines.size() - 1).trim().length() > 0) lines.add("");
            lines.add("[" + section + "]");
            lines.add(key + " = " + value);
            return;
        }
        for (int i = sectionStart + 1; i < sectionEnd; i++) {
            String t = lines.get(i).trim();
            if (t.startsWith(";") || t.startsWith("#")) continue;
            int eq = t.indexOf('=');
            if (eq > 0 && t.substring(0, eq).trim().equalsIgnoreCase(key)) {
                lines.set(i, key + " = " + value);
                return;
            }
        }
        int insert = sectionEnd;
        while (insert > sectionStart + 1 && lines.get(insert - 1).trim().length() == 0) insert--;
        lines.add(insert, key + " = " + value);
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
                    boolean local = url.startsWith("content://") || url.startsWith("file://");
                    if (!local && !url.startsWith("http://") && !url.startsWith("https://") && !url.startsWith("b2://") && !url.startsWith("gh://"))
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

                    if (local) {
                        report(callback, "Копирование архива…");
                        copyLocalArchive(android.net.Uri.parse(url), part, callback);
                    } else {
                        downloadMirrored(url, part, callback);
                    }
                    if (archive.exists() && !archive.delete())
                        throw new IllegalStateException("Не удалось заменить старый архив.");
                    if (!part.renameTo(archive)) throw new IllegalStateException("Не удалось сохранить архив.");

                    if (expectedMd5 != null && expectedMd5.trim().length() > 0) {
                        report(callback, "Проверка архива (MD5)…");
                        String actual = fileDigest(archive, "MD5");
                        if (!actual.equalsIgnoreCase(expectedMd5.trim()))
                            throw new SecurityException("MD5 не совпадает! Архив повреждён или это другая версия.");
                    }

                    report(callback, "Проверка содержимого архива…");
                    String prefix = detectGamePrefix(archive);
                    report(callback, "Распаковка файлов игры…");
                    int count = extractZip(archive, staging, prefix, callback);
                    if (count == 0)
                        throw new IllegalStateException("В архиве нет файлов игры («" + prefix + "»).");
                    report(callback, "Проверка установки…");
                    String summary = verifyInstall(staging, count);
                    java.io.Writer marker = new java.io.OutputStreamWriter(
                            new FileOutputStream(new File(staging, INSTALL_MARKER)), "UTF-8");
                    try { marker.write(summary); } finally { marker.close(); }

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

    /** ZIP, выбранный на телефоне (например, скачанный в браузере кэш SAMP-Mobile). */
    private void copyLocalArchive(android.net.Uri uri, File target, Callback callback) throws Exception {
        InputStream in = context.getContentResolver().openInputStream(uri);
        if (in == null) throw new IllegalStateException("Не удалось открыть выбранный файл.");
        long expected = -1;
        try {
            android.database.Cursor c = context.getContentResolver().query(uri, null, null, null, null);
            if (c != null) {
                try {
                    int idx = c.getColumnIndex(android.provider.OpenableColumns.SIZE);
                    if (idx >= 0 && c.moveToFirst() && !c.isNull(idx)) expected = c.getLong(idx);
                } finally { c.close(); }
            }
        } catch (Exception ignored) { }
        FileOutputStream out = new FileOutputStream(target);
        try {
            byte[] buffer = new byte[65536];
            long total = 0;
            int n, last = -1;
            while ((n = in.read(buffer)) != -1) {
                total += n;
                if (total > MAX_ARCHIVE_SIZE) throw new IllegalStateException("Архив превышает лимит 2 ГБ.");
                out.write(buffer, 0, n);
                if (expected > 0) {
                    int percent = (int) Math.min(100, total * 100 / expected);
                    if (percent != last) { last = percent; report(callback, "Копирование архива: " + percent + "%"); }
                }
            }
            out.getFD().sync();
        } catch (Exception e) {
            target.delete();
            throw e;
        } finally { try { in.close(); } finally { out.close(); } }
        if (target.length() == 0) throw new IllegalStateException("Выбранный файл пустой.");
    }

    /** Источники архива: зеркала через «;» (те же файлы в разных местах). */
    static String[] mirrors(String address) {
        java.util.List<String> list = new java.util.ArrayList<String>();
        for (String m : address.split("[;\\s|]+")) {
            if (m.length() > 0) list.add(m);
        }
        return list.toArray(new String[0]);
    }

    /** Яндекс Диск и GitHub: туда ключ лаунчера не отправляется. */
    static boolean isPublicHost(String host) {
        String h = host.toLowerCase(java.util.Locale.ROOT);
        return h.equals("yadi.sk") || h.startsWith("disk.") || h.equals("github.com") || h.endsWith(".github.com")
                || h.endsWith(".githubusercontent.com");
    }

    /**
     * Скачивает архив с первого зеркала, которое его отдаст: лимит Яндекс Диска,
     * 404 или обрыв связи на одном — лаунчер пробует следующее.
     */
    private void downloadMirrored(String address, File target, Callback callback) throws Exception {
        String[] list = mirrors(address);
        if (list.length > 16) throw new IllegalArgumentException("Слишком много зеркал архива (больше 16).");
        for (int i = 0; i < list.length; i++) {
            String m = list[i];
            if (!m.startsWith("http://") && !m.startsWith("https://") && !m.startsWith("b2://") && !m.startsWith("gh://"))
                throw new IllegalArgumentException("Нужна прямая ссылка на ZIP-архив (http:// или https://).");
            report(callback, list.length == 1 ? "Подключение к серверу…"
                    : "Подключение к зеркалу " + (i + 1) + " из " + list.length + "…");
            try {
                downloadArchive(m, target, callback);
                return;
            } catch (Exception e) {
                if (i == list.length - 1) throw e;
                report(callback, "Зеркало " + (i + 1) + " не отдаёт файл, переключаюсь на следующее…");
                try { Thread.sleep(1500); } catch (InterruptedException ignored) { }
            }
        }
    }

    /** Ключ уходит только по https и только на хост архива по умолчанию (любого из зеркал). */
    private static boolean sendsDownloadKey(String address) {
        if (DOWNLOAD_KEY.length() == 0) return false;
        try {
            URL target = new URL(address);
            if (!"https".equals(target.getProtocol()) || isPublicHost(target.getHost())) return false;
            for (String src : mirrors(GAME_ARCHIVE_URL)) {
                try {
                    URL official = new URL(src);
                    if ("https".equals(official.getProtocol()) && target.getHost().equalsIgnoreCase(official.getHost()))
                        return true;
                } catch (Exception ignored) { }
            }
            return false;
        } catch (Exception e) {
            return false;
        }
    }

    /**
     * b2://бакет/путь → настоящая ссылка на файл в закрытом бакете Backblaze B2 и
     * временный токен (b2_authorize_account, ключ только на чтение). Возвращает
     * {url, token}.
     */
    private static String[] resolveB2(String address) throws Exception {
        if (B2_KEY_ID.length() == 0 || B2_KEY.length() == 0)
            throw new IllegalStateException("В этой сборке нет ключа для закрытого хранилища B2.");
        String rest = address.substring("b2://".length());
        int slash = rest.indexOf('/');
        if (slash <= 0 || slash == rest.length() - 1)
            throw new IllegalArgumentException("Ссылка B2 должна быть вида b2://бакет/файл.zip");
        String bucket = rest.substring(0, slash), file = rest.substring(slash + 1);

        HttpURLConnection auth = (HttpURLConnection)
                new URL("https://api.backblazeb2.com/b2api/v2/b2_authorize_account").openConnection();
        auth.setConnectTimeout(CONNECT_TIMEOUT);
        auth.setReadTimeout(READ_TIMEOUT);
        String basic = android.util.Base64.encodeToString(
                (B2_KEY_ID + ":" + B2_KEY).getBytes("UTF-8"), android.util.Base64.NO_WRAP);
        auth.setRequestProperty("Authorization", "Basic " + basic);
        try {
            int code = auth.getResponseCode();
            if (code == 401) throw new IllegalStateException("Хранилище B2 не приняло ключ лаунчера (HTTP 401): ключ удалён или устарел.");
            if (code != 200) throw new IllegalStateException("Хранилище B2 недоступно: HTTP " + code);
            java.io.ByteArrayOutputStream buf = new java.io.ByteArrayOutputStream();
            InputStream in = auth.getInputStream();
            try {
                byte[] b = new byte[8192];
                int n;
                while ((n = in.read(b)) != -1) { buf.write(b, 0, n); if (buf.size() > 262144) break; }
            } finally { in.close(); }
            JSONObject json = new JSONObject(buf.toString("UTF-8"));
            StringBuilder path = new StringBuilder();
            for (String part : file.split("/")) {
                if (path.length() > 0) path.append('/');
                path.append(java.net.URLEncoder.encode(part, "UTF-8").replace("+", "%20"));
            }
            String url = json.getString("downloadUrl") + "/file/"
                    + java.net.URLEncoder.encode(bucket, "UTF-8") + "/" + path;
            return new String[] { url, json.getString("authorizationToken") };
        } finally { auth.disconnect(); }
    }

    /**
     * gh://владелец/репо/тег/файл → временная прямая ссылка на файл релиза в
     * приватном репозитории GitHub. Токен уходит только на api.github.com.
     */
    private static String resolveGitHub(String address) throws Exception {
        if (GH_TOKEN.length() == 0)
            throw new IllegalStateException("В этой сборке нет токена для приватного релиза GitHub.");
        String[] p = address.substring("gh://".length()).split("/");
        if (p.length != 4 || p[0].isEmpty() || p[1].isEmpty() || p[2].isEmpty() || p[3].isEmpty())
            throw new IllegalArgumentException("Ссылка GitHub должна быть вида gh://владелец/репо/тег/файл");
        String api = "https://api.github.com/repos/" + java.net.URLEncoder.encode(p[0], "UTF-8") + "/"
                + java.net.URLEncoder.encode(p[1], "UTF-8");
        HttpURLConnection c = gitHub(api + "/releases/tags/" + java.net.URLEncoder.encode(p[2], "UTF-8"),
                "application/vnd.github+json");
        String assetUrl = null;
        try {
            int code = c.getResponseCode();
            checkGitHub(code, "релиз " + p[2]);
            InputStream in = c.getInputStream();
            java.io.ByteArrayOutputStream buf = new java.io.ByteArrayOutputStream();
            try {
                byte[] b = new byte[8192];
                int n;
                while ((n = in.read(b)) != -1) buf.write(b, 0, n);
            } finally { in.close(); }
            JSONArray assets = new JSONObject(buf.toString("UTF-8")).getJSONArray("assets");
            for (int i = 0; i < assets.length(); i++) {
                JSONObject a = assets.getJSONObject(i);
                if (p[3].equals(a.optString("name"))) { assetUrl = a.getString("url"); break; }
            }
        } finally { c.disconnect(); }
        if (assetUrl == null || !assetUrl.startsWith("https://api.github.com/"))
            throw new IllegalStateException("В релизе " + p[2] + " на GitHub нет файла " + p[3] + ".");
        c = gitHub(assetUrl, "application/octet-stream");
        try {
            int code = c.getResponseCode();
            if (code < 300 || code >= 400) checkGitHub(code, "файл " + p[3]);
            String location = c.getHeaderField("Location");
            if (location == null || !location.startsWith("https://"))
                throw new IllegalStateException("GitHub не выдал ссылку на файл " + p[3] + ".");
            return location;
        } finally { c.disconnect(); }
    }

    private static HttpURLConnection gitHub(String url, String accept) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(CONNECT_TIMEOUT);
        c.setReadTimeout(READ_TIMEOUT);
        c.setInstanceFollowRedirects(false); // токен не должен уйти дальше api.github.com
        c.setRequestProperty("User-Agent", "Bastion-Launcher/0.4");
        c.setRequestProperty("Accept", accept);
        c.setRequestProperty("Authorization", "Bearer " + GH_TOKEN);
        c.setRequestProperty("X-GitHub-Api-Version", "2022-11-28");
        return c;
    }

    private static void checkGitHub(int code, String what) {
        if (code >= 200 && code < 300) return;
        if (code == 401) throw new IllegalStateException("GitHub не принял токен лаунчера (401): токен удалён или истёк.");
        if (code == 404) throw new IllegalStateException("GitHub: " + what + " не найден (404) или у токена нет доступа к репозиторию.");
        if (code == 403 || code == 429) throw new IllegalStateException("GitHub временно ограничил запросы (" + code + "). Попробуй позже.");
        throw new IllegalStateException("GitHub ответил HTTP " + code + ".");
    }

    /** Ссылка на Яндекс Диск (disk.yandex.ru/d/…, yadi.sk/…)? */
    private static boolean isYandexDisk(String address) {
        try {
            String h = new URL(address).getHost().toLowerCase(java.util.Locale.ROOT);
            return h.equals("yadi.sk") || h.equals("disk.yandex.ru") || h.equals("disk.yandex.com")
                    || h.equals("disk.360.yandex.ru");
        } catch (Exception e) {
            return false;
        }
    }

    /** Публичная ссылка Яндекс Диска → временная прямая ссылка на файл (API Диска). */
    private static String resolveYandexDisk(String address) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(
                "https://cloud-api.yandex.net/v1/disk/public/resources/download?public_key="
                        + java.net.URLEncoder.encode(address, "UTF-8")).openConnection();
        c.setConnectTimeout(CONNECT_TIMEOUT);
        c.setReadTimeout(READ_TIMEOUT);
        try {
            int code = c.getResponseCode();
            if (code == 404) throw new IllegalStateException("Яндекс Диск: файл по ссылке не найден или доступ по ссылке выключен.");
            if (code == 403 || code == 429)
                throw new IllegalStateException("Яндекс Диск временно ограничил скачивание этого файла (HTTP " + code + "). Попробуй позже.");
            if (code != 200) throw new IllegalStateException("Яндекс Диск недоступен: HTTP " + code);
            java.io.ByteArrayOutputStream buf = new java.io.ByteArrayOutputStream();
            InputStream in = c.getInputStream();
            try {
                byte[] b = new byte[8192];
                int n;
                while ((n = in.read(b)) != -1) { buf.write(b, 0, n); if (buf.size() > 65536) break; }
            } finally { in.close(); }
            String href = new JSONObject(buf.toString("UTF-8")).getString("href");
            if (!href.startsWith("https://")) throw new IllegalStateException("Яндекс Диск вернул непонятную ссылку.");
            return href;
        } finally { c.disconnect(); }
    }

    private void downloadArchive(String address, File target, Callback callback) throws Exception {
        if (isYandexDisk(address)) {
            report(callback, "Получение ссылки Яндекс Диска…");
            address = resolveYandexDisk(address);
        }
        if (address.startsWith("gh://")) {
            report(callback, "Получение ссылки GitHub…");
            address = resolveGitHub(address);
        }
        String b2Token = null;
        if (address.startsWith("b2://")) {
            report(callback, "Получение доступа к хранилищу…");
            String[] r = resolveB2(address);
            address = r[0];
            b2Token = r[1];
        }
        HttpURLConnection connection = (HttpURLConnection) new URL(address).openConnection();
        connection.setConnectTimeout(CONNECT_TIMEOUT);
        connection.setReadTimeout(READ_TIMEOUT);
        connection.setInstanceFollowRedirects(true);
        connection.setRequestProperty("User-Agent", "Bastion-Launcher/0.4");
        if (b2Token != null) connection.setRequestProperty("Authorization", b2Token);
        else if (sendsDownloadKey(address)) connection.setRequestProperty("X-Bastion-Key", DOWNLOAD_KEY);
        try {
            int code = connection.getResponseCode();
            if (b2Token != null && (code == 401 || code == 403))
                throw new IllegalStateException("Хранилище B2 не отдало архив (HTTP " + code + "): у ключа нет доступа к этому бакету.");
            if (b2Token != null && code == 404)
                throw new IllegalStateException("В хранилище B2 нет файла архива (HTTP 404). Проверь имя бакета и файла.");
            if (code == 404 && sendsDownloadKey(address))
                throw new IllegalStateException("Сервер не отдал архив (HTTP 404): файла нет в хранилище или ключ лаунчера устарел.");
            if (code < 200 || code >= 300) throw new IllegalStateException("Ошибка загрузки архива: HTTP " + code);
            long expected = connection.getContentLengthLong();
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

    /**
     * Определяет папку архива с игрой: сначала GAME_ARCHIVE_PREFIX, затем самая
     * верхняя папка, содержащая SAMP/. Если SAMP/ нигде нет — архив не подходит.
     */
    static String detectGamePrefix(File archive) throws Exception {
        java.util.zip.ZipFile zip = new java.util.zip.ZipFile(archive);
        try {
            String best = null;
            boolean hasDefault = false;
            java.util.Enumeration<? extends java.util.zip.ZipEntry> entries = zip.entries();
            while (entries.hasMoreElements()) {
                String name = entries.nextElement().getName().replace('\\', '/');
                if (name.startsWith(GAME_ARCHIVE_PREFIX + SAMP_DIR + "/")) hasDefault = true;
                int from = 0;
                while (true) {
                    int idx = name.indexOf(SAMP_DIR + "/", from);
                    if (idx < 0) break;
                    if (idx == 0 || name.charAt(idx - 1) == '/') {
                        String p = name.substring(0, idx);
                        if (best == null || p.length() < best.length()) best = p;
                        break;
                    }
                    from = idx + 1;
                }
            }
            if (hasDefault) return GAME_ARCHIVE_PREFIX;
            if (best == null)
                throw new IllegalStateException("В архиве нет папки SAMP/ — это не кэш SAMP-Mobile GTA-2.11.");
            return best;
        } finally { zip.close(); }
    }

    /**
     * Проверка распакованной игры. Обязательна непустая папка SAMP/; про
     * отсутствующие стандартные папки GTA SA сообщается в итоге установки.
     */
    static String verifyInstall(File root, int count) {
        File samp = new File(root, SAMP_DIR);
        String[] sampFiles = samp.list();
        if (!samp.isDirectory() || sampFiles == null || sampFiles.length == 0)
            throw new IllegalStateException("После распаковки нет папки SAMP/ с файлами клиента.");
        // Без этих файлов SAMP-Mobile GTA-2.11 гарантированно падает при старте:
        // такой архив не устанавливается, предыдущая установка остаётся.
        java.util.List<String> required = new java.util.ArrayList<String>();
        for (String rel : REQUIRED_FILES) {
            if (rel.equals(SAMP_FONT)) continue;   // шрифт лаунчер подкладывает сам
            File f = resolveIgnoreCase(root, rel);
            if (f == null || !f.isFile() || f.length() == 0) required.add(rel);
        }
        File texdbDir = resolveIgnoreCase(root, "texdb");
        if (texdbDir == null || !texdbDir.isDirectory()) required.add("texdb/");
        if (chooseGameLanguage(root) == null) required.add("text/russian.gxt или text/american.gxt");
        if (!required.isEmpty()) {
            StringBuilder list = new StringBuilder();
            for (String m : required) list.append("\n• ").append(m);
            throw new IllegalStateException("Архив неполный, это не кэш SAMP-Mobile GTA-2.11. Нет файлов:" + list
                    + "\n\nУстановка отменена, прежние файлы игры не тронуты.");
        }
        StringBuilder missing = new StringBuilder();
        for (String dir : new String[]{"texdb", "audio", "data", "models"}) {
            if (!existsIgnoreCase(root, dir)) missing.append(missing.length() == 0 ? "" : ", ").append(dir);
        }
        if (!existsIgnoreCase(samp, "fonts")) missing.append(missing.length() == 0 ? "" : ", ").append("SAMP/fonts");
        StringBuilder out = new StringBuilder();
        out.append("files=").append(count).append('\n');
        out.append("samp_files=").append(sampFiles.length).append('\n');
        out.append("missing=").append(missing).append('\n');
        return out.toString();
    }

    private static boolean existsIgnoreCase(File dir, String name) {
        String[] names = dir.list();
        if (names == null) return false;
        for (String n : names) if (n.equalsIgnoreCase(name)) return true;
        return false;
    }

    /** Текст предупреждения о неполной установке (пусто, если всё на месте). */
    public static String installWarnings(Context context) {
        File dir = gameDir(context);
        if (dir == null) return "";
        File marker = new File(dir, INSTALL_MARKER);
        try {
            java.io.BufferedReader r = new java.io.BufferedReader(new java.io.InputStreamReader(
                    new java.io.FileInputStream(marker), "UTF-8"));
            try {
                String l;
                while ((l = r.readLine()) != null)
                    if (l.startsWith("missing=") && l.length() > 8) return "Не найдены папки: " + l.substring(8);
            } finally { r.close(); }
        } catch (Exception ignored) { }
        return "";
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