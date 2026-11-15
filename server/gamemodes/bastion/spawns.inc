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
//
//  Дополнительные спавны фракций (секретный офис ФБР, подразделения и т.п.):
//    админ 4+: /addfspawn [фракция] [название] — там, где стоишь (можно в интерьере), /delfspawn [№]
//    лидер (10) и зам (9): /fspawnlist, /fspawnaccess [id] [номера через запятую], /fspawnsquad [№] [взвод]
//  Игрок выбирает в /setspawn > Организация: главный офис или открытые ему спавны.
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
#define MAX_FSPAWNS         60      // доп. спавнов фракций на весь сервер
#define FSPAWN_PICK_MAX     8       // сколько своих доп. спавнов показывать в /setspawn
enum E_FSPAWN
{
	fsDbId,
	fsFaction,
	fsSquad,                // взвод, которому спавн открыт сам (0 — никому)
	fsName[32],
	Float:fsX, Float:fsY, Float:fsZ, Float:fsA,
	fsInt, fsWorld
}
static gFSpawn[MAX_FSPAWNS][E_FSPAWN];
static gFSpawnCount = 0;

// строка окна -> { режим, ref }; +3: вокзал, дом, база фракции
static gSpawnPick[MAX_PLAYERS][MAX_SPAWNS + 3][2];

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

	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS faction_spawns (id INTEGER PRIMARY KEY AUTOINCREMENT, faction INTEGER, rank INTEGER DEFAULT 1, name TEXT, x REAL, y REAL, z REAL, a REAL, interior INTEGER DEFAULT 0, world INTEGER DEFAULT 0)"));
	DB_AddColumn("faction_spawns", "squad", "INTEGER DEFAULT 0");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS fspawn_access (account INTEGER NOT NULL, spawn INTEGER NOT NULL, allow INTEGER DEFAULT 1, PRIMARY KEY (account, spawn))"));
	r = DB_ExecuteQuery(gDB, "SELECT id, faction, squad, name, x, y, z, a, interior, world FROM faction_spawns ORDER BY id");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows && gFSpawnCount < MAX_FSPAWNS; i++)
		{
			new s = gFSpawnCount++;
			gFSpawn[s][fsDbId] = DB_GetFieldInt(r, 0);
			gFSpawn[s][fsFaction] = DB_GetFieldInt(r, 1);
			gFSpawn[s][fsSquad] = DB_GetFieldInt(r, 2);
			DB_GetFieldString(r, 3, gFSpawn[s][fsName], 32);
			gFSpawn[s][fsX] = DB_GetFieldFloat(r, 4);
			gFSpawn[s][fsY] = DB_GetFieldFloat(r, 5);
			gFSpawn[s][fsZ] = DB_GetFieldFloat(r, 6);
			gFSpawn[s][fsA] = DB_GetFieldFloat(r, 7);
			gFSpawn[s][fsInt] = DB_GetFieldInt(r, 8);
			gFSpawn[s][fsWorld] = DB_GetFieldInt(r, 9);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Spawns] Loaded %d faction spawn points.", gFSpawnCount);
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
//  Дополнительные спавны фракций
//
//  По умолчанию у всех только главный офис (/setfspawn). Доп. спавны:
//    - лидер (ранг 10) видит все спавны своей фракции;
//    - остальным, и заму (9) тоже, их открывает лидер: /fspawnaccess [id] [номера через запятую];
//    - зам (9) раздаёт рангам ниже себя только то, что открыто ему самому:
//      лидер закрыл заму спавн — зам его больше не видит и не может раздать;
//    - спавн может сам открываться взводу: /fspawnsquad [№] [взвод],
//      но личный список игрока (если задан) важнее.
//  Номера спавнов: /fspawnlist (лидер и зам).
// ---------------------------------------------------------------------
#define FSPAWN_LEADER_RANK  10
#define FSPAWN_DEPUTY_RANK  9
#define FSPAWN_LIST_MAX     16

static gSetSpawnFMap[MAX_PLAYERS][FSPAWN_PICK_MAX + 1];

static FSpawn_FindByDbId(dbid)
{
	for (new s = 0; s < gFSpawnCount; s++) if (gFSpawn[s][fsDbId] == dbid) return s;
	return -1;
}

// Личная настройка: 1 — открыт, 0 — закрыт, -1 — не задано (тогда по взводу)
static FSpawn_PersonalAccess(playerid, s)
{
	new res = -1;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT allow FROM fspawn_access WHERE account = %d AND spawn = %d",
		Player[playerid][pDbId], gFSpawn[s][fsDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) res = DB_GetFieldInt(r, 0) ? 1 : 0;
		DB_FreeResultSet(r);
	}
	return res;
}

static bool:FSpawn_Allowed(playerid, s)
{
	if (Player[playerid][pFaction] != gFSpawn[s][fsFaction]) return false;
	if (Player[playerid][pRank] >= FSPAWN_LEADER_RANK) return true;
	new p = FSpawn_PersonalAccess(playerid, s);
	if (p != -1) return (p == 1);
	return (gFSpawn[s][fsSquad] > 0 && Squad_Of(playerid) == gFSpawn[s][fsSquad]);
}

// Может ли игрок раздавать спавн s (и менять ему взвод): админ 4+, лидер — все, зам — только открытые ему
static bool:FSpawn_CanManage(playerid, s)
{
	if (Player[playerid][pAdmin] >= 4) return true;
	if (Player[playerid][pFaction] != gFSpawn[s][fsFaction]) return false;
	if (Player[playerid][pRank] >= FSPAWN_LEADER_RANK) return true;
	return (Player[playerid][pRank] >= FSPAWN_DEPUTY_RANK && FSpawn_Allowed(playerid, s));
}

// Выбранный в /setspawn доп. спавн фракции. 0 — не выбран, убран или нет доступа (тогда — главный офис).
stock Spawns_GetFactionExtra(playerid, &Float:x, &Float:y, &Float:z, &Float:a, &interior, &world)
{
	if (gSpawnMode[playerid] != SPAWN_FACTION || gSpawnRef[playerid] <= 0) return 0;
	new s = FSpawn_FindByDbId(gSpawnRef[playerid]);
	if (s == -1 || !FSpawn_Allowed(playerid, s)) return 0;
	x = gFSpawn[s][fsX]; y = gFSpawn[s][fsY]; z = gFSpawn[s][fsZ]; a = gFSpawn[s][fsA];
	interior = gFSpawn[s][fsInt];
	world = gFSpawn[s][fsWorld];
	return 1;
}

stock bool:Cmd_AddFSpawn(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f, name[32];
	if (!NextIntParam(params, pos, f) || !RestParam(params, pos, name) || !Faction_IsValid(f))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /addfspawn [фракция] [название] — доп. спавн фракции там, где стоишь");
	if (gFSpawnCount >= MAX_FSPAWNS) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит доп. спавнов фракций (%d).", MAX_FSPAWNS);
	new mine = 0;
	for (new i = 0; i < gFSpawnCount; i++) if (gFSpawn[i][fsFaction] == f) mine++;
	if (mine >= FSPAWN_PICK_MAX) return SendClientMessage(playerid, COLOR_GREY, "У фракции уже %d доп. спавнов, больше нельзя.", FSPAWN_PICK_MAX);

	new s = gFSpawnCount++;
	gFSpawn[s][fsFaction] = f;
	gFSpawn[s][fsSquad] = 0;
	gFSpawn[s][fsName][0] = '\0';
	strcat(gFSpawn[s][fsName], name, 32);
	GetPlayerPos(playerid, gFSpawn[s][fsX], gFSpawn[s][fsY], gFSpawn[s][fsZ]);
	GetPlayerFacingAngle(playerid, gFSpawn[s][fsA]);
	gFSpawn[s][fsInt] = GetPlayerInterior(playerid);
	gFSpawn[s][fsWorld] = GetPlayerVirtualWorld(playerid);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO faction_spawns (faction, squad, name, x, y, z, a, interior, world) VALUES (%d, 0, '%q', %f, %f, %f, %f, %d, %d)",
		f, name, gFSpawn[s][fsX], gFSpawn[s][fsY], gFSpawn[s][fsZ], gFSpawn[s][fsA], gFSpawn[s][fsInt], gFSpawn[s][fsWorld]));
	gFSpawn[s][fsDbId] = DB_LastInsertId();
	SendClientMessage(playerid, COLOR_ADMIN, "Доп. спавн №%d «%s» для «%s» создан. Его видит только лидер, остальным открывает он: /fspawnaccess [id] %d",
		gFSpawn[s][fsDbId], name, Faction_GetName(f), gFSpawn[s][fsDbId]);
	return true;
}

stock bool:Cmd_DelFSpawn(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, dbid;
	if (!NextIntParam(params, pos, dbid)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /delfspawn [№] (номера: /fspawns)");
	new s = FSpawn_FindByDbId(dbid);
	if (s == -1) return SendClientMessage(playerid, COLOR_GREY, "Доп. спавна с таким номером нет. Список: /fspawns");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM faction_spawns WHERE id = %d", dbid));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM fspawn_access WHERE spawn = %d", dbid));
	SendClientMessage(playerid, COLOR_ADMIN, "Доп. спавн №%d «%s» убран. Кто его выбрал, появится в главном офисе.", dbid, gFSpawn[s][fsName]);
	new last = gFSpawnCount - 1;
	if (s != last) gFSpawn[s] = gFSpawn[last];
	gFSpawnCount--;
	return true;
}

// /fspawnlist (/fspawns) — номера спавнов. Админ 3+ — все (или одной фракции), лидер — все свои, зам — открытые ему.
stock bool:Cmd_FSpawns(playerid, const params[])
{
	new pos = 0, f = 0;
	NextIntParam(params, pos, f);
	new bool:admin = (Player[playerid][pAdmin] >= 3);
	if (!admin)
	{
		if (Player[playerid][pRank] < FSPAWN_DEPUTY_RANK || !Faction_IsValid(Player[playerid][pFaction]))
			return SendClientMessage(playerid, COLOR_GREY, "Доступно лидеру и заму фракции (ранг %d+).", FSPAWN_DEPUTY_RANK);
		f = Player[playerid][pFaction];
	}
	new shown = 0, sq[24];
	SendClientMessage(playerid, COLOR_YELLOW, "Спавны организации (главный офис есть у всех):");
	for (new s = 0; s < gFSpawnCount; s++)
	{
		if (f > 0 && gFSpawn[s][fsFaction] != f) continue;
		if (!admin && !FSpawn_Allowed(playerid, s)) continue;
		sq[0] = '\0';
		if (gFSpawn[s][fsSquad] > 0) format(sq, sizeof sq, ", сам открыт взводу %d", gFSpawn[s][fsSquad]);
		if (admin) SendClientMessage(playerid, COLOR_WHITE, " №%d — %s: %s%s", gFSpawn[s][fsDbId], Faction_GetName(gFSpawn[s][fsFaction]), gFSpawn[s][fsName], sq);
		else SendClientMessage(playerid, COLOR_WHITE, " №%d — %s%s", gFSpawn[s][fsDbId], gFSpawn[s][fsName], sq);
		shown++;
	}
	if (shown == 0) SendClientMessage(playerid, COLOR_GREY, "Доп. спавнов нет. Их ставит админ: /addfspawn [фракция] [название]");
	else SendClientMessage(playerid, COLOR_GREY, "Открыть игроку: /fspawnaccess [id] [номера через запятую], 0 — только главный, auto — по взводу.");
	return true;
}

// /fspawnsquad [№] [взвод|0] — спавн сам открывается этому взводу
stock bool:Cmd_FSpawnSquad(playerid, const params[])
{
	new pos = 0, dbid, q;
	if (!NextIntParam(params, pos, dbid) || !NextIntParam(params, pos, q) || q < 0 || q > Squads_Max())
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /fspawnsquad [№ спавна] [взвод 1-%d, 0 — никому]. Номера: /fspawnlist", Squads_Max());
	new s = FSpawn_FindByDbId(dbid);
	if (s == -1 || !FSpawn_CanManage(playerid, s)) return SendClientMessage(playerid, COLOR_GREY, "Такого спавна у тебя нет. Список: /fspawnlist");
	gFSpawn[s][fsSquad] = q;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE faction_spawns SET squad = %d WHERE id = %d", q, dbid));
	if (q == 0) SendClientMessage(playerid, COLOR_GREEN, "Спавн «%s» больше не открывается взводу сам.", gFSpawn[s][fsName]);
	else SendClientMessage(playerid, COLOR_GREEN, "Спавн «%s» теперь сам открыт взводу %d (кроме тех, кому задан личный список).", gFSpawn[s][fsName], q);
	return true;
}

// Номера из строки «3,5,7» -> list; возвращает количество
static FSpawn_ParseList(const s[], list[], max)
{
	new n = 0, v = 0, bool:num = false;
	for (new i = 0; ; i++)
	{
		if (s[i] >= '0' && s[i] <= '9') { if (v < 1000000) v = v * 10 + (s[i] - '0'); num = true; continue; }
		if (num && v > 0 && n < max) list[n++] = v;
		v = 0; num = false;
		if (s[i] == '\0') break;
	}
	return n;
}

// /fspawnaccess [id] [номера через запятую | 0 | auto] — какие доп. спавны открыты игроку
stock bool:Cmd_FSpawnAccess(playerid, const params[])
{
	new pos = 0;
	new t = NextPlayerParam(params, pos);
	if (t == INVALID_PLAYER_ID || !Player[t][pLogged])
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /fspawnaccess [id] [номера через запятую] — 0: только главный офис, auto: по взводу. Номера: /fspawnlist");
	new f = Player[t][pFaction];
	if (!Faction_IsValid(f)) return SendClientMessage(playerid, COLOR_GREY, "Игрок не состоит во фракции.");
	new bool:admin = (Player[playerid][pAdmin] >= 4);
	if (!admin)
	{
		if (Player[playerid][pFaction] != f || Player[playerid][pRank] < FSPAWN_DEPUTY_RANK)
			return SendClientMessage(playerid, COLOR_GREY, "Это может лидер или зам его фракции.");
		if (Player[t][pRank] >= Player[playerid][pRank])
			return SendClientMessage(playerid, COLOR_GREY, "Спавны можно раздавать только рангам ниже себя.");
	}
	if (Player[t][pRank] >= FSPAWN_LEADER_RANK) return SendClientMessage(playerid, COLOR_GREY, "Лидер и так видит все спавны фракции.");

	new rest[96];
	RestParam(params, pos, rest);
	if (rest[0] == '\0')
	{
		// показать, что открыто сейчас
		new cur[96], part[12];
		for (new s = 0; s < gFSpawnCount; s++)
		{
			if (gFSpawn[s][fsFaction] != f || !FSpawn_Allowed(t, s)) continue;
			if (cur[0] != '\0') strcat(cur, ",");
			valstr(part, gFSpawn[s][fsDbId]);
			strcat(cur, part);
		}
		if (cur[0] == '\0') strcat(cur, "только главный офис");
		SendClientMessage(playerid, COLOR_YELLOW, "%s: открыты спавны %s. Изменить: /fspawnaccess %d [номера через запятую]", GetRPName(t), cur, t);
		return true;
	}

	new bool:autoMode = (!strcmp(rest, "auto", true) || !strcmp(rest, "авто", true));
	new list[FSPAWN_LIST_MAX], n = autoMode ? 0 : FSpawn_ParseList(rest, list, sizeof list);
	new changed = 0, skipped = 0;
	for (new k = 0; k < n; k++)
	{
		new s = FSpawn_FindByDbId(list[k]);
		if (s == -1 || gFSpawn[s][fsFaction] != f || !FSpawn_CanManage(playerid, s)) skipped++;
	}
	for (new s = 0; s < gFSpawnCount; s++)
	{
		if (gFSpawn[s][fsFaction] != f || !FSpawn_CanManage(playerid, s)) continue;    // чужое заму не трогаем
		if (autoMode)
		{
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM fspawn_access WHERE account = %d AND spawn = %d", Player[t][pDbId], gFSpawn[s][fsDbId]));
		}
		else
		{
			new bool:on = false;
			for (new k = 0; k < n; k++) if (list[k] == gFSpawn[s][fsDbId]) { on = true; break; }
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO fspawn_access (account, spawn, allow) VALUES (%d, %d, %d)",
				Player[t][pDbId], gFSpawn[s][fsDbId], on ? 1 : 0));
		}
		changed++;
	}
	if (changed == 0) return SendClientMessage(playerid, COLOR_GREY, "Нечего менять: доступных тебе спавнов нет. Список: /fspawnlist");
	if (skipped > 0) SendClientMessage(playerid, COLOR_GREY, "Пропущено номеров: %d (нет такого спавна или он закрыт тебе).", skipped);
	if (autoMode)
	{
		SendClientMessage(playerid, COLOR_GREEN, "%s: спавны снова по взводу.", GetRPName(t));
		SendClientMessage(t, COLOR_YELLOW, "%s изменил твои спавны организации: /setspawn > Организация.", GetRPName(playerid));
	}
	else
	{
		SendClientMessage(playerid, COLOR_GREEN, "%s: список спавнов сохранён.", GetRPName(t));
		SendClientMessage(t, COLOR_YELLOW, "%s изменил твои спавны организации: /setspawn > Организация.", GetRPName(playerid));
	}
	return true;
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
	if (House_GetOwned(playerid) != -1 || House_GetRented(playerid) != -1 || Family_SpouseHouse(playerid) != -1 || Family_HouseOf(playerid) != -1)
	{
		gSpawnPick[playerid][n][0] = SPAWN_HOUSE; gSpawnPick[playerid][n][1] = 0; n++;
		strcat(list, "{33CCFF}У своего дома\n");
	}
	if (Player[playerid][pFaction] != FACTION_NONE)
	{
		gSpawnPick[playerid][n][0] = SPAWN_FACTION; gSpawnPick[playerid][n][1] = -1; n++;    // -1: окно спавнов организации
		strcat(list, "{FFFF66}Организация\n");
	}
	for (new s = 0; s < gSpawnCount; s++)
	{
		if (n >= sizeof (gSpawnPick[])) break;
		gSpawnPick[playerid][n][0] = SPAWN_STATION; gSpawnPick[playerid][n][1] = gSpawn[s][spDbId]; n++;
		format(line, sizeof line, "%s\n", gSpawn[s][spName]);
		strcat(list, line);
	}
	for (new i = n; i < sizeof (gSpawnPick[]); i++) gSpawnPick[playerid][i][0] = -1;
	ShowPlayerDialog(playerid, DIALOG_SETSPAWN, DIALOG_STYLE_LIST, "Где появляться", "%s", "Выбрать", "Закрыть", list);
	return 1;
}

// /setspawn > Организация: главный офис и доп. спавны, открытые игроку
static Spawns_ShowFaction(playerid)
{
	new list[512], line[64], n = 0;
	strcat(list, "{FFFF66}Главный офис\n");
	gSetSpawnFMap[playerid][n++] = 0;
	for (new s = 0; s < gFSpawnCount && n <= FSPAWN_PICK_MAX; s++)
	{
		if (!FSpawn_Allowed(playerid, s)) continue;
		format(line, sizeof line, "{FFFFFF}%s\n", gFSpawn[s][fsName]);
		strcat(list, line);
		gSetSpawnFMap[playerid][n++] = gFSpawn[s][fsDbId];
	}
	for (new i = n; i <= FSPAWN_PICK_MAX; i++) gSetSpawnFMap[playerid][i] = -1;
	ShowPlayerDialog(playerid, DIALOG_SETSPAWN_F, DIALOG_STYLE_LIST, "Спавн организации", "%s", "Выбрать", "Назад", list);
}

static Spawns_Choose(playerid, mode, ref)
{
	gSpawnMode[playerid] = mode;
	gSpawnRef[playerid] = ref;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET spawn_mode = %d, spawn_ref = %d WHERE id = %d",
		gSpawnMode[playerid], gSpawnRef[playerid], Player[playerid][pDbId]));
	Account_SetNormalSpawn(playerid);
	switch (mode)
	{
		case SPAWN_HOUSE:   SendClientMessage(playerid, COLOR_GREEN, "Теперь ты появляешься у своего дома.");
		case SPAWN_FACTION:
		{
			new s = (ref > 0) ? FSpawn_FindByDbId(ref) : -1;
			if (s != -1) SendClientMessage(playerid, COLOR_GREEN, "Теперь ты появляешься здесь: %s.", gFSpawn[s][fsName]);
			else SendClientMessage(playerid, COLOR_GREEN, "Теперь ты появляешься в главном офисе организации.");
		}
		case SPAWN_STATION:
		{
			new s = Spawn_FindByDbId(ref);
			if (s != -1) SendClientMessage(playerid, COLOR_GREEN, "Теперь ты появляешься здесь: %s.", gSpawn[s][spName]);
		}
		case SPAWN_ANYSTATION: SendClientMessage(playerid, COLOR_GREEN, "Теперь ты появляешься на вокзале, где меньше людей.");
		default: SendClientMessage(playerid, COLOR_GREEN, "Место появления выбрано.");
	}
}

// Из OnDialogResponse
stock Spawns_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid == DIALOG_SETSPAWN_F)
	{
		if (!response) { Cmd_SetSpawn(playerid); return 1; }
		if (listitem < 0 || listitem > FSPAWN_PICK_MAX || gSetSpawnFMap[playerid][listitem] == -1) return 1;
		new ref = gSetSpawnFMap[playerid][listitem];
		if (ref > 0)
		{
			new s = FSpawn_FindByDbId(ref);
			if (s == -1 || !FSpawn_Allowed(playerid, s)) return SendClientMessage(playerid, COLOR_GREY, "Этот спавн тебе больше недоступен.");
		}
		Spawns_Choose(playerid, SPAWN_FACTION, ref);
		return 1;
	}
	if (dialogid != DIALOG_SETSPAWN) return 0;
	if (!response || listitem < 0 || listitem >= sizeof (gSpawnPick[]) || gSpawnPick[playerid][listitem][0] == -1) return 1;
	if (gSpawnPick[playerid][listitem][0] == SPAWN_FACTION)
	{
		Spawns_ShowFaction(playerid);
		return 1;
	}
	Spawns_Choose(playerid, gSpawnPick[playerid][listitem][0], gSpawnPick[playerid][listitem][1]);
	return 1;
}
