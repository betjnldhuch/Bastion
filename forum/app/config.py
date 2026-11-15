"""Чтение forum.ini."""

import configparser
import os
from dataclasses import dataclass, field


@dataclass
class GameServer:
    key: str
    name: str
    db: str
    address: str = "127.0.0.1"
    port: int = 7777
    logs: str = ""          # журнал сервера logs.db (по умолчанию рядом с rp.db)


@dataclass
class Config:
    base_dir: str
    title: str = "Bastion Online"
    subtitle: str = ""
    host: str = "0.0.0.0"
    port: int = 8080
    public_url: str = "http://127.0.0.1:8080"
    owners: list = field(default_factory=list)
    moderator_level: int = 1
    admin_level: int = 4
    logs_level: int = 3     # с какого уровня игровой админки видна страница /logs
    discord: str = ""
    map_image: str = ""
    donate_rub_per_coin: float = 1.0
    donate_min_coins: int = 10
    donate_payment: str = ""
    donate_contact: str = ""
    servers: dict = field(default_factory=dict)
    data_dir: str = ""

    def server(self, key):
        return self.servers.get(key)


def _int(value, default):
    try:
        return int(str(value).strip())
    except (TypeError, ValueError):
        return default


def _sibling(path, name):
    """Файл name в той же папке, что path (путь может быть виндовым и на Linux)."""
    cut = max(path.rfind("/"), path.rfind("\\"))
    return path[:cut + 1] + name if cut >= 0 else name


def load(path, base_dir=None):
    base_dir = base_dir or os.path.dirname(os.path.abspath(path))
    parser = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=None)
    if os.path.exists(path):
        with open(path, encoding="utf-8-sig") as fh:
            parser.read_file(fh)

    f = parser["forum"] if parser.has_section("forum") else {}
    cfg = Config(base_dir=base_dir)
    cfg.title = f.get("title", cfg.title).strip() or cfg.title
    cfg.subtitle = f.get("subtitle", "").strip()
    cfg.host = f.get("host", cfg.host).strip() or cfg.host
    cfg.port = _int(f.get("port"), cfg.port)
    cfg.public_url = (f.get("public_url", "") or cfg.public_url).strip().rstrip("/")
    cfg.owners = [n.strip().lower() for n in f.get("owners", "").split(",") if n.strip()]
    cfg.moderator_level = max(1, _int(f.get("moderator_level"), 1))
    cfg.admin_level = max(cfg.moderator_level, _int(f.get("admin_level"), 4))
    cfg.logs_level = max(1, _int(f.get("logs_level"), cfg.logs_level))
    cfg.discord = f.get("discord", "").strip()
    cfg.data_dir = os.path.join(base_dir, "data")

    d = parser["donate"] if parser.has_section("donate") else {}
    try:
        cfg.donate_rub_per_coin = max(0.0, float(str(d.get("rub_per_coin", "1")).replace(",", ".")))
    except ValueError:
        pass
    cfg.donate_min_coins = max(1, _int(d.get("min_coins"), cfg.donate_min_coins))
    cfg.donate_payment = d.get("payment", "").strip()
    cfg.donate_contact = d.get("contact", "").strip()
    cfg.map_image = f.get("map_image", "").strip()

    for section in parser.sections():
        if not section.startswith("server:"):
            continue
        key = section.split(":", 1)[1].strip().lower()
        if not key:
            continue
        s = parser[section]
        db = s.get("db", "").strip()
        if db and not os.path.isabs(db) and not (len(db) > 1 and db[1] == ":"):
            db = os.path.normpath(os.path.join(base_dir, db))
        logs = s.get("logs", "").strip()
        if logs and not os.path.isabs(logs) and not (len(logs) > 1 and logs[1] == ":"):
            logs = os.path.normpath(os.path.join(base_dir, logs))
        if not logs and db:
            logs = _sibling(db, "logs.db")
        cfg.servers[key] = GameServer(
            key=key,
            logs=logs,
            name=s.get("name", key).strip() or key,
            db=db,
            address=s.get("address", "127.0.0.1").strip(),
            port=_int(s.get("port"), 7777),
        )
    return cfg
