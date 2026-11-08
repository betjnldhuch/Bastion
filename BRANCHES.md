# Ветки разработки Bastion Online

## Основные

| Ветка | Назначение |
|---|---|
| `main` | Стабильная версия. Только то, что проверено на сервере. Сюда сливается `develop` перед обновлением сервера. |
| `develop` | Общая ветка разработки. Все направления сливаются сюда через Pull Request. |

На 07.11.2026 `main` = мод с фракциями (`server/`), ПК-лаунчер (`launcher/`) и
мобильный лаунчер со встроенным SAMP-Mobile GTA-2.11 и установкой игры из ZIP
с телефона (`mobile/`, коммит `10cca32`, CI зелёный).

## Версии

Каждое обновление `main` автоматически сохраняется навсегда (workflow `release-main.yml`):
- ветка `release/<дата>-<коммит>` с кодом этой версии;
- релиз на странице Releases: APK, архив `gamemodes` с готовым `rp.amx`.

Первая сохранённая версия: `release/2026-11-08-8ec862f` (мод, установленный на сервер 08.11.2026).
Ветки `release/*` не удалять и не менять.

## Направления

Создаются от `develop`, PR обратно в `develop`. Под-задачи: `feature/<направление>-<что>`,
например `feature/server-factions-fbi`, PR в ветку направления или сразу в `develop`.

| Ветка | Что в ней делается | Файлы |
|---|---|---|
| `feature/server-factions` | Организации и фракции: гос. структуры, банды, ранги, рации, ворота, районы | `factions.inc`, `gates.inc`, `turfs.inc` |
| `feature/server-mechanics` | Механики: работы, транспорт, топливо, права, дома, бизнесы, клавиши, телефон, чат | `jobs.inc`, `vehicles.inc`, `houses.inc`, `business.inc`, `keys.inc`, `phone.inc`, `chat.inc` |
| `feature/server-bank-atm` | Банк, банкоматы, переводы, PayDay, налоги | `bank.inc` |
| `feature/server-accounts-admin` | Аккаунты, база данных, админка, баны, античит | `accounts.inc`, `database.inc`, `admin.inc` |
| `feature/launcher-pc` | ПК-лаунчер (C#) | `launcher/` |
| `feature/launcher-mobile` | Мобильный лаунчер и клиент: APK, установка из ZIP, патчи SAMP-Mobile | `mobile/` |
| `feature/forum` | Форум сервера (только читает `scriptfiles/rp.db`) | `forum/` |

Общие файлы (`rp.pwn`, `config.inc`, `base.inc`, `commands.inc`) правятся
маленькими отдельными PR, чтобы направления не конфликтовали.

Срочные исправления живой версии: `hotfix/<коротко>` от `main`, PR в `main`,
потом `main` сливается в `develop`.

## Правила

1. Не коммитить напрямую в `main` и `develop`, только через PR.
2. Перед PR `develop` → `main`: мод собирается в Qawno без ошибок, вход на сервер
   проверен с ПК и с телефона.

## Архив

Старые ветки разработки (`integrate/*`, `launcher-*`, `more-systems`, `initial-import`,
`claude/project-thread-*` ранних версий) удалены: всё из них уже в `main`.
Клиент SampDroid (`integrate/openmp-login-spawn`, релизы `android-v0.6.x`) не используется,
вместо него SAMP-Mobile GTA-2.11.

- `archive/main-2026-11-05` — состояние `main` до обновления 07.11.
- `archive/backup-2026-11-04`, `archive/feature-android-launcher-v2`,
  `archive/feature-android-mod-installer` — ранние варианты мобильного лаунчера,
  которых нет в `main`.
