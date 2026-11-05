package online.bastion.launcher.data;

import android.content.Context;
import android.content.SharedPreferences;

import online.bastion.launcher.core.LauncherConfig;
import online.bastion.launcher.core.ServerAddress;
import online.bastion.launcher.core.ServerEntry;

/**
 * То, что игрок меняет сам: ник, выбранный сервер, свои адреса серверов и настройки игры.
 * Хранится в SharedPreferences. Если в launcher.ini lock_servers = true, сохранённые
 * игроком адреса игнорируются — всегда используются адреса из сборки.
 */
public final class LauncherPrefs {

    /** Варианты настроек игры. Первый элемент — значение по умолчанию. */
    public static final int[] CHAT_LINES = {6, 10, 15};
    public static final int[] FONT_SIZES = {37, 30, 44};
    public static final int[] FPS_LIMITS = {60, 30, 90, 120};

    private static final String FILE = "bastion";
    private static final String K_NICK = "nickname";
    private static final String K_SELECTED = "selected_server";
    private static final String K_CHAT = "chat_lines";
    private static final String K_FONT = "font_size";
    private static final String K_FPS = "fps_limit";

    private final SharedPreferences prefs;
    private final LauncherConfig config;

    public LauncherPrefs(Context context, LauncherConfig config) {
        this.prefs = context.getSharedPreferences(FILE, Context.MODE_PRIVATE);
        this.config = config;
    }

    public String nickname() { return prefs.getString(K_NICK, ""); }

    public void setNickname(String nick) { prefs.edit().putString(K_NICK, nick == null ? "" : nick.trim()).apply(); }

    /** Выбранный сервер; если сохранённого больше нет в launcher.ini — первый. */
    public ServerEntry selectedServer() {
        String id = prefs.getString(K_SELECTED, "");
        for (ServerEntry s : config.servers) if (s.id.equals(id)) return s;
        return config.servers.isEmpty() ? null : config.servers.get(0);
    }

    public void setSelectedServer(ServerEntry server) { prefs.edit().putString(K_SELECTED, server.id).apply(); }

    /** Адрес, по которому реально подключаемся: свой адрес игрока или адрес из сборки. */
    public ServerAddress addressOf(ServerEntry server) {
        if (config.serversLocked) return server.defaultAddress;
        String host = prefs.getString(hostKey(server), null);
        int port = prefs.getInt(portKey(server), -1);
        if (host == null || port < 0) return server.defaultAddress;
        try {
            return ServerAddress.parse(host, port);
        } catch (ServerAddress.InvalidException e) {
            return server.defaultAddress;  // испорченное значение — не ломаем запуск
        }
    }

    /** @return false, если адреса заблокированы сборкой и сохранение запрещено. */
    public boolean setAddress(ServerEntry server, ServerAddress address) {
        if (config.serversLocked) return false;
        if (address.equals(server.defaultAddress)) {
            prefs.edit().remove(hostKey(server)).remove(portKey(server)).apply();
        } else {
            prefs.edit().putString(hostKey(server), address.host).putInt(portKey(server), address.port).apply();
        }
        return true;
    }

    public void resetAddresses() {
        SharedPreferences.Editor e = prefs.edit();
        for (ServerEntry s : config.servers) e.remove(hostKey(s)).remove(portKey(s));
        e.apply();
    }

    public int chatLines() { return oneOf(prefs.getInt(K_CHAT, CHAT_LINES[0]), CHAT_LINES); }
    public void setChatLines(int v) { prefs.edit().putInt(K_CHAT, oneOf(v, CHAT_LINES)).apply(); }

    public int fontSize() { return oneOf(prefs.getInt(K_FONT, FONT_SIZES[0]), FONT_SIZES); }
    public void setFontSize(int v) { prefs.edit().putInt(K_FONT, oneOf(v, FONT_SIZES)).apply(); }

    public int fpsLimit() { return oneOf(prefs.getInt(K_FPS, FPS_LIMITS[0]), FPS_LIMITS); }
    public void setFpsLimit(int v) { prefs.edit().putInt(K_FPS, oneOf(v, FPS_LIMITS)).apply(); }

    private static int oneOf(int v, int[] allowed) {
        for (int a : allowed) if (a == v) return v;
        return allowed[0];
    }

    private static String hostKey(ServerEntry s) { return "server." + s.id + ".host"; }
    private static String portKey(ServerEntry s) { return "server." + s.id + ".port"; }
}
