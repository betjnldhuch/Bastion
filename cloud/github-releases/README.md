# Раздача архива игры через GitHub Releases (+ зеркала)

Бесплатно, без карты и без лимита на число скачиваний. Один файл релиза — до 2 ГБ,
поэтому большой архив для ПК режется на части (лаунчер сам скачает и склеит).
Яндекс Диск остаётся запасным зеркалом: если GitHub не отдаёт файл, лаунчер
переключается на Диск и докачивает с того же места.

## 1. Отдельный репозиторий
github.com → **New repository**, например `bastion-files`. Код Bastion туда не клади,
только части архива.

- **Public** — ссылки обычные (`https://github.com/…/releases/download/…`), токен не нужен.
- **Private** — ссылки вида `gh://<ник>/bastion-files/game-1/game.zip.part1`, лаунчер
  получает файл по токену только на чтение (см. ниже). Токен лежит внутри exe/APK,
  поэтому вытащить его можно, но он даёт только чтение этого одного репозитория.

### Токен для приватного репо
github.com → Settings → Developer settings → **Fine-grained tokens** → Generate new token:
- Expiration: максимальный срок (по истечении лаунчеры перестанут качать — выпусти новый и пересобери);
- Repository access: **Only select repositories** → `bastion-files`;
- Permissions → Repository → **Contents: Read-only** (больше ничего).

Скопируй токен в секрет `BASTION_GH_TOKEN` репозитория bastion.
Лимит API GitHub — 5000 запросов в час на токен (около 2 на каждую часть), для
разработки с запасом.

## 2. Части архива
Нарежь `game.zip` на части по 900 МБ командой из `cloud/yandex-disk/README.md`
(получатся `game.zip.part1`, `game.zip.part2`, …). Архив меньше 2 ГБ можно не резать.
Для зеркала на Яндекс Диске используй **те же самые** части.

## 3. Релиз
Репозиторий → **Releases** → **Create a new release** → тег `game-1` →
перетащи части в **Attach binaries** → **Publish release**.

Ссылка на часть: публичный репо — `https://github.com/<ник>/bastion-files/releases/download/game-1/game.zip.part1`,
приватный — `gh://<ник>/bastion-files/game-1/game.zip.part1`.

## 4. Секреты в репозитории bastion
Settings → Secrets and variables → Actions:

| Имя | Значение |
|---|---|
| `BASTION_PC_GAME_URL` | части GitHub через `\|`, потом `;`, потом те же части на Яндекс Диске через `\|` |
| `BASTION_PC_GAME_MD5` | MD5 целого `game.zip` (не меняется) |
| `BASTION_GAME_URL` | архив для телефона: ссылка GitHub `;` ссылка Яндекс Диска |

Пример `BASTION_PC_GAME_URL` (одной строкой):
```
https://github.com/<ник>/bastion-files/releases/download/game-1/game.zip.part1 | https://github.com/<ник>/bastion-files/releases/download/game-1/game.zip.part2 ; https://disk.yandex.ru/d/AAA | https://disk.yandex.ru/d/BBB
```
У всех зеркал должно быть одинаковое число частей. Зеркал может быть до 16, в любом
сочетании: GitHub (`https://` или `gh://`), Яндекс Диск (в том числе с разных аккаунтов), прямая https-ссылка, `b2://`.

## 5. Пересборка
Actions → **Build Bastion PC launcher** и **Build single Bastion Android APK** → Run workflow.

## Новая версия архива
Новый релиз (`game-2`) с новыми частями, новые ссылки и MD5 в секретах, пересборка.
Уже скачанное у игроков с прежним MD5 при смене зеркал не теряется.
