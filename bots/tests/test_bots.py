"""Проверки логики без интернета: python -m unittest discover -s tests"""

import os
import sqlite3
import sys
import tempfile
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from app import config, db, logic  # noqa: E402


def make_db(path):
    con = sqlite3.connect(path)
    con.execute("CREATE TABLE accounts (id INTEGER PRIMARY KEY, name TEXT UNIQUE COLLATE NOCASE, password TEXT,"
                " salt TEXT, money INTEGER, level INTEGER, exp INTEGER, admin INTEGER DEFAULT 0, job INTEGER,"
                " reg_date INTEGER, faction INTEGER, frank INTEGER, wanted INTEGER, bank INTEGER, phone INTEGER)")
    con.execute("CREATE TABLE bans (id INTEGER PRIMARY KEY, name TEXT, reason TEXT, admin TEXT, date INTEGER, until INTEGER)")
    con.execute("INSERT INTO accounts VALUES (1, 'Ivan_Petrov', 'h', 's', 5000, 7, 3, 0, 3, 1700000000, 1, 10, 0, 100000, 5551234)")
    con.execute("INSERT INTO accounts VALUES (2, 'Pal_Layach', 'h', 's', 1, 20, 0, 7, 0, 1700000000, 0, 0, 0, 0, 0)")
    con.commit()
    con.close()


class BotTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        path = os.path.join(self.tmp, "rp.db")
        make_db(path)
        server = config.GameServer(key="peace", name="Peace", db=path)
        self.db = db.GameDB(server)
        cfg = config.Config(news_level=5, servers={"peace": server})
        self.sent = []
        self.bot = logic.Logic(cfg, {"peace": self.db}, lambda a, p: {"players": 2, "max_players": 50,
                                                                      "hostname": "x", "names": ["B", "a"]})
        self.bot.senders["tg"] = lambda uid, r: self.sent.append((uid, r.text))
        self.path = path

    def sql(self, q, args=()):
        con = sqlite3.connect(self.path)
        try:
            rows = con.execute(q, args).fetchall()
            con.commit()
            return rows
        finally:
            con.close()

    def give_code(self, account, code="123456", until=None):
        self.db.available() and self.db._connect().close()  # таблицы
        self.sql("INSERT INTO social_codes VALUES (?, ?, ?)", (code, account, until or int(time.time()) + 600))

    def text(self, user, msg):
        return "\n".join(r.text for r in self.bot.handle("tg", user, msg))

    def test_link_and_me(self):
        self.give_code(1)
        self.assertIn("Ivan_Petrov привязан", self.text(10, "123456"))
        me = self.text(10, "/me")
        self.assertIn("Полиция LSPD (Шеф полиции)", me)
        self.assertIn("$100 000", me)
        self.assertIn("Код не подошёл", self.text(11, "/link 123456"))  # код одноразовый

    def test_expired_code_and_bruteforce(self):
        self.give_code(1, until=int(time.time()) - 1)
        self.assertIn("Код не подошёл", self.text(10, "/link 123456"))
        for _ in range(5):
            self.text(10, "/link 000000")
        self.assertIn("Слишком много", self.text(10, "/link 000000"))

    def test_restore_and_notme(self):
        self.give_code(1)
        self.text(10, "/link 123456")
        self.assertIn("Новый пароль для Ivan_Petrov", self.text(10, "/restore_yes"))
        self.text(10, "/notme")
        actions = self.sql("SELECT account, action, length(data) FROM bot_actions ORDER BY id")
        self.assertEqual(actions, [(1, "setpass", 10), (1, "kick", 0)])

    def test_outbox_cp1251(self):
        self.give_code(1)
        self.text(10, "/link 123456")
        self.sql("INSERT INTO bot_outbox (account, text) VALUES (1, CAST(? AS TEXT))", ("Вход в аккаунт. «Это не я»".encode("cp1251"),))
        self.sql("INSERT INTO bot_outbox (account, text) VALUES (2, 'никому')")
        self.bot.deliver_outbox()
        self.assertEqual(self.sent, [(10, "Вход в аккаунт. «Это не я»")])
        self.assertEqual(self.sql("SELECT COUNT(*) FROM bot_outbox"), [(0,)])

    def test_notify_toggle_and_news(self):
        self.give_code(2)
        self.text(10, "/link 123456")
        self.assertIn("выключены", self.text(10, "/notify"))
        self.assertIn("Отправлено: 1", self.text(10, "/news Обновление!"))
        self.give_code(1, "654321")
        self.text(11, "654321")
        self.assertIn("Рассылка доступна", self.text(11, "/news x"))

    def test_online_top_unlink(self):
        self.assertIn("2/50", self.text(1, "онлайн"))
        self.assertIn("a, B", self.text(1, "/online"))
        self.assertIn("1. Ivan_Petrov — 7 ур.", self.text(1, "/top"))
        self.give_code(1)
        self.text(10, "/link 123456")
        self.assertIn("отвязан", self.text(10, "/unlink ivan_petrov"))
        self.assertIn("не привязан", self.text(10, "/me"))


if __name__ == "__main__":
    unittest.main()
