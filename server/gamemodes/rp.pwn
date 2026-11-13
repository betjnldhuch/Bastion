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

#define MAX_PLAYERS 100          // слотов на сервере (должно совпадать с max_players в config.json или быть больше)

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
#include "bastion/markers.inc"    // все маркеры (значки и надписи) в одном месте
#include "bastion/spawns.inc"     // точки появления игроков (вокзалы): /addspawn
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
#include "bastion/backup.inc"      // копии базы раз в сутки, /backup
#include "bastion/dev.inc"         // разработка в игре: /pos, /dev, маппинг /mo
#include "bastion/textures.inc"    // перекраска объектов маппинга: /mt
#include "bastion/doors.inc"       // входы в здания: LSPD
#include "bastion/factions.inc"   // после admin.inc: использует Admin_Check
#include "bastion/law.inc"        // метка задержания, дверь участка, взятка
#include "bastion/turfs.inc"
#include "bastion/bank.inc"
#include "bastion/phone.inc"
#include "bastion/business.inc"
#include "bastion/garage.inc"
#include "bastion/gps.inc"        // /markers — правка маркеров в игре, /gps
#include "bastion/debts.inc"      // долги штату: налоги и штрафы отдельными счетами
#include "bastion/economy.inc"
#include "bastion/families.inc"   // семьи: организации игроков
#include "bastion/wounded.inc"    // ранение вместо смерти, /revive, /aid
#include "bastion/robbery.inc"    // /rob — ограбление магазинов
#include "bastion/gates.inc"
#include "bastion/keys.inc"
#include "bastion/fps.inc"
#include "bastion/hud.inc"        // свой HUD: панель статуса и спидометр
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
	Markers_Init();       // таблица markers: правки маркеров из игры
	Admin_Init();         // таблицы admin_log, reports, сроки банов

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
	Gates_Init();
	Fps_Init();
	Licenses_Init();
	Needs_Init();
	Dev_Init();           // свои объекты карты, убранные здания
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
	AC_Init();            // античит (anticheat.inc)
	Backup_Init();        // копия базы, если сегодняшней ещё нет

	new hour;
	gettime(hour);
	SetWorldTime(hour);

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
	Families_OnConnect(playerid);
	Dev_OnConnect(playerid);            // убранные здания GTA (dev.inc)
	Tex_OnConnect(playerid);
	Gps_OnConnect(playerid);            // значки на карте, сброс GPS
	if (!Account_OnConnect(playerid))
	{
		Player[playerid][pAuthShown] = true;    // игрока кикают — окно входа не показываем
		return 1;
	}
	SendClientMessage(playerid, COLOR_BLUE, "Добро пожаловать на %s!", SERVER_NAME);
	return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
	Law_OnDisconnect(playerid, reason); // метка задержания: вышел из игры — деморган (law.inc)
	Platform_OnDisconnect(playerid);
	UI_OnDisconnect(playerid);
	Supply_ResetPlayer(playerid);
	Admin_OnDisconnect(playerid);
	Job_Stop(playerid);
	Jobs_OnDisconnect(playerid);
	Licenses_Abort(playerid);
	Phone_HangUp(playerid);
	Account_Save(playerid);
	Vehicles_UnloadForPlayer(playerid);
	Fuel_OnDisconnect(playerid);
	Inv_ResetMask(playerid);
	Inv_UiReset(playerid);
	Hud_OnDisconnect(playerid);
	Gps_OnDisconnect(playerid);
	Wound_Reset(playerid);
	Rob_OnDisconnect(playerid);
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
		GiveMoney(playerid, -HOSPITAL_FEE, "счёт больницы");
		Econ_ToTreasury(HOSPITAL_FEE);
		SendClientMessage(playerid, COLOR_WHITE, "Тебя вылечили в больнице. Счёт за лечение: $%d.", HOSPITAL_FEE);
	}

	Account_SetNormalSpawn(playerid);   // следующий спавн — снова обычный
	Houses_OnSpawn(playerid);           // дом в общежитии: вернуть интерьер
	Factions_OnSpawn(playerid);         // цвет ника, розыск, тюрьма
	Admin_OnSpawn(playerid);            // деморган, возврат после /spec
	Turfs_ShowForPlayer(playerid);      // районы банд на радаре
	Wound_OnSpawn(playerid);            // после смерти — лежит раненым на месте
	Hud_OnSpawn(playerid);              // панель статуса (hud.inc)
	return 1;
}

// Урон: при малом здоровье игрок падает раненым (wounded.inc)
public OnPlayerTakeDamage(playerid, issuerid, Float:amount, WEAPON:weaponid, bodypart)
{
	new bool:was = Wound_IsWounded(playerid);
	Wound_OnDamage(playerid, amount);
	// Упал раненым от чужой пули — в войне за район это считается убийством (turfs.inc)
	if (!was && Wound_IsWounded(playerid)) Turfs_OnDeath(playerid, issuerid);
	return 1;
}

public OnPlayerDeath(playerid, killerid, WEAPON:reason)
{
	AC_OnDeath(playerid);
	Job_Stop(playerid);
	Licenses_Abort(playerid);
	Factions_OnDeath(playerid);
	Phone_HangUp(playerid);
	Inv_ResetMask(playerid);            // маска слетает (needs.inc)
	if (!Wound_IsWounded(playerid)) Turfs_OnDeath(playerid, killerid);   // раненого засчитали, когда он упал

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
	// Игроки не могут красить текст кодами {RRGGBB}: так подделывают системные сообщения
	if (Player[playerid][pAdmin] < 1) StripColorCodes(text);
	if (text[0] == '\0') return 0;
	Chat_OnText(playerid, text);
	return 0;   // 0 = не отправлять стандартный глобальный чат
}

public OnPlayerCommandText(playerid, cmdtext[])
{
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
	if (Account_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Spawns_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Admin_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Vehicles_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Bank_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Business_OnDialog(playerid, dialogid, response, listitem)) return 1;
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
	if (Factions_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	return 0;
}

// =====================================================================
//  Прочие события
// =====================================================================
public OnPlayerEnterCheckpoint(playerid)
{
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
		if (!Factions_OnBecomeDriver(playerid)) Vehicles_OnBecomeDriver(playerid);
		Jobs_OnBecomeDriver(playerid);      // рабочие машины — только работникам
		Licenses_OnBecomeDriver(playerid);  // учебная машина, предупреждение без прав
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
	Fuel_OnVehicleSpawn(vehicleid);
	Vehicles_OnSpawn(vehicleid);
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
	Needs_OnMinute();       // голод
	Factions_OnMinute();    // тюремные сроки
	Admin_OnMinute();       // деморган и муты

	if (minute == 0 && hour != gLastPayDayHour)
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
		Bank_PayDay(i);         // проценты по счёту и баланс (bank.inc)

		if (Player[i][pExp] >= Player[i][pLevel] * EXP_PER_LEVEL)
		{
			Player[i][pExp] = 0;
			Player[i][pLevel]++;
			SetPlayerScore(i, Player[i][pLevel]);
			SendClientMessage(i, COLOR_GREEN, "Поздравляем! Твой уровень повышен до %d.", Player[i][pLevel]);
		}
		Account_Save(i);
	}
}
