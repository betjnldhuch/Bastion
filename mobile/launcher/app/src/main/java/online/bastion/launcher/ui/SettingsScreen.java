package online.bastion.launcher.ui;

import android.app.Activity;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import online.bastion.launcher.core.ServerAddress;
import online.bastion.launcher.core.ServerEntry;
import online.bastion.launcher.data.LauncherPrefs;
import online.bastion.launcher.game.GameLauncher;

import static online.bastion.launcher.ui.Theme.dp;
import static online.bastion.launcher.ui.Views.MATCH;
import static online.bastion.launcher.ui.Views.WRAP;

/**
 * Настройки: слева игра и кеш, справа адреса серверов.
 * Адреса можно заблокировать в launcher.ini (lock_servers = true) — тогда поля только для чтения.
 */
public final class SettingsScreen {

    /** Поля одного сервера. */
    private static final class ServerRow {
        final ServerEntry server;
        final EditText host;
        final EditText port;
        final TextView error;

        ServerRow(ServerEntry server, EditText host, EditText port, TextView error) {
            this.server = server;
            this.host = host;
            this.port = port;
            this.error = error;
        }
    }

    private final ScreenHost host;
    private final Activity a;
    private final GameLauncher game;
    private final List<ServerRow> rows = new ArrayList<ServerRow>();
    private final View root;

    public SettingsScreen(ScreenHost host, GameLauncher game) {
        this.host = host;
        this.a = host.activity();
        this.game = game;
        this.root = build();
    }

    public View view() { return root; }

    private boolean locked() { return host.config().serversLocked; }

    private View build() {
        LinearLayout page = Views.vertical(a);
        page.setPadding(dp(28), dp(14), dp(28), dp(14));

        LinearLayout top = Views.horizontal(a);
        TextView title = Views.text(a, "НАСТРОЙКИ", 26, Theme.TEXT, Theme.heavy);
        title.setLetterSpacing(0.08f);
        top.addView(title, Views.lp(0, WRAP, 1));
        TextView done = Views.primaryButton(a, "Готово", 12);
        done.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { if (save()) host.openMain(); }
        });
        top.addView(done, Views.lp(WRAP, dp(38)));
        page.addView(top, Views.lp(MATCH, WRAP));
        page.addView(Views.rule(a), Views.margins(Views.lp(MATCH, dp(1)), 0, 10, 0, 14));

        LinearLayout columns = Views.horizontal(a);
        columns.setGravity(Gravity.TOP);
        columns.addView(gamePanel(), Views.margins(Views.lp(0, WRAP, 1), 0, 0, 16, 0));
        columns.addView(serverPanel(), Views.lp(0, WRAP, 1));

        ScrollView scroll = new ScrollView(a);
        scroll.setVerticalScrollBarEnabled(false);
        scroll.addView(columns);
        page.addView(scroll, Views.lp(MATCH, 0, 1));
        return page;
    }

    private LinearLayout panel() {
        LinearLayout p = Views.vertical(a);
        p.setPadding(dp(18), dp(16), dp(18), dp(18));
        p.setBackground(Views.box(Theme.SURFACE, Theme.LINE, 1));
        return p;
    }

    // ---------------------------------------------------------------- игра и кеш

    private View gamePanel() {
        final LauncherPrefs prefs = host.prefs();
        LinearLayout p = panel();
        p.addView(Views.caption(a, "Игра"));
        p.addView(Views.segmented(a, "Строк в чате", new String[]{"6", "10", "15"},
                LauncherPrefs.CHAT_LINES, prefs.chatLines(), new Views.OnPick() {
                    @Override public void onPick(int v) { prefs.setChatLines(v); }
                }));
        p.addView(Views.segmented(a, "Размер текста", new String[]{"S", "M", "L"},
                new int[]{LauncherPrefs.FONT_SIZES[1], LauncherPrefs.FONT_SIZES[0], LauncherPrefs.FONT_SIZES[2]},
                prefs.fontSize(), new Views.OnPick() {
                    @Override public void onPick(int v) { prefs.setFontSize(v); }
                }));
        p.addView(Views.segmented(a, "Лимит FPS", new String[]{"30", "60", "90", "120"},
                new int[]{30, 60, 90, 120}, prefs.fpsLimit(), new Views.OnPick() {
                    @Override public void onPick(int v) { prefs.setFpsLimit(v); }
                }));

        p.addView(Views.caption(a, "Кеш игры"), Views.margins(Views.lp(MATCH, WRAP), 0, 22, 0, 0));
        File dir = game.storageDir();
        List<String> missing = game.missingCacheFiles();
        String status;
        int color;
        if (dir == null) {
            status = "Память недоступна";
            color = Theme.ERROR;
        } else if (missing.isEmpty()) {
            status = "Файлы на месте";
            color = Theme.ORANGE;
        } else {
            status = "Не хватает файлов: " + missing.size();
            color = Theme.ERROR;
        }
        TextView st = Views.text(a, status.toUpperCase(), 11, color, Theme.heavy);
        st.setLetterSpacing(0.08f);
        p.addView(st, Views.margins(Views.lp(MATCH, WRAP), 0, 12, 0, 0));
        TextView path = Views.text(a, dir == null ? "—" : dir.getAbsolutePath(), 11, Theme.MUTED, Theme.body);
        path.setTextIsSelectable(true);
        p.addView(path, Views.margins(Views.lp(MATCH, WRAP), 0, 6, 0, 0));

        TextView diagnostics = Views.ghostButton(a, "Диагностика");
        diagnostics.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { host.showDiagnostics(); }
        });
        p.addView(diagnostics, Views.margins(Views.lp(WRAP, dp(36)), 0, 16, 0, 0));
        return p;
    }

    // ---------------------------------------------------------------- серверы

    private View serverPanel() {
        LinearLayout p = panel();
        LinearLayout head = Views.horizontal(a);
        head.addView(Views.caption(a, "Адреса серверов"), Views.lp(0, WRAP, 1));
        if (locked()) {
            TextView badge = Views.text(a, "ЗАБЛОКИРОВАНО", 9, Theme.BLACK, Theme.heavy);
            badge.setLetterSpacing(0.12f);
            badge.setPadding(dp(8), dp(4), dp(8), dp(4));
            badge.setBackground(Views.box(Theme.ORANGE, 0, 0));
            head.addView(badge);
        }
        p.addView(head);

        for (ServerEntry s : host.config().servers) p.addView(serverRow(s));

        String note = locked()
                ? "Адреса заданы в сборке и не меняются."
                : "Для теста дома укажи IP компьютера в своей Wi-Fi сети. 127.0.0.1 — это сам телефон.";
        TextView hint = Views.text(a, note, 11, Theme.DIM, Theme.body);
        hint.setLineSpacing(dp(2), 1f);
        p.addView(hint, Views.margins(Views.lp(MATCH, WRAP), 0, 12, 0, 0));

        if (!locked()) {
            LinearLayout buttons = Views.horizontal(a);
            TextView check = Views.ghostButton(a, "Сохранить и проверить");
            check.setOnClickListener(new View.OnClickListener() {
                @Override public void onClick(View v) { save(); }
            });
            buttons.addView(check, Views.lp(WRAP, dp(36)));
            TextView reset = Views.ghostButton(a, "По умолчанию");
            reset.setOnClickListener(new View.OnClickListener() {
                @Override public void onClick(View v) { resetAddresses(); }
            });
            buttons.addView(reset, Views.margins(Views.lp(WRAP, dp(36)), 10, 0, 0, 0));
            p.addView(buttons, Views.margins(Views.lp(MATCH, WRAP), 0, 14, 0, 0));
        }
        return p;
    }

    private View serverRow(ServerEntry s) {
        LinearLayout box = Views.vertical(a);
        TextView name = Views.text(a, s.name.toUpperCase(), 12, Theme.TEXT, Theme.heavy);
        name.setLetterSpacing(0.06f);
        box.addView(name, Views.margins(Views.lp(MATCH, WRAP), 0, 16, 0, 0));

        ServerAddress current = host.prefs().addressOf(s);
        LinearLayout line = Views.horizontal(a);
        EditText hostField = Views.field(a, current.host, "IP или домен", ServerAddress.MAX_HOST_LENGTH);
        EditText portField = Views.field(a, String.valueOf(current.port), "порт", 5);
        portField.setInputType(InputType.TYPE_CLASS_NUMBER);
        if (locked()) {
            for (EditText e : new EditText[]{hostField, portField}) {
                e.setEnabled(false);
                e.setFocusable(false);
                Views.setFieldState(e, false);
            }
        }
        line.addView(hostField, Views.lp(0, dp(42), 1));
        line.addView(portField, Views.margins(Views.lp(dp(86), dp(42)), 8, 0, 0, 0));
        box.addView(line, Views.margins(Views.lp(MATCH, WRAP), 0, 8, 0, 0));

        TextView error = Views.text(a, "", 11, Theme.ERROR, Theme.bodyMedium);
        error.setVisibility(View.GONE);
        box.addView(error, Views.margins(Views.lp(MATCH, WRAP), 0, 6, 0, 0));

        rows.add(new ServerRow(s, hostField, portField, error));
        return box;
    }

    private void resetAddresses() {
        host.prefs().resetAddresses();
        for (ServerRow r : rows) {
            r.host.setText(r.server.defaultAddress.host);
            r.port.setText(String.valueOf(r.server.defaultAddress.port));
            showError(r, null);
        }
        host.monitor().refreshAll();
    }

    /**
     * Проверяет и сохраняет адреса. Неверные поля подсвечиваются, и экран не закрывается.
     * @return true, если всё сохранено.
     */
    public boolean save() {
        if (locked()) return true;
        boolean ok = true;
        for (ServerRow r : rows) {
            try {
                ServerAddress addr = ServerAddress.parse(r.host.getText().toString(), r.port.getText().toString());
                showError(r, null);
                if (!addr.equals(host.prefs().addressOf(r.server))) {
                    host.prefs().setAddress(r.server, addr);
                }
            } catch (ServerAddress.InvalidException e) {
                showError(r, e.getMessage());
                ok = false;
            }
        }
        host.monitor().refreshAll();
        return ok;
    }

    private void showError(ServerRow r, String message) {
        r.error.setText(message == null ? "" : message);
        r.error.setVisibility(message == null ? View.GONE : View.VISIBLE);
        Views.setFieldState(r.host, message != null);
        Views.setFieldState(r.port, message != null);
    }
}
