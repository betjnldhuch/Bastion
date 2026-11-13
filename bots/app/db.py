"""Работа с базой сервера rp.db.

Бот пишет только в свои таблицы (social_links, social_codes, bot_outbox, bot_actions).
Таблицу accounts только читает: пароль меняет сам мод по просьбе из bot_actions.
"""

import os
import sqlite3
import time

SCHEMA = [
    "CREATE TABLE IF NOT EXISTS social_links (id INTEGER PRIMARY KEY AUTOINCREMENT, account INTEGER NOT NULL,"
    " platform TEXT NOT NULL, user_id INTEGER NOT NULL, notify INTEGER DEFAULT 1, linked INTEGER DEFAULT 0,"
    " UNIQUE (account, platform))",
    "CREATE INDEX IF NOT EXISTS social_links_user ON social_links (platform, user_id)",
    "CREATE TABLE IF NOT EXISTS social_codes (code TEXT PRIMARY KEY, account INTEGER NOT NULL, until INTEGER NOT NULL)",
    "CREATE TABLE IF NOT EXISTS bot_outbox (id INTEGER PRIMARY KEY AUTOINCREMENT, account INTEGER NOT NULL,"
    " text TEXT NOT NULL, created INTEGER DEFAULT 0)",
    "CREATE TABLE IF NOT EXISTS bot_actions (id INTEGER PRIMARY KEY AUTOINCREMENT, account INTEGER NOT NULL,"
    " action TEXT NOT NULL, data TEXT DEFAULT '', created INTEGER DEFAULT 0)",
]

ACCOUNT_COLUMNS = ["id", "name", "level", "exp", "money", "bank", "admin", "job", "faction", "frank",
                   "wanted", "phone", "reg_date"]


def _text(raw):
    """Мод пишет текст в Windows-1251, бот — в UTF-8. Читаем оба."""
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("cp1251", "replace")


class GameDB:
    def __init__(self, server):
        self.server = server
        self._ready = False

    def available(self):
        return bool(self.server.db) and os.path.exists(self.server.db)

    def _connect(self):
        con = sqlite3.connect(self.server.db, timeout=5, isolation_level=None)
        con.text_factory = _text
        con.row_factory = sqlite3.Row
        con.execute("PRAGMA busy_timeout = 5000")
        if not self._ready:
            for sql in SCHEMA:
                con.execute(sql)
            self._ready = True
        return con

    def _query(self, sql, args=()):
        con = self._connect()
        try:
            return [dict(r) for r in con.execute(sql, args)]
        finally:
            con.close()

    def _exec(self, sql, args=()):
        con = self._connect()
        try:
            return con.execute(sql, args).rowcount
        finally:
            con.close()

    # --- аккаунты ---------------------------------------------------
    def _account_select(self, con, prefix=""):
        cols = {r[1] for r in con.execute("SELECT * FROM pragma_table_info('accounts')")}
        return ", ".join(prefix + c if c in cols else "0 AS " + c for c in ACCOUNT_COLUMNS)

    def account(self, account_id):
        con = self._connect()
        try:
            row = con.execute(f"SELECT {self._account_select(con)} FROM accounts WHERE id = ?", (account_id,)).fetchone()
            return dict(row) if row else None
        finally:
            con.close()

    def ban_of(self, name):
        rows = self._query("SELECT reason, admin, date, until FROM bans WHERE name = ? COLLATE NOCASE", (name,)) \
            if self._has_column("bans", "until") else \
            self._query("SELECT reason, admin, date, 0 AS until FROM bans WHERE name = ? COLLATE NOCASE", (name,))
        return rows[0] if rows else None

    def _has_column(self, table, column):
        return bool(self._query("SELECT 1 FROM pragma_table_info(?) WHERE name = ?", (table, column)))

    def top(self, limit=10):
        return self._query("SELECT name, level, exp FROM accounts WHERE admin = 0 ORDER BY level DESC, exp DESC LIMIT ?",
                           (limit,))

    # --- привязка ---------------------------------------------------
    def take_code(self, code):
        """Код из /bot -> id аккаунта. Код одноразовый."""
        con = self._connect()
        try:
            con.execute("BEGIN IMMEDIATE")
            row = con.execute("SELECT account, until FROM social_codes WHERE code = ?", (code,)).fetchone()
            if row:
                con.execute("DELETE FROM social_codes WHERE code = ?", (code,))
            con.execute("COMMIT")
        finally:
            con.close()
        if not row or row["until"] < time.time():
            return None
        return row["account"]

    def link(self, platform, user_id, account_id):
        """Привязать. Если аккаунт уже был привязан к другому ВК/ТГ — перепривязываем."""
        self._exec("INSERT OR REPLACE INTO social_links (account, platform, user_id, notify, linked) VALUES (?, ?, ?, 1, ?)",
                   (account_id, platform, user_id, int(time.time())))

    def unlink(self, platform, user_id, account_id):
        return self._exec("DELETE FROM social_links WHERE platform = ? AND user_id = ? AND account = ?",
                          (platform, user_id, account_id))

    def links_of(self, platform, user_id):
        """Аккаунты, привязанные к этому ВК/ТГ."""
        con = self._connect()
        try:
            sel = self._account_select(con, "a.")
            rows = con.execute(
                f"SELECT {sel}, s.notify AS notify FROM social_links s JOIN accounts a ON a.id = s.account"
                " WHERE s.platform = ? AND s.user_id = ? ORDER BY a.name", (platform, user_id)).fetchall()
            return [dict(r) for r in rows]
        finally:
            con.close()

    def set_notify(self, platform, user_id, on):
        self._exec("UPDATE social_links SET notify = ? WHERE platform = ? AND user_id = ?", (1 if on else 0, platform, user_id))

    def users(self, platform):
        return [r["user_id"] for r in self._query("SELECT DISTINCT user_id FROM social_links WHERE platform = ?", (platform,))]

    # --- общение с модом -------------------------------------------
    def add_action(self, account_id, action, data=""):
        self._exec("INSERT INTO bot_actions (account, action, data, created) VALUES (?, ?, ?, ?)",
                   (account_id, action, data, int(time.time())))

    def take_outbox(self, limit=50):
        """Уведомления из игры: [(текст, [(платформа, user_id), ...])]. Забранные строки удаляются."""
        con = self._connect()
        try:
            con.execute("BEGIN IMMEDIATE")
            rows = con.execute("SELECT id, account, text FROM bot_outbox ORDER BY id LIMIT ?", (limit,)).fetchall()
            result = []
            for r in rows:
                links = con.execute("SELECT platform, user_id FROM social_links WHERE account = ? AND notify = 1",
                                    (r["account"],)).fetchall()
                text = _text(r["text"]) if isinstance(r["text"], bytes) else r["text"]
                result.append((text, [(l["platform"], l["user_id"]) for l in links]))
            if rows:
                con.execute("DELETE FROM bot_outbox WHERE id <= ?", (rows[-1]["id"],))
            con.execute("COMMIT")
            return result
        finally:
            con.close()
