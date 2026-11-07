"""Проверка форума целиком: python -m unittest discover -s tests (из папки forum)."""

import hashlib
import http.cookiejar
import os
import re
import shutil
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)

from app import config, game, markup, server  # noqa: E402
from tools.make_demo_db import make_demo  # noqa: E402


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *a, **kw):
        return None


class Client:
    def __init__(self, base):
        self.base = base
        self.jar = http.cookiejar.CookieJar()
        self.opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(self.jar))

    def get(self, path):
        try:
            with self.opener.open(self.base + path) as r:
                return r.status, r.read().decode()
        except urllib.error.HTTPError as err:
            return err.code, err.read().decode()

    def csrf(self, path):
        _, html = self.get(path)
        m = re.search(r'name="csrf" value="([^"]+)"', html)
        return m.group(1) if m else ""

    def post(self, path, data, csrf_from=None):
        data = dict(data)
        data.setdefault("csrf", self.csrf(csrf_from or "/"))
        body = urllib.parse.urlencode(data).encode()
        try:
            with self.opener.open(self.base + path, body) as r:
                return r.status, r.read().decode(), r.url
        except urllib.error.HTTPError as err:
            return err.code, err.read().decode(), path

    def login(self, name, password="demo123", srv="peace"):
        return self.post("/login", {"name": name, "password": password, "server": srv, "next": "/"}, "/login")


class ForumTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp()
        cfg = config.load(os.path.join(HERE, "forum.ini"), HERE)
        cfg.data_dir = cls.tmp
        cfg.port = 0
        cfg.host = "127.0.0.1"
        cfg = make_demo(cfg)
        for s in cfg.servers.values():
            s.port = 1  # никакого игрового сервера: статус «оффлайн»
        cls.app, cls.httpd = server.serve(cfg)
        cls.base = "http://127.0.0.1:%d" % cls.httpd.server_address[1]
        threading.Thread(target=cls.httpd.serve_forever, daemon=True).start()
        server.POST_COOLDOWN = 0
        server.QUIET = True

    @classmethod
    def tearDownClass(cls):
        cls.httpd.shutdown()
        cls.httpd.server_close()
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def forum_id(self, kind, faction=0):
        return next(f["id"] for f in self.app.store.all_forums() if f["kind"] == kind and f["faction"] == faction)

    # ------------------------------------------------------------

    def test_password_hash_matches_openmp(self):
        salt = "AbCdEfGh12345678"
        stored = hashlib.sha256(("пароль1" + salt).encode("cp1251")).hexdigest().upper()
        self.assertTrue(game.check_password("пароль1", salt, stored))
        self.assertFalse(game.check_password("пароль2", salt, stored))
        self.assertFalse(game.check_password("x", salt, ""))

    def test_markup_is_safe(self):
        out = markup.render('<script>alert(1)</script>[b]ok[/b][url=javascript:alert(1)]x[/url]'
                            '[img]https://e.com/a.png" onerror="x[/img][color=red;background:url(x)]c[/color]')
        self.assertNotIn("<script", out)
        self.assertNotIn("javascript:", out)
        self.assertNotIn('" onerror', out)
        self.assertNotIn("style=", out)
        self.assertIn("<strong>ok</strong>", out)
        self.assertIn('<blockquote class="bb-quote"><div class="bb-quote-head">Ivan пишет:</div>hi</blockquote>',
                      markup.render("[quote=Ivan]hi[/quote]"))
        self.assertIn('href="https://bastion.online/x?a=1&amp;b=2"', markup.render("https://bastion.online/x?a=1&b=2"))

    def test_pages_for_guest(self):
        c = Client(self.base)
        for path in ("/", "/bans", "/team", "/search?q=test", "/login", "/news.txt", "/api/status.json",
                     "/static/style.css", f"/f/{self.forum_id('complaint')}"):
            code, _ = c.get(path)
            self.assertEqual(code, 200, path)
        code, html = c.get("/bans")
        self.assertIn("Cheater Boy", html)
        self.assertEqual(c.get("/static/../app/server.py")[0], 404)
        self.assertEqual(c.get("/nope")[0], 404)

    def test_login(self):
        c = Client(self.base)
        code, html, _ = c.login("Ivan_Petrov", "wrong")
        self.assertIn("Неверный ник или пароль", html)
        code, html, _ = c.login("ivan_petrov")
        self.assertEqual(code, 200)
        self.assertIn("Добро пожаловать, Ivan Petrov", html)
        self.assertIn("Выйти", html)

    def test_csrf_required(self):
        c = Client(self.base)
        c.login("Ivan_Petrov")
        fid = self.forum_id("normal")
        code, html, _ = c.post(f"/f/{fid}/new", {"title": "Тема", "body": "текст", "csrf": "bad"})
        self.assertEqual(code, 403)

    def test_complaint_flow(self):
        player, admin = Client(self.base), Client(self.base)
        player.login("Ivan_Petrov")
        admin.login("Pal_Layach")
        fid = self.forum_id("complaint")

        code, html = player.get(f"/f/{fid}/new")
        self.assertIn("Ник нарушителя", html)  # шаблон формы
        code, html, url = player.post(f"/f/{fid}/new", {"title": "Жалоба на Tony_Ballas",
                                                        "body": "DM у банка, видео: https://youtu.be/x"},
                                      f"/f/{fid}/new")
        self.assertEqual(code, 200)
        tid = int(re.search(r"/t/(\d+)", url).group(1))
        self.assertIn("На рассмотрении", html)

        # Посторонний игрок не может отвечать в чужой жалобе
        other = Client(self.base)
        other.login("Tony_Ballas")
        code, html = other.get(f"/t/{tid}")
        self.assertIn("Отвечать здесь могут только автор и администрация", html)
        code, _, _ = other.post(f"/t/{tid}/reply", {"body": "я не виноват"}, f"/t/{tid}")
        self.assertEqual(code, 403)

        # Игрок не модератор
        code, _, _ = player.post(f"/t/{tid}/mod", {"action": "status:approved"}, f"/t/{tid}")
        self.assertEqual(code, 403)

        # Админ одобряет -> тема закрыта, автор больше не пишет
        code, html, _ = admin.post(f"/t/{tid}/mod", {"action": "status:approved"}, f"/t/{tid}")
        self.assertIn("Статус изменён: Одобрено", html)
        code, _, _ = player.post(f"/t/{tid}/reply", {"body": "спасибо"}, f"/t/{tid}")
        self.assertEqual(code, 403)
        self.assertEqual(self.app.store.topic(tid)["status"], "approved")

    def test_news_only_admin_and_launcher_feed(self):
        player, admin = Client(self.base), Client(self.base)
        player.login("Ivan_Petrov")
        admin.login("Pal_Layach")
        fid = self.forum_id("news")
        self.assertEqual(player.get(f"/f/{fid}/new")[0], 403)
        code, _, _ = admin.post(f"/f/{fid}/new", {"title": "Открытие | War", "body": "[b]Сервер War[/b] открыт!"},
                                f"/f/{fid}/new")
        self.assertEqual(code, 200)
        _, txt = player.get("/news.txt")
        self.assertRegex(txt, r"\d\d\.\d\d\.\d{4} \| Открытие / War \| Сервер War открыт!")

    def test_faction_leader_moderates_own_faction(self):
        leader, gang = Client(self.base), Client(self.base)
        leader.login("John_Smith")      # лидер полиции
        gang.login("Carl_Johnson")      # лидер Grove
        police = self.forum_id("application", 1)
        applicant = Client(self.base)
        applicant.login("Ivan_Petrov")
        _, _, url = applicant.post(f"/f/{police}/new", {"title": "Заявление в LSPD", "body": "Хочу служить"},
                                   f"/f/{police}/new")
        tid = int(re.search(r"/t/(\d+)", url).group(1))
        self.assertEqual(gang.post(f"/t/{tid}/mod", {"action": "status:rejected"}, f"/t/{tid}")[0], 403)
        code, html, _ = leader.post(f"/t/{tid}/mod", {"action": "status:approved"}, f"/t/{tid}")
        self.assertEqual(code, 200)
        self.assertIn("Одобрено", html)

    def test_banned_player_can_only_appeal(self):
        c = Client(self.base)
        code, html, _ = c.login("Cheater_Boy")
        self.assertEqual(code, 200)
        self.assertEqual(c.get(f"/f/{self.forum_id('normal')}/new")[0], 403)
        appeal = self.forum_id("appeal")
        code, html, _ = c.post(f"/f/{appeal}/new", {"title": "Снимите бан", "body": "Я не читер"}, f"/f/{appeal}/new")
        self.assertEqual(code, 200)
        self.assertIn("Заблокирован в игре", html)

    def test_edit_and_delete(self):
        author, admin = Client(self.base), Client(self.base)
        author.login("Ivan_Petrov")
        admin.login("Pal_Layach")
        fid = self.forum_id("normal")
        _, _, url = author.post(f"/f/{fid}/new", {"title": "Привет всем", "body": "первое"}, f"/f/{fid}/new")
        tid = int(re.search(r"/t/(\d+)", url).group(1))
        author.post(f"/t/{tid}/reply", {"body": "второе"}, f"/t/{tid}")
        posts = self.app.store.posts(tid, 0, 10)
        second = posts[1]["id"]
        code, html, _ = author.post(f"/p/{second}/edit", {"body": "второе (исправлено)"}, f"/p/{second}/edit")
        self.assertIn("второе (исправлено)", html)
        stranger = Client(self.base)
        stranger.login("Tony_Ballas")
        self.assertEqual(stranger.get(f"/p/{second}/edit")[0], 403)
        admin.post(f"/p/{second}/delete", {}, f"/t/{tid}")
        self.assertEqual(self.app.store.topic(tid)["posts_count"], 1)
        admin.post(f"/t/{tid}/mod", {"action": "delete"}, f"/t/{tid}")
        self.assertIsNone(self.app.store.topic(tid))

    def test_open_redirect_blocked(self):
        self.assertEqual(server._safe_next("//evil.com"), "/")
        self.assertEqual(server._safe_next("https://evil.com"), "/")
        self.assertEqual(server._safe_next("/t/5"), "/t/5")


if __name__ == "__main__":
    unittest.main()
