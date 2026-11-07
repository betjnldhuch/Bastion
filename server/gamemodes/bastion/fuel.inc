// =====================================================================
//  fuel.inc — топливо, АЗС и канистры.
//
//  У каждой машины есть бак на FUEL_TANK литров. Бензин тратится, пока
//  работает двигатель: на холостых чуть-чуть, на скорости больше.
//  Пустой бак — двигатель глохнет и не заводится.
//
//  Игрокам:
//    H в машине на АЗС (или /fill) — заправиться (заглуши двигатель)
//    ALT пешком на АЗС            — купить канистру на FUEL_CANISTER л
//    /canister                     — залить канистру в машину рядом
//    /fuel                         — сколько бензина в баке
//    /speedo                       — спрятать / показать спидометр
//  Админам:
//    /addfuel [цена за литр] (4+) — АЗС там, где стоишь
//    /delfuel (4+)                — удалить ближайшую АЗС
//    /setfuel [литры] (3+)        — заправить машину, в которой сидишь
//
//  Личная машина помнит бензин в таблице vehicles (колонка fuel),
//  канистра игрока — в accounts (колонка canister).
// =====================================================================

#define MAX_FUEL_STATIONS   40
#define FUEL_STATION_RANGE  10.0    // радиус АЗС для заправки из машины
#define FUEL_IDLE_PER_SEC   0.003   // литров в секунду на холостых
#define FUEL_SPEED_FACTOR   0.0004  // + литров в секунду за каждый км/ч
#define FUEL_LOW_WARNING    8.0     // предупреждение «бензин заканчивается»

#define DIALOG_FUEL_FILL    1100
#define DIALOG_FUEL_CAN     1101

enum E_FUEL_STATION
{
	bool:fsUsed,
	fsDbId,
	Float:fsX,
	Float:fsY,
	Float:fsZ,
	fsPrice,
	fsPickup,
	Text3D:fsLabel
}
new FuelStation[MAX_FUEL_STATIONS][E_FUEL_STATION];

static Float:gVehFuel[MAX_VEHICLES] = { -1.0, ... };    // -1 = полный бак
static bool:gVehFuelWarned[MAX_VEHICLES];
static gPlayerCanister[MAX_PLAYERS];
static gFuelStationFor[MAX_PLAYERS] = { -1, ... };      // АЗС, на которой открыто окно
static PlayerText:gSpeedo[MAX_PLAYERS] = { INVALID_PLAYER_TEXT_DRAW, ... };
static bool:gSpeedoHidden[MAX_PLAYERS];

// Стандартные АЗС San Andreas — создаются при первом запуске
static const Float:gDefaultStations[][3] =
{
	{ 1941.6, -1772.9, 13.6 },    // Idlewood, Лос-Сантос
	{ 1004.0, -939.3, 42.2 },     // Mulholland, Лос-Сантос
	{ -90.5, -1169.4, 2.4 },      // Flint County
	{ 655.7, -565.2, 16.3 },      // Dillimore
	{ 1381.9, 459.1, 20.0 },      // Montgomery
	{ -2244.0, -2560.6, 32.0 },   // Angel Pine
	{ -1676.0, 414.0, 7.2 },      // Easter Basin, Сан-Фиерро
	{ -2410.8, 976.2, 45.3 },     // Juniper Hollow, Сан-Фиерро
	{ -1328.0, 2677.5, 50.0 },    // Tierra Robada
	{ 70.4, 1218.6, 18.8 },       // Fort Carson
	{ 611.6, 1694.9, 7.0 },       // Bone County
	{ 2114.9, 920.0, 10.8 },      // Лас-Вентурас, юг
	{ 2640.0, 1106.0, 10.8 },     // Лас-Вентурас, восток
	{ 2202.2, 2474.3, 10.8 }      // Лас-Вентурас, Come-A-Lot
};

// ---------------------------------------------------------------------
//  Бак машины
// ---------------------------------------------------------------------
stock bool:Fuel_VehicleUsesFuel(vehicleid)
{
	switch (GetVehicleModel(vehicleid))
	{
		case 0, 481, 509, 510: return false;     // велосипеды
	}
	return true;
}

stock Float:Fuel_Get(vehicleid)
{
	if (vehicleid < 1 || vehicleid >= MAX_VEHICLES) return 0.0;
	if (gVehFuel[vehicleid] < 0.0) return float(FUEL_TANK);
	return gVehFuel[vehicleid];
}

stock Fuel_Set(vehicleid, Float:liters)
{
	if (vehicleid < 1 || vehicleid >= MAX_VEHICLES) return;
	if (liters < 0.0) liters = 0.0;
	if (liters > float(FUEL_TANK)) liters = float(FUEL_TANK);
	gVehFuel[vehicleid] = liters;
	if (liters > FUEL_LOW_WARNING) gVehFuelWarned[vehicleid] = false;
}

// Вызывается из Vehicle_Clear (vehicles.inc): новая машина — полный бак
stock Fuel_ResetVehicle(vehicleid)
{
	if (vehicleid < 1 || vehicleid >= MAX_VEHICLES) return;
	gVehFuel[vehicleid] = -1.0;
	gVehFuelWarned[vehicleid] = false;
}

stock bool:Fuel_IsEmpty(vehicleid)
{
	return Fuel_VehicleUsesFuel(vehicleid) && Fuel_Get(vehicleid) < 0.1;
}

// Из OnVehicleSpawn: общие машины появляются с неполным баком
stock Fuel_OnVehicleSpawn(vehicleid)
{
	if (gVehOwner[vehicleid] != 0) return;      // личная машина помнит свой бензин
	if (gVehFaction[vehicleid] != 0 || gVehAdmin[vehicleid]) Fuel_ResetVehicle(vehicleid);
	else Fuel_Set(vehicleid, float(FUEL_TANK / 2 + random(FUEL_TANK / 2 + 1)));
}

stock Fuel_SavePersonal(playerid)
{
	new v = gPlayerVeh[playerid];
	if (v == INVALID_VEHICLE_ID || Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE vehicles SET fuel = %f WHERE owner = %d",
		Fuel_Get(v), Player[playerid][pDbId]));
}

// Из Vehicles_LoadForPlayer: бензин личной машины и канистра
stock Fuel_LoadPlayer(playerid)
{
	gPlayerCanister[playerid] = 0;
	gSpeedoHidden[playerid] = false;
	gFuelStationFor[playerid] = -1;

	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT canister FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) gPlayerCanister[playerid] = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}

	new v = gPlayerVeh[playerid];
	if (v == INVALID_VEHICLE_ID) return;
	r = DB_ExecuteQuery(gDB, "SELECT fuel FROM vehicles WHERE owner = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			new Float:f = DB_GetFieldFloat(r, 0);
			if (f >= 0.0) Fuel_Set(v, f);
		}
		DB_FreeResultSet(r);
	}
}

static Fuel_SaveCanister(playerid)
{
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET canister = %d WHERE id = %d",
		gPlayerCanister[playerid], Player[playerid][pDbId]));
}

// ---------------------------------------------------------------------
//  АЗС
// ---------------------------------------------------------------------
static Fuel_UpdateLabel(s)
{
	if (FuelStation[s][fsLabel] != Text3D:INVALID_3DTEXT_ID) Delete3DTextLabel(FuelStation[s][fsLabel]);
	FuelStation[s][fsLabel] = Create3DTextLabel(
		"{FFCC00}АЗС\n{FFFFFF}Бензин: ${FFFF00}%d{FFFFFF} за литр\nH в машине — заправка\nALT — канистра (%d л)",
		COLOR_WHITE, FuelStation[s][fsX], FuelStation[s][fsY], FuelStation[s][fsZ] + 0.8, 25.0, 0, true,
		FuelStation[s][fsPrice], FUEL_CANISTER);
}

static Fuel_AddStation(dbid, Float:x, Float:y, Float:z, price)
{
	for (new s = 0; s < MAX_FUEL_STATIONS; s++)
	{
		if (FuelStation[s][fsUsed]) continue;
		FuelStation[s][fsUsed] = true;
		FuelStation[s][fsDbId] = dbid;
		FuelStation[s][fsX] = x;
		FuelStation[s][fsY] = y;
		FuelStation[s][fsZ] = z;
		FuelStation[s][fsPrice] = price;
		FuelStation[s][fsPickup] = CreatePickup(1650, 1, x, y, z, 0);    // канистра
		FuelStation[s][fsLabel] = Text3D:INVALID_3DTEXT_ID;
		Fuel_UpdateLabel(s);
		return s;
	}
	return -1;
}

stock Fuel_GetNearbyStation(playerid, Float:range = FUEL_STATION_RANGE)
{
	if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return -1;
	for (new s = 0; s < MAX_FUEL_STATIONS; s++)
	{
		if (FuelStation[s][fsUsed] && IsPlayerInRangeOfPoint(playerid, range, FuelStation[s][fsX], FuelStation[s][fsY], FuelStation[s][fsZ]))
			return s;
	}
	return -1;
}

forward Fuel_OnSecond();

stock Fuel_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS fuel_stations (id INTEGER PRIMARY KEY AUTOINCREMENT, x REAL, y REAL, z REAL, price INTEGER)"));
	DB_AddColumn("vehicles", "fuel", "REAL DEFAULT -1");
	DB_AddColumn("accounts", "canister", "INTEGER DEFAULT 0");

	new count = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM fuel_stations");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (count == 0)
	{
		for (new i = 0; i < sizeof (gDefaultStations); i++)
		{
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO fuel_stations (x, y, z, price) VALUES (%f, %f, %f, %d)",
				gDefaultStations[i][0], gDefaultStations[i][1], gDefaultStations[i][2], FUEL_PRICE));
		}
	}

	r = DB_ExecuteQuery(gDB, "SELECT id, x, y, z, price FROM fuel_stations");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			Fuel_AddStation(DB_GetFieldInt(r, 0), DB_GetFieldFloat(r, 1), DB_GetFieldFloat(r, 2),
				DB_GetFieldFloat(r, 3), DB_GetFieldInt(r, 4));
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}

	// Машины, созданные до нас (общественные, фракционные), получают бензин
	for (new v = 1; v < MAX_VEHICLES; v++)
	{
		if (IsValidVehicle(v)) Fuel_OnVehicleSpawn(v);
	}
	SetTimer("Fuel_OnSecond", 1000, true);
}

// При выключении сервера — сохранить бензин личных машин
stock Fuel_Exit()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged]) Fuel_SavePersonal(i);
	}
}

// ---------------------------------------------------------------------
//  Спидометр и расход (раз в секунду)
// ---------------------------------------------------------------------
stock Fuel_GetSpeed(vehicleid)
{
	new Float:vx, Float:vy, Float:vz;
	GetVehicleVelocity(vehicleid, vx, vy, vz);
	return floatround(floatsqroot(vx * vx + vy * vy + vz * vz) * 180.0);    // км/ч
}

static Fuel_ShowSpeedo(playerid, speed, Float:fuel, bool:usesFuel)
{
	if (gSpeedo[playerid] == INVALID_PLAYER_TEXT_DRAW)
	{
		gSpeedo[playerid] = CreatePlayerTextDraw(playerid, 630.0, 400.0, "_");
		PlayerTextDrawAlignment(playerid, gSpeedo[playerid], TEXT_DRAW_ALIGN_RIGHT);
		PlayerTextDrawFont(playerid, gSpeedo[playerid], TEXT_DRAW_FONT_1);
		PlayerTextDrawLetterSize(playerid, gSpeedo[playerid], 0.3, 1.3);
		PlayerTextDrawSetOutline(playerid, gSpeedo[playerid], 1);
		PlayerTextDrawSetProportional(playerid, gSpeedo[playerid], true);
	}
	new text[64];
	if (!usesFuel) format(text, sizeof text, "~w~%d ~y~km/h", speed);
	else if (fuel < FUEL_LOW_WARNING) format(text, sizeof text, "~w~%d ~y~km/h~n~~r~Fuel %d / %d L", speed, floatround(fuel, floatround_floor), FUEL_TANK);
	else format(text, sizeof text, "~w~%d ~y~km/h~n~~w~Fuel ~g~%d ~w~/ %d L", speed, floatround(fuel, floatround_floor), FUEL_TANK);
	PlayerTextDrawSetString(playerid, gSpeedo[playerid], text);
	PlayerTextDrawShow(playerid, gSpeedo[playerid]);
}

static Fuel_HideSpeedo(playerid)
{
	if (gSpeedo[playerid] == INVALID_PLAYER_TEXT_DRAW) return;
	PlayerTextDrawDestroy(playerid, gSpeedo[playerid]);
	gSpeedo[playerid] = INVALID_PLAYER_TEXT_DRAW;
}

// Из OnPlayerDisconnect: textdraw игрока удаляется сам, сбрасываем ссылку
stock Fuel_OnDisconnect(playerid)
{
	gSpeedo[playerid] = INVALID_PLAYER_TEXT_DRAW;
	gFuelStationFor[playerid] = -1;
}

public Fuel_OnSecond()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i)) continue;
		if (!Player[i][pLogged] || GetPlayerState(i) != PLAYER_STATE_DRIVER)
		{
			Fuel_HideSpeedo(i);
			continue;
		}

		new v = GetPlayerVehicleID(i);
		new speed = Fuel_GetSpeed(v);
		new bool:usesFuel = Fuel_VehicleUsesFuel(v);

		if (usesFuel)
		{
			new engine, lights, alarm, doors, bonnet, boot, objective;
			GetVehicleParamsEx(v, engine, lights, alarm, doors, bonnet, boot, objective);
			if (engine == 1)
			{
				new Float:fuel = Fuel_Get(v) - (FUEL_IDLE_PER_SEC + FUEL_SPEED_FACTOR * float(speed));
				Fuel_Set(v, fuel);
				if (fuel <= 0.0)
				{
					SetVehicleParamsEx(v, 0, 0, alarm, doors, bonnet, boot, objective);
					GameTextForPlayer(i, "~r~Out of fuel", 3000, 3);
					SendClientMessage(i, COLOR_RED, "Бензин закончился, двигатель заглох. Нужна канистра (/canister) или эвакуатор.");
				}
				else if (fuel < FUEL_LOW_WARNING && !gVehFuelWarned[v])
				{
					gVehFuelWarned[v] = true;
					SendClientMessage(i, COLOR_ORANGE, "Бензин заканчивается (%d л). Заправься на ближайшей АЗС.", floatround(fuel, floatround_ceil));
				}
			}
		}

		if (gSpeedoHidden[i]) Fuel_HideSpeedo(i);
		else Fuel_ShowSpeedo(i, speed, Fuel_Get(v), usesFuel);
	}
	return 1;
}

// ---------------------------------------------------------------------
//  Заправка
// ---------------------------------------------------------------------
// H в машине (keys.inc). Возвращает true, если игрок на АЗС.
stock bool:Fuel_TryFill(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return false;
	new s = Fuel_GetNearbyStation(playerid);
	if (s == -1) return false;
	Cmd_Fill(playerid);
	return true;
}

stock bool:Cmd_Fill(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER)
		return SendClientMessage(playerid, COLOR_GREY, "Ты должен сидеть за рулём.");
	new s = Fuel_GetNearbyStation(playerid);
	if (s == -1) return SendClientMessage(playerid, COLOR_GREY, "Рядом нет АЗС.");

	new v = GetPlayerVehicleID(playerid);
	if (!Fuel_VehicleUsesFuel(v)) return SendClientMessage(playerid, COLOR_GREY, "Этому транспорту бензин не нужен.");

	new engine, lights, alarm, doors, bonnet, boot, objective;
	GetVehicleParamsEx(v, engine, lights, alarm, doors, bonnet, boot, objective);
	if (engine == 1) return SendClientMessage(playerid, COLOR_GREY, "Сначала заглуши двигатель (2).");

	new have = floatround(Fuel_Get(v), floatround_floor);
	new need = FUEL_TANK - have;
	if (need <= 0) return SendClientMessage(playerid, COLOR_GREY, "Бак уже полный.");

	gFuelStationFor[playerid] = s;
	ShowPlayerDialog(playerid, DIALOG_FUEL_FILL, DIALOG_STYLE_INPUT, "АЗС",
		"{FFFFFF}В баке: {FFFF00}%d{FFFFFF} из %d л.\nЦена: ${FFFF00}%d{FFFFFF} за литр. Полный бак: ${FFFF00}%d\n\nСколько литров залить? (пусто — полный бак)",
		"Заправить", "Отмена", have, FUEL_TANK, FuelStation[s][fsPrice], need * FuelStation[s][fsPrice]);
	return true;
}

// ALT пешком (keys.inc). Возвращает true, если игрок на АЗС.
stock bool:Fuel_TryCanisterShop(playerid)
{
	new s = Fuel_GetNearbyStation(playerid, 3.0);
	if (s == -1) return false;
	if (gPlayerCanister[playerid] > 0)
	{
		SendClientMessage(playerid, COLOR_GREY, "У тебя уже есть полная канистра. Залить в машину: /canister");
		return true;
	}
	gFuelStationFor[playerid] = s;
	ShowPlayerDialog(playerid, DIALOG_FUEL_CAN, DIALOG_STYLE_MSGBOX, "АЗС",
		"{FFFFFF}Канистра бензина на %d л.\nЦена: ${FFFF00}%d\n\n{FFFFFF}Купить?", "Купить", "Отмена",
		FUEL_CANISTER, FUEL_CANISTER * FuelStation[s][fsPrice]);
	return true;
}

stock Fuel_OnDialog(playerid, dialogid, response, const inputtext[])
{
	if (dialogid != DIALOG_FUEL_FILL && dialogid != DIALOG_FUEL_CAN) return 0;
	new s = gFuelStationFor[playerid];
	gFuelStationFor[playerid] = -1;
	if (!response || s < 0 || s >= MAX_FUEL_STATIONS || !FuelStation[s][fsUsed]) return 1;
	new price = FuelStation[s][fsPrice];

	if (dialogid == DIALOG_FUEL_CAN)
	{
		if (Fuel_GetNearbyStation(playerid, 3.0) != s || gPlayerCanister[playerid] > 0) return 1;
		new cost = FUEL_CANISTER * price;
		if (GetMoney(playerid) < cost) { SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", cost); return 1; }
		GiveMoney(playerid, -cost);
		gPlayerCanister[playerid] = 1;
		Fuel_SaveCanister(playerid);
		Account_Save(playerid);
		SendClientMessage(playerid, COLOR_GREEN, "Ты купил канистру на %d л за $%d. Залить в машину: /canister", FUEL_CANISTER, cost);
		return 1;
	}

	// DIALOG_FUEL_FILL: игрок мог уехать или завести двигатель, пока окно было открыто
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || Fuel_GetNearbyStation(playerid) != s) return 1;
	new v = GetPlayerVehicleID(playerid);
	new have = floatround(Fuel_Get(v), floatround_floor);
	new liters = FUEL_TANK - have;
	if (inputtext[0] != '\0')
	{
		if (!IsNumeric(inputtext)) { SendClientMessage(playerid, COLOR_GREY, "Введи число литров."); return 1; }
		liters = strval(inputtext);
	}
	if (liters > FUEL_TANK - have) liters = FUEL_TANK - have;
	if (liters < 1) { SendClientMessage(playerid, COLOR_GREY, "Бак уже полный."); return 1; }

	new cost = liters * price;
	if (GetMoney(playerid) < cost)
	{
		liters = GetMoney(playerid) / price;
		if (liters < 1) { SendClientMessage(playerid, COLOR_GREY, "Не хватает денег даже на литр."); return 1; }
		cost = liters * price;
	}
	GiveMoney(playerid, -cost);
	Fuel_Set(v, Fuel_Get(v) + float(liters));
	if (v == gPlayerVeh[playerid]) Fuel_SavePersonal(playerid);
	Account_Save(playerid);

	new msg[96];
	format(msg, sizeof msg, "%s заправляет машину.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(playerid, COLOR_GREEN, "Залито %d л за $%d. В баке %d из %d л.", liters, cost,
		floatround(Fuel_Get(v), floatround_floor), FUEL_TANK);
	return 1;
}

// ---------------------------------------------------------------------
//  Команды игрока
// ---------------------------------------------------------------------
stock bool:Cmd_Fuel(playerid)
{
	if (!IsPlayerInAnyVehicle(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Ты не в машине.");
	new v = GetPlayerVehicleID(playerid);
	if (!Fuel_VehicleUsesFuel(v)) return SendClientMessage(playerid, COLOR_GREY, "Этому транспорту бензин не нужен.");
	SendClientMessage(playerid, COLOR_WHITE, "В баке %d из %d л.", floatround(Fuel_Get(v), floatround_floor), FUEL_TANK);
	return true;
}

stock bool:Cmd_Speedo(playerid)
{
	gSpeedoHidden[playerid] = !gSpeedoHidden[playerid];
	if (gSpeedoHidden[playerid]) Fuel_HideSpeedo(playerid);
	if (gSpeedoHidden[playerid]) SendClientMessage(playerid, COLOR_WHITE, "Спидометр спрятан. Вернуть: /speedo");
	else SendClientMessage(playerid, COLOR_WHITE, "Спидометр включён.");
	return true;
}

stock bool:Cmd_Canister(playerid)
{
	if (gPlayerCanister[playerid] <= 0) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет канистры. Купить можно на любой АЗС (ALT).");
	if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return SendClientMessage(playerid, COLOR_GREY, "Выйди из машины и встань рядом с ней.");

	new Float:px, Float:py, Float:pz;
	GetPlayerPos(playerid, px, py, pz);
	new best = INVALID_VEHICLE_ID, Float:bestDist = 5.0;
	for (new v = 1; v < MAX_VEHICLES; v++)
	{
		if (!IsValidVehicle(v) || !Fuel_VehicleUsesFuel(v)) continue;
		new Float:d = GetVehicleDistanceFromPoint(v, px, py, pz);
		if (d < bestDist)
		{
			best = v;
			bestDist = d;
		}
	}
	if (best == INVALID_VEHICLE_ID) return SendClientMessage(playerid, COLOR_GREY, "Рядом нет машины.");
	if (Fuel_Get(best) > float(FUEL_TANK - 1)) return SendClientMessage(playerid, COLOR_GREY, "У этой машины полный бак.");

	Fuel_Set(best, Fuel_Get(best) + float(FUEL_CANISTER));
	gPlayerCanister[playerid] = 0;
	Fuel_SaveCanister(playerid);
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && gPlayerVeh[i] == best) Fuel_SavePersonal(i);
	}

	ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, false, false, false, false, 0);
	new msg[96];
	format(msg, sizeof msg, "%s заливает бензин из канистры в машину.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(playerid, COLOR_GREEN, "Залито %d л. В баке %d из %d л.", FUEL_CANISTER,
		floatround(Fuel_Get(best), floatround_floor), FUEL_TANK);
	return true;
}

// ---------------------------------------------------------------------
//  Команды админа
// ---------------------------------------------------------------------
stock bool:Cmd_AddFuel(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, price = FUEL_PRICE;
	NextIntParam(params, pos, price);
	if (price < 1 || price > 1000) return SendClientMessage(playerid, COLOR_GREY, "Использование: /addfuel [цена за литр 1-1000]");

	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO fuel_stations (x, y, z, price) VALUES (%f, %f, %f, %d)", x, y, z, price));
	new s = Fuel_AddStation(DB_LastInsertId(), x, y, z, price);
	if (s == -1) return SendClientMessage(playerid, COLOR_RED, "Лимит АЗС (%d). Станция сохранена в базе, появится после перезапуска, если удалить другую.", MAX_FUEL_STATIONS);
	SendClientMessage(playerid, COLOR_ADMIN, "АЗС №%d создана, бензин $%d за литр.", FuelStation[s][fsDbId], price);
	return true;
}

stock bool:Cmd_DelFuel(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new s = Fuel_GetNearbyStation(playerid);
	if (s == -1) return SendClientMessage(playerid, COLOR_GREY, "Рядом нет АЗС.");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM fuel_stations WHERE id = %d", FuelStation[s][fsDbId]));
	DestroyPickup(FuelStation[s][fsPickup]);
	if (FuelStation[s][fsLabel] != Text3D:INVALID_3DTEXT_ID) Delete3DTextLabel(FuelStation[s][fsLabel]);
	SendClientMessage(playerid, COLOR_ADMIN, "АЗС №%d удалена.", FuelStation[s][fsDbId]);
	FuelStation[s][fsUsed] = false;
	return true;
}

stock bool:Cmd_SetFuel(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	if (!IsPlayerInAnyVehicle(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Сядь в машину.");
	new pos = 0, liters = FUEL_TANK;
	NextIntParam(params, pos, liters);
	new v = GetPlayerVehicleID(playerid);
	Fuel_Set(v, float(liters));
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && gPlayerVeh[i] == v) Fuel_SavePersonal(i);
	}
	SendClientMessage(playerid, COLOR_ADMIN, "В баке теперь %d л.", floatround(Fuel_Get(v), floatround_floor));
	return true;
}
