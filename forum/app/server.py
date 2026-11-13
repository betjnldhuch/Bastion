"""HTTP-сервер форума: маршруты, вход, права."""

import hmac
import http.cookies
import json
import mimetypes
import os
import re
import secrets
import threading
import time
import traceback
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from . import game, markup, views
from .storage import STATUSES, STATUS_KINDS, Storage

TOPICS_PER_PAGE = 20
POSTS_PER_PAGE = 15
MAX_BODY = 20000
MAX_TITLE = 120
POST_COOLDOWN = 15            # секунд между сообщениями одного человека
LOGIN_WINDOW, LOGIN_TRIES = 600, 6
COOKIE = "bf_session"
ANON_COOKIE = "bf_anon"
QUIET = False
ACCOUNT_TTL = 60              # как часто перечитывать уровень/админку/фракцию из rp.db

STATIC_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "static")


class Perm:
    """Права текущего человека."""

    def __init__(self, cfg, user=None, account=None, banned=None):
        self.cfg = cfg
        self.user = user
        self.account = account or {}
        self.banned_in_game = bool(banned)
        admin = int(self.account.get("admin") or 0)
        owner = bool(user) and user["name"].lower() in cfg.owners
        self.is_admin = owner or (bool(user) and admin >= cfg.admin_level)
        self.is_moderator = self.is_admin or (bool(user) and admin >= cfg.moderator_level)
        self.faction = int(self.account.get("faction") or 0)
        self.is_leader = self.faction > 0 and int(self.account.get("frank") or 0) >= game.LEADER_RANK
        self.forum_banned = bool(user) and user["forum_ban_until"] > time.time()

    def can_moderate(self, forum):
        if not self.user:
            return False
        if self.is_moderator:
            return True
        return bool(forum["faction"]) and self.is_leader and self.faction == forum["faction"]

    def why_cannot_post(self, forum):
        if not self.user:
            return "Нужно войти."
        if self.forum_banned:
            return "Вы заблокированы на форуме: " + (self.user["forum_ban_reason"] or "без причины")
        if forum["kind"] in ("news", "rules") and not self.is_admin:
            return "Темы в этом разделе создаёт только администрация."
        if self.banned_in_game and forum["kind"] != "appeal":
            return "Аккаунт заблокирован в игре. Можно писать только в «Обжалование наказаний»."
        return ""

    def why_cannot_reply(self, forum, topic):
        if not self.user:
            return "Нужно войти."
        if self.forum_banned:
            return "Вы заблокированы на форуме."
        mod = self.can_moderate(forum)
        if topic["locked"] and not mod:
            return "Тема закрыта."
        if forum["kind"] in STATUS_KINDS and not mod and topic["user_id"] != self.user["id"]:
            return "Отвечать здесь могут только автор и администрация."
        if forum["kind"] in ("news", "rules") and not mod:
            return "Комментарии в этом разделе отключены."
        if self.banned_in_game and not (forum["kind"] == "appeal" and topic["user_id"] == self.user["id"]):
            return "Аккаунт заблокирован в игре."
        return ""

    def can_edit(self, forum, topic, post):
        if not self.user or self.forum_banned:
            return False
        if self.can_moderate(forum):
            return True
        return post["user_id"] == self.user["id"] and not topic["locked"]


class Context:
    def __init__(self, app, handler):
        self.cfg = app.cfg
        self.path = handler.path
        self.user = None
        self.perm = Perm(app.cfg)
        self.csrf = ""
        self.flash = ""
        self.ip = handler.client_address[0]
        self.local = self.ip in ("127.0.0.1", "::1")
        self.posts_per_page = POSTS_PER_PAGE
        self.appeal_forum = app.appeal_forum


class Redirect(Exception):
    def __init__(self, location, cookies=None):
        self.location = location
        self.cookies = cookies or []


class HttpError(Exception):
    def __init__(self, code, text):
        self.code = code
        self.text = text


class App:
    def __init__(self, cfg):
        self.cfg = cfg
        self.store = Storage(os.path.join(cfg.data_dir, "forum.db"))
        self.games = {k: game.GameDB(s) for k, s in cfg.servers.items()}
        self._acc_cache = {}
        self._lock = threading.Lock()
        self._login_fails = {}
        self._last_post = {}
        self.appeal_forum = next((f["id"] for f in self.store.all_forums() if f["kind"] == "appeal"), 0)
        self.secure_cookies = cfg.public_url.startswith("https://")
        self.anon_secret = secrets.token_bytes(32)

    # ---------- данные из игры ----------

    def game_info(self, user):
        """Аккаунт из rp.db (кэш на минуту), бан, роль."""
        key = user["id"]
        now = time.time()
        with self._lock:
            cached = self._acc_cache.get(key)
        if cached and now - cached[0] < ACCOUNT_TTL:
            return cached[1]
        gdb = self.games.get(user["server"])
        account = gdb.account_by_id(user["game_id"]) if gdb else None
        banned = gdb.ban_of(user["name"]) if gdb else None
        perm = Perm(self.cfg, user, account, banned)
        role, role_cls = "", ""
        if perm.is_admin:
            role, role_cls = "Администратор", "admin"
        elif perm.is_moderator:
            role, role_cls = "Модератор", "mod"
        elif perm.is_leader:
            role, role_cls = "Лидер фракции" if int(account.get("frank") or 0) == game.MAX_RANK else "Зам. лидера", "leader"
        srv = self.cfg.servers.get(user["server"])
        info = {"account": account, "banned": banned, "role": role, "role_cls": role_cls,
                "server_name": srv.name if srv else user["server"]}
        with self._lock:
            self._acc_cache[key] = (now, info)
        return info

    def forget(self, user_id):
        with self._lock:
            self._acc_cache.pop(user_id, None)


def _int(value, default=0):
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _safe_next(nxt):
    if not nxt or not nxt.startswith("/") or nxt.startswith("//") or "\\" in nxt:
        return "/"
    return nxt


def _clean_title(title):
    return re.sub(r"\s+", " ", title or "").strip()[:MAX_TITLE]


def make_handler(app):
    class Handler(BaseHTTPRequestHandler):
        server_version = "BastionForum/1.0"
        protocol_version = "HTTP/1.1"

        def log_message(self, fmt, *args):
            if QUIET:
                return
            print("[%s] %s %s" % (time.strftime("%H:%M:%S"), self.client_address[0], fmt % args))

        # ---------- низкий уровень ----------

        def _cookies(self):
            c = http.cookies.SimpleCookie()
            try:
                c.load(self.headers.get("Cookie", ""))
            except http.cookies.CookieError:
                pass
            return {k: v.value for k, v in c.items()}

        def _cookie_header(self, name, value, max_age):
            parts = [f"{name}={value}", "Path=/", "HttpOnly", "SameSite=Lax", f"Max-Age={max_age}"]
            if app.secure_cookies:
                parts.append("Secure")
            return "; ".join(parts)

        def _send(self, code, body, ctype="text/html; charset=utf-8", headers=None):
            data = body.encode("utf-8") if isinstance(body, str) else body
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("X-Frame-Options", "SAMEORIGIN")
            self.send_header("Referrer-Policy", "same-origin")
            if ctype.startswith("text/html"):
                self.send_header("Content-Security-Policy",
                                 "default-src 'self'; img-src * data:; style-src 'self' 'unsafe-inline'; "
                                 "script-src 'self'; frame-ancestors 'self'; form-action 'self'")
                self.send_header("Cache-Control", "no-store")
            for k, v in headers or []:
                self.send_header(k, v)
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(data)

        def _form(self):
            length = _int(self.headers.get("Content-Length"), 0)
            if length > 200_000:
                raise HttpError(413, "Слишком большой запрос.")
            raw = self.rfile.read(length).decode("utf-8", "replace") if length else ""
            return {k: v[0] for k, v in urllib.parse.parse_qs(raw, keep_blank_values=True).items()}

        def _context(self):
            ctx = Context(app, self)
            cookies = self._cookies()
            sess = app.store.session(cookies.get(COOKIE))
            if sess:
                user = app.store.user(sess["user_id"])
                if user:
                    info = app.game_info(user)
                    ctx.user = user
                    ctx.perm = Perm(app.cfg, user, info["account"], info["banned"])
                    ctx.csrf = sess["csrf"]
                    ctx.session_token = sess["token"]
                    if time.time() - user["last_seen"] > 60:
                        app.store.touch_user(user["id"])
            if not ctx.csrf:
                # Гость: токен для формы входа, привязан к cookie bf_anon
                anon = cookies.get(ANON_COOKIE) or ""
                if not re.fullmatch(r"[A-Za-z0-9_-]{16,64}", anon):
                    anon = secrets.token_urlsafe(16)
                    self._new_anon = anon
                ctx.anon = anon
                ctx.csrf = hmac.new(app.anon_secret, anon.encode(), "sha256").hexdigest()[:32]
            ctx.flash = urllib.parse.unquote(cookies.get("bf_flash", ""))[:200]
            return ctx

        def _check_csrf(self, ctx, form):
            if not hmac.compare_digest(form.get("csrf", ""), ctx.csrf or "-"):
                raise HttpError(403, "Страница устарела. Обновите её и попробуйте ещё раз.")

        # ---------- точка входа ----------

        def do_HEAD(self):
            self.do_GET()

        def do_GET(self):
            self._dispatch("GET")

        def do_POST(self):
            self._dispatch("POST")

        def _dispatch(self, method):
            self._new_anon = None
            url = urllib.parse.urlsplit(self.path)
            path = url.path
            query = {k: v[0] for k, v in urllib.parse.parse_qs(url.query).items()}
            ctx = None
            extra = []
            try:
                if path.startswith("/static/"):
                    return self._static(path[8:])
                ctx = self._context()
                if self._new_anon:
                    extra.append(("Set-Cookie", self._cookie_header(ANON_COOKIE, self._new_anon, 86400 * 30)))
                if ctx.flash:
                    extra.append(("Set-Cookie", self._cookie_header("bf_flash", "", 0)))
                form = {}
                if method == "POST":
                    form = self._form()
                    self._check_csrf(ctx, form)
                result = route(app, ctx, method, path, query, form)
                if isinstance(result, tuple) and len(result) == 3:
                    code, body, ctype = result
                else:
                    code, body = result
                    ctype = "text/html; charset=utf-8"
                self._send(code, body, ctype, extra)
            except Redirect as r:
                self.send_response(303)
                self.send_header("Location", r.location)
                self.send_header("Content-Length", "0")
                for k, v in extra:
                    self.send_header(k, v)
                for c in r.cookies:
                    self.send_header("Set-Cookie", self._cookie_header(*c))
                self.end_headers()
            except HttpError as err:
                ctx = ctx or Context(app, self)
                self._send(*views.error(ctx, err.code, err.text), headers=extra)
            except (BrokenPipeError, ConnectionResetError):
                pass
            except Exception:
                traceback.print_exc()
                ctx = ctx or Context(app, self)
                self._send(*views.error(ctx, 500, "Внутренняя ошибка форума. Подробности — в окне форума."))

        def _static(self, name):
            name = urllib.parse.unquote(name)
            full = os.path.normpath(os.path.join(STATIC_DIR, name))
            if not full.startswith(STATIC_DIR + os.sep) or not os.path.isfile(full):
                self._send(404, "Not found", "text/plain; charset=utf-8")
                return
            ctype = mimetypes.guess_type(full)[0] or "application/octet-stream"
            if ctype.startswith("text/") or ctype in ("application/javascript", "image/svg+xml"):
                ctype += "; charset=utf-8"
            with open(full, "rb") as fh:
                data = fh.read()
            self._send(200, data, ctype, [("Cache-Control", "public, max-age=3600")])

    return Handler


# ------------------------------------------------------------------
#  Маршруты
# ------------------------------------------------------------------

ROUTES = []


def get(pattern):
    return _route("GET", pattern)


def post(pattern):
    return _route("POST", pattern)


def _route(method, pattern):
    rx = re.compile("^" + pattern + "$")

    def deco(fn):
        ROUTES.append((method, rx, fn))
        return fn
    return deco


def route(app, ctx, method, path, query, form):
    for m, rx, fn in ROUTES:
        match = rx.match(path)
        if match and (m == method or (m == "GET" and method == "HEAD")):
            return fn(app, ctx, query, form, *match.groups())
    if any(rx.match(path) for _, rx, _ in ROUTES):
        raise HttpError(405, "Метод не поддерживается.")
    raise HttpError(404, "Страница не найдена.")


def flash(text):
    return ("bf_flash", urllib.parse.quote(text), 30)


def need_login(ctx):
    if not ctx.user:
        raise Redirect("/login?next=" + urllib.parse.quote(ctx.path, safe="/"))


def load_forum(app, forum_id):
    forum = app.store.forum(_int(forum_id))
    if not forum:
        raise HttpError(404, "Раздел не найден.")
    return forum


def load_topic(app, topic_id):
    topic = app.store.topic(_int(topic_id))
    if not topic:
        raise HttpError(404, "Тема не найдена или удалена.")
    return topic, app.store.forum(topic["forum_id"])


def cooldown(app, ctx):
    if ctx.perm.is_moderator:
        return
    now = time.time()
    last = app._last_post.get(ctx.user["id"], 0)
    if now - last < POST_COOLDOWN:
        raise HttpError(429, f"Не так быстро: между сообщениями нужно подождать {POST_COOLDOWN} секунд.")
    app._last_post[ctx.user["id"]] = now


# ---------- сайт: главная, карта, донат ----------

NAME_RX = re.compile(r"[A-Za-z][A-Za-z]{1,19}_[A-Za-z][A-Za-z]{1,19}")
DONATE_COOLDOWN = 60
MAX_DONATE_COINS = 100000


def map_image(app):
    if app.cfg.map_image:
        return app.cfg.map_image
    return "/static/map.jpg" if os.path.isfile(os.path.join(STATIC_DIR, "map.jpg")) else ""


@get("/")
def page_site(app, ctx, query, form):
    return views.site_home(ctx, app.store.latest_topics("news", 3))


@get("/map")
def page_map(app, ctx, query, form):
    return views.map_page(ctx, map_image(app))


@get("/api/map.json")
def api_map(app, ctx, query, form):
    data = {"servers": []}
    for key, srv in app.cfg.servers.items():
        data["servers"].append({"key": key, "name": srv.name, **app.games[key].map_data()})
    return 200, json.dumps(data, ensure_ascii=False), "application/json; charset=utf-8"


def donate_rub(app, coins):
    return int(round(coins * app.cfg.donate_rub_per_coin))


@get("/donate")
def page_donate(app, ctx, query, form):
    name = ctx.user["name"] if ctx.user else ""
    server_key = ctx.user["server"] if ctx.user else ""
    return views.donate(ctx, name=name, server=server_key)


@post("/donate")
def do_donate(app, ctx, query, form):
    name = (form.get("name") or "").strip()
    server_key = (form.get("server") or "").strip().lower()
    contact = (form.get("contact") or "").strip()[:100]
    coins = _int(form.get("coins"), 0)
    if server_key not in app.games and len(app.games) == 1:
        server_key = next(iter(app.games))

    def fail(msg):
        return views.donate(ctx, name=name, server=server_key, coins=coins, contact=contact, error=msg)

    if not NAME_RX.fullmatch(name):
        return fail("Ник пишется как в игре: Имя_Фамилия латиницей.")
    gdb = app.games.get(server_key)
    if not gdb:
        return fail("Выберите сервер.")
    if gdb.available() and not gdb.account_exists(name):
        return fail(f"На сервере {app.cfg.servers[server_key].name} нет аккаунта {name}.")
    if not app.cfg.donate_min_coins <= coins <= MAX_DONATE_COINS:
        return fail(f"Количество монет: от {app.cfg.donate_min_coins} до {MAX_DONATE_COINS}.")
    now = time.time()
    key = "donate:" + ctx.ip
    if now - app._last_post.get(key, 0) < DONATE_COOLDOWN:
        return fail("Заявка уже отправлена. Новую можно через минуту.")
    app._last_post[key] = now
    req_id = app.store.add_donate_request(server_key, name, coins, donate_rub(app, coins), contact, ctx.ip)
    return views.donate_done(ctx, app.store.donate_request(req_id))


@get("/donate/requests")
def page_donate_requests(app, ctx, query, form):
    need_login(ctx)
    if not ctx.perm.is_admin:
        raise HttpError(403, "Только для администрации.")
    return views.donate_requests(ctx, app.store.donate_requests())


@post(r"/donate/requests/(\d+)")
def do_donate_request(app, ctx, query, form, req_id):
    need_login(ctx)
    if not ctx.perm.is_admin:
        raise HttpError(403, "Только для администрации.")
    req = app.store.donate_request(_int(req_id))
    if not req:
        raise HttpError(404, "Заявка не найдена.")
    status = form.get("status")
    if status not in views.DONATE_STATUSES:
        raise HttpError(400, "Неизвестный статус.")
    app.store.set_donate_status(req["id"], status, ctx.user["name"])
    raise Redirect("/donate/requests", [flash(f"Заявка №{req['id']}: {views.DONATE_STATUSES[status]}")])


# ---------- форум ----------

@get("/forum")
def page_index(app, ctx, query, form):
    return views.index(ctx, app.store.categories(), app.store.totals(), app.store.online_users(),
                       app.store.recent_topics())


@get(r"/f/(\d+)")
def page_forum(app, ctx, query, form, forum_id):
    forum = load_forum(app, forum_id)
    status = query.get("status") if query.get("status") in STATUSES else None
    total = app.store.count_topics(forum["id"], status)
    pages = max(1, (total + TOPICS_PER_PAGE - 1) // TOPICS_PER_PAGE)
    page = min(max(1, _int(query.get("page"), 1)), pages)
    topics = app.store.topics(forum["id"], (page - 1) * TOPICS_PER_PAGE, TOPICS_PER_PAGE, status)
    can_post = bool(ctx.user) and not ctx.perm.why_cannot_post(forum)
    return views.forum_page(ctx, forum, topics, page, pages, status, can_post)


@get(r"/f/(\d+)/new")
def page_new_topic(app, ctx, query, form, forum_id):
    need_login(ctx)
    forum = load_forum(app, forum_id)
    reason = ctx.perm.why_cannot_post(forum)
    if reason:
        raise HttpError(403, reason)
    return views.new_topic(ctx, forum)


@post(r"/f/(\d+)/new")
def do_new_topic(app, ctx, query, form, forum_id):
    need_login(ctx)
    forum = load_forum(app, forum_id)
    reason = ctx.perm.why_cannot_post(forum)
    if reason:
        raise HttpError(403, reason)
    title, body = _clean_title(form.get("title")), (form.get("body") or "").strip()
    error = ""
    if len(title) < 3:
        error = "Заголовок слишком короткий."
    elif len(body) < 2 or len(body) > MAX_BODY:
        error = f"Текст должен быть от 2 до {MAX_BODY} символов."
    elif forum["template"] and body.strip() == forum["template"].strip():
        error = "Заполните форму: пока в ней только вопросы без ответов."
    if error:
        return views.new_topic(ctx, forum, title, body, error)
    cooldown(app, ctx)
    tid = app.store.create_topic(forum, ctx.user["id"], title, body)
    raise Redirect(f"/t/{tid}")


def topic_perms(app, ctx, forum, topic):
    reason = ctx.perm.why_cannot_reply(forum, topic)
    return {
        "can_reply": bool(ctx.user) and not reason,
        "reply_reason": reason,
        "can_moderate": ctx.perm.can_moderate(forum),
        "can_move": ctx.perm.is_admin,
        "forums": app.store.all_forums() if ctx.perm.is_admin else [],
        "edit": lambda p: ctx.perm.can_edit(forum, topic, p),
    }


@get(r"/t/(\d+)")
def page_topic(app, ctx, query, form, topic_id):
    topic, forum = load_topic(app, topic_id)
    pages = max(1, (topic["posts_count"] + POSTS_PER_PAGE - 1) // POSTS_PER_PAGE)
    page = pages if query.get("last") else min(max(1, _int(query.get("page"), 1)), pages)
    offset = (page - 1) * POSTS_PER_PAGE
    posts = app.store.posts(topic["id"], offset, POSTS_PER_PAGE)
    authors = {}
    for p in posts:
        if p["user_id"] not in authors:
            u = app.store.user(p["user_id"])
            authors[p["user_id"]] = app.game_info(u) if u else {}
    if page == 1 and not (ctx.user and ctx.user["id"] == topic["user_id"]):
        app.store.add_view(topic["id"])
    return views.topic_page(ctx, forum, topic, posts, authors, page, pages, topic_perms(app, ctx, forum, topic),
                            offset)


@post(r"/t/(\d+)/reply")
def do_reply(app, ctx, query, form, topic_id):
    need_login(ctx)
    topic, forum = load_topic(app, topic_id)
    reason = ctx.perm.why_cannot_reply(forum, topic)
    if reason:
        raise HttpError(403, reason)
    body = (form.get("body") or "").strip()
    if len(body) < 2 or len(body) > MAX_BODY:
        raise HttpError(400, f"Сообщение должно быть от 2 до {MAX_BODY} символов.")
    cooldown(app, ctx)
    pid = app.store.add_post(topic["id"], ctx.user["id"], body)
    pos = app.store.post_position(topic["id"], pid)
    raise Redirect(f"/t/{topic['id']}?page={pos // POSTS_PER_PAGE + 1}#p{pid}")


@post(r"/t/(\d+)/mod")
def do_moderate(app, ctx, query, form, topic_id):
    need_login(ctx)
    topic, forum = load_topic(app, topic_id)
    if not ctx.perm.can_moderate(forum):
        raise HttpError(403, "Нет прав на модерацию этого раздела.")
    action = form.get("action", "")
    uid, tid = ctx.user["id"], topic["id"]
    if action.startswith("status:") and forum["kind"] in STATUS_KINDS:
        st = action.split(":", 1)[1]
        if st not in STATUSES:
            raise HttpError(400, "Неизвестный статус.")
        app.store.update_topic(tid, status=st, locked=0 if st == "open" else 1)
        app.store.add_post(tid, uid, f"[b]Статус изменён: {STATUSES[st]}.[/b]" +
                           ("" if st == "open" else " Тема закрыта."))
        app.store.log(uid, "статус: " + STATUSES[st], tid)
        raise Redirect(f"/t/{tid}?last=1")
    if action in ("pin", "unpin"):
        app.store.update_topic(tid, pinned=1 if action == "pin" else 0)
    elif action in ("lock", "unlock"):
        app.store.update_topic(tid, locked=1 if action == "lock" else 0)
    elif action == "move" and ctx.perm.is_admin:
        target = app.store.forum(_int(form.get("forum_id")))
        if not target:
            raise HttpError(400, "Раздел не найден.")
        status = topic["status"] or ("open" if target["kind"] in STATUS_KINDS else "")
        if target["kind"] not in STATUS_KINDS:
            status = ""
        app.store.update_topic(tid, forum_id=target["id"], status=status)
        app.store.log(uid, "перенос", tid, f'{forum["title"]} → {target["title"]}')
        raise Redirect(f"/t/{tid}", [flash("Тема перенесена.")])
    elif action == "delete":
        app.store.log(uid, "удаление темы", 0, topic["title"])
        app.store.delete_topic(tid)
        raise Redirect(f"/f/{forum['id']}", [flash("Тема удалена.")])
    else:
        raise HttpError(400, "Неизвестное действие.")
    app.store.log(uid, action, tid)
    raise Redirect(f"/t/{tid}")


@get(r"/p/(\d+)/edit")
def page_edit(app, ctx, query, form, post_id):
    need_login(ctx)
    post_, topic, forum = _load_post(app, post_id)
    if not ctx.perm.can_edit(forum, topic, post_):
        raise HttpError(403, "Это сообщение нельзя изменить.")
    return views.edit_post(ctx, forum, topic, post_, app.store.first_post_id(topic["id"]) == post_["id"])


@post(r"/p/(\d+)/edit")
def do_edit(app, ctx, query, form, post_id):
    need_login(ctx)
    post_, topic, forum = _load_post(app, post_id)
    if not ctx.perm.can_edit(forum, topic, post_):
        raise HttpError(403, "Это сообщение нельзя изменить.")
    body = (form.get("body") or "").strip()
    is_first = app.store.first_post_id(topic["id"]) == post_["id"]
    if len(body) < 2 or len(body) > MAX_BODY:
        return views.edit_post(ctx, forum, topic, dict(post_, body=body), is_first,
                               f"Сообщение должно быть от 2 до {MAX_BODY} символов.")
    app.store.edit_post(post_["id"], body, ctx.user["id"])
    if is_first:
        title = _clean_title(form.get("title"))
        if len(title) >= 3:
            app.store.update_topic(topic["id"], title=title)
    if post_["user_id"] != ctx.user["id"]:
        app.store.log(ctx.user["id"], "правка чужого сообщения", topic["id"])
    pos = app.store.post_position(topic["id"], post_["id"])
    raise Redirect(f"/t/{topic['id']}?page={pos // POSTS_PER_PAGE + 1}#p{post_['id']}")


@post(r"/p/(\d+)/delete")
def do_delete_post(app, ctx, query, form, post_id):
    need_login(ctx)
    post_, topic, forum = _load_post(app, post_id)
    if not ctx.perm.can_moderate(forum):
        raise HttpError(403, "Нет прав на удаление.")
    if app.store.first_post_id(topic["id"]) == post_["id"]:
        app.store.log(ctx.user["id"], "удаление темы", 0, topic["title"])
        app.store.delete_topic(topic["id"])
        raise Redirect(f"/f/{forum['id']}", [flash("Первое сообщение удалено вместе с темой.")])
    app.store.delete_post(post_)
    app.store.log(ctx.user["id"], "удаление сообщения", topic["id"], markup.plain(post_["body"], 80))
    raise Redirect(f"/t/{topic['id']}?last=1", [flash("Сообщение удалено.")])


def _load_post(app, post_id):
    post_ = app.store.post(_int(post_id))
    if not post_:
        raise HttpError(404, "Сообщение не найдено.")
    topic, forum = load_topic(app, post_["topic_id"])
    return post_, topic, forum


# ---------- вход ----------

@get("/login")
def page_login(app, ctx, query, form):
    if ctx.user:
        raise Redirect(_safe_next(query.get("next")))
    return views.login(ctx, _safe_next(query.get("next")))


@post("/login")
def do_login(app, ctx, query, form):
    nxt = _safe_next(form.get("next"))
    name = (form.get("name") or "").strip()
    server_key = (form.get("server") or "").strip().lower()
    password = form.get("password") or ""
    if server_key not in app.games and len(app.games) == 1:
        server_key = next(iter(app.games))
    gdb = app.games.get(server_key)
    if not gdb:
        return views.login(ctx, nxt, name, server_key, "Выберите сервер.")

    now = time.time()
    with app._lock:
        fails = [t for t in app._login_fails.get(name.lower(), []) if now - t < LOGIN_WINDOW]
        app._login_fails[name.lower()] = fails
    if len(fails) >= LOGIN_TRIES:
        return views.login(ctx, nxt, name, server_key,
                           "Слишком много неверных попыток. Подождите 10 минут.")
    if not gdb.available():
        return views.login(ctx, nxt, name, server_key,
                           "База игрового сервера недоступна. Сообщите администрации.")
    acc = gdb.account_by_name(name)
    if not acc or not game.check_password(password, acc.get("salt"), acc.get("password")):
        with app._lock:
            app._login_fails.setdefault(name.lower(), []).append(now)
        return views.login(ctx, nxt, name, server_key,
                           "Неверный ник или пароль. Используйте данные от игрового аккаунта.")
    with app._lock:
        app._login_fails.pop(name.lower(), None)
    uid = app.store.upsert_user(server_key, acc["id"], acc["name"])
    app.forget(uid)
    token = app.store.create_session(uid)
    raise Redirect(nxt, [(COOKIE, token, 86400 * 30), flash(f"Добро пожаловать, {views.rp(acc['name'])}!")])


@post("/logout")
def do_logout(app, ctx, query, form):
    if ctx.user:
        app.store.delete_session(getattr(ctx, "session_token", ""))
    raise Redirect("/", [(COOKIE, "", 0)])


# ---------- профиль и прочее ----------

@get(r"/u/(\d+)")
def page_profile(app, ctx, query, form, user_id):
    user = app.store.user(_int(user_id))
    if not user:
        raise HttpError(404, "Пользователь не найден.")
    posts, topics = app.store.user_stats(user["id"])
    is_self = bool(ctx.user) and ctx.user["id"] == user["id"]
    return views.profile(ctx, user, app.game_info(user), posts, topics,
                         ctx.perm.is_admin and not is_self, is_self)


@post(r"/u/(\d+)/signature")
def do_signature(app, ctx, query, form, user_id):
    need_login(ctx)
    if ctx.user["id"] != _int(user_id) or ctx.perm.forum_banned:
        raise HttpError(403, "Нельзя менять чужую подпись.")
    app.store.set_signature(ctx.user["id"], (form.get("signature") or "").strip()[:300])
    raise Redirect(f"/u/{ctx.user['id']}", [flash("Подпись сохранена.")])


@post(r"/u/(\d+)/ban")
def do_forum_ban(app, ctx, query, form, user_id):
    need_login(ctx)
    if not ctx.perm.is_admin:
        raise HttpError(403, "Блокировать может только администратор форума.")
    user = app.store.user(_int(user_id))
    if not user or user["id"] == ctx.user["id"]:
        raise HttpError(400, "Пользователь не найден.")
    days = max(0, min(3650, _int(form.get("days"))))
    reason = (form.get("reason") or "").strip()[:120]
    until = int(time.time()) + days * 86400 if days else 0
    app.store.set_forum_ban(user["id"], until, reason)
    app.store.log(ctx.user["id"], "блокировка на форуме" if days else "снятие блокировки", 0,
                  f'{user["name"]}: {days} дн. {reason}')
    raise Redirect(f"/u/{user['id']}", [flash("Готово.")])


@get("/team")
def page_team(app, ctx, query, form):
    per_server = []
    for key, srv in app.cfg.servers.items():
        gdb = app.games[key]
        leaders = []
        for f in game.FACTIONS:
            if f:
                leaders += [(f, m) for m in gdb.faction_members(f) if int(m["frank"] or 0) >= game.LEADER_RANK]
        per_server.append((srv, gdb.admins(), leaders))
    return views.team(ctx, per_server)


@get("/bans")
def page_bans(app, ctx, query, form):
    return views.bans(ctx, [(srv, app.games[k].bans()) for k, srv in app.cfg.servers.items()])


@get("/search")
def page_search(app, ctx, query, form):
    q = (query.get("q") or "").strip()[:60]
    return views.search(ctx, q, app.store.search(q) if len(q) >= 3 else [])


@get("/modlog")
def page_modlog(app, ctx, query, form):
    need_login(ctx)
    if not ctx.perm.is_moderator:
        raise HttpError(403, "Только для администрации.")
    return views.modlog(ctx, app.store.mod_log())


# ---------- для лаунчеров ----------

@get("/news.txt")
def news_txt(app, ctx, query, form):
    """Новости в формате launcher/news.txt: дата | заголовок | текст (UTF-8)."""
    lines = ["; Новости с форума " + app.cfg.title]
    for t in app.store.latest_topics("news", 10):
        def clean(s):
            return s.replace("|", "/").replace("\n", " ").strip()
        lines.append(f'{views.date(t["created"])} | {clean(t["title"])} | {clean(markup.plain(t["body"], 200))}')
    return 200, "\n".join(lines) + "\n", "text/plain; charset=utf-8"


@get("/api/status.json")
def api_status(app, ctx, query, form):
    data = {"servers": [], "news": []}
    for key, srv in app.cfg.servers.items():
        st = game.server_status(srv)
        data["servers"].append({"key": key, "name": srv.name, "address": srv.address, "port": srv.port, **st})
    for t in app.store.latest_topics("news", 5):
        data["news"].append({"id": t["id"], "title": t["title"], "date": views.date(t["created"]),
                             "text": markup.plain(t["body"], 300),
                             "url": f'{app.cfg.public_url}/t/{t["id"]}'})
    return 200, json.dumps(data, ensure_ascii=False), "application/json; charset=utf-8"


def serve(cfg):
    app = App(cfg)
    httpd = ThreadingHTTPServer((cfg.host, cfg.port), make_handler(app))
    httpd.daemon_threads = True
    return app, httpd
