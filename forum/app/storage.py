"""База самого форума: data/forum.db (темы, сообщения, сессии)."""

import os
import secrets
import sqlite3
import time
from contextlib import contextmanager

SCHEMA = """
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    server TEXT NOT NULL,
    game_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    created INTEGER NOT NULL,
    last_seen INTEGER NOT NULL,
    signature TEXT NOT NULL DEFAULT '',
    forum_ban_until INTEGER NOT NULL DEFAULT 0,
    forum_ban_reason TEXT NOT NULL DEFAULT '',
    UNIQUE (server, game_id)
);
CREATE TABLE IF NOT EXISTS categories (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    sort INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS forums (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    category_id INTEGER NOT NULL REFERENCES categories(id),
    title TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    kind TEXT NOT NULL DEFAULT 'normal',
    faction INTEGER NOT NULL DEFAULT 0,
    template TEXT NOT NULL DEFAULT '',
    sort INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS topics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    forum_id INTEGER NOT NULL REFERENCES forums(id),
    user_id INTEGER NOT NULL REFERENCES users(id),
    title TEXT NOT NULL,
    created INTEGER NOT NULL,
    last_post_at INTEGER NOT NULL,
    last_user_id INTEGER NOT NULL,
    posts_count INTEGER NOT NULL DEFAULT 0,
    views INTEGER NOT NULL DEFAULT 0,
    pinned INTEGER NOT NULL DEFAULT 0,
    locked INTEGER NOT NULL DEFAULT 0,
    status TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS topics_forum ON topics (forum_id, pinned DESC, last_post_at DESC);
CREATE TABLE IF NOT EXISTS posts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    topic_id INTEGER NOT NULL REFERENCES topics(id),
    user_id INTEGER NOT NULL REFERENCES users(id),
    body TEXT NOT NULL,
    created INTEGER NOT NULL,
    edited INTEGER NOT NULL DEFAULT 0,
    edited_by INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS posts_topic ON posts (topic_id, id);
CREATE INDEX IF NOT EXISTS posts_user ON posts (user_id);
CREATE TABLE IF NOT EXISTS sessions (
    token TEXT PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id),
    csrf TEXT NOT NULL,
    created INTEGER NOT NULL,
    expires INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS mod_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    action TEXT NOT NULL,
    topic_id INTEGER NOT NULL DEFAULT 0,
    details TEXT NOT NULL DEFAULT '',
    created INTEGER NOT NULL
);
"""

SESSION_DAYS = 30

# Статусы заявок и жалоб
STATUSES = {
    "open": "На рассмотрении",
    "approved": "Одобрено",
    "rejected": "Отказано",
    "closed": "Закрыто",
}
STATUS_KINDS = ("complaint", "appeal", "application")

COMPLAINT_TEMPLATE = """[b]1. Ваш ник:[/b]
[b]2. Ник нарушителя:[/b]
[b]3. Сервер (Peace / War):[/b]
[b]4. Дата и время (МСК):[/b]
[b]5. Суть нарушения:[/b]
[b]6. Доказательства (скриншоты, видео):[/b] """

ADMIN_COMPLAINT_TEMPLATE = """[b]1. Ваш ник:[/b]
[b]2. Ник администратора:[/b]
[b]3. Сервер (Peace / War):[/b]
[b]4. Дата и время (МСК):[/b]
[b]5. Что произошло:[/b]
[b]6. Доказательства (скриншоты, видео):[/b] """

APPEAL_TEMPLATE = """[b]1. Ваш ник:[/b]
[b]2. Ник администратора, выдавшего наказание:[/b]
[b]3. Наказание и причина (как в игре):[/b]
[b]4. Дата наказания:[/b]
[b]5. Почему наказание стоит снять или смягчить:[/b]
[b]6. Доказательства (если есть):[/b] """

ADMIN_APP_TEMPLATE = """[b]1. Ник в игре:[/b]
[b]2. Уровень персонажа:[/b]
[b]3. Ваш возраст:[/b]
[b]4. Сколько часов в день готовы уделять серверу:[/b]
[b]5. Был ли опыт администрирования:[/b]
[b]6. Были ли наказания на сервере:[/b]
[b]7. Почему именно вы:[/b]
[b]8. Discord для связи:[/b] """

FACTION_APP_TEMPLATE = """[b]OOC информация[/b]
[b]1. Ник в игре:[/b]
[b]2. Уровень персонажа:[/b]
[b]3. Ваш возраст:[/b]
[b]4. Discord для связи:[/b]

[b]IC информация[/b]
[b]5. Имя и фамилия персонажа:[/b]
[b]6. Возраст персонажа:[/b]
[b]7. Краткая биография:[/b]
[b]8. Почему хотите вступить:[/b] """

BUG_TEMPLATE = """[b]1. Сервер (Peace / War) или лаунчер (ПК / телефон):[/b]
[b]2. Что случилось:[/b]
[b]3. Как повторить (по шагам):[/b]
[b]4. Что должно было произойти:[/b]
[b]5. Скриншоты, видео, текст ошибки:[/b] """

SUGGEST_TEMPLATE = """[b]1. Суть предложения:[/b]
[b]2. Как это должно работать:[/b]
[b]3. Чем это полезно серверу:[/b] """


def default_structure():
    """Разделы, создаваемые при первом запуске. Потом их можно менять прямо в forum.db."""
    from .game import FACTIONS

    factions = [
        (f"Заявления: {name}", f"Заявления на вступление во фракцию «{name}». Рассматривают лидер и заместитель.",
         "application", fid, FACTION_APP_TEMPLATE)
        for fid, name in FACTIONS.items() if fid
    ]
    return [
        ("Информация", [
            ("Новости проекта", "Обновления, события, открытие серверов.", "news", 0, ""),
            ("Правила сервера", "Общие правила, правила фракций, RP-термины.", "rules", 0, ""),
            ("Помощь новичкам", "Гайды, ответы на частые вопросы, как начать игру.", "normal", 0, ""),
        ]),
        ("Администрация", [
            ("Жалобы на игроков", "Нарушения правил игроками. Пишите строго по форме.", "complaint", 0,
             COMPLAINT_TEMPLATE),
            ("Жалобы на администрацию", "Если считаете, что администратор поступил неправильно.", "complaint", 0,
             ADMIN_COMPLAINT_TEMPLATE),
            ("Обжалование наказаний", "Бан, мут, тюрьма. Писать могут и заблокированные в игре.", "appeal", 0,
             APPEAL_TEMPLATE),
            ("Заявки в администрацию", "Хочешь помогать серверу? Подай заявку.", "application", 0,
             ADMIN_APP_TEMPLATE),
        ]),
        ("Фракции", factions),
        ("Сообщество", [
            ("Общение", "Разговоры обо всём, знакомства, поиск компании.", "normal", 0, ""),
            ("Барахолка", "Продажа и покупка домов, машин и бизнесов.", "normal", 0, ""),
            ("Предложения по серверу", "Идеи для новых систем и улучшений.", "normal", 0, SUGGEST_TEMPLATE),
            ("Баг-репорты", "Нашёл ошибку на сервере или в лаунчере — расскажи.", "normal", 0, BUG_TEMPLATE),
        ]),
    ]


class Storage:
    def __init__(self, path):
        self.path = path
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        with self.db() as con:
            con.executescript(SCHEMA)
            if con.execute("SELECT COUNT(*) FROM categories").fetchone()[0] == 0:
                self._seed(con)

    @contextmanager
    def db(self):
        con = sqlite3.connect(self.path, timeout=10)
        con.row_factory = sqlite3.Row
        con.execute("PRAGMA foreign_keys = ON")
        con.execute("PRAGMA journal_mode = WAL")
        try:
            yield con
            con.commit()
        except Exception:
            con.rollback()
            raise
        finally:
            con.close()

    def _seed(self, con):
        for ci, (title, forums) in enumerate(default_structure()):
            cid = con.execute("INSERT INTO categories (title, sort) VALUES (?, ?)", (title, ci)).lastrowid
            for fi, (ftitle, desc, kind, faction, template) in enumerate(forums):
                con.execute(
                    "INSERT INTO forums (category_id, title, description, kind, faction, template, sort)"
                    " VALUES (?, ?, ?, ?, ?, ?, ?)", (cid, ftitle, desc, kind, faction, template, fi))

    # ---------------- пользователи и сессии ----------------

    def upsert_user(self, server, game_id, name):
        now = int(time.time())
        with self.db() as con:
            row = con.execute("SELECT id FROM users WHERE server = ? AND game_id = ?", (server, game_id)).fetchone()
            if row:
                con.execute("UPDATE users SET name = ?, last_seen = ? WHERE id = ?", (name, now, row["id"]))
                return row["id"]
            return con.execute(
                "INSERT INTO users (server, game_id, name, created, last_seen) VALUES (?, ?, ?, ?, ?)",
                (server, game_id, name, now, now)).lastrowid

    def user(self, user_id):
        with self.db() as con:
            row = con.execute("SELECT * FROM users WHERE id = ?", (user_id,)).fetchone()
            return dict(row) if row else None

    def touch_user(self, user_id):
        with self.db() as con:
            con.execute("UPDATE users SET last_seen = ? WHERE id = ?", (int(time.time()), user_id))

    def set_signature(self, user_id, text):
        with self.db() as con:
            con.execute("UPDATE users SET signature = ? WHERE id = ?", (text, user_id))

    def set_forum_ban(self, user_id, until, reason):
        with self.db() as con:
            con.execute("UPDATE users SET forum_ban_until = ?, forum_ban_reason = ? WHERE id = ?",
                        (until, reason, user_id))

    def create_session(self, user_id):
        token, csrf = secrets.token_urlsafe(32), secrets.token_urlsafe(16)
        now = int(time.time())
        with self.db() as con:
            con.execute("DELETE FROM sessions WHERE expires < ?", (now,))
            con.execute("INSERT INTO sessions (token, user_id, csrf, created, expires) VALUES (?, ?, ?, ?, ?)",
                        (token, user_id, csrf, now, now + SESSION_DAYS * 86400))
        return token

    def session(self, token):
        if not token:
            return None
        with self.db() as con:
            row = con.execute("SELECT * FROM sessions WHERE token = ? AND expires > ?",
                              (token, int(time.time()))).fetchone()
            return dict(row) if row else None

    def delete_session(self, token):
        with self.db() as con:
            con.execute("DELETE FROM sessions WHERE token = ?", (token,))

    def online_users(self, minutes=15):
        with self.db() as con:
            return [dict(r) for r in con.execute(
                "SELECT id, name, server FROM users WHERE last_seen > ? ORDER BY name",
                (int(time.time()) - minutes * 60,))]

    # ---------------- разделы ----------------

    def categories(self):
        with self.db() as con:
            cats = [dict(r) for r in con.execute("SELECT * FROM categories ORDER BY sort, id")]
            forums = [dict(r) for r in con.execute(
                """SELECT f.*,
                      (SELECT COUNT(*) FROM topics t WHERE t.forum_id = f.id) AS topics_count,
                      (SELECT COALESCE(SUM(t.posts_count), 0) FROM topics t WHERE t.forum_id = f.id) AS posts_count
                   FROM forums f ORDER BY f.sort, f.id""")]
            last = {r["forum_id"]: dict(r) for r in con.execute(
                """SELECT t.forum_id, t.id AS topic_id, t.title, t.last_post_at, u.id AS user_id, u.name AS user_name
                   FROM (SELECT *, ROW_NUMBER() OVER (PARTITION BY forum_id
                                                      ORDER BY last_post_at DESC, id DESC) AS rn
                         FROM topics) t
                   JOIN users u ON u.id = t.last_user_id WHERE t.rn = 1""")}
        for f in forums:
            f["last"] = last.get(f["id"])
        for c in cats:
            c["forums"] = [f for f in forums if f["category_id"] == c["id"]]
        return cats

    def forum(self, forum_id):
        with self.db() as con:
            row = con.execute("SELECT * FROM forums WHERE id = ?", (forum_id,)).fetchone()
            return dict(row) if row else None

    def all_forums(self):
        with self.db() as con:
            return [dict(r) for r in con.execute("SELECT * FROM forums ORDER BY category_id, sort, id")]

    # ---------------- темы ----------------

    def topics(self, forum_id, offset, limit, status=None):
        sql = """SELECT t.*, a.name AS author_name, a.server AS author_server, l.name AS last_name
                 FROM topics t JOIN users a ON a.id = t.user_id JOIN users l ON l.id = t.last_user_id
                 WHERE t.forum_id = ?"""
        args = [forum_id]
        if status:
            sql += " AND t.status = ?"
            args.append(status)
        sql += " ORDER BY t.pinned DESC, t.last_post_at DESC LIMIT ? OFFSET ?"
        args += [limit, offset]
        with self.db() as con:
            return [dict(r) for r in con.execute(sql, args)]

    def count_topics(self, forum_id, status=None):
        with self.db() as con:
            if status:
                return con.execute("SELECT COUNT(*) FROM topics WHERE forum_id = ? AND status = ?",
                                   (forum_id, status)).fetchone()[0]
            return con.execute("SELECT COUNT(*) FROM topics WHERE forum_id = ?", (forum_id,)).fetchone()[0]

    def topic(self, topic_id):
        with self.db() as con:
            row = con.execute(
                "SELECT t.*, u.name AS author_name FROM topics t JOIN users u ON u.id = t.user_id WHERE t.id = ?",
                (topic_id,)).fetchone()
            return dict(row) if row else None

    def create_topic(self, forum, user_id, title, body):
        now = int(time.time())
        status = "open" if forum["kind"] in STATUS_KINDS else ""
        with self.db() as con:
            tid = con.execute(
                """INSERT INTO topics (forum_id, user_id, title, created, last_post_at, last_user_id, posts_count, status)
                   VALUES (?, ?, ?, ?, ?, ?, 1, ?)""", (forum["id"], user_id, title, now, now, user_id, status)).lastrowid
            con.execute("INSERT INTO posts (topic_id, user_id, body, created) VALUES (?, ?, ?, ?)",
                        (tid, user_id, body, now))
        return tid

    def add_view(self, topic_id):
        with self.db() as con:
            con.execute("UPDATE topics SET views = views + 1 WHERE id = ?", (topic_id,))

    def update_topic(self, topic_id, **fields):
        allowed = {"title", "pinned", "locked", "status", "forum_id"}
        sets = {k: v for k, v in fields.items() if k in allowed}
        if not sets:
            return
        with self.db() as con:
            con.execute("UPDATE topics SET " + ", ".join(f"{k} = ?" for k in sets) + " WHERE id = ?",
                        (*sets.values(), topic_id))

    def delete_topic(self, topic_id):
        with self.db() as con:
            con.execute("DELETE FROM posts WHERE topic_id = ?", (topic_id,))
            con.execute("DELETE FROM topics WHERE id = ?", (topic_id,))

    def latest_topics(self, kind, limit=10):
        with self.db() as con:
            return [dict(r) for r in con.execute(
                """SELECT t.*, u.name AS author_name,
                          (SELECT body FROM posts p WHERE p.topic_id = t.id ORDER BY p.id LIMIT 1) AS body
                   FROM topics t JOIN forums f ON f.id = t.forum_id JOIN users u ON u.id = t.user_id
                   WHERE f.kind = ? ORDER BY t.created DESC, t.id DESC LIMIT ?""", (kind, limit))]

    def recent_topics(self, limit=8):
        with self.db() as con:
            return [dict(r) for r in con.execute(
                """SELECT t.id, t.title, t.last_post_at, t.forum_id, f.title AS forum_title, u.name AS last_name,
                          u.id AS last_user_id
                   FROM topics t JOIN forums f ON f.id = t.forum_id JOIN users u ON u.id = t.last_user_id
                   ORDER BY t.last_post_at DESC LIMIT ?""", (limit,))]

    # ---------------- сообщения ----------------

    def posts(self, topic_id, offset, limit):
        with self.db() as con:
            return [dict(r) for r in con.execute(
                """SELECT p.*, u.name AS user_name, u.server AS user_server, u.signature, u.game_id,
                          (SELECT COUNT(*) FROM posts p2 WHERE p2.user_id = p.user_id) AS user_posts
                   FROM posts p JOIN users u ON u.id = p.user_id
                   WHERE p.topic_id = ? ORDER BY p.id LIMIT ? OFFSET ?""", (topic_id, limit, offset))]

    def post(self, post_id):
        with self.db() as con:
            row = con.execute("SELECT * FROM posts WHERE id = ?", (post_id,)).fetchone()
            return dict(row) if row else None

    def first_post_id(self, topic_id):
        with self.db() as con:
            row = con.execute("SELECT MIN(id) FROM posts WHERE topic_id = ?", (topic_id,)).fetchone()
            return row[0]

    def post_position(self, topic_id, post_id):
        with self.db() as con:
            return con.execute("SELECT COUNT(*) FROM posts WHERE topic_id = ? AND id < ?",
                               (topic_id, post_id)).fetchone()[0]

    def add_post(self, topic_id, user_id, body):
        now = int(time.time())
        with self.db() as con:
            pid = con.execute("INSERT INTO posts (topic_id, user_id, body, created) VALUES (?, ?, ?, ?)",
                              (topic_id, user_id, body, now)).lastrowid
            con.execute("UPDATE topics SET posts_count = posts_count + 1, last_post_at = ?, last_user_id = ?"
                        " WHERE id = ?", (now, user_id, topic_id))
        return pid

    def edit_post(self, post_id, body, editor_id):
        with self.db() as con:
            con.execute("UPDATE posts SET body = ?, edited = ?, edited_by = ? WHERE id = ?",
                        (body, int(time.time()), editor_id, post_id))

    def delete_post(self, post):
        with self.db() as con:
            con.execute("DELETE FROM posts WHERE id = ?", (post["id"],))
            last = con.execute("SELECT user_id, created FROM posts WHERE topic_id = ? ORDER BY id DESC LIMIT 1",
                               (post["topic_id"],)).fetchone()
            con.execute("UPDATE topics SET posts_count = posts_count - 1, last_post_at = ?, last_user_id = ?"
                        " WHERE id = ?", (last["created"], last["user_id"], post["topic_id"]))

    def user_stats(self, user_id):
        with self.db() as con:
            posts = con.execute("SELECT COUNT(*) FROM posts WHERE user_id = ?", (user_id,)).fetchone()[0]
            topics = [dict(r) for r in con.execute(
                """SELECT t.id, t.title, t.created, t.status, f.title AS forum_title FROM topics t
                   JOIN forums f ON f.id = t.forum_id WHERE t.user_id = ? ORDER BY t.created DESC LIMIT 10""",
                (user_id,))]
        return posts, topics

    def search(self, q, limit=50):
        like = "%" + q.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_") + "%"
        with self.db() as con:
            return [dict(r) for r in con.execute(
                """SELECT t.id, t.title, t.last_post_at, f.title AS forum_title, u.name AS author_name
                   FROM topics t JOIN forums f ON f.id = t.forum_id JOIN users u ON u.id = t.user_id
                   WHERE t.title LIKE ? ESCAPE '\\'
                      OR t.id IN (SELECT topic_id FROM posts WHERE body LIKE ? ESCAPE '\\')
                   ORDER BY t.last_post_at DESC LIMIT ?""", (like, like, limit))]

    def totals(self):
        with self.db() as con:
            return {
                "users": con.execute("SELECT COUNT(*) FROM users").fetchone()[0],
                "topics": con.execute("SELECT COUNT(*) FROM topics").fetchone()[0],
                "posts": con.execute("SELECT COUNT(*) FROM posts").fetchone()[0],
            }

    def log(self, user_id, action, topic_id=0, details=""):
        with self.db() as con:
            con.execute("INSERT INTO mod_log (user_id, action, topic_id, details, created) VALUES (?, ?, ?, ?, ?)",
                        (user_id, action, topic_id, details, int(time.time())))

    def mod_log(self, limit=100):
        with self.db() as con:
            return [dict(r) for r in con.execute(
                """SELECT m.*, u.name AS user_name, t.title AS topic_title FROM mod_log m
                   JOIN users u ON u.id = m.user_id LEFT JOIN topics t ON t.id = m.topic_id
                   ORDER BY m.id DESC LIMIT ?""", (limit,))]
