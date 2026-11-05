# Bastion Online для Android

Android-сборка собирается как **один APK** с единственным пакетом `online.bastion.launcher`.
Твой Bastion Launcher остаётся главным экраном и единственной точкой входа; SampDroid подключается как Android Library и открывается внутри того же приложения. Отдельный пакет/значок Bastion Native Client больше не нужен.

Исходник игрового клиента закреплён submodule-ом `mobile/sampdroid`; CI применяет локализацию и преобразует его app-модуль в library только в рабочей копии сборки. Игровые файлы GTA: San Andreas в APK не включаются: игрок выбирает собственные файлы через SAF.

| Папка | Что это |
|---|---|
| `launcher/` | Единственный устанавливаемый APK: интерфейс Bastion, серверы, ник, новости и настройки |
| `sampdroid/` | Закреплённый исходный код нативного SA-MP/open.mp клиента |
| `sampdroid-patches/` | Русская локализация, передача параметров сессии и подготовка Android Library |

## Сборка

Запусти workflow **Build single Bastion Android APK** в GitHub Actions.
Из успешного запуска скачай artifact `Bastion-Launcher-Embedded-debug`. Внутри один файл `Bastion-Launcher-Embedded-debug.apk`.

Локальная ручная сборка через старый `mobile/launcher/build.sh` не поддерживает нативную библиотеку; для полной сборки используй Android Studio/Gradle с установленными Android SDK, NDK 26.1.10909125 и CMake 3.22.1.

## Что происходит при нажатии «Играть»

1. Bastion сохраняет ник и выбранный сервер.
2. Запускает `com.xykal.sampdroid.ui.MainActivity` как Activity того же APK.
3. Передаёт `bastion.host`, `bastion.port`, `bastion.name` и `bastion.nick`.
4. Клиент сохраняет ник, проверяет сервер и открывает экран подключения.

Наличие успешной CI-сборки подтверждает компоновку APK, но не заменяет проверку установки и реального входа на сервер на телефоне.
