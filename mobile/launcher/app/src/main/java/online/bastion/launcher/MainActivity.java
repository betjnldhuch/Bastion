package online.bastion.launcher;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;
import android.widget.Toast;

import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;

import online.bastion.launcher.core.LauncherConfig;
import online.bastion.launcher.core.Nickname;
import online.bastion.launcher.core.QueryResult;
import online.bastion.launcher.core.ServerAddress;
import online.bastion.launcher.core.ServerEntry;
import online.bastion.launcher.data.LauncherPrefs;
import online.bastion.launcher.game.Diagnostics;
import online.bastion.launcher.game.GameLauncher;
import online.bastion.launcher.net.ServerMonitor;
import online.bastion.launcher.ui.MainScreen;
import online.bastion.launcher.ui.Overlay;
import online.bastion.launcher.ui.ScreenHost;
import online.bastion.launcher.ui.SettingsScreen;
import online.bastion.launcher.ui.Theme;

/**
 * Единственная Activity лаунчера. Здесь только жизненный цикл и переходы между экранами;
 * внешний вид — в пакете ui, логика — в core / net / game.
 */
public class MainActivity extends Activity implements ScreenHost {

    private static final String CONFIG_ASSET = "launcher.ini";

    private LauncherConfig config;
    private LauncherPrefs prefs;
    private ServerMonitor monitor;
    private GameLauncher game;

    private FrameLayout root;      // экран + окна поверх него
    private FrameLayout content;   // текущий экран
    private MainScreen mainScreen;
    private SettingsScreen settingsScreen;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        Theme.init(this);
        config = loadConfig();
        prefs = new LauncherPrefs(this, config);
        game = new GameLauncher(this, config);
        monitor = new ServerMonitor(config.servers, new ServerMonitor.AddressSource() {
            @Override public ServerAddress addressOf(ServerEntry server) { return prefs.addressOf(server); }
        }, config.pollIntervalSec);

        root = new FrameLayout(this);
        root.setBackgroundColor(Theme.BLACK);
        content = new FrameLayout(this);
        root.addView(content, new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        setContentView(root);
        openMain();

        if (config.servers.isEmpty()) {
            Overlay.message(root, "Ошибка сборки",
                    "В assets/" + CONFIG_ASSET + " нет ни одного сервера.\n\n" + join(config));
        }
    }

    /** Читает assets/launcher.ini. Если файла нет — пустая конфигурация с предупреждением. */
    private LauncherConfig loadConfig() {
        Reader r = null;
        try {
            r = new InputStreamReader(getAssets().open(CONFIG_ASSET), "UTF-8");
            return LauncherConfig.parse(r);
        } catch (IOException e) {
            try {
                return LauncherConfig.parse(new java.io.StringReader(""));
            } catch (IOException impossible) {
                throw new IllegalStateException(impossible);
            }
        } finally {
            if (r != null) try { r.close(); } catch (IOException ignored) { }
        }
    }

    private static String join(LauncherConfig c) {
        StringBuilder b = new StringBuilder();
        for (String w : c.warnings) b.append("• ").append(w).append('\n');
        return b.toString().trim();
    }

    @Override protected void onResume() {
        super.onResume();
        hideSystemBars();
        monitor.start();
    }

    @Override protected void onPause() {
        super.onPause();
        saveNickname();
        monitor.stop();
    }

    @Override protected void onDestroy() {
        super.onDestroy();
        monitor.shutdown();
    }

    @Override public void onWindowFocusChanged(boolean focus) {
        super.onWindowFocusChanged(focus);
        if (focus) hideSystemBars();
    }

    @SuppressWarnings("deprecation")
    private void hideSystemBars() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    @SuppressWarnings("deprecation")
    @Override public void onBackPressed() {
        if (Overlay.dismissTop(root)) return;
        if (settingsScreen != null) {
            if (settingsScreen.save()) openMain();
            return;
        }
        super.onBackPressed();
    }

    // ------------------------------------------------------------------ ScreenHost

    @Override public Activity activity() { return this; }
    @Override public LauncherConfig config() { return config; }
    @Override public LauncherPrefs prefs() { return prefs; }
    @Override public ServerMonitor monitor() { return monitor; }

    @Override public void openMain() {
        hideKeyboard();
        settingsScreen = null;
        mainScreen = new MainScreen(this);
        monitor.setListener(mainScreen);
        show(mainScreen.view());
    }

    @Override public void openSettings() {
        saveNickname();
        hideKeyboard();
        mainScreen = null;
        monitor.setListener(null);
        settingsScreen = new SettingsScreen(this, game);
        show(settingsScreen.view());
    }

    private void show(View screen) {
        content.removeAllViews();
        content.addView(screen, new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
    }

    @Override public void play() {
        hideKeyboard();
        saveNickname();

        String error = Nickname.validate(prefs.nickname(), config.requireRpName);
        if (error != null) {
            Overlay.message(root, "Проверь ник", error);
            return;
        }
        final ServerEntry server = prefs.selectedServer();
        if (server == null) {
            Overlay.message(root, "Нет серверов", "В сборке не указан ни один сервер.");
            return;
        }

        // Сервер не ответил на последний опрос — предупреждаем, но даём запустить:
        // query-порт может быть закрыт, а игровой — работать.
        ServerMonitor.Status status = monitor.statusOf(server);
        QueryResult last = status == null ? null : status.result;
        if (last != null && !last.isOnline()) {
            String why = last.state == QueryResult.State.UNRESOLVED
                    ? "Адрес " + status.address.host + " не найден."
                    : "Сервер " + status.address + " не отвечает. Возможно, он выключен.";
            Overlay.show(root, "Сервер недоступен", why,
                    new Overlay.Action("Отмена", false, null),
                    new Overlay.Action("Всё равно играть", true, new Runnable() {
                        @Override public void run() { launch(server); }
                    }));
            return;
        }
        launch(server);
    }

    private void launch(ServerEntry server) {
        try {
            game.launch(this, prefs.nickname(), server, prefs.addressOf(server), prefs);
        } catch (GameLauncher.LaunchException e) {
            Overlay.message(root, e.title, e.getMessage());
        }
    }

    @Override public void showDiagnostics() {
        final String report = Diagnostics.report(this, config, game);
        Overlay.show(root, "Диагностика", report,
                new Overlay.Action("Закрыть", false, null),
                new Overlay.Action("Копировать", true, new Runnable() {
                    @Override public void run() {
                        ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
                        if (cm != null) {
                            cm.setPrimaryClip(ClipData.newPlainText("Bastion diagnostics", report));
                            Toast.makeText(MainActivity.this, "Скопировано", Toast.LENGTH_SHORT).show();
                        }
                    }
                }));
    }

    // ------------------------------------------------------------------ helpers

    private void saveNickname() {
        if (mainScreen != null) prefs.setNickname(mainScreen.nickname());
    }

    private void hideKeyboard() {
        View f = getCurrentFocus();
        if (f == null) return;
        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) imm.hideSoftInputFromWindow(f.getWindowToken(), 0);
        f.clearFocus();
    }
}
