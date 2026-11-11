# Закрытая раздача game.zip через Cloudflare

Бакет R2 остаётся закрытым. Файл отдаёт Worker (`worker.js`), и только тем
запросам, где есть заголовок `X-Bastion-Key` с ключом лаунчера. Без ключа
ответ 404. Свой домен не нужен: Worker живёт на `*.workers.dev`.

Ключ и ссылка не хранятся в репозитории: при сборке их подставляет
GitHub Actions из секретов. Ключ лаунчер отправляет только по https и только
на хост из `BASTION_GAME_URL`, на чужие ссылки он не уходит.

## 1. Бакет R2
1. dash.cloudflare.com → **R2 Object Storage** → **Create bucket**, имя `bastion`.
2. Залей `game.zip`. Сайт принимает файлы до 300 МБ; больше — через rclone или
   Cyberduck (ключ: R2 → **Manage API tokens**).
3. **Public access не включай.**

## 2. Worker
1. **Workers & Pages** → **Create** → **Create Worker** → имя `bastion-download` → **Deploy**.
2. **Edit code**, удали всё и вставь `worker.js` целиком → **Deploy**.
3. Worker → **Settings → Bindings → Add → R2 bucket**: имя переменной `GAME`, бакет `bastion`.
4. Worker → **Settings → Variables and Secrets → Add**: тип **Secret**, имя `DOWNLOAD_KEY`,
   значение — длинный случайный ключ (латиница и цифры).
5. Адрес архива: `https://bastion-download.<твой-ник>.workers.dev/game.zip`.
   В браузере он должен показать «Not found» — так и надо, ключа у браузера нет.

## 3. Секреты GitHub
Репозиторий → **Settings → Secrets and variables → Actions → New repository secret**:

| Имя | Значение |
|---|---|
| `BASTION_GAME_URL` | адрес из шага 2.5 |
| `BASTION_GAME_MD5` | MD5 архива (`Get-FileHash game.zip -Algorithm MD5`) |
| `BASTION_DOWNLOAD_KEY` | тот же ключ, что `DOWNLOAD_KEY` в Worker |

После этого перезапусти сборки **Build single Bastion Android APK** и
**Build Bastion PC launcher**: в новых APK и exe будут ссылка и ключ.

## Проверка
```
curl -I https://bastion-download.<ник>.workers.dev/game.zip                         # 404
curl -I -H "X-Bastion-Key: <ключ>" https://bastion-download.<ник>.workers.dev/game.zip # 200 и размер
```

## Новая версия архива
Залей новый `game.zip` в бакет поверх старого и обнови секрет `BASTION_GAME_MD5`,
затем пересобери лаунчеры. Сменить ключ = новый `DOWNLOAD_KEY` в Worker и тот же
в `BASTION_DOWNLOAD_KEY`; старые лаунчеры после этого скачать архив не смогут.

## Копии базы сервера в R2

Сервер раз в сутки сам сохраняет `scriptfiles/rp_backup_ГГГГ-ММ-ДД.db`. Чтобы эти копии
уходили в R2:

1. Worker → **Edit code**: вставь свежий `worker.js` из репозитория → **Deploy**.
2. Worker → **Settings → Variables and Secrets → Add**: тип **Secret**, имя `BACKUP_KEY`,
   значение — новый длинный случайный ключ. **Не тот же, что `DOWNLOAD_KEY`**: этот ключ
   в лаунчеры не попадает и нигде, кроме ПК с сервером, лежать не должен.
3. Скопируй `server/tools/backup_r2.bat` в папку сервера (рядом с `omp-server.exe`),
   открой Блокнотом и впиши адрес Worker и `BACKUP_KEY`.
4. Запусти файл двойным щелчком: должно появиться `Saved backups/rp_backup_....db`.
   Копия видна в панели Cloudflare: R2 → бакет → папка `backups`.
5. Планировщик заданий Windows → **Создать простую задачу** → ежедневно → запуск `backup_r2.bat`.

Хранится 30 последних копий, старые Worker удаляет сам. Скачать копию можно только
из панели Cloudflare: через Worker базу не отдаёт ни один ключ, потому что в ней хеши
паролей игроков. В GitHub этот ключ и сами копии не клади.

Вернуть базу: скачай нужный файл из панели, останови сервер, переименуй файл в `rp.db`,
положи в `scriptfiles`, запусти сервер.

## Новости, список серверов и обновления лаунчеров

Папка `launcher/` в бакете открыта всем, без ключа. Лаунчеры на ПК и телефоне сами
берут из неё три файла (адрес они знают из `BASTION_GAME_URL`):

| Файл в бакете | Что это | Образец |
|---|---|---|
| `launcher/news.txt` | новости: `дата \| заголовок \| текст` | `launcher/remote/news.txt` |
| `launcher/servers.ini` | сервера: `server = Имя \| адрес \| порт \| описание` | `launcher/remote/servers.ini` |
| `launcher/version.ini` | номер свежей версии и имя файла обновления | `launcher/remote/version.ini` |

Как включить:
1. Worker → **Edit code**: вставь свежий `worker.js` → **Deploy**.
2. R2 → бакет → **Create folder** `launcher` → залей в неё файлы из `launcher/remote/`,
   вписав свои новости и настоящий адрес сервера. Сохраняй в UTF-8.
3. Проверка в браузере: `https://bastion-download.<ник>.workers.dev/launcher/news.txt` показывает текст.

Поменял новость или адрес сервера — залей файл поверх. У игроков он обновится при
следующем запуске лаунчера (до минуты кэша). Файла нет или нет сети — лаунчер
показывает последнее скачанное, а если и его нет, то вшитое при сборке.

Выпуск новой версии лаунчера:
- **ПК**: подними `AssemblyVersion` в `launcher/source/Core.cs`, собери (Actions →
  **Build Bastion PC launcher**), скачай архив, залей его как `launcher/BastionLauncher.zip`
  и впиши тот же номер в `pc_version`. Лаунчеры предложат обновиться и заменят себя сами.
- **Телефон**: подними `versionCode` в `mobile/launcher/app/build.gradle`, собери APK,
  залей как `launcher/Bastion.apk` и впиши номер в `android_version_code`. Лаунчер
  покажет окно со ссылкой, APK ставится поверх.

Сначала заливай файл, потом меняй номер в `version.ini`. В папку `launcher/` не клади
ничего секретного: копии базы и `game.zip` лежат отдельно и без ключа не отдаются.

## Предел защиты
Ключ лежит внутри APK и exe, его можно достать, разобрав лаунчер. Это защита от
скачивания по ссылке, а не от целенаправленного взлома.
