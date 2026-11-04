# Bastion Online для Android

Мобильная версия: один APK с лаунчером и (позже) своим клиентом мультиплеера.
Игрок устанавливает совместимую GTA: San Andreas для Android. Лаунчер проверяет её наличие и устанавливает файлы Bastion из удалённого манифеста.

| Папка | Что это |
|---|---|
| `launcher/` | Лаунчер (Java, Android 5.0+): выбор сервера, онлайн и пинг, ник, новости, настройки |

## Как поставить на телефон

1. Скачай `BastionOnline.apk` и открой его на телефоне.
2. Android спросит разрешение ставить приложения из этого источника: разреши.
3. Открой Bastion Online, зайди в **Настройки** и впиши адрес сервера.
   Для теста дома: адрес компьютера в Wi-Fi (на ПК команда `ipconfig`, строка IPv4).
   Сервер на ПК должен быть запущен, а телефон в той же Wi-Fi сети.

## Как собрать

- **Android Studio**: открой папку `mobile/launcher` и нажми Run.
- **Linux без Android Studio**: `mobile/launcher/build.sh`
  (нужны пакеты Ubuntu `android-sdk android-sdk-platform-23 dalvik-exchange` и JDK).
  APK появится в `mobile/launcher/build/BastionOnline.apk`.

`tools/debug.keystore` — тестовый ключ подписи (пароль `android`), чтобы новые тестовые
сборки ставились поверх старых. Для публичного релиза сделаем отдельный ключ и
в репозиторий его не кладём.

## Что лаунчер уже делает

- Карточки серверов из `assets/servers.ini` (тот же формат, что `launcher.ini` на ПК),
  онлайн и пинг по query-протоколу SA-MP каждые 10 секунд.
- Ник с проверкой формата Имя_Фамилия.
- Новости из `assets/news.txt`.
- Настройки: FPS 30/60/90/120, качество, строки чата, размер кнопок, адреса серверов.
- Проверка, что GTA: San Andreas установлена (пакет `com.rockstargames.gtasa`).

- Кнопка «Играть» открывает установщик Bastion.
- Установщик читает `mobile/installer/manifest.json`, скачивает перечисленные файлы по HTTPS и проверяет SHA-256.
- Файлы Bastion сохраняются в приватную папку приложения `files/bastion-mod`; манифест можно дополнять новыми файлами и обновлениями.
- Пока манифест пустой: добавь в него реальные URL файлов мода и их SHA-256.
- Установщик не содержит сам мобильный SA-MP/open.mp игровой клиент. Для полноценного входа в игру нужен совместимый Android-клиент; его исходников или готовой сборки в репозитории пока нет. GTA SA также должна быть установлена отдельно.

- Настройки → «Диагностика»: отчёт о телефоне и GTA SA (версия, ABI, SHA-256 libGTASA.so),
  его можно скопировать или отправить. Нужен, чтобы подобрать адреса функций игры для этапа 1.
- По кнопке «Играть» пишет `Android/data/online.bastion.launcher/files/bastion.cfg`
  (ник, сервер, настройки) — его прочитает игровой клиент на следующих этапах.


## Android SA-MP client handoff

The Bastion launcher can open an independently installed SampDroid client
(package `com.xykal.sampdroid`) from the installer screen. If it is absent,
the launcher opens the upstream source repository so the player can review and
install a suitable build themselves. The launcher does not silently download
or install an unsigned third-party APK.

This is currently a launch handoff only:
- nickname and server selection still happen inside SampDroid;
- SampDroid does not currently expose a documented external-intent API for
  receiving Bastion's nickname/server configuration;
- files downloaded by the Bastion installer remain in the launcher's private
  app directory and are **not yet loaded by SampDroid**.

Therefore this step does not claim that Bastion assets are applied in-game.
A later integration requires a documented client-side bridge/API and a
compatible, tested build of the Android client.
