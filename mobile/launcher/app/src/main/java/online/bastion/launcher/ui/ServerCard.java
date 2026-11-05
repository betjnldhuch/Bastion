package online.bastion.launcher.ui;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import online.bastion.launcher.core.QueryResult;
import online.bastion.launcher.core.ServerEntry;
import online.bastion.launcher.net.ServerMonitor;

import static online.bastion.launcher.ui.Theme.dp;
import static online.bastion.launcher.ui.Views.MATCH;
import static online.bastion.launcher.ui.Views.WRAP;

/** Карточка сервера: название, адрес, онлайн и пинг. Выбранная — с оранжевой полосой. */
public final class ServerCard extends LinearLayout {

    public final ServerEntry server;
    private final View stripe;
    private final TextView address;
    private final TextView players;
    private final TextView state;

    public ServerCard(Context c, ServerEntry server) {
        super(c);
        this.server = server;
        setOrientation(HORIZONTAL);
        setClickable(true);
        setFocusable(true);

        stripe = new View(c);
        addView(stripe, Views.lp(dp(4), MATCH));

        LinearLayout body = Views.vertical(c);
        body.setPadding(dp(14), dp(12), dp(14), dp(12));

        LinearLayout top = Views.horizontal(c);
        TextView name = Views.text(c, server.name.toUpperCase(), 17, Theme.TEXT, Theme.heavy);
        name.setLetterSpacing(0.04f);
        name.setSingleLine(true);
        top.addView(name, Views.lp(0, WRAP, 1));
        players = Views.text(c, "", 17, Theme.TEXT, Theme.heavy);
        players.setGravity(Gravity.END);
        top.addView(players);
        body.addView(top);

        LinearLayout bottom = Views.horizontal(c);
        address = Views.text(c, "", 11, Theme.DIM, Theme.bodyMedium);
        address.setSingleLine(true);
        bottom.addView(address, Views.lp(0, WRAP, 1));
        state = Views.text(c, "", 10, Theme.MUTED, Theme.heavy);
        state.setLetterSpacing(0.1f);
        bottom.addView(state);
        body.addView(bottom, Views.margins(Views.lp(MATCH, WRAP), 0, 6, 0, 0));

        if (server.description.length() > 0) {
            TextView d = Views.text(c, server.description, 12, Theme.MUTED, Theme.body);
            d.setMaxLines(2);
            d.setLineSpacing(dp(2), 1f);
            body.addView(d, Views.margins(Views.lp(MATCH, WRAP), 0, 8, 0, 0));
        }
        addView(body, Views.lp(0, WRAP, 1));
        setSelectedCard(false);
        bind(null);
    }

    public void setSelectedCard(boolean selected) {
        setBackground(Views.box(selected ? Theme.SURFACE_HI : Theme.SURFACE,
                selected ? Theme.ORANGE : Theme.LINE, 1));
        stripe.setBackgroundColor(selected ? Theme.ORANGE : 0x00000000);
    }

    public void bind(ServerMonitor.Status s) {
        address.setText(s == null ? server.defaultAddress.toString() : s.address.toString());
        QueryResult r = s == null ? null : s.result;

        if (r == null) {
            players.setText("—");
            players.setTextColor(Theme.DIM);
            state.setText("ПРОВЕРКА…");
            state.setTextColor(Theme.MUTED);
        } else if (r.isOnline()) {
            players.setText(r.players + " / " + r.maxPlayers);
            players.setTextColor(Theme.TEXT);
            state.setText(r.pingMs >= 0 ? "ОНЛАЙН · " + r.pingMs + " МС" : "ОНЛАЙН");
            state.setTextColor(Theme.ORANGE);
        } else {
            players.setText("OFF");
            players.setTextColor(Theme.DIM);
            state.setText(r.state == QueryResult.State.UNRESOLVED ? "АДРЕС НЕ НАЙДЕН" : "НЕ ОТВЕЧАЕТ");
            state.setTextColor(Theme.DIM);
        }
    }
}
