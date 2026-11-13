"""Бот Telegram: getUpdates (long polling), как и у ВК — белый IP не нужен."""

import json
import threading
import time

from . import http

COMMANDS = [
    ("me", "Мой персонаж"), ("online", "Кто на сервере"), ("top", "Топ игроков"),
    ("link", "Привязать аккаунт: /link код"), ("notify", "Уведомления вкл/выкл"),
    ("notme", "Это не я: выкинуть аккаунт с сервера"), ("restore", "Забыл пароль"), ("help", "Помощь"),
]


class TgError(Exception):
    pass


class TgBot:
    def __init__(self, token, logic):
        self.url = f"https://api.telegram.org/bot{token}/"
        self.logic = logic

    def api(self, method, http_timeout=35, **params):
        data = http.post(self.url + method, params, timeout=http_timeout)
        if not data.get("ok"):
            raise TgError(f"{method}: {data.get('error_code')} {data.get('description')}")
        return data["result"]

    @staticmethod
    def markup(reply):
        if not reply.buttons:
            return None
        if reply.inline:
            return json.dumps({"inline_keyboard": [[{"text": label, "callback_data": cmd[:64]} for label, cmd in row]
                                                   for row in reply.buttons]}, ensure_ascii=False)
        return json.dumps({"keyboard": [[{"text": label} for label, _ in row] for row in reply.buttons],
                           "resize_keyboard": True, "is_persistent": True}, ensure_ascii=False)

    def send(self, chat_id, reply):
        self.api("sendMessage", chat_id=chat_id, text=reply.text[:4000], reply_markup=self.markup(reply),
                 disable_web_page_preview="true")

    def _answer(self, chat_id, user_id, text):
        for reply in self.logic.handle("tg", user_id, text):
            self.send(chat_id, reply)

    def on_update(self, upd):
        if "callback_query" in upd:
            cq = upd["callback_query"]
            try:
                self.api("answerCallbackQuery", callback_query_id=cq["id"])
            except Exception:
                pass
            msg = cq.get("message") or {}
            if msg.get("chat", {}).get("type") == "private":
                self._answer(msg["chat"]["id"], cq["from"]["id"], cq.get("data", ""))
            return
        msg = upd.get("message")
        if not msg or msg.get("chat", {}).get("type") != "private":  # только личка
            return
        self._answer(msg["chat"]["id"], msg["from"]["id"], msg.get("text", ""))

    def run(self):
        print("[tg] бот Telegram запущен")
        try:
            self.api("setMyCommands", commands=json.dumps([{"command": c, "description": d} for c, d in COMMANDS],
                                                          ensure_ascii=False))
        except Exception as e:
            print(f"[tg] не удалось задать меню команд: {e}")
        offset = None
        while True:
            try:
                updates = self.api("getUpdates", http_timeout=35, offset=offset, timeout=25)
                for upd in updates:
                    offset = upd["update_id"] + 1
                    threading.Thread(target=self._safe, args=(upd,), daemon=True).start()
            except TgError as e:
                print(f"[tg] {e}")
                if "401" in str(e):
                    print("[tg] Неверный токен. Проверь token в bots.ini")
                time.sleep(10)
            except Exception as e:
                print(f"[tg] нет связи: {e}")
                time.sleep(5)

    def _safe(self, upd):
        try:
            self.on_update(upd)
        except Exception as e:
            print(f"[tg] ошибка ответа: {e}")
