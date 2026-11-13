// =====================================================================
//  admin_tools.inc — телепорты и инструменты администрации.
//
//  Телепорты:
//    /tp [название]  — в сохранённую точку. Без названия — окно со списком
//                      (удобно с телефона).
//    /addtp [название], /deltp [название] — сохранить / удалить точку (4+).
//    /back           — вернуться туда, где стоял до телепорта.
//    /gotocoord x y z [интерьер], /gotocar [id машины], /sendto [id] [точка].
//    Метка на карте (ESC > Карта, правая кнопка мыши) — админ сразу
//    переносится туда, с машиной, если за рулём. Включено у всех админов
//    по умолчанию, выключить/включить: /maptp.
//  Если админ за рулём — переносится вместе с машиной.
//
//  Инструменты:
//    /aduty — дежурство (бессмертие, розовый ник, видно в /admins)
//    /ao — объявление от администрации всем
//    /fixveh /flip — починить / перевернуть машину
//    /respawncars — вернуть пустые машины на места
//    /setweather — погода
//  Античит: джетпак у не-админа — кик и запись в лог.
//
//  Точки хранятся в таблице teleports (создаётся сама). При первом
//  запуске туда записываются основные места карты.
// =====================================================================

#define MAX_TELEPORTS           60
#define ADMIN_DUTY_COLOR        0xFF66FFFF
#define ADMIN_DUTY_HEALTH       100000.0

enum E_TELEPORT
{
	bool:tpUsed,
	tpDbId,
	tpName[24],
	Float:tpX,
	Float:tpY,
	Float:tpZ,
	Float:tpA,
	tpInt,
	tpWorld
}
static gTeleport[MAX_TELEPORTS][E_TELEPORT];
static gTpList[MAX_PLAYERS][MAX_TELEPORTS];     // строка окна -> слот
static gTpSendTarget[MAX_PLAYERS];              // кого отправляем через окно (/sendto без точки)
static gTpSendTargetDb[MAX_PLAYERS];

// Куда вернуть по /back
static bool:gBackSaved[MAX_PLAYERS];
static Float:gBackPos[MAX_PLAYERS][4];
static gBackInt[MAX_PLAYERS];
static gBackWorld[MAX_PLAYERS];

static bool:gMapTpOff[MAX_PLAYERS];             // админ выключил телепорт по метке
static gMapTpVeh[MAX_PLAYERS];                  // машина, которую ставим под админа после метки
static bool:gADuty[MAX_PLAYERS];
static bool:gJetpackFlag[MAX_PLAYERS];

// =====================================================================
//  Запуск
// =====================================================================
stock Teleports_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS teleports (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE COLLATE NOCASE, x REAL, y REAL, z REAL, a REAL, interior INTEGER DEFAULT 0, world INTEGER DEFAULT 0, creator TEXT DEFAULT '')"));

	new count = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM teleports");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (count == 0)
	{
		// Основные места — один раз, дальше админы правят сами (/addtp /deltp)
		Teleports_Seed("spawn",      SPAWN_X, SPAWN_Y, SPAWN_Z, SPAWN_A);
		Teleports_Seed("cityhall",   1481.0, -1745.0, 13.55, 0.0);
		Teleports_Seed("autoschool", LIC_SCHOOL_X, LIC_SCHOOL_Y, LIC_SCHOOL_Z, 0.0);
		Teleports_Seed("lspd",       1544.0, -1675.5, 13.56, 90.0);
		Teleports_Seed("hospital",   HOSPITAL_X, HOSPITAL_Y, HOSPITAL_Z, 0.0);
		Teleports_Seed("port",       LOADER_JOB_X, LOADER_JOB_Y, LOADER_JOB_Z, 0.0);
		Teleports_Seed("unity",      TAXI_JOB_X, TAXI_JOB_Y, TAXI_JOB_Z, 0.0);
		Teleports_Seed("idlewood",   PIZZA_JOB_X, PIZZA_JOB_Y, PIZZA_JOB_Z, 0.0);
		Teleports_Seed("lsair",      1682.0, -2286.0, 13.55, 0.0);
		Teleports_Seed("sf",         -1972.0, 138.0, 27.7, 90.0);
		Teleports_Seed("sfair",      -1422.0, -287.0, 14.15, 135.0);
		Teleports_Seed("lv",         2027.0, 1008.0, 10.82, 270.0);
		Teleports_Seed("lvair",      1319.0, 1260.0, 10.82, 0.0);
		Teleports_Seed("chiliad",    -2324.0, -1626.0, 483.7, 0.0);
		print("[admin] Default teleports created.");
	}
	Teleports_Load();
}

static Teleports_Seed(const name[], Float:x, Float:y, Float:z, Float:a)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR IGNORE INTO teleports (name, x, y, z, a, creator) VALUES ('%q', %f, %f, %f, %f, 'Сервер')",
		name, x, y, z, a));
}

static Teleports_Load()
{
	for (new i = 0; i < MAX_TELEPORTS; i++) gTeleport[i][tpUsed] = false;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, name, x, y, z, a, interior, world FROM teleports ORDER BY name LIMIT %d", MAX_TELEPORTS);
	if (r == DBResult:0) return;
	new rows = DB_GetRowCount(r);
	for (new i = 0; i < rows; i++)
	{
		gTeleport[i][tpUsed] = true;
		gTeleport[i][tpDbId] = DB_GetFieldInt(r, 0);
		DB_GetFieldString(r, 1, gTeleport[i][tpName], 24);
		gTeleport[i][tpX] = DB_GetFieldFloat(r, 2);
		gTeleport[i][tpY] = DB_GetFieldFloat(r, 3);
		gTeleport[i][tpZ] = DB_GetFieldFloat(r, 4);
		gTeleport[i][tpA] = DB_GetFieldFloat(r, 5);
		gTeleport[i][tpInt] = DB_GetFieldInt(r, 6);
		gTeleport[i][tpWorld] = DB_GetFieldInt(r, 7);
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
}

static Teleports_Find(const name[])
{
	for (new i = 0; i < MAX_TELEPORTS; i++)
		if (gTeleport[i][tpUsed] && !strcmp(gTeleport[i][tpName], name, true)) return i;
	return -1;
}

// =====================================================================
//  События (вызываются из admin.inc и rp.pwn)
// =====================================================================
stock AdminTools_OnConnect(playerid)
{
	gBackSaved[playerid] = false;
	gMapTpOff[playerid] = false;
	gMapTpVeh[playerid] = INVALID_VEHICLE_ID;
	gADuty[playerid] = false;
	gJetpackFlag[playerid] = false;
	gTpSendTarget[playerid] = INVALID_PLAYER_ID;
}

stock AdminTools_OnSpawn(playerid)
{
	if (gADuty[playerid])
	{
		SetPlayerHealth(playerid, ADMIN_DUTY_HEALTH);
		SetPlayerColor(playerid, ADMIN_DUTY_COLOR);
	}
}

// Из OnPlayerUpdate — только дешёвые проверки
stock AdminTools_OnUpdate(playerid)
{
	if (gJetpackFlag[playerid] || Player[playerid][pAdmin] > 0) return;
	if (!Player[playerid][pLogged]) return;
	if (GetPlayerSpecialAction(playerid) == SPECIAL_ACTION_USEJETPACK)
	{
		gJetpackFlag[playerid] = true;
		Admin_Log(INVALID_PLAYER_ID, "anticheat kick", playerid, "джетпак");
		new msg[144];
		format(msg, sizeof msg, "[Античит] %s[%d] кикнут: джетпак.", GetRPName(playerid), playerid);
		SendAdminMessage(COLOR_RED, msg);
		SendClientMessage(playerid, COLOR_RED, "Ты кикнут античитом (джетпак). Если это ошибка — напиши на форум.");
		KickWithDelay(playerid);
	}
}

// Метка на карте (OnPlayerClickMap). Высота z от клиента часто 0 (место ещё
// не прогружено), поэтому ставим админа через SetPlayerPosFindZ — клиент сам
// находит землю, а машину подтягиваем под него через полсекунды.
forward MapTp_PlaceVehicle(playerid, vehicleid);
stock AdminTools_OnClickMap(playerid, Float:x, Float:y, Float:z)
{
	if (gMapTpOff[playerid] || Player[playerid][pAdmin] < 1) return;
	if (Admin_IsSpectating(playerid)) return;
	if (Admin_IsInAJail(playerid)) return;

	Teleport_SaveBack(playerid);
	new v = GetPlayerVehicleID(playerid);
	new bool:driver = (v != 0 && GetPlayerState(playerid) == PLAYER_STATE_DRIVER);
	if (v != 0) RemovePlayerFromVehicle(playerid);

	SetPlayerInterior(playerid, 0);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerPosFindZ(playerid, x, y, (z < 1.0) ? 300.0 : z + 1.0);
	Player[playerid][pHouseInside] = -1;

	if (driver)
	{
		gMapTpVeh[playerid] = v;
		SetTimerEx("MapTp_PlaceVehicle", 600, false, "ii", playerid, v);
	}
	SendClientMessage(playerid, COLOR_ADMIN, "Телепорт по метке. Вернуться: /back   Выключить: /maptp");
}

public MapTp_PlaceVehicle(playerid, vehicleid)
{
	if (!IsPlayerConnected(playerid) || gMapTpVeh[playerid] != vehicleid || GetVehicleModel(vehicleid) == 0) return 0;
	gMapTpVeh[playerid] = INVALID_VEHICLE_ID;
	if (IsVehicleOccupied(vehicleid)) return 0;
	new Float:px, Float:py, Float:pz, Float:a;
	GetPlayerPos(playerid, px, py, pz);
	GetPlayerFacingAngle(playerid, a);
	LinkVehicleToInterior(vehicleid, 0);
	SetVehicleVirtualWorld(vehicleid, 0);
	SetVehiclePos(vehicleid, px, py, pz + 0.5);
	SetVehicleZAngle(vehicleid, a);
	PutPlayerInVehicle(playerid, vehicleid, 0);
	return 1;
}

stock bool:Admin_IsOnDuty(playerid)
{
	return gADuty[playerid];
}

// =====================================================================
//  Телепорт
// =====================================================================
stock Teleport_SaveBack(playerid)
{
	GetPlayerPos(playerid, gBackPos[playerid][0], gBackPos[playerid][1], gBackPos[playerid][2]);
	GetPlayerFacingAngle(playerid, gBackPos[playerid][3]);
	gBackInt[playerid] = GetPlayerInterior(playerid);
	gBackWorld[playerid] = GetPlayerVirtualWorld(playerid);
	gBackSaved[playerid] = true;
}

// Перенести игрока (водителя — вместе с машиной). a < 0 — не менять угол.
stock Teleport_Player(playerid, Float:x, Float:y, Float:z, Float:a, interior, world)
{
	new v = GetPlayerVehicleID(playerid);
	if (v != 0 && GetPlayerState(playerid) == PLAYER_STATE_DRIVER && interior == 0)
	{
		SetVehiclePos(v, x, y, z);
		if (a >= 0.0) SetVehicleZAngle(v, a);
		LinkVehicleToInterior(v, interior);
		SetVehicleVirtualWorld(v, world);
		SetPlayerInterior(playerid, interior);
		SetPlayerVirtualWorld(playerid, world);
		PutPlayerInVehicle(playerid, v, 0);
	}
	else
	{
		if (v != 0) RemovePlayerFromVehicle(playerid);
		SetPlayerInterior(playerid, interior);
		SetPlayerVirtualWorld(playerid, world);
		SetPlayerPos(playerid, x, y, z);
		if (a >= 0.0) SetPlayerFacingAngle(playerid, a);
		SetCameraBehindPlayer(playerid);
	}
	Player[playerid][pHouseInside] = -1;
}

static Teleport_ToPoint(playerid, slot)
{
	Teleport_Player(playerid, gTeleport[slot][tpX], gTeleport[slot][tpY], gTeleport[slot][tpZ], gTeleport[slot][tpA],
		gTeleport[slot][tpInt], gTeleport[slot][tpWorld]);
}

static Teleports_ShowList(playerid, const title[])
{
	new text[1600], line[48], n = 0;
	for (new i = 0; i < MAX_TELEPORTS; i++)
	{
		if (!gTeleport[i][tpUsed]) continue;
		format(line, sizeof line, "%s\n", gTeleport[i][tpName]);
		strcat(text, line);
		gTpList[playerid][n++] = i;
	}
	if (n == 0) return SendClientMessage(playerid, COLOR_GREY, "Точек телепорта нет. Добавить: /addtp [название]");
	ShowPlayerDialog(playerid, DIALOG_TPLIST, DIALOG_STYLE_LIST, title, "%s", "Телепорт", "Закрыть", text);
	return 1;
}

stock bool:Cmd_Tp(playerid, const params[])
{
	if (!Admin_Check(playerid, 1)) return true;
	new pos = 0, name[24];
	if (!NextParam(params, pos, name))
	{
		gTpSendTarget[playerid] = INVALID_PLAYER_ID;
		Teleports_ShowList(playerid, "Телепорты");
		return true;
	}
	new slot = Teleports_Find(name);
	if (slot == -1) return SendClientMessage(playerid, COLOR_GREY, "Точки \"%s\" нет. Список: /tp", name);
	Teleport_SaveBack(playerid);
	Teleport_ToPoint(playerid, slot);
	SendClientMessage(playerid, COLOR_ADMIN, "Ты в точке %s. Вернуться: /back", gTeleport[slot][tpName]);
	return true;
}

stock bool:Cmd_AddTp(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, name[24];
	if (!NextParam(params, pos, name)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /addtp [название одним словом]");
	if (strlen(name) > 20) return SendClientMessage(playerid, COLOR_GREY, "Название до 20 символов.");
	if (Teleports_Find(name) != -1) return SendClientMessage(playerid, COLOR_GREY, "Точка \"%s\" уже есть. Сначала /deltp %s", name, name);

	new free = -1;
	for (new i = 0; i < MAX_TELEPORTS; i++) if (!gTeleport[i][tpUsed]) { free = i; break; }
	if (free == -1) return SendClientMessage(playerid, COLOR_GREY, "Слишком много точек (максимум %d).", MAX_TELEPORTS);

	new Float:x, Float:y, Float:z, Float:a, admin[MAX_PLAYER_NAME];
	GetPlayerPos(playerid, x, y, z);
	GetPlayerFacingAngle(playerid, a);
	GetPlayerName(playerid, admin, sizeof admin);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO teleports (name, x, y, z, a, interior, world, creator) VALUES ('%q', %f, %f, %f, %f, %d, %d, '%q')",
		name, x, y, z, a, GetPlayerInterior(playerid), GetPlayerVirtualWorld(playerid), admin));
	Teleports_Load();
	Admin_Log(playerid, "addtp", INVALID_PLAYER_ID, name);
	SendClientMessage(playerid, COLOR_ADMIN, "Точка \"%s\" сохранена. Телепорт: /tp %s", name, name);
	return true;
}

stock bool:Cmd_DelTp(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, name[24];
	if (!NextParam(params, pos, name)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /deltp [название]");
	new slot = Teleports_Find(name);
	if (slot == -1) return SendClientMessage(playerid, COLOR_GREY, "Точки \"%s\" нет.", name);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM teleports WHERE id = %d", gTeleport[slot][tpDbId]));
	Admin_Log(playerid, "deltp", INVALID_PLAYER_ID, gTeleport[slot][tpName]);
	SendClientMessage(playerid, COLOR_ADMIN, "Точка \"%s\" удалена.", gTeleport[slot][tpName]);
	Teleports_Load();
	return true;
}

stock bool:Cmd_Back(playerid)
{
	if (!Admin_Check(playerid, 1)) return true;
	if (!gBackSaved[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Некуда возвращаться: ты ещё не телепортировался.");
	new Float:x = gBackPos[playerid][0], Float:y = gBackPos[playerid][1], Float:z = gBackPos[playerid][2], Float:a = gBackPos[playerid][3];
	new interior = gBackInt[playerid], world = gBackWorld[playerid];
	Teleport_SaveBack(playerid);     // повторный /back вернёт обратно
	Teleport_Player(playerid, x, y, z, a, interior, world);
	SendClientMessage(playerid, COLOR_ADMIN, "Ты вернулся на прошлое место.");
	return true;
}

stock bool:Cmd_GotoCoord(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new pos = 0, bx[16], by[16], bz[16], interior = 0;
	if (!NextParam(params, pos, bx) || !NextParam(params, pos, by) || !NextParam(params, pos, bz))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /gotocoord [x] [y] [z] [интерьер]");
	NextIntParam(params, pos, interior);
	Teleport_SaveBack(playerid);
	Teleport_Player(playerid, floatstr(bx), floatstr(by), floatstr(bz), -1.0, interior, 0);
	SendClientMessage(playerid, COLOR_ADMIN, "Телепорт на %.1f %.1f %.1f (интерьер %d). Вернуться: /back", floatstr(bx), floatstr(by), floatstr(bz), interior);
	return true;
}

// Стандартные интерьеры GTA: /gotoint [номер] [мир]
enum E_GTA_INT
{
	giName[32],
	giInt,
	Float:giX, Float:giY, Float:giZ
}
static const gGtaInts[][E_GTA_INT] =
{
	{ "Полиция LS",              6,  246.78,   63.90, 1003.64 },
	{ "Полиция SF",              10, 246.40,  110.84, 1003.22 },
	{ "Полиция LV",              3,  288.47,  170.06, 1007.17 },
	{ "Мэрия (Planning Dept.)",  3,  386.52,  173.64, 1008.38 },
	{ "Дом CJ",                  3,  2496.05, -1695.17, 1014.74 },
	{ "Дом Райдера",             2,  2447.87, -1704.45, 1013.50 },
	{ "Дом Свита",               1,  2527.02, -1679.21, 1015.50 },
	{ "Наркопритон Биг Смоука",  2,  2536.53, -1294.84, 1044.12 },
	{ "Мотель Джефферсон",       15, 2220.26, -1148.01, 1025.80 },
	{ "Особняк Мэдд Догга",      5,  1267.84, -776.96, 1091.91 },
	{ "Притон",                  5,  318.56,  1118.21, 1083.88 },
	{ "Склад",                   18, 1290.41,    1.95, 1001.02 },
	{ "Атриум",                  18, 1726.19, -1641.00,   20.23 },
	{ "Магазин 24/7",            17, -25.88,  -185.87, 1003.55 },
	{ "Бар Ten Green Bottles",   11, 501.98,   -69.15,  998.76 },
	{ "Binco",                   15, 207.74,  -109.02, 1005.13 },
	{ "Ammu-Nation",             1,  286.15,   -40.64, 1001.52 }
};

stock bool:Cmd_GotoInt(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new pos = 0, n, world = 0;
	if (!NextIntParam(params, pos, n) || n < 1 || n > sizeof (gGtaInts))
	{
		SendClientMessage(playerid, COLOR_GREY, "Использование: /gotoint [номер] [мир — необязательно]. Интерьеры:");
		new line[144], part[48];
		for (new i = 0; i < sizeof (gGtaInts); i++)
		{
			format(part, sizeof part, "%d %s   ", i + 1, gGtaInts[i][giName]);
			if (strlen(line) + strlen(part) >= sizeof (line) - 1)
			{
				SendClientMessage(playerid, COLOR_WHITE, line);
				line[0] = '\0';
			}
			strcat(line, part);
		}
		if (line[0] != '\0') SendClientMessage(playerid, COLOR_WHITE, line);
		return true;
	}
	NextIntParam(params, pos, world);
	if (world < 0) world = 0;
	n--;
	Teleport_SaveBack(playerid);
	Teleport_Player(playerid, gGtaInts[n][giX], gGtaInts[n][giY], gGtaInts[n][giZ], -1.0, gGtaInts[n][giInt], world);
	SendClientMessage(playerid, COLOR_ADMIN, "%s: интерьер %d, мир %d. Вернуться: /back", gGtaInts[n][giName], gGtaInts[n][giInt], world);
	return true;
}

stock bool:Cmd_GotoCar(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0, v;
	if (!NextIntParam(params, pos, v) || v < 1 || v >= MAX_VEHICLES || GetVehicleModel(v) == 0)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /gotocar [id машины]");
	new Float:x, Float:y, Float:z;
	GetVehiclePos(v, x, y, z);
	Teleport_SaveBack(playerid);
	if (IsPlayerInAnyVehicle(playerid)) RemovePlayerFromVehicle(playerid);
	SetPlayerInterior(playerid, GetVehicleInterior(v));
	SetPlayerVirtualWorld(playerid, GetVehicleVirtualWorld(v));
	SetPlayerPos(playerid, x + 2.0, y, z + 0.5);
	Player[playerid][pHouseInside] = -1;
	SendClientMessage(playerid, COLOR_ADMIN, "Ты у машины %d (модель %d). Вернуться: /back", v, GetVehicleModel(v));
	return true;
}

static Teleport_Send(playerid, target, slot)
{
	Teleport_ToPoint(target, slot);
	Admin_Log(playerid, "sendto", target, gTeleport[slot][tpName]);
	SendClientMessage(target, COLOR_ADMIN, "Администратор %s переместил тебя: %s.", GetRPName(playerid), gTeleport[slot][tpName]);
	SendClientMessage(playerid, COLOR_ADMIN, "%s отправлен в точку %s.", GetRPName(target), gTeleport[slot][tpName]);
}

stock bool:Cmd_SendTo(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0, name[24];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /sendto [id] [точка]  (без точки — список)");
	if (Admin_IsInAJail(target)) return SendClientMessage(playerid, COLOR_GREY, "Игрок в деморгане. Сначала /unajail.");
	if (Player[target][pJail] > 0) return SendClientMessage(playerid, COLOR_GREY, "Игрок в тюрьме.");
	if (!NextParam(params, pos, name))
	{
		gTpSendTarget[playerid] = target;
		gTpSendTargetDb[playerid] = Player[target][pDbId];
		new title[48];
		format(title, sizeof title, "Куда отправить %s", GetRPName(target));
		Teleports_ShowList(playerid, title);
		return true;
	}
	new slot = Teleports_Find(name);
	if (slot == -1) return SendClientMessage(playerid, COLOR_GREY, "Точки \"%s\" нет. Список: /tp", name);
	Teleport_Send(playerid, target, slot);
	return true;
}

stock bool:Cmd_MapTp(playerid)
{
	if (!Admin_Check(playerid, 1)) return true;
	gMapTpOff[playerid] = !gMapTpOff[playerid];
	if (!gMapTpOff[playerid]) SendClientMessage(playerid, COLOR_ADMIN, "Телепорт по метке включён: ESC > Карта, правая кнопка мыши. Выключить: /maptp");
	else SendClientMessage(playerid, COLOR_ADMIN, "Телепорт по метке выключен — метка снова обычная. Включить: /maptp");
	return true;
}

// =====================================================================
//  Инструменты
// =====================================================================
stock bool:Cmd_ADuty(playerid)
{
	if (!Admin_Check(playerid, 1)) return true;
	gADuty[playerid] = !gADuty[playerid];
	new msg[144];
	if (gADuty[playerid])
	{
		SetPlayerHealth(playerid, ADMIN_DUTY_HEALTH);
		SetPlayerColor(playerid, ADMIN_DUTY_COLOR);
		format(msg, sizeof msg, "[A] %s заступил на дежурство.", GetRPName(playerid));
	}
	else
	{
		SetPlayerHealth(playerid, 100.0);
		Faction_UpdateColor(playerid);      // вернуть цвет ника (factions.inc)
		format(msg, sizeof msg, "[A] %s ушёл с дежурства.", GetRPName(playerid));
	}
	SendAdminMessage(COLOR_ADMIN, msg);
	return true;
}

stock bool:Cmd_AO(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new text[110];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /ao [текст объявления]");
	SendClientMessageToAll(COLOR_ADMIN, "[Администрация] %s: %s", GetRPName(playerid), text);
	Admin_Log(playerid, "ao", INVALID_PLAYER_ID, text);
	return true;
}

// Машина, в которой сидит игрок (или сам админ, если id не указан)
static Admin_ParamVehicle(playerid, const params[], &target)
{
	new pos = 0;
	target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) target = playerid;
	return GetPlayerVehicleID(target);
}

stock bool:Cmd_FixVeh(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new target;
	new v = Admin_ParamVehicle(playerid, params, target);
	if (v == 0) return SendClientMessage(playerid, COLOR_GREY, "Использование: /fixveh [id игрока в машине] — или сядь в машину сам.");
	RepairVehicle(v);
	SetVehicleHealth(v, 1000.0);
	if (target != playerid)
	{
		Admin_Log(playerid, "fixveh", target);
		SendClientMessage(target, COLOR_ADMIN, "Администратор %s починил твою машину.", GetRPName(playerid));
	}
	SendClientMessage(playerid, COLOR_ADMIN, "Машина %d починена.", v);
	return true;
}

stock bool:Cmd_Flip(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new target;
	new v = Admin_ParamVehicle(playerid, params, target);
	if (v == 0) return SendClientMessage(playerid, COLOR_GREY, "Использование: /flip [id игрока в машине] — или сядь в машину сам.");
	new Float:x, Float:y, Float:z, Float:a;
	GetVehiclePos(v, x, y, z);
	GetVehicleZAngle(v, a);
	SetVehiclePos(v, x, y, z + 1.0);
	SetVehicleZAngle(v, a);
	SendClientMessage(playerid, COLOR_ADMIN, "Машина %d перевёрнута.", v);
	return true;
}

stock bool:Cmd_RespawnCars(playerid)
{
	if (!Admin_Check(playerid, 3)) return true;
	new respawned = 0, removed = 0;
	for (new v = 1; v < MAX_VEHICLES; v++)
	{
		if (GetVehicleModel(v) == 0 || IsVehicleOccupied(v)) continue;
		if (gVehAdmin[v])
		{
			Vehicle_Clear(v);
			DestroyVehicle(v);
			removed++;
		}
		else
		{
			SetVehicleToRespawn(v);
			respawned++;
		}
	}
	Admin_Log(playerid, "respawncars", INVALID_PLAYER_ID, "", respawned);
	SendClientMessageToAll(COLOR_ADMIN, "[Администрация] Пустой транспорт возвращён на места.");
	SendClientMessage(playerid, COLOR_ADMIN, "Возвращено машин: %d, удалено созданных через /veh: %d.", respawned, removed);
	return true;
}

stock bool:Cmd_SetWeather(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new pos = 0, weather;
	if (!NextIntParam(params, pos, weather) || weather < 0 || weather > 45)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setweather [0-45]  (1 — ясно, 8 — дождь, 9 — туман, 19 — песчаная буря)");
	SetWeather(weather);
	gServerWeather = weather;      // drugs.inc: после эффекта игроку вернётся эта погода
	Admin_Log(playerid, "setweather", INVALID_PLAYER_ID, "", weather);
	SendClientMessage(playerid, COLOR_ADMIN, "Погода изменена на %d.", weather);
	return true;
}

// =====================================================================
//  Окна и разбор команд (вызываются из admin.inc)
// =====================================================================
stock AdminTools_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid != DIALOG_TPLIST) return 0;
	if (!response || Player[playerid][pAdmin] < 1) return 1;
	if (listitem < 0 || listitem >= MAX_TELEPORTS) return 1;
	new slot = gTpList[playerid][listitem];
	if (!gTeleport[slot][tpUsed]) return 1;

	new target = gTpSendTarget[playerid];
	if (target != INVALID_PLAYER_ID)
	{
		gTpSendTarget[playerid] = INVALID_PLAYER_ID;
		if (!IsPlayerConnected(target) || Player[target][pDbId] != gTpSendTargetDb[playerid])
		{
			SendClientMessage(playerid, COLOR_GREY, "Игрок уже вышел.");
			return 1;
		}
		Teleport_Send(playerid, target, slot);
		return 1;
	}
	Teleport_SaveBack(playerid);
	Teleport_ToPoint(playerid, slot);
	SendClientMessage(playerid, COLOR_ADMIN, "Ты в точке %s. Вернуться: /back", gTeleport[slot][tpName]);
	return 1;
}

// Только для админов (проверка уровня в admin.inc уже прошла: pAdmin >= 1)
stock AdminTools_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/tp", true))          { Cmd_Tp(playerid, params); return 1; }
	if (!strcmp(cmd, "/tplist", true))      { Cmd_Tp(playerid, ""); return 1; }
	if (!strcmp(cmd, "/addtp", true))       { Cmd_AddTp(playerid, params); return 1; }
	if (!strcmp(cmd, "/deltp", true))       { Cmd_DelTp(playerid, params); return 1; }
	if (!strcmp(cmd, "/back", true))        { Cmd_Back(playerid); return 1; }
	if (!strcmp(cmd, "/gotocoord", true))   { Cmd_GotoCoord(playerid, params); return 1; }
	if (!strcmp(cmd, "/gotoint", true))     { Cmd_GotoInt(playerid, params); return 1; }
	if (!strcmp(cmd, "/gotocar", true))     { Cmd_GotoCar(playerid, params); return 1; }
	if (!strcmp(cmd, "/sendto", true))      { Cmd_SendTo(playerid, params); return 1; }
	if (!strcmp(cmd, "/maptp", true))       { Cmd_MapTp(playerid); return 1; }
	if (!strcmp(cmd, "/aduty", true))       { Cmd_ADuty(playerid); return 1; }
	if (!strcmp(cmd, "/ao", true))          { Cmd_AO(playerid, params); return 1; }
	if (!strcmp(cmd, "/fixveh", true))      { Cmd_FixVeh(playerid, params); return 1; }
	if (!strcmp(cmd, "/flip", true))        { Cmd_Flip(playerid, params); return 1; }
	if (!strcmp(cmd, "/respawncars", true)) { Cmd_RespawnCars(playerid); return 1; }
	if (!strcmp(cmd, "/setweather", true))  { Cmd_SetWeather(playerid, params); return 1; }
	return 0;
}
