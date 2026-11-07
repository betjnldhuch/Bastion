# Bastion Online для Android

Android-сборка — это **один APK** с единственным пакетом `online.bastion.launcher`.
Bastion Launcher остаётся главным экраном и единственной точкой входа: выбор сервера,
онлайн/пинг, ник, новости, настройки и ZIP-установщик игровых файлов. SampDroid
подключается как Android Library и открывается внутри того же приложения — отдельный
пакет или значок клиента не нужен.

Исходник игрового клиента закреплён submodule-ом `mobile/sampdroid`; CI применяет патчи
Bastion и преобразует его app-модуль в library только в рабочей копии сборки.

| Папка | Назначение |
|---|---|
| `launcher/` | Единственный устанавливаемый APK: интерфейс Bastion, серверы, ник, новости, настройки, установщик |
| `installer/` | Манифест и инструкция по публикации файлов клиента |
| `sampdroid/` | Закреплённый исходный код нативного SA-MP/open.mp клиента |
| `sampdroid-patches/` | Автоподключение, папка игры от установщика, русская локализация, подготовка Android Library |

## Сборка

Запусти workflow **Build single Bastion Android APK** (`bastion-single-apk.yml`) в GitHub Actions
или запушь изменения в ветку `integrate/**`. Из успешного запуска скачай artifact
`Bastion-Launcher-Embedded-debug` — внутри один файл `Bastion-Launcher-Embedded-debug.apk`.
Workflow сам проверяет, что в APK есть `lib/armeabi-v7a/libsampdroid.so` и
`lib/arm64-v8a/libsampdroid.so`, Activity клиента и ZIP-установщик.

Старый `mobile/launcher/build.sh` и workflow `android-apk.yml` собирают лаунчер без
нативной библиотеки и для полной сборки не подходят.

## ZIP-установщик игры

Кнопка **УСТАНОВИТЬ** (на месте «Играть», пока игра не скачана) или
**Настройки → Установить / обновить игру**:

1. Скачивает ZIP (по умолчанию `http://192.168.31.182:8000/game.zip`), показывает проценты.
2. Для адреса по умолчанию сверяет MD5 архива.
3. Распаковывает из архива папку `Data file Lite (samp)/ru.unisamp_mobile.game/files/`
   во временную папку, затем заменяет ею
   `Android/data/online.bastion.launcher/files/game/`. При ошибке старая установка остаётся.
4. Игра считается установленной, когда есть отметка `.install_complete` и папка `SAMP`.

Отдельно установленная GTA SA не нужна и не проверяется.

## Что происходит при нажатии «Играть»

1. Bastion проверяет ник и сохраняет настройки в `bastion.cfg`.
2. Запускает `com.xykal.sampdroid.ui.MainActivity` как Activity того же APK.
3. Передаёт `bastion.host`, `bastion.port`, `bastion.name`, `bastion.nick`,
   `bastion.game_dir` (папка с `data/gta.dat` внутри `files/game`),
   `bastion.fps`, `bastion.quality`, `bastion.chat_lines`, `bastion.button_size`
   и `bastion.config` (путь к `bastion.cfg`).
4. Клиент подключает папку игры от установщика (без ручного выбора папки),
   сохраняет ник, проверяет сервер и открывает экран подключения.

Успешная CI-сборка подтверждает компоновку APK, но не заменяет проверку установки
и реального входа на сервер на телефоне.
