// =====================================================================
//  turfs.inc — районы банд и войны за них.
//
//  Карта поделена на районы (цветные квадраты на радаре). Цвет района —
//  цвет банды, которая им владеет. Серый — ничей.
//
//  Как захватить район:
//  1. Член банды ранга TURF_MIN_RANK+ встаёт в чужой (или ничей) район
//     и вводит /capture.
//  2. Район мигает у всех, банды получают сообщение. Захват длится
//     TURF_CAPTURE_TIME секунд.
//  3. Очки: +1 за каждое убийство врага в районе и +1 за каждого живого
//     бойца, который стоит в районе в конце. Больше очков у нападающих —
//     район их. Ничья — район остаётся у защитников.
//
//  Каждый PayDay члены банды онлайн получают TURF_INCOME за каждый район.
//  Время, когда можно захватывать, задаётся в config.inc.
//
//  Админы (4+): /addturf [размер] [название] — новый район вокруг себя,
//  /delturf — удалить район, в котором стоишь.
// =====================================================================

#define MAX_TURFS           40

enum E_TURF
{
	tDbId,
	tName[32],
	Float:tMinX,
	Float:tMinY,
	Float:tMaxX,
	Float:tMaxY,
	tOwner,             // FACTION_* банды (0 = ничей)
	tZone,              // id зоны на радаре
	tLastWar            // когда последний раз атаковали (gettime)
}
new Turf[MAX_TURFS][E_TURF];
new gTurfCount = 0;

// Текущая война (одна на весь сервер)
new gWarTurf = -1;          // какой район атакуют (-1 = войны нет)
new gWarAttacker;           // банда-нападающий
new gWarDefender;           // банда-защитник (0 = район был ничей)
new gWarAttackScore;
new gWarDefendScore;

// Районы по умолчанию (создаются при первом запуске). Координаты примерные.
enum E_TURF_DEF
{
	tdName[32],
	Float:tdMinX,
	Float:tdMinY,
	Float:tdMaxX,
	Float:tdMaxY,
	tdOwner
}
static const gTurfDefaults[][E_TURF_DEF] =
{
	{ "Grove Street",  2400.0, -1730.0, 2540.0, -1620.0, FACTION_GROVE },
	{ "Ganton",        2220.0, -1770.0, 2400.0, -1620.0, FACTION_GROVE },
	{ "Idlewood",      1820.0, -1880.0, 2100.0, -1700.0, FACTION_NONE },
	{ "Jefferson",     2140.0, -1300.0, 2330.0, -1100.0, FACTION_BALLAS },
	{ "Glen Park",     1880.0, -1300.0, 2060.0, -1130.0, FACTION_BALLAS },
	{ "East LS",       2330.0, -1400.0, 2550.0, -1200.0, FACTION_NONE },
	{ "El Corona",     1800.0, -2140.0, 2000.0, -1940.0, FACTION_AZTECAS },
	{ "Little Mexico", 1680.0, -1880.0, 1820.0, -1720.0, FACTION_AZTECAS },
	{ "Willowfield",   2280.0, -2060.0, 2520.0, -1890.0, FACTION_NONE }
};

// Цвет района на радаре (полупрозрачный)
stock Turf_GetColor(t)
{
	if (!Faction_IsGang(Turf[t][tOwner])) return 0xFFFFFF40;
	return (Faction_GetColor(Turf[t][tOwner]) & 0xFFFFFF00) | 0x80;
}

stock bool:Turf_IsPlayerIn(playerid, t)
{
	if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return false;
	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	return (x >= Turf[t][tMinX] && x <= Turf[t][tMaxX] && y >= Turf[t][tMinY] && y <= Turf[t][tMaxY]);
}

// В каком районе стоит игрок (-1 = ни в каком)
stock Turf_GetAt(playerid)
{
	for (new t = 0; t < gTurfCount; t++)
	{
		if (Turf_IsPlayerIn(playerid, t)) return t;
	}
	return -1;
}

stock Turf_CountOwned(f)
{
	new count = 0;
	for (new t = 0; t < gTurfCount; t++)
	{
		if (Turf[t][tOwner] == f) count++;
	}
	return count;
}

stock Turf_Save(t)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE turfs SET owner = %d WHERE id = %d", Turf[t][tOwner], Turf[t][tDbId]));
}

stock Turf_Add(dbid, const name[], Float:minx, Float:miny, Float:maxx, Float:maxy, owner)
{
	if (gTurfCount >= MAX_TURFS) return -1;
	new t = gTurfCount++;
	Turf[t][tDbId] = dbid;
	Turf[t][tName][0] = '\0';
	strcat(Turf[t][tName], name, 32);
	Turf[t][tMinX] = minx;
	Turf[t][tMinY] = miny;
	Turf[t][tMaxX] = maxx;
	Turf[t][tMaxY] = maxy;
	Turf[t][tOwner] = Faction_IsGang(owner) ? owner : FACTION_NONE;
	Turf[t][tLastWar] = 0;
	Turf[t][tZone] = GangZoneCreate(minx, miny, maxx, maxy);
	GangZoneShowForAll(Turf[t][tZone], Turf_GetColor(t));
	return t;
}

stock Turfs_Init()
{
	new count = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM turfs");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (count == 0)
	{
		for (new i = 0; i < sizeof (gTurfDefaults); i++)
		{
			DB_FreeResultSet(DB_ExecuteQuery(gDB,
				"INSERT INTO turfs (name, minx, miny, maxx, maxy, owner) VALUES ('%q', %f, %f, %f, %f, %d)",
				gTurfDefaults[i][tdName], gTurfDefaults[i][tdMinX], gTurfDefaults[i][tdMinY],
				gTurfDefaults[i][tdMaxX], gTurfDefaults[i][tdMaxY], gTurfDefaults[i][tdOwner]));
		}
	}

	r = DB_ExecuteQuery(gDB, "SELECT id, name, minx, miny, maxx, maxy, owner FROM turfs ORDER BY id");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		new name[32];
		for (new i = 0; i < rows; i++)
		{
			DB_GetFieldString(r, 1, name, sizeof name);
			Turf_Add(DB_GetFieldInt(r, 0), name, DB_GetFieldFloat(r, 2), DB_GetFieldFloat(r, 3),
				DB_GetFieldFloat(r, 4), DB_GetFieldFloat(r, 5), DB_GetFieldInt(r, 6));
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Turfs] Loaded %d turfs.", gTurfCount);
}

// Показать районы игроку, который только что зашёл (из OnPlayerSpawn)
stock Turfs_ShowForPlayer(playerid)
{
	for (new t = 0; t < gTurfCount; t++)
	{
		GangZoneShowForPlayer(playerid, Turf[t][tZone], Turf_GetColor(t));
		if (t == gWarTurf) GangZoneFlashForPlayer(playerid, Turf[t][tZone], 0xFF000080);
	}
}

// Сообщение обеим бандам, которые воюют
stock Turf_WarMessage(const message[])
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged]) continue;
		new f = Player[i][pFaction];
		if (f == gWarAttacker || (f == gWarDefender && f != FACTION_NONE))
			SendClientMessage(i, COLOR_ORANGE, "%s", message);
	}
}

// Из OnPlayerDeath: очки за убийства во время войны
stock Turfs_OnDeath(playerid, killerid)
{
	if (gWarTurf == -1 || killerid == INVALID_PLAYER_ID || !IsPlayerConnected(killerid)) return;
	new kf = Player[killerid][pFaction], vf = Player[playerid][pFaction];
	if (!Turf_IsPlayerIn(killerid, gWarTurf)) return;

	new msg[144];
	if (kf == gWarAttacker && vf == gWarDefender && vf != FACTION_NONE)
	{
		gWarAttackScore++;
		format(msg, sizeof msg, "[Война] %s убил защитника. Счёт: нападающие %d — защитники %d", GetRPName(killerid), gWarAttackScore, gWarDefendScore);
		Turf_WarMessage(msg);
	}
	else if (kf == gWarDefender && kf != FACTION_NONE && vf == gWarAttacker)
	{
		gWarDefendScore++;
		format(msg, sizeof msg, "[Война] %s убил нападающего. Счёт: нападающие %d — защитники %d", GetRPName(killerid), gWarAttackScore, gWarDefendScore);
		Turf_WarMessage(msg);
	}
}

forward OnTurfWarEnd();
public OnTurfWarEnd()
{
	new t = gWarTurf;
	if (t == -1) return 1;

	// +1 очко за каждого живого бойца в районе
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || !Turf_IsPlayerIn(i, t)) continue;
		if (GetPlayerState(i) == PLAYER_STATE_WASTED) continue;
		if (Player[i][pFaction] == gWarAttacker) gWarAttackScore++;
		else if (gWarDefender != FACTION_NONE && Player[i][pFaction] == gWarDefender) gWarDefendScore++;
	}

	GangZoneStopFlashForAll(Turf[t][tZone]);
	if (gWarAttackScore > gWarDefendScore)
	{
		Turf[t][tOwner] = gWarAttacker;
		Turf_Save(t);
		SendClientMessageToAll(Faction_GetColor(gWarAttacker), "[Война] «%s» захватили район %s! Счёт %d:%d.",
			Faction_GetName(gWarAttacker), Turf[t][tName], gWarAttackScore, gWarDefendScore);
	}
	else
	{
		SendClientMessageToAll(COLOR_ORANGE, "[Война] Захват района %s провалился. Счёт %d:%d.",
			Turf[t][tName], gWarAttackScore, gWarDefendScore);
	}
	GangZoneShowForAll(Turf[t][tZone], Turf_GetColor(t));
	Turf[t][tLastWar] = gettime();
	gWarTurf = -1;
	return 1;
}

// Зарплата банды за районы на PayDay. Возвращает сумму.
stock Turfs_GetIncome(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsGang(f)) return 0;
	return Turf_CountOwned(f) * TURF_INCOME;
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Cmd_Turfs(playerid)
{
	SendClientMessage(playerid, COLOR_ORANGE, "Районы банд:");
	for (new t = 0; t < gTurfCount; t++)
	{
		if (Faction_IsGang(Turf[t][tOwner]))
			SendClientMessage(playerid, Faction_GetColor(Turf[t][tOwner]), " %s — %s", Turf[t][tName], Faction_GetName(Turf[t][tOwner]));
		else
			SendClientMessage(playerid, COLOR_GREY, " %s — ничей", Turf[t][tName]);
	}
	if (gWarTurf != -1)
		SendClientMessage(playerid, COLOR_RED, "Сейчас идёт захват района %s.", Turf[gWarTurf][tName]);
	return true;
}

stock bool:Cmd_Capture(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsGang(f)) return SendClientMessage(playerid, COLOR_GREY, "Захватывать районы могут только банды.");
	if (Player[playerid][pRank] < TURF_MIN_RANK)
		return SendClientMessage(playerid, COLOR_GREY, "Начать захват можно с ранга %d.", TURF_MIN_RANK);
	if (gWarTurf != -1) return SendClientMessage(playerid, COLOR_GREY, "Уже идёт захват района %s.", Turf[gWarTurf][tName]);

	new hour;
	gettime(hour);
	if (hour < CAPTURE_HOUR_FROM || hour >= CAPTURE_HOUR_TO)
		return SendClientMessage(playerid, COLOR_GREY, "Захватывать районы можно с %d:00 до %d:00.", CAPTURE_HOUR_FROM, CAPTURE_HOUR_TO);

	new t = Turf_GetAt(playerid);
	if (t == -1) return SendClientMessage(playerid, COLOR_GREY, "Ты не в районе банд. Список: /turfs");
	if (Turf[t][tOwner] == f) return SendClientMessage(playerid, COLOR_GREY, "Этот район уже ваш.");
	new wait = Turf[t][tLastWar] + TURF_COOLDOWN - gettime();
	if (Turf[t][tLastWar] != 0 && wait > 0)
		return SendClientMessage(playerid, COLOR_GREY, "На этот район недавно нападали. Подожди %d мин.", wait / 60 + 1);

	gWarTurf = t;
	gWarAttacker = f;
	gWarDefender = Turf[t][tOwner];
	gWarAttackScore = 0;
	gWarDefendScore = 0;
	GangZoneFlashForAll(Turf[t][tZone], 0xFF000080);
	SetTimer("OnTurfWarEnd", TURF_CAPTURE_TIME * 1000, false);

	if (gWarDefender != FACTION_NONE)
		SendClientMessageToAll(COLOR_ORANGE, "[Война] «%s» нападают на район %s («%s»)! Захват — %d мин.",
			Faction_GetName(f), Turf[t][tName], Faction_GetName(gWarDefender), TURF_CAPTURE_TIME / 60);
	else
		SendClientMessageToAll(COLOR_ORANGE, "[Война] «%s» захватывают ничейный район %s! Захват — %d мин.",
			Faction_GetName(f), Turf[t][tName], TURF_CAPTURE_TIME / 60);
	return true;
}

stock bool:Cmd_AddTurf(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, size, name[32];
	if (!NextIntParam(params, pos, size) || size < 30 || size > 500 || !RestParam(params, pos, name))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /addturf [размер 30-500] [название] — квадрат вокруг тебя");
	if (gTurfCount >= MAX_TURFS) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит районов (MAX_TURFS).");

	new Float:x, Float:y, Float:z, Float:half = float(size) / 2.0;
	GetPlayerPos(playerid, x, y, z);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO turfs (name, minx, miny, maxx, maxy, owner) VALUES ('%q', %f, %f, %f, %f, 0)",
		name, x - half, y - half, x + half, y + half));
	Turf_Add(DB_LastInsertId(), name, x - half, y - half, x + half, y + half, FACTION_NONE);
	SendClientMessage(playerid, COLOR_ADMIN, "Район «%s» создан. Отдать банде: /setturf [банда 3-5]", name);
	return true;
}

stock bool:Cmd_SetTurf(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f;
	if (!NextIntParam(params, pos, f) || (f != FACTION_NONE && !Faction_IsGang(f)))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setturf [0 — ничей, 3 Grove, 4 Ballas, 5 Aztecas]");
	new t = Turf_GetAt(playerid);
	if (t == -1) return SendClientMessage(playerid, COLOR_GREY, "Встань в район.");
	if (t == gWarTurf) return SendClientMessage(playerid, COLOR_GREY, "Здесь идёт захват, подожди.");
	Turf[t][tOwner] = f;
	Turf_Save(t);
	GangZoneShowForAll(Turf[t][tZone], Turf_GetColor(t));
	if (f == FACTION_NONE) SendClientMessage(playerid, COLOR_ADMIN, "Район %s теперь ничей.", Turf[t][tName]);
	else SendClientMessage(playerid, COLOR_ADMIN, "Район %s теперь: %s.", Turf[t][tName], Faction_GetName(f));
	return true;
}

stock bool:Cmd_DelTurf(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new t = Turf_GetAt(playerid);
	if (t == -1) return SendClientMessage(playerid, COLOR_GREY, "Встань в район, который хочешь удалить.");
	if (t == gWarTurf) return SendClientMessage(playerid, COLOR_GREY, "Здесь идёт захват, подожди.");

	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM turfs WHERE id = %d", Turf[t][tDbId]));
	GangZoneDestroy(Turf[t][tZone]);
	SendClientMessage(playerid, COLOR_ADMIN, "Район %s удалён.", Turf[t][tName]);

	// сдвигаем последний район на место удалённого
	new last = gTurfCount - 1;
	if (t != last)
	{
		Turf[t] = Turf[last];
		if (gWarTurf == last) gWarTurf = t;
	}
	gTurfCount--;
	return true;
}
