# Закрытая раздача game.zip через Backblaze B2

Бакет закрытый (Private). Лаунчеры (ПК и Android) перед скачиванием сами
получают временный токен (`b2_authorize_account`) по ключу **только на чтение**
и качают файл с заголовком `Authorization`. Ключ и адрес не лежат в репозитории:
GitHub Actions подставляет их из секретов при сборке.

## 1. Проверь, что архив — один файл
B2 → **Buckets** → бакет → **Browse Files**. В корне должен лежать один `game.zip`
(~581 МБ). Если там части (`game.zip.001`, `.part1` и т. п.) или папка с файлами
игры — перезалей архив одним файлом (Cyberduck сам соберёт большой файл из частей,
в списке он будет одной строкой).

## 2. Ключ только на чтение
B2 → **Application Keys** → **Add a New Application Key**:
- Name of Key: `bastion-launcher-read`
- Allow access to Bucket(s): только этот бакет
- Type of Access: **Read Only**

Сохрани `keyID` и `applicationKey` — второй показывается один раз.
Ключ с правом записи в лаунчеры не вставляй.

## 3. Секреты GitHub
Репозиторий → **Settings → Secrets and variables → Actions → New repository secret**:

| Имя | Значение |
|---|---|
| `BASTION_GAME_URL` | `b2://ИМЯ_БАКЕТА/game.zip` (имя бакета и путь к файлу в нём) |
| `BASTION_GAME_MD5` | MD5 архива (`Get-FileHash game.zip -Algorithm MD5`) |
| `BASTION_B2_KEY_ID` | keyID ключа только на чтение |
| `BASTION_B2_KEY` | applicationKey этого ключа |

`BASTION_DOWNLOAD_KEY` (для Cloudflare Worker) при B2 не нужен.

## 4. Пересобери лаунчеры
Actions → **Build Bastion PC launcher** и **Build single Bastion Android APK** →
**Run workflow** на `develop` (или дождись релиза `main`). В новых exe и APK кнопка
УСТАНОВИТЬ скачивает архив из B2.

## Новая версия архива
Залей новый `game.zip` поверх старого, обнови секрет `BASTION_GAME_MD5`,
пересобери лаунчеры. Сменить ключ = новый ключ в B2, новые `BASTION_B2_KEY_ID` и
`BASTION_B2_KEY`, пересборка; старые лаунчеры после удаления старого ключа скачать не смогут.

## Ограничения
- Ключ лежит внутри exe и APK: его можно достать, разобрав лаунчер. Поэтому ключ
  только на чтение и только к одному бакету.
- Новости, список серверов и обновления лаунчера с B2 пока не берутся (они настроены
  на Cloudflare Worker); без него лаунчер берёт их из `launcher.ini` и встроенных файлов.
- Бесплатно B2 отдаёт 3× объёма хранения в месяц (≈1,7 ГБ при 581 МБ, то есть
  примерно 3 скачивания); дальше $0,01 за ГБ. Через Cloudflare скачивание бесплатное.
