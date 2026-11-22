// =====================================================================
//  rental.inc — аренда мопедов.
//
//  Админ (4+) ставит точки проката там, где стоит: /addmoped (лицом туда,
//  куда должен смотреть мопед). Убрать ближайшую: /delmoped, список: /mopeds.
//  Цена и срок общие для всех точек: /mopedprice [цена] [минут].
//
//  Игрок жмёт ALT у точки, платит и сразу садится на Faggio.
//  Мопед только его: другим за руль нельзя. Аренда кончается, когда выйдет
//  срок, игрок выйдет из игры, мопед взорвётся или игрок введёт /unrentmoped.
//  Тогда мопед пропадает. Одновременно — один мопед на игрока.
// =====================================================================

#define MAX_RENT_POINTS     20
#define RENT_MODEL          462         // Faggio
#define RENT_MARKER_MODEL   1239        // значок «i»
#define RENT_RANGE          2.0
#define RENT_PRICE_DEF      150
#define RENT_MINUTES_DEF    30
#define RENT_MINUTES_MAX    240
#define RENT_PRICE_MAX      100000

enum E_RENTPT
{
	rpDbId,
	Float:rpX, Float:rpY, Float:rpZ, Float:rpA,
	rpMarker
}
static gRentPt[MAX_RENT_POINTS][E_RENTPT];
static gRentPtCount = 0;
static gRentPrice = RENT_PRICE_DEF;
static gRentMinutes = RENT_MINUTES_DEF;

static gRentVeh[MAX_PLAYERS] = { INVALID_VEHICLE_ID, ... };    // мопед игрока
static gRentLeft[MAX_PLAYERS];                                  // минут аренды осталось
static gRentOwner[MAX_VEHICLES] = { INVALID_PLAYER_ID, ... };   // чей это прокатный мопед

static Rental_Label(p)
{
	new text[128];
	format(text, sizeof text, "{FFFF66}Аренда мопеда\n{FFFFFF}$%d за %d мин\n{AAAAAA}ALT — взять", gRentPrice, gRentMinutes);
	Marker_SetLabel(gRentPt[p][rpMarker], text);
}

static Rental_Build(p)
{
	new Float:x = gRentPt[p][rpX], Float:y = gRentPt[p][rpY], Float:z = gRentPt[p][rpZ];
	gRentPt[p][rpMarker] = Marker_Create(MK_RENTAL, gRentPt[p][rpDbId], "Аренда мопедов", RENT_MARKER_MODEL, x, y, z, 0, 0, GPS_TRANSPORT, gRentPt[p][rpA]);
	gRentPt[p][rpX] = x; gRentPt[p][rpY] = y; gRentPt[p][rpZ] = z;
	Rental_Label(p);
}

stock Rental_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS rent_points (id INTEGER PRIMARY KEY AUTOINCREMENT, x REAL, y REAL, z REAL, a REAL)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS rent_cfg (id INTEGER PRIMARY KEY, price INTEGER, minutes INTEGER)"));

	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT price, minutes FROM rent_cfg WHERE id = 1");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			gRentPrice = DB_GetFieldInt(r, 0);
			gRentMinutes = DB_GetFieldInt(r, 1);
			if (gRentMinutes < 1) gRentMinutes = RENT_MINUTES_DEF;
		}
		DB_FreeResultSet(r);
	}

	r = DB_ExecuteQuery(gDB, "SELECT id, x, y, z, a FROM rent_points ORDER BY id");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows && gRentPtCount < MAX_RENT_POINTS; i++)
		{
			new p = gRentPtCount++;
			gRentPt[p][rpDbId] = DB_GetFieldInt(r, 0);
			gRentPt[p][rpX] = DB_GetFieldFloat(r, 1);
			gRentPt[p][rpY] = DB_GetFieldFloat(r, 2);
			gRentPt[p][rpZ] = DB_GetFieldFloat(r, 3);
			gRentPt[p][rpA] = DB_GetFieldFloat(r, 4);
			Rental_Build(p);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Rental] %d moped rental points.", gRentPtCount);
}

// Админ передвинул маркер точки в /markers
stock Rental_OnMarkerMoved(dbid, Float:x, Float:y, Float:z)
{
	for (new p = 0; p < gRentPtCount; p++)
	{
		if (gRentPt[p][rpDbId] != dbid) continue;
		gRentPt[p][rpX] = x; gRentPt[p][rpY] = y; gRentPt[p][rpZ] = z;
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE rent_points SET x = %f, y = %f, z = %f WHERE id = %d", x, y, z, dbid));
		return;
	}
}

static Rental_Near(playerid)
{
	if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return -1;
	for (new p = 0; p < gRentPtCount; p++)
	{
		if (Marker_InRange(playerid, gRentPt[p][rpMarker], RENT_RANGE)) return p;
	}
	return -1;
}

// Закончить аренду: мопед пропадает
static Rental_End(playerid, const why[])
{
	new v = gRentVeh[playerid];
	gRentVeh[playerid] = INVALID_VEHICLE_ID;
	gRentLeft[playerid] = 0;
	if (v == INVALID_VEHICLE_ID) return;
	gRentOwner[v] = INVALID_PLAYER_ID;
	Vehicle_Clear(v);
	DestroyVehicle(v);
	if (why[0] && IsPlayerConnected(playerid)) SendClientMessage(playerid, COLOR_ORANGE, "Аренда мопеда закончилась: %s", why);
}

// ALT у точки проката (keys.inc). 1 — сработало
stock Rental_TryAlt(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return 0;
	if (Rental_Near(playerid) == -1) return 0;
	if (gRentVeh[playerid] != INVALID_VEHICLE_ID)
	{
		ShowPlayerDialog(playerid, DIALOG_RENTAL, DIALOG_STYLE_MSGBOX, Biz_IconCaption(BIZ_ICON_RENTAL, "Аренда мопеда"),
			"{FFFFFF}У тебя уже есть мопед (осталось %d мин).\nСдать его?", "Сдать", "Отмена", gRentLeft[playerid]);
		return 1;
	}
	ShowPlayerDialog(playerid, DIALOG_RENTAL, DIALOG_STYLE_MSGBOX, Biz_IconCaption(BIZ_ICON_RENTAL, "Аренда мопеда"),
		"{FFFFFF}Мопед Faggio на {FFFF00}%d мин{FFFFFF} за {33AA33}$%d{FFFFFF}.\nКроме тебя за руль никто не сядет. Сдать раньше: /unrentmoped\n\nВзять?",
		"Взять", "Отмена", gRentMinutes, gRentPrice);
	return 1;
}

static Rental_Take(playerid)
{
	new p = Rental_Near(playerid);
	if (p == -1) return SendClientMessage(playerid, COLOR_GREY, "Подойди к точке проката.");
	if (gRentVeh[playerid] != INVALID_VEHICLE_ID) return SendClientMessage(playerid, COLOR_GREY, "У тебя уже есть арендованный мопед.");
	if (GetMoney(playerid) < gRentPrice) return SendClientMessage(playerid, COLOR_GREY, "Не хватает денег: аренда стоит $%d.", gRentPrice);

	new Float:x, Float:y, Float:z, Float:a = gRentPt[p][rpA];
	GetPlayerPos(playerid, x, y, z);
	x += 1.5 * floatsin(-a, degrees);
	y += 1.5 * floatcos(-a, degrees);
	new v = CreateVehicle(RENT_MODEL, x, y, z + 0.3, a, -1, -1, -1);
	if (v == INVALID_VEHICLE_ID) return SendClientMessage(playerid, COLOR_RED, "Свободных мопедов нет, попробуй позже.");
	Vehicle_Clear(v);
	gRentOwner[v] = playerid;
	gRentVeh[playerid] = v;
	gRentLeft[playerid] = gRentMinutes;
	if (gRentPrice > 0) GiveMoney(playerid, -gRentPrice, "аренда мопеда");
	PutPlayerInVehicle(playerid, v, 0);
	SendClientMessage(playerid, COLOR_GREEN, "Мопед твой на %d мин. Двигатель: 2. Сдать раньше: /unrentmoped.", gRentMinutes);
	return 1;
}

stock Rental_OnDialog(playerid, dialogid, response)
{
	if (dialogid != DIALOG_RENTAL) return 0;
	if (!response) return 1;
	if (gRentVeh[playerid] != INVALID_VEHICLE_ID) Rental_End(playerid, "мопед сдан.");
	else Rental_Take(playerid);
	return 1;
}

// Из OnPlayerStateChange: на чужой прокатный мопед за руль нельзя
stock Rental_OnBecomeDriver(playerid)
{
	new v = GetPlayerVehicleID(playerid);
	if (v <= 0 || v >= MAX_VEHICLES) return;
	new owner = gRentOwner[v];
	if (owner == INVALID_PLAYER_ID || owner == playerid) return;
	RemovePlayerFromVehicle(playerid);
	SendClientMessage(playerid, COLOR_GREY, "Это чужой арендованный мопед.");
}

// Из OnVehicleSpawn: прокатный мопед взорвался или утонул и появился снова — аренде конец
stock Rental_OnVehicleSpawn(vehicleid)
{
	if (vehicleid <= 0 || vehicleid >= MAX_VEHICLES) return 0;
	new owner = gRentOwner[vehicleid];
	if (owner == INVALID_PLAYER_ID) return 0;
	Rental_End(owner, "мопед разбит.");
	return 1;
}

stock Rental_OnDisconnect(playerid)
{
	Rental_End(playerid, "");
}

// Раз в минуту (OnMinuteTimer): срок аренды
stock Rental_OnMinute()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gRentVeh[i] == INVALID_VEHICLE_ID) continue;
		gRentLeft[i]--;
		if (gRentLeft[i] <= 0) Rental_End(i, "вышло время.");
		else if (gRentLeft[i] == 5) SendClientMessage(i, COLOR_ORANGE, "Аренда мопеда кончится через 5 мин.");
	}
}

stock bool:Cmd_UnrentMoped(playerid)
{
	if (gRentVeh[playerid] == INVALID_VEHICLE_ID) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет арендованного мопеда.");
	Rental_End(playerid, "мопед сдан.");
	return true;
}

stock bool:Cmd_AddMoped(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	if (gRentPtCount >= MAX_RENT_POINTS) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит точек проката (%d).", MAX_RENT_POINTS);
	if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return SendClientMessage(playerid, COLOR_GREY, "Точку проката ставят на улице.");
	new p = gRentPtCount++;
	GetPlayerPos(playerid, gRentPt[p][rpX], gRentPt[p][rpY], gRentPt[p][rpZ]);
	GetPlayerFacingAngle(playerid, gRentPt[p][rpA]);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO rent_points (x, y, z, a) VALUES (%f, %f, %f, %f)",
		gRentPt[p][rpX], gRentPt[p][rpY], gRentPt[p][rpZ], gRentPt[p][rpA]));
	gRentPt[p][rpDbId] = DB_LastInsertId();
	Rental_Build(p);
	SendClientMessage(playerid, COLOR_ADMIN, "Точка проката №%d поставлена. Мопед появится перед тобой, куда ты смотришь. Цена: /mopedprice", gRentPt[p][rpDbId]);
	return true;
}

stock bool:Cmd_DelMoped(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new p = -1;
	for (new i = 0; i < gRentPtCount; i++)
	{
		if (Marker_InRange(playerid, gRentPt[i][rpMarker], 5.0)) { p = i; break; }
	}
	if (p == -1) return SendClientMessage(playerid, COLOR_GREY, "Рядом нет точки проката. Список: /mopeds");
	new dbid = gRentPt[p][rpDbId];
	Marker_Destroy(gRentPt[p][rpMarker], true);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM rent_points WHERE id = %d", dbid));
	new last = gRentPtCount - 1;
	if (p != last) gRentPt[p] = gRentPt[last];
	gRentPtCount--;
	SendClientMessage(playerid, COLOR_ADMIN, "Точка проката №%d убрана.", dbid);
	return true;
}

stock bool:Cmd_Mopeds(playerid)
{
	if (!Admin_Check(playerid, 3)) return true;
	SendClientMessage(playerid, COLOR_YELLOW, "Прокат мопедов: $%d за %d мин. Точек: %d", gRentPrice, gRentMinutes, gRentPtCount);
	for (new p = 0; p < gRentPtCount; p++)
		SendClientMessage(playerid, COLOR_WHITE, " №%d — (%.0f, %.0f)", gRentPt[p][rpDbId], gRentPt[p][rpX], gRentPt[p][rpY]);
	return true;
}

stock bool:Cmd_MopedPrice(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, price, minutes = gRentMinutes;
	if (!NextIntParam(params, pos, price))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /mopedprice [цена] [минут] (сейчас $%d за %d мин)", gRentPrice, gRentMinutes);
	NextIntParam(params, pos, minutes);
	if (price < 0 || price > RENT_PRICE_MAX) return SendClientMessage(playerid, COLOR_GREY, "Цена от 0 до %d.", RENT_PRICE_MAX);
	if (minutes < 1 || minutes > RENT_MINUTES_MAX) return SendClientMessage(playerid, COLOR_GREY, "Срок от 1 до %d мин.", RENT_MINUTES_MAX);
	gRentPrice = price;
	gRentMinutes = minutes;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO rent_cfg (id, price, minutes) VALUES (1, %d, %d)", price, minutes));
	for (new p = 0; p < gRentPtCount; p++) Rental_Label(p);
	SendClientMessage(playerid, COLOR_ADMIN, "Аренда мопеда: $%d за %d мин.", price, minutes);
	return true;
}
