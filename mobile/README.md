# Bastion Online для Android

Android-сборка — это **один APK** с единственным пакетом `online.bastion.launcher`
(только ARM64, Android 9+, поддержка 16 КБ страниц памяти).
Bastion Launcher остаётся главным экраном и единственной точкой входа: выбор сервера,
онлайн/пинг, ник, новости, настройки и ZIP-установщик игровых файлов. Клиент
**SAMP-Mobile GTA-2.11** подключается как Android Library и открывается внутри того же
приложения — отдельный пакет, значок клиента или установленная GTA SA не нужны.

Исходник клиента закреплён submodule-ом `mobile/samp-mobile`; CI применяет патчи
Bastion (`samp-mobile-patches/apply.py`) только в рабочей копии сборки.
Подробности и вопрос лицензии — в `SAMP-MOBILE-INTEGRATION.md`.

| Папка | Назначение |
|---|---|
| `launcher/` | Единственный устанавливаемый APK: интерфейс Bastion, серверы, ник, новости, настройки, установщик |
| `installer/` | Манифест и инструкция по публикации файлов клиента |
| `samp-mobile/` | Закреплённый исходный код клиента SAMP-Mobile (GTA-2.11) |
| `samp-mobile-patches/` | Подготовка Android Library, путь к игре от установщика, манифест |

## Сборка

Запусти workflow **Build single Bastion Android APK** (`bastion-single-apk.yml`) в GitHub Actions
или запушь изменения в ветку `integrate/**`. Из успешного запуска скачай artifact
`Bastion-Launcher-SAMP-Mobile-debug` — внутри один файл `Bastion-Launcher-SAMP-Mobile-debug.apk`.
Workflow проверяет пакет, Activity клиента, `lib/arm64-v8a/libGame.so` + `libsamp.so`
и все их зависимости, выравнивание 16 КБ и ZIP-установщик.

Старый `mobile/launcher/build.sh` и workflow `android-apk.yml` собирают лаунчер без
нативной библиотеки и для полной сборки не подходят.

## ZIP-установщик игры

Кнопка **УСТАНОВИТЬ** (на месте «Играть», пока игра не скачана) или
**Настройки → Установить / обновить игру**:

1. Скачивает ZIP (по умолчанию `http://192.168.31.182:8000/game.zip`), показывает проценты.
2. Для адреса по умолчанию сверяет MD5 архива.
3. Находит в архиве корень игры: `Data file Lite (samp)/ru.unisamp_mobile.game/files/`,
   а если его нет — самую верхнюю папку, где лежит `SAMP/` (кэш SAMP-Mobile — это
   содержимое `/storage/emulated/0/GTA/`). Распаковывает во временную папку (CRC
   каждого файла проверяется), затем заменяет ею
   `Android/data/online.bastion.launcher/files/game/`. При ошибке старая установка остаётся.
4. Проверяет установку: обязательна непустая `SAMP/`; если нет `texdb`, `audio`,
   `data`, `models` или `SAMP/fonts`, показывает предупреждение.
5. Игра считается установленной, когда есть отметка `.install_complete` и папка `SAMP`.

Отдельно установленная GTA SA не нужна и не проверяется.

## Что происходит при нажатии «Играть»

1. Bastion проверяет ник и сохраняет настройки в `bastion.cfg`.
2. Записывает в `files/game/SAMP/settings.ini` ник (`[client] name`), IP (`host`),
   порт (`port`), лимит FPS (`[gui] FPSLimit`) и строки чата (`ChatMaxMessages`);
   остальные ключи файла из кэша сохраняются.
3. Запускает `com.gta.game.SAMP` (процесс `:game` того же APK) и передаёт
   `bastion.storage` (корень игры), `bastion.host`, `bastion.port`, `bastion.name`,
   `bastion.nick`, `bastion.fps`, `bastion.quality`, `bastion.chat_lines`,
   `bastion.button_size`, `bastion.config`.
4. Клиент читает игру и `settings.ini` из папки установщика и сразу подключается
   к выбранному серверу.

Успешная CI-сборка подтверждает компоновку APK, но не заменяет проверку установки
и реального входа на сервер на телефоне.
