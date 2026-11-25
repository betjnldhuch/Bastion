# Bastion Online — передача проекта (25.11.2026)

Состояние на коммите `main` **7a4d610** (PR #68, админка /amenu). Правила работы для агента — в `AGENTS.md` в корне.

## 1. Что это

Свой RP-сервер GTA San Andreas Multiplayer на **open.mp** (Pawn, SQLite). Два названия на одном коде: **Bastion Online: Peace** и **War**.
Игроки заходят с ПК (свой лаунчер, GTA SA 1.0 + SA-MP 0.3.7) и с телефона (свой APK: лаунчер + SAMP-Mobile, Android 9+, ARM64).
Сервер стоит у владельца на ПК с Windows (`C:\openmp\Server`), запускается стартером `BastionServer.exe`.

## 2. Компоненты и сборка

| Что | Где в репо | Сборка (CI) | Что получает владелец |
|---|---|---|---|
| Мод | `server/gamemodes/rp.pwn` + `bastion/*.inc` | `server-build.yml` (push/PR в `server/**`) | `rp.amx` → папка `gamemodes`, перезапуск сервера |
| Стартер + Telegram-бот | `server/starter/` (`Starter.cs`, `Telegram.cs`, `Logs.cs`) | `server-starter.yml` | `BastionServer.zip` рядом с `omp-server.exe`. Бот: long polling, токен и `tg_admins` в `BastionServer.ini` |
| ПК-лаунчер | `launcher/source/*.cs`, `launcher.ini`, `client-pack/` | `pc-launcher.yml` | `BastionLauncher-PC.zip` |
| APK | `mobile/launcher` + submodule `mobile/samp-mobile` + `mobile/samp-mobile-patches` | `bastion-single-apk.yml` | `Bastion-Launcher-SAMP-Mobile-debug.apk` |
| HTML-окна | `mobile/launcher/app/src/main/assets/ui/`, `client-mods/bastion-ui/` | `bastion-ui-pc.yml` | входят в APK и в клиент-пак ПК |
| Текстуры/HUD/карта | `client-mods/*` (`radar`, `map`, `markers`, `weapons`, `lspd`, `hospital`, `bank`, `bar`, `shop`, `dealer`, `gangs`) | `python client-mods/tools/build_*.py` | `.txd` в `gta3.img`/`gta_int.img`/`models/txd`, `radar/hud.txd` |
| Всё вместе | — | `release-main.yml` (каждый push в `main`) | релиз `main-<дата>-<коммит>` + ветка `release/*` |

Прочее: `symbolize-crash.yml` (разбор вылетов APK по `#00 pc ...`), `cleanup-artifacts.yml` (чистка артефактов), `forum/` (сайт и форум на Python, читает `rp.db`), `bots/` (боты VK/TG), `cloud/` (раздача `game.zip`: Cloudflare Worker, B2, Я.Диск, GitHub Releases).
Компиляция мода вручную: Qawno → `rp.pwn` → F5. Локально: `pawncc rp.pwn -i<omp-stdlib> -i. -orp.amx -d3 '-;+' '-(+'`.

## 3. Ключевые системы и команды

Полный список команд: в игре `/help` (с поиском), в коде `bastion/commands.inc` (`Commands_Process`), `server/README.md`.

- **Аккаунты**: регистрация Имя_Фамилия, SHA-256 + соль, `accounts.inc`. `/stats`, `/bot` (привязка VK/TG).
- **Фракции** (`factions.inc`, `fmanage.inc`, `orgs.inc`): полиция, больница, армия, ФБР, тюрьма, пожарные (тип 9), банды. `/duty`, `/cuff`, `/arrest`, `/su`, `/d` (чат госструктур), `/gd` (чат банд), `/fbank`, `/fsklad`, `/lmenu`, `/fmute`, `/swarn`. Права по рангам и личные.
- **/orgs** — панель главного админа по всем организациям: лимиты (`/flimit`), доступы (`/deptaccess`), спец-выговоры, настройки заводов и саботажа.
- **Заводы** (`factions.inc`): военные (только владелец) и обычные (захват). Работа по ALT: сырьё → станок → ящик на склад. Владелец выбирает продукцию: материалы, топливо, продукты, запчасти, ширпотреб. Ящики везут в бизнесы (`/unload` или H), `/contracts`, `/supply`, `/factories`, `/market` (рынок). **Саботаж** — поджог склада (`/sabotage`, ALT).
- **Пожары и пожарные** (`fire.inc`): очаги огня, тушат огнетушителем или из машины 407/544 (`/addfcar`), плата за очаг и бонус. Источники: саботаж, случайные пожары в бизнесах, `/startfire`/`/stopfire`. `/fires` (GPS), `/firecheck` — пожарная проверка бизнеса (нет сигнализации → штраф). Сигнализация — улучшение бизнеса.
- **Бизнесы** (`business.inc`, `bizplus.inc`, `bizwork.inc`, `bizstat.inc`): типы `/biztypes`, `/biz`, `/bizmenu`, `/bizinfo`, `/bizcash`, `/bizlock`, `/bizlist`, `/bizquit`, сотрудники, налог, улучшения, выписка. Закупка «из воздуха» дороже на `biz_air%`.
- **/srvset** (`srvcfg.inc`, только админ 7): все цифры сервера из игры без перезапуска (таблица `server_settings`): игроки и цены, банк, фракции, банды/районы, бизнесы, заводы, поставки и рынок, пожары, налоги (`/settax`).
- **Админка** (`admin.inc`, `admin_auth.inc`, `admin_tools.inc`, `admin_give.inc`, `admin_plus.inc`): 7 уровней, вход по отдельному паролю, `/makeadmin`. **`/amenu`** — панель (игроки, репорты, наказания, античит, ТП, МП, рейтинг, лог). Наказания по правилам: `/punish`, быстрые `/dm /db /sk /tk /rk /pg /mg /nrp /caps /flood /osk /oskrod /oska /bag /uhod /rekl /cheat /obman /nick` (срок за повтор растёт), `/rules`, `/history`, офлайн `/offwarn /offmute /offjail`, `/acheck` (лента античита), `/aip`, `/atop` (норма), `/event` + `/mp`, `/ann`, `/cc`, `/ban /kick /jail /mute /warn`, `/backup`.
- **Ежедневные задания** (`daily.inc`): `/daily` (`/tasks`), уровни профессий `/jobstat`.
- **Сезонный пропуск** (`season.inc`): `/season` (`/pass`), 30 уровней, бесплатная и премиум дорожки; админ `/newseason`, `/seasonxp`.
- **Семьи и войны семей** (`families.inc`, `famwar.inc`): `/famcreate`, `/fam`, `/famhelp`, `/faminvite`, `/fambank`, `/famhouse`…; войны по расписанию `/famwar`, `/famwars`, `/famtop`.
- **Районы банд** (`turfs.inc`): `/turfs`, `/capture`.
- **Суд и адвокаты** (`court.inc`): `/lawyer`, `/free`, `/appeal`, `/cases`.
- **Аукцион домов** (`auction.inc`): `/auction` (`/auctions`), `/bid`, `/unauction`. Дома: `/house`, `/houses`, `/housepay`.
- **Квест новичка** (`quest.inc`, `referral.inc`): «Первые шаги», `/quest` (`/quests`), правила и «кто пригласил».
- **Телефон** (`phone.inc`, `phoneapp.inc`): `/phone`, HTML-телефон с приложениями (окно `phone`).
- **Паспорт и документы** (`docs.inc`, `licenses.inc`): `/passport`, `/showpass`, `/showlic`, `/showmedcard`.
- **HUD и карта** (`hud.inc`, `gps.inc`, `markers.inc`): свой HUD (панель статуса, спидометр) `/hud`, `/gps`, `/markers`, `/maptp`. Клиентская часть: `radar/hud.txd`, `client-mods/radar`, `client-mods/map`, `client-mods/markers`.
- Ещё: банк `/bank`, экономика/казна/долги, работы `/jobs` и `worklife.inc`, транспорт/топливо/СТО/автосалоны, VIP (`/vip…`) и донат `/donate`, казино, наркотики, похищения, ограбления, ранения, потребности и инвентарь, брак, античит, FPS, «меню помнят последний выбор» (`menumem.inc`), ежедневный бэкап базы.

## 4. Модули мода (`server/gamemodes/bastion/`)

| Файл | Строк | Что делает |
|---|---|---|
| `accounts.inc` | 362 | регистрация, вход, сохранение аккаунта. |
| `admin.inc` | 2095 | система администрации. |
| `admin_auth.inc` | 582 | вход в админку по отдельному паролю и учёт работы. |
| `admin_give.inc` | 197 | админские выдачи: оружие, броня, лечение, деньги. |
| `admin_plus.inc` | 1150 | развитие админки (всё через окна: ПК и телефон). |
| `admin_tools.inc` | 707 | телепорты и инструменты администрации. |
| `anticheat.inc` | 459 | античит. |
| `auction.inc` | 380 | аукцион домов: /auction (/auctions), /bid, /unauction. |
| `backup.inc` | 83 | резервные копии базы. |
| `bank.inc` | 720 | банк и банкоматы. |
| `base.inc` | 369 | данные игроков и общие функции-помощники. |
| `bizplus.inc` | 888 | новые виды бизнесов и управление для владельца. |
| `bizstat.inc` | 315 | финансовая выписка бизнеса и история дохода по дням. |
| `bizwork.inc` | 615 | сотрудники бизнеса, налог и улучшения. |
| `business.inc` | 994 | бизнесы: магазины, бары, одежда, оружие. |
| `casino.inc` | 369 | казино и лотерея штата. |
| `chat.inc` | 189 | RP-чат. |
| `commands.inc` | 487 | список всех команд и общие команды игрока. |
| `config.inc` | 134 | все настройки сервера в одном месте. |
| `court.inc` | 346 | суд и адвокаты: /lawyer, /free, /appeal, /cases. |
| `daily.inc` | 430 | ежедневные задания: /daily. |
| `database.inc` | 121 | база данных SQLite. |
| `dealers.inc` | 485 | автосалоны, авиасалон и личный транспорт игрока. |
| `debts.inc` | 154 | долги игрока перед штатом: налоги и штрафы. |
| `dev.inc` | 638 | инструменты разработки прямо в игре (админ DEV_LEVEL+). |
| `dialogs.inc` | 347 | номера всех окон (диалогов) сервера в одном месте. |
| `docs.inc` | 524 | документы в сумке: паспорт, медкарта, лицензии. |
| `donate.inc` | 656 | донат-меню: донат-монеты и что на них купить. |
| `doors.inc` | 451 | входы в здания со стандартными интерьерами GTA. |
| `drugs.inc` | 626 | наркотики: выращивание, торговля, эффекты, ломка, изъятие. |
| `economy.inc` | 258 | казна штата, налоги и журнал денег. |
| `factions.inc` | 5723 | фракции: полиция, больница, банды. |
| `families.inc` | 1733 | семьи: организации, которые создают сами игроки. |
| `famwar.inc` | 399 | войны семей по расписанию: /famwar, /famwars, /famtop. |
| `fbi.inc` | 349 | спецсредства ФБР. |
| `fire.inc` | 395 | пожары и пожарные. |
| `fmanage.inc` | 854 | управление фракцией: права по рангам и личные права, |
| `fps.inc` | 118 | FPS игроков. |
| `fuel.inc` | 542 | топливо, АЗС и канистры. |
| `garage.inc` | 598 | СТО (ремонт, покраска, тюнинг) и штрафстоянка. |
| `gates.inc` | 178 | шлагбаумы и ворота фракций. |
| `gps.inc` | 938 | правка маркеров в игре (/markers) и GPS для игроков (/gps). |
| `help.inc` | 803 | /help: разделы со всеми командами сервера. |
| `hospital.inc` | 231 | больница: платное лечение, страховка, медкарта. |
| `houses.inc` | 1215 | дома: покупка, продажа, вход/выход, замок, |
| `hud.inc` | 424 | свой HUD на экране: панель статуса и спидометр. |
| `interiors.inc` | 573 | свои интерьеры, собранные из объектов (маппинг). |
| `jobs.inc` | 999 | работы. |
| `keys.inc` | 375 | управление кнопками (как на популярных RP-серверах). |
| `kidnap.inc` | 641 | похищения: подвал банды для заложников. |
| `law.inc` | 328 | закон: метка задержания, дверь участка, взятка, побег. |
| `licenses.inc` | 552 | лицензии и автошкола. |
| `logs.inc` | 496 | общий журнал всех событий сервера. |
| `markers.inc` | 600 | все маркеры сервера (значок + надпись) в одном месте. |
| `marriage.inc` | 242 | свадьба, развод, общий дом и семейный счёт. |
| `menumem.inc` | 295 | меню помнят последний выбор. |
| `needs.inc` | 1659 | голод и инвентарь. |
| `orgs.inc` | 722 | панель владельца по всем организациям (/orgs) и спец-выговоры. |
| `phone.inc` | 208 | мобильный телефон. |
| `phoneapp.inc` | 654 | телефон с приложениями (HTML-окно "phone", ui.inc). |
| `places.inc` | 479 | точки новых систем, которые админ расставляет сам. |
| `platform.inc` | 89 | определение платформы игрока: ПК или телефон. |
| `prison.inc` | 384 | федеральная тюрьма и её охрана (организация типа «тюрьма»). |
| `property.inc` | 110 | сколько домов и транспорта может быть у игрока, |
| `quest.inc` | 506 | «Первые шаги»: цепочка заданий для новичка с наградами. |
| `referral.inc` | 136 | первые шаги новичка: правила сервера и «кто пригласил». |
| `rental.inc` | 279 | аренда мопедов. |
| `robbery.inc` | 137 | ограбление магазинов 24/7 и баров. |
| `season.inc` | 382 | сезонный пропуск: /season (/pass). |
| `skills.inc` | 422 | навыки оружия с прокачкой. |
| `social.inc` | 183 | связь с ботами ВКонтакте и Telegram (папка bots/). |
| `spawns.inc` | 646 | точки появления обычных игроков (вокзалы и т.п.). |
| `srvcfg.inc` | 322 | настройки сервера из игры: /srvset. |
| `textures.inc` | 676 | перекраска объектов маппинга (админ DEV_LEVEL+). |
| `turfs.inc` | 378 | районы банд и войны за них. |
| `ui.inc` | 84 | красивые HTML-окна в клиенте Bastion (пока Android). |
| `vehicles.inc` | 331 | транспорт. |
| `vip.inc` | 590 | VIP с уровнями: Bronze, Silver, Gold, Platinum, Diamond. |
| `worklife.inc` | 546 | новые работы: рыбак, фермер, инкассатор, мусорщик. |
| `wounded.inc` | 576 | ранение вместо мгновенной смерти. |

`rp.pwn`: 672 строки; модулей 80, всего ~46 200 строк.

## 5. Что осталось и что открыто

| Задача | Состояние / чего ждём |
|---|---|
| HD-скины и HD-машины | ждём от владельца `bastion_refs.zip` (референсы) |
| Вылет APK на realme | ждём `logcat` с телефона, потом `symbolize-crash.yml` |
| Вылет на ПК друга | ждём `crash.log` (папка GTA / SA-MP) |
| Новые системы (заводы, пожары, /srvset, /amenu и наказания, daily, season, войны семей, суд, аукцион) | в игре ещё **не проверялись** — первым делом прогнать на сервере и чинить по отзывам |
| Сайт с личным кабинетом | не начат (основа — `forum/`, читает `rp.db`) |

## 6. Где лежит остальное

- Бэкап и восстановление с нуля: `docs/BACKUP.md`.
- Инструкция по серверу и устройство кода: `server/README.md`. Ветки: `BRANCHES.md`. Текстуры: `TEXTURES.md`.
- База игроков `scriptfiles/rp.db` и `config.json` — только на ПК владельца, в репо их нет.
