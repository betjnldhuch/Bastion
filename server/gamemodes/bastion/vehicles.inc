// =====================================================================
//  vehicles.inc — транспорт.
//  * Двигатель заводится вручную: /engine
//  * Общественные машины стоят у спавна
//  * Автосалон: /buycar, своя машина: /park /lock /sellcar
//  * Админ-машины: /veh /dveh (admin.inc)
// =====================================================================

// Автосалон (Coutt & Schutz, Лос-Сантос)
#define DEALER_X            2131.8
#define DEALER_Y            -1150.6
#define DEALER_Z            24.1
#define DEALER_SPAWN_X      2124.0
#define DEALER_SPAWN_Y      -1135.0
#define DEALER_SPAWN_Z      25.3
#define DEALER_SPAWN_A      0.0

#define PERSONAL_RESPAWN    600     // через сколько секунд брошенная машина вернётся на парковку

new gVehOwner[MAX_VEHICLES];        // id аккаунта владельца (0 = общая машина)
new bool:gVehLocked[MAX_VEHICLES];
new bool:gVehAdmin[MAX_VEHICLES];   // создана админом через /veh
new gVehFaction[MAX_VEHICLES];      // машина фракции (FACTION_*, 0 = не фракционная)
new gPlayerVeh[MAX_PLAYERS] = { INVALID_VEHICLE_ID, ... };

static const gDealerModels[] = { 462, 410, 401, 516, 405, 560 };
static const gDealerPrices[] = { 3000, 9000, 12000, 18000, 25000, 60000 };
static const gDealerNames[][] = { "Faggio (мопед)", "Manana", "Bravura", "Nebula", "Sentinel", "Sultan" };

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

	// Автосалон
	CreatePickup(1274, 1, DEALER_X, DEALER_Y, DEALER_Z, 0);
	Create3DTextLabel("{FFFF00}Автосалон\n{FFFFFF}ALT — купить машину", COLOR_WHITE,
		DEALER_X, DEALER_Y, DEALER_Z + 0.6, 15.0, 0, true);
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
}

// Создать личную машину игрока
stock Vehicles_SpawnPersonal(playerid, model, Float:x, Float:y, Float:z, Float:a, c1, c2, bool:locked)
{
	new v = CreateVehicle(model, x, y, z, a, c1, c2, PERSONAL_RESPAWN);
	if (v == INVALID_VEHICLE_ID) return INVALID_VEHICLE_ID;
	Vehicle_Clear(v);
	gVehOwner[v] = Player[playerid][pDbId];
	gVehLocked[v] = locked;
	Vehicle_ApplyLock(v);
	gPlayerVeh[playerid] = v;
	return v;
}

// После входа в аккаунт (accounts.inc)
stock Vehicles_LoadForPlayer(playerid)
{
	new DBResult:r = DB_ExecuteQuery(gDB,
		"SELECT model, x, y, z, a, c1, c2, locked FROM vehicles WHERE owner = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		Vehicles_SpawnPersonal(playerid, DB_GetFieldInt(r, 0),
			DB_GetFieldFloat(r, 1), DB_GetFieldFloat(r, 2), DB_GetFieldFloat(r, 3), DB_GetFieldFloat(r, 4),
			DB_GetFieldInt(r, 5), DB_GetFieldInt(r, 6), DB_GetFieldInt(r, 7) != 0);
	}
	DB_FreeResultSet(r);
}

// При выходе игрока машина пропадает (и появится снова при входе)
stock Vehicles_UnloadForPlayer(playerid)
{
	new v = gPlayerVeh[playerid];
	if (v != INVALID_VEHICLE_ID)
	{
		Vehicle_Clear(v);
		DestroyVehicle(v);
		gPlayerVeh[playerid] = INVALID_VEHICLE_ID;
	}
}

// Вызывается из OnVehicleSpawn: после респавна вернуть замок
stock Vehicles_OnSpawn(vehicleid)
{
	if (gVehOwner[vehicleid] != 0) Vehicle_ApplyLock(vehicleid);
}

// Подсказка при посадке за руль (из OnPlayerStateChange)
stock Vehicles_OnBecomeDriver(playerid)
{
	new v = GetPlayerVehicleID(playerid);
	if (gVehOwner[v] != 0 && gVehOwner[v] == Player[playerid][pDbId])
		SendClientMessage(playerid, COLOR_BLUE, "Это твоя машина. 2 — двигатель, /park — припарковать, /lock — замок.");
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
	SetVehicleParamsEx(v, engine, engine, alarm, doors, bonnet, boot, objective);   // фары вместе с двигателем

	new msg[96];
	if (engine) format(msg, sizeof msg, "%s заводит двигатель.", GetRPName(playerid));
	else format(msg, sizeof msg, "%s глушит двигатель.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_BuyCar(playerid)
{
	if (!IsPlayerInRangeOfPoint(playerid, 3.0, DEALER_X, DEALER_Y, DEALER_Z))
		return SendClientMessage(playerid, COLOR_GREY, "Ты должен быть в автосалоне (значок доллара).");
	if (gPlayerVeh[playerid] != INVALID_VEHICLE_ID)
		return SendClientMessage(playerid, COLOR_GREY, "У тебя уже есть машина. Продать: /sellcar");

	new list[512] = "Модель\tЦена\n";
	new line[64];
	for (new i = 0; i < sizeof (gDealerModels); i++)
	{
		format(line, sizeof line, "%s\t$%d\n", gDealerNames[i], gDealerPrices[i]);
		strcat(list, line);
	}
	ShowPlayerDialog(playerid, DIALOG_BUYCAR, DIALOG_STYLE_TABLIST_HEADERS, "Автосалон", "%s", "Купить", "Отмена", list);
	return true;
}

// Ответ на окно автосалона (из OnDialogResponse)
stock Vehicles_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid != DIALOG_BUYCAR) return 0;
	if (!response || listitem < 0 || listitem >= sizeof (gDealerModels)) return 1;
	if (gPlayerVeh[playerid] != INVALID_VEHICLE_ID) return 1;

	new price = gDealerPrices[listitem];
	if (GetMoney(playerid) < price)
	{
		SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", price);
		return 1;
	}

	new model = gDealerModels[listitem];
	new c1 = random(126), c2 = random(126);
	new v = Vehicles_SpawnPersonal(playerid, model, DEALER_SPAWN_X, DEALER_SPAWN_Y, DEALER_SPAWN_Z, DEALER_SPAWN_A, c1, c2, false);
	if (v == INVALID_VEHICLE_ID)
	{
		SendClientMessage(playerid, COLOR_RED, "Не удалось создать машину (лимит транспорта).");
		return 1;
	}

	GiveMoney(playerid, -price);
	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"INSERT INTO vehicles (owner, model, x, y, z, a, c1, c2) VALUES (%d, %d, %f, %f, %f, %f, %d, %d)",
		Player[playerid][pDbId], model, DEALER_SPAWN_X, DEALER_SPAWN_Y, DEALER_SPAWN_Z, DEALER_SPAWN_A, c1, c2));
	Account_Save(playerid);

	PutPlayerInVehicle(playerid, v, 0);
	SendClientMessage(playerid, COLOR_GREEN, "Ты купил %s за $%d! Припаркуй её в удобном месте: /park", gDealerNames[listitem], price);
	return 1;
}

stock bool:Cmd_Park(playerid)
{
	new v = GetPlayerVehicleID(playerid);
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || v != gPlayerVeh[playerid])
		return SendClientMessage(playerid, COLOR_GREY, "Ты должен сидеть за рулём своей машины.");

	new Float:x, Float:y, Float:z, Float:a;
	GetVehiclePos(v, x, y, z);
	GetVehicleZAngle(v, a);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE vehicles SET x = %f, y = %f, z = %f, a = %f WHERE owner = %d",
		x, y, z, a, Player[playerid][pDbId]));

	// Пересоздаём машину, чтобы она "запомнила" новое место респавна
	new model = GetVehicleModel(v);
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT c1, c2 FROM vehicles WHERE owner = %d", Player[playerid][pDbId]);
	new c1 = -1, c2 = -1;
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			c1 = DB_GetFieldInt(r, 0);
			c2 = DB_GetFieldInt(r, 1);
		}
		DB_FreeResultSet(r);
	}
	new bool:locked = gVehLocked[v];
	Vehicle_Clear(v);
	DestroyVehicle(v);
	gPlayerVeh[playerid] = INVALID_VEHICLE_ID;
	Vehicles_SpawnPersonal(playerid, model, x, y, z, a, c1, c2, locked);

	SendClientMessage(playerid, COLOR_GREEN, "Машина припаркована. Здесь она будет появляться.");
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
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE vehicles SET locked = %d WHERE owner = %d",
		gVehLocked[v] ? 1 : 0, Player[playerid][pDbId]));
	if (gVehLocked[v]) SendClientMessage(playerid, COLOR_WHITE, "Машина {FF6347}закрыта{FFFFFF}.");
	else SendClientMessage(playerid, COLOR_WHITE, "Машина {33AA33}открыта{FFFFFF}.");
	return true;
}

stock bool:Cmd_SellCar(playerid)
{
	new v = gPlayerVeh[playerid];
	if (v == INVALID_VEHICLE_ID) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет машины.");
	if (!IsPlayerInRangeOfPoint(playerid, 3.0, DEALER_X, DEALER_Y, DEALER_Z))
		return SendClientMessage(playerid, COLOR_GREY, "Продать машину можно в автосалоне.");

	new model = GetVehicleModel(v), refund = 1000;
	for (new i = 0; i < sizeof (gDealerModels); i++)
	{
		if (gDealerModels[i] == model) refund = gDealerPrices[i] / 2;
	}
	Vehicles_UnloadForPlayer(playerid);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM vehicles WHERE owner = %d", Player[playerid][pDbId]));
	GiveMoney(playerid, refund);
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Машина продана за $%d.", refund);
	return true;
}
