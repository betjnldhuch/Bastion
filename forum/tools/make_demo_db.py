"""Тестовая база rp.db в формате мода (accounts, bans) — чтобы посмотреть форум без игрового сервера."""

import hashlib
import os
import secrets
import sqlite3
import time

ACCOUNTS = [
    # name, admin, faction, frank, level
    ("Pal_Layach", 5, 0, 0, 12),
    ("Ivan_Petrov", 0, 0, 0, 3),
    ("John_Smith", 0, 1, 6, 9),
    ("Maria_Lopez", 2, 2, 3, 7),
    ("Carl_Johnson", 0, 3, 6, 15),
    ("Tony_Ballas", 0, 4, 2, 4),
    ("Cheater_Boy", 0, 0, 0, 1),
]
PASSWORD = "demo123"


def make_rp_db(path, prefix=""):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if os.path.exists(path):
        os.remove(path)
    con = sqlite3.connect(path)
    con.execute("CREATE TABLE accounts (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE COLLATE NOCASE,"
                " password TEXT NOT NULL, salt TEXT NOT NULL, money INTEGER DEFAULT 0, level INTEGER DEFAULT 1,"
                " exp INTEGER DEFAULT 0, skin INTEGER DEFAULT 60, admin INTEGER DEFAULT 0, job INTEGER DEFAULT 0,"
                " reg_date INTEGER DEFAULT 0, faction INTEGER DEFAULT 0, frank INTEGER DEFAULT 0,"
                " wanted INTEGER DEFAULT 0, jail INTEGER DEFAULT 0, bank INTEGER DEFAULT 0, phone INTEGER DEFAULT 0)")
    con.execute("CREATE TABLE bans (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE COLLATE NOCASE,"
                " reason TEXT, admin TEXT, date INTEGER DEFAULT 0)")
    now = int(time.time())
    for i, (name, admin, faction, frank, level) in enumerate(ACCOUNTS):
        salt = secrets.token_hex(8)
        # Как SHA256_Hash в open.mp: SHA-256(пароль + соль), заглавный hex
        digest = hashlib.sha256((PASSWORD + salt).encode()).hexdigest().upper()
        con.execute("INSERT INTO accounts (name, password, salt, level, admin, faction, frank, reg_date)"
                    " VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                    (name, digest, salt, level, admin, faction, frank, now - (30 - i) * 86400))
    con.execute("INSERT INTO bans (name, reason, admin, date) VALUES (?, ?, ?, ?)",
                ("Cheater_Boy", "Чит на деньги", "Pal_Layach", now - 3600))
    con.commit()
    con.close()


DEMO_TOPICS = [
    # раздел (kind, faction) или его название, автор, заголовок, текст, ответы [(автор, текст)], статус
    (("news", 0), 0, "Открытие Bastion Peace",
     "[b]Сервер запущен в тестовом режиме![/b]\nЗаходи, регистрируйся, ищи баги. Обо всех ошибках пиши в «Баг-репорты».",
     [], ""),
    (("news", 0), 0, "Фракции уже в игре",
     "Полиция LSPD, больница и три банды: Grove, Ballas и Aztecas. Заявления подавайте в разделе «Фракции».", [], ""),
    (("rules", 0), 0, "Общие правила сервера",
     "[list][*]DM (убийство без причины) запрещён.[*]Не используйте читы и баги.[*]Уважайте других игроков.[/list]",
     [], ""),
    (("complaint", 0), 1, "Жалоба на Tony_Ballas",
     "[b]1. Ваш ник:[/b] Ivan_Petrov\n[b]2. Ник нарушителя:[/b] Tony_Ballas\n[b]3. Сервер:[/b] Peace\n"
     "[b]5. Суть нарушения:[/b] DM у мэрии\n[b]6. Доказательства:[/b] https://youtu.be/demo",
     [(3, "Принято к рассмотрению, ждите ответа.")], "open"),
    (("application", 1), 1, "Заявление в LSPD — Ivan Petrov",
     "[b]1. Ник:[/b] Ivan_Petrov\n[b]2. Уровень:[/b] 3\n[b]8. Почему хотите вступить:[/b] Хочу защищать город.",
     [(2, "Приходи на собеседование в участок в 20:00.")], "approved"),
    ("Общение", 4, "Ищу банду для RP", "Кто со мной на районы? Пишите в игре /pm.",
     [(5, "Ballas всегда рады новичкам."), (1, "+1, я тоже ищу")], ""),
]


def seed_forum(cfg):
    """Несколько тем, чтобы демо-форум не был пустым."""
    from app.storage import Storage
    store = Storage(os.path.join(cfg.data_dir, "forum.db"))
    key = next(iter(cfg.servers))
    ids = [store.upsert_user(key, i + 1, a[0]) for i, a in enumerate(ACCOUNTS)]
    forums = store.all_forums()
    for where, author, title, body, replies, status in DEMO_TOPICS:
        if isinstance(where, str):
            forum = next(f for f in forums if f["title"] == where)
        else:
            forum = next(f for f in forums if (f["kind"], f["faction"]) == where)
        kind = forum["kind"]
        tid = store.create_topic(forum, ids[author], title, body)
        for who, text in replies:
            store.add_post(tid, ids[who], text)
        if status:
            store.update_topic(tid, status=status, locked=0 if status == "open" else 1)
        if kind == "rules":
            store.update_topic(tid, pinned=1)


def make_demo(cfg):
    demo_dir = os.path.join(cfg.data_dir, "demo")
    for key, srv in cfg.servers.items():
        srv.db = os.path.join(demo_dir, f"rp-{key}.db")
        srv.address = "127.0.0.1"
        make_rp_db(srv.db)
    cfg.data_dir = demo_dir
    forum_db = os.path.join(demo_dir, "forum.db")
    for suffix in ("", "-wal", "-shm"):
        if os.path.exists(forum_db + suffix):
            os.remove(forum_db + suffix)
    seed_forum(cfg)
    return cfg


if __name__ == "__main__":
    import sys
    target = sys.argv[1] if len(sys.argv) > 1 else os.path.join("data", "demo", "rp.db")
    make_rp_db(target)
    print("Готово:", target, "— вход:", ", ".join(a[0] for a in ACCOUNTS), "пароль", PASSWORD)
