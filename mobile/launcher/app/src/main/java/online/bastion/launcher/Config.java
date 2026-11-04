package online.bastion.launcher;

import android.content.Context;
import android.content.SharedPreferences;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.List;

/** Настройки лаунчера: сервера из assets/servers.ini + то, что игрок поменял сам. */
public final class Config {

    public static final class Server {
        public String name, host, description;
        public int port;
        // результат опроса
        public volatile boolean online;
        public volatile boolean queried;
        public volatile int players, maxPlayers, ping;
        public volatile String hostName = "";
    }

    public final List<Server> servers = new ArrayList<Server>();
    public final List<String[]> news = new ArrayList<String[]>();

    public String nickname = "";
    public int selected = 0;
    public int fps = 60;          // 30 / 60 / 90 / 120
    public int quality = 1;       // 0 низко, 1 средне, 2 высоко
    public int chatLines = 10;    // 10 / 15 / 20
    public int buttonSize = 1;    // 0 S, 1 M, 2 L
    public boolean requireRpName = true;

    private final SharedPreferences prefs;

    public Config(Context ctx) {
        prefs = ctx.getSharedPreferences("bastion", Context.MODE_PRIVATE);
        loadServers(ctx);
        loadNews(ctx);
        nickname = prefs.getString("nickname", "");
        selected = prefs.getInt("selected", 0);
        fps = prefs.getInt("fps", 60);
        quality = prefs.getInt("quality", 1);
        chatLines = prefs.getInt("chatLines", 10);
        buttonSize = prefs.getInt("buttonSize", 1);
        for (int i = 0; i < servers.size(); i++) {
            Server s = servers.get(i);
            s.host = prefs.getString("host" + i, s.host);
            s.port = prefs.getInt("port" + i, s.port);
        }
        if (selected < 0 || selected >= servers.size()) selected = 0;
    }

    public void save() {
        SharedPreferences.Editor e = prefs.edit();
        e.putString("nickname", nickname);
        e.putInt("selected", selected);
        e.putInt("fps", fps);
        e.putInt("quality", quality);
        e.putInt("chatLines", chatLines);
        e.putInt("buttonSize", buttonSize);
        for (int i = 0; i < servers.size(); i++) {
            e.putString("host" + i, servers.get(i).host);
            e.putInt("port" + i, servers.get(i).port);
        }
        e.apply();
    }

    /** Сброс адресов серверов к тем, что зашиты в servers.ini. */
    public void resetServers(Context ctx) {
        SharedPreferences.Editor e = prefs.edit();
        for (int i = 0; i < servers.size(); i++) { e.remove("host" + i); e.remove("port" + i); }
        e.apply();
        servers.clear();
        loadServers(ctx);
    }

    public static boolean isRpName(String n) {
        return n != null && n.length() >= 6 && n.length() <= 24
                && n.matches("^[A-Z][a-z]+_[A-Z][a-z]+$");
    }

    /**
     * Файл для будущего игрового клиента (этап 2): клиент прочитает отсюда
     * ник, сервер и графику. Лежит в Android/data/online.bastion.launcher/files/.
     */
    public File writeClientConfig(Context ctx) {
        Server s = servers.get(selected);
        File dir = ctx.getExternalFilesDir(null);
        if (dir == null) dir = ctx.getFilesDir();
        File f = new File(dir, "bastion.cfg");
        try {
            OutputStreamWriter w = new OutputStreamWriter(new FileOutputStream(f), "UTF-8");
            w.write("name=" + nickname + "\n");
            w.write("host=" + s.host + "\n");
            w.write("port=" + s.port + "\n");
            w.write("fps=" + fps + "\n");
            w.write("quality=" + quality + "\n");
            w.write("chat_lines=" + chatLines + "\n");
            w.write("button_size=" + buttonSize + "\n");
            w.close();
        } catch (Exception ignored) { }
        return f;
    }

    private void loadServers(Context ctx) {
        for (String line : readAsset(ctx, "servers.ini")) {
            line = line.trim();
            if (line.length() == 0 || line.startsWith(";")) continue;
            int eq = line.indexOf('=');
            if (eq < 0) continue;
            String key = line.substring(0, eq).trim().toLowerCase();
            String val = line.substring(eq + 1).trim();
            if (key.equals("require_rp_name")) {
                requireRpName = !val.equalsIgnoreCase("false");
            } else if (key.equals("server")) {
                String[] p = val.split("\\|");
                if (p.length < 3) continue;
                Server s = new Server();
                s.name = p[0].trim();
                s.host = p[1].trim();
                try { s.port = Integer.parseInt(p[2].trim()); } catch (NumberFormatException ex) { s.port = 7777; }
                s.description = p.length > 3 ? p[3].trim() : "";
                servers.add(s);
            }
        }
        if (servers.isEmpty()) {
            Server s = new Server();
            s.name = "Bastion Peace"; s.host = "127.0.0.1"; s.port = 7777; s.description = "";
            servers.add(s);
        }
    }

    private void loadNews(Context ctx) {
        for (String line : readAsset(ctx, "news.txt")) {
            line = line.trim();
            if (line.length() == 0 || line.startsWith(";")) continue;
            String[] p = line.split("\\|", 3);
            if (p.length == 3) news.add(new String[]{p[0].trim(), p[1].trim(), p[2].trim()});
        }
    }

    private static List<String> readAsset(Context ctx, String name) {
        List<String> out = new ArrayList<String>();
        try {
            BufferedReader r = new BufferedReader(new InputStreamReader(ctx.getAssets().open(name), "UTF-8"));
            String l;
            while ((l = r.readLine()) != null) {
                if (out.isEmpty() && l.length() > 0 && l.charAt(0) == '﻿') l = l.substring(1);
                out.add(l);
            }
            r.close();
        } catch (Exception ignored) { }
        return out;
    }
}
