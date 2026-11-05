package online.bastion.launcher.ui;

import android.app.Activity;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.view.Gravity;
import android.view.View;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

import online.bastion.launcher.core.Nickname;
import online.bastion.launcher.core.ServerEntry;
import online.bastion.launcher.data.News;
import online.bastion.launcher.net.ServerMonitor;

import static online.bastion.launcher.ui.Theme.dp;
import static online.bastion.launcher.ui.Views.MATCH;
import static online.bastion.launcher.ui.Views.WRAP;

/**
 * Главный экран (альбомная ориентация):
 *   [ЛОГОТИП]                               [НАСТРОЙКИ]
 *   ───────────────────────────────────────────────────
 *   Новости                    │ Серверы (карточки)
 *   ───────────────────────────────────────────────────
 *   Ник [______________]                     [ ИГРАТЬ ]
 */
public final class MainScreen implements ServerMonitor.Listener {

    private final ScreenHost host;
    private final Activity a;
    private final List<ServerCard> cards = new ArrayList<ServerCard>();
    private EditText nickField;
    private final View root;

    public MainScreen(ScreenHost host) {
        this.host = host;
        this.a = host.activity();
        this.root = build();
    }

    public View view() { return root; }

    /** Текущий текст из поля ника (сохраняется при уходе с экрана). */
    public String nickname() { return nickField.getText().toString().trim(); }

    private View build() {
        LinearLayout page = Views.vertical(a);
        page.setPadding(dp(28), dp(14), dp(28), dp(14));
        page.addView(header(), Views.lp(MATCH, WRAP));
        page.addView(Views.rule(a), Views.margins(Views.lp(MATCH, dp(1)), 0, 10, 0, 14));

        LinearLayout middle = Views.horizontal(a);
        middle.setGravity(Gravity.TOP);
        middle.addView(newsColumn(), Views.margins(Views.lp(0, MATCH, 1.1f), 0, 0, 24, 0));
        middle.addView(serverColumn(), Views.lp(0, MATCH, 1f));
        page.addView(middle, Views.lp(MATCH, 0, 1));

        page.addView(footer(), Views.margins(Views.lp(MATCH, WRAP), 0, 12, 0, 0));
        return page;
    }

    private View header() {
        LinearLayout row = Views.horizontal(a);

        LinearLayout brand = Views.vertical(a);
        SpannableStringBuilder title = new SpannableStringBuilder();
        title.append(host.config().title.toUpperCase());
        if (host.config().subtitle.length() > 0) {
            int start = title.length() + 1;
            title.append(' ').append(host.config().subtitle.toUpperCase());
            title.setSpan(new ForegroundColorSpan(Theme.ORANGE), start, title.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        TextView logo = Views.text(a, "", 34, Theme.TEXT, Theme.title);
        logo.setText(title);
        logo.setLetterSpacing(0.02f);
        logo.setSingleLine(true);
        brand.addView(logo);
        TextView tagline = Views.text(a, "SAN ANDREAS MULTIPLAYER", 9, Theme.MUTED, Theme.heavy);
        tagline.setLetterSpacing(0.45f);
        brand.addView(tagline, Views.margins(Views.lp(WRAP, WRAP), 2, 6, 0, 0));
        row.addView(brand, Views.lp(0, WRAP, 1));

        TextView settings = Views.ghostButton(a, "Настройки");
        settings.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { host.openSettings(); }
        });
        row.addView(settings, Views.lp(WRAP, dp(38)));
        return row;
    }

    private View newsColumn() {
        LinearLayout col = Views.vertical(a);
        col.addView(Views.caption(a, "Новости"));

        LinearLayout list = Views.vertical(a);
        List<News.Item> news = News.load(a);
        if (news.isEmpty()) list.addView(Views.text(a, "Пока новостей нет.", 13, Theme.MUTED, Theme.body));
        for (int i = 0; i < news.size(); i++) {
            News.Item n = news.get(i);
            LinearLayout item = Views.vertical(a);
            TextView date = Views.text(a, n.date, 10, Theme.ORANGE, Theme.heavy);
            date.setLetterSpacing(0.1f);
            item.addView(date);
            item.addView(Views.text(a, n.title, 15, Theme.TEXT, Theme.bodyMedium), Views.margins(Views.lp(MATCH, WRAP), 0, 5, 0, 0));
            TextView body = Views.text(a, n.text, 12, Theme.MUTED, Theme.body);
            body.setLineSpacing(dp(2), 1f);
            item.addView(body, Views.margins(Views.lp(MATCH, WRAP), 0, 4, 0, 0));
            list.addView(item, Views.margins(Views.lp(MATCH, WRAP), 0, i == 0 ? 0 : 16, 0, 0));
        }
        ScrollView scroll = new ScrollView(a);
        scroll.setVerticalScrollBarEnabled(false);
        scroll.addView(list);
        col.addView(scroll, Views.margins(Views.lp(MATCH, 0, 1), 0, 12, 0, 0));
        return col;
    }

    private View serverColumn() {
        LinearLayout col = Views.vertical(a);
        col.addView(Views.caption(a, "Сервер"));

        LinearLayout list = Views.vertical(a);
        ServerEntry selected = host.prefs().selectedServer();
        for (final ServerEntry s : host.config().servers) {
            final ServerCard card = new ServerCard(a, s);
            card.setSelectedCard(s == selected);
            card.bind(host.monitor().statusOf(s));
            card.setOnClickListener(new View.OnClickListener() {
                @Override public void onClick(View v) {
                    host.prefs().setSelectedServer(s);
                    for (ServerCard c : cards) c.setSelectedCard(c == card);
                    host.monitor().refresh(s);
                }
            });
            cards.add(card);
            list.addView(card, Views.margins(Views.lp(MATCH, WRAP), 0, cards.size() == 1 ? 0 : 10, 0, 0));
        }
        ScrollView scroll = new ScrollView(a);
        scroll.setVerticalScrollBarEnabled(false);
        scroll.addView(list);
        col.addView(scroll, Views.margins(Views.lp(MATCH, 0, 1), 0, 12, 0, 0));
        return col;
    }

    private View footer() {
        LinearLayout row = Views.horizontal(a);

        LinearLayout nick = Views.vertical(a);
        TextView label = Views.text(a, "НИК", 10, Theme.MUTED, Theme.heavy);
        label.setLetterSpacing(0.3f);
        nick.addView(label);
        nickField = Views.field(a, host.prefs().nickname(), "Имя_Фамилия", Nickname.MAX_LENGTH);
        nick.addView(nickField, Views.margins(Views.lp(MATCH, dp(46)), 0, 6, 0, 0));
        row.addView(nick, Views.margins(Views.lp(0, WRAP, 1), 0, 0, 20, 0));

        TextView play = Views.primaryButton(a, "Играть", 22);
        play.setTypeface(Theme.heavy);
        play.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { host.play(); }
        });
        LinearLayout.LayoutParams playLp = Views.lp(dp(220), dp(58));
        playLp.gravity = Gravity.BOTTOM;
        row.addView(play, playLp);
        return row;
    }

    @Override public void onStatus(ServerEntry server, ServerMonitor.Status status) {
        for (ServerCard c : cards) if (c.server == server) c.bind(status);
    }
}
