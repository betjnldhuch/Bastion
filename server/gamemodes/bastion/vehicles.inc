// =====================================================================
//  vehicles.inc — транспорт.
//  * Двигатель заводится вручную: /engine
//  * Общественные машины стоят у спавна
//  * Своя машина: /park /lock. У игрока может быть несколько машин и вертолётов
//    (таблица vehicles, по строке на каждую), но на улице стоит одна — «текущая»:
//    gPlayerVeh — её машина в игре, gPlayerVehDb — её id в базе. Остальные «в гараже»,
//    их вызывают через /cars (dealers.inc).
//  * Автосалоны и авиасалон: /buycar, /sellcar, /cars — dealers.inc
//  * Админ-машины: /veh /dveh (admin.inc)
//  * Бензин, АЗС, спидометр — fuel.inc
//  * СТО (ремонт, тюнинг) и штрафстоянка — garage.inc
// =====================================================================

#define PERSONAL_RESPAWN    600     // через сколько секунд брошенная машина вернётся на парковку

// Функции из fuel.inc, которые нужны здесь раньше, чем он подключён
forward Float:Fuel_Get(vehicleid);
forward bool:Fuel_IsEmpty(vehicleid);
// ...и из factions.inc и admin.inc (для fuel.inc, licenses.inc, jobs.inc)
forward bool:IsPlayerNearPlayer(playerid, target, Float:range);
forward bool:Admin_Check(playerid, level);

new gVehOwner[MAX_VEHICLES];        // id аккаунта владельца (0 = общая машина)
new bool:gVehLocked[MAX_VEHICLES];
new bool:gVehAdmin[MAX_VEHICLES];   // создана админом через /veh
new gVehFaction[MAX_VEHICLES];      // машина фракции (FACTION_*, 0 = не фракционная)
new gVehCargo[MAX_VEHICLES];        // припасы в кузове машины армии (factions.inc, /supply)
new gVehCargoProd[MAX_VEHICLES];    // что в кузове: PROD_* (factions.inc)
new gVehCargoSrc[MAX_VEHICLES];     // откуда груз: 1 — база снабжения, 2 — склад армии
new gPlayerVeh[MAX_PLAYERS] = { INVALID_VEHICLE_ID, ... };    // текущая личная машина игрока в игре
new gPlayerVehDb[MAX_PLAYERS];                                // её id в таблице vehicles (0 = нет)

// ---------------------------------------------------------------------
//  База: раньше у игрока могла быть одна машина (owner ... UNIQUE).
//  Один раз пересобираем таблицу без этого ограничения. Все строки и все
//  поля (тюнинг, бензин, штрафстоянка) переносятся как есть.
// ---------------------------------------------------------------------
static Vehicles_DbCount(const table[])
{
	new n = -1;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM %s", table);
	if (r == DBResult:0) return -1;
	if (DB_GetRowCount(r) > 0) n = DB_GetFieldInt(r, 0);
	DB_FreeResultSet(r);
	return n;
}

static Vehicles_Migrate()
{
	// «текущая» машина игрока: она появляется при входе
	DB_AddColumn("vehicles", "active", "INTEGER DEFAULT 0");

	new sql[1024];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT sql FROM sqlite_master WHERE type = 'table' AND name = 'vehicles'");
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0) DB_GetFieldString(r, 0, sql, sizeof sql);
	DB_FreeResultSet(r);

	new u = strfind(sql, " UNIQUE", true);
	new p = strfind(sql, "(");
	if (u != -1 && p != -1 && strlen(sql) < sizeof (sql) - 1)
	{
		strdel(sql, u, u + 7);
		new before = Vehicles_DbCount("vehicles");
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "DROP TABLE IF EXISTS vehicles_new"));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "BEGIN"));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE vehicles_new %s", sql[p]));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO vehicles_new SELECT * FROM vehicles"));
		if (before < 0 || Vehicles_DbCount("vehicles_new") != before)
		{
			// что-то пошло не так — старая таблица остаётся нетронутой
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "ROLLBACK"));
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "DROP TABLE IF EXISTS vehicles_new"));
			print("[Vehicles] ERROR: vehicles table was NOT migrated, still one vehicle per player.");
			return;
		}
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "DROP TABLE vehicles"));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "ALTER TABLE vehicles_new RENAME TO vehicles"));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE vehicles SET active = 1"));     // до сих пор машина была одна
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "COMMIT"));
		printf("[Vehicles] Table vehicles migrated: several vehicles per player (%d rows kept).", before);
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS idx_vehicles_owner ON vehicles (owner)"));
}

// Сколько у игрока личного транспорта (вместе с тем, что в гараже и на штрафстоянке)
stock Vehicles_CountOwned(playerid)
{
	new n = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM vehicles WHERE owner = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return 0;
	if (DB_GetRowCount(r) > 0) n = DB_GetFieldInt(r, 0);
	DB_FreeResultSet(r);
	return n;
}

// Цена всего личного транспорта игрока по ценам салонов (для налога, economy.inc)
stock Vehicles_GetOwnedValue(playerid)
{
	new value = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT model FROM vehicles WHERE owner = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return 0;
	for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
	{
		value += Vehicles_GetModelPrice(DB_GetFieldInt(r, 0));
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	return value;
}

// ---------------------------------------------------------------------
stock Vehicles_Init()
{
	ManualVehicleEngineAndLights();   // двигатели больше не заводятся сами

	// Общественные машины у спавна (модель, x, y, z, угол, цвет1, цвет2, респавн в сек.)
	AddStaticVehicleEx(492, 2508.7, -1666.5, 13.2, 13.0, -1, -1, 300);   // Greenwood
	AddStaticVehicleEx(567, 2473.6, -1655.8, 13.2, 90.0, -1, -1, 300);   // Savanna
	AddStaticVehicleEx(412, 2484.0, -1655.8, 13.2, 90.0, -1, -1, 300);   // Voodoo
	AddStaticVehicleEx(462, 2497.0, -1680.0, 13.0, 0.0, -1, -1, 300);    // Faggio
	AddStaticVehicleEx(462, 2499.0, -1680.0, 13.0, 0.0, -1, -1, 300);    // Faggio

	Vehicles_Migrate();     // несколько машин у игрока
	Dealers_Init();         // автосалоны и авиасалон (dealers.inc)
}

// Применить "двери закрыты / открыты" к машине
stock Vehicle_ApplyLock(vehicleid)
{
	new engine, lights, alarm, doors, bonnet, boot, objective;
	GetVehicleParamsEx(vehicleid, engine, lights, alarm, doors, bonnet, boot, objective);
	SetVehicleParamsEx(vehicleid, engine, lights, alarm, gVehLocked[vehicleid] ? 1 : 0, bonnet, boot, objective);
}

stock Vehicle_Clear(vehicleid)
{
	gVehOwner[vehicleid] = 0;
	gVehLocked[vehicleid] = false;
	gVehAdmin[vehicleid] = false;
	gVehFaction[vehicleid] = 0;
	gVehCargo[vehicleid] = 0;
	gVehCargoSrc[vehicleid] = 0;
	gVehCargoProd[vehicleid] = 0;
	Fuel_ResetVehicle(vehicleid);     // fuel.inc: новая машина — полный бак
	Garage_ResetVehicle(vehicleid);   // garage.inc: без тюнинга
}

// Создать личную машину игрока (dbid — её строка в vehicles); она становится текущей
stock Vehicles_SpawnPersonal(playerid, dbid, model, Float:x, Float:y, Float:z, Float:a, c1, c2, bool:locked)
{
	new v = CreateVehicle(model, x, y, z, a, c1, c2, PERSONAL_RESPAWN);
	if (v == INVALID_VEHICLE_ID) return INVALID_VEHICLE_ID;
	Vehicle_Clear(v);
	gVehOwner[v] = Player[playerid][pDbId];
	gVehLocked[v] = locked;
	Vehicle_ApplyLock(v);
	gPlayerVeh[playerid] = v;
	gPlayerVehDb[playerid] = dbid;
	Garage_LoadMods(playerid, v);     // покраска и тюнинг (garage.inc)
	return v;
}

// Создать машину dbid из базы (если она этого игрока и не на штрафстоянке), с её бензином
stock Vehicles_SpawnDb(playerid, dbid)
{
	new v = INVALID_VEHICLE_ID;
	new DBResult:r = DB_ExecuteQuery(gDB,
		"SELECT model, x, y, z, a, c1, c2, locked, IFNULL(fuel, -1) FROM vehicles WHERE id = %d AND owner = %d AND impound = 0", dbid, Player[playerid][pDbId]);
	if (r == DBResult:0) return v;
	if (DB_GetRowCount(r) > 0)
	{
		new Float:fuel = DB_GetFieldFloat(r, 8);
		v = Vehicles_SpawnPersonal(playerid, dbid, DB_GetFieldInt(r, 0),
			DB_GetFieldFloat(r, 1), DB_GetFieldFloat(r, 2), DB_GetFieldFloat(r, 3), DB_GetFieldFloat(r, 4),
			DB_GetFieldInt(r, 5), DB_GetFieldInt(r, 6), DB_GetFieldInt(r, 7) != 0);
		if (v != INVALID_VEHICLE_ID && fuel >= 0.0) Fuel_Set(v, fuel);
	}
	DB_FreeResultSet(r);
	return v;
}

// Запомнить, какая машина игрока текущая (она появится при следующем входе)
stock Vehicles_SetActive(playerid, dbid)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE vehicles SET active = (id = %d) WHERE owner = %d", dbid, Player[playerid][pDbId]));
}

// Создать текущую машину игрока из базы. Если отмеченная стоит на штрафстоянке — первую свободную
stock Vehicles_SpawnFromDb(playerid)
{
	new dbid = 0;
	new DBResult:r = DB_ExecuteQuery(gDB,
		"SELECT id FROM vehicles WHERE owner = %d AND impound = 0 ORDER BY active DESC, id LIMIT 1", Player[playerid][pDbId]);
	if (r == DBResult:0) return INVALID_VEHICLE_ID;
	if (DB_GetRowCount(r) > 0) dbid = DB_GetFieldInt(r, 0);
	DB_FreeResultSet(r);
	if (dbid == 0) return INVALID_VEHICLE_ID;
	return Vehicles_SpawnDb(playerid, dbid);
}

// Поменять текущую машину на dbid: прежняя уходит в гараж, эта появляется на своей парковке
stock Vehicles_Summon(playerid, dbid)
{
	Vehicles_UnloadForPlayer(playerid);
	new v = Vehicles_SpawnDb(playerid, dbid);
	if (v != INVALID_VEHICLE_ID) Vehicles_SetActive(playerid, dbid);
	return v;
}

// После входа в аккаунт (accounts.inc)
stock Vehicles_LoadForPlayer(playerid)
{
	Vehicles_SpawnFromDb(playerid);
	Fuel_LoadPlayer(playerid);          // бензин машины и канистра (fuel.inc)
	Licenses_LoadPlayer(playerid);      // лицензии (licenses.inc)
	Jobs_LoadPlayer(playerid);          // стаж на работе (jobs.inc)
	Needs_LoadPlayer(playerid);         // сытость и инвентарь (needs.inc)
	Houses_LoadPlayer(playerid);        // съёмная комната (houses.inc)
	Garage_LoadPlayer(playerid);        // машина на штрафстоянке (garage.inc)
	Drugs_LoadPlayer(playerid);         // зависимость (drugs.inc)
	Prison_LoadPlayer(playerid);        // сидит ли в тюрьме (prison.inc)
	Hospital_LoadPlayer(playerid);      // страховка и медкарта (hospital.inc)
	Marriage_LoadPlayer(playerid);      // супруг (marriage.inc)
	Donate_LoadPlayer(playerid);        // донат-монеты и VIP (donate.inc)
}

// При выходе игрока машина пропадает (и появится снова при входе)
stock Vehicles_UnloadForPlayer(playerid)
{
	new v = gPlayerVeh[playerid];
	if (v != INVALID_VEHICLE_ID)
	{
		Fuel_SavePersonal(playerid);
		Vehicle_Clear(v);
		DestroyVehicle(v);
		gPlayerVeh[playerid] = INVALID_VEHICLE_ID;
	}
	gPlayerVehDb[playerid] = 0;
}

// Вызывается из OnVehicleSpawn: после респавна вернуть замок
stock Vehicles_OnSpawn(vehicleid)
{
	if (gVehOwner[vehicleid] != 0)
	{
		Vehicle_ApplyLock(vehicleid);
		Garage_ApplyMods(vehicleid);    // респавн снимает тюнинг — вешаем обратно
	}
	Dealers_OnVehicleSpawn(vehicleid);  // выставочные машины салона остаются закрытыми
}

// Подсказка при посадке за руль (из OnPlayerStateChange)
stock Vehicles_OnBecomeDriver(playerid)
{
	new v = GetPlayerVehicleID(playerid);
	if (gVehOwner[v] != 0 && gVehOwner[v] == Player[playerid][pDbId])
		SendClientMessage(playerid, COLOR_BLUE, "Это твоя машина. 2 — двигатель, /park — припарковать, /lock — замок, /cars — весь твой транспорт.");
	else
		SendClientMessage(playerid, COLOR_GREY, "Чтобы завести двигатель, нажми 2 (или /engine)");
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Cmd_Engine(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER)
		return SendClientMessage(playerid, COLOR_GREY, "Ты должен сидеть за рулём.");

	new v = GetPlayerVehicleID(playerid);
	new engine, lights, alarm, doors, bonnet, boot, objective;
	GetVehicleParamsEx(v, engine, lights, alarm, doors, bonnet, boot, objective);
	engine = (engine == 1) ? 0 : 1;
	if (engine == 1 && Fuel_IsEmpty(v))
		return SendClientMessage(playerid, COLOR_RED, "Бак пуст. Купи канистру на АЗС (/canister) или вызови помощь.");
	SetVehicleParamsEx(v, engine, engine, alarm, doors, bonnet, boot, objective);   // фары вместе с двигателем

	new msg[96];
	if (engine) format(msg, sizeof msg, "%s заводит двигатель.", GetRPName(playerid));
	else format(msg, sizeof msg, "%s глушит двигатель.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_Park(playerid)
{
	new v = GetPlayerVehicleID(playerid);
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || v != gPlayerVeh[playerid])
		return SendClientMessage(playerid, COLOR_GREY, "Ты должен сидеть за рулём своей машины.");
	new Float:x, Float:y, Float:z, Float:a;
	GetVehicleVelocity(v, x, y, z);
	if (x * x + y * y + z * z > 0.0025) return SendClientMessage(playerid, COLOR_GREY, "Сначала остановись.");

	new dbid = gPlayerVehDb[playerid];
	GetVehiclePos(v, x, y, z);
	GetVehicleZAngle(v, a);
	Fuel_SavePersonal(playerid);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE vehicles SET x = %f, y = %f, z = %f, a = %f, locked = %d WHERE id = %d",
		x, y, z, a, gVehLocked[v] ? 1 : 0, dbid));

	// Пересоздаём машину, чтобы она "запомнила" новое место респавна
	Vehicle_Clear(v);
	DestroyVehicle(v);
	gPlayerVeh[playerid] = INVALID_VEHICLE_ID;
	new nv = Vehicles_SpawnDb(playerid, dbid);
	if (nv != INVALID_VEHICLE_ID) PutPlayerInVehicle(playerid, nv, 0);

	SendClientMessage(playerid, COLOR_GREEN, "Транспорт припаркован. Здесь он будет появляться.");
	return true;
}

stock bool:Vehicles_ToggleLock(playerid)
{
	new v = gPlayerVeh[playerid];
	if (v == INVALID_VEHICLE_ID) return SendClientMessage(playerid, COLOR_GREY, "Рядом нет твоего дома или машины.");

	new Float:x, Float:y, Float:z;
	GetVehiclePos(v, x, y, z);
	if (!IsPlayerInRangeOfPoint(playerid, 6.0, x, y, z))
		return SendClientMessage(playerid, COLOR_GREY, "Твоя машина слишком далеко.");

	gVehLocked[v] = !gVehLocked[v];
	Vehicle_ApplyLock(v);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE vehicles SET locked = %d WHERE id = %d",
		gVehLocked[v] ? 1 : 0, gPlayerVehDb[playerid]));
	if (gVehLocked[v]) SendClientMessage(playerid, COLOR_WHITE, "Машина {FF6347}закрыта{FFFFFF}.");
	else SendClientMessage(playerid, COLOR_WHITE, "Машина {33AA33}открыта{FFFFFF}.");
	return true;
}
