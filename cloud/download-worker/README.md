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

## Предел защиты
Ключ лежит внутри APK и exe, его можно достать, разобрав лаунчер. Это защита от
скачивания по ссылке, а не от целенаправленного взлома.
