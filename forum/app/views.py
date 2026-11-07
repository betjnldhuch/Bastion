"""HTML-страницы форума."""

import html
import time
import urllib.parse

from . import game, markup
from .storage import STATUSES, STATUS_KINDS

MONTHS = ["января", "февраля", "марта", "апреля", "мая", "июня", "июля", "августа",
          "сентября", "октября", "ноября", "декабря"]

KIND_LABELS = {"news": "Новости", "rules": "Правила", "complaint": "Жалобы", "appeal": "Обжалования",
               "application": "Заявки"}


def e(value):
    return html.escape("" if value is None else str(value), quote=True)


def rp(name):
    return (name or "").replace("_", " ")


def when(ts):
    if not ts:
        return "—"
    lt = time.localtime(ts)
    now = time.localtime()
    hm = time.strftime("%H:%M", lt)
    if lt.tm_year == now.tm_year and lt.tm_yday == now.tm_yday:
        return f"сегодня в {hm}"
    if lt.tm_year == now.tm_year and lt.tm_yday == now.tm_yday - 1:
        return f"вчера в {hm}"
    year = "" if lt.tm_year == now.tm_year else f" {lt.tm_year}"
    return f"{lt.tm_mday} {MONTHS[lt.tm_mon - 1]}{year} в {hm}"


def date(ts):
    return time.strftime("%d.%m.%Y", time.localtime(ts)) if ts else "—"


def plural(n, one, few, many):
    n10, n100 = n % 10, n % 100
    word = one if n10 == 1 and n100 != 11 else few if 2 <= n10 <= 4 and not 12 <= n100 <= 14 else many
    return f"{n} {word}"


def status_badge(status):
    if not status:
        return ""
    return f'<span class="badge st-{e(status)}">{e(STATUSES.get(status, status))}</span>'


def user_link(user_id, name, server=None, servers=None):
    tag = ""
    if server and servers and len(servers) > 1 and server in servers:
        tag = f' <span class="srv srv-{e(server)}">{e(servers[server].name.split()[-1])}</span>'
    return f'<a class="user" href="/u/{int(user_id)}">{e(rp(name))}</a>{tag}'


def pager(base, page, total_pages):
    if total_pages <= 1:
        return ""
    sep = "&" if "?" in base else "?"
    items = []
    for p in range(1, total_pages + 1):
        if p in (1, total_pages) or abs(p - page) <= 2:
            cls = ' class="on"' if p == page else ""
            items.append(f'<a{cls} href="{e(base)}{sep}page={p}">{p}</a>')
        elif items and items[-1] != "…":
            items.append("…")
    return '<nav class="pager">' + "".join(i if i != "…" else "<span>…</span>" for i in items) + "</nav>"


def csrf_field(ctx):
    return f'<input type="hidden" name="csrf" value="{e(ctx.csrf)}">' if ctx.csrf else ""


BB_TOOLBAR = """<div class="bb-bar" data-for="{id}">
<button type="button" data-tag="b" title="Жирный"><b>B</b></button>
<button type="button" data-tag="i" title="Курсив"><i>I</i></button>
<button type="button" data-tag="u" title="Подчёркнутый"><u>U</u></button>
<button type="button" data-tag="s" title="Зачёркнутый"><s>S</s></button>
<button type="button" data-tag="quote" title="Цитата">❝</button>
<button type="button" data-tag="spoiler" title="Спойлер">▸</button>
<button type="button" data-tag="url" title="Ссылка">🔗</button>
<button type="button" data-tag="img" title="Картинка по ссылке">🖼</button>
<button type="button" data-tag="code" title="Код">{{}}</button>
</div>"""


def editor(name, value="", rows=10, field_id="body"):
    return (BB_TOOLBAR.format(id=field_id) +
            f'<textarea id="{field_id}" name="{name}" rows="{rows}" maxlength="20000" required>{e(value)}</textarea>')


# ------------------------------------------------------------------
#  Общий каркас страницы
# ------------------------------------------------------------------

def layout(ctx, title, body, crumbs=None, status=200):
    cfg = ctx.cfg
    user = ctx.user
    if user:
        role = ""
        if ctx.perm.is_admin:
            role = '<span class="badge role-admin">Админ</span>'
        elif ctx.perm.is_moderator:
            role = '<span class="badge role-mod">Модератор</span>'
        account = (f'<a class="me" href="/u/{user["id"]}">{e(rp(user["name"]))}</a>{role}'
                   f'<form method="post" action="/logout" class="inline">{csrf_field(ctx)}'
                   f'<button class="link">Выйти</button></form>')
    else:
        nxt = urllib.parse.quote(ctx.path, safe="/")
        account = f'<a class="btn small" href="/login?next={e(nxt)}">Войти</a>'
    discord = f'<a href="{e(cfg.discord)}" target="_blank" rel="noopener">Discord</a>' if cfg.discord else ""
    nav = "".join(
        f'<li><a href="{e(u)}">{e(t)}</a></li>' for t, u in crumbs or [])
    crumbs_html = (f'<ol class="crumbs"><li><a href="/">Форум</a></li>{nav}</ol>' if crumbs else "")
    flash = f'<div class="flash">{e(ctx.flash)}</div>' if ctx.flash else ""
    page_title = f"{title} — {cfg.title}" if title else cfg.title
    return status, f"""<!doctype html>
<html lang="ru"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{e(page_title)}</title>
<link rel="stylesheet" href="/static/style.css">
<link rel="icon" href="/static/icon.svg" type="image/svg+xml">
</head><body>
<header class="top"><div class="wrap">
  <a class="logo" href="/"><span>{e(cfg.title.upper())}</span><small>форум</small></a>
  <nav class="menu">
    <a href="/">Разделы</a><a href="/team">Администрация</a><a href="/bans">Баны</a>{discord}
    <form action="/search" method="get" class="search"><input name="q" placeholder="Поиск" aria-label="Поиск"></form>
  </nav>
  <div class="account">{account}</div>
</div></header>
<main class="wrap">{crumbs_html}{flash}{body}</main>
<footer class="wrap foot">{e(cfg.title)} · форум работает на том же компьютере, что и сервер · вход по игровому аккаунту</footer>
<script src="/static/forum.js" defer></script>
</body></html>"""


# ------------------------------------------------------------------
#  Главная
# ------------------------------------------------------------------

def server_cards(ctx):
    cards = []
    for srv in ctx.cfg.servers.values():
        st = game.server_status(srv)
        if st.get("online"):
            pct = min(100, int(100 * st["players"] / max(1, st["max_players"])))
            info = (f'<div class="srv-num"><b>{st["players"]}</b> / {st["max_players"]}</div>'
                    f'<div class="meter"><span style="width:{pct}%"></span></div>'
                    f'<div class="muted">{e(st["gamemode"])}</div>')
            dot = '<span class="dot on"></span>Онлайн'
        else:
            info = '<div class="muted">Сервер сейчас выключен</div>'
            dot = '<span class="dot"></span>Оффлайн'
        cards.append(f"""<div class="card srv-card"><div class="srv-head"><b>{e(srv.name)}</b><span>{dot}</span></div>
{info}<div class="muted mono">{e(srv.address)}:{srv.port}</div></div>""")
    return "".join(cards)


def index(ctx, cats, totals, online, recent):
    rows = []
    for c in cats:
        forums = []
        for f in c["forums"]:
            last = f["last"]
            last_html = (f'<a href="/t/{last["topic_id"]}?last=1">{e(markup.plain(last["title"], 40))}</a>'
                         f'<div class="muted">{user_link(last["user_id"], last["user_name"])} · {when(last["last_post_at"])}</div>'
                         if last else '<span class="muted">Пока пусто</span>')
            kind = KIND_LABELS.get(f["kind"])
            kind_html = f'<span class="kind k-{e(f["kind"])}">{e(kind)}</span>' if kind else ""
            color = game.FACTION_COLORS.get(f["faction"])
            style = f' style="--accent:{color}"' if color else ""
            forums.append(f"""<div class="forum-row"{style}>
<div class="forum-icon"></div>
<div class="forum-main"><a class="forum-title" href="/f/{f['id']}">{e(f['title'])}</a>{kind_html}
<div class="muted">{e(f['description'])}</div></div>
<div class="forum-stats"><b>{f['topics_count']}</b><span>тем</span></div>
<div class="forum-stats"><b>{f['posts_count']}</b><span>сообщ.</span></div>
<div class="forum-last">{last_html}</div></div>""")
        rows.append(f'<section class="block"><h2>{e(c["title"])}</h2>{"".join(forums)}</section>')

    online_html = ", ".join(user_link(u["id"], u["name"], u["server"], ctx.cfg.servers) for u in online) \
        or '<span class="muted">никого</span>'
    recent_html = "".join(
        f'<li><a href="/t/{t["id"]}?last=1">{e(markup.plain(t["title"], 50))}</a>'
        f'<div class="muted">{e(t["forum_title"])} · {e(rp(t["last_name"]))} · {when(t["last_post_at"])}</div></li>'
        for t in recent) or '<li class="muted">Сообщений пока нет</li>'
    hero = f"""<section class="hero"><div><h1>{e(ctx.cfg.title)}</h1><p>{e(ctx.cfg.subtitle)}</p>
{'' if ctx.user else '<a class="btn" href="/login">Войти игровым аккаунтом</a>'}</div>
<div class="srv-cards">{server_cards(ctx)}</div></section>"""
    side = f"""<aside class="side">
<div class="card"><h3>Последние сообщения</h3><ul class="recent">{recent_html}</ul></div>
<div class="card"><h3>Статистика</h3><div class="totals">
<div><b>{totals['topics']}</b><span>тем</span></div><div><b>{totals['posts']}</b><span>сообщений</span></div>
<div><b>{totals['users']}</b><span>участников</span></div></div></div>
<div class="card"><h3>Сейчас на форуме</h3><p>{online_html}</p></div>
</aside>"""
    return layout(ctx, "", f'{hero}<div class="cols"><div class="main-col">{"".join(rows)}</div>{side}</div>')


# ------------------------------------------------------------------
#  Раздел
# ------------------------------------------------------------------

def forum_page(ctx, forum, topics, page, pages, status_filter, can_post):
    rows = []
    for t in topics:
        flags = ""
        if t["pinned"]:
            flags += '<span class="badge pin">Важно</span>'
        if t["locked"]:
            flags += '<span class="badge lock">Закрыто</span>'
        flags += status_badge(t["status"])
        tpages = max(1, (t["posts_count"] + ctx.posts_per_page - 1) // ctx.posts_per_page)
        rows.append(f"""<div class="topic-row{' pinned' if t['pinned'] else ''}">
<div class="topic-main">{flags}<a class="topic-title" href="/t/{t['id']}">{e(t['title'])}</a>
<div class="muted">{user_link(t['user_id'], t['author_name'], t['author_server'], ctx.cfg.servers)} · {when(t['created'])}
{f' · страниц: {tpages}' if tpages > 1 else ''}</div></div>
<div class="forum-stats"><b>{t['posts_count'] - 1}</b><span>ответов</span></div>
<div class="forum-stats"><b>{t['views']}</b><span>просм.</span></div>
<div class="forum-last"><a href="/t/{t['id']}?last=1">{when(t['last_post_at'])}</a>
<div class="muted">{e(rp(t['last_name']))}</div></div></div>""")
    if not rows:
        rows.append('<div class="empty">Здесь пока нет тем.</div>')

    filters = ""
    if forum["kind"] in STATUS_KINDS:
        opts = [("", "Все")] + list(STATUSES.items())
        filters = '<div class="filters">' + "".join(
            f'<a class="{"on" if (status_filter or "") == k else ""}" href="/f/{forum["id"]}{"?status=" + k if k else ""}">{e(v)}</a>'
            for k, v in opts) + "</div>"
    new_btn = (f'<a class="btn" href="/f/{forum["id"]}/new">Создать тему</a>' if can_post else
               ('' if ctx.user else f'<a class="btn ghost" href="/login?next=/f/{forum["id"]}">Войдите, чтобы создать тему</a>'))
    base = f'/f/{forum["id"]}' + (f'?status={e(status_filter)}' if status_filter else "")
    body = f"""<div class="page-head"><div><h1>{e(forum['title'])}</h1><p class="muted">{e(forum['description'])}</p></div>{new_btn}</div>
{filters}<section class="block">{''.join(rows)}</section>{pager(base, page, pages)}"""
    return layout(ctx, forum["title"], body, crumbs=[(forum["title"], f"/f/{forum['id']}")])


# ------------------------------------------------------------------
#  Тема
# ------------------------------------------------------------------

def _author_card(ctx, p, info):
    acc = info.get("account") or {}
    lines = []
    if info.get("server_name") and len(ctx.cfg.servers) > 1:
        lines.append(f'<div class="srv srv-{e(p["user_server"])}">{e(info["server_name"])}</div>')
    if info.get("role"):
        lines.append(f'<div class="badge role-{e(info["role_cls"])}">{e(info["role"])}</div>')
    if acc:
        lines.append(f'<div>Уровень: <b>{acc.get("level", 1)}</b></div>')
        f = acc.get("faction") or 0
        if f in game.FACTIONS and f:
            color = game.FACTION_COLORS.get(f, "#aaa")
            lines.append(f'<div class="fac" style="color:{color}">{e(game.FACTIONS[f])}</div>'
                         f'<div class="muted">{e(game.rank_name(f, acc.get("frank") or 0))}</div>')
    if info.get("banned"):
        lines.append('<div class="badge st-rejected">Заблокирован в игре</div>')
    lines.append(f'<div class="muted">Сообщений: {p["user_posts"]}</div>')
    initials = "".join(w[:1] for w in rp(p["user_name"]).split()[:2]).upper() or "?"
    return f"""<div class="author"><div class="avatar">{e(initials)}</div>
{user_link(p['user_id'], p['user_name'])}{''.join(lines)}</div>"""


def topic_page(ctx, forum, topic, posts, authors, page, pages, perms, offset):
    items = []
    for i, p in enumerate(posts):
        tools = []
        if perms["can_reply"]:
            tools.append(f'<button type="button" class="link quote-btn" data-name="{e(p["user_name"])}" '
                         f'data-src="src-{p["id"]}">Цитировать</button>')
        if perms["edit"](p):
            tools.append(f'<a href="/p/{p["id"]}/edit">Изменить</a>')
        if perms["can_moderate"]:
            tools.append(f'<form method="post" action="/p/{p["id"]}/delete" class="inline confirm" '
                         f'data-confirm="Удалить сообщение?">{csrf_field(ctx)}<button class="link danger">Удалить</button></form>')
        edited = (f'<div class="muted edited">Изменено {when(p["edited"])}</div>' if p["edited"] else "")
        sig = (f'<div class="sig">{markup.render(p["signature"])}</div>' if p["signature"] else "")
        items.append(f"""<article class="post" id="p{p['id']}">
{_author_card(ctx, p, authors.get(p['user_id'], {}))}
<div class="post-body"><div class="post-meta"><a href="/t/{topic['id']}?page={page}#p{p['id']}">#{offset + i + 1}</a> · {when(p['created'])}</div>
<div class="content">{markup.render(p['body'])}</div>{edited}{sig}
<textarea id="src-{p['id']}" hidden>{e(p['body'])}</textarea>
<div class="post-tools">{''.join(tools)}</div></div></article>""")

    mod = ""
    if perms["can_moderate"]:
        st_buttons = ""
        if forum["kind"] in STATUS_KINDS:
            st_buttons = "".join(
                f'<button name="action" value="status:{k}" class="btn small st-{k}">{e(v)}</button>'
                for k, v in STATUSES.items() if k != topic["status"])
        move = ""
        if perms["can_move"]:
            opts = "".join(f'<option value="{f["id"]}"{" selected" if f["id"] == forum["id"] else ""}>{e(f["title"])}</option>'
                           for f in perms["forums"])
            move = (f'<select name="forum_id" aria-label="Раздел">{opts}</select>'
                    f'<button name="action" value="move" class="btn small ghost">Перенести</button>')
        mod = f"""<form method="post" action="/t/{topic['id']}/mod" class="mod-bar">{csrf_field(ctx)}
<span class="muted">Модерация:</span>{st_buttons}
<button name="action" value="{'unpin' if topic['pinned'] else 'pin'}" class="btn small ghost">{'Открепить' if topic['pinned'] else 'Закрепить'}</button>
<button name="action" value="{'unlock' if topic['locked'] else 'lock'}" class="btn small ghost">{'Открыть' if topic['locked'] else 'Закрыть'}</button>
{move}
<button name="action" value="delete" class="btn small danger" data-confirm="Удалить тему целиком?">Удалить тему</button>
</form>"""

    if perms["can_reply"]:
        reply = f"""<section class="card reply" id="reply"><h3>Ответить</h3>
<form method="post" action="/t/{topic['id']}/reply">{csrf_field(ctx)}{editor('body', rows=6)}
<div class="form-foot"><button class="btn">Отправить</button></div></form></section>"""
    elif not ctx.user:
        reply = f'<div class="card notice"><a href="/login?next=/t/{topic["id"]}">Войдите</a>, чтобы ответить.</div>'
    else:
        reply = f'<div class="card notice">{e(perms["reply_reason"])}</div>'

    flags = status_badge(topic["status"])
    if topic["locked"]:
        flags += '<span class="badge lock">Закрыто</span>'
    body = f"""<div class="page-head"><div><h1>{flags}{e(topic['title'])}</h1>
<p class="muted">{user_link(topic['user_id'], topic['author_name'])} · {when(topic['created'])} · {plural(topic['views'], 'просмотр', 'просмотра', 'просмотров')}</p></div></div>
{mod}{pager(f"/t/{topic['id']}", page, pages)}<div class="posts">{''.join(items)}</div>{pager(f"/t/{topic['id']}", page, pages)}{reply}"""
    return layout(ctx, topic["title"], body,
                  crumbs=[(forum["title"], f"/f/{forum['id']}"), (topic["title"], f"/t/{topic['id']}")])


def new_topic(ctx, forum, title="", body="", error=""):
    err = f'<div class="error">{e(error)}</div>' if error else ""
    hint = ('<p class="muted">Заполните форму ниже. Тема без формы или без доказательств может быть закрыта.</p>'
            if forum["template"] and forum["kind"] in STATUS_KINDS else "")
    html_ = f"""<div class="page-head"><h1>Новая тема</h1></div>
<section class="card"><form method="post" action="/f/{forum['id']}/new">{csrf_field(ctx)}{err}{hint}
<label>Заголовок<input name="title" value="{e(title)}" maxlength="120" required autofocus></label>
<label>Текст</label>{editor('body', body or forum['template'], rows=14)}
<div class="form-foot"><button class="btn">Создать тему</button><a class="btn ghost" href="/f/{forum['id']}">Отмена</a></div>
</form></section>"""
    return layout(ctx, "Новая тема", html_, crumbs=[(forum["title"], f"/f/{forum['id']}"), ("Новая тема", "#")])


def edit_post(ctx, forum, topic, post, is_first, error=""):
    err = f'<div class="error">{e(error)}</div>' if error else ""
    title = (f'<label>Заголовок темы<input name="title" value="{e(topic["title"])}" maxlength="120" required></label>'
             if is_first else "")
    html_ = f"""<div class="page-head"><h1>Изменить сообщение</h1></div>
<section class="card"><form method="post" action="/p/{post['id']}/edit">{csrf_field(ctx)}{err}{title}
{editor('body', post['body'], rows=14)}
<div class="form-foot"><button class="btn">Сохранить</button><a class="btn ghost" href="/t/{topic['id']}">Отмена</a></div>
</form></section>"""
    return layout(ctx, "Изменить сообщение", html_,
                  crumbs=[(forum["title"], f"/f/{forum['id']}"), (topic["title"], f"/t/{topic['id']}")])


# ------------------------------------------------------------------
#  Вход
# ------------------------------------------------------------------

def login(ctx, nxt, name="", server="", error=""):
    servers = ctx.cfg.servers
    if len(servers) > 1:
        opts = "".join(f'<option value="{e(k)}"{" selected" if k == server else ""}>{e(s.name)}</option>'
                       for k, s in servers.items())
        server_field = f'<label>Сервер<select name="server">{opts}</select></label>'
    elif servers:
        server_field = f'<input type="hidden" name="server" value="{e(next(iter(servers)))}">'
    else:
        server_field = ""
    missing = [s for s in servers.values() if not game.GameDB(s).available()]
    warn = ""
    if missing and ctx.local:
        warn = ('<div class="error">Не найдена база сервера: ' +
                ", ".join(f'{e(s.name)} ({e(s.db)})' for s in missing) +
                '. Проверь путь db в forum.ini.</div>')
    err = f'<div class="error">{e(error)}</div>' if error else ""
    body = f"""<section class="card narrow"><h1>Вход на форум</h1>
<p class="muted">Ник и пароль — те же, что в игре. Регистрация на форуме не нужна: зайди на сервер, зарегистрируйся там, и аккаунт сразу работает здесь.</p>
{warn}{err}<form method="post" action="/login">{csrf_field(ctx)}
<input type="hidden" name="next" value="{e(nxt)}">{server_field}
<label>Ник (Имя_Фамилия)<input name="name" value="{e(name)}" maxlength="24" required autofocus autocomplete="username"></label>
<label>Пароль<input name="password" type="password" maxlength="64" required autocomplete="current-password"></label>
<div class="form-foot"><button class="btn">Войти</button></div></form></section>"""
    return layout(ctx, "Вход", body, crumbs=[("Вход", "/login")])


# ------------------------------------------------------------------
#  Профиль, администрация, баны, поиск
# ------------------------------------------------------------------

def profile(ctx, user, info, posts_count, topics, can_ban, is_self):
    acc = info.get("account") or {}
    rows = [("Сервер", info.get("server_name") or user["server"])]
    if acc:
        rows.append(("Уровень", acc.get("level", 1)))
        f = acc.get("faction") or 0
        if f:
            rows.append(("Фракция", f'{game.FACTIONS.get(f, "—")} · {game.rank_name(f, acc.get("frank") or 0)}'))
        if acc.get("admin"):
            rows.append(("Администратор", f'{acc["admin"]} уровня'))
        rows.append(("В игре с", date(acc.get("reg_date"))))
    rows += [("На форуме с", date(user["created"])), ("Был на форуме", when(user["last_seen"])),
             ("Сообщений", posts_count)]
    if info.get("banned"):
        b = info["banned"]
        rows.append(("Блокировка в игре", f'{b.get("reason") or "без причины"} (выдал {rp(b.get("admin"))})'))
    if user["forum_ban_until"] > time.time():
        rows.append(("Блокировка на форуме", f'до {date(user["forum_ban_until"])}: {user["forum_ban_reason"]}'))
    table = "".join(f"<tr><th>{e(k)}</th><td>{e(v)}</td></tr>" for k, v in rows)
    topics_html = "".join(
        f'<li>{status_badge(t["status"])}<a href="/t/{t["id"]}">{e(t["title"])}</a>'
        f'<div class="muted">{e(t["forum_title"])} · {when(t["created"])}</div></li>' for t in topics) \
        or '<li class="muted">Тем пока нет</li>'
    sig = ""
    if is_self:
        sig = f"""<section class="card"><h3>Подпись</h3><p class="muted">Показывается под каждым вашим сообщением. До 300 символов.</p>
<form method="post" action="/u/{user['id']}/signature">{csrf_field(ctx)}
<textarea name="signature" rows="3" maxlength="300">{e(user['signature'])}</textarea>
<div class="form-foot"><button class="btn small">Сохранить</button></div></form></section>"""
    ban = ""
    if can_ban:
        ban = f"""<section class="card"><h3>Блокировка на форуме</h3>
<form method="post" action="/u/{user['id']}/ban" class="row-form">{csrf_field(ctx)}
<label>Дней (0 — снять)<input name="days" type="number" min="0" max="3650" value="7"></label>
<label>Причина<input name="reason" maxlength="120"></label><button class="btn small danger">Применить</button></form></section>"""
    initials = "".join(w[:1] for w in rp(user["name"]).split()[:2]).upper() or "?"
    body = f"""<div class="profile"><section class="card profile-card"><div class="avatar big">{e(initials)}</div>
<h1>{e(rp(user['name']))}</h1>{f'<div class="badge role-{e(info["role_cls"])}">{e(info["role"])}</div>' if info.get('role') else ''}
<table class="kv">{table}</table></section>
<div><section class="card"><h3>Темы</h3><ul class="recent">{topics_html}</ul></section>{sig}{ban}</div></div>"""
    return layout(ctx, rp(user["name"]), body, crumbs=[(rp(user["name"]), f"/u/{user['id']}")])


def team(ctx, per_server):
    blocks = []
    for srv, admins, leaders in per_server:
        a = "".join(f'<tr><td>{e(rp(x["name"]))}</td><td>{x["admin"]} уровень</td></tr>' for x in admins) \
            or '<tr><td colspan="2" class="muted">Нет данных</td></tr>'
        l_rows = "".join(
            f'<tr><td style="color:{game.FACTION_COLORS.get(f, "#aaa")}">{e(game.FACTIONS[f])}</td>'
            f'<td>{e(rp(x["name"]))}</td><td class="muted">{e(game.rank_name(f, x["frank"]))}</td></tr>'
            for f, x in leaders) or '<tr><td colspan="3" class="muted">Нет данных</td></tr>'
        blocks.append(f"""<section class="card"><h2>{e(srv.name)}</h2>
<h3>Администрация</h3><table class="list">{a}</table>
<h3>Лидеры и заместители фракций</h3><table class="list">{l_rows}</table></section>""")
    return layout(ctx, "Администрация", '<div class="page-head"><h1>Администрация и лидеры</h1></div>' +
                  '<div class="grid2">' + "".join(blocks) + "</div>", crumbs=[("Администрация", "/team")])


def bans(ctx, per_server):
    blocks = []
    for srv, rows in per_server:
        r = "".join(f'<tr><td>{e(rp(b["name"]))}</td><td>{e(b.get("reason") or "—")}</td>'
                    f'<td>{e(rp(b.get("admin")))}</td><td>{date(b.get("date"))}</td></tr>' for b in rows) \
            or '<tr><td colspan="4" class="muted">Банов нет</td></tr>'
        blocks.append(f"""<section class="card"><h2>{e(srv.name)}</h2><table class="list">
<tr><th>Ник</th><th>Причина</th><th>Администратор</th><th>Дата</th></tr>{r}</table></section>""")
    appeal = ""
    if ctx.appeal_forum:
        appeal = f'<p>Не согласен с баном? <a href="/f/{ctx.appeal_forum}">Подай обжалование</a>, войти на форум можно и с заблокированного аккаунта.</p>'
    return layout(ctx, "Баны", f'<div class="page-head"><div><h1>Список банов</h1>{appeal}</div></div>' + "".join(blocks),
                  crumbs=[("Баны", "/bans")])


def search(ctx, q, results):
    rows = "".join(
        f'<div class="topic-row"><div class="topic-main"><a class="topic-title" href="/t/{t["id"]}">{e(t["title"])}</a>'
        f'<div class="muted">{e(t["forum_title"])} · {e(rp(t["author_name"]))} · {when(t["last_post_at"])}</div></div></div>'
        for t in results)
    if q and not rows:
        rows = '<div class="empty">Ничего не найдено.</div>'
    body = f"""<div class="page-head"><h1>Поиск</h1></div>
<form action="/search" method="get" class="card row-form"><input name="q" value="{e(q)}" minlength="3" placeholder="Слово из темы или сообщения">
<button class="btn">Найти</button></form><section class="block">{rows}</section>"""
    return layout(ctx, "Поиск", body, crumbs=[("Поиск", "/search")])


def modlog(ctx, rows):
    def link(x):
        if not x["topic_title"]:
            return ""
        return '<a href="/t/%d">%s</a>' % (x["topic_id"], e(x["topic_title"]))
    r = "".join(f'<tr><td>{when(x["created"])}</td><td>{e(rp(x["user_name"]))}</td><td>{e(x["action"])}</td>'
                f'<td>{link(x)}</td><td class="muted">{e(x["details"])}</td></tr>' for x in rows)
    return layout(ctx, "Журнал модерации", f'<div class="page-head"><h1>Журнал модерации</h1></div>'
                  f'<section class="card"><table class="list">{r}</table></section>', crumbs=[("Журнал", "/modlog")])


def error(ctx, code, text):
    return layout(ctx, "Ошибка", f'<section class="card narrow"><h1>{code}</h1><p>{e(text)}</p>'
                  '<a class="btn" href="/">На главную</a></section>', status=code)
