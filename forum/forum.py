"""Форум Bastion Online.

Запуск:  python forum.py            (настройки в forum.ini рядом)
         python forum.py --demo     (тестовая база с игроками, чтобы посмотреть форум без сервера)
"""

import argparse
import os
import sys

if sys.version_info < (3, 9):
    sys.exit("Нужен Python 3.9 или новее: https://www.python.org/downloads/")

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from app import config, server  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description="Форум Bastion Online")
    ap.add_argument("--config", default=os.path.join(HERE, "forum.ini"))
    ap.add_argument("--port", type=int)
    ap.add_argument("--demo", action="store_true",
                    help="создать тестовую базу игроков в data/demo и работать с ней")
    args = ap.parse_args()

    cfg = config.load(args.config, HERE)
    if args.port:
        cfg.port = args.port
    if args.demo:
        from tools.make_demo_db import make_demo
        cfg = make_demo(cfg)

    app, httpd = server.serve(cfg)
    print("=" * 60)
    print(f" Форум {cfg.title} запущен")
    print(f" Открой в браузере: http://127.0.0.1:{cfg.port}")
    for s in cfg.servers.values():
        ok = "найдена" if os.path.exists(s.db) else "НЕ НАЙДЕНА — проверь db в forum.ini"
        print(f" {s.name}: база {s.db} — {ok}")
    if args.demo:
        print(" Демо-вход: Pal_Layach / demo123 (админ), Ivan_Petrov / demo123 (игрок)")
    print(" Остановить: закрой это окно или нажми Ctrl+C")
    print("=" * 60)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        httpd.server_close()


if __name__ == "__main__":
    main()
