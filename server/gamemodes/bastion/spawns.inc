// =====================================================================
//  spawns.inc — точки появления обычных игроков (вокзалы и т.п.).
//
//  Игрок сам выбирает, где появляться: /setspawn (вокзал, дом или база фракции).
//  Пока не выбрал — у дома, иначе на базе фракции, иначе на самом свободном вокзале
//  (где сейчас меньше всего игроков рядом).
//  Пока точек нет — на стандартном спавне из config.inc.
//  Админ (4+), стоя на улице:
//    /addspawn [название] — точка там, где стоишь (взгляд запоминается)
//    /delspawn [№]        — убрать точку
//    /spawns              — список точек, /gotospawn [№] — телепорт
// =====================================================================

#define MAX_SPAWNS          20
#define SPAWN_LOAD_RANGE    80.0    // игроки в этом радиусе считаются «на вокзале»

enum E_SPAWN
{
	spDbId,
	spName[32],
	Float:spX, Float:spY, Float:spZ, Float:spA
}
static gSpawn[MAX_SPAWNS][E_SPAWN];
static gSpawnCount = 0;

// Что игрок выбрал в /setspawn (accounts.spawn_mode, spawn_ref)
enum
{
	SPAWN_AUTO,         // дом, иначе фракция, иначе случайный вокзал
	SPAWN_STATION,      // вокзал: ref = номер точки в базе
	SPAWN_HOUSE,        // свой или съёмный дом
	SPAWN_FACTION,      // база фракции
	SPAWN_ANYSTATION    // самый свободный вокзал
}
static gSpawnMode[MAX_PLAYERS];
static gSpawnRef[MAX_PLAYERS];
static gSpawnPick[MAX_PLAYERS][MAX_SPAWNS + 3][2];   // +3: вокзал, дом, фракция   // строка окна -> { режим, ref }

stock Spawns_Init()
{
	DB_AddColumn("accounts", "spawn_mode", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "spawn_ref", "INTEGER DEFAULT 0");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS spawns (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, x REAL, y REAL, z REAL, a REAL)"));
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, name, x, y, z, a FROM spawns ORDER BY id");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows && gSpawnCount < MAX_SPAWNS; i++)
		{
			new s = gSpawnCount++;
			gSpawn[s][spDbId] = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 1, gSpawn[s][spName], 32);
			gSpawn[s][spX] = DB_GetFieldFloat(r, 2);
			gSpawn[s][spY] = DB_GetFieldFloat(r, 3);
			gSpawn[s][spZ] = DB_GetFieldFloat(r, 4);
			gSpawn[s][spA] = DB_GetFieldFloat(r, 5);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Spawns] Loaded %d spawn points.", gSpawnCount);
}

// Самый свободный вокзал: где рядом меньше всего игроков (при равенстве — случайный из них).
// 0 — точек нет, координаты не тронуты.
stock Spawns_Get(&Float:x, &Float:y, &Float:z, &Float:a)
{
	if (gSpawnCount == 0) return 0;
	new load[MAX_SPAWNS], best = 0x7FFFFFFF, ties = 0;
	for (new s = 0; s < gSpawnCount; s++)
	{
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (!IsPlayerConnected(i) || GetPlayerInterior(i) != 0 || GetPlayerVirtualWorld(i) != 0) continue;
			if (IsPlayerInRangeOfPoint(i, SPAWN_LOAD_RANGE, gSpawn[s][spX], gSpawn[s][spY], gSpawn[s][spZ])) load[s]++;
		}
		if (load[s] < best) { best = load[s]; ties = 1; }
		else if (load[s] == best) ties++;
	}
	new pick = random(ties), s = 0;
	for (new k = 0; k < gSpawnCount; k++)
	{
		if (load[k] != best) continue;
		if (pick-- == 0) { s = k; break; }
	}
	x = gSpawn[s][spX];
	y = gSpawn[s][spY];
	z = gSpawn[s][spZ];
	a = gSpawn[s][spA];
	return 1;
}

static Spawn_FindByDbId(dbid)
{
	for (new s = 0; s < gSpawnCount; s++)
	{
		if (gSpawn[s][spDbId] == dbid) return s;
	}
	return -1;
}

// Уровень админа для этих команд проверяется в commands.inc
stock Cmd_AddSpawn(playerid, const params[])
{
	new name[32];
	if (!RestParam(params, 0, name))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /addspawn [название] — точка появления игроков там, где ты стоишь");
	if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0)
		return SendClientMessage(playerid, COLOR_GREY, "Точку спавна можно ставить только на улице.");
	if (gSpawnCount >= MAX_SPAWNS) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит точек спавна (%d).", MAX_SPAWNS);

	new s = gSpawnCount++;
	GetPlayerPos(playerid, gSpawn[s][spX], gSpawn[s][spY], gSpawn[s][spZ]);
	GetPlayerFacingAngle(playerid, gSpawn[s][spA]);
	gSpawn[s][spName][0] = '\0';
	strcat(gSpawn[s][spName], name, 32);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO spawns (name, x, y, z, a) VALUES ('%q', %f, %f, %f, %f)",
		name, gSpawn[s][spX], gSpawn[s][spY], gSpawn[s][spZ], gSpawn[s][spA]));
	gSpawn[s][spDbId] = DB_LastInsertId();
	SendClientMessage(playerid, COLOR_ADMIN, "Точка спавна №%d «%s» создана. Всего точек: %d. Список: /spawns", gSpawn[s][spDbId], name, gSpawnCount);
	return 1;
}

stock Cmd_DelSpawn(playerid, const params[])
{
	new pos = 0, dbid;
	if (!NextIntParam(params, pos, dbid)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /delspawn [№] (номера: /spawns)");
	new s = Spawn_FindByDbId(dbid);
	if (s == -1) return SendClientMessage(playerid, COLOR_GREY, "Точки с таким номером нет. Список: /spawns");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM spawns WHERE id = %d", dbid));
	SendClientMessage(playerid, COLOR_ADMIN, "Точка спавна №%d «%s» убрана.", dbid, gSpawn[s][spName]);
	new last = gSpawnCount - 1;
	if (s != last) gSpawn[s] = gSpawn[last];
	gSpawnCount--;
	return 1;
}

stock Cmd_Spawns(playerid)
{
	if (gSpawnCount == 0) return SendClientMessage(playerid, COLOR_GREY, "Точек спавна нет: игроки появляются на стандартном месте. Добавить: /addspawn [название]");
	SendClientMessage(playerid, COLOR_YELLOW, "Точки спавна (игрок без дома и фракции появляется на самой свободной):");
	for (new s = 0; s < gSpawnCount; s++)
	{
		SendClientMessage(playerid, COLOR_WHITE, " №%d — %s (%.0f, %.0f)", gSpawn[s][spDbId], gSpawn[s][spName], gSpawn[s][spX], gSpawn[s][spY]);
	}
	return 1;
}

stock Cmd_GotoSpawn(playerid, const params[])
{
	new pos = 0, dbid;
	if (!NextIntParam(params, pos, dbid)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /gotospawn [№] (номера: /spawns)");
	new s = Spawn_FindByDbId(dbid);
	if (s == -1) return SendClientMessage(playerid, COLOR_GREY, "Точки с таким номером нет. Список: /spawns");
	if (IsPlayerInAnyVehicle(playerid)) RemovePlayerFromVehicle(playerid);
	SetPlayerInterior(playerid, 0);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerPos(playerid, gSpawn[s][spX], gSpawn[s][spY], gSpawn[s][spZ]);
	SetPlayerFacingAngle(playerid, gSpawn[s][spA]);
	SetCameraBehindPlayer(playerid);
	Player[playerid][pHouseInside] = -1;
	return 1;
}

// ---------------------------------------------------------------------
//  /setspawn — игрок выбирает, где появляться
// ---------------------------------------------------------------------

// Из Account_FinishLogin: выбор игрока из базы
stock Spawns_LoadPlayer(playerid)
{
	gSpawnMode[playerid] = SPAWN_AUTO;
	gSpawnRef[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT spawn_mode, spawn_ref FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			gSpawnMode[playerid] = DB_GetFieldInt(r, 0);
			gSpawnRef[playerid] = DB_GetFieldInt(r, 1);
		}
		DB_FreeResultSet(r);
	}
	if (gSpawnMode[playerid] < SPAWN_AUTO || gSpawnMode[playerid] > SPAWN_ANYSTATION) gSpawnMode[playerid] = SPAWN_AUTO;
}

stock Spawns_GetMode(playerid) { return gSpawnMode[playerid]; }

// Вокзал, выбранный игроком. 0 — не выбран или точку уже убрали.
stock Spawns_GetChosen(playerid, &Float:x, &Float:y, &Float:z, &Float:a)
{
	if (gSpawnMode[playerid] != SPAWN_STATION) return 0;
	new s = Spawn_FindByDbId(gSpawnRef[playerid]);
	if (s == -1) return 0;
	x = gSpawn[s][spX];
	y = gSpawn[s][spY];
	z = gSpawn[s][spZ];
	a = gSpawn[s][spA];
	return 1;
}

stock Cmd_SetSpawn(playerid)
{
	new list[1024], line[64], n = 0;
	gSpawnPick[playerid][n][0] = SPAWN_ANYSTATION; gSpawnPick[playerid][n][1] = 0; n++;
	strcat(list, "{FFFFFF}Вокзал {AFAFAF}(где меньше людей)\n");
	if (House_GetOwned(playerid) != -1 || House_GetRented(playerid) != -1 || Family_SpouseHouse(playerid) != -1)
	{
		gSpawnPick[playerid][n][0] = SPAWN_HOUSE; gSpawnPick[playerid][n][1] = 0; n++;
		strcat(list, "{33CCFF}У своего дома\n");
	}
	if (Player[playerid][pFaction] != FACTION_NONE)
	{
		gSpawnPick[playerid][n][0] = SPAWN_FACTION; gSpawnPick[playerid][n][1] = 0; n++;
		strcat(list, "{FFFF66}На базе фракции\n");
	}
	for (new s = 0; s < gSpawnCount; s++)
	{
		gSpawnPick[playerid][n][0] = SPAWN_STATION; gSpawnPick[playerid][n][1] = gSpawn[s][spDbId]; n++;
		format(line, sizeof line, "%s\n", gSpawn[s][spName]);
		strcat(list, line);
		if (n >= sizeof (gSpawnPick[])) break;
	}
	for (new i = n; i < sizeof (gSpawnPick[]); i++) gSpawnPick[playerid][i][0] = -1;
	ShowPlayerDialog(playerid, DIALOG_SETSPAWN, DIALOG_STYLE_LIST, "Где появляться", "%s", "Выбрать", "Закрыть", list);
	return 1;
}

// Из OnDialogResponse
stock Spawns_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid != DIALOG_SETSPAWN) return 0;
	if (!response || listitem < 0 || listitem >= sizeof (gSpawnPick[]) || gSpawnPick[playerid][listitem][0] == -1) return 1;
	gSpawnMode[playerid] = gSpawnPick[playerid][listitem][0];
	gSpawnRef[playerid] = gSpawnPick[playerid][listitem][1];
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET spawn_mode = %d, spawn_ref = %d WHERE id = %d",
		gSpawnMode[playerid], gSpawnRef[playerid], Player[playerid][pDbId]));
	Account_SetNormalSpawn(playerid);
	switch (gSpawnMode[playerid])
	{
		case SPAWN_HOUSE:   SendClientMessage(playerid, COLOR_GREEN, "Теперь ты появляешься у своего дома.");
		case SPAWN_FACTION: SendClientMessage(playerid, COLOR_GREEN, "Теперь ты появляешься на базе фракции.");
		case SPAWN_STATION:
		{
			new s = Spawn_FindByDbId(gSpawnRef[playerid]);
			if (s != -1) SendClientMessage(playerid, COLOR_GREEN, "Теперь ты появляешься здесь: %s.", gSpawn[s][spName]);
		}
		case SPAWN_ANYSTATION: SendClientMessage(playerid, COLOR_GREEN, "Теперь ты появляешься на вокзале, где меньше людей.");
		default: SendClientMessage(playerid, COLOR_GREEN, "Место появления выбрано.");
	}
	return 1;
}
