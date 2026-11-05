# Bastion Online для Android

Android-лаунчер Bastion Online: выбор сервера, онлайн и пинг, ник, новости, настройки
и запуск игры с твоим модом и кешем.

| Папка | Назначение |
|---|---|
| `launcher/` | Android-лаунчер на Java (без сторонних библиотек) |
| `installer/ANDROID-NATIVE-BUILD.md` | Сборка нативного мода `libsamp.so` в GitHub Actions |

## Сборка

- Android Studio: открыть `mobile/launcher`.
- Linux без Android Studio: `mobile/launcher/build.sh` (пакеты Ubuntu `android-sdk`,
  `android-sdk-platform-23`, `dalvik-exchange`). APK: `mobile/launcher/build/BastionOnline.apk`.
- GitHub Actions: workflow **Build Bastion Android APK** гоняет тесты и собирает debug APK.

Тесты ядра без Android SDK: `mobile/launcher/tools/run-unit-tests.sh` (нужны JDK и `junit4`).

## Что где менять

Все настройки сборки лежат в **`launcher/app/src/main/assets/launcher.ini`**, код трогать не нужно:

| Ключ | Что делает |
|---|---|
| `server = Название \| адрес \| порт \| описание \| пароль` | Сервер в списке. Сейчас два: Peace (7777) и War (7778) |
| `lock_servers` | `true` — адреса в Настройках становятся только для чтения и всегда берутся из ini |
| `require_rp_name` | Требовать ник вида `Имя_Фамилия` |
| `poll_interval` | Как часто обновлять онлайн и пинг, секунды |
| `game_activity` / `game_package` | Какую Activity запускает кнопка **ИГРАТЬ** |
| `require_file` | Файлы кеша, без которых запуск не начнётся |
| `title` / `subtitle` | Логотип: белая и оранжевая части |

Внешний вид (цвета, шрифты) — в одном файле `ui/Theme.java`. Новости — `assets/news.txt`.

### Блокировка адресов серверов

Пока сервера тестовые, `lock_servers = false`: в **Настройках → Адреса серверов** можно вписать
IP компьютера в Wi-Fi сети и порт. Для релиза впиши боевые адреса в `server = …` и поставь
`lock_servers = true`. После этого поля становятся серыми и неизменяемыми, а адреса, которые игрок
сохранял раньше, игнорируются.

## Как устроен код

```
online.bastion.launcher
├── MainActivity        жизненный цикл и переходы между экранами
├── core/               чистая Java, без Android, покрыта тестами
│   ├── LauncherConfig  разбор launcher.ini
│   ├── ServerAddress   проверка адреса и порта
│   ├── Nickname        правила ника SA-MP
│   ├── SampQuery       UDP query-протокол SA-MP/open.mp ('i' и 'p')
│   └── SampSettingsIni обновление SAMP/settings.ini без потери твоих правок
├── data/               SharedPreferences (ник, свои адреса, настройки игры), новости
├── net/ServerMonitor   фоновый опрос серверов, результат в главный поток
├── game/GameLauncher   проверка кеша → запись settings.ini → запуск игры
└── ui/                 Theme, элементы, экраны (MainScreen, SettingsScreen), окна (Overlay)
```

## Связь с модом и кешем

Мод (`jni/settings.cpp`) читает `g_pszStorage + "SAMP/settings.ini"`. Папка игры — это
`Android/data/online.bastion.launcher/files/`, поэтому игра собирается **в этот же APK**:
добавь её Activity в `AndroidManifest.xml` (пример там в комментарии) и укажи в `game_activity`.

Перед запуском лаунчер обновляет в `SAMP/settings.ini` только свои ключи, остальное не трогает:

```ini
[client]
name = Ivan_Petrov
host = 192.168.1.10
port = 7777
password =

[debug]
online = true
debug = false        ; только если ключа не было

[gui]
ChatMaxMessages = 6
FontSize = 37

[launcher]
fps_limit = 60       ; мод пока не читает, ключ на будущее
```

Те же ник, адрес и порт дублируются в Intent: `online.bastion.extra.NAME`, `…HOST`, `…PORT`.

Важно для мода: в текущем `jni/main.cpp` подключение идёт на `encryptedAddress` и `port`,
зашитые в код (сейчас пустые), а `szHost` / `iPort` из settings.ini не используются. Чтобы мод
подключался к серверу, выбранному в лаунчере, в `InitInGame()` нужно передавать в `CNetGame`
`pSettings->Get().szHost` и `pSettings->Get().iPort`.

Кеш: лаунчер проверяет файлы из `require_file` в папке игры. Если чего-то нет, кнопка
**ИГРАТЬ** показывает список файлов и путь к папке. Загрузку кеша ты пишешь сам.
