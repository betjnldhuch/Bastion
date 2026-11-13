// =====================================================================
//  anticheat.inc — античит.
//
//  Сервер сам помнит, что он выдал игроку, и раз в секунду сверяет это
//  с тем, что присылает клиент:
//
//  - оружие и патроны: чего сервер не выдавал — отбирается, админам уходит
//    сообщение; AC_KICK_HITS срабатываний за AC_KICK_WINDOW секунд — кик;
//  - здоровье и броня: выросли сами — возвращаются назад, сообщение админам;
//  - скорость и телепорт: только сообщение админам (никого не кикает,
//    потому что лаги выглядят так же);
//  - деньги: на экране игрока всегда ровно столько, сколько знает сервер.
//
//  Админов (уровень 1+) античит не проверяет. У телефонов пороги скорости
//  выше (AC_MOBILE_FACTOR): у них чаще задержки.
//
//  ВАЖНО для тех, кто пишет код мода. Этот файл подменяет стандартные
//  функции (см. конец файла): GivePlayerWeapon, ResetPlayerWeapons,
//  SetPlayerHealth, SetPlayerArmour, SetPlayerPos, SetPlayerPosFindZ,
//  PutPlayerInVehicle, SetVehiclePos, SetPlayerInterior. Пиши их как обычно —
//  античит сам узнает, что это сделал сервер. Файл должен подключаться
//  в rp.pwn раньше остальных модулей.
// =====================================================================

#define AC_GRACE_MS         4000        // столько мс после действия сервера клиенту дают «догнать»
#define AC_FOOT_SPEED       35.0        // м/с пешком (по земле, без высоты)
#define AC_VEH_SPEED        110.0       // м/с в транспорте (около 400 км/ч)
#define AC_MOBILE_FACTOR    1.5         // во сколько раз мягче пороги скорости для телефонов
#define AC_REPORT_COOLDOWN  20          // секунд между сообщениями админам об одном игроке
#define AC_KICK_HITS        3           // столько раз чужое оружие...
#define AC_KICK_WINDOW      120         // ...за столько секунд — кик
#define AC_SET_WINDOW_MS    15000       // столько мс после лечения здоровье может «дорасти» до выданного

static WEAPON:gAcWeapon[MAX_PLAYERS][13];   // что сервер выдал в каждый слот
static gAcAmmo[MAX_PLAYERS][13];            // и сколько патронов там может быть
static gAcWeaponTick[MAX_PLAYERS];          // когда сервер последний раз менял оружие

static Float:gAcHealth[MAX_PLAYERS];        // последнее честное здоровье
static Float:gAcHealthSet[MAX_PLAYERS];     // что сервер выставил последним
static gAcHealthTick[MAX_PLAYERS];
static Float:gAcArmour[MAX_PLAYERS];
static Float:gAcArmourSet[MAX_PLAYERS];
static gAcArmourTick[MAX_PLAYERS];

static Float:gAcPos[MAX_PLAYERS][3];        // где игрок был на прошлой проверке
static gAcPosTick[MAX_PLAYERS];
static gAcPosInterior[MAX_PLAYERS];
static bool:gAcPosValid[MAX_PLAYERS];
static gAcMoveTick[MAX_PLAYERS];            // когда сервер сам перемещал игрока
static gAcUpdateTick[MAX_PLAYERS];          // когда от клиента последний раз пришли данные

static gAcReportAt[MAX_PLAYERS];
static gAcHits[MAX_PLAYERS];
static gAcHitsFrom[MAX_PLAYERS];
static bool:gAcKicked[MAX_PLAYERS];

forward AC_Tick();

static bool:AC_Valid(playerid)
{
	return (playerid >= 0 && playerid < MAX_PLAYERS);
}

// В каком слоте лежит оружие (-1 — такого оружия нет)
static AC_WeaponSlot(WEAPON:weaponid)
{
	switch (_:weaponid)
	{
		case 0, 1: return 0;
		case 2 .. 9: return 1;
		case 22 .. 24: return 2;
		case 25 .. 27: return 3;
		case 28, 29, 32: return 4;
		case 30, 31: return 5;
		case 33, 34: return 6;
		case 35 .. 38: return 7;
		case 16 .. 18, 39: return 8;
		case 41 .. 43: return 9;
		case 10 .. 15: return 10;
		case 44 .. 46: return 11;
		case 40: return 12;
	}
	return -1;
}

// Сервер только что сам переместил игрока — проверку скорости пропускаем
stock AC_Moved(playerid)
{
	if (AC_Valid(playerid)) gAcMoveTick[playerid] = GetTickCount();
}

// Запомнить выданное оружие (без самой выдачи — для оружия, которое даёт игра: дробовик в полицейской машине)
static AC_AllowWeapon(playerid, WEAPON:weaponid, ammo)
{
	new s = AC_WeaponSlot(weaponid);
	if (s == -1 || !AC_Valid(playerid)) return;
	gAcWeapon[playerid][s] = weaponid;
	if (ammo > 0) gAcAmmo[playerid][s] += ammo;     // в одном слоте патроны общие — складываем
	gAcWeaponTick[playerid] = GetTickCount();
}

static AC_ClearWeapons(playerid)
{
	for (new s = 0; s < 13; s++)
	{
		gAcWeapon[playerid][s] = WEAPON:0;
		gAcAmmo[playerid][s] = 0;
	}
	gAcWeaponTick[playerid] = GetTickCount();
}

// ---------------------------------------------------------------------
//  Замены стандартных функций (подключаются в конце файла)
// ---------------------------------------------------------------------
stock AC_GivePlayerWeapon(playerid, WEAPON:weaponid, ammo)
{
	AC_AllowWeapon(playerid, weaponid, ammo);
	return _:GivePlayerWeapon(playerid, weaponid, ammo);
}

stock AC_ResetPlayerWeapons(playerid)
{
	if (AC_Valid(playerid)) AC_ClearWeapons(playerid);
	return _:ResetPlayerWeapons(playerid);
}

stock AC_SetPlayerHealth(playerid, Float:health)
{
	if (AC_Valid(playerid))
	{
		gAcHealth[playerid] = health;
		gAcHealthSet[playerid] = health;
		gAcHealthTick[playerid] = GetTickCount();
	}
	return _:SetPlayerHealth(playerid, health);
}

stock AC_SetPlayerArmour(playerid, Float:armour)
{
	if (AC_Valid(playerid))
	{
		gAcArmour[playerid] = armour;
		gAcArmourSet[playerid] = armour;
		gAcArmourTick[playerid] = GetTickCount();
	}
	return _:SetPlayerArmour(playerid, armour);
}

stock AC_SetPlayerPos(playerid, Float:x, Float:y, Float:z)
{
	AC_Moved(playerid);
	return _:SetPlayerPos(playerid, x, y, z);
}

stock AC_SetPlayerPosFindZ(playerid, Float:x, Float:y, Float:z)
{
	AC_Moved(playerid);
	return _:SetPlayerPosFindZ(playerid, x, y, z);
}

stock AC_PutPlayerInVehicle(playerid, vehicleid, seatid)
{
	AC_Moved(playerid);
	return _:PutPlayerInVehicle(playerid, vehicleid, seatid);
}

stock AC_SetPlayerInterior(playerid, interiorid)
{
	AC_Moved(playerid);
	return _:SetPlayerInterior(playerid, interiorid);
}

stock AC_SetVehiclePos(vehicleid, Float:x, Float:y, Float:z)
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && IsPlayerInVehicle(i, vehicleid)) AC_Moved(i);
	}
	return _:SetVehiclePos(vehicleid, x, y, z);
}

// ---------------------------------------------------------------------
//  События (вызываются из rp.pwn)
// ---------------------------------------------------------------------
stock AC_Init()
{
	SetTimer("AC_Tick", 1000, true);
}

stock AC_OnConnect(playerid)
{
	new now = GetTickCount();
	AC_ClearWeapons(playerid);
	gAcHealth[playerid] = 100.0;
	gAcHealthSet[playerid] = 100.0;
	gAcHealthTick[playerid] = now;
	gAcArmour[playerid] = 0.0;
	gAcArmourSet[playerid] = 0.0;
	gAcArmourTick[playerid] = now;
	gAcPosValid[playerid] = false;
	gAcMoveTick[playerid] = now;
	gAcUpdateTick[playerid] = now;
	gAcReportAt[playerid] = 0;
	gAcHits[playerid] = 0;
	gAcHitsFrom[playerid] = 0;
	gAcKicked[playerid] = false;
}

// Спавн: игра сама даёт 100 здоровья и убирает оружие и броню.
// Вызывать ПЕРВЫМ в OnPlayerSpawn — модули после него выдают своё уже через замены.
stock AC_OnSpawn(playerid)
{
	new now = GetTickCount();
	AC_ClearWeapons(playerid);
	gAcHealth[playerid] = 100.0;
	gAcHealthSet[playerid] = 100.0;
	gAcHealthTick[playerid] = now;
	gAcArmour[playerid] = 0.0;
	gAcArmourSet[playerid] = 0.0;
	gAcArmourTick[playerid] = now;
	gAcPosValid[playerid] = false;
	gAcMoveTick[playerid] = now;
}

stock AC_OnDeath(playerid)
{
	AC_ClearWeapons(playerid);
	gAcArmour[playerid] = 0.0;
	gAcArmourSet[playerid] = 0.0;
	gAcPosValid[playerid] = false;
	AC_Moved(playerid);
}

// Из OnPlayerUpdate: только отметка времени
stock AC_OnUpdate(playerid)
{
	gAcUpdateTick[playerid] = GetTickCount();
}

// Сел за руль: некоторые машины сами что-то дают водителю — это не чит
stock AC_OnStateChange(playerid, PLAYER_STATE:newstate)
{
	AC_Moved(playerid);
	if (newstate != PLAYER_STATE_DRIVER && newstate != PLAYER_STATE_PASSENGER) return;
	new now = GetTickCount();
	switch (GetVehicleModel(GetPlayerVehicleID(playerid)))
	{
		case 596, 597, 598, 599: AC_AllowWeapon(playerid, WEAPON:25, 5);    // полицейская машина: дробовик
		case 457: AC_AllowWeapon(playerid, WEAPON:2, 1);                    // гольф-кар: клюшка
		case 416:                                                           // скорая: +20 здоровья
		{
			gAcHealthSet[playerid] = 100.0;
			gAcHealthTick[playerid] = now;
		}
		case 427:                                                           // Enforcer: броня
		{
			gAcArmourSet[playerid] = 100.0;
			gAcArmourTick[playerid] = now;
		}
	}
}

// ---------------------------------------------------------------------
//  Сообщения и наказания
// ---------------------------------------------------------------------
static AC_Report(playerid, const what[])
{
	new now = gettime();
	if (now - gAcReportAt[playerid] < AC_REPORT_COOLDOWN) return;
	gAcReportAt[playerid] = now;

	new msg[144];
	format(msg, sizeof msg, "[Античит] %s[%d]%s: %s", GetRPName(playerid), playerid, IsPlayerMobile[playerid] ? (" (телефон)") : (""), what);
	SendAdminMessage(COLOR_RED, msg);
	printf("%s", msg);
}

// Чужое оружие: отбираем, считаем, на AC_KICK_HITS-й раз кикаем
static AC_WeaponHit(playerid, const what[])
{
	new now = gettime();
	if (now - gAcHitsFrom[playerid] > AC_KICK_WINDOW)
	{
		gAcHitsFrom[playerid] = now;
		gAcHits[playerid] = 0;
	}
	gAcHits[playerid]++;
	gAcReportAt[playerid] = 0;      // об оружии сообщаем всегда
	AC_Report(playerid, what);

	// Вернуть игроку ровно то, что выдавал сервер
	ResetPlayerWeapons(playerid);
	for (new s = 0; s < 13; s++)
	{
		if (gAcWeapon[playerid][s] == WEAPON:0) continue;
		if (s >= 2 && s <= 9 && gAcAmmo[playerid][s] <= 0)
		{
			gAcWeapon[playerid][s] = WEAPON:0;
			continue;
		}
		GivePlayerWeapon(playerid, gAcWeapon[playerid][s], (gAcAmmo[playerid][s] > 0) ? gAcAmmo[playerid][s] : 1);
	}
	gAcWeaponTick[playerid] = GetTickCount();

	if (gAcHits[playerid] >= AC_KICK_HITS && !gAcKicked[playerid])
	{
		gAcKicked[playerid] = true;
		Admin_Log(INVALID_PLAYER_ID, "anticheat kick", playerid, what);
		SendClientMessage(playerid, COLOR_RED, "Ты кикнут античитом (оружие). Если это ошибка — напиши на форум.");
		KickWithDelay(playerid);
	}
}

// ---------------------------------------------------------------------
//  Проверки, раз в секунду
// ---------------------------------------------------------------------
static AC_CheckWeapons(playerid, now)
{
	if (now - gAcWeaponTick[playerid] < AC_GRACE_MS) return;

	new WEAPON:weapon, ammo, name[32], what[96];
	for (new s = 0; s < 13; s++)
	{
		weapon = WEAPON:0;
		ammo = 0;
		GetPlayerWeaponData(playerid, WEAPON_SLOT:s, weapon, ammo);
		if (weapon == WEAPON:0) continue;
		if (s >= 2 && s <= 9 && ammo <= 0) continue;                    // пустой ствол — не оружие
		if (weapon == WEAPON:46) continue;                              // парашют игра выдаёт сама
		if (weapon == WEAPON:40 && gAcWeapon[playerid][8] == WEAPON:39) continue;   // детонатор идёт с взрывчаткой

		if (weapon != gAcWeapon[playerid][s])
		{
			GetWeaponName(weapon, name, sizeof name);
			format(what, sizeof what, "оружие, которого сервер не выдавал: %s (%d патр.)", name, ammo);
			AC_WeaponHit(playerid, what);
			return;
		}
		if (s >= 2 && s <= 9)
		{
			if (ammo > gAcAmmo[playerid][s])
			{
				GetWeaponName(weapon, name, sizeof name);
				format(what, sizeof what, "лишние патроны: %s, %d вместо %d", name, ammo, gAcAmmo[playerid][s]);
				AC_WeaponHit(playerid, what);
				return;
			}
			gAcAmmo[playerid][s] = ammo;    // патроны только тратятся
		}
	}
}

static AC_CheckHealth(playerid, now)
{
	new Float:value, what[96];

	GetPlayerHealth(playerid, value);
	if (value <= gAcHealth[playerid] + 1.0) gAcHealth[playerid] = value;
	else if (now - gAcHealthTick[playerid] < AC_SET_WINDOW_MS && value <= gAcHealthSet[playerid] + 1.0) gAcHealth[playerid] = value;
	else
	{
		format(what, sizeof what, "здоровье выросло само: %.0f вместо %.0f (возвращено)", value, gAcHealth[playerid]);
		AC_Report(playerid, what);
		SetPlayerHealth(playerid, gAcHealth[playerid]);
	}

	GetPlayerArmour(playerid, value);
	if (value <= gAcArmour[playerid] + 1.0) gAcArmour[playerid] = value;
	else if (now - gAcArmourTick[playerid] < AC_SET_WINDOW_MS && value <= gAcArmourSet[playerid] + 1.0) gAcArmour[playerid] = value;
	else
	{
		format(what, sizeof what, "броня появилась сама: %.0f вместо %.0f (возвращена)", value, gAcArmour[playerid]);
		AC_Report(playerid, what);
		SetPlayerArmour(playerid, gAcArmour[playerid]);
	}
}

static AC_CheckMove(playerid, now, PLAYER_STATE:st)
{
	new Float:x, Float:y, Float:z, interior = GetPlayerInterior(playerid);
	GetPlayerPos(playerid, x, y, z);

	new elapsed = now - gAcPosTick[playerid];
	if (gAcPosValid[playerid] && interior == gAcPosInterior[playerid]
		&& now - gAcMoveTick[playerid] >= AC_GRACE_MS      // сервер его не перемещал
		&& now - gAcUpdateTick[playerid] < 1500             // клиент на связи, не свёрнут
		&& elapsed >= 500 && elapsed <= 2500)
	{
		new Float:dx = x - gAcPos[playerid][0], Float:dy = y - gAcPos[playerid][1];
		new Float:speed = floatsqroot(dx * dx + dy * dy) * 1000.0 / float(elapsed);
		new bool:riding = (st != PLAYER_STATE_ONFOOT || GetPlayerSurfingVehicleID(playerid) != INVALID_VEHICLE_ID);
		new Float:limit = riding ? AC_VEH_SPEED : AC_FOOT_SPEED;
		if (IsPlayerMobile[playerid]) limit *= AC_MOBILE_FACTOR;
		if (speed > limit)
		{
			new what[96];
			format(what, sizeof what, "%s: %.0f м за %.1f с (%s)", (speed > limit * 3.0) ? ("телепорт") : ("скорость"),
				floatsqroot(dx * dx + dy * dy), float(elapsed) / 1000.0, riding ? ("в транспорте") : ("пешком"));
			AC_Report(playerid, what);
		}
	}
	gAcPos[playerid][0] = x;
	gAcPos[playerid][1] = y;
	gAcPos[playerid][2] = z;
	gAcPosTick[playerid] = now;
	gAcPosInterior[playerid] = interior;
	gAcPosValid[playerid] = true;
}

public AC_Tick()
{
	new now = GetTickCount();
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || gAcKicked[i]) continue;

		// Деньги: на экране ровно столько, сколько знает сервер
		if (GetPlayerMoney(i) != Player[i][pMoney])
		{
			ResetPlayerMoney(i);
			GivePlayerMoney(i, Player[i][pMoney]);
		}

		new PLAYER_STATE:st = GetPlayerState(i);
		if (st != PLAYER_STATE_ONFOOT && st != PLAYER_STATE_DRIVER && st != PLAYER_STATE_PASSENGER)
		{
			gAcPosValid[i] = false;
			continue;
		}
		if (Player[i][pAdmin] > 0)      // админов не проверяем, только запоминаем их здоровье
		{
			GetPlayerHealth(i, gAcHealth[i]);
			GetPlayerArmour(i, gAcArmour[i]);
			gAcPosValid[i] = false;
			continue;
		}
		AC_CheckWeapons(i, now);
		if (gAcKicked[i]) continue;
		AC_CheckHealth(i, now);
		AC_CheckMove(i, now, st);
	}
	return 1;
}

// ---------------------------------------------------------------------
//  Подмена стандартных функций. Всё, что ниже по rp.pwn, вызывает уже наши.
// ---------------------------------------------------------------------
#define GivePlayerWeapon        AC_GivePlayerWeapon
#define ResetPlayerWeapons      AC_ResetPlayerWeapons
#define SetPlayerHealth         AC_SetPlayerHealth
#define SetPlayerArmour         AC_SetPlayerArmour
#define SetPlayerPosFindZ       AC_SetPlayerPosFindZ
#define SetPlayerPos            AC_SetPlayerPos
#define PutPlayerInVehicle      AC_PutPlayerInVehicle
#define SetPlayerInterior       AC_SetPlayerInterior
#define SetVehiclePos           AC_SetVehiclePos
