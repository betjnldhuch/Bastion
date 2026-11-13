"""Бот ВКонтакте: Bots Long Poll API.

Как это работает:
1. groups.getLongPollServer выдаёт адрес «почтового ящика» сообщества (server, key, ts).
2. Бот держит к нему запрос до 25 секунд. Как только кто-то пишет сообществу —
   ВК сразу отвечает списком событий (message_new), иначе пустым списком.
3. Бот отвечает через messages.send и снова ждёт. Белый IP и сайт не нужны.
"""

import json
import random
import threading
import time

from . import http

API = "https://api.vk.com/method/"
VERSION = "5.199"


class VkError(Exception):
    pass


class VkBot:
    def __init__(self, token, group_id, logic):
        self.token = token
        self.group_id = group_id
        self.logic = logic

    def api(self, method, **params):
        params.update(access_token=self.token, v=VERSION)
        data = http.post(API + method, params)
        if "error" in data:
            err = data["error"]
            raise VkError(f"{method}: {err.get('error_code')} {err.get('error_msg')}")
        return data["response"]

    # --- отправка -----------------------------------------------------
    @staticmethod
    def keyboard(reply):
        if not reply.buttons:
            return None
        rows = []
        for row in reply.buttons:
            rows.append([{"action": {"type": "text", "label": label[:40],
                                     "payload": json.dumps({"cmd": cmd}, ensure_ascii=False)},
                          "color": "secondary"} for label, cmd in row])
        kb = {"buttons": rows, "inline": reply.inline}
        if not reply.inline:
            kb["one_time"] = False
        return json.dumps(kb, ensure_ascii=False)

    def send(self, user_id, reply):
        self.api("messages.send", peer_id=user_id, message=reply.text[:4000],
                 random_id=random.randint(1, 2 ** 31 - 1), keyboard=self.keyboard(reply))

    # --- приём --------------------------------------------------------
    def on_message(self, msg):
        peer, user = msg.get("peer_id"), msg.get("from_id")
        if not user or peer != user:  # только личные сообщения, в беседах молчим
            return
        text = msg.get("text", "")
        payload = msg.get("payload")
        if payload:
            try:
                p = json.loads(payload)
                text = p.get("cmd") or ("/start" if p.get("command") == "start" else text)
            except (ValueError, AttributeError):
                pass
        for reply in self.logic.handle("vk", user, text):
            self.send(user, reply)

    def run(self):
        print("[vk] бот ВКонтакте запущен")
        server = None
        while True:
            try:
                if server is None:
                    server = self.api("groups.getLongPollServer", group_id=self.group_id)
                data = http.get(server["server"], {"act": "a_check", "key": server["key"], "ts": server["ts"], "wait": 25})
                if "failed" in data:
                    if data["failed"] == 1:
                        server["ts"] = data["ts"]
                    else:
                        server = None  # ключ устарел — берём новый
                    continue
                server["ts"] = data["ts"]
                for upd in data.get("updates", []):
                    if upd.get("type") == "message_new":
                        msg = upd.get("object", {}).get("message", {})
                        threading.Thread(target=self._safe, args=(msg,), daemon=True).start()
            except VkError as e:
                print(f"[vk] {e}")
                if " 5 " in f" {e} " or "5 User authorization failed" in str(e):
                    print("[vk] Неверный токен. Проверь token в bots.ini")
                server = None
                time.sleep(10)
            except Exception as e:  # сеть пропала и т.п. — ждём и пробуем снова
                print(f"[vk] нет связи: {e}")
                server = None
                time.sleep(5)

    def _safe(self, msg):
        try:
            self.on_message(msg)
        except Exception as e:
            print(f"[vk] ошибка ответа: {e}")
