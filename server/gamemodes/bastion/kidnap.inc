// =====================================================================
//  kidnap.inc — похищения: подвал банды для заложников.
//
//  У каждой банды есть свой подвал (вход виден только своим). Порядок:
//    1. /tie [id]   — связать человека. Он должен быть ранен, стоять с
//                     поднятыми руками (/handsup) или быть у тебя на
//                     прицеле. Похитителю — розыск KIDNAP_WANTED звёзд.
//    2. /lead [id]  — вести связанного за собой (ещё раз — отпустить).
//       /gcar [id]  — посадить связанного в свою машину (сидя за рулём)
//                     или высадить его.
//    3. /hold [id]  — у входа в подвал: запереть связанного в подвале.
//    4. /ransom [id] [сумма] — назначить выкуп. Платит кто угодно:
//       /payransom [id] — деньги идут в общак банды, заложник на свободе.
//       /release [id]   — отпустить (бандиты или полиция на штурме).
//  Вход/выход в подвал — ALT у двери или /hideout. Полиция на смене
//  может войти на штурм, если стоит у двери. Лидер банды (ранг 5+)
//  переносит вход: /sethideout. Через KIDNAP_HOLD_MAX заложника
//  отпускают сами. Вышел из игры в подвале — при входе вернётся туда.
// =====================================================================

#define KIDNAP_INT          5           // интерьер «наркопритон»
#define KIDNAP_IX           318.56
#define KIDNAP_IY           1118.21
#define KIDNAP_IZ           1083.88
#define KIDNAP_VW_BASE      7700        // мир подвала = 7700 + номер банды
#define KIDNAP_MIN_LEVEL    2           // и похитителю, и жертве
#define KIDNAP_WANTED       3
#define KIDNAP_HOLD_MAX     1800        // сек. максимум в подвале
#define KIDNAP_IMMUNE       1800        // сек. нельзя похитить снова
#define KIDNAP_MAX_HELD     3           // заложников в одном подвале
#define KIDNAP_RANSOM_MAX   100000
#define KIDNAP_RESCUE_PRIZE 500         // полиции за освобождение
#define KIDNAP_DOOR_RANGE   3.0
#define KIDNAP_SAVED        32          // помнить вышедших заложников

enum
{
	KS_NONE,
	KS_TIED,        // связан (pCuffed)
	KS_HELD         // заперт в подвале
}

forward Kidnap_OnSecond();
forward Kidnap_OnLead();
forward bool:Cmd_Hideout(playerid);

static gKidState[MAX_PLAYERS];
static gKidFac[MAX_PLAYERS];                                // какая банда держит
static gKidLeft[MAX_PLAYERS];                               // секунд до освобождения
static gKidRansom[MAX_PLAYERS];
static gKidLeader[MAX_PLAYERS] = { INVALID_PLAYER_ID, ... };   // кто ведёт жертву
static gKidImmune[MAX_PLAYERS];                             // до какого gettime() нельзя похищать
static gKidGrace[MAX_PLAYERS];                              // сек. после спавна без проверки мира
static gHideOut[MAX_FACTIONS] = { -1, ... };                // маркер входа
static gHideIn[MAX_FACTIONS] = { -1, ... };                 // маркер выхода внутри

// Заложники, вышедшие из игры (до перезапуска сервера)
static gKidSaveDb[KIDNAP_SAVED];
static gKidSaveFac[KIDNAP_SAVED];
static gKidSaveLeft[KIDNAP_SAVED];
static gKidSaveRansom[KIDNAP_SAVED];

stock Kidnap_Init()
{
	for (new f = 1; f < MAX_FACTIONS; f++) if (Faction_IsGang(f)) Kidnap_Ensure(f);
	SetTimer("Kidnap_OnSecond", 1000, true);
	SetTimer("Kidnap_OnLead", 400, true);
}

// Маркеры подвала банды (создаются при первом обращении — и для новых банд)
static Kidnap_Ensure(f)
{
	if (!Faction_IsGang(f)) return 0;
	if (gHideOut[f] == -1)
	{
		new Float:x = Faction[f][fSpawnX] + 3.0, Float:y = Faction[f][fSpawnY], Float:z = Faction[f][fSpawnZ];
		gHideOut[f] = Marker_Create(MK_HIDEOUT, f * 2, "Подвал банды", 1318, x, y, z, Faction[f][fSpawnWorld], Faction[f][fSpawnInt]);
		if (gHideOut[f] != -1)
		{
			Marker_SetLabel(gHideOut[f], "{C0A060}Подвал банды\n{FFFFFF}ALT — войти", COLOR_WHITE, 10.0);
			Marker_SetFaction(gHideOut[f], f);
		}
	}
	if (gHideIn[f] == -1)
	{
		new Float:x = KIDNAP_IX, Float:y = KIDNAP_IY, Float:z = KIDNAP_IZ;
		gHideIn[f] = Marker_Create(MK_HIDEOUT, f * 2 + 1, "Подвал: выход", 1318, x, y, z, KIDNAP_VW_BASE + f, KIDNAP_INT);
		if (gHideIn[f] != -1) Marker_SetLabel(gHideIn[f], "{FFFFFF}Выход\nALT", COLOR_WHITE, 8.0);
	}
	return (gHideOut[f] != -1 && gHideIn[f] != -1);
}

static bool:Kidnap_AtMarker(playerid, m, Float:range)
{
	if (!Marker_IsValid(m)) return false;
	if (GetPlayerVirtualWorld(playerid) != Marker_GetWorld(m) || GetPlayerInterior(playerid) != Marker_GetInterior(m)) return false;
	new Float:x, Float:y, Float:z;
	Marker_GetPos(m, x, y, z);
	return IsPlayerInRangeOfPoint(playerid, range, x, y, z);
}

// У входа какой банды стоит игрок (-1 — ни у какой). Чужой вход виден только полиции на смене.
static Kidnap_DoorAt(playerid)
{
	new f = Player[playerid][pFaction];
	new bool:cop = Faction_IsLaw(f) && Player[playerid][pDuty];
	for (new g = 1; g < MAX_FACTIONS; g++)
	{
		if (g != f && !cop && Player[playerid][pAdmin] < 4) continue;
		if (!Kidnap_Ensure(g)) continue;
		if (Kidnap_AtMarker(playerid, gHideOut[g], KIDNAP_DOOR_RANGE)) return g;
	}
	return -1;
}

// В подвале какой банды игрок (-1 — не в подвале)
static Kidnap_InsideOf(playerid)
{
	if (GetPlayerInterior(playerid) != KIDNAP_INT) return -1;
	new g = GetPlayerVirtualWorld(playerid) - KIDNAP_VW_BASE;
	return (g > 0 && g < MAX_FACTIONS) ? g : -1;
}

static Kidnap_PutInside(playerid, f)
{
	new Float:x, Float:y, Float:z;
	Marker_GetPos(gHideIn[f], x, y, z);
	if (IsPlayerInAnyVehicle(playerid)) RemovePlayerFromVehicle(playerid, true);
	SetPlayerInterior(playerid, Marker_GetInterior(gHideIn[f]));
	SetPlayerVirtualWorld(playerid, Marker_GetWorld(gHideIn[f]));
	SetPlayerPos(playerid, x, y + 1.0, z);
	SetCameraBehindPlayer(playerid);
}

static Kidnap_PutOutside(playerid, f)
{
	new Float:x, Float:y, Float:z;
	Marker_GetPos(gHideOut[f], x, y, z);
	SetPlayerInterior(playerid, Marker_GetInterior(gHideOut[f]));
	SetPlayerVirtualWorld(playerid, Marker_GetWorld(gHideOut[f]));
	SetPlayerPos(playerid, x + 1.0, y, z);
	SetCameraBehindPlayer(playerid);
}

static Kidnap_HeldCount(f)
{
	new n = 0;
	for (new i = 0; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && gKidState[i] == KS_HELD && gKidFac[i] == f) n++;
	return n;
}

static Kidnap_AddWanted(playerid, stars)
{
	new w = Player[playerid][pWanted] + stars;
	if (w > 6) w = 6;
	Player[playerid][pWanted] = w;
	SetPlayerWantedLevel(playerid, w);
	Account_Save(playerid);
}

static Kidnap_StopLead(victim)
{
	gKidLeader[victim] = INVALID_PLAYER_ID;
}

// Снять все состояния без сообщений
static Kidnap_Clear(playerid)
{
	gKidState[playerid] = KS_NONE;
	gKidFac[playerid] = 0;
	gKidLeft[playerid] = 0;
	gKidRansom[playerid] = 0;
	Kidnap_StopLead(playerid);
	for (new i = 0; i < MAX_PLAYERS; i++) if (gKidLeader[i] == playerid) gKidLeader[i] = INVALID_PLAYER_ID;
}

// Отпустить заложника из подвала на улицу
static Kidnap_Free(victim, const why[])
{
	new f = gKidFac[victim];
	new bool:held = (gKidState[victim] == KS_HELD);
	Kidnap_Clear(victim);
	gKidImmune[victim] = gettime() + KIDNAP_IMMUNE;
	if (Player[victim][pCuffed]) Faction_Uncuff(victim);
	if (held && Kidnap_Ensure(f) && Kidnap_InsideOf(victim) == f) Kidnap_PutOutside(victim, f);
	TogglePlayerControllable(victim, true);
	SendClientMessage(victim, COLOR_GREEN, "Ты на свободе: %s.", why);
	if (Faction_IsValid(f))
	{
		new msg[144];
		format(msg, sizeof msg, "[Подвал] %s на свободе: %s.", GetRPName(victim), why);
		Faction_Message(f, 0xC0A060FF, msg);
	}
}

// ---------------------------------------------------------------------
//  Связать / развязать
// ---------------------------------------------------------------------
stock bool:Cmd_Tie(playerid, const params[])
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsGang(f)) return SendClientMessage(playerid, COLOR_GREY, "Связывать людей могут только банды.");
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /tie [id]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл.");
	if (!IsPlayerNearPlayer(playerid, target, 2.5)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе.");
	if (IsPlayerInAnyVehicle(target)) return SendClientMessage(playerid, COLOR_GREY, "Сначала вытащи человека из машины.");
	if (Player[target][pFaction] == f) return SendClientMessage(playerid, COLOR_GREY, "Своих не похищают.");
	if (Player[target][pCuffed] || gKidState[target] != KS_NONE) return SendClientMessage(playerid, COLOR_GREY, "Он уже связан или в наручниках.");
	if (Player[target][pJail] > 0) return SendClientMessage(playerid, COLOR_GREY, "Он сидит в тюрьме.");
	if (Player[playerid][pLevel] < KIDNAP_MIN_LEVEL || Player[target][pLevel] < KIDNAP_MIN_LEVEL)
		return SendClientMessage(playerid, COLOR_GREY, "Похищать можно с %d уровня, и жертва тоже должна быть не ниже %d уровня.", KIDNAP_MIN_LEVEL, KIDNAP_MIN_LEVEL);
	new now = gettime();
	if (gKidImmune[target] > now) return SendClientMessage(playerid, COLOR_GREY, "Этого человека недавно отпустили. Подожди %d мин.", (gKidImmune[target] - now) / 60 + 1);

	new w = _:GetPlayerWeapon(playerid);
	new bool:aimed = (GetPlayerTargetPlayer(playerid) == target && w >= 22 && w <= 34);
	new bool:hands = (GetPlayerSpecialAction(target) == SPECIAL_ACTION_HANDSUP);
	if (!Wound_IsWounded(target) && !aimed && !hands)
		return SendClientMessage(playerid, COLOR_GREY, "Он сопротивляется. Нужно: он ранен, поднял руки (/handsup) или у тебя на прицеле.");

	Player[target][pCuffed] = true;
	gKidState[target] = KS_TIED;
	gKidFac[target] = f;
	if (!Wound_IsWounded(target))
	{
		TogglePlayerControllable(target, false);
		SetPlayerSpecialAction(target, SPECIAL_ACTION_CUFFED);
	}
	new msg[144];
	format(msg, sizeof msg, "%s связывает руки %s верёвкой.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(target, COLOR_RED, "Тебя связали. Можешь звать на помощь в чат: развязать может любой рядом (/untie).");
	SendClientMessage(playerid, COLOR_WHITE, "Дальше: /lead %d — вести, /gcar %d — в машину, /hold %d — у входа в подвал.", target, target, target);
	if (!Faction_IsLaw(Player[playerid][pFaction])) Kidnap_AddWanted(playerid, KIDNAP_WANTED);
	return true;
}

stock bool:Cmd_Untie(playerid, const params[])
{
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /untie [id]");
	if (gKidState[target] != KS_TIED) return SendClientMessage(playerid, COLOR_GREY, "Он не связан.");
	if (!IsPlayerNearPlayer(playerid, target, 2.5)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе.");
	new msg[144];
	format(msg, sizeof msg, "%s развязывает %s.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	new f = gKidFac[target];
	Kidnap_Clear(target);
	if (Player[playerid][pFaction] != f) gKidImmune[target] = gettime() + KIDNAP_IMMUNE;
	if (IsPlayerInAnyVehicle(target)) RemovePlayerFromVehicle(target, true);
	Faction_Uncuff(target);
	TogglePlayerControllable(target, true);
	return true;
}

stock bool:Cmd_HandsUp(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT || Player[playerid][pCuffed] || Wound_IsWounded(playerid)) return true;
	if (GetPlayerSpecialAction(playerid) == SPECIAL_ACTION_HANDSUP)
	{
		SetPlayerSpecialAction(playerid, SPECIAL_ACTION_NONE);
		return true;
	}
	SetPlayerSpecialAction(playerid, SPECIAL_ACTION_HANDSUP);
	new msg[96];
	format(msg, sizeof msg, "%s поднимает руки.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

// ---------------------------------------------------------------------
//  Вести / в машину
// ---------------------------------------------------------------------
stock bool:Cmd_Lead(playerid, const params[])
{
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /lead [id]");
	if (gKidState[target] != KS_TIED || gKidFac[target] != Player[playerid][pFaction])
		return SendClientMessage(playerid, COLOR_GREY, "Вести можно только связанного вашей бандой.");
	if (gKidLeader[target] == playerid)
	{
		Kidnap_StopLead(target);
		return SendClientMessage(playerid, COLOR_WHITE, "Ты отпустил %s.", GetRPName(target));
	}
	if (!IsPlayerNearPlayer(playerid, target, 3.0)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе.");
	if (IsPlayerInAnyVehicle(target)) RemovePlayerFromVehicle(target, true);
	gKidLeader[target] = playerid;
	new msg[144];
	format(msg, sizeof msg, "%s хватает %s за верёвку и ведёт за собой.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_GCar(playerid, const params[])
{
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /gcar [id] (сидя за рулём)");
	if (gKidState[target] != KS_TIED || gKidFac[target] != Player[playerid][pFaction])
		return SendClientMessage(playerid, COLOR_GREY, "Сажать можно только связанного вашей бандой.");
	new v = GetPlayerVehicleID(playerid);
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return SendClientMessage(playerid, COLOR_GREY, "Сядь за руль.");
	new msg[144];
	if (GetPlayerVehicleID(target) == v)
	{
		RemovePlayerFromVehicle(target, true);
		format(msg, sizeof msg, "%s вытаскивает %s из машины.", GetRPName(playerid), GetRPName(target));
		SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
		return true;
	}
	if (!IsPlayerNearPlayer(playerid, target, 6.0)) return SendClientMessage(playerid, COLOR_GREY, "Связанный слишком далеко от машины.");
	new seat = -1;
	for (new s = 3; s >= 1 && seat == -1; s--)
	{
		new bool:busy = false;
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (IsPlayerConnected(i) && GetPlayerVehicleID(i) == v && GetPlayerVehicleSeat(i) == s) { busy = true; break; }
		}
		if (!busy) seat = s;
	}
	if (seat == -1) return SendClientMessage(playerid, COLOR_GREY, "В машине нет свободных мест.");
	Kidnap_StopLead(target);
	PutPlayerInVehicle(target, v, seat);
	TogglePlayerControllable(target, false);
	format(msg, sizeof msg, "%s заталкивает %s в машину.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

public Kidnap_OnLead()
{
	new Float:x, Float:y, Float:z, Float:a, Float:d;
	for (new v = 0; v < MAX_PLAYERS; v++)
	{
		new l = gKidLeader[v];
		if (l == INVALID_PLAYER_ID) continue;
		if (!IsPlayerConnected(v) || !IsPlayerConnected(l) || gKidState[v] != KS_TIED || Wound_IsWounded(l) || Player[l][pCuffed])
		{
			gKidLeader[v] = INVALID_PLAYER_ID;
			continue;
		}
		if (IsPlayerInAnyVehicle(v) || IsPlayerInAnyVehicle(l)) continue;
		GetPlayerPos(l, x, y, z);
		d = GetPlayerDistanceFromPoint(v, x, y, z);
		if (GetPlayerVirtualWorld(v) != GetPlayerVirtualWorld(l) || GetPlayerInterior(v) != GetPlayerInterior(l) || d > 25.0)
		{
			gKidLeader[v] = INVALID_PLAYER_ID;
			SendClientMessage(l, COLOR_GREY, "Ты упустил %s.", GetRPName(v));
			continue;
		}
		if (d < 2.0) continue;
		GetPlayerFacingAngle(l, a);
		SetPlayerPos(v, x - 1.2 * floatsin(-a, degrees), y - 1.2 * floatcos(-a, degrees), z);
		SetPlayerFacingAngle(v, a);
	}
	return 1;
}

// ---------------------------------------------------------------------
//  Подвал: запереть, вход/выход, выкуп, отпустить
// ---------------------------------------------------------------------
stock bool:Cmd_Hold(playerid, const params[])
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsGang(f)) return SendClientMessage(playerid, COLOR_GREY, "Подвал есть только у банд.");
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /hold [id] (у входа в подвал)");
	if (gKidState[target] != KS_TIED || gKidFac[target] != f) return SendClientMessage(playerid, COLOR_GREY, "Это не ваш связанный.");
	if (!Kidnap_Ensure(f) || !Kidnap_AtMarker(playerid, gHideOut[f], 6.0)) return SendClientMessage(playerid, COLOR_GREY, "Встань у входа в подвал своей банды.");
	if (!IsPlayerNearPlayer(playerid, target, 8.0)) return SendClientMessage(playerid, COLOR_GREY, "Связанный слишком далеко.");
	if (Kidnap_HeldCount(f) >= KIDNAP_MAX_HELD) return SendClientMessage(playerid, COLOR_GREY, "В подвале уже %d заложника, больше не влезет.", KIDNAP_MAX_HELD);

	Kidnap_StopLead(target);
	Faction_Uncuff(target);
	gKidState[target] = KS_HELD;
	gKidFac[target] = f;
	gKidLeft[target] = KIDNAP_HOLD_MAX;
	gKidRansom[target] = 0;
	Kidnap_PutInside(target, f);
	TogglePlayerControllable(target, true);

	new msg[144];
	format(msg, sizeof msg, "%s заталкивает %s в подвал.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(target, COLOR_RED, "Тебя заперли в подвале банды «%s». Выйти нельзя.", Faction[f][fName]);
	SendClientMessage(target, COLOR_WHITE, "Освободят: выкуп (/payransom %d), сами бандиты, штурм полиции или через %d мин.", target, KIDNAP_HOLD_MAX / 60);
	format(msg, sizeof msg, "[Подвал] %s запер в подвале %s [%d]. Назначить выкуп: /ransom %d [сумма].", GetRPName(playerid), GetRPName(target), target, target);
	Faction_Message(f, 0xC0A060FF, msg);
	format(msg, sizeof msg, "[Диспетчер] Заявление о похищении: пропал %s [%d]. Подозревается банда «%s».", GetRPName(target), target, Faction[f][fName]);
	Faction_MessageLaw(msg);
	return true;
}

// ALT у двери подвала (из keys.inc)
stock bool:Kidnap_TryAlt(playerid)
{
	new f = Kidnap_InsideOf(playerid);
	if (f != -1 && Kidnap_Ensure(f) && Kidnap_AtMarker(playerid, gHideIn[f], KIDNAP_DOOR_RANGE))
	{
		Cmd_Hideout(playerid);
		return true;
	}
	if (Kidnap_DoorAt(playerid) != -1)
	{
		Cmd_Hideout(playerid);
		return true;
	}
	return false;
}

stock bool:Cmd_Hideout(playerid)
{
	new f = Kidnap_InsideOf(playerid);
	if (f != -1 && Kidnap_Ensure(f))
	{
		if (!Kidnap_AtMarker(playerid, gHideIn[f], KIDNAP_DOOR_RANGE)) return SendClientMessage(playerid, COLOR_GREY, "Подойди к двери.");
		if (gKidState[playerid] == KS_HELD) return SendClientMessage(playerid, COLOR_GREY, "Дверь заперта снаружи.");
		if (Player[playerid][pCuffed]) return true;
		Kidnap_PutOutside(playerid, f);
		return true;
	}
	f = Kidnap_DoorAt(playerid);
	if (f == -1)
	{
		if (Faction_IsGang(Player[playerid][pFaction])) return SendClientMessage(playerid, COLOR_GREY, "Ты не у входа в подвал. Найти его: метка рядом со спавном банды.");
		return SendClientMessage(playerid, COLOR_GREY, "Здесь нет входа.");
	}
	if (IsPlayerInAnyVehicle(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Выйди из машины.");
	if (Player[playerid][pFaction] != f && Player[playerid][pAdmin] < 4)
	{
		new msg[144];
		format(msg, sizeof msg, "[Подвал] Полиция ломает дверь подвала! Внутри %s.", GetRPName(playerid));
		Faction_Message(f, COLOR_RED, msg);
		format(msg, sizeof msg, "%s выбивает дверь и входит в подвал.", GetRPName(playerid));
		SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	}
	Kidnap_PutInside(playerid, f);
	// ведёт связанного — тащит с собой
	for (new v = 0; v < MAX_PLAYERS; v++)
	{
		if (gKidLeader[v] == playerid && IsPlayerConnected(v)) Kidnap_PutInside(v, f);
	}
	return true;
}

stock bool:Cmd_Ransom(playerid, const params[])
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsGang(f)) return SendClientMessage(playerid, COLOR_GREY, "Это для банд.");
	new pos = 0, sum;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, sum)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /ransom [id] [сумма]");
	if (gKidState[target] != KS_HELD || gKidFac[target] != f) return SendClientMessage(playerid, COLOR_GREY, "Он не в вашем подвале.");
	if (sum < 100 || sum > KIDNAP_RANSOM_MAX) return SendClientMessage(playerid, COLOR_GREY, "Выкуп от $100 до $%d.", KIDNAP_RANSOM_MAX);
	gKidRansom[target] = sum;
	new msg[144];
	format(msg, sizeof msg, "[Подвал] %s назначил выкуп за %s: $%d.", GetRPName(playerid), GetRPName(target), sum);
	Faction_Message(f, 0xC0A060FF, msg);
	SendClientMessage(target, COLOR_YELLOW, "За тебя требуют выкуп $%d. Заплатить может кто угодно: /payransom %d", sum, target);
	format(msg, sizeof msg, "[Похищение] Банда требует $%d за %s [%d]. Заплатить: /payransom %d", sum, GetRPName(target), target, target);
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || i == target || !Player[i][pLogged]) continue;
		if (Faction_IsLaw(Player[i][pFaction]) || (Player[target][pFaction] != FACTION_NONE && Player[i][pFaction] == Player[target][pFaction]))
			SendClientMessage(i, COLOR_YELLOW, "%s", msg);
	}
	return true;
}

stock bool:Cmd_PayRansom(playerid, const params[])
{
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /payransom [id заложника]");
	if (gKidState[target] != KS_HELD) return SendClientMessage(playerid, COLOR_GREY, "Этого человека никто не держит.");
	new sum = gKidRansom[target], f = gKidFac[target];
	if (sum <= 0) return SendClientMessage(playerid, COLOR_GREY, "Выкуп ещё не назначен.");
	if (Player[playerid][pFaction] == f) return SendClientMessage(playerid, COLOR_GREY, "Платить своей банде нельзя.");
	if (GetMoney(playerid) < sum) return SendClientMessage(playerid, COLOR_GREY, "Нужно $%d наличными.", sum);
	if (!Faction_AddBank(f, sum)) return SendClientMessage(playerid, COLOR_GREY, "Общак банды переполнен — выкуп не принят.");
	GiveMoney(playerid, -sum, "выкуп заложника");
	Account_Save(playerid);
	new msg[144];
	format(msg, sizeof msg, "[Подвал] За %s заплатили выкуп $%d (%s). Деньги в общаке.", GetRPName(target), sum, GetRPName(playerid));
	Faction_Message(f, 0xC0A060FF, msg);
	if (playerid != target) SendClientMessage(playerid, COLOR_GREEN, "Ты заплатил $%d за %s.", sum, GetRPName(target));
	Kidnap_Free(target, "выкуп заплачен");
	return true;
}

stock bool:Cmd_Release(playerid, const params[])
{
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /release [id]");
	if (gKidState[target] != KS_HELD) return SendClientMessage(playerid, COLOR_GREY, "Он не в подвале.");
	new f = gKidFac[target], pf = Player[playerid][pFaction];
	if (pf == f) { Kidnap_Free(target, "бандиты отпустили"); return true; }
	if (Faction_IsLaw(pf) && Player[playerid][pDuty])
	{
		if (Kidnap_InsideOf(playerid) != f || !IsPlayerNearPlayer(playerid, target, 4.0))
			return SendClientMessage(playerid, COLOR_GREY, "Нужно войти в подвал и подойти к заложнику.");
		Kidnap_Free(target, "освобождён полицией");
		GiveMoney(playerid, KIDNAP_RESCUE_PRIZE, "освобождение заложника");
		Account_Save(playerid);
		new msg[144];
		format(msg, sizeof msg, "[Диспетчер] %s освободил заложника %s из подвала банды «%s».", GetRPName(playerid), GetRPName(target), Faction[f][fName]);
		Faction_MessageLaw(msg);
		return true;
	}
	if (Player[playerid][pAdmin] >= 2) { Kidnap_Free(target, "администратор"); return true; }
	return SendClientMessage(playerid, COLOR_GREY, "Отпустить могут бандиты или полиция на штурме.");
}

stock bool:Cmd_SetHideout(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsGang(f)) return SendClientMessage(playerid, COLOR_GREY, "Подвал есть только у банд.");
	if (Player[playerid][pRank] < 5 && Player[playerid][pAdmin] < 4) return SendClientMessage(playerid, COLOR_GREY, "Переносить вход может ранг 5+.");
	if (IsPlayerInAnyVehicle(playerid) || Kidnap_InsideOf(playerid) != -1) return SendClientMessage(playerid, COLOR_GREY, "Встань пешком на улице.");
	if (!Kidnap_Ensure(f)) return SendClientMessage(playerid, COLOR_GREY, "Не удалось создать вход.");
	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	Marker_SetPos(gHideOut[f], x, y, z);
	SendClientMessage(playerid, COLOR_GREEN, "Вход в подвал перенесён сюда. Его видят только члены банды.");
	return true;
}

// Список заложников банды
stock bool:Cmd_Hostages(playerid)
{
	new f = Player[playerid][pFaction];
	new bool:cop = Faction_IsLaw(f) && Player[playerid][pDuty];
	if (!Faction_IsGang(f) && !cop) return SendClientMessage(playerid, COLOR_GREY, "Это для банд и полиции.");
	new n = 0;
	SendClientMessage(playerid, 0xC0A060FF, cop ? "Похищенные:" : "Заложники в подвале:");
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || gKidState[i] != KS_HELD) continue;
		if (!cop && gKidFac[i] != f) continue;
		if (cop) SendClientMessage(playerid, COLOR_WHITE, " %s [%d] — банда «%s», выкуп $%d", GetRPName(i), i, Faction[gKidFac[i]][fName], gKidRansom[i]);
		else SendClientMessage(playerid, COLOR_WHITE, " %s [%d] — выкуп $%d, осталось %d мин.", GetRPName(i), i, gKidRansom[i], gKidLeft[i] / 60 + 1);
		n++;
	}
	if (n == 0) SendClientMessage(playerid, COLOR_GREY, " Никого.");
	return true;
}

// ---------------------------------------------------------------------
//  Таймер и события игрока
// ---------------------------------------------------------------------
public Kidnap_OnSecond()
{
	static tick;
	if (++tick >= 60)
	{
		tick = 0;
		for (new f = 1; f < MAX_FACTIONS; f++) if (Faction_IsGang(f)) Kidnap_Ensure(f);
	}
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gKidState[i] == KS_NONE || !IsPlayerConnected(i)) continue;
		if (gKidState[i] == KS_TIED)
		{
			// сняли наручники/верёвку другим способом (спавн, /uncuff полиции)
			if (!Player[i][pCuffed]) Kidnap_Clear(i);
			continue;
		}
		if (!Faction_IsGang(gKidFac[i])) { Kidnap_Free(i, "банда распалась"); continue; }
		if (gKidGrace[i] > 0) { gKidGrace[i]--; continue; }
		if (GetPlayerState(i) == PLAYER_STATE_WASTED) continue;
		if (Kidnap_InsideOf(i) != gKidFac[i]) { Kidnap_Clear(i); gKidImmune[i] = gettime() + KIDNAP_IMMUNE; continue; }   // админ телепортировал и т.п.
		if (--gKidLeft[i] <= 0) Kidnap_Free(i, "время вышло, тебя выкинули на улицу");
	}
	return 1;
}

// Из OnPlayerSpawn (последним): заложник возвращается в подвал
stock Kidnap_OnSpawn(playerid)
{
	if (gKidState[playerid] == KS_TIED) Kidnap_Clear(playerid);
	if (gKidState[playerid] == KS_NONE)
	{
		new db = Player[playerid][pDbId];
		for (new s = 0; s < KIDNAP_SAVED; s++)
		{
			if (db == 0 || gKidSaveDb[s] != db) continue;
			gKidSaveDb[s] = 0;
			if (!Faction_IsGang(gKidSaveFac[s]) || Player[playerid][pJail] > 0) break;
			gKidState[playerid] = KS_HELD;
			gKidFac[playerid] = gKidSaveFac[s];
			gKidLeft[playerid] = gKidSaveLeft[s];
			gKidRansom[playerid] = gKidSaveRansom[s];
			SendClientMessage(playerid, COLOR_RED, "Ты вышел из игры в подвале — ты всё ещё заложник.");
			break;
		}
	}
	if (gKidState[playerid] != KS_HELD || !Kidnap_Ensure(gKidFac[playerid])) return;
	gKidGrace[playerid] = 3;
	Kidnap_PutInside(playerid, gKidFac[playerid]);
}

stock Kidnap_OnDisconnect(playerid)
{
	if (gKidState[playerid] == KS_HELD && Player[playerid][pDbId] != 0)
	{
		new slot = 0, oldest = cellmax;
		for (new s = 0; s < KIDNAP_SAVED; s++)
		{
			if (gKidSaveDb[s] == 0) { slot = s; break; }
			if (gKidSaveLeft[s] < oldest) { oldest = gKidSaveLeft[s]; slot = s; }
		}
		gKidSaveDb[slot] = Player[playerid][pDbId];
		gKidSaveFac[slot] = gKidFac[playerid];
		gKidSaveLeft[slot] = gKidLeft[playerid];
		gKidSaveRansom[slot] = gKidRansom[playerid];
		new msg[144];
		format(msg, sizeof msg, "[Подвал] Заложник %s вышел из игры. Вернётся в подвал, когда зайдёт.", GetRPName(playerid));
		Faction_Message(gKidFac[playerid], 0xC0A060FF, msg);
	}
	Kidnap_Clear(playerid);
	gKidImmune[playerid] = 0;
	gKidGrace[playerid] = 0;
}

// Для меню взаимодействия (keys.inc)
stock bool:Kidnap_IsTiedBy(target, f)
{
	return (gKidState[target] == KS_TIED && gKidFac[target] == f);
}

stock bool:Kidnap_IsTied(target)
{
	return (gKidState[target] == KS_TIED);
}
