package online.bastion.launcher;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

public class MainActivity extends Activity {

    /** Пакет официальной GTA: San Andreas из Google Play. */
    public static final String GTA_PACKAGE = "com.rockstargames.gtasa";

    private static final int MATCH = ViewGroup.LayoutParams.MATCH_PARENT;
    private static final int WRAP = ViewGroup.LayoutParams.WRAP_CONTENT;

    private Config cfg;
    private FrameLayout root;
    private FrameLayout content;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private final List<ServerCard> cards = new ArrayList<ServerCard>();
    private EditText nickField;
    private boolean resumed;

    private final Runnable poll = new Runnable() {
        @Override public void run() {
            if (!resumed) return;
            refreshServers();
            ui.postDelayed(this, 10000);
        }
    };

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        Ui.init(this);
        cfg = new Config(this);

        root = new FrameLayout(this);
        root.addView(new Ui.Background(this), new FrameLayout.LayoutParams(MATCH, MATCH));
        content = new FrameLayout(this);
        root.addView(content, new FrameLayout.LayoutParams(MATCH, MATCH));
        setContentView(root);
        showMain();
    }

    @Override protected void onResume() {
        super.onResume();
        resumed = true;
        hideSystemBars();
        ui.removeCallbacks(poll);
        ui.post(poll);
    }

    @Override protected void onPause() {
        super.onPause();
        resumed = false;
        ui.removeCallbacks(poll);
        if (nickField != null) cfg.nickname = nickField.getText().toString().trim();
        cfg.save();
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

    @Override public void onBackPressed() {
        if ("settings".equals(content.getTag())) { showMain(); return; }
        if (root.getChildCount() > 2) { root.removeViewAt(root.getChildCount() - 1); return; }
        super.onBackPressed();
    }

    // ==================================================================
    //  Главный экран
    // ==================================================================
    private void showMain() {
        content.removeAllViews();
        content.setTag("main");
        cards.clear();

        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(Ui.dp(24), Ui.dp(14), Ui.dp(24), Ui.dp(14));

        // --- шапка ---
        LinearLayout top = new LinearLayout(this);
        top.setOrientation(LinearLayout.HORIZONTAL);
        top.setGravity(Gravity.CENTER_VERTICAL);
        top.addView(new Ui.Header(this), new LinearLayout.LayoutParams(0, WRAP, 1));
        TextView settings = Ui.ghostButton(this, "Настройки");
        settings.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { saveNick(); showSettings(); }
        });
        top.addView(settings, new LinearLayout.LayoutParams(WRAP, Ui.dp(38)));
        page.addView(top, new LinearLayout.LayoutParams(MATCH, WRAP));

        // оранжевая линия под шапкой
        View rule = new View(this);
        rule.setBackground(new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,
                new int[]{Ui.ORANGE, 0x55FF7A1A, 0x00FF7A1A}));
        LinearLayout.LayoutParams rlp = new LinearLayout.LayoutParams(MATCH, Ui.dp(2));
        rlp.topMargin = Ui.dp(6);
        rlp.bottomMargin = Ui.dp(12);
        page.addView(rule, rlp);

        // --- середина: сервера слева, новости справа ---
        LinearLayout mid = new LinearLayout(this);
        mid.setOrientation(LinearLayout.HORIZONTAL);

        LinearLayout left = new LinearLayout(this);
        left.setOrientation(LinearLayout.VERTICAL);
        left.addView(Ui.caption(this, "Выбор сервера"));
        ScrollView ls = new ScrollView(this);
        ls.setVerticalScrollBarEnabled(false);
        LinearLayout list = new LinearLayout(this);
        list.setOrientation(LinearLayout.VERTICAL);
        for (int i = 0; i < cfg.servers.size(); i++) {
            ServerCard c = new ServerCard(i);
            cards.add(c);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(MATCH, WRAP);
            lp.topMargin = Ui.dp(8);
            list.addView(c.view, lp);
        }
        ls.addView(list);
        left.addView(ls, new LinearLayout.LayoutParams(MATCH, 0, 1));
        LinearLayout.LayoutParams llp = new LinearLayout.LayoutParams(0, MATCH, 1.15f);
        llp.rightMargin = Ui.dp(16);
        mid.addView(left, llp);

        LinearLayout right = new LinearLayout(this);
        right.setOrientation(LinearLayout.VERTICAL);
        right.addView(Ui.caption(this, "Новости"));
        ScrollView ns = new ScrollView(this);
        ns.setVerticalScrollBarEnabled(false);
        LinearLayout newsList = new LinearLayout(this);
        newsList.setOrientation(LinearLayout.VERTICAL);
        newsList.setPadding(Ui.dp(14), Ui.dp(10), Ui.dp(14), Ui.dp(10));
        newsList.setBackground(Ui.box(0xCC14161A, Ui.LINE, 8, 1));
        if (cfg.news.isEmpty()) newsList.addView(Ui.text(this, "Пока новостей нет.", 13, Ui.MUTED, Ui.body));
        for (int i = 0; i < cfg.news.size(); i++) {
            String[] n = cfg.news.get(i);
            LinearLayout item = new LinearLayout(this);
            item.setOrientation(LinearLayout.VERTICAL);
            item.setPadding(0, i == 0 ? 0 : Ui.dp(10), 0, Ui.dp(10));
            TextView d = Ui.text(this, n[0], 10, Ui.ORANGE, Ui.heavy);
            d.setLetterSpacing(0.1f);
            item.addView(d);
            TextView h = Ui.text(this, n[1], 14, Ui.TEXT, Ui.bodyBold);
            h.setPadding(0, Ui.dp(4), 0, Ui.dp(3));
            item.addView(h);
            TextView b = Ui.text(this, n[2], 12, Ui.MUTED, Ui.body);
            b.setLineSpacing(Ui.dp(2), 1f);
            item.addView(b);
            newsList.addView(item);
            if (i < cfg.news.size() - 1) {
                View sep = new View(this);
                sep.setBackgroundColor(Ui.LINE);
                newsList.addView(sep, new LinearLayout.LayoutParams(MATCH, 1));
            }
        }
        ns.addView(newsList);
        LinearLayout.LayoutParams nlp = new LinearLayout.LayoutParams(MATCH, 0, 1);
        nlp.topMargin = Ui.dp(8);
        right.addView(ns, nlp);
        mid.addView(right, new LinearLayout.LayoutParams(0, MATCH, 1f));

        page.addView(mid, new LinearLayout.LayoutParams(MATCH, 0, 1));

        // --- низ: ник + ИГРАТЬ ---
        LinearLayout bottom = new LinearLayout(this);
        bottom.setOrientation(LinearLayout.HORIZONTAL);
        bottom.setGravity(Gravity.CENTER_VERTICAL);

        LinearLayout nickBox = new LinearLayout(this);
        nickBox.setOrientation(LinearLayout.VERTICAL);
        nickBox.setGravity(Gravity.CENTER_VERTICAL);
        nickBox.setPadding(Ui.dp(14), 0, Ui.dp(14), 0);
        nickBox.setBackground(Ui.box(0xE614161A, Ui.LINE, 6, 1));
        TextView nl = Ui.text(this, "НИК  ·  Имя_Фамилия", 9, Ui.DIM, Ui.heavy);
        nl.setLetterSpacing(0.12f);
        nickBox.addView(nl);
        nickField = field(cfg.nickname, "Например: Ivan_Petrov");
        nickField.setBackground(null);
        nickField.setPadding(0, Ui.dp(3), 0, 0);
        nickBox.addView(nickField, new LinearLayout.LayoutParams(MATCH, WRAP));
        LinearLayout.LayoutParams nbl = new LinearLayout.LayoutParams(0, Ui.dp(52), 1);
        nbl.rightMargin = Ui.dp(12);
        bottom.addView(nickBox, nbl);

        TextView play = Ui.primaryButton(this, "ИГРАТЬ");
        play.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { onPlay(); }
        });
        bottom.addView(play, new LinearLayout.LayoutParams(Ui.dp(220), Ui.dp(52)));

        LinearLayout.LayoutParams blp = new LinearLayout.LayoutParams(MATCH, WRAP);
        blp.topMargin = Ui.dp(12);
        page.addView(bottom, blp);

        content.addView(page, new FrameLayout.LayoutParams(MATCH, MATCH));
        updateCards();
    }

    private EditText field(String value, String hint) {
        EditText e = new EditText(this);
        e.setText(value);
        e.setHint(hint);
        e.setHintTextColor(Ui.DIM);
        e.setTextColor(Ui.TEXT);
        e.setTypeface(Ui.bodyBold);
        e.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        e.setSingleLine(true);
        e.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        e.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_ACTION_DONE);
        return e;
    }

    private void saveNick() {
        if (nickField != null) cfg.nickname = nickField.getText().toString().trim();
        cfg.save();
    }

    private void hideKeyboard() {
        View f = getCurrentFocus();
        if (f != null) {
            InputMethodManager imm = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
            imm.hideSoftInputFromWindow(f.getWindowToken(), 0);
            f.clearFocus();
        }
    }

    /** Карточка сервера: название, описание, онлайн и пинг. */
    private final class ServerCard {
        final int index;
        final LinearLayout view;
        final TextView status;
        final View stripe;

        ServerCard(final int index) {
            this.index = index;
            Config.Server s = cfg.servers.get(index);
            view = new LinearLayout(MainActivity.this);
            view.setOrientation(LinearLayout.HORIZONTAL);

            stripe = new View(MainActivity.this);
            view.addView(stripe, new LinearLayout.LayoutParams(Ui.dp(4), MATCH));

            LinearLayout body = new LinearLayout(MainActivity.this);
            body.setOrientation(LinearLayout.VERTICAL);
            body.setPadding(Ui.dp(14), Ui.dp(11), Ui.dp(14), Ui.dp(11));

            LinearLayout row = new LinearLayout(MainActivity.this);
            row.setOrientation(LinearLayout.HORIZONTAL);
            row.setGravity(Gravity.CENTER_VERTICAL);
            TextView name = Ui.text(MainActivity.this, s.name.toUpperCase(), 17, Ui.TEXT, Ui.heavy);
            name.setLetterSpacing(0.04f);
            row.addView(name, new LinearLayout.LayoutParams(0, WRAP, 1));
            status = Ui.text(MainActivity.this, "…", 11, Ui.MUTED, Ui.bodyBold);
            row.addView(status);
            body.addView(row);

            TextView d = Ui.text(MainActivity.this, s.description, 12, Ui.MUTED, Ui.body);
            d.setPadding(0, Ui.dp(5), 0, 0);
            d.setMaxLines(2);
            body.addView(d);
            view.addView(body, new LinearLayout.LayoutParams(0, WRAP, 1));

            view.setOnClickListener(new View.OnClickListener() {
                @Override public void onClick(View v) {
                    cfg.selected = ServerCard.this.index;
                    cfg.save();
                    updateCards();
                }
            });
            Ui.pressFeedback(view);
        }

        void update() {
            Config.Server s = cfg.servers.get(index);
            boolean sel = cfg.selected == index;
            view.setBackground(Ui.box(sel ? 0xF01B1E23 : 0xCC14161A, sel ? Ui.ORANGE : Ui.LINE, 6, sel ? 1.5f : 1));
            stripe.setBackground(sel ? Ui.box(Ui.ORANGE, 0, 2, 0) : null);
            if (!s.queried) {
                status.setText("проверка…");
                status.setTextColor(Ui.MUTED);
            } else if (s.online) {
                status.setText("●  " + s.players + " / " + s.maxPlayers + "   " + s.ping + " мс");
                status.setTextColor(Ui.GREEN);
            } else {
                status.setText("●  оффлайн");
                status.setTextColor(Ui.RED);
            }
        }
    }

    private void updateCards() {
        for (ServerCard c : cards) c.update();
    }

    private void refreshServers() {
        for (final Config.Server s : cfg.servers) {
            new Thread(new Runnable() {
                @Override public void run() {
                    SampQuery.query(s);
                    ui.post(new Runnable() { @Override public void run() { updateCards(); } });
                }
            }, "query").start();
        }
    }

    // ==================================================================
    //  Кнопка ИГРАТЬ
    // ==================================================================
    private void onPlay() {
        hideKeyboard();
        saveNick();
        String nick = cfg.nickname;
        if (nick.length() == 0) {
            notice("Введи ник", "Ник нужен для входа на сервер. Формат: Имя_Фамилия, например Ivan_Petrov.", null, null);
            return;
        }
        if (cfg.requireRpName && !Config.isRpName(nick)) {
            notice("Неверный ник", "На сервере нужен РП-ник: Имя_Фамилия латиницей, каждое слово с большой буквы. Например: Ivan_Petrov.", null, null);
            return;
        }
        if (!isGtaInstalled()) {
            notice("Нужна GTA: San Andreas",
                    "Для игры нужна официальная GTA: San Andreas для Android из Google Play. Установи её один раз, лаунчер найдёт её сам.",
                    "Открыть Google Play", new Runnable() {
                        @Override public void run() { openGtaInStore(); }
                    });
            return;
        }
        cfg.writeClientConfig(this);
        Config.Server s = cfg.servers.get(cfg.selected);
        notice("Почти готово",
                "Ник " + nick + " и сервер " + s.name + " (" + s.host + ":" + s.port + ") сохранены.\n\n"
                        + "Игровой клиент Bastion для телефона ещё в разработке: сейчас это первая сборка лаунчера.",
                null, null);
    }

    private boolean isGtaInstalled() {
        try {
            PackageInfo pi = getPackageManager().getPackageInfo(GTA_PACKAGE, 0);
            return pi != null;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    private String gtaVersion() {
        try {
            return getPackageManager().getPackageInfo(GTA_PACKAGE, 0).versionName;
        } catch (Exception e) {
            return null;
        }
    }

    private void openGtaInStore() {
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("market://details?id=" + GTA_PACKAGE)));
        } catch (Exception e) {
            startActivity(new Intent(Intent.ACTION_VIEW,
                    Uri.parse("https://play.google.com/store/apps/details?id=" + GTA_PACKAGE)));
        }
    }

    /** Своё окно-сообщение поверх экрана, в стиле лаунчера. */
    private void notice(String title, String message, String action, final Runnable onAction) {
        final FrameLayout dim = new FrameLayout(this);
        dim.setBackgroundColor(0xB3000000);
        dim.setClickable(true);

        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(Ui.dp(22), Ui.dp(18), Ui.dp(22), Ui.dp(18));
        box.setBackground(Ui.box(Ui.PANEL, Ui.LINE, 10, 1));

        TextView t = Ui.text(this, title.toUpperCase(), 17, Ui.TEXT, Ui.heavy);
        t.setLetterSpacing(0.05f);
        box.addView(t);
        View bar = new View(this);
        bar.setBackgroundColor(Ui.ORANGE);
        LinearLayout.LayoutParams bl = new LinearLayout.LayoutParams(Ui.dp(36), Ui.dp(2));
        bl.topMargin = Ui.dp(8);
        bl.bottomMargin = Ui.dp(12);
        box.addView(bar, bl);
        TextView m = Ui.text(this, message, 13, Ui.MUTED, Ui.body);
        m.setLineSpacing(Ui.dp(3), 1f);
        box.addView(m);

        LinearLayout buttons = new LinearLayout(this);
        buttons.setOrientation(LinearLayout.HORIZONTAL);
        buttons.setGravity(Gravity.END);
        final Runnable close = new Runnable() {
            @Override public void run() { root.removeView(dim); }
        };
        TextView ok = Ui.ghostButton(this, action == null ? "Понятно" : "Закрыть");
        ok.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { close.run(); }
        });
        buttons.addView(ok, new LinearLayout.LayoutParams(WRAP, Ui.dp(40)));
        if (action != null) {
            TextView a = Ui.primaryButton(this, action.toUpperCase());
            a.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
            a.setPadding(Ui.dp(18), 0, Ui.dp(18), 0);
            a.setOnClickListener(new View.OnClickListener() {
                @Override public void onClick(View v) { close.run(); onAction.run(); }
            });
            LinearLayout.LayoutParams alp = new LinearLayout.LayoutParams(WRAP, Ui.dp(40));
            alp.leftMargin = Ui.dp(10);
            buttons.addView(a, alp);
        }
        LinearLayout.LayoutParams bp = new LinearLayout.LayoutParams(MATCH, WRAP);
        bp.topMargin = Ui.dp(18);
        box.addView(buttons, bp);

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(Ui.dp(440), WRAP, Gravity.CENTER);
        dim.addView(box, lp);
        root.addView(dim, new FrameLayout.LayoutParams(MATCH, MATCH));
    }

    // ==================================================================
    //  Настройки
    // ==================================================================
    private final List<EditText[]> serverFields = new ArrayList<EditText[]>();

    private void showSettings() {
        content.removeAllViews();
        content.setTag("settings");
        serverFields.clear();

        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(Ui.dp(24), Ui.dp(14), Ui.dp(24), Ui.dp(14));

        LinearLayout top = new LinearLayout(this);
        top.setOrientation(LinearLayout.HORIZONTAL);
        top.setGravity(Gravity.CENTER_VERTICAL);
        TextView title = Ui.text(this, "НАСТРОЙКИ", 26, Ui.TEXT, Ui.heavy);
        title.setShadowLayer(Ui.dp(4), 0, Ui.dp(2), 0xCC000000);
        top.addView(title, new LinearLayout.LayoutParams(0, WRAP, 1));
        TextView back = Ui.primaryButton(this, "ГОТОВО");
        back.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        back.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { applySettings(); showMain(); }
        });
        top.addView(back, new LinearLayout.LayoutParams(WRAP, Ui.dp(40)));
        page.addView(top);

        View rule = new View(this);
        rule.setBackground(new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,
                new int[]{Ui.ORANGE, 0x55FF7A1A, 0x00FF7A1A}));
        LinearLayout.LayoutParams rlp = new LinearLayout.LayoutParams(MATCH, Ui.dp(2));
        rlp.topMargin = Ui.dp(10);
        page.addView(rule, rlp);

        ScrollView sv = new ScrollView(this);
        LinearLayout cols = new LinearLayout(this);
        cols.setOrientation(LinearLayout.HORIZONTAL);
        cols.setPadding(0, Ui.dp(12), 0, Ui.dp(12));

        // левая колонка: графика и интерфейс
        LinearLayout c1 = new LinearLayout(this);
        c1.setOrientation(LinearLayout.VERTICAL);
        c1.addView(Ui.caption(this, "Графика"));
        c1.addView(option("Ограничение FPS", new String[]{"30", "60", "90", "120"}, new int[]{30, 60, 90, 120},
                cfg.fps, new Pick() { @Override public void set(int v) { cfg.fps = v; } }));
        c1.addView(option("Качество", new String[]{"Низкое", "Среднее", "Высокое"}, new int[]{0, 1, 2},
                cfg.quality, new Pick() { @Override public void set(int v) { cfg.quality = v; } }));
        TextView fpsNote = Ui.text(this, "90 и 120 FPS — только если экран телефона это поддерживает.", 11, Ui.DIM, Ui.body);
        fpsNote.setPadding(0, Ui.dp(6), 0, Ui.dp(14));
        c1.addView(fpsNote);
        c1.addView(Ui.caption(this, "Интерфейс"));
        c1.addView(option("Строк чата", new String[]{"10", "15", "20"}, new int[]{10, 15, 20},
                cfg.chatLines, new Pick() { @Override public void set(int v) { cfg.chatLines = v; } }));
        c1.addView(option("Размер кнопок", new String[]{"S", "M", "L"}, new int[]{0, 1, 2},
                cfg.buttonSize, new Pick() { @Override public void set(int v) { cfg.buttonSize = v; } }));
        LinearLayout.LayoutParams c1p = new LinearLayout.LayoutParams(0, WRAP, 1);
        c1p.rightMargin = Ui.dp(20);
        cols.addView(c1, c1p);

        // правая колонка: сервера и игра
        LinearLayout c2 = new LinearLayout(this);
        c2.setOrientation(LinearLayout.VERTICAL);
        c2.addView(Ui.caption(this, "Адреса серверов"));
        for (Config.Server s : cfg.servers) {
            LinearLayout row = new LinearLayout(this);
            row.setOrientation(LinearLayout.HORIZONTAL);
            row.setGravity(Gravity.CENTER_VERTICAL);
            row.setPadding(0, Ui.dp(8), 0, 0);
            TextView n = Ui.text(this, s.name, 12, Ui.TEXT, Ui.bodyBold);
            row.addView(n, new LinearLayout.LayoutParams(Ui.dp(100), WRAP));
            EditText host = boxedField(s.host, "адрес");
            row.addView(host, new LinearLayout.LayoutParams(0, Ui.dp(38), 1));
            EditText port = boxedField(String.valueOf(s.port), "порт");
            port.setInputType(InputType.TYPE_CLASS_NUMBER);
            LinearLayout.LayoutParams pp = new LinearLayout.LayoutParams(Ui.dp(76), Ui.dp(38));
            pp.leftMargin = Ui.dp(6);
            row.addView(port, pp);
            serverFields.add(new EditText[]{host, port});
            c2.addView(row);
        }
        TextView hint = Ui.text(this, "Дома: адрес компьютера с сервером в той же Wi-Fi сети (например 192.168.1.10). 127.0.0.1 на телефоне — это сам телефон.", 11, Ui.DIM, Ui.body);
        hint.setPadding(0, Ui.dp(6), 0, 0);
        hint.setLineSpacing(Ui.dp(2), 1f);
        c2.addView(hint);
        TextView reset = Ui.ghostButton(this, "Вернуть адреса по умолчанию");
        reset.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        reset.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                cfg.resetServers(MainActivity.this);
                for (Config.Server s : cfg.servers) s.queried = false;
                showSettings();
            }
        });
        LinearLayout.LayoutParams rp = new LinearLayout.LayoutParams(WRAP, Ui.dp(32));
        rp.topMargin = Ui.dp(8);
        rp.bottomMargin = Ui.dp(14);
        c2.addView(reset, rp);

        c2.addView(Ui.caption(this, "Игра"));
        String ver = gtaVersion();
        TextView gta = Ui.text(this, ver != null ? "●  GTA: San Andreas найдена (версия " + ver + ")"
                : "●  GTA: San Andreas не установлена", 12, ver != null ? Ui.GREEN : Ui.RED, Ui.bodyBold);
        gta.setPadding(0, Ui.dp(8), 0, 0);
        c2.addView(gta);
        TextView about = Ui.text(this, "Bastion Online Launcher " + appVersion(), 10, Ui.DIM, Ui.body);
        about.setPadding(0, Ui.dp(10), 0, 0);
        c2.addView(about);
        cols.addView(c2, new LinearLayout.LayoutParams(0, WRAP, 1));

        sv.addView(cols);
        page.addView(sv, new LinearLayout.LayoutParams(MATCH, 0, 1));
        content.addView(page, new FrameLayout.LayoutParams(MATCH, MATCH));
    }

    private String appVersion() {
        try { return getPackageManager().getPackageInfo(getPackageName(), 0).versionName; }
        catch (Exception e) { return ""; }
    }

    private EditText boxedField(String v, String hint) {
        EditText e = field(v, hint);
        e.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        e.setBackground(Ui.box(0xE60E1013, Ui.LINE, 5, 1));
        e.setPadding(Ui.dp(10), 0, Ui.dp(10), 0);
        return e;
    }

    private void applySettings() {
        hideKeyboard();
        for (int i = 0; i < serverFields.size() && i < cfg.servers.size(); i++) {
            Config.Server s = cfg.servers.get(i);
            String h = serverFields.get(i)[0].getText().toString().trim();
            int p;
            try { p = Integer.parseInt(serverFields.get(i)[1].getText().toString().trim()); }
            catch (NumberFormatException e) { p = s.port; }
            if (h.length() > 0 && (!h.equals(s.host) || p != s.port)) {
                s.host = h;
                s.port = p;
                s.queried = false;
            }
        }
        cfg.save();
        refreshServers();
    }

    private interface Pick { void set(int v); }

    /** Строка «название + переключатель из нескольких кнопок». */
    private View option(String label, String[] names, final int[] values, int current, final Pick pick) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, Ui.dp(8), 0, 0);
        TextView l = Ui.text(this, label, 13, Ui.TEXT, Ui.body);
        row.addView(l, new LinearLayout.LayoutParams(0, WRAP, 1));

        final LinearLayout seg = new LinearLayout(this);
        seg.setOrientation(LinearLayout.HORIZONTAL);
        seg.setBackground(Ui.box(0xE60E1013, Ui.LINE, 6, 1));
        seg.setPadding(Ui.dp(3), Ui.dp(3), Ui.dp(3), Ui.dp(3));
        final List<TextView> items = new ArrayList<TextView>();
        for (int i = 0; i < names.length; i++) {
            final TextView t = Ui.text(this, names[i], 12, Ui.MUTED, Ui.heavy);
            t.setGravity(Gravity.CENTER);
            t.setPadding(Ui.dp(12), 0, Ui.dp(12), 0);
            t.setMinWidth(Ui.dp(44));
            items.add(t);
            final int idx = i;
            t.setOnClickListener(new View.OnClickListener() {
                @Override public void onClick(View v) {
                    pick.set(values[idx]);
                    paintSegments(items, idx);
                }
            });
            seg.addView(t, new LinearLayout.LayoutParams(WRAP, Ui.dp(30)));
        }
        int sel = 0;
        for (int i = 0; i < values.length; i++) if (values[i] == current) sel = i;
        paintSegments(items, sel);
        row.addView(seg);
        return row;
    }

    private void paintSegments(List<TextView> items, int sel) {
        for (int i = 0; i < items.size(); i++) {
            TextView t = items.get(i);
            if (i == sel) {
                t.setBackground(Ui.box(Ui.ORANGE, 0, 4, 0));
                t.setTextColor(Color.WHITE);
            } else {
                t.setBackground(null);
                t.setTextColor(Ui.MUTED);
            }
        }
    }
}
