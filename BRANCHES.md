# Ветки разработки Bastion Online

## Основные

| Ветка | Назначение |
|---|---|
| `main` | Стабильная версия. Только то, что проверено на сервере. Сюда сливается `develop` перед обновлением сервера. |
| `develop` | Общая ветка разработки. Все направления сливаются сюда через Pull Request. |

На 07.11.2026 `main` = мод с фракциями (`server/`), ПК-лаунчер (`launcher/`) и
мобильный лаунчер со встроенным SAMP-Mobile GTA-2.11 и установкой игры из ZIP
с телефона (`mobile/`, коммит `10cca32`, CI зелёный).

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

## Старые ветки

`integrate/*`, `feature/android-*`, `launcher-*`, `more-systems`, `initial-import`,
`backup/*`, `claude/project-thread-30q0tf`, `claude/project-thread-f2os3l` — история.
Всё, кроме перечисленного ниже, уже входит в `main`.

- `integrate/openmp-login-spawn` — альтернативный клиент SampDroid с патчем входа/спавна
  open.mp и e2e-тестом в CI (релизы `android-v0.6.x`). Заменён SAMP-Mobile GTA-2.11,
  оставлен как источник идей.
- `feature/android-launcher-v2`, `feature/android-mod-installer`, `backup/2026-11-04` —
  ранние варианты лаунчера, заменены текущим `mobile/launcher`.
- `archive/main-2026-11-05` — состояние `main` до обновления 07.11.
