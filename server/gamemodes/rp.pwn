// =====================================================================
//  Bastion Online — мод сервера для open.mp (GTA San Andreas Multiplayer)
//
//  Это главный файл. Он подключает модули из папки bastion/ и раздаёт им
//  события сервера (callbacks). Вся логика — в модулях:
//
//    bastion/config.inc    — настройки (деньги, координаты, цвета)
//    bastion/base.inc — данные игрока и функции-помощники
//    bastion/database.inc  — база SQLite (scriptfiles/rp.db)
//    bastion/accounts.inc  — регистрация и вход
//    bastion/vehicles.inc  — транспорт, автосалон
//    bastion/houses.inc    — дома
//    bastion/jobs.inc      — работы (грузчик, пицца)
//    bastion/factions.inc  — фракции (полиция, больница, банды), тюрьма
//    bastion/turfs.inc     — районы банд и войны за них
//    bastion/bank.inc      — банк и банкоматы
//    bastion/phone.inc     — телефон: звонки, SMS, 911
//    bastion/business.inc  — бизнесы: 24/7, бар, одежда, оружие
//    bastion/garage.inc    — СТО (ремонт, тюнинг) и штрафстоянка
//    bastion/economy.inc   — казна, налоги, журнал денег
//    bastion/gates.inc     — шлагбаумы и ворота фракций
//    bastion/keys.inc      — управление кнопками: ALT, Y, N, H, 2
//    bastion/chat.inc      — RP-чат
//    bastion/admin.inc     — администрация: наказания, логи, репорты
//    bastion/admin_tools.inc — телепорты (метка на карте, /tp) и инструменты админов
//    bastion/dev.inc       — разработка в игре: координаты /pos, маппинг /mo
//    bastion/textures.inc  — перекраска объектов маппинга: текстуры, цвет, надписи /mt
//    bastion/doors.inc     — входы в здания со стандартными интерьерами (LSPD)
//    bastion/dialogs.inc   — номера всех окон
//    bastion/anticheat.inc — античит: оружие, здоровье, скорость, деньги
//    bastion/backup.inc    — резервные копии базы
//    bastion/commands.inc  — список команд
//
//  Компиляция: открой этот файл в Qawno и нажми F5.
//  ВАЖНО: файлы сохранены в кодировке Windows-1251, иначе русский текст
//  в игре превратится в "кракозябры".
// =====================================================================

#define MAX_PLAYERS 1000         // слотов на сервере (max_players в config.json должно быть не больше; 1000 — предел open.mp)

// Память под локальные переменные (в ячейках по 4 байта). Стандартных 16 КБ мало: окна
// /amenu, /reports, /moneylog и списки фракций собирают текст по 3-4 тысячи символов
// и без этой строки обрываются с ошибкой "stack/heap collision".
#pragma dynamic 65536

#include <open.mp>

#include "bastion/config.inc"
#include "bastion/dialogs.inc"    // номера всех окон в одном месте
#include "bastion/base.inc"
#include "bastion/ui.inc"         // HTML-окна в клиенте Bastion: UI_Show, UI_Has
#include "bastion/platform.inc"   // ПК или телефон: IsPlayerMobile[playerid]
#include "bastion/anticheat.inc"  // античит; подменяет SetPlayerPos, GivePlayerWeapon и др., поэтому стоит до модулей
#include "bastion/database.inc"
#include "bastion/srvcfg.inc"     // /srvset: настройки сервера из игры
#include "bastion/logs.inc"       // общий журнал всех событий: scriptfiles/logs.db, /logs
#include "bastion/social.inc"     // боты ВК и Telegram: /bot, уведомления (папка bots/)
#include "bastion/markers.inc"    // все маркеры (значки и надписи) в одном месте
#include "bastion/spawns.inc"     // точки появления игроков (вокзалы): /addspawn
#include "bastion/referral.inc"   // правила для новичков и «кто пригласил»
#include "bastion/accounts.inc"
#include "bastion/vehicles.inc"
#include "bastion/fuel.inc"
#include "bastion/licenses.inc"
#include "bastion/needs.inc"
#include "bastion/houses.inc"
#include "bastion/jobs.inc"
#include "bastion/chat.inc"
#include "bastion/admin.inc"
#include "bastion/admin_tools.inc"
#include "bastion/admin_auth.inc"   // /alogin — админ-пароль, /alogout, /apass, /astats, /alist
#include "bastion/backup.inc"      // копии базы раз в сутки, /backup
#include "bastion/dev.inc"         // разработка в игре: /pos, /dev, маппинг /mo
#include "bastion/textures.inc"    // перекраска объектов маппинга: /mt
#include "bastion/interiors.inc"  // interiors built from objects: showroom
#include "bastion/doors.inc"       // входы в здания: LSPD
#include "bastion/places.inc"     // точки новых систем, которые ставит админ: /place
#include "bastion/factions.inc"   // после admin.inc: использует Admin_Check
#include "bastion/fmanage.inc"    // «Управление» фракцией: права рангов и личные, карточки, журнал
#include "bastion/law.inc"        // метка задержания, дверь участка, взятка
#include "bastion/turfs.inc"
#include "bastion/bank.inc"
#include "bastion/phone.inc"
#include "bastion/fbi.inc"        // ФБР: маскировка, прослушка, слежка, досье
#include "bastion/business.inc"
#include "bastion/bizstat.inc"    // финансовая выписка бизнеса, /showbiz, /checkbiz
#include "bastion/bizwork.inc"    // сотрудники бизнеса, налог, улучшения, /sell
#include "bastion/bizplus.inc"    // новые виды бизнесов: склад, наценка, меню владельца
#include "bastion/garage.inc"
#include "bastion/dealers.inc"    // автосалоны и авиасалон, /cars — несколько машин у игрока
#include "bastion/property.inc"   // лимиты домов и транспорта (в том числе по VIP), /myhouses
#include "bastion/gps.inc"        // /markers — правка маркеров в игре, /gps
#include "bastion/debts.inc"      // долги штату: налоги и штрафы отдельными счетами
#include "bastion/economy.inc"
#include "bastion/families.inc"   // семьи: организации игроков
#include "bastion/orgs.inc"       // /orgs — панель владельца по всем организациям, спец-выговоры
#include "bastion/wounded.inc"    // ранение вместо смерти, /revive, /aid
#include "bastion/admin_give.inc" // админ: /givegun /aheal /setarmour /disarm /healall /takemoney
#include "bastion/kidnap.inc"     // подвал банды: /tie, /hold, выкуп
#include "bastion/robbery.inc"    // /rob — ограбление магазинов
#include "bastion/drugs.inc"      // наркотики: кусты, торговец, эффекты, ломка
#include "bastion/worklife.inc"   // работы: рыбак, фермер, инкассатор, мусорщик
#include "bastion/casino.inc"     // казино и лотерея
#include "bastion/prison.inc"     // федеральная тюрьма и её охрана
#include "bastion/hospital.inc"   // больница: лечение, страховка, медкарта
#include "bastion/marriage.inc"   // свадьба, общий дом и семейный счёт
#include "bastion/donate.inc"     // донат-меню: монеты, VIP
#include "bastion/quest.inc"      // квест новичка «Первые шаги»: /quest
#include "bastion/daily.inc"      // ежедневные задания: /daily
#include "bastion/season.inc"     // сезонный пропуск: /season
#include "bastion/famwar.inc"     // войны семей по расписанию: /famwar
#include "bastion/court.inc"      // суд и адвокаты: /lawyer, /free, /appeal, /cases
#include "bastion/auction.inc"    // аукцион домов: /auction, /bid
#include "bastion/rental.inc"     // аренда мопедов: /addmoped, ALT у точки
#include "bastion/gates.inc"
#include "bastion/keys.inc"
#include "bastion/fps.inc"
#include "bastion/hud.inc"        // свой HUD: панель статуса и спидометр
#include "bastion/docs.inc"       // документы в сумке: паспорт, медкарта, лицензии
#include "bastion/phoneapp.inc"   // телефон с приложениями: /phone, N
#include "bastion/skills.inc"     // навыки оружия с прокачкой: /skills
#include "bastion/help.inc"       // /help: разделы со всеми командами
#include "bastion/commands.inc"

new gLastPayDayHour = -1;

main()
{
	print("\n  -----------------------------");
	printf("  |  %s loaded  |", SERVER_NAME);
	print("  -----------------------------\n");
}

// =====================================================================
//  Запуск и остановка сервера
// =====================================================================
public OnGameModeInit()
{
	SetGameModeText(MODE_TEXT);
	DB_Init();
	Log_Init();           // журнал событий (logs.db)
	Factions_RunMoves();  // перенос фракций на другой номер (один раз, до загрузки всего)
	Markers_Init();       // таблица markers: правки маркеров из игры
	Admin_Init();         // таблицы admin_log, reports, сроки банов
	Cfg_Init();           // настройки /srvset (srvcfg.inc)

	ShowPlayerMarkers(PLAYER_MARKERS_MODE_STREAMED);
	DisableInteriorEnterExits();        // убираем жёлтые маркеры входов из одиночной игры
	EnableStuntBonusForAll(false);
	UsePlayerPedAnims();

	// Одна "заглушка"-класс: выбор скина не используется, спавн делаем сами
	AddPlayerClass(SKIN_MALE, SPAWN_X, SPAWN_Y, SPAWN_Z, SPAWN_A);

	Vehicles_Init();
	Jobs_Init();
	Spawns_Init();        // точки появления игроков (вокзалы)
	Houses_Load();
	Factions_Init();
	Turfs_Init();
	Bank_Init();
	Business_Init();
	BizX_Init();
	Gates_Init();
	Fps_Init();
	Licenses_Init();
	Docs_Init();           // паспортный стол в мэрии (docs.inc)
	Skills_Init();         // навыки оружия в accounts (skills.inc)
	Needs_Init();
	Dev_Init();           // свои объекты карты, убранные здания
	Interiors_Init();
	Doors_Init();         // входы в здания (маркеры LSPD)
	Econ_Init();          // казна, налоги, журнал денег
	Debts_Init();         // счета долгов в accounts
	Law_Init();           // решётка камеры LSPD (law.inc)
	Families_Init();      // семьи игроков
	Garage_Init();        // СТО, штрафстоянка, поля тюнинга в vehicles
	Gps_Init();
	Hud_Init();           // панель статуса и спидометр (hud.inc)
	Fuel_Init();          // после всех машин: им выдаётся бензин
	Wound_Init();
	Robbery_Init();
	Kidnap_Init();        // подвалы банд (после Factions_Init)
	Places_Init();        // точки новых систем (places.inc)
	Drugs_Init();
	WorkLife_Init();      // после Places_Init: машины работ стоят в точках
	Casino_Init();
	Prison_Init();
	Hospital_Init();
	Marriage_Init();
	Donate_Init();
	Quest_Init();          // поля квеста новичка в accounts (quest.inc)
	Daily_Init();          // поля ежедневных заданий в accounts (daily.inc)
	Season_Init();         // сезонный пропуск (season.inc)
	FamWar_Init();         // войны семей (famwar.inc), после Families_Init
	Court_Init();          // суд и адвокаты (court.inc)
	Auction_Init();        // аукцион домов (auction.inc), после Houses_Load
	PhoneApp_Init();       // SMS, контакты, звонки (phoneapp.inc)
	Rental_Init();         // точки проката мопедов (rental.inc)
	AC_Init();            // античит (anticheat.inc)
	Social_Init();        // таблицы для ботов ВК/Telegram (social.inc)
	Backup_Init();        // копия базы, если сегодняшней ещё нет

	new hour, minute;
	gettime(hour, minute);
	SetWorldTime(hour);
	// PayDay — при смене часа. Запуск посреди часа не должен сразу давать зарплату.
	if (minute > 0) gLastPayDayHour = hour;

	SetTimer("OnMinuteTimer", 60000, true);
	return 1;
}

public OnGameModeExit()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i)) Account_Save(i);
	}
	Fuel_Exit();
	Log_Exit();
	DB_Shutdown();
	return 1;
}

// =====================================================================
//  Вход / выход игрока
// =====================================================================
public OnPlayerConnect(playerid)
{
	Fps_Reset(playerid);
	Platform_OnConnect(playerid);
	UI_OnConnect(playerid);
	AC_OnConnect(playerid);
	Admin_OnConnect(playerid);
	Law_OnConnect(playerid);
	Debts_OnConnect(playerid);
	BizW_OnConnect(playerid);
	Families_OnConnect(playerid);
	Markers_OnConnect(playerid);        // свои надписи фракционных точек
	Dev_OnConnect(playerid);            // убранные здания GTA (dev.inc)
	Tex_OnConnect(playerid);
	Gps_OnConnect(playerid);            // значки на карте, сброс GPS
	if (!Account_OnConnect(playerid))
	{
		Player[playerid][pAuthShown] = true;    // игрока кикают — окно входа не показываем
		return 1;
	}
	Log_OnConnect(playerid);
	Chat_FloodReset(playerid);
	SendClientMessage(playerid, COLOR_BLUE, "Добро пожаловать на %s!", SERVER_NAME);
	return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
	Log_OnDisconnect(playerid, reason);
	Law_OnDisconnect(playerid, reason); // метка задержания: вышел из игры — деморган (law.inc)
	Prison_OnDisconnect(playerid);      // до сохранения: сорванный побег добавляет срок
	Drugs_OnDisconnect(playerid);
	WorkLife_OnDisconnect(playerid);
	Casino_OnDisconnect(playerid);
	Platform_OnDisconnect(playerid);
	UI_OnDisconnect(playerid);
	Supply_ResetPlayer(playerid);
	FPerm_OnDisconnect(playerid);
	Admin_OnDisconnect(playerid);
	Job_Stop(playerid);
	Jobs_OnDisconnect(playerid);
	Daily_OnDisconnect(playerid);
	Season_OnDisconnect(playerid);
	Court_OnDisconnect(playerid);
	Licenses_Abort(playerid);
	Docs_OnDisconnect(playerid);
	Phone_HangUp(playerid);
	PhoneApp_OnDisconnect(playerid);
	Skills_OnDisconnect(playerid);      // навыки оружия (skills.inc)
	Account_Save(playerid);
	Vehicles_UnloadForPlayer(playerid);
	Fuel_OnDisconnect(playerid);
	Inv_ResetMask(playerid);
	Inv_UiReset(playerid);
	Hud_OnDisconnect(playerid);
	Gps_OnDisconnect(playerid);
	Wound_Reset(playerid);
	Fbi_OnDisconnect(playerid);         // ФБР: маскировка, прослушка, слежка
	Rob_OnDisconnect(playerid);
	Kidnap_OnDisconnect(playerid);
	Rental_OnDisconnect(playerid);
	Player[playerid][pLogged] = false;
	return 1;
}

// Много раз в секунду: тут только замер FPS (fps.inc)
public OnPlayerUpdate(playerid)
{
	Fps_OnUpdate(playerid);
	AC_OnUpdate(playerid);              // античит: отметка «клиент на связи»
	AdminTools_OnUpdate(playerid);      // античит: джетпак
	return 1;
}

// Метка на карте: админа телепортирует туда (admin_tools.inc)
public OnPlayerClickMap(playerid, Float:fX, Float:fY, Float:fZ)
{
	AdminTools_OnClickMap(playerid, fX, fY, fZ);
	return 1;
}

// Игрок появился рядом с другим: если он в маске — ник не показываем (needs.inc)
public OnPlayerStreamIn(playerid, forplayerid)
{
	Inv_OnStreamIn(playerid, forplayerid);
	return 1;
}

// Маппинг: объект подвинули мышью / выбрали кликом (dev.inc)
public OnPlayerEditObject(playerid, playerobject, objectid, EDIT_RESPONSE:response, Float:fX, Float:fY, Float:fZ, Float:fRotX, Float:fRotY, Float:fRotZ)
{
	if (!playerobject) Dev_OnEditObject(playerid, objectid, response, fX, fY, fZ, fRotX, fRotY, fRotZ);
	return 1;
}

public OnPlayerSelectObject(playerid, SELECT_OBJECT:type, objectid, modelid, Float:fX, Float:fY, Float:fZ)
{
	Dev_OnSelectObject(playerid, type, objectid);
	return 1;
}

// Вместо выбора скина показываем окно входа/регистрации
public OnPlayerRequestClass(playerid, classid)
{
	if (!Player[playerid][pLogged] && !Player[playerid][pAuthShown])
	{
		Player[playerid][pAuthShown] = true;
		TogglePlayerSpectating(playerid, true);
		Account_ShowLoginCamera(playerid);
		Account_ShowAuth(playerid);
	}
	return 1;
}

public OnPlayerRequestSpawn(playerid)
{
	// Без входа в аккаунт спавниться нельзя
	if (!Player[playerid][pLogged]) return 0;
	return 1;
}

// =====================================================================
//  Спавн и смерть
// =====================================================================
public OnPlayerSpawn(playerid)
{
	if (!Player[playerid][pLogged])
	{
		Kick(playerid);
		return 1;
	}

	AC_OnSpawn(playerid);               // античит: первым, до выдачи здоровья и оружия
	SetPlayerSkin(playerid, Player[playerid][pSkin]);
	SetPlayerInterior(playerid, 0);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerHealth(playerid, 100.0);
	SetCameraBehindPlayer(playerid);    // вернуть камеру за спину после окна входа
	Player[playerid][pHouseInside] = -1;

	if (Player[playerid][pDied])
	{
		Player[playerid][pDied] = false;
		Needs_OnHospital(playerid);         // в больнице покормили (needs.inc)
		Hospital_Bill(playerid);            // счёт больницы, со страховкой — бесплатно (hospital.inc)
	}

	Account_SetNormalSpawn(playerid);   // следующий спавн — снова обычный
	Houses_OnSpawn(playerid);           // дом в общежитии: вернуть интерьер
	Factions_OnSpawn(playerid);         // цвет ника, розыск, тюрьма
	Admin_OnSpawn(playerid);            // деморган, возврат после /spec
	Turfs_ShowForPlayer(playerid);      // районы банд на радаре
	Wound_OnSpawn(playerid);            // после смерти — лежит раненым на месте
	Hud_OnSpawn(playerid);              // панель статуса (hud.inc)
	BizX_OnSpawn(playerid);             // стиль боя из спортзала (bizplus.inc)
	Skills_OnSpawn(playerid);           // навыки оружия (skills.inc)
	Kidnap_OnSpawn(playerid);           // последним: заложник возвращается в подвал
	return 1;
}

// Выстрел: попадания качают навык оружия (skills.inc)
public OnPlayerWeaponShot(playerid, WEAPON:weaponid, BULLET_HIT_TYPE:hittype, hitid, Float:fX, Float:fY, Float:fZ)
{
	Skills_OnShot(playerid, weaponid, hittype, hitid);
	return 1;
}

// Урон: при малом здоровье игрок падает раненым (wounded.inc)
public OnPlayerTakeDamage(playerid, issuerid, Float:amount, WEAPON:weaponid, bodypart)
{
	new bool:was = Wound_IsWounded(playerid);
	Wound_OnDamage(playerid, amount);
	Log_OnDamage(playerid, issuerid, amount, weaponid, bodypart, !was && Wound_IsWounded(playerid));
	// Упал раненым от чужой пули — в войне за район это считается убийством (turfs.inc)
	if (!was && Wound_IsWounded(playerid)) { Turfs_OnDeath(playerid, issuerid); FamWar_OnKill(playerid, issuerid); }
	return 1;
}

public OnPlayerDeath(playerid, killerid, WEAPON:reason)
{
	Log_OnDeath(playerid, killerid, reason);
	AC_OnDeath(playerid);
	Job_Stop(playerid);
	Licenses_Abort(playerid);
	Factions_OnDeath(playerid);
	Phone_HangUp(playerid);
	Inv_ResetMask(playerid);            // маска слетает (needs.inc)
	Drugs_OnDeath(playerid);            // эффект наркотиков проходит
	if (!Wound_IsWounded(playerid)) { Turfs_OnDeath(playerid, killerid); FamWar_OnKill(playerid, killerid); }   // раненого засчитали, когда он упал

	if (Wound_OnDeath(playerid)) return 1;  // появится раненым на месте (wounded.inc)

	Player[playerid][pDied] = true;
	SetSpawnInfo(playerid, NO_TEAM, Player[playerid][pSkin], HOSPITAL_X, HOSPITAL_Y, HOSPITAL_Z, 0.0);
	return 1;
}

// =====================================================================
//  Чат, команды, окна
// =====================================================================
public OnPlayerText(playerid, text[])
{
	if (!Player[playerid][pLogged]) return 0;
	if (Admin_IsMuted(playerid)) return 0;
	if (Chat_IsFlood(playerid, false)) return 0;   // антифлуд (chat.inc)
	// Игроки не могут красить текст кодами {RRGGBB}: так подделывают системные сообщения
	if (Player[playerid][pAdmin] < 1) StripColorCodes(text);
	if (text[0] == '\0') return 0;
	// «.ыефеы» / «.stats» — команда, набранная в русской раскладке (chat.inc).
	// Неизвестная команда уходит в чат как обычный текст.
	new fixed[145];
	if (text[0] == '.' && Chat_CmdFromLayout(text, fixed))
	{
		if (Commands_Process(playerid, fixed, true)) { Log_OnCommand(playerid, fixed); return 0; }
	}
	Log_OnText(playerid, text);
	Chat_OnText(playerid, text);
	return 0;   // 0 = не отправлять стандартный глобальный чат
}

public OnPlayerCommandText(playerid, cmdtext[])
{
	if (!strcmp(cmdtext, "/__bui")) return UI_OnPcHello(playerid);
	if (!strcmp(cmdtext, "/__bui ", false, 7)) return UI_OnPcHello(playerid, cmdtext[7]);   // "/__bui 2" — версия плагина   // ПК-плагин HTML-окон (ui.inc)
	if (Player[playerid][pLogged])
	{
		if (Chat_IsFlood(playerid, true)) return 1;   // антифлуд (chat.inc)
		Log_OnCommand(playerid, cmdtext);
	}
	// «/ыефеы» — имя команды в русской раскладке -> «/stats» (chat.inc)
	new fixed[145];
	if (Chat_CmdFromLayout(cmdtext, fixed)) return Commands_Process(playerid, fixed);
	return Commands_Process(playerid, cmdtext);
}

// Клик по текстдраву игрока (окно сумки)
public OnPlayerClickPlayerTextDraw(playerid, PlayerText:playertextid)
{
	if (Inv_OnClickPlayerTD(playerid, playertextid)) return 1;
	return 0;
}

// Клик по общему текстдраву; INVALID_TEXT_DRAW — игрок нажал ESC
public OnPlayerClickTextDraw(playerid, Text:clickedid)
{
	if (Inv_OnClickTD(playerid, clickedid)) return 1;
	return 0;
}

public OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
	if (Hud_IsDialog(dialogid)) return 1;   // скрытая панель (hud.inc)
	if (Account_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Spawns_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Admin_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Vehicles_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Props_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Bank_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Business_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (BizX_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Market_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (BizW_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Keys_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Fuel_OnDialog(playerid, dialogid, response, inputtext)) return 1;
	if (Licenses_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Jobs_OnDialog(playerid, dialogid, response)) return 1;
	if (Needs_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Dev_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Tex_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Houses_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Garage_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Help_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Wound_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Markers_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (FColor_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (BM_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Factory_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Families_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (FrMenu_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Orgs_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (SrvCfg_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Factions_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Drugs_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Casino_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Hospital_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Donate_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Rental_OnDialog(playerid, dialogid, response)) return 1;
	if (Quest_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Daily_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Season_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Court_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Auction_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (PhoneApp_OnDialog(playerid, dialogid, response, inputtext)) return 1;
	if (Docs_OnDialog(playerid, dialogid, response)) return 1;
	return 0;
}

// =====================================================================
//  Прочие события
// =====================================================================
public OnPlayerEnterCheckpoint(playerid)
{
	if (Prison_OnEnterCheckpoint(playerid)) return 1;   // работа заключённых (prison.inc)
	Jobs_OnEnterCheckpoint(playerid);
	return 1;
}

public OnPlayerEnterRaceCheckpoint(playerid)
{
	Licenses_OnEnterRaceCheckpoint(playerid);   // экзамен в автошколе
	return 1;
}

public OnPlayerStateChange(playerid, PLAYER_STATE:newstate, PLAYER_STATE:oldstate)
{
	AC_OnStateChange(playerid, newstate);
	if (newstate == PLAYER_STATE_DRIVER)
	{
		Log_OnDriver(playerid);
		if (!Factions_OnBecomeDriver(playerid)) Vehicles_OnBecomeDriver(playerid);
		Jobs_OnBecomeDriver(playerid);      // рабочие машины — только работникам
		Licenses_OnBecomeDriver(playerid);  // учебная машина, предупреждение без прав
		Rental_OnBecomeDriver(playerid);    // чужой прокатный мопед
	}
	else if (newstate == PLAYER_STATE_PASSENGER)
	{
		Jobs_OnBecomePassenger(playerid);   // оплата такси
	}
	return 1;
}

// Нажатия кнопок: ALT, Y, N, H, 2 (keys.inc)
public OnPlayerKeyStateChange(playerid, KEY:newkeys, KEY:oldkeys)
{
	if (Admin_OnKey(playerid, _:newkeys, _:oldkeys)) return 1;   // слежка: ЛКМ/ПКМ — следующий/предыдущий (admin.inc)
	Keys_OnKeyStateChange(playerid, _:newkeys, _:oldkeys);
	return 1;
}

public OnVehicleSpawn(vehicleid)
{
	if (Rental_OnVehicleSpawn(vehicleid)) return 1;   // разбитый прокатный мопед пропадает
	Fuel_OnVehicleSpawn(vehicleid);
	Vehicles_OnSpawn(vehicleid);
	WorkLife_OnVehicleSpawn(vehicleid); // инкассаторская машина вернулась — денег в ней нет
	return 1;
}

// =====================================================================
//  Таймер раз в минуту: время суток и зарплата (PayDay)
// =====================================================================
forward OnMinuteTimer();
public OnMinuteTimer()
{
	new hour, minute;
	gettime(hour, minute);
	SetWorldTime(hour);
	// Все записи этой минуты (автосохранение, PayDay, журналы) идут в базу одной пачкой:
	// диск трогаем один раз, и сервер не подвисает, когда игроков много
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "BEGIN"));
	FamWar_OnMinute();      // войны семей по расписанию (famwar.inc)
	Auction_OnMinute();     // аукцион домов (auction.inc)
	Needs_OnMinute();       // голод
	Factions_OnMinute();    // тюремные сроки
	Admin_OnMinute();       // деморган и муты
	Drugs_OnMinute();       // кусты растут, ломка
	Casino_OnMinute();      // розыгрыш лотереи
	Rental_OnMinute();      // срок аренды мопедов
	Skills_OnMinute();      // навыки оружия: лимит за минуту, сохранение

	// По смене часа, а не по minute == 0: таймер со временем сдвигается и может пропустить нулевую минуту
	if (hour != gLastPayDayHour)
	{
		gLastPayDayHour = hour;
		PayDay();
	}
	else if (minute % 10 == 0)
	{
		// автосохранение каждые 10 минут
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (IsPlayerConnected(i)) Account_Save(i);
		}
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "COMMIT"));
	Backup_Daily();         // копия базы раз в сутки (после COMMIT: внутри транзакции копию не снять)
	return 1;
}

stock PayDay()
{
	Season_HourCheck();     // сезон кончился — новый (season.inc)
	Business_PayDay();      // доход в кассы бизнесов
	Houses_PayDay();        // аренда комнат и налог на дома
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged]) continue;

		Bank_Salary(i, PAYDAY_SALARY);
		Player[i][pExp]++;
		SendClientMessage(i, COLOR_YELLOW, "===== PayDay =====");
		SendClientMessage(i, COLOR_WHITE, "Зарплата: $%d. Опыт: %d/%d.", PAYDAY_SALARY,
			Player[i][pExp], Player[i][pLevel] * EXP_PER_LEVEL);

		new fpay = Factions_GetSalary(i);
		if (fpay > 0)
		{
			Bank_Salary(i, fpay);
			SendClientMessage(i, COLOR_WHITE, "Зарплата во фракции (%s): $%d.", Faction_GetRankName(Player[i][pFaction], Player[i][pRank]), fpay);
		}
		new tpay = Turfs_GetIncome(i);
		if (tpay > 0)
		{
			Bank_Salary(i, tpay);
			SendClientMessage(i, COLOR_WHITE, "Доход банды с районов (%d): $%d.", tpay / TURF_INCOME, tpay);
		}

		Econ_PayDay(i, PAYDAY_SALARY + fpay);   // налоги (economy.inc)
		Debts_PayDay(i);                        // напоминание о долгах, розыск должников (debts.inc)
		Donate_PayDay(i);       // VIP: прибавка и двойной опыт (donate.inc)
		Bank_PayDay(i);         // проценты по счёту и баланс (bank.inc)
		Quest_OnPayDay(i);      // квест новичка: первая зарплата, +опыт (quest.inc)
		Daily_HourCheck(i);     // новый день — новые ежедневные задания (daily.inc)
		Daily_OnPayDay(i);
		Season_OnPayDay(i);     // очки сезонного пропуска (season.inc)

		if (Player[i][pExp] >= Player[i][pLevel] * EXP_PER_LEVEL)
		{
			Player[i][pExp] = 0;
			Player[i][pLevel]++;
			SetPlayerScore(i, Player[i][pLevel]);
			SendClientMessage(i, COLOR_GREEN, "Поздравляем! Твой уровень повышен до %d.", Player[i][pLevel]);
			Referral_OnLevelUp(i);   // награда пригласившему (referral.inc)
		}
		Account_Save(i);
	}
}
