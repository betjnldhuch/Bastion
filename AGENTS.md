# AGENTS.md — правила для ИИ-агента (Codex и др.) в проекте Bastion Online

Это главный файл для агента. Подробная карта проекта, системы и команды — в `docs/HANDOFF.md`.
Полный бэкап и восстановление — `docs/BACKUP.md`. Инструкция по серверу — `server/README.md`.

## 1. Как работать с владельцем (обязательно)

- Отвечай **коротко и по-русски**. Не пересказывай, что сделал по шагам: только результат и что сделать владельцу.
- **После каждого изменения/слияния — одна строка, что обновить на ПК-сервере** и нужен ли перезапуск. Примеры:
  - «обнови gamemodes (rp.amx), перезапусти сервер»
  - «замени genintintpolicea.txd в gta_int.img»
  - «поставь новый APK», «обнови лаунчер ПК», «обнови BastionServer.exe», «поправь config.json».
  Без длинных пошаговых инструкций.
- **Всё делать сразу для ПК и для телефона (APK)**: графика, текстуры, HUD, окна, фичи клиента. Окно на сервере — для ПК через диалог/TextDraw, для телефона через HTML-окно (`ui.inc`) — обе ветки.
- **Графику не рисовать самому.** Иконки, текстуры, картинки: напиши готовый промпт для ChatGPT (размер, фон, стиль, что на картинке). Владелец сгенерирует и пришлёт, дальше ты нарезаешь и встраиваешь.
- **Большие файлы (`rp.pwn`, `factions.inc` ~5700 строк и др.) не читать целиком**: ищи `grep -n` нужную функцию и читай только кусок (`sed -n 'A,Bp'`).
- Не проверяй повторно то, что не менялось.
- **Компиляцию и CI запускать один раз** после всех правок, не после каждой. Не опрашивай сборку каждую минуту.
- Без «глубоких исследований» и пачек субагентов, если владелец прямо не попросил.
- Владелец разрешает сливать свои PR в `main`, если CI зелёный.

## 2. Кодировка — самое важное

- `*.pwn` и `*.inc` — в **Windows-1251** (иначе русский текст в игре ломается). `.gitattributes` хранит их байты как есть (`-text`).
- Читать: `iconv -f cp1251 -t utf-8 file.inc | sed -n '100,160p'`.
- Править: конвертируй в UTF-8, правь, конвертируй обратно `iconv -f utf-8 -t cp1251`, проверь, что `git diff` показывает только твои строки. Никогда не сохраняй `.inc` в UTF-8.
- Остальное (md, cs, py, kt, html, yml) — UTF-8.

## 3. Структура репо (коротко)

| Путь | Что |
|---|---|
| `server/gamemodes/rp.pwn` | главный файл мода (open.mp, Pawn, SQLite `scriptfiles/rp.db`) |
| `server/gamemodes/bastion/*.inc` | 80 модулей мода (таблица в `docs/HANDOFF.md`) |
| `server/starter/` | `BastionServer.exe` (C#, .NET 4, C# 5): старт/стоп, логи, обновление мода, бэкап, **Telegram-бот** (`Telegram.cs`) |
| `server/tools/` | PowerShell-скрипты бэкапа базы |
| `launcher/` | ПК-лаунчер (C#), `launcher.ini` (адреса серверов), `client-pack/` (HUD, txd, интерьеры) |
| `mobile/` | Android: лаунчер `online.bastion.launcher` + submodule `mobile/samp-mobile` (SAMP-Mobile GTA-2.11) + `samp-mobile-patches/` |
| `client-mods/` | исходники модов клиента (txd/dff, HTML-окна `bastion-ui`), сборка `client-mods/tools/build_*.py` |
| `forum/`, `bots/`, `cloud/` | сайт/форум (Python), боты VK/TG, раздача game.zip |
| `.github/workflows/` | CI (см. ниже) |

## 4. Сборка и где брать файлы

- **rp.amx**: CI `server-build.yml` (на push/PR с изменениями в `server/**`), компилятор pawncc 3.10.11 + omp-stdlib:
  `pawncc rp.pwn -i<omp-stdlib> -i. -orp.amx -d3 '-;+' '-(+'` в `server/gamemodes`. Артефакт сборки или релиз.
- **Всё сразу**: каждый push в `main` → `release-main.yml` → релиз `main-<дата>-<коммит>` (APK, ПК-лаунчер, `Bastion-gamemodes` с rp.amx) и ветка `release/*` (не удалять).
- **APK**: `bastion-single-apk.yml` (артефакт `Bastion-Launcher-SAMP-Mobile-debug`, символы `Bastion-native-symbols`). Вылеты разбирать `symbolize-crash.yml`.
- **ПК-лаунчер**: `pc-launcher.yml` (артефакт `BastionLauncher-PC`). **Стартер**: `server-starter.yml`. **HTML-окна для ПК**: `bastion-ui-pc.yml`.
- Секреты Actions (`BASTION_GH_TOKEN`, `BASTION_DOWNLOAD_KEY`, `BASTION_GAME_URL`, `BASTION_GAME_MD5`, `BASTION_PC_GAME_URL`, `BASTION_PC_GAME_MD5`, `BASTION_B2_KEY_ID`, `BASTION_B2_KEY`) — не трогать и не печатать.

## 5. Как добавлять в мод

- Новая команда: функция `Cmd_X` в модуле + строка в `Commands_Process` (`bastion/commands.inc`) + пункт в `/help` (`help.inc`).
- Номера диалогов — только в `dialogs.inc`. Новые настройки с цифрами — в `/srvset` (`srvcfg.inc`: enum `CFG_*` + `gCfgDef`, в коде `Cfg(CFG_...)`).
- Окна: для телефона — HTML-окно через `ui.inc` (`UI_Has(playerid)`), иначе обычный диалог. Делать обе ветки.
- Таблицы SQLite создаются модулем сам (`CREATE TABLE IF NOT EXISTS`).
- Ветки: работать в своей ветке, PR в `main`. Описание PR — по-русски, коротко.

## 6. Что открыто

Список открытых задач и ожиданий от владельца — в `docs/HANDOFF.md`, раздел «Что осталось».
