"""Команды бота. Одна логика на ВК и Telegram: на вход текст, на выход ответы Reply."""

import re
import secrets
import sqlite3
import threading
import time
from dataclasses import dataclass, field

# Должно совпадать с модом: factions.inc, jobs.inc, config.inc
FACTIONS = {0: "нет", 1: "Полиция LSPD", 2: "Больница LS (EMS)", 3: "Grove Street Families", 4: "The Ballas",
            5: "Varrios Los Aztecas", 6: "ФБР", 7: "Армия"}
RANKS = {
    "police": ["Кадет", "Офицер", "Сержант", "Лейтенант", "Ст. лейтенант", "Капитан", "Майор", "Подполковник",
               "Зам. шефа", "Шеф полиции"],
    "ems": ["Интерн", "Фельдшер", "Врач", "Хирург", "Ст. хирург", "Зав. отделением", "Нач. смены", "Консультант",
            "Зам. главврача", "Главврач"],
    "gang": ["Новичок", "Боец", "Бывалый", "Старший", "Смотрящий", "Авторитет", "Советник", "Капитан",
             "Правая рука", "Лидер"],
    "fbi": ["Стажёр", "Мл. агент", "Агент", "Ст. агент", "Спец. агент", "Ст. спец. агент", "Нач. отдела",
            "Инспектор", "Зам. директора", "Директор ФБР"],
    "army": ["Рядовой", "Ефрейтор", "Сержант", "Ст. сержант", "Прапорщик", "Лейтенант", "Капитан", "Майор",
             "Полковник", "Генерал"],
}
JOBS = {0: "безработный", 1: "Грузчик", 2: "Развозчик пиццы", 3: "Таксист", 4: "Водитель автобуса",
        5: "Дальнобойщик", 6: "Механик"}
EXP_PER_LEVEL = 4
MIN_PASSWORD_LEN = 6

LINK_TRIES = 5          # неверных кодов подряд
LINK_TRIES_WINDOW = 600  # ... за столько секунд, потом пауза
PASSWORD_ALPHABET = "abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789"


def rank_name(faction, rank):
    if faction == 0 or faction not in FACTIONS or not 1 <= rank <= 10:
        return ""
    table = {1: "police", 2: "ems", 6: "fbi", 7: "army"}.get(faction, "gang")
    return RANKS[table][rank - 1]


def money(n):
    return "$" + f"{int(n or 0):,}".replace(",", " ")


@dataclass
class Reply:
    text: str
    buttons: list = field(default_factory=list)   # [[(надпись, команда), ...], ...]
    inline: bool = False                          # кнопки под сообщением (True) или меню внизу (False)


MENU = [[("👤 Мой аккаунт", "/me"), ("🟢 Онлайн", "/online")],
        [("🏆 Топ игроков", "/top"), ("❓ Помощь", "/help")]]

# Русские слова и надписи кнопок -> команды
ALIASES = {
    "начать": "/start", "start": "/start", "меню": "/help", "помощь": "/help", "help": "/help",
    "мой аккаунт": "/me", "аккаунт": "/me", "статистика": "/me", "стата": "/me", "stats": "/me",
    "онлайн": "/online", "online": "/online", "топ": "/top", "топ игроков": "/top",
    "это не я": "/notme", "привязать": "/link", "отвязать": "/unlink", "восстановить": "/restore",
    "уведомления": "/notify",
}


class Logic:
    def __init__(self, cfg, dbs, query_fn):
        self.cfg = cfg
        self.dbs = dbs                # {ключ сервера: GameDB}
        self.query = query_fn         # (address, port) -> info | None
        self.senders = {}             # {"vk"/"tg": функция(user_id, Reply)} — для рассылки /news
        self._fails = {}
        self._lock = threading.Lock()

    # ------------------------------------------------------------------
    def server_label(self, key):
        return f"[{self.dbs[key].server.name}] " if len(self.dbs) > 1 else ""

    def handle(self, platform, user_id, text):
        """Разобрать сообщение пользователя. Возвращает список Reply."""
        text = (text or "").strip()
        if not text:
            return []
        low = text.lower().lstrip("❓👤🟢🏆 ").strip()
        if low in ALIASES:
            text = ALIASES[low]
        elif re.fullmatch(r"\d{6}", text):
            text = "/link " + text
        if not text.startswith("/"):
            return [Reply("Не понял. Вот что я умею:\n\n" + self.help_text(), MENU)]

        parts = text.split(maxsplit=1)
        cmd = parts[0].lower().split("@", 1)[0]      # /cmd@BotName в Telegram
        arg = parts[1].strip() if len(parts) > 1 else ""
        handler = {
            "/start": self.cmd_help, "/help": self.cmd_help, "/link": self.cmd_link, "/unlink": self.cmd_unlink,
            "/me": self.cmd_me, "/stats": self.cmd_me, "/online": self.cmd_online, "/top": self.cmd_top,
            "/notify": self.cmd_notify, "/notme": self.cmd_notme, "/restore": self.cmd_restore,
            "/restore_yes": self.cmd_restore_yes, "/news": self.cmd_news,
        }.get(cmd)
        if not handler:
            return [Reply("Нет такой команды.\n\n" + self.help_text(), MENU)]
        try:
            result = handler(platform, user_id, arg)
        except sqlite3.Error as e:
            print(f"[bot] ошибка базы: {e}")
            return [Reply("База сервера сейчас занята или недоступна. Попробуй через минуту.")]
        return result if isinstance(result, list) else [result]

    def help_text(self):
        lines = [
            "Привязать аккаунт: зайди в игру, введи /bot и пришли сюда код: /link 123456",
            "",
            "/me — мой персонаж: уровень, деньги, фракция",
            "/online — кто сейчас на сервере",
            "/top — лучшие игроки по уровню",
            "/notify — включить или выключить уведомления о входе",
            "/notme — «это не я»: выкинуть мой аккаунт с сервера",
            "/restore — забыл пароль: пришлю новый",
            "/unlink Ник — отвязать аккаунт",
        ]
        if self.cfg.forum_url:
            lines += ["", "Форум: " + self.cfg.forum_url]
        return "\n".join(lines)

    def cmd_help(self, platform, user_id, arg):
        return Reply("Бот Bastion Online.\n\n" + self.help_text(), MENU)

    # --- привязка -----------------------------------------------------
    def _too_many_fails(self, key):
        now = time.time()
        with self._lock:
            fails = [t for t in self._fails.get(key, []) if now - t < LINK_TRIES_WINDOW]
            self._fails[key] = fails
            return len(fails) >= LINK_TRIES

    def _add_fail(self, key):
        with self._lock:
            self._fails.setdefault(key, []).append(time.time())

    def cmd_link(self, platform, user_id, arg):
        code = arg.strip()
        if not re.fullmatch(r"\d{6}", code):
            return Reply("Зайди в игру, введи /bot и пришли сюда код из окна. Например: /link 123456")
        key = (platform, user_id)
        if self._too_many_fails(key):
            return Reply("Слишком много неверных кодов. Попробуй через 10 минут.")
        for skey, db in self.dbs.items():
            if not db.available():
                continue
            account_id = db.take_code(code)
            if account_id:
                db.link(platform, user_id, account_id)
                acc = db.account(account_id) or {"name": "?"}
                return Reply(f"Готово! {self.server_label(skey)}Аккаунт {acc['name']} привязан.\n"
                             "Теперь я буду писать, когда кто-то входит в аккаунт. Отключить: /notify", MENU)
        self._add_fail(key)
        return Reply("Код не подошёл или устарел (он живёт 10 минут). Введи /bot в игре ещё раз.")

    def _links(self, platform, user_id):
        """[(ключ сервера, аккаунт), ...] — все привязанные к этому пользователю."""
        result = []
        for skey, db in self.dbs.items():
            if db.available():
                result += [(skey, acc) for acc in db.links_of(platform, user_id)]
        return result

    def _pick(self, platform, user_id, arg, command):
        """Выбрать один аккаунт по нику. Возвращает (ключ, аккаунт) или Reply с вопросом."""
        links = self._links(platform, user_id)
        if not links:
            return Reply("К тебе не привязан ни один аккаунт. Зайди в игру, введи /bot и пришли сюда код.")
        if arg:
            found = [l for l in links if l[1]["name"].lower() == arg.lower()]
            if not found:
                return Reply(f"Аккаунт {arg} к тебе не привязан.")
            links = found
        if len(links) == 1:
            return links[0]
        buttons = [[(f"{self.server_label(k)}{a['name']}", f"{command} {a['name']}")] for k, a in links]
        return Reply("Какой аккаунт?", buttons, inline=True)

    def cmd_unlink(self, platform, user_id, arg):
        picked = self._pick(platform, user_id, arg, "/unlink")
        if isinstance(picked, Reply):
            return picked
        skey, acc = picked
        self.dbs[skey].unlink(platform, user_id, acc["id"])
        return Reply(f"Аккаунт {acc['name']} отвязан. Уведомлений о нём больше не будет.")

    # --- информация ---------------------------------------------------
    def cmd_me(self, platform, user_id, arg):
        links = self._links(platform, user_id)
        if not links:
            return Reply("К тебе не привязан ни один аккаунт. Зайди в игру, введи /bot и пришли сюда код.")
        out = []
        for skey, a in links:
            faction = FACTIONS.get(a["faction"], "нет")
            rank = rank_name(a["faction"], a["frank"])
            lines = [
                f"{self.server_label(skey)}👤 {a['name']}",
                f"Уровень: {a['level']} (опыт {a['exp']}/{a['level'] * EXP_PER_LEVEL})",
                f"Деньги: {money(a['money'])}, в банке {money(a['bank'])}",
                f"Работа: {JOBS.get(a['job'], 'безработный')}",
                f"Фракция: {faction}" + (f" ({rank})" if rank else ""),
            ]
            if a["phone"]:
                lines.append(f"Телефон: {a['phone']}")
            if a["wanted"]:
                lines.append(f"Розыск: {a['wanted']} зв.")
            if a["reg_date"]:
                lines.append("В игре с " + time.strftime("%d.%m.%Y", time.localtime(a["reg_date"])))
            ban = self.dbs[skey].ban_of(a["name"])
            if ban:
                term = "навсегда" if not ban.get("until") else "до " + time.strftime("%d.%m.%Y %H:%M", time.localtime(ban["until"]))
                lines.append(f"⛔ Бан {term}: {ban['reason']} (админ {ban['admin']})")
            lines.append("Уведомления: " + ("включены" if a["notify"] else "выключены"))
            out.append("\n".join(lines))
        return Reply("\n\n".join(out), MENU)

    def cmd_online(self, platform, user_id, arg):
        out = []
        for skey, db in self.dbs.items():
            s = db.server
            info = self.query(s.address, s.port)
            if not info:
                out.append(f"🔴 {s.name}: сервер выключен")
                continue
            text = f"🟢 {s.name}: {info['players']}/{info['max_players']} игроков"
            if info["names"]:
                text += "\n" + ", ".join(sorted(info["names"], key=str.lower))
            out.append(text)
        return Reply("\n\n".join(out) or "Сервера не настроены.", MENU)

    def cmd_top(self, platform, user_id, arg):
        out = []
        for skey, db in self.dbs.items():
            if not db.available():
                continue
            rows = db.top(10)
            lines = [f"🏆 {db.server.name}"] + [f"{i}. {r['name']} — {r['level']} ур." for i, r in enumerate(rows, 1)]
            out.append("\n".join(lines))
        return Reply("\n\n".join(out) or "База сервера недоступна.", MENU)

    # --- защита аккаунта ---------------------------------------------
    def cmd_notify(self, platform, user_id, arg):
        links = self._links(platform, user_id)
        if not links:
            return Reply("К тебе не привязан ни один аккаунт.")
        on = not any(a["notify"] for _, a in links)
        if arg.lower() in ("on", "вкл", "да"):
            on = True
        elif arg.lower() in ("off", "выкл", "нет"):
            on = False
        for skey in {k for k, _ in links}:
            self.dbs[skey].set_notify(platform, user_id, on)
        return Reply("Уведомления " + ("включены." if on else "выключены. Включить снова: /notify"))

    def cmd_notme(self, platform, user_id, arg):
        links = self._links(platform, user_id)
        if not links:
            return Reply("К тебе не привязан ни один аккаунт.")
        for skey, a in links:
            if not arg or a["name"].lower() == arg.lower():
                self.dbs[skey].add_action(a["id"], "kick")
        return Reply("Выкидываю аккаунт с сервера (если он сейчас в игре).\n"
                     "Обязательно смени пароль: /restore", [[("🔑 Сменить пароль", "/restore")]], inline=True)

    def cmd_restore(self, platform, user_id, arg):
        picked = self._pick(platform, user_id, arg, "/restore")
        if isinstance(picked, Reply):
            return picked
        skey, acc = picked
        return Reply(f"Сделать новый пароль для {acc['name']}? Старый перестанет работать, "
                     "а если аккаунт сейчас в игре — его выкинет.",
                     [[("✅ Да, новый пароль", f"/restore_yes {acc['name']}")]], inline=True)

    def cmd_restore_yes(self, platform, user_id, arg):
        picked = self._pick(platform, user_id, arg, "/restore_yes")
        if isinstance(picked, Reply):
            return picked
        skey, acc = picked
        password = "".join(secrets.choice(PASSWORD_ALPHABET) for _ in range(10))
        self.dbs[skey].add_action(acc["id"], "setpass", password)
        return Reply(f"Новый пароль для {acc['name']}: {password}\n\n"
                     "Он заработает через пару секунд (если сервер выключен — сразу после запуска). "
                     "Удали это сообщение и смени пароль в игре, если захочешь свой.")

    # --- администрация -----------------------------------------------
    def cmd_news(self, platform, user_id, arg):
        links = self._links(platform, user_id)
        if not any(a["admin"] >= self.cfg.news_level for _, a in links):
            return Reply("Рассылка доступна администрации с " + str(self.cfg.news_level) + " уровня.")
        if not arg:
            return Reply("Использование: /news текст рассылки")
        sent = 0
        msg = Reply("📢 " + arg)
        for plat, send in self.senders.items():
            users = set()
            for db in self.dbs.values():
                if db.available():
                    users.update(db.users(plat))
            for uid in users:
                try:
                    send(uid, msg)
                    sent += 1
                except Exception as e:  # один заблокировавший бота не должен ломать рассылку
                    print(f"[news] {plat} {uid}: {e}")
        return Reply(f"Отправлено: {sent}.")

    # --- уведомления из игры ------------------------------------------
    def deliver_outbox(self):
        """Забрать уведомления из всех баз и разослать. Вызывается раз в несколько секунд."""
        for skey, db in self.dbs.items():
            if not db.available():
                continue
            try:
                items = db.take_outbox()
            except sqlite3.Error as e:
                print(f"[outbox] {skey}: {e}")
                continue
            for text, targets in items:
                buttons = [[("🚫 Это не я", "/notme"), ("🔑 Новый пароль", "/restore")]] if "Это не я" in text else []
                msg = Reply(self.server_label(skey) + text, buttons, inline=True)
                for plat, uid in targets:
                    send = self.senders.get(plat)
                    if not send:
                        continue
                    try:
                        send(uid, msg)
                    except Exception as e:
                        print(f"[outbox] {plat} {uid}: {e}")
