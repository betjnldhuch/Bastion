// =====================================================================
//  garage.inc — СТО (ремонт, покраска, тюнинг) и штрафстоянка.
//
//  СТО: заехать на точку за рулём и нажать H (или /tune).
//    Ремонт — любой машине. Покраска, нитро, гидравлика и диски —
//    только своей личной машине, тюнинг сохраняется в базе.
//  Штрафстоянка: полицейский на смене цепляет личную машину служебным эвакуатором
//    (H за рулём Towtruck фракции), везёт на штрафстоянку и там вводит /impound [штраф].
//    Машина пропадает и ждёт хозяина на штрафстоянке:
//    там ALT (или /unimpound) — заплатить штраф и забрать.
//
//  Точки хранятся в таблице garages. Стартовые координаты примерные,
//  их стоит проверить в игре. Админ (4+): /addgarage [1 — СТО, 2 —
//  штрафстоянка] там, где стоит, /delgarage — удалить ближайшую.
// =====================================================================

#define MAX_GARAGES         20
#define GARAGE_RANGE        6.0     // радиус точки СТО (машина большая)

#define GARAGE_TUNE         1
#define GARAGE_IMPOUND      2

#define TUNE_REPAIR_PRICE   250
#define TUNE_PAINT_PRICE    500
#define TUNE_NITRO_PRICE    3000
#define TUNE_HYDRA_PRICE    2000
#define TUNE_WHEELS_PRICE   1500

#define IMPOUND_FINE_MIN    100
#define IMPOUND_FINE_MAX    5000
#define IMPOUND_REWARD      100     // премия полицейскому за эвакуацию


enum E_GARAGE
{
	gaDbId,
	gaType,
	Float:gaX, Float:gaY, Float:gaZ,
	gaMarker                    // маркер (markers.inc)
}
static Garage[MAX_GARAGES][E_GARAGE];
static gGarageCount = 0;

// Тюнинг каждой машины (чтобы вернуть его после респавна)
static gModNitro[MAX_VEHICLES];
static gModHydra[MAX_VEHICLES];
static gModWheels[MAX_VEHICLES];
static gModC1[MAX_VEHICLES] = { -1, ... };
static gModC2[MAX_VEHICLES] = { -1, ... };

static gImpoundFine[MAX_PLAYERS];   // штраф за машину на штрафстоянке (0 = машина не там)

static const gWheelIds[] = { 1025, 1073, 1074, 1075, 1076, 1077, 1078, 1079, 1080, 1081, 1082, 1083, 1084, 1085, 1096, 1097, 1098 };
static const gWheelNames[][] = { "Offroad", "Shadow", "Mega", "Rimshine", "Wires", "Classic", "Twist", "Cutter", "Switch", "Grove", "Import", "Dollar", "Trance", "Atomic", "Ahab", "Virtual", "Access" };

// ---------------------------------------------------------------------
//  Запуск
// ---------------------------------------------------------------------
stock Garage_Init()
{
	DB_AddColumn("vehicles", "nitro", "INTEGER DEFAULT 0");
	DB_AddColumn("vehicles", "hydra", "INTEGER DEFAULT 0");
	DB_AddColumn("vehicles", "wheels", "INTEGER DEFAULT 0");
	DB_AddColumn("vehicles", "impound", "INTEGER DEFAULT 0");
	DB_AddColumn("vehicles", "imp_by", "TEXT DEFAULT ''");       // кто отправил на штрафстоянку
	DB_AddColumn("vehicles", "imp_date", "INTEGER DEFAULT 0");   // когда
	DB_AddColumn("vehicles", "imp_reason", "TEXT DEFAULT ''");   // за что
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS garages (id INTEGER PRIMARY KEY AUTOINCREMENT, type INTEGER, x REAL, y REAL, z REAL)"));

	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM garages");
	new count = 0;
	if (r != DBResult:0)
	{
		count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (count == 0)
	{
		// Примерные точки — проверить в игре и при необходимости /addgarage /delgarage
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO garages (type, x, y, z) VALUES (1, 1041.4, -1030.0, 32.1)"));   // СТО Transfender, Temple
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO garages (type, x, y, z) VALUES (1, 2644.8, -2039.0, 13.6)"));   // СТО Loco Low Co, Willowfield
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO garages (type, x, y, z) VALUES (2, 1540.5, -1675.5, 13.6)"));   // штрафстоянка у LSPD
		print("[Garage] Default garages created.");
	}

	r = DB_ExecuteQuery(gDB, "SELECT id, type, x, y, z FROM garages ORDER BY id");
	if (r == DBResult:0) return;
	new rows = DB_GetRowCount(r);
	for (new i = 0; i < rows && gGarageCount < MAX_GARAGES; i++)
	{
		new g = gGarageCount++;
		Garage[g][gaDbId] = DB_GetFieldInt(r, 0);
		Garage[g][gaType] = DB_GetFieldInt(r, 1);
		Garage[g][gaX] = DB_GetFieldFloat(r, 2);
		Garage[g][gaY] = DB_GetFieldFloat(r, 3);
		Garage[g][gaZ] = DB_GetFieldFloat(r, 4);
		Garage_CreateMarker(g);
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	printf("[Garage] Loaded %d garages.", gGarageCount);
}

static Garage_CreateMarker(g)
{
	if (Garage[g][gaType] == GARAGE_IMPOUND)
	{
		Garage[g][gaMarker] = Marker_Create(MK_GARAGE, Garage[g][gaDbId], "Штрафстоянка", 1239,
			Garage[g][gaX], Garage[g][gaY], Garage[g][gaZ], 0, 0, GPS_TRANSPORT);
		Marker_SetLabel(Garage[g][gaMarker], "{FF9900}Штрафстоянка\n{FFFFFF}ALT — забрать машину", COLOR_WHITE, 20.0);
	}
	else
	{
		Garage[g][gaMarker] = Marker_Create(MK_GARAGE, Garage[g][gaDbId], "СТО", 1239,
			Garage[g][gaX], Garage[g][gaY], Garage[g][gaZ], 0, 0, GPS_TRANSPORT);
		Marker_SetLabel(Garage[g][gaMarker], "{33CCFF}СТО\n{FFFFFF}Ремонт, покраска, тюнинг\nЗаедь за рулём и нажми H", COLOR_WHITE, 20.0);
	}
}

// Админ перенёс маркер СТО или штрафстоянки (/markers)
stock Garage_OnMarkerMoved(dbid, Float:x, Float:y, Float:z)
{
	for (new g = 0; g < gGarageCount; g++)
	{
		if (Garage[g][gaDbId] != dbid) continue;
		Garage[g][gaX] = x;
		Garage[g][gaY] = y;
		Garage[g][gaZ] = z;
		return;
	}
}

// Ближайшая точка нужного типа (-1 = нет)
static Garage_GetNearby(playerid, type, Float:range)
{
	if (GetPlayerVirtualWorld(playerid) != 0) return -1;
	for (new g = 0; g < gGarageCount; g++)
	{
		if (Garage[g][gaType] == type && IsPlayerInRangeOfPoint(playerid, range, Garage[g][gaX], Garage[g][gaY], Garage[g][gaZ])) return g;
	}
	return -1;
}

// ---------------------------------------------------------------------
//  Тюнинг личной машины: загрузка и применение
// ---------------------------------------------------------------------
// Из Vehicle_Clear: новая машина — без тюнинга
stock Garage_ResetVehicle(vehicleid)
{
	gModNitro[vehicleid] = 0;
	gModHydra[vehicleid] = 0;
	gModWheels[vehicleid] = 0;
	gModC1[vehicleid] = -1;
	gModC2[vehicleid] = -1;
}

// Повесить тюнинг и цвет на машину (после создания и после респавна)
stock Garage_ApplyMods(vehicleid)
{
	if (gModNitro[vehicleid]) AddVehicleComponent(vehicleid, gModNitro[vehicleid]);
	if (gModHydra[vehicleid]) AddVehicleComponent(vehicleid, gModHydra[vehicleid]);
	if (gModWheels[vehicleid]) AddVehicleComponent(vehicleid, gModWheels[vehicleid]);
	if (gModC1[vehicleid] != -1) ChangeVehicleColours(vehicleid, gModC1[vehicleid], gModC2[vehicleid]);
}

// Из Vehicles_SpawnPersonal: тюнинг из базы
stock Garage_LoadMods(playerid, vehicleid)
{
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT nitro, hydra, wheels, c1, c2 FROM vehicles WHERE owner = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		gModNitro[vehicleid] = DB_GetFieldInt(r, 0);
		gModHydra[vehicleid] = DB_GetFieldInt(r, 1);
		gModWheels[vehicleid] = DB_GetFieldInt(r, 2);
		gModC1[vehicleid] = DB_GetFieldInt(r, 3);
		gModC2[vehicleid] = DB_GetFieldInt(r, 4);
	}
	DB_FreeResultSet(r);
	Garage_ApplyMods(vehicleid);
}

static Garage_SaveMods(playerid, vehicleid)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE vehicles SET nitro = %d, hydra = %d, wheels = %d, c1 = %d, c2 = %d WHERE owner = %d",
		gModNitro[vehicleid], gModHydra[vehicleid], gModWheels[vehicleid], gModC1[vehicleid], gModC2[vehicleid], Player[playerid][pDbId]));
}

// ---------------------------------------------------------------------
//  СТО
// ---------------------------------------------------------------------
// H за рулём (keys.inc): 1 — игрок на СТО, меню открыто
stock Garage_TryOpen(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return 0;
	if (Garage_GetNearby(playerid, GARAGE_TUNE, GARAGE_RANGE) == -1) return 0;
	Garage_ShowTune(playerid);
	return 1;
}

stock bool:Cmd_Tune(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return SendClientMessage(playerid, COLOR_GREY, "Сядь за руль и заедь на СТО.");
	if (Garage_GetNearby(playerid, GARAGE_TUNE, GARAGE_RANGE) == -1) return SendClientMessage(playerid, COLOR_GREY, "Ты не на СТО.");
	Garage_ShowTune(playerid);
	return true;
}

static Garage_ShowTune(playerid)
{
	new v = GetPlayerVehicleID(playerid);
	new Float:hp;
	GetVehicleHealth(v, hp);
	if (v == gPlayerVeh[playerid])
	{
		ShowPlayerDialog(playerid, DIALOG_TUNE, DIALOG_STYLE_TABLIST_HEADERS, "СТО",
			"Услуга\tЦена\nРемонт (сейчас %.0f%%)\t$%d\nПокраска\t$%d\nЗакись азота\t$%d\nГидравлика\t$%d\nДиски\t$%d\nСнять весь тюнинг\tбесплатно",
			"Выбрать", "Уехать", hp / 10.0, TUNE_REPAIR_PRICE, TUNE_PAINT_PRICE, TUNE_NITRO_PRICE, TUNE_HYDRA_PRICE, TUNE_WHEELS_PRICE);
	}
	else
	{
		ShowPlayerDialog(playerid, DIALOG_TUNE, DIALOG_STYLE_TABLIST_HEADERS, "СТО",
			"Услуга\tЦена\nРемонт (сейчас %.0f%%)\t$%d\n{AFAFAF}Тюнинг — только своей машине\t",
			"Выбрать", "Уехать", hp / 10.0, TUNE_REPAIR_PRICE);
	}
}

static bool:Garage_Pay(playerid, price, const reason[] = "СТО")
{
	if (GetMoney(playerid) < price)
	{
		SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", price);
		return false;
	}
	GiveMoney(playerid, -price, reason);
	Account_Save(playerid);
	return true;
}

// Поставить деталь, если она подходит к машине
static Garage_InstallPart(playerid, v, part, price, const name[])
{
	if (!VehicleCanHaveComponent(GetVehicleModel(v), part))
	{
		SendClientMessage(playerid, COLOR_GREY, "%s не ставится на эту модель.", name);
		return 0;
	}
	if (!Garage_Pay(playerid, price)) return 0;
	AddVehicleComponent(v, part);
	SendClientMessage(playerid, COLOR_GREEN, "%s установлен(а) за $%d.", name, price);
	return 1;
}

// Ответы на окна СТО и штрафстоянки (из OnDialogResponse)
stock Garage_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid < DIALOG_TUNE || dialogid > DIALOG_UNIMPOUND) return 0;
	if (dialogid == DIALOG_UNIMPOUND)
	{
		if (response) Garage_Unimpound(playerid);
		return 1;
	}
	if (!response) return 1;
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || Garage_GetNearby(playerid, GARAGE_TUNE, GARAGE_RANGE) == -1) return 1;

	new v = GetPlayerVehicleID(playerid);
	new bool:own = (v == gPlayerVeh[playerid]);

	if (dialogid == DIALOG_TUNE)
	{
		if (listitem == 0)
		{
			if (!Garage_Pay(playerid, TUNE_REPAIR_PRICE)) return 1;
			RepairVehicle(v);
			SendClientMessage(playerid, COLOR_GREEN, "Машина отремонтирована за $%d.", TUNE_REPAIR_PRICE);
			return 1;
		}
		if (!own) return 1;
		switch (listitem)
		{
			case 1: ShowPlayerDialog(playerid, DIALOG_TUNE_PAINT, DIALOG_STYLE_INPUT, "Покраска",
				"{FFFFFF}Введи два цвета через пробел (0-255), например: {FFFF00}1 0\n{FFFFFF}0 — чёрный, 1 — белый, 3 — красный, 6 — жёлтый, 79 — синий, 86 — зелёный", "Покрасить", "Назад");
			case 2:
			{
				if (Garage_InstallPart(playerid, v, 1010, TUNE_NITRO_PRICE, "Закись азота"))
				{
					gModNitro[v] = 1010;
					Garage_SaveMods(playerid, v);
				}
			}
			case 3:
			{
				if (Garage_InstallPart(playerid, v, 1087, TUNE_HYDRA_PRICE, "Гидравлика"))
				{
					gModHydra[v] = 1087;
					Garage_SaveMods(playerid, v);
				}
			}
			case 4:
			{
				new list[512], line[32];
				for (new i = 0; i < sizeof (gWheelIds); i++)
				{
					format(line, sizeof line, "%s\n", gWheelNames[i]);
					strcat(list, line);
				}
				ShowPlayerDialog(playerid, DIALOG_TUNE_WHEELS, DIALOG_STYLE_LIST, "Диски", "%s", "Поставить", "Назад", list);
			}
			case 5:
			{
				if (gModNitro[v]) RemoveVehicleComponent(v, gModNitro[v]);
				if (gModHydra[v]) RemoveVehicleComponent(v, gModHydra[v]);
				if (gModWheels[v]) RemoveVehicleComponent(v, gModWheels[v]);
				gModNitro[v] = 0;
				gModHydra[v] = 0;
				gModWheels[v] = 0;
				Garage_SaveMods(playerid, v);
				SendClientMessage(playerid, COLOR_WHITE, "Тюнинг снят.");
			}
		}
		return 1;
	}

	if (!own) return 1;
	if (dialogid == DIALOG_TUNE_PAINT)
	{
		new pos = 0, c1, c2;
		if (!NextIntParam(inputtext, pos, c1) || !NextIntParam(inputtext, pos, c2) || c1 < 0 || c1 > 255 || c2 < 0 || c2 > 255)
		{
			SendClientMessage(playerid, COLOR_GREY, "Нужно два числа от 0 до 255, например: 1 0");
			return 1;
		}
		if (!Garage_Pay(playerid, TUNE_PAINT_PRICE)) return 1;
		gModC1[v] = c1;
		gModC2[v] = c2;
		ChangeVehicleColours(v, c1, c2);
		Garage_SaveMods(playerid, v);
		SendClientMessage(playerid, COLOR_GREEN, "Машина перекрашена за $%d.", TUNE_PAINT_PRICE);
		return 1;
	}
	if (dialogid == DIALOG_TUNE_WHEELS)
	{
		if (listitem < 0 || listitem >= sizeof (gWheelIds)) return 1;
		if (Garage_InstallPart(playerid, v, gWheelIds[listitem], TUNE_WHEELS_PRICE, "Диски"))
		{
			gModWheels[v] = gWheelIds[listitem];
			Garage_SaveMods(playerid, v);
		}
	}
	return 1;
}

// ---------------------------------------------------------------------
//  Штрафстоянка
// ---------------------------------------------------------------------
// После входа в аккаунт (из Vehicles_LoadForPlayer)
stock Garage_LoadPlayer(playerid)
{
	gImpoundFine[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT impound FROM vehicles WHERE owner = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0) gImpoundFine[playerid] = DB_GetFieldInt(r, 0);
	DB_FreeResultSet(r);
	if (gImpoundFine[playerid] > 0)
		SendClientMessage(playerid, COLOR_ORANGE, "Твоя машина на штрафстоянке (штраф $%d). Забрать: ALT на штрафстоянке у LSPD.", gImpoundFine[playerid]);
}

stock Garage_GetImpound(playerid)
{
	return gImpoundFine[playerid];
}

// ---------------------------------------------------------------------
//  Эвакуатор полиции: H за рулём служебного Towtruck (525) — зацепить машину
//  сзади или отцепить. На штрафстоянке: /impound [штраф] [причина].
// ---------------------------------------------------------------------
#define TOW_MODEL           525
#define TOW_RANGE           8.0         // как близко должна стоять машина, чтобы зацепить
#define IMPOUND_LOT_RANGE   20.0        // как близко к штрафстоянке сдавать машину

static bool:Garage_IsPoliceTow(v)
{
	return v != 0 && GetVehicleModel(v) == TOW_MODEL && Faction_IsLaw(gVehFaction[v]);
}

// Чья личная машина v (INVALID_PLAYER_ID — не личная)
static Garage_PersonalOwner(v)
{
	for (new i = 0; i < MAX_PLAYERS; i++)
		if (IsPlayerConnected(i) && gPlayerVeh[i] == v) return i;
	return INVALID_PLAYER_ID;
}

// Из keys.inc (H в машине). 1 — сработало
stock Garage_TryTow(playerid)
{
	new tow = GetPlayerVehicleID(playerid);
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || !Garage_IsPoliceTow(tow)) return 0;
	if (!Faction_IsLaw(Player[playerid][pFaction]) || !Player[playerid][pDuty])
	{
		SendClientMessage(playerid, COLOR_GREY, "Эвакуатором пользуется полиция на смене.");
		return 1;
	}
	if (IsTrailerAttachedToVehicle(tow))
	{
		DetachTrailerFromVehicle(tow);
		SendClientMessage(playerid, COLOR_WHITE, "Машина отцеплена.");
		return 1;
	}
	new Float:tx, Float:ty, Float:tz, Float:ta;
	GetVehiclePos(tow, tx, ty, tz);
	GetVehicleZAngle(tow, ta);
	// точка за эвакуатором
	tx -= 5.0 * floatsin(-ta, degrees);
	ty -= 5.0 * floatcos(-ta, degrees);
	new best = INVALID_VEHICLE_ID, Float:bd = TOW_RANGE;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i)) continue;
		new v = gPlayerVeh[i];
		if (v == INVALID_VEHICLE_ID || v == tow) continue;
		new Float:d = GetVehicleDistanceFromPoint(v, tx, ty, tz);
		if (d < bd) { bd = d; best = v; }
	}
	if (best == INVALID_VEHICLE_ID)
	{
		SendClientMessage(playerid, COLOR_GREY, "Сзади нет личной машины. Подъедь задом вплотную (до %d м).", floatround(TOW_RANGE));
		return 1;
	}
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && GetPlayerVehicleID(i) == best)
		{
			SendClientMessage(playerid, COLOR_GREY, "В машине кто-то сидит.");
			return 1;
		}
	}
	AttachTrailerToVehicle(best, tow);
	new owner = Garage_PersonalOwner(best);
	SendClientMessage(playerid, COLOR_GREEN, "Машина %s зацеплена. Вези на штрафстоянку и введи /impound [штраф] [причина]. Отцепить: H.",
		(owner != INVALID_PLAYER_ID) ? GetRPName(owner) : "-");
	return 1;
}

stock bool:Cmd_Impound(playerid, const params[])
{
	if (!Faction_NeedLaw(playerid)) return true;
	new tow = GetPlayerVehicleID(playerid);
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || !Garage_IsPoliceTow(tow))
		return SendClientMessage(playerid, COLOR_GREY, "Машину увозят служебным эвакуатором: зацепи её (H за рулём эвакуатора) и привези на штрафстоянку.");
	new pos = 0, fine, reason[64];
	if (!NextIntParam(params, pos, fine) || fine < IMPOUND_FINE_MIN || fine > IMPOUND_FINE_MAX)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /impound [штраф $%d-%d] [причина] (на штрафстоянке, с машиной на крюке)", IMPOUND_FINE_MIN, IMPOUND_FINE_MAX);
	if (!RestParam(params, pos, reason)) strcat(reason, "не указана");
	if (!IsTrailerAttachedToVehicle(tow)) return SendClientMessage(playerid, COLOR_GREY, "На крюке нет машины. Зацепить: H.");
	if (Garage_GetNearby(playerid, GARAGE_IMPOUND, IMPOUND_LOT_RANGE) == -1) return SendClientMessage(playerid, COLOR_GREY, "Ты не на штрафстоянке (у LSPD, /gps).");
	new v = GetVehicleTrailer(tow);
	new owner = Garage_PersonalOwner(v);
	if (owner == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Это не личная машина.");
	if (owner == playerid) return SendClientMessage(playerid, COLOR_GREY, "Нельзя эвакуировать свою машину.");

	DetachTrailerFromVehicle(tow);
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && GetPlayerVehicleID(i) == v) RemovePlayerFromVehicle(i);
	}
	Vehicles_UnloadForPlayer(owner);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE vehicles SET impound = %d, imp_by = '%q', imp_date = %d, imp_reason = '%q' WHERE owner = %d",
		fine, GetRPName(playerid), gettime(), reason, Player[owner][pDbId]));
	gImpoundFine[owner] = fine;

	GiveMoney(playerid, IMPOUND_REWARD, "премия за эвакуацию");
	Account_Save(playerid);

	SendClientMessage(owner, COLOR_RED, "Твою машину эвакуировал %s. Причина: %s. Штраф $%d, забрать: штрафстоянка у LSPD (ALT).", GetRPName(playerid), reason, fine);
	SendClientMessage(playerid, COLOR_GREEN, "Машина %s сдана на штрафстоянку. Премия $%d.", GetRPName(owner), IMPOUND_REWARD);
	new msg[144];
	format(msg, sizeof msg, "[Диспетчер] %s эвакуировал машину %s (штраф $%d).", GetRPName(playerid), GetRPName(owner), fine);
	Faction_MessageLaw(msg);
	return true;
}

stock bool:Cmd_Unimpound(playerid)
{
	if (Garage_GetNearby(playerid, GARAGE_IMPOUND, 3.0) == -1) return SendClientMessage(playerid, COLOR_GREY, "Подойди к штрафстоянке.");
	if (gImpoundFine[playerid] <= 0) return SendClientMessage(playerid, COLOR_GREY, "Твоей машины здесь нет.");
	// кто, когда и за что отправил машину сюда
	new by[MAX_PLAYER_NAME], when[24], reason[64];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT IFNULL(imp_by, ''), strftime('%q', imp_date, 'unixepoch', 'localtime'), IFNULL(imp_reason, ''), IFNULL(imp_date, 0) FROM vehicles WHERE owner = %d",
		"%d.%m.%Y %H:%M", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			DB_GetFieldString(r, 0, by, sizeof by);
			if (DB_GetFieldInt(r, 3) > 0) DB_GetFieldString(r, 1, when, sizeof when);
			DB_GetFieldString(r, 2, reason, sizeof reason);
		}
		DB_FreeResultSet(r);
	}
	if (by[0] == '\0') strcat(by, "неизвестно");
	if (when[0] == '\0') strcat(when, "неизвестно");
	if (reason[0] == '\0') strcat(reason, "не указана");
	ShowPlayerDialog(playerid, DIALOG_UNIMPOUND, DIALOG_STYLE_MSGBOX, "Штрафстоянка",
		"{FFFFFF}Твоя машина на штрафстоянке.\n\nОтправил: {FFFF00}%s\n{FFFFFF}Когда: {FFFF00}%s\n{FFFFFF}Причина: {FFFF00}%s\n{FFFFFF}Штраф: {FFFF00}$%d\n\n{FFFFFF}Оплатить и забрать?",
		"Оплатить", "Отмена", by, when, reason, gImpoundFine[playerid]);
	return true;
}

// ALT пешком (keys.inc): 1 — игрок на штрафстоянке
stock Garage_TryImpoundLot(playerid)
{
	if (Garage_GetNearby(playerid, GARAGE_IMPOUND, 3.0) == -1) return 0;
	Cmd_Unimpound(playerid);
	return 1;
}

static Garage_Unimpound(playerid)
{
	new g = Garage_GetNearby(playerid, GARAGE_IMPOUND, 3.0);
	new fine = gImpoundFine[playerid];
	if (g == -1 || fine <= 0) return 0;
	if (!Garage_Pay(playerid, fine, "штраф штрафстоянки")) return 0;
	Econ_ToTreasury(fine);

	new Float:x = Garage[g][gaX] + 4.0, Float:y = Garage[g][gaY], Float:z = Garage[g][gaZ] + 0.5;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE vehicles SET impound = 0, x = %f, y = %f, z = %f, a = 0.0 WHERE owner = %d",
		x, y, z, Player[playerid][pDbId]));
	gImpoundFine[playerid] = 0;
	new v = Vehicles_SpawnFromDb(playerid);
	if (v != INVALID_VEHICLE_ID)
	{
		// бензин — сколько было при эвакуации
		new DBResult:r = DB_ExecuteQuery(gDB, "SELECT fuel FROM vehicles WHERE owner = %d", Player[playerid][pDbId]);
		if (r != DBResult:0)
		{
			if (DB_GetRowCount(r) > 0 && DB_GetFieldFloat(r, 0) >= 0.0) Fuel_Set(v, DB_GetFieldFloat(r, 0));
			DB_FreeResultSet(r);
		}
	}
	SendClientMessage(playerid, COLOR_GREEN, "Штраф $%d оплачен. Машина стоит рядом, припаркуй её: /park", fine);
	return 1;
}

// ---------------------------------------------------------------------
//  Админ-команды
// ---------------------------------------------------------------------
stock bool:Cmd_AddGarage(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, type;
	if (!NextIntParam(params, pos, type) || type < GARAGE_TUNE || type > GARAGE_IMPOUND)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /addgarage [1 — СТО, 2 — штрафстоянка]");
	if (gGarageCount >= MAX_GARAGES) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит (%d).", MAX_GARAGES);

	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO garages (type, x, y, z) VALUES (%d, %f, %f, %f)", type, x, y, z));
	new g = gGarageCount++;
	Garage[g][gaDbId] = DB_LastInsertId();
	Garage[g][gaType] = type;
	Garage[g][gaX] = x;
	Garage[g][gaY] = y;
	Garage[g][gaZ] = z;
	Garage_CreateMarker(g);
	Admin_Log(playerid, "addgarage", INVALID_PLAYER_ID, "", type);
	SendClientMessage(playerid, COLOR_ADMIN, "Точка №%d создана (тип %d).", Garage[g][gaDbId], type);
	return true;
}

stock bool:Cmd_DelGarage(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new g = -1;
	for (new i = 0; i < gGarageCount; i++)
	{
		if (IsPlayerInRangeOfPoint(playerid, GARAGE_RANGE, Garage[i][gaX], Garage[i][gaY], Garage[i][gaZ])) { g = i; break; }
	}
	if (g == -1) return SendClientMessage(playerid, COLOR_GREY, "Рядом нет СТО или штрафстоянки.");

	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM garages WHERE id = %d", Garage[g][gaDbId]));
	Marker_Destroy(Garage[g][gaMarker], true);
	Admin_Log(playerid, "delgarage", INVALID_PLAYER_ID, "", Garage[g][gaDbId]);
	SendClientMessage(playerid, COLOR_ADMIN, "Точка №%d удалена.", Garage[g][gaDbId]);

	// последний элемент на место удалённого
	gGarageCount--;
	if (g != gGarageCount) Garage[g] = Garage[gGarageCount];
	return true;
}
