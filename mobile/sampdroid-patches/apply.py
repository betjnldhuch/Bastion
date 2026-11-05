#!/usr/bin/env python3
"""Патчи Bastion поверх SampDroid (MIT, (c) 2026 xykal), применяются при сборке в CI.

1. Автоподключение: лаунчер Bastion передаёт ник и сервер через Intent,
   клиент сразу проверяет сервер и открывает экран подключения, минуя свой список серверов.
2. Русский перевод экранов, которые видит игрок (главный экран, сервер, сессия,
   чат, управление, мир). Отладочные экраны остаются как в оригинале.

Запуск: python3 apply.py <путь к mobile/sampdroid>
Исходники submodule в git не меняются: CI правит только рабочую копию перед сборкой.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ru_strings import RU  # noqa: E402

root = pathlib.Path(sys.argv[1])
src = root / "app/src/main/java/com/xykal/sampdroid"
main = src / "ui/MainActivity.kt"

# ---------------------------------------------------------------- 1. автоподключение
AUTOCONNECT_ANCHOR = "        restorePersistedGtaTree()\n        loadServers()\n    }\n"
AUTOCONNECT = """        restorePersistedGtaTree()
        loadServers()
        handleBastionLaunch(intent)
    }

    override fun onNewIntent(intent: android.content.Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleBastionLaunch(intent)
    }

    /** Bastion: лаунчер передаёт ник и сервер, сразу открываем подключение. */
    private fun handleBastionLaunch(launch: android.content.Intent?) {
        if (launch == null) return
        val host = launch.getStringExtra("bastion.host")?.trim().orEmpty()
        val port = launch.getIntExtra("bastion.port", 0)
        val nick = launch.getStringExtra("bastion.nick")?.trim().orEmpty()
        val name = launch.getStringExtra("bastion.name")?.trim().orEmpty()
        if (nick.isNotEmpty()) {
            PlayerProfile.saveNickname(this, nick)
            updateDashboard()
        }
        if (host.isNotEmpty() && port in 1..65535) {
            launch.removeExtra("bastion.host")
            resolveAndOpenServer(host, port, name.ifEmpty { "Bastion Online" })
        }
    }
"""

text = main.read_text(encoding="utf-8")
if "handleBastionLaunch" not in text:
    if text.count(AUTOCONNECT_ANCHOR) != 1:
        sys.exit("apply.py: не нашёл место для автоподключения в MainActivity.kt (обновился SampDroid?)")
    text = text.replace(AUTOCONNECT_ANCHOR, AUTOCONNECT)
text = text.replace('title = "SampDroid"', 'title = "Bastion Online"')
text = text.replace('subtitle = "native SA-MP client · v"', 'subtitle = "мобильный клиент · v"')
main.write_text(text, encoding="utf-8")

# ---------------------------------------------------------------- 2. перевод
files = list(src.rglob("*.kt")) + list((root / "app/src/main/res").rglob("*.xml"))
done = set()
for f in files:
    t = f.read_text(encoding="utf-8")
    orig = t
    for idn, ru in RU.items():
        if f.suffix == ".kt":
            a, b = '"' + idn + '"', '"' + ru + '"'
        else:
            a, b = '"' + idn + '"', '"' + ru + '"'
            if a not in t:
                a, b = ">" + idn + "<", ">" + ru + "<"
        if a in t:
            t = t.replace(a, b)
            done.add(idn)
    if t != orig:
        f.write_text(t, encoding="utf-8")

missing = [k for k in RU if k not in done]
print(f"apply.py: автоподключение OK, переведено строк: {len(done)} из {len(RU)}")
for k in missing:
    print("  не найдено (возможно, изменилось в SampDroid):", k[:80])
