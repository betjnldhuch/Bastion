# Bastion + SAMP-Mobile GTA-2.11 — один APK

Игровой клиент закреплён submodule-ом `mobile/samp-mobile`:

- Upstream: https://github.com/kuzia15/SAMP-Mobile (ветка `GTA-2.11`)
- Закреплённый коммит: `0a805d4c3d1adbc0baccd02125901f9987cb1ab1`
- **Лицензия: файла LICENSE в репозитории нет.** README говорит «for educational
  and research purposes only». Без явной лицензии права на распространение кода
  не предоставлены — перед публикацией APK нужно согласие авторов
  (Egor Kuzn / kuzia15). Поэтому исходники клиента не копируются в Bastion,
  а подключаются submodule-ом и патчатся только в CI.
- `app/src/main/jniLibs/arm64-v8a/libGame.so` в upstream — движок GTA SA для
  Android (символы `OS_Rockstar*`), это проприетарный бинарник Rockstar Games.
  Отдельная установка GTA SA не нужна именно потому, что движок уже внутри
  клиента. Публичная раздача такого APK — юридический риск владельца сервера.

## Что делает `samp-mobile-patches/apply.py`

1. **Нативный путь к игре.** Вместо жёсткого `/storage/emulated/0/GTA/`
   (main.cpp, settings.cpp, hooks.cpp) используется `BastionStoragePath()` —
   значение `$BASTION_GTA_STORAGE`, иначе прежний путь.
2. **Java.** `GameActivityBase.onCreate` выставляет `BASTION_GTA_STORAGE` до
   загрузки `libGame.so`/`libsamp.so`: из extra `bastion.storage` или
   `Android/data/online.bastion.launcher/files/game/`. Разрешение на доступ ко
   всем файлам больше не нужно.
3. **Gradle.** `:app` → Android library (`com.gta.game` — только namespace).
   Удалены Firebase, google-services, crashlytics (их конфиг привязан к
   `com.gta.game`), `applicationId`, `applicationVariants`.
4. **Манифест.** Удалены стартовый экран upstream (`com.gta.launcher...MainActivity`)
   с LAUNCHER intent-filter, атрибуты `<application>`, `package`, `uses-sdk`,
   разрешения `MANAGE_EXTERNAL_STORAGE`, `REQUEST_INSTALL_PACKAGES`, storage/media,
   Firebase. Activity клиента `exported=false` и работают в процессе `:game`
   (выход из игры через `System.exit` не убивает лаунчер). FileProvider
   получает authority `online.bastion.launcher.provider`.
5. **Ресурсы.** Удалены иконки upstream (адаптивная `mipmap-anydpi-v26`
   перекрыла бы иконку Bastion), `app_name` = Bastion.
6. **JNI.** В APK добавлен `libGlossHook.so`, с которым слинкован `libsamp.so`.

## Сборка

Workflow **Build single Bastion Android APK** (`.github/workflows/bastion-single-apk.yml`),
запускается пушем в `integrate/**` / `main` или вручную. Artifact:
`Bastion-Launcher-SAMP-Mobile-debug`.

CI проверяет: единственный пакет и единственный launcher, наличие Activity
клиента и процесса `:game`, только `arm64-v8a`, что все `DT_NEEDED` нативных
библиотек есть в APK, выравнивание ELF LOAD = 16 КБ и zip-выравнивание
несжатых `.so` (`zipalign -P 16`), наличие патча хранилища в `libsamp.so`.

Успешная сборка доказывает компоновку, но не запуск игры на телефоне.
