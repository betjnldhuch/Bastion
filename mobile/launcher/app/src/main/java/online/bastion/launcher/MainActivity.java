package online.bastion.launcher;

import android.app.Activity;
import android.app.ProgressDialog;
import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
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

    private static final int MATCH = ViewGroup.LayoutParams.MATCH_PARENT;
    private static final int WRAP = ViewGroup.LayoutParams.WRAP_CONTENT;

    private Config cfg;
    private FrameLayout root;
    private FrameLayout content;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private final List<ServerCard> cards = new ArrayList<ServerCard>();
    private EditText nickField;
    private TextView playButton;
    private java.io.File clientConfig;
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
        Diagnostics.installCrashLog(this);
        Ui.init(this);
        cfg = new Config(this);

        root = new FrameLayout(this);
        root.addView(new Ui.Background(this), new FrameLayout.LayoutParams(MATCH, MATCH));
        content = new FrameLayout(this);
        root.addView(content, new FrameLayout.LayoutParams(MATCH, MATCH));
        setContentView(root);
        showMain();
        loadRemote();
    }

    /** Новости, список серверов и проверка версии из сети (Remote). Раз за запуск. */
    private void loadRemote() {
        new Thread(new Runnable() {
            @Override public void run() {
                final boolean changed = Remote.refresh(getApplicationContext());
                final Remote.Update update = Remote.checkUpdate();
                ui.post(new Runnable() {
                    @Override public void run() {
                        if (isFinishing()) return;
                        // перерисовываем только спокойный главный экран: без окон поверх и не в настройках
                        if (changed && "main".equals(content.getTag()) && root.getChildCount() <= 2) {
                            saveNick();
                            cfg.reload(MainActivity.this);
                            showMain();
                            updatePlayButton();
                            refreshServers();
                        }
                        if (update != null) offerUpdate(update);
                    }
                });
            }
        }).start();
    }

    private void offerUpdate(final Remote.Update update) {
        String text = "Вышла новая версия лаунчера (сборка " + update.versionCode + ", у тебя " + BuildConfig.VERSION_CODE + ").";
        if (update.notes.length() > 0) text += "\n\n" + update.notes;
        text += "\n\nСкачай APK и установи поверх: игра и настройки останутся.";
        notice("Обновление", text, "Скачать", new Runnable() {
            @Override public void run() {
                try {
                    startActivity(new Intent(Intent.ACTION_VIEW, android.net.Uri.parse(update.url)));
                } catch (Exception e) {
                    notice("Обновление", "Не получилось открыть ссылку:\n" + update.url, null, null);
                }
            }
        });
    }

    @Override protected void onResume() {
        super.onResume();
        resumed = true;
        updatePlayButton();
        showGameCrashIfAny();
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
        if (root.getChildCount() > 2) { root.removeViewAt(root.getChildCount() - 1); return; }
        if ("settings".equals(content.getTag())) { applySettings(); showMain(); return; }
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
        page.setPadding(Ui.dp(24), Ui.dp(10), Ui.dp(24), Ui.dp(10));

        LinearLayout top = new LinearLayout(this);
        top.setOrientation(LinearLayout.HORIZONTAL);
        top.setGravity(Gravity.CENTER_VERTICAL);

        View mark = new View(this);
        mark.setBackgroundColor(Ui.ORANGE);
        LinearLayout.LayoutParams markLp = new LinearLayout.LayoutParams(Ui.dp(7), Ui.dp(7));
        markLp.rightMargin = Ui.dp(10);
        top.addView(mark, markLp);

        TextView brand = Ui.text(this, "BASTION ONLINE", 11, Ui.MUTED, Ui.heavy);
        brand.setLetterSpacing(0.22f);
        top.addView(brand, new LinearLayout.LayoutParams(0, WRAP, 1));

        TextView settings = Ui.ghostButton(this, "Настройки");
        settings.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { saveNick(); showSettings(); }
        });
        top.addView(settings, new LinearLayout.LayoutParams(WRAP, Ui.dp(34)));
        page.addView(top, new LinearLayout.LayoutParams(MATCH, Ui.dp(38)));

        View topRule = new View(this);
        topRule.setBackground(new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,
                new int[]{Ui.ORANGE, 0x66808080, 0x00808080}));
        LinearLayout.LayoutParams trp = new LinearLayout.LayoutParams(MATCH, Ui.dp(1));
        trp.topMargin = Ui.dp(5);
        trp.bottomMargin = Ui.dp(12);
        page.addView(topRule, trp);

        LinearLayout middle = new LinearLayout(this);
        middle.setOrientation(LinearLayout.HORIZONTAL);

        LinearLayout left = new LinearLayout(this);
        left.setOrientation(LinearLayout.VERTICAL);

        TextView hero = Ui.text(this, "BASTION", 46, Ui.TEXT, Ui.heavy);
        hero.setLetterSpacing(-0.035f);
        hero.setShadowLayer(Ui.dp(5), 0, Ui.dp(3), 0x99000000);
        left.addView(hero, new LinearLayout.LayoutParams(MATCH, WRAP));

        LinearLayout subtitle = new LinearLayout(this);
        subtitle.setOrientation(LinearLayout.HORIZONTAL);
        subtitle.setGravity(Gravity.CENTER_VERTICAL);
        View orangeDash = new View(this);
        orangeDash.setBackgroundColor(Ui.ORANGE);
        LinearLayout.LayoutParams dashLp = new LinearLayout.LayoutParams(Ui.dp(34), Ui.dp(3));
        dashLp.rightMargin = Ui.dp(12);
        subtitle.addView(orangeDash, dashLp);
        TextView tagline = Ui.text(this, "ONLINE  ·  SAN ANDREAS MULTIPLAYER", 10, Ui.MUTED, Ui.heavy);
        tagline.setLetterSpacing(0.19f);
        subtitle.addView(tagline);
        LinearLayout.LayoutParams subLp = new LinearLayout.LayoutParams(MATCH, WRAP);
        subLp.topMargin = Ui.dp(9);
        subLp.bottomMargin = Ui.dp(24);
        left.addView(subtitle, subLp);

        left.addView(Ui.caption(this, "Новости"));
        ScrollView newsScroll = new ScrollView(this);
        newsScroll.setVerticalScrollBarEnabled(false);
        LinearLayout newsList = new LinearLayout(this);
        newsList.setOrientation(LinearLayout.VERTICAL);
        newsList.setPadding(0, Ui.dp(10), Ui.dp(12), Ui.dp(4));

        if (cfg.news.isEmpty()) {
            newsList.addView(Ui.text(this, "Пока новостей нет.", 13, Ui.MUTED, Ui.body));
        }
        for (int i = 0; i < cfg.news.size(); i++) {
            final String[] n = cfg.news.get(i);
            LinearLayout item = new LinearLayout(this);
            item.setOrientation(LinearLayout.HORIZONTAL);
            item.setPadding(0, i == 0 ? 0 : Ui.dp(13), 0, Ui.dp(7));

            TextView date = Ui.text(this, n[0], 10, Ui.DIM, Ui.heavy);
            LinearLayout.LayoutParams dateLp = new LinearLayout.LayoutParams(Ui.dp(76), WRAP);
            dateLp.topMargin = Ui.dp(4);
            item.addView(date, dateLp);

            LinearLayout copy = new LinearLayout(this);
            copy.setOrientation(LinearLayout.VERTICAL);
            TextView headline = Ui.text(this, n[1], 15, Ui.TEXT, Ui.bodyBold);
            copy.addView(headline);
            TextView body = Ui.text(this, n[2], 12, Ui.MUTED, Ui.body);
            body.setLineSpacing(Ui.dp(2), 1f);
            body.setPadding(0, Ui.dp(4), 0, 0);
            copy.addView(body);
            item.addView(copy, new LinearLayout.LayoutParams(0, WRAP, 1));
            newsList.addView(item);
        }
        newsScroll.addView(newsList);
        LinearLayout.LayoutParams newsLp = new LinearLayout.LayoutParams(MATCH, 0, 1);
        newsLp.topMargin = Ui.dp(2);
        left.addView(newsScroll, newsLp);

        LinearLayout.LayoutParams leftLp = new LinearLayout.LayoutParams(0, MATCH, 1.18f);
        leftLp.rightMargin = Ui.dp(22);
        middle.addView(left, leftLp);

        LinearLayout right = new LinearLayout(this);
        right.setOrientation(LinearLayout.VERTICAL);
        right.addView(Ui.caption(this, "Выбери сервер"));

        ScrollView serverScroll = new ScrollView(this);
        serverScroll.setVerticalScrollBarEnabled(false);
        LinearLayout serverList = new LinearLayout(this);
        serverList.setOrientation(LinearLayout.VERTICAL);
        for (int i = 0; i < cfg.servers.size(); i++) {
            ServerCard card = new ServerCard(i);
            cards.add(card);
            LinearLayout.LayoutParams cardLp = new LinearLayout.LayoutParams(MATCH, WRAP);
            cardLp.topMargin = Ui.dp(9);
            serverList.addView(card.view, cardLp);
        }
        serverScroll.addView(serverList);
        LinearLayout.LayoutParams serverScrollLp = new LinearLayout.LayoutParams(MATCH, 0, 1);
        serverScrollLp.topMargin = Ui.dp(8);
        right.addView(serverScroll, serverScrollLp);
        middle.addView(right, new LinearLayout.LayoutParams(0, MATCH, 0.92f));

        page.addView(middle, new LinearLayout.LayoutParams(MATCH, 0, 1));
        page.addView(buildFooter(), new LinearLayout.LayoutParams(MATCH, WRAP));
        content.addView(page, new FrameLayout.LayoutParams(MATCH, MATCH));
        updateCards();
    }

    /** Нижняя панель с ником и большой кнопкой запуска. */
    private LinearLayout buildFooter() {
        LinearLayout footer = new LinearLayout(this);
        footer.setOrientation(LinearLayout.VERTICAL);

        FrameLayout rule = new FrameLayout(this);
        View base = new View(this);
        base.setBackgroundColor(Ui.LINE);
        rule.addView(base, new FrameLayout.LayoutParams(MATCH, Ui.dp(1), Gravity.TOP));
        View accent = new View(this);
        accent.setBackgroundColor(Ui.ORANGE);
        rule.addView(accent, new FrameLayout.LayoutParams(Ui.dp(110), Ui.dp(2), Gravity.TOP | Gravity.LEFT));
        footer.addView(rule, new LinearLayout.LayoutParams(MATCH, Ui.dp(2)));

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(Ui.dp(14), Ui.dp(12), Ui.dp(14), 0);

        LinearLayout nick = new LinearLayout(this);
        nick.setOrientation(LinearLayout.VERTICAL);
        TextView label = Ui.text(this, "ТВОЙ НИК", 10, Ui.MUTED, Ui.heavy);
        label.setLetterSpacing(0.22f);
        nick.addView(label);
        nickField = field(cfg.nickname, "Имя_Фамилия");
        nickField.setBackground(null);
        nickField.setPadding(0, Ui.dp(5), 0, Ui.dp(3));
        nick.addView(nickField, new LinearLayout.LayoutParams(MATCH, WRAP));
        View underline = new View(this);
        underline.setBackgroundColor(Ui.LINE);
        nick.addView(underline, new LinearLayout.LayoutParams(MATCH, Ui.dp(1)));

        LinearLayout.LayoutParams nickLp = new LinearLayout.LayoutParams(0, WRAP, 1);
        nickLp.rightMargin = Ui.dp(18);
        row.addView(nick, nickLp);

        TextView play = Ui.primaryButton(this, "ИГРАТЬ");
        play.setTextColor(Color.BLACK);
        play.setLetterSpacing(0.02f);
        playButton = play;
        updatePlayButton();
        play.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { onPlay(); }
        });
        row.addView(play, new LinearLayout.LayoutParams(Ui.dp(210), Ui.dp(58)));
        footer.addView(row);
        return footer;
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
        final TextView statusMain;
        final TextView statusSub;
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
            body.setPadding(Ui.dp(14), Ui.dp(12), Ui.dp(14), Ui.dp(9));

            LinearLayout row = new LinearLayout(MainActivity.this);
            row.setOrientation(LinearLayout.HORIZONTAL);
            row.setGravity(Gravity.CENTER_VERTICAL);
            TextView name = Ui.text(MainActivity.this, s.name.toUpperCase(), 17, Ui.TEXT, Ui.heavy);
            name.setLetterSpacing(0.025f);
            row.addView(name, new LinearLayout.LayoutParams(0, WRAP, 1));

            LinearLayout state = new LinearLayout(MainActivity.this);
            state.setOrientation(LinearLayout.VERTICAL);
            state.setGravity(Gravity.RIGHT | Gravity.CENTER_VERTICAL);
            statusMain = Ui.text(MainActivity.this, "…", 16, Ui.TEXT, Ui.heavy);
            statusMain.setGravity(Gravity.RIGHT);
            statusSub = Ui.text(MainActivity.this, "ПРОВЕРКА", 9, Ui.MUTED, Ui.heavy);
            statusSub.setGravity(Gravity.RIGHT);
            statusSub.setLetterSpacing(0.05f);
            state.addView(statusMain);
            state.addView(statusSub);
            row.addView(state);
            body.addView(row);

            TextView description = Ui.text(MainActivity.this, s.description, 12, Ui.MUTED, Ui.body);
            description.setPadding(0, Ui.dp(6), 0, Ui.dp(8));
            description.setMaxLines(2);
            body.addView(description);

            View bottomLine = new View(MainActivity.this);
            bottomLine.setBackgroundColor(Ui.LINE);
            body.addView(bottomLine, new LinearLayout.LayoutParams(MATCH, Ui.dp(1)));
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
            boolean selected = cfg.selected == index;
            view.setBackground(Ui.box(selected ? 0xF01B1E23 : 0xCC14161A,
                    selected ? Ui.ORANGE : Ui.LINE, 2, selected ? 1.5f : 1));
            stripe.setBackground(selected ? Ui.box(Ui.ORANGE, 0, 1, 0) : null);

            if (!s.queried) {
                statusMain.setText("…");
                statusSub.setText("ПРОВЕРКА");
                statusMain.setTextColor(Ui.MUTED);
                statusSub.setTextColor(Ui.MUTED);
            } else if (s.online) {
                statusMain.setText(s.players + " / " + s.maxPlayers);
                statusSub.setText("ОНЛАЙН · " + s.ping + " МС");
                statusMain.setTextColor(Ui.TEXT);
                statusSub.setTextColor(Ui.ORANGE);
            } else {
                statusMain.setText("OFF");
                statusSub.setText("НЕ ОТВЕЧАЕТ");
                statusMain.setTextColor(Ui.DIM);
                statusSub.setTextColor(Ui.DIM);
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
    /** «ИГРАТЬ», если файлы игры установлены, иначе «УСТАНОВИТЬ». */
    private void updatePlayButton() {
        if (playButton == null) return;
        boolean installed = ClientInstaller.isGameInstalled(this);
        playButton.setText(installed ? "ИГРАТЬ" : "УСТАНОВИТЬ");
        playButton.setTextSize(TypedValue.COMPLEX_UNIT_SP, installed ? 22 : 18);
    }

    private void onPlay() {
        hideKeyboard();
        saveNick();
        if (!ClientInstaller.isGameInstalled(this)) {
            notice("Нужно установить игру",
                    "Файлы игры ещё не скачаны. Лаунчер скачает архив с сервера Bastion, проверит его и распакует.\n\nАдрес: "
                            + ClientInstaller.GAME_ARCHIVE_URL,
                    "Скачать", new Runnable() {
                        @Override public void run() {
                            startArchiveInstall(ClientInstaller.GAME_ARCHIVE_URL, ClientInstaller.GAME_ARCHIVE_MD5);
                        }
                    });
            return;
        }
        String nick = cfg.nickname;
        if (nick.length() == 0) {
            notice("Введи ник", "Ник нужен для входа на сервер. Формат: Имя_Фамилия, например Ivan_Petrov.", null, null);
            return;
        }
        if (cfg.requireRpName && !Config.isRpName(nick)) {
            notice("Неверный ник", "На сервере нужен РП-ник: Имя_Фамилия латиницей, каждое слово с большой буквы. Например: Ivan_Petrov.", null, null);
            return;
        }
        clientConfig = cfg.writeClientConfig(this);
        java.io.File storage = ClientInstaller.findSampStorageRoot(this);
        if (storage == null) {
            notice("Файлы игры повреждены", "Не найдена папка SAMP в установленной игре. Переустанови игру в настройках.", null, null);
            return;
        }
        List<String> missing = ClientInstaller.prepareForLaunch(this, storage);
        if (!missing.isEmpty()) {
            StringBuilder list = new StringBuilder();
            for (String m : missing) list.append("• ").append(m).append('\n');
            notice("Не хватает файлов игры",
                    "В установленном архиве нет файлов, без которых SAMP-Mobile GTA-2.11 закрывается при запуске:\n\n"
                            + list + "\nНужен кэш именно для SAMP-Mobile GTA-2.11 (содержимое папки GTA/). "
                            + "Замени архив на сервере и переустанови игру в настройках.",
                    null, null);
            return;
        }
        launchPrepared();
    }

    /** Пишет SAMP/settings.ini и открывает клиент (файлы игры уже проверены). */
    private void launchPrepared() {
        java.io.File storage = ClientInstaller.findSampStorageRoot(this);
        if (storage == null) return;
        String nick = cfg.nickname;
        Config.Server server = cfg.servers.get(cfg.selected);
        try {
            ClientInstaller.writeSampSettings(storage, nick, server.host, server.port, cfg.fps, cfg.chatLines);
        } catch (Exception e) {
            notice("Не удалось подготовить запуск", "Ошибка записи SAMP/settings.ini: " + e.getMessage(), null, null);
            return;
        }
        openSampClient();
    }

    /** Opens the built-in ZIP installer. The archive is downloaded and unpacked inside app storage. */
    private void installClientAssets() {
        final EditText urlField = new EditText(this);
        urlField.setSingleLine(true);
        urlField.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        urlField.setText(ClientInstaller.GAME_ARCHIVE_URL);
        urlField.setHint("http://адрес:8000/game.zip");
        urlField.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        urlField.setPadding(Ui.dp(12), Ui.dp(10), Ui.dp(12), Ui.dp(10));
        urlField.setBackground(Ui.box(0xE60E1013, Ui.LINE, 5, 1));

        LinearLayout form = new LinearLayout(this);
        form.setOrientation(LinearLayout.VERTICAL);
        form.setPadding(Ui.dp(20), Ui.dp(8), Ui.dp(20), 0);
        TextView hint = Ui.text(this,
                "Ссылка на ZIP-архив игры. По умолчанию — сервер Bastion (архив проверяется по MD5). Если сервер переехал, впиши новый адрес: тогда MD5 не проверяется.\n\nИли выбери ZIP, уже скачанный на телефон (например, кэш SAMP-Mobile GTA-2.11 из браузера). Неполный архив не установится — лаунчер покажет, каких файлов нет.",
                12, Ui.MUTED, Ui.body);
        hint.setPadding(0, 0, 0, Ui.dp(10));
        form.addView(hint);
        form.addView(urlField, new LinearLayout.LayoutParams(MATCH, Ui.dp(44)));

        new AlertDialog.Builder(this)
                .setTitle("Установка файлов клиента")
                .setView(form)
                .setNegativeButton("Отмена", null)
                .setNeutralButton("ZIP НА ТЕЛЕФОНЕ", new android.content.DialogInterface.OnClickListener() {
                    @Override public void onClick(android.content.DialogInterface dialog, int which) { pickLocalArchive(); }
                })
                .setPositiveButton("СКАЧАТЬ И УСТАНОВИТЬ", new android.content.DialogInterface.OnClickListener() {
                    @Override public void onClick(android.content.DialogInterface dialog, int which) {
                        String address = urlField.getText().toString().trim();
                        if (address.length() == 0) {
                            notice("Нет ссылки", "Вставь прямую ссылку на ZIP-архив.", null, null);
                            return;
                        }
                        String md5 = address.equals(ClientInstaller.GAME_ARCHIVE_URL) ? ClientInstaller.GAME_ARCHIVE_MD5 : "";
                        startArchiveInstall(address, md5);
                    }
                })
                .show();
    }

    private static final int PICK_ARCHIVE = 4711;

    /** Выбор ZIP-архива игры из памяти телефона (Загрузки, Google Drive и т. д.). */
    private void pickLocalArchive() {
        try {
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            pick.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"application/zip", "application/x-zip-compressed", "application/octet-stream"});
            startActivityForResult(pick, PICK_ARCHIVE);
        } catch (Exception e) {
            notice("Нет доступа к файлам", "Не удалось открыть выбор файла: " + e.getMessage(), null, null);
        }
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == PICK_ARCHIVE && resultCode == RESULT_OK && data != null && data.getData() != null)
            startArchiveInstall(data.getData().toString(), "");
    }

    private void startArchiveInstall(String address, String md5) {
        final ProgressDialog progress = new ProgressDialog(this);
        progress.setTitle("Установка игры Bastion");
        progress.setMessage("Подготовка загрузки…");
        progress.setIndeterminate(true);
        progress.setCancelable(false);
        progress.show();

        new ClientInstaller(this).installArchive(address, md5, new ClientInstaller.Callback() {
            @Override public void onProgress(final String message) {
                ui.post(new Runnable() {
                    @Override public void run() {
                        if (progress.isShowing()) progress.setMessage(message);
                    }
                });
            }

            @Override public void onComplete(int count, java.io.File directory) {
                if (progress.isShowing()) progress.dismiss();
                updatePlayButton();
                String warnings = ClientInstaller.installWarnings(MainActivity.this);
                notice("Установка завершена",
                        "Установлено файлов: " + count + "\nПапка: " + directory.getAbsolutePath()
                                + (warnings.length() > 0 ? "\n\nВнимание: " + warnings
                                + ". Если игра не запустится, проверь, что это кэш SAMP-Mobile GTA-2.11." : ""),
                        null, null);
            }

            @Override public void onError(String message) {
                if (progress.isShowing()) progress.dismiss();
                updatePlayButton();
                notice("Ошибка установки", message, null, null);
            }
        });
    }

    /** Если игра упала после последнего запуска — показывает причину и даёт её скопировать. */
    private void showGameCrashIfAny() {
        final String report = GameCrashReport.collectNew(this);
        if (report == null) return;
        String brief = report.length() > 900 ? report.substring(0, 900) + "…" : report;
        notice("Игра закрылась с ошибкой",
                brief + "\n\nПолный отчёт (стек, библиотеки, logcat) — кнопка ниже: его можно скопировать или отправить.",
                "Полный отчёт", new Runnable() {
                    @Override public void run() { runDiagnostics(); }
                });
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
        cards.clear();

        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(Ui.dp(24), Ui.dp(10), Ui.dp(24), Ui.dp(10));

        LinearLayout top = new LinearLayout(this);
        top.setOrientation(LinearLayout.HORIZONTAL);
        top.setGravity(Gravity.CENTER_VERTICAL);
        TextView title = Ui.text(this, "НАСТРОЙКИ", 17, Ui.MUTED, Ui.heavy);
        title.setLetterSpacing(0.2f);
        top.addView(title, new LinearLayout.LayoutParams(0, WRAP, 1));
        TextView done = Ui.ghostButton(this, "Готово");
        done.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { applySettings(); showMain(); }
        });
        top.addView(done, new LinearLayout.LayoutParams(WRAP, Ui.dp(34)));
        page.addView(top, new LinearLayout.LayoutParams(MATCH, Ui.dp(38)));

        View rule = new View(this);
        rule.setBackground(new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,
                new int[]{Ui.ORANGE, 0x66808080, 0x00808080}));
        LinearLayout.LayoutParams ruleLp = new LinearLayout.LayoutParams(MATCH, Ui.dp(1));
        ruleLp.topMargin = Ui.dp(5);
        ruleLp.bottomMargin = Ui.dp(10);
        page.addView(rule, ruleLp);

        ScrollView all = new ScrollView(this);
        all.setVerticalScrollBarEnabled(false);
        LinearLayout columns = new LinearLayout(this);
        columns.setOrientation(LinearLayout.HORIZONTAL);
        columns.setPadding(0, Ui.dp(2), 0, Ui.dp(12));

        LinearLayout leftPanel = new LinearLayout(this);
        leftPanel.setOrientation(LinearLayout.VERTICAL);
        leftPanel.setPadding(Ui.dp(16), Ui.dp(14), Ui.dp(16), Ui.dp(14));
        leftPanel.setBackground(Ui.box(Ui.PANEL, Ui.LINE, 2, 1));
        leftPanel.addView(Ui.caption(this, "Игра"));

        leftPanel.addView(option("Лимит FPS", new String[]{"30", "60", "90", "120"},
                new int[]{30, 60, 90, 120}, cfg.fps,
                new Pick() { @Override public void set(int v) { cfg.fps = v; } }));
        leftPanel.addView(option("Качество графики", new String[]{"Низкое", "Среднее", "Высокое"},
                new int[]{0, 1, 2}, cfg.quality,
                new Pick() { @Override public void set(int v) { cfg.quality = v; } }));
        TextView fpsNote = Ui.text(this, "Высокие значения FPS доступны не на каждом экране.", 10, Ui.DIM, Ui.body);
        fpsNote.setPadding(0, Ui.dp(5), 0, Ui.dp(12));
        leftPanel.addView(fpsNote);

        leftPanel.addView(Ui.caption(this, "Интерфейс"));
        leftPanel.addView(option("Строк в чате", new String[]{"10", "15", "20"},
                new int[]{10, 15, 20}, cfg.chatLines,
                new Pick() { @Override public void set(int v) { cfg.chatLines = v; } }));
        leftPanel.addView(option("Размер кнопок", new String[]{"S", "M", "L"},
                new int[]{0, 1, 2}, cfg.buttonSize,
                new Pick() { @Override public void set(int v) { cfg.buttonSize = v; } }));

        TextView diagnostics = Ui.ghostButton(this, "Диагностика");
        diagnostics.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { runDiagnostics(); }
        });
        leftPanel.addView(diagnostics, new LinearLayout.LayoutParams(WRAP, Ui.dp(34)));

        TextView install = Ui.primaryButton(this, "УСТАНОВИТЬ / ОБНОВИТЬ ИГРУ");
        install.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        install.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { installClientAssets(); }
        });
        LinearLayout.LayoutParams installLp = new LinearLayout.LayoutParams(MATCH, Ui.dp(40));
        installLp.topMargin = Ui.dp(9);
        leftPanel.addView(install, installLp);

        LinearLayout.LayoutParams leftLp = new LinearLayout.LayoutParams(0, WRAP, 1.08f);
        leftLp.rightMargin = Ui.dp(16);
        columns.addView(leftPanel, leftLp);

        LinearLayout rightPanel = new LinearLayout(this);
        rightPanel.setOrientation(LinearLayout.VERTICAL);
        rightPanel.addView(Ui.caption(this, "Выбери сервер"));
        for (int i = 0; i < cfg.servers.size(); i++) {
            ServerCard card = new ServerCard(i);
            cards.add(card);
            LinearLayout.LayoutParams cardLp = new LinearLayout.LayoutParams(MATCH, WRAP);
            cardLp.topMargin = Ui.dp(8);
            rightPanel.addView(card.view, cardLp);
        }

        TextView addresses = Ui.caption(this, "Адреса серверов");
        LinearLayout.LayoutParams addressesLp = new LinearLayout.LayoutParams(MATCH, WRAP);
        addressesLp.topMargin = Ui.dp(14);
        rightPanel.addView(addresses, addressesLp);

        for (Config.Server server : cfg.servers) {
            LinearLayout addressRow = new LinearLayout(this);
            addressRow.setOrientation(LinearLayout.HORIZONTAL);
            addressRow.setGravity(Gravity.CENTER_VERTICAL);
            addressRow.setPadding(0, Ui.dp(7), 0, 0);

            TextView name = Ui.text(this, server.name.toUpperCase(), 10, Ui.TEXT, Ui.heavy);
            addressRow.addView(name, new LinearLayout.LayoutParams(Ui.dp(92), WRAP));

            EditText host = boxedField(server.host, "адрес");
            addressRow.addView(host, new LinearLayout.LayoutParams(0, Ui.dp(36), 1));

            EditText port = boxedField(String.valueOf(server.port), "порт");
            port.setInputType(InputType.TYPE_CLASS_NUMBER);
            LinearLayout.LayoutParams portLp = new LinearLayout.LayoutParams(Ui.dp(62), Ui.dp(36));
            portLp.leftMargin = Ui.dp(6);
            addressRow.addView(port, portLp);
            serverFields.add(new EditText[]{host, port});
            rightPanel.addView(addressRow);
        }

        TextView hint = Ui.text(this,
                "Для домашнего теста укажи IP компьютера в своей Wi-Fi сети.",
                10, Ui.DIM, Ui.body);
        hint.setPadding(0, Ui.dp(6), 0, 0);
        rightPanel.addView(hint);

        TextView reset = Ui.ghostButton(this, "Вернуть адреса по умолчанию");
        reset.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9);
        reset.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                cfg.resetServers(MainActivity.this);
                for (Config.Server server : cfg.servers) server.queried = false;
                showSettings();
            }
        });
        LinearLayout.LayoutParams resetLp = new LinearLayout.LayoutParams(WRAP, Ui.dp(32));
        resetLp.topMargin = Ui.dp(7);
        rightPanel.addView(reset, resetLp);

        columns.addView(rightPanel, new LinearLayout.LayoutParams(0, WRAP, 0.92f));
        all.addView(columns);
        page.addView(all, new LinearLayout.LayoutParams(MATCH, 0, 1));
        page.addView(buildFooter(), new LinearLayout.LayoutParams(MATCH, WRAP));

        content.addView(page, new FrameLayout.LayoutParams(MATCH, MATCH));
        updateCards();
    }


    /** Передаёт нативному клиенту ник и выбранный сервер: он сразу откроет подключение. */
    private Intent withBastionSession(Intent i) {
        Config.Server s = cfg.servers.get(cfg.selected);
        i.putExtra("bastion.host", s.host);
        i.putExtra("bastion.port", s.port);
        i.putExtra("bastion.name", s.name);
        i.putExtra("bastion.nick", cfg.nickname);
        // Папка с игрой, которую скачал и распаковал ZIP-установщик лаунчера.
        // bastion.storage — корень для SAMP-Mobile (где лежит SAMP/), заменяет /storage/emulated/0/GTA/.
        java.io.File storage = ClientInstaller.findSampStorageRoot(this);
        if (storage != null) i.putExtra("bastion.storage", storage.getAbsolutePath() + "/");
        // Язык, для которого установлен text/*.gxt (движок иначе берёт язык телефона).
        String language = storage == null ? null : ClientInstaller.chooseGameLanguage(storage);
        if (language != null) i.putExtra("bastion.language", language);
        java.io.File gameData = ClientInstaller.findGameDataRoot(this);
        if (gameData != null) i.putExtra("bastion.game_dir", gameData.getAbsolutePath());
        // Настройки лаунчера (те же значения записаны в bastion.cfg).
        i.putExtra("bastion.fps", cfg.fps);
        i.putExtra("bastion.quality", cfg.quality);
        i.putExtra("bastion.chat_lines", cfg.chatLines);
        i.putExtra("bastion.button_size", cfg.buttonSize);
        if (clientConfig != null) i.putExtra("bastion.config", clientConfig.getAbsolutePath());
        return i;
    }

    /** Activity встроенного клиента SAMP-Mobile GTA-2.11 (модуль :samp, процесс ":game"). */
    static final String SAMP_ACTIVITY = "com.gta.game.SAMP";

    /** Запускает встроенный SAMP-Mobile внутри того же Bastion APK. */
    private void openSampClient() {
        try {
            Intent embeddedClient = new Intent();
            embeddedClient.setClassName(this, SAMP_ACTIVITY);
            GameCrashReport.markLaunch(this);
            startActivity(withBastionSession(embeddedClient));
        } catch (Exception e) {
            notice("Не удалось открыть игровой клиент",
                    "Встроенный клиент не запустился: " + e.getMessage(), null, null);
        }
    }

    /** Повторное открытие игрового экрана не обращается к отдельному пакету. */
    private void launchInstalledSampClient() {
        openSampClient();
    }

    //  Диагностика устройства и ресурсов Bastion
    // ==================================================================
    private void runDiagnostics() {
        applySettings();
        final TextView body = Ui.text(this, "Собираю данные…", 11, Ui.TEXT, Typeface.MONOSPACE);
        final String[] result = {null};
        final FrameLayout dim = new FrameLayout(this);
        dim.setBackgroundColor(0xB3000000);
        dim.setClickable(true);

        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(Ui.dp(20), Ui.dp(16), Ui.dp(20), Ui.dp(14));
        box.setBackground(Ui.box(Ui.PANEL, Ui.LINE, 10, 1));
        TextView t = Ui.text(this, "ДИАГНОСТИКА", 17, Ui.TEXT, Ui.heavy);
        box.addView(t);
        TextView sub = Ui.text(this, "Скопируй или отправь этот текст в чат проекта.", 12, Ui.MUTED, Ui.body);
        sub.setPadding(0, Ui.dp(6), 0, Ui.dp(10));
        box.addView(sub);
        ScrollView sv = new ScrollView(this);
        body.setTextIsSelectable(true);
        body.setPadding(Ui.dp(10), Ui.dp(8), Ui.dp(10), Ui.dp(8));
        body.setBackground(Ui.box(0xFF0B0C0F, Ui.LINE, 6, 1));
        sv.addView(body);
        box.addView(sv, new LinearLayout.LayoutParams(MATCH, 0, 1));

        LinearLayout buttons = new LinearLayout(this);
        buttons.setOrientation(LinearLayout.HORIZONTAL);
        buttons.setGravity(Gravity.END);
        TextView close = Ui.ghostButton(this, "Закрыть");
        close.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { root.removeView(dim); }
        });
        buttons.addView(close, new LinearLayout.LayoutParams(WRAP, Ui.dp(38)));
        TextView copy = Ui.ghostButton(this, "Копировать");
        copy.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                if (result[0] == null) return;
                android.content.ClipboardManager cm = (android.content.ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
                cm.setPrimaryClip(android.content.ClipData.newPlainText("Bastion diagnostics", result[0]));
                android.widget.Toast.makeText(MainActivity.this, "Скопировано", android.widget.Toast.LENGTH_SHORT).show();
            }
        });
        LinearLayout.LayoutParams cp = new LinearLayout.LayoutParams(WRAP, Ui.dp(38));
        cp.leftMargin = Ui.dp(8);
        buttons.addView(copy, cp);
        TextView share = Ui.primaryButton(this, "ОТПРАВИТЬ");
        share.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        share.setPadding(Ui.dp(18), 0, Ui.dp(18), 0);
        share.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                if (result[0] == null) return;
                Intent i = new Intent(Intent.ACTION_SEND);
                i.setType("text/plain");
                i.putExtra(Intent.EXTRA_TEXT, result[0]);
                startActivity(Intent.createChooser(i, "Отправить диагностику"));
            }
        });
        LinearLayout.LayoutParams sp = new LinearLayout.LayoutParams(WRAP, Ui.dp(38));
        sp.leftMargin = Ui.dp(8);
        buttons.addView(share, sp);
        LinearLayout.LayoutParams bp = new LinearLayout.LayoutParams(MATCH, WRAP);
        bp.topMargin = Ui.dp(12);
        box.addView(buttons, bp);

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(MATCH, MATCH);
        lp.setMargins(Ui.dp(60), Ui.dp(20), Ui.dp(60), Ui.dp(20));
        dim.addView(box, lp);
        root.addView(dim, new FrameLayout.LayoutParams(MATCH, MATCH));

        new Thread(new Runnable() {
            @Override public void run() {
                String rep;
                try { rep = Diagnostics.report(MainActivity.this); }
                catch (Throwable e) { rep = "Ошибка диагностики: " + e; }
                final String fin = rep;
                ui.post(new Runnable() {
                    @Override public void run() { result[0] = fin; body.setText(fin); }
                });
            }
        }, "diag").start();
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
