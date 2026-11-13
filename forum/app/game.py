"""Связь с игровым сервером: база rp.db (только чтение) и статус через SA-MP query."""

import hashlib
import hmac
import os
import socket
import sqlite3
import struct
import threading
import time
import urllib.parse

# Должно совпадать с server/gamemodes/bastion/factions.inc
FACTIONS = {
    0: "Нет",
    1: "Полиция LSPD",
    2: "Больница LS (EMS)",
    3: "Grove Street Families",
    4: "The Ballas",
    5: "Varrios Los Aztecas",
    6: "ФБР",
    7: "Армия",
}
FACTION_COLORS = {1: "#3a6eff", 2: "#ff8080", 3: "#10a010", 4: "#b040ff", 5: "#00c8f0", 6: "#6c8cd5", 7: "#6b8e23"}
# Должно совпадать с gBizTypeNames в server/gamemodes/bastion/business.inc
BIZ_TYPES = {1: "Магазин 24/7", 2: "Бар", 3: "Магазин одежды", 4: "Оружейный магазин"}
MAX_RANK = 10
LEADER_RANK = MAX_RANK - 1  # 9 = заместитель, 10 = лидер (как Faction_NeedLeader в моде)
RANK_NAMES = {
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


def rank_name(faction, rank):
    if faction not in FACTIONS or faction == 0 or not 1 <= rank <= MAX_RANK:
        return ""
    table = "police" if faction == 1 else "ems" if faction == 2 else "fbi" if faction == 6 else "army" if faction == 7 else "gang"
    return RANK_NAMES[table][rank - 1]


def _pawn_bytes(text):
    """Игровой клиент присылает пароль в Windows-1251 — хешируем те же байты."""
    try:
        return text.encode("cp1251")
    except UnicodeEncodeError:
        return text.encode("utf-8")


def check_password(password, salt, stored_hash):
    """open.mp SHA256_Hash(password, salt) = SHA-256(пароль + соль), hex.

    Регистр hex не важен. На случай другой сборки сервера проверяем и порядок соль + пароль.
    """
    if not stored_hash:
        return False
    stored = stored_hash.strip().lower()
    p, s = _pawn_bytes(password), _pawn_bytes(salt or "")
    for data in (p + s, s + p):
        if hmac.compare_digest(hashlib.sha256(data).hexdigest(), stored):
            return True
    return False


class GameDB:
    """Только чтение rp.db одного сервера."""

    def __init__(self, server):
        self.server = server

    def available(self):
        return bool(self.server.db) and os.path.exists(self.server.db)

    def _connect(self):
        path = os.path.abspath(self.server.db).replace("\\", "/")
        if not path.startswith("/"):
            path = "/" + path  # C:/... -> /C:/...
        uri = "file:" + urllib.parse.quote(path, safe="/:") + "?mode=ro"
        con = sqlite3.connect(uri, uri=True, timeout=3)
        con.row_factory = sqlite3.Row
        return con

    def _columns(self, con, table):
        return {r[1] for r in con.execute("SELECT * FROM pragma_table_info(?)", (table,))}

    def _account(self, where, arg):
        if not self.available():
            return None
        try:
            con = self._connect()
        except sqlite3.Error:
            return None
        try:
            cols = self._columns(con, "accounts")
            if not cols:
                return None
            want = ["id", "name", "password", "salt", "level", "admin", "reg_date",
                    "faction", "frank", "skin", "job"]
            sel = ", ".join(c if c in cols else "0 AS " + c for c in want)
            row = con.execute(f"SELECT {sel} FROM accounts WHERE {where} LIMIT 1", (arg,)).fetchone()
            return dict(row) if row else None
        except sqlite3.Error:
            return None
        finally:
            con.close()

    def account_by_name(self, name):
        return self._account("name = ? COLLATE NOCASE", name)

    def account_by_id(self, game_id):
        return self._account("id = ?", game_id)

    def ban_of(self, name):
        rows = self._query("SELECT name, reason, admin, date FROM bans WHERE name = ? COLLATE NOCASE", (name,))
        return rows[0] if rows else None

    def bans(self, limit=200):
        return self._query("SELECT name, reason, admin, date FROM bans ORDER BY date DESC LIMIT ?", (limit,))

    def faction_members(self, faction):
        return self._query(
            "SELECT id, name, frank, level FROM accounts WHERE faction = ? ORDER BY frank DESC, level DESC LIMIT 200",
            (faction,))

    def admins(self):
        return self._query("SELECT id, name, admin FROM accounts WHERE admin > 0 ORDER BY admin DESC, name")

    def map_data(self):
        """Дома, бизнесы и территории банд для карты сайта (только уличные входы)."""
        data = {"houses": [], "businesses": [], "turfs": []}
        if not self.available():
            return data
        try:
            con = self._connect()
        except sqlite3.Error:
            return data
        try:
            names = {}

            def owner(oid):
                oid = int(oid or 0)
                if oid <= 0:
                    return ""
                if oid not in names:
                    row = con.execute("SELECT name FROM accounts WHERE id = ?", (oid,)).fetchone()
                    names[oid] = row[0] if row else ""
                return names[oid]

            hcols = self._columns(con, "houses")
            if hcols:
                where = " WHERE oint = 0 AND oworld = 0" if {"oint", "oworld"} <= hcols else ""
                for r in con.execute(f"SELECT id, owner, price, x, y FROM houses{where} ORDER BY id"):
                    data["houses"].append({"id": r["id"], "x": round(r["x"] or 0, 1), "y": round(r["y"] or 0, 1),
                                           "price": int(r["price"] or 0), "owner": owner(r["owner"])})
            if self._columns(con, "businesses"):
                for r in con.execute("SELECT id, name, type, owner, price, x, y FROM businesses ORDER BY id"):
                    t = int(r["type"] or 0)
                    data["businesses"].append({"id": r["id"], "x": round(r["x"] or 0, 1), "y": round(r["y"] or 0, 1),
                                               "name": r["name"] or BIZ_TYPES.get(t, "Бизнес"),
                                               "type": BIZ_TYPES.get(t, "Бизнес"),
                                               "price": int(r["price"] or 0), "owner": owner(r["owner"])})
            if self._columns(con, "turfs"):
                for r in con.execute("SELECT id, name, minx, miny, maxx, maxy, owner FROM turfs ORDER BY id"):
                    f = int(r["owner"] or 0)
                    data["turfs"].append({"id": r["id"], "name": r["name"] or "",
                                          "box": [round(r[k] or 0, 1) for k in ("minx", "miny", "maxx", "maxy")],
                                          "faction": FACTIONS.get(f, "") if f else "",
                                          "color": FACTION_COLORS.get(f, "#808491")})
        except sqlite3.Error:
            pass
        finally:
            con.close()
        return data

    def account_exists(self, name):
        rows = self._query("SELECT id FROM accounts WHERE name = ? COLLATE NOCASE LIMIT 1", (name,))
        return bool(rows)

    def _query(self, sql, args=()):
        if not self.available():
            return []
        try:
            con = self._connect()
        except sqlite3.Error:
            return []
        try:
            return [dict(r) for r in con.execute(sql, args)]
        except sqlite3.Error:
            return []
        finally:
            con.close()


# ------------------------------------------------------------------
#  Статус игрового сервера (SA-MP query, пакет 'i')
# ------------------------------------------------------------------

_status_cache = {}
_status_lock = threading.Lock()
STATUS_TTL = 30


def _read_str(data, pos):
    (n,) = struct.unpack_from("<I", data, pos)
    pos += 4
    return data[pos:pos + n].decode("cp1251", "replace"), pos + n


def query_server(address, port, timeout=1.0):
    try:
        ip = socket.gethostbyname(address)
        packet = b"SAMP" + socket.inet_aton(ip) + struct.pack("<H", port) + b"i"
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(timeout)
            sock.sendto(packet, (ip, port))
            data, _ = sock.recvfrom(2048)
        if len(data) < 11 + 5 or data[:4] != b"SAMP" or data[10:11] != b"i":
            return None
        pos = 11
        password = data[pos] != 0
        players, max_players = struct.unpack_from("<HH", data, pos + 1)
        pos += 5
        hostname, pos = _read_str(data, pos)
        gamemode, pos = _read_str(data, pos)
        return {"online": True, "players": players, "max_players": max_players,
                "hostname": hostname, "gamemode": gamemode, "password": password}
    except (OSError, struct.error, IndexError):
        return None


def server_status(server):
    key = (server.address, server.port)
    now = time.time()
    with _status_lock:
        cached = _status_cache.get(key)
        if cached and now - cached[0] < STATUS_TTL:
            return cached[1]
    status = query_server(server.address, server.port) or {"online": False}
    with _status_lock:
        _status_cache[key] = (now, status)
    return status
