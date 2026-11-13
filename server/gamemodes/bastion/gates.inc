// =====================================================================
//  gates.inc — шлагбаумы и ворота фракций.
//
//  H в машине (или ALT пешком) рядом со шлагбаумом своей фракции —
//  открыть. Через GATE_OPEN_TIME секунд он закроется сам.
//
//  Админы (4+):
//    /addgate [фракция 0-5] [тип 1-2] — поставить там, где стоишь
//        (тип 1 — шлагбаум, 2 — большие ворота; фракция 0 — открывают все)
//        Шлагбаум встаёт поперёк того направления, куда ты смотришь.
//    /delgate — удалить ближайший
// =====================================================================

#define MAX_GATES           40
#define GATE_RANGE          10.0    // с какого расстояния можно открыть
#define GATE_OPEN_TIME      6       // через сколько секунд закрыть

#define GATE_BARRIER        1       // шлагбаум (объект 968)
#define GATE_DOOR           2       // ворота (объект 980), уезжают вниз

enum E_GATE
{
	gtDbId,
	gtFaction,
	gtType,
	Float:gtX,
	Float:gtY,
	Float:gtZ,
	Float:gtA,
	gtObject,
	bool:gtOpen
}
new Gate[MAX_GATES][E_GATE];
new gGateCount = 0;

stock Gate_Create(dbid, f, type, Float:x, Float:y, Float:z, Float:a)
{
	if (gGateCount >= MAX_GATES) return -1;
	new g = gGateCount++;
	Gate[g][gtDbId] = dbid;
	Gate[g][gtFaction] = f;
	Gate[g][gtType] = type;
	Gate[g][gtX] = x;
	Gate[g][gtY] = y;
	Gate[g][gtZ] = z;
	Gate[g][gtA] = a;
	Gate[g][gtOpen] = false;
	if (type == GATE_BARRIER) Gate[g][gtObject] = CreateObject(968, x, y, z, 0.0, 90.0, a, 150.0);
	else Gate[g][gtObject] = CreateObject(980, x, y, z, 0.0, 0.0, a, 150.0);
	return g;
}

stock Gates_Init()
{
	new count = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM gates");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (count == 0)
	{
		// Шлагбаум на въезде к LSPD (координаты примерные)
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO gates (faction, type, x, y, z, a) VALUES (%d, %d, %f, %f, %f, %f)",
			FACTION_POLICE, GATE_BARRIER, 1544.7, -1630.7, 13.1, 90.0));
	}

	r = DB_ExecuteQuery(gDB, "SELECT id, faction, type, x, y, z, a FROM gates ORDER BY id");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			Gate_Create(DB_GetFieldInt(r, 0), DB_GetFieldInt(r, 1), DB_GetFieldInt(r, 2),
				DB_GetFieldFloat(r, 3), DB_GetFieldFloat(r, 4), DB_GetFieldFloat(r, 5), DB_GetFieldFloat(r, 6));
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Gates] Loaded %d gates.", gGateCount);
}

// Ближайший шлагбаум в радиусе (-1 = нет)
stock Gate_GetNearby(playerid, Float:range = GATE_RANGE)
{
	new best = -1, Float:bestDist = range + 1.0;
	for (new g = 0; g < gGateCount; g++)
	{
		new Float:d = GetPlayerDistanceFromPoint(playerid, Gate[g][gtX], Gate[g][gtY], Gate[g][gtZ]);
		if (d <= range && d < bestDist)
		{
			best = g;
			bestDist = d;
		}
	}
	return best;
}

stock Gate_Move(g, bool:open)
{
	Gate[g][gtOpen] = open;
	if (Gate[g][gtType] == GATE_BARRIER)
	{
		// поднимаем стрелу: поворот с 90 до 0 градусов (z чуть меняем, чтобы объект поехал)
		if (open) MoveObject(Gate[g][gtObject], Gate[g][gtX], Gate[g][gtY], Gate[g][gtZ] + 0.01, 0.02, 0.0, 0.0, Gate[g][gtA]);
		else MoveObject(Gate[g][gtObject], Gate[g][gtX], Gate[g][gtY], Gate[g][gtZ], 0.02, 0.0, 90.0, Gate[g][gtA]);
	}
	else
	{
		// ворота уезжают под землю
		if (open) MoveObject(Gate[g][gtObject], Gate[g][gtX], Gate[g][gtY], Gate[g][gtZ] - 5.8, 2.5);
		else MoveObject(Gate[g][gtObject], Gate[g][gtX], Gate[g][gtY], Gate[g][gtZ], 2.5);
	}
}

forward OnGateClose(g, dbid);
public OnGateClose(g, dbid)
{
	// dbid — защита: за это время шлагбаум могли удалить и номер занял другой
	if (g >= 0 && g < gGateCount && Gate[g][gtDbId] == dbid && Gate[g][gtOpen]) Gate_Move(g, false);
	return 1;
}

// Открыть шлагбаум рядом (из keys.inc). Возвращает 1, если рядом был шлагбаум.
stock Gates_TryOpen(playerid)
{
	new g = Gate_GetNearby(playerid);
	if (g == -1) return 0;
	new f = Gate[g][gtFaction];
	if (f != FACTION_NONE && Player[playerid][pFaction] != f && Player[playerid][pAdmin] < 3)
	{
		SendClientMessage(playerid, COLOR_GREY, "Этот проезд открывается только для фракции «%s».", Faction_GetName(f));
		return 1;
	}
	if (Gate[g][gtOpen]) return 1;
	Gate_Move(g, true);
	SetTimerEx("OnGateClose", GATE_OPEN_TIME * 1000, false, "ii", g, Gate[g][gtDbId]);
	return 1;
}

// ---------------------------------------------------------------------
//  Админ-команды
// ---------------------------------------------------------------------
stock bool:Cmd_AddGate(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f, type;
	if (!NextIntParam(params, pos, f) || f < 0 || f >= MAX_FACTIONS || (f != FACTION_NONE && !Faction_IsValid(f)) || !NextIntParam(params, pos, type) || type < 1 || type > 2)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /addgate [фракция, 0 — для всех] [тип: 1 шлагбаум, 2 ворота]");
	if (GetPlayerInterior(playerid) != 0) return SendClientMessage(playerid, COLOR_GREY, "Только на улице.");
	if (gGateCount >= MAX_GATES) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит (MAX_GATES).");

	new Float:x, Float:y, Float:z, Float:a;
	GetPlayerPos(playerid, x, y, z);
	GetPlayerFacingAngle(playerid, a);
	if (type == GATE_BARRIER) { z -= 0.9; a += 90.0; }    // стрела поперёк дороги, чуть ниже
	else z += 1.5;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO gates (faction, type, x, y, z, a) VALUES (%d, %d, %f, %f, %f, %f)", f, type, x, y, z, a));
	Gate_Create(DB_LastInsertId(), f, type, x, y, z, a);
	SetPlayerPos(playerid, x + 2.0, y + 2.0, z + 1.0);   // отойти, чтобы не застрять в объекте
	SendClientMessage(playerid, COLOR_ADMIN, "Готово. Открыть: H в машине или ALT пешком. Удалить: /delgate");
	return true;
}

stock bool:Cmd_DelGate(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new g = Gate_GetNearby(playerid);
	if (g == -1) return SendClientMessage(playerid, COLOR_GREY, "Рядом нет шлагбаума или ворот.");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM gates WHERE id = %d", Gate[g][gtDbId]));
	DestroyObject(Gate[g][gtObject]);
	new last = gGateCount - 1;
	if (g != last) Gate[g] = Gate[last];
	gGateCount--;
	SendClientMessage(playerid, COLOR_ADMIN, "Удалено.");
	return true;
}
