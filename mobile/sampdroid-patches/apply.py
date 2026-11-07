#!/usr/bin/env python3
"""Патчи Bastion поверх SampDroid (MIT, (c) 2026 xykal), применяются при сборке в CI.

1. Автоподключение: лаунчер Bastion передаёт ник и сервер через Intent,
   клиент сразу проверяет сервер и открывает экран подключения, минуя свой список серверов.
2. Русский перевод экранов, которые видит игрок (главный экран, сервер, сессия,
   чат, управление, мир). Отладочные экраны остаются как в оригинале.
4. handling.cfg: секции лодок/мотоциклов/самолётов и нестандартные записи
   пропускаются, а не отвергают всю папку игры.
0. Нативный клиент: формат RPC open.mp, спавн после входа, кодировка Windows-1251
   (native-openmp-rpc-spawn-cp1251.patch).
3. Папка игры от лаунчера: ZIP-установщик Bastion распаковывает файлы в
   Android/data/online.bastion.launcher/files/game, лаунчер передаёт этот путь
   в "bastion.game_dir", и клиент подключает его сам, без ручного выбора папки (SAF).

Запуск: python3 apply.py <путь к mobile/sampdroid>
Исходники submodule в git не меняются: CI правит только рабочую копию перед сборкой.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ru_strings import RU  # noqa: E402

root = pathlib.Path(sys.argv[1])
src = root / "app/src/main/java/com/xykal/sampdroid"

# ---------------------------------------------------------------- 0. нативный клиент
# native-openmp-rpc-spawn-cp1251.patch (git diff против закреплённого f9daca1):
#  * RPC в формате RakPeer::RPC open.mp: ID_RPC, id, WriteCompressed(число бит), данные.
#    Без поля длины open.mp отбрасывал ClientJoin, игрок не создавался и клиент
#    вечно ждал PlayerInit («Menunggu PlayerInit»).
#  * Спавн по TogglePlayerSpectating(false) и SpawnPlayer() (RPC 129 без данных):
#    клиент сам отправляет PlayerSpawn, как ПК-клиент (так спавнит мод Bastion).
#  * Тексты сервера в Windows-1251 -> UTF-8 (русские окна входа и чат), ввод
#    игрока -> Windows-1251.
import subprocess  # noqa: E402
NATIVE_PATCH = pathlib.Path(__file__).resolve().parent / "native-openmp-rpc-spawn-cp1251.patch"
if "encodeLegacyRpcWire" not in (root / "app/src/main/cpp/raknet_codec.h").read_text(encoding="utf-8"):
    r = subprocess.run(["git", "apply", "--whitespace=nowarn", str(NATIVE_PATCH)], cwd=root)
    if r.returncode != 0:
        sys.exit("apply.py: не удалось наложить " + NATIVE_PATCH.name + " (обновился SampDroid?)")
main = src / "ui/MainActivity.kt"

# ---------------------------------------------------------------- 1. автоподключение
AUTOCONNECT_ANCHOR = "        restorePersistedGtaTree()\n        loadServers()\n    }\n"
AUTOCONNECT = """        adoptBastionGameData(intent)
        restorePersistedGtaTree()
        loadServers()
        handleBastionLaunch(intent)
    }

    override fun onNewIntent(intent: android.content.Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        if (adoptBastionGameData(intent)) restorePersistedGtaTree()
        handleBastionLaunch(intent)
    }

    /**
     * Bastion: папка с игрой, которую скачал и распаковал установщик лаунчера.
     * Лежит в собственной папке приложения, поэтому SAF-разрешение не нужно:
     * запоминаем её как file:// дерево, дальше работает штатный restorePersistedGtaTree().
     * Возвращает true, если папка сменилась и её нужно подключить заново.
     */
    private fun adoptBastionGameData(launch: android.content.Intent?): Boolean {
        val path = launch?.getStringExtra("bastion.game_dir")?.trim().orEmpty()
        if (path.isEmpty()) return false
        val dir = java.io.File(path)
        if (!dir.isDirectory) return false
        val uri = android.net.Uri.fromFile(dir).toString()
        val preferences = getSharedPreferences(GAME_DATA_PREFS, MODE_PRIVATE)
        if (preferences.getString(KEY_GTA_DATA_TREE_URI, null) == uri) return false
        preferences.edit()
            .putString(KEY_GTA_DATA_TREE_URI, uri)
            .remove(KEY_GTA_DATA_TREE_RELATIVE_PATH)
            .remove(KEY_GTA_IMG_URI)
            // Сразу собрать world & collision при подключении папки.
            .putBoolean(KEY_GTA_MAP_SOURCES_SYNCED, true)
            .remove(KEY_GTA_ARCHIVE_SHA256)
            .remove(KEY_GTA_ARCHIVE_HASH_VERIFIED)
            .remove(KEY_GTA_ARCHIVE_LITE_AUDIO_OMITTED)
            .remove(KEY_GTA_ARCHIVE_OMITTED_AUDIO_FILES)
            .remove(KEY_GTA_ARCHIVE_OMITTED_AUDIO_BYTES)
            .apply()
        ++gameDataSelectionEpoch
        persistedTreeMountReady = null
        persistedTreeMountProblem = null
        return true
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

# ---------------------------------------------------------------- 3. file:// дерево
# GtaDataTreeResolver умеет только SAF-деревья. Для папки, которую лаунчер
# распаковал в files/game, открываем её как обычный каталог (file://).
resolver = src / "ui/GtaDataTreeResolver.kt"
rt = resolver.read_text(encoding="utf-8")
TREE_OLD = """            DocumentFile.fromTreeUri(context, uri)
                ?.takeIf { it.isDirectory }"""
TREE_NEW = """            (if (uri.scheme == "file") uri.path?.let { DocumentFile.fromFile(java.io.File(it)) }
            else DocumentFile.fromTreeUri(context, uri))
                ?.takeIf { it.isDirectory }"""
if "uri.scheme == \"file\"" not in rt:
    if rt.count(TREE_OLD) != 1:
        sys.exit("apply.py: не нашёл fromTreeUri в GtaDataTreeResolver.kt (обновился SampDroid?)")
    resolver.write_text(rt.replace(TREE_OLD, TREE_NEW), encoding="utf-8")

# ---------------------------------------------------------------- 4. handling.cfg
# Стандартный handling.cfg GTA SA после машин содержит секции лодок (%), мотоциклов (!),
# самолётов ($) и анимаций (^). Исходный парсер принимает их за ошибку и отвергает
# всю папку игры («nilai handling.cfg tidak valid pada baris N»). Пропускаем эти
# секции и отдельные нестандартные записи: для них остаётся общий профиль машины.
hcpp = root / "app/src/main/cpp/gta_vehicle_handling.cpp"
ht = hcpp.read_text(encoding="utf-8")
H_REPL = [
    (
        "    if (line.front() == ';' || line.front() == '#') continue;\n",
        "    if (line.front() == ';' || line.front() == '#' || line.front() == '%' ||\n"
        "        line.front() == '!' || line.front() == '$' || line.front() == '^') continue;  // Bastion\n",
    ),
    (
        '      error = "record handling.cfg tidak lengkap pada baris " + std::to_string(lineNumber);\n'
        "      return false;\n",
        "      continue;  // Bastion: неполная запись, остаётся общий профиль\n",
    ),
    (
        '      error = "nilai handling.cfg tidak valid pada baris " + std::to_string(lineNumber);\n'
        "      return false;\n",
        "      continue;  // Bastion: нестандартные значения, остаётся общий профиль\n",
    ),
    (
        "    if (candidate.size() >= maxProfiles || !candidate.emplace(profile.id, std::move(profile)).second) {\n",
        "    if (candidate.size() < maxProfiles && candidate.count(profile.id) != 0u) continue;  // Bastion: дубликат\n"
        "    if (candidate.size() >= maxProfiles || !candidate.emplace(profile.id, std::move(profile)).second) {\n",
    ),
]
if "// Bastion" not in ht:
    for old, new in H_REPL:
        if ht.count(old) != 1:
            sys.exit("apply.py: не нашёл место для патча handling.cfg (обновился SampDroid?): " + old.strip()[:60])
        ht = ht.replace(old, new)
    hcpp.write_text(ht, encoding="utf-8")

# ---------------------------------------------------------------- 5. окна сервера
# Мод Bastion пишет в окнах и чате цветовые коды SA-MP вида {33CCFF}. Раньше они
# показывались как текст. Теперь текст окна раскрашивается (слишком светлые
# цвета остаются цветом темы, иначе белый текст пропадает на светлом окне), а
# из заголовка, кнопок, пунктов списка и чата коды просто убираются.
# Касание мимо окна больше не закрывает его: закрытие = «Отмена», и Bastion за
# отмену окна входа кикает игрока. Кнопка «Назад» по-прежнему работает как «Отмена».
ui_dir = src / "ui"
(ui_dir / "BastionSampText.kt").write_text("""package com.xykal.sampdroid.ui

import android.graphics.Color
import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.style.ForegroundColorSpan

private val bastionSampColorTag = Regex("[{]([0-9A-Fa-f]{6})[}]")

/** Bastion: убирает цветовые коды SA-MP {RRGGBB}. */
internal fun bastionStripSampColors(text: String): String = text.replace(bastionSampColorTag, "")

/** Bastion: раскрашивает текст по кодам SA-MP {RRGGBB}. */
internal fun bastionSampColored(text: String): CharSequence {
    val out = SpannableStringBuilder()
    var color: Int? = null
    var start = 0
    fun flush(end: Int) {
        if (end <= start) return
        val from = out.length
        out.append(text, start, end)
        color?.let { out.setSpan(ForegroundColorSpan(it), from, out.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE) }
    }
    for (match in bastionSampColorTag.findAll(text)) {
        flush(match.range.first)
        val rgb = match.groupValues[1].toInt(16)
        val r = (rgb shr 16) and 0xFF
        val g = (rgb shr 8) and 0xFF
        val b = rgb and 0xFF
        val light = (0.299 * r + 0.587 * g + 0.114 * b) > 200.0
        color = if (light) null else Color.rgb(r, g, b)
        start = match.range.last + 1
    }
    flush(text.length)
    return out
}
""", encoding="utf-8")
dlg = ui_dir / "NativeServerDialogController.kt"
dt = dlg.read_text(encoding="utf-8")
D_REPL = [
    ("            text = body.take(MAX_BODY_CHARS)\n",
     "            text = bastionSampColored(body.take(MAX_BODY_CHARS))\n"),
    ("        val dialogTitle = serverDialog.title.ifBlank {",
     "        val dialogTitle = bastionStripSampColors(serverDialog.title).ifBlank {"),
    ("\\n${tabHeader.replace('\\t', ' ')",
     "\\n${bastionStripSampColors(tabHeader).replace('\\t', ' ')"),
    ("            row.replace('\\t', ' ').ifBlank { \" \" }",
     "            bastionStripSampColors(row).replace('\\t', ' ').ifBlank { \" \" }"),
    ("        builder.setPositiveButton(serverDialog.button1.ifBlank {",
     "        builder.setPositiveButton(bastionStripSampColors(serverDialog.button1).ifBlank {"),
    ("            builder.setNegativeButton(serverDialog.button2) {",
     "            builder.setNegativeButton(bastionStripSampColors(serverDialog.button2)) {"),
    ("        dialog.setOnCancelListener { queueResponse(false) }\n",
     "        dialog.setCanceledOnTouchOutside(false)  // Bastion\n"
     "        dialog.setOnCancelListener { queueResponse(false) }\n"),
]
if "bastionSampColored" not in dt:
    for old, new in D_REPL:
        if dt.count(old) != 1:
            sys.exit("apply.py: не нашёл место в NativeServerDialogController.kt (обновился SampDroid?): " + old.strip()[:60])
        dt = dt.replace(old, new)
    dlg.write_text(dt, encoding="utf-8")
hud = ui_dir / "NativeWorldHudView.kt"
ht2 = hud.read_text(encoding="utf-8")
CHAT_OLD = "            val text = sanitize(line.text).take(MAX_CHAT_CHARS)\n"
if "bastionStripSampColors" not in ht2:
    if ht2.count(CHAT_OLD) != 1:
        sys.exit("apply.py: не нашёл вывод чата в NativeWorldHudView.kt (обновился SampDroid?)")
    hud.write_text(ht2.replace(CHAT_OLD, "            val text = sanitize(bastionStripSampColors(line.text)).take(MAX_CHAT_CHARS)\n"),
                   encoding="utf-8")

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
print(f"apply.py: автоподключение и папка игры OK, переведено строк: {len(done)} из {len(RU)}")
for k in missing:
    print("  не найдено (возможно, изменилось в SampDroid):", k[:80])
