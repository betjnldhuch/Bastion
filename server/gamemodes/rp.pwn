// =====================================================================
//  Bastion Online Ч мод сервера дл€ open.mp (GTA San Andreas Multiplayer)
//
//  Ёто главный файл. ќн подключает модули из папки bastion/ и раздаЄт им
//  событи€ сервера (callbacks). ¬с€ логика Ч в модул€х:
//
//    bastion/config.inc    Ч настройки (деньги, координаты, цвета)
//    bastion/base.inc Ч данные игрока и функции-помощники
//    bastion/database.inc  Ч база SQLite (scriptfiles/rp.db)
//    bastion/accounts.inc  Ч регистраци€ и вход
//    bastion/vehicles.inc  Ч транспорт, автосалон
//    bastion/houses.inc    Ч дома
//    bastion/jobs.inc      Ч работы (грузчик, пицца)
//    bastion/factions.inc  Ч фракции (полици€, больница, банды), тюрьма
//    bastion/turfs.inc     Ч районы банд и войны за них
//    bastion/bank.inc      Ч банк и банкоматы
//    bastion/phone.inc     Ч телефон: звонки, SMS, 911
//    bastion/business.inc  Ч бизнесы: 24/7, бар, одежда, оружие
//    bastion/gates.inc     Ч шлагбаумы и ворота фракций
//    bastion/keys.inc      Ч управление кнопками: ALT, Y, N, H, 2
//    bastion/chat.inc      Ч RP-чат
//    bastion/admin.inc     Ч админ-команды
//    bastion/commands.inc  Ч список команд
//
//   омпил€ци€: открой этот файл в Qawno и нажми F5.
//  ¬ј∆Ќќ: файлы сохранены в кодировке Windows-1251, иначе русский текст
//  в игре превратитс€ в "кракоз€бры".
// =====================================================================

#define MAX_PLAYERS 100          // слотов на сервере (должно совпадать с max_players в config.json или быть больше)

#include <open.mp>

#include "bastion/config.inc"
#include "bastion/base.inc"
#include "bastion/database.inc"
#include "bastion/accounts.inc"
#include "bastion/vehicles.inc"
#include "bastion/houses.inc"
#include "bastion/jobs.inc"
#include "bastion/chat.inc"
#include "bastion/admin.inc"
#include "bastion/factions.inc"   // после admin.inc: использует Admin_Check
#include "bastion/turfs.inc"
#include "bastion/bank.inc"
#include "bastion/phone.inc"
#include "bastion/business.inc"
#include "bastion/gates.inc"
#include "bastion/keys.inc"
#include "bastion/commands.inc"

new gLastPayDayHour = -1;

main()
{
	print("\n  -----------------------------");
	printf("  |  %s loaded  |", SERVER_NAME);
	print("  -----------------------------\n");
}

// =====================================================================
//  «апуск и остановка сервера
// =====================================================================
public OnGameModeInit()
{
	SetGameModeText(MODE_TEXT);
	DB_Init();

	ShowPlayerMarkers(PLAYER_MARKERS_MODE_STREAMED);
	DisableInteriorEnterExits();        // убираем жЄлтые маркеры входов из одиночной игры
	EnableStuntBonusForAll(false);
	UsePlayerPedAnims();

	// ќдна "заглушка"-класс: выбор скина не используетс€, спавн делаем сами
	AddPlayerClass(SKIN_MALE, SPAWN_X, SPAWN_Y, SPAWN_Z, SPAWN_A);

	Vehicles_Init();
	Jobs_Init();
	Houses_Load();
	Factions_Init();
	Turfs_Init();
	Bank_Init();
	Business_Init();
	Gates_Init();

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
	DB_Shutdown();
	return 1;
}

// =====================================================================
//  ¬ход / выход игрока
// =====================================================================
public OnPlayerConnect(playerid)
{
	if (!Account_OnConnect(playerid))
	{
		Player[playerid][pAuthShown] = true;    // игрока кикают Ч окно входа не показываем
		return 1;
	}
	SendClientMessage(playerid, COLOR_BLUE, "ƒобро пожаловать на %s!", SERVER_NAME);
	return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
	Job_Stop(playerid);
	Phone_HangUp(playerid);
	Account_Save(playerid);
	Vehicles_UnloadForPlayer(playerid);
	Player[playerid][pLogged] = false;
	return 1;
}

// ¬место выбора скина показываем окно входа/регистрации
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
	// Ѕез входа в аккаунт спавнитьс€ нельз€
	if (!Player[playerid][pLogged]) return 0;
	return 1;
}

// =====================================================================
//  —павн и смерть
// =====================================================================
public OnPlayerSpawn(playerid)
{
	if (!Player[playerid][pLogged])
	{
		Kick(playerid);
		return 1;
	}

	SetPlayerSkin(playerid, Player[playerid][pSkin]);
	SetPlayerInterior(playerid, 0);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerHealth(playerid, 100.0);
	SetCameraBehindPlayer(playerid);    // вернуть камеру за спину после окна входа
	Player[playerid][pHouseInside] = -1;

	if (Player[playerid][pDied])
	{
		Player[playerid][pDied] = false;
		GiveMoney(playerid, -HOSPITAL_FEE);
		SendClientMessage(playerid, COLOR_WHITE, "“еб€ вылечили в больнице. —чЄт за лечение: $%d.", HOSPITAL_FEE);
	}

	Account_SetNormalSpawn(playerid);   // следующий спавн Ч снова обычный
	Factions_OnSpawn(playerid);         // цвет ника, розыск, тюрьма
	Turfs_ShowForPlayer(playerid);      // районы банд на радаре
	return 1;
}

public OnPlayerDeath(playerid, killerid, WEAPON:reason)
{
	Job_Stop(playerid);
	Factions_OnDeath(playerid);
	Phone_HangUp(playerid);
	Turfs_OnDeath(playerid, killerid);

	Player[playerid][pDied] = true;
	SetSpawnInfo(playerid, NO_TEAM, Player[playerid][pSkin], HOSPITAL_X, HOSPITAL_Y, HOSPITAL_Z, 0.0);
	return 1;
}

// =====================================================================
//  „ат, команды, окна
// =====================================================================
public OnPlayerText(playerid, text[])
{
	if (!Player[playerid][pLogged]) return 0;
	Chat_OnText(playerid, text);
	return 0;   // 0 = не отправл€ть стандартный глобальный чат
}

public OnPlayerCommandText(playerid, cmdtext[])
{
	return Commands_Process(playerid, cmdtext);
}

public OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
	if (Account_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Vehicles_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Bank_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	if (Business_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (Keys_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;
	return 0;
}

// =====================================================================
//  ѕрочие событи€
// =====================================================================
public OnPlayerEnterCheckpoint(playerid)
{
	Jobs_OnEnterCheckpoint(playerid);
	return 1;
}

public OnPlayerStateChange(playerid, PLAYER_STATE:newstate, PLAYER_STATE:oldstate)
{
	if (newstate == PLAYER_STATE_DRIVER)
	{
		if (!Factions_OnBecomeDriver(playerid)) Vehicles_OnBecomeDriver(playerid);
	}
	return 1;
}

// Ќажати€ кнопок: ALT, Y, N, H, 2 (keys.inc)
public OnPlayerKeyStateChange(playerid, KEY:newkeys, KEY:oldkeys)
{
	Keys_OnKeyStateChange(playerid, _:newkeys, _:oldkeys);
	return 1;
}

public OnVehicleSpawn(vehicleid)
{
	Vehicles_OnSpawn(vehicleid);
	return 1;
}

// =====================================================================
//  “аймер раз в минуту: врем€ суток и зарплата (PayDay)
// =====================================================================
forward OnMinuteTimer();
public OnMinuteTimer()
{
	new hour, minute;
	gettime(hour, minute);
	SetWorldTime(hour);
	Factions_OnMinute();    // тюремные сроки

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
	return 1;
}

stock PayDay()
{
	Business_PayDay();      // доход в кассы бизнесов
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged]) continue;

		GiveMoney(i, PAYDAY_SALARY);
		Player[i][pExp]++;
		SendClientMessage(i, COLOR_YELLOW, "===== PayDay =====");
		SendClientMessage(i, COLOR_WHITE, "«арплата: $%d. ќпыт: %d/%d.", PAYDAY_SALARY,
			Player[i][pExp], Player[i][pLevel] * EXP_PER_LEVEL);

		new fpay = Factions_GetSalary(i);
		if (fpay > 0)
		{
			GiveMoney(i, fpay);
			SendClientMessage(i, COLOR_WHITE, "«арплата во фракции (%s): $%d.", Faction_GetRankName(Player[i][pFaction], Player[i][pRank]), fpay);
		}
		new tpay = Turfs_GetIncome(i);
		if (tpay > 0)
		{
			GiveMoney(i, tpay);
			SendClientMessage(i, COLOR_WHITE, "ƒоход банды с районов (%d): $%d.", tpay / TURF_INCOME, tpay);
		}

		if (Player[i][pExp] >= Player[i][pLevel] * EXP_PER_LEVEL)
		{
			Player[i][pExp] = 0;
			Player[i][pLevel]++;
			SetPlayerScore(i, Player[i][pLevel]);
			SendClientMessage(i, COLOR_GREEN, "ѕоздравл€ем! “вой уровень повышен до %d.", Player[i][pLevel]);
		}
		Account_Save(i);
	}
}
