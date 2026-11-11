// =====================================================================
//  spawns.inc — точки появления обычных игроков (вокзалы и т.п.).
//
//  Игрок без дома и без фракции появляется в одной из этих точек (случайной).
//  Пока точек нет — на стандартном спавне из config.inc.
//  Админ (4+), стоя на улице:
//    /addspawn [название] — точка там, где стоишь (взгляд запоминается)
//    /delspawn [№]        — убрать точку
//    /spawns              — список точек, /gotospawn [№] — телепорт
// =====================================================================

#define MAX_SPAWNS          20

enum E_SPAWN
{
	spDbId,
	spName[32],
	Float:spX, Float:spY, Float:spZ, Float:spA
}
static gSpawn[MAX_SPAWNS][E_SPAWN];
static gSpawnCount = 0;

stock Spawns_Init()
{
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

// Случайная точка спавна. 0 — точек нет, координаты не тронуты.
stock Spawns_Get(&Float:x, &Float:y, &Float:z, &Float:a)
{
	if (gSpawnCount == 0) return 0;
	new s = random(gSpawnCount);
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
	SendClientMessage(playerid, COLOR_YELLOW, "Точки спавна (игрок без дома и фракции появляется в случайной):");
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
