"""Статус игрового сервера через SA-MP query (как в браузере серверов)."""

import socket
import struct


def _packet(ip, port, opcode):
    return b"SAMP" + socket.inet_aton(ip) + struct.pack("<H", port) + opcode


def _ask(address, port, opcode, timeout):
    ip = socket.gethostbyname(address)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        sock.sendto(_packet(ip, port, opcode), (ip, port))
        data, _ = sock.recvfrom(4096)
    if len(data) < 11 or data[:4] != b"SAMP" or data[10:11] != opcode:
        raise OSError("bad reply")
    return data


def _str(data, pos, size_fmt):
    (n,) = struct.unpack_from(size_fmt, data, pos)
    pos += struct.calcsize(size_fmt)
    return data[pos:pos + n].decode("cp1251", "replace"), pos + n


def server_info(address, port, timeout=1.5):
    """{'players', 'max_players', 'hostname', 'names'} или None, если сервер не отвечает."""
    try:
        data = _ask(address, port, b"i", timeout)
        players, max_players = struct.unpack_from("<HH", data, 12)
        hostname, _ = _str(data, 16, "<I")
        info = {"players": players, "max_players": max_players, "hostname": hostname, "names": []}
    except (OSError, struct.error, IndexError):
        return None
    if 0 < players <= 100:   # список ников SA-MP отдаёт, пока игроков не больше 100
        try:
            data = _ask(address, port, b"c", timeout)
            (count,) = struct.unpack_from("<H", data, 11)
            pos = 13
            for _ in range(count):
                name, pos = _str(data, pos, "<B")
                pos += 4  # счёт (уровень)
                info["names"].append(name)
        except (OSError, struct.error, IndexError):
            pass
    return info
