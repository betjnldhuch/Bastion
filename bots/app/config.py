"""Чтение bots.ini."""

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


@dataclass
class Config:
    vk_enabled: bool = False
    vk_token: str = ""
    vk_group_id: int = 0
    tg_enabled: bool = False
    tg_token: str = ""
    news_level: int = 5
    forum_url: str = ""
    outbox_seconds: float = 3.0
    servers: dict = field(default_factory=dict)


def _int(value, default):
    try:
        return int(str(value).strip())
    except (TypeError, ValueError):
        return default


def _on(value):
    return str(value or "").strip().lower() in ("on", "1", "yes", "true", "да")


def load(path):
    base_dir = os.path.dirname(os.path.abspath(path))
    parser = configparser.ConfigParser(interpolation=None)
    if os.path.exists(path):
        with open(path, encoding="utf-8-sig") as fh:
            parser.read_file(fh)

    def sec(name):
        return parser[name] if parser.has_section(name) else {}

    vk, tg, bot = sec("vk"), sec("tg"), sec("bot")
    cfg = Config()
    cfg.vk_enabled = _on(vk.get("enabled"))
    cfg.vk_token = vk.get("token", "").strip()
    cfg.vk_group_id = abs(_int(vk.get("group_id"), 0))
    cfg.tg_enabled = _on(tg.get("enabled"))
    cfg.tg_token = tg.get("token", "").strip()
    cfg.news_level = max(1, _int(bot.get("news_level"), 5))
    cfg.forum_url = bot.get("forum_url", "").strip()
    cfg.outbox_seconds = max(1, _int(bot.get("outbox_seconds"), 3))

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
        cfg.servers[key] = GameServer(key=key, name=s.get("name", key).strip() or key, db=db,
                                      address=s.get("address", "127.0.0.1").strip(),
                                      port=_int(s.get("port"), 7777))
    return cfg
