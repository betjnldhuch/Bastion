"""Запуск ботов Bastion Online (ВКонтакте и Telegram). Настройки — bots.ini."""

import os
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from app import config, db, logic, query  # noqa: E402
from app.tg import TgBot  # noqa: E402
from app.vk import VkBot  # noqa: E402


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ini = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bots.ini")
    cfg = config.load(ini)
    dbs = {key: db.GameDB(s) for key, s in cfg.servers.items()}
    for key, gdb in dbs.items():
        state = "найдена" if gdb.available() else "НЕ НАЙДЕНА — проверь db в bots.ini"
        print(f"[db] {gdb.server.name}: {gdb.server.db} — {state}")

    core = logic.Logic(cfg, dbs, query.server_info)
    threads = []
    if cfg.vk_enabled:
        if not cfg.vk_token or not cfg.vk_group_id:
            print("[vk] Впиши token и group_id в bots.ini (раздел [vk])")
        else:
            vk = VkBot(cfg.vk_token, cfg.vk_group_id, core)
            core.senders["vk"] = vk.send
            threads.append(threading.Thread(target=vk.run, daemon=True))
    if cfg.tg_enabled:
        if not cfg.tg_token:
            print("[tg] Впиши token в bots.ini (раздел [tg])")
        else:
            tg = TgBot(cfg.tg_token, core)
            core.senders["tg"] = tg.send
            threads.append(threading.Thread(target=tg.run, daemon=True))
    if not threads:
        print("Ни один бот не включён. Открой bots.ini, впиши токены и поставь enabled = on.")
        return 1

    for t in threads:
        t.start()
    print("Боты работают. Остановить — закрой это окно.")
    try:
        while True:
            core.deliver_outbox()
            time.sleep(cfg.outbox_seconds)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
