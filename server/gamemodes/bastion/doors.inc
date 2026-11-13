// =====================================================================
//  doors.inc — входы в здания со стандартными интерьерами GTA.
//
//  У двери снаружи и внутри — маркер (стрелка) и надпись.
//  Войти/выйти: ALT рядом с маркером или /enter, /exit.
//  Новый вход — строчка в конце таблицы gDoors ниже (координаты: /pos).
//  Маркеры можно двигать в игре: /markers (место выхода — шаг перед маркером, куда он «смотрит»).
//
//  Общие входы, которые админ (4+) ставит в игре (раздел в конце файла):
//    /adddoor [название] — вход там, где стоишь (у двери, спиной к ней)
//    /setdoor [№]        — точка внутри там, где стоишь; /setdoor [№] out — переставить вход
//    /deldoor [№], /doors — убрать вход, список входов
//  Обе точки можно ставить и внутри дома или бизнеса (например, дверь из гостиной в подвал).
//  Так делается, например, общежитие: вход с улицы в коридор, а в коридоре — двери комнат (/createhouse).
// =====================================================================

#define DOOR_RANGE          1.5
#define DOOR_PICKUP_MODEL   1318    // белая стрелка
// Маркер стоит на 1.4 м позади точки появления (у самой двери), чтобы не войти/выйти дважды
#define Door_MarkerOffset   (1.4)

enum E_DOOR
{
	dName[32],
	Float:dOutX, Float:dOutY, Float:dOutZ, Float:dOutA,   // снаружи у двери (куда выходишь)
	Float:dInX, Float:dInY, Float:dInZ, Float:dInA,       // внутри у двери (куда входишь)
	dInterior,
	dWorld,
	dOutInterior, dOutWorld                                 // где стоит вход (0, 0 — улица)
}

static const gDoors[][E_DOOR] =
{
	// Полиция Лос-Сантоса: главный вход со стороны Pershing Square -> интерьер 6
	{ "Полиция LSPD", 1553.90, -1675.60, 16.19, 90.0,   246.78, 63.90, 1003.64, 0.0,   6, 0,   0, 0 },
	// Допросная LSPD: вход из коридора участка (комната в модели есть, но замурована).
	// Точки сняты /pos в игре: маркер в коридоре — 221.54, 66.86; внутри — 227.96, 67.70 (пол 1005.03).
	{ "Допросная", 220.45, 67.74, 1005.03, 51.3,   227.96, 67.70, 1005.03, 253.9,   6, 0,   6, 0 }
};


static gDoorMk[sizeof gDoors][2];      // маркеры двери: [0] снаружи, [1] внутри

stock Doors_Init()
{
	new label[64], name[32];
	for (new d = 0; d < sizeof gDoors; d++)
	{
		new Float:x, Float:y, Float:z = gDoors[d][dOutZ];
		Door_OuterMarker(d, x, y);
		gDoorMk[d][0] = Marker_Create(MK_DOOR, d * 2, gDoors[d][dName], DOOR_PICKUP_MODEL, x, y, z,
			gDoors[d][dOutWorld], gDoors[d][dOutInterior], gDoors[d][dOutInterior] ? GPS_NONE : GPS_GOV, gDoors[d][dOutA]);
		format(label, sizeof label, "{FFFF66}%s\n{FFFFFF}ALT — войти", gDoors[d][dName]);
		Marker_SetLabel(gDoorMk[d][0], label);

		z = gDoors[d][dInZ];
		Door_InnerMarker(d, x, y);
		format(name, sizeof name, "%s, выход", gDoors[d][dName]);
		gDoorMk[d][1] = Marker_Create(MK_DOOR, d * 2 + 1, name, DOOR_PICKUP_MODEL, x, y, z,
			gDoors[d][dWorld], gDoors[d][dInterior], GPS_NONE, gDoors[d][dInA]);
		Marker_SetLabel(gDoorMk[d][1], "{FFFFFF}ALT — выйти", COLOR_WHITE, 10.0);
	}
	printf("[Doors] %d entrances.", sizeof gDoors);
	PDoors_Load();
}

// Маркер снаружи — на шаг ближе к двери, чем точка выхода (по направлению взгляда назад)
static Door_OuterMarker(d, &Float:x, &Float:y)
{
	x = gDoors[d][dOutX] + Door_MarkerOffset * floatsin(gDoors[d][dOutA], degrees);
	y = gDoors[d][dOutY] - Door_MarkerOffset * floatcos(gDoors[d][dOutA], degrees);
}

static Door_InnerMarker(d, &Float:x, &Float:y)
{
	x = gDoors[d][dInX] + Door_MarkerOffset * floatsin(gDoors[d][dInA], degrees);
	y = gDoors[d][dInY] - Door_MarkerOffset * floatcos(gDoors[d][dInA], degrees);
}

// Войти или выйти, если игрок у маркера. true — сработало.
forward bool:Law_CellGateUse(playerid);     // law.inc: полицейский у решётки камеры
forward bool:FDoor_Try(playerid);           // factions.inc: входы в здания фракций (/setfint)
forward bool:PDoor_Try(playerid);           // общие входы из /adddoor (ниже)

stock bool:Doors_Try(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return false;
	if (Law_CellGateUse(playerid)) return true;
	if (FDoor_Try(playerid)) return true;
	if (PDoor_Try(playerid)) return true;
	for (new d = 0; d < sizeof gDoors; d++)
	{
		if (Marker_InRange(playerid, gDoorMk[d][0], DOOR_RANGE))
		{
			if (Law_TryLeave(playerid)) Door_SpawnAt(playerid, gDoorMk[d][1]);
			return true;
		}
		if (Marker_InRange(playerid, gDoorMk[d][1], DOOR_RANGE))
		{
			// камера в том же здании: заключённого дверь выпускает, только если её открыл
			// полицейский (/celldoor) или он договорился (/bribe) — law.inc
			if (Law_TryLeave(playerid)) Door_SpawnAt(playerid, gDoorMk[d][0]);
			return true;
		}
	}
	return false;
}

// Поставить игрока на шаг перед маркером двери, лицом по углу маркера
stock Door_SpawnAt(playerid, m)
{
	new Float:x, Float:y, Float:z;
	if (!Marker_GetPos(m, x, y, z)) return;
	new Float:a = Marker_GetAngle(m);
	x -= Door_MarkerOffset * floatsin(a, degrees);
	y += Door_MarkerOffset * floatcos(a, degrees);
	SetPlayerInterior(playerid, Marker_GetInterior(m));
	SetPlayerVirtualWorld(playerid, Marker_GetWorld(m));
	Player[playerid][pHouseInside] = House_ByWorld(Marker_GetWorld(m));   // вход может вести в дом
	SetPlayerPos(playerid, x, y, z);
	SetPlayerFacingAngle(playerid, a);
	SetCameraBehindPlayer(playerid);
}

// =====================================================================
//  Общие входы, поставленные в игре (/adddoor). Хранятся в таблице doors.
// =====================================================================
#define MAX_PDOORS          60
#define PDOOR_REF           2000    // ref маркеров MK_DOOR: 2000 + номер входа * 2 (внутри: + 1)
// Мир интерьера дома зависит от его места в списке и меняется, когда дом сносят.
// Поэтому в базе для точки внутри дома хранится PDOOR_HOUSE_W + номер дома, а мир считается при постройке.
#define PDOOR_HOUSE_W       1000000
#define PDOOR_NOWHERE       999999  // дом с этим входом снесён: маркер в пустом мире

enum E_PDOOR
{
	pdDbId,
	pdName[32],
	Float:pdOX, Float:pdOY, Float:pdOZ, Float:pdOA, pdOInt, pdOWorld,
	bool:pdIn,                  // точка внутри задана
	Float:pdIX, Float:pdIY, Float:pdIZ, Float:pdIA, pdIInt, pdIWorld,
	pdMkOut, pdMkIn
}
static gPDoor[MAX_PDOORS][E_PDOOR];
static gPDoorCount = 0;

// Мир игрока -> как хранить; хранимое -> настоящий мир
static PDoor_EncWorld(world)
{
	new dbid = House_DbIdByWorld(world);
	return (dbid > 0) ? PDOOR_HOUSE_W + dbid : world;
}
static PDoor_DecWorld(stored)
{
	if (stored < PDOOR_HOUSE_W) return stored;
	new w = House_WorldByDbId(stored - PDOOR_HOUSE_W);
	return (w == -1) ? PDOOR_NOWHERE : w;
}

static PDoor_Build(d)
{
	if (gPDoor[d][pdMkOut] != -1) { Marker_Destroy(gPDoor[d][pdMkOut]); gPDoor[d][pdMkOut] = -1; }
	if (gPDoor[d][pdMkIn] != -1)  { Marker_Destroy(gPDoor[d][pdMkIn]);  gPDoor[d][pdMkIn] = -1; }

	new label[96], name[32];
	new Float:x = gPDoor[d][pdOX], Float:y = gPDoor[d][pdOY], Float:z = gPDoor[d][pdOZ];
	gPDoor[d][pdMkOut] = Marker_Create(MK_DOOR, PDOOR_REF + gPDoor[d][pdDbId] * 2, gPDoor[d][pdName], DOOR_PICKUP_MODEL, x, y, z,
		PDoor_DecWorld(gPDoor[d][pdOWorld]), gPDoor[d][pdOInt], GPS_NONE, gPDoor[d][pdOA]);
	if (gPDoor[d][pdIn]) format(label, sizeof label, "{FFFF66}%s\n{FFFFFF}ALT — войти", gPDoor[d][pdName]);
	else format(label, sizeof label, "{FFFF66}%s\n{AAAAAA}вход №%d ещё не готов: /setdoor %d внутри", gPDoor[d][pdName], gPDoor[d][pdDbId], gPDoor[d][pdDbId]);
	Marker_SetLabel(gPDoor[d][pdMkOut], label);
	if (!gPDoor[d][pdIn]) return;

	x = gPDoor[d][pdIX]; y = gPDoor[d][pdIY]; z = gPDoor[d][pdIZ];
	strcat(name, gPDoor[d][pdName], 22);
	strcat(name, ", выход");
	gPDoor[d][pdMkIn] = Marker_Create(MK_DOOR, PDOOR_REF + gPDoor[d][pdDbId] * 2 + 1, name, DOOR_PICKUP_MODEL, x, y, z,
		PDoor_DecWorld(gPDoor[d][pdIWorld]), gPDoor[d][pdIInt], GPS_NONE, gPDoor[d][pdIA]);
	Marker_SetLabel(gPDoor[d][pdMkIn], "{FFFFFF}ALT — выйти", COLOR_WHITE, 10.0);
}

static PDoor_Save(d)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"UPDATE doors SET ox = %f, oy = %f, oz = %f, oa = %f, oint = %d, oworld = %d, has_in = %d, ix = %f, iy = %f, iz = %f, ia = %f, iint = %d, iworld = %d WHERE id = %d",
		gPDoor[d][pdOX], gPDoor[d][pdOY], gPDoor[d][pdOZ], gPDoor[d][pdOA], gPDoor[d][pdOInt], gPDoor[d][pdOWorld],
		gPDoor[d][pdIn] ? 1 : 0, gPDoor[d][pdIX], gPDoor[d][pdIY], gPDoor[d][pdIZ], gPDoor[d][pdIA], gPDoor[d][pdIInt], gPDoor[d][pdIWorld],
		gPDoor[d][pdDbId]));
}

// Забыть правки маркеров из /markers: точку поставили заново или вход убрали
static PDoor_ForgetMarkers(dbid)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM markers WHERE type = %d AND ref IN (%d, %d)",
		MK_DOOR, PDOOR_REF + dbid * 2, PDOOR_REF + dbid * 2 + 1));
}

static PDoor_FindByDbId(dbid)
{
	for (new d = 0; d < gPDoorCount; d++)
	{
		if (gPDoor[d][pdDbId] == dbid) return d;
	}
	return -1;
}

stock PDoors_Load()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS doors (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, ox REAL DEFAULT 0, oy REAL DEFAULT 0, oz REAL DEFAULT 0, oa REAL DEFAULT 0, oint INTEGER DEFAULT 0, oworld INTEGER DEFAULT 0, has_in INTEGER DEFAULT 0, ix REAL DEFAULT 0, iy REAL DEFAULT 0, iz REAL DEFAULT 0, ia REAL DEFAULT 0, iint INTEGER DEFAULT 0, iworld INTEGER DEFAULT 0)"));
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, name, ox, oy, oz, oa, oint, oworld, has_in, ix, iy, iz, ia, iint, iworld FROM doors ORDER BY id");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows && gPDoorCount < MAX_PDOORS; i++)
		{
			new d = gPDoorCount++;
			gPDoor[d][pdDbId]   = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 1, gPDoor[d][pdName], 32);
			gPDoor[d][pdOX]     = DB_GetFieldFloat(r, 2);
			gPDoor[d][pdOY]     = DB_GetFieldFloat(r, 3);
			gPDoor[d][pdOZ]     = DB_GetFieldFloat(r, 4);
			gPDoor[d][pdOA]     = DB_GetFieldFloat(r, 5);
			gPDoor[d][pdOInt]   = DB_GetFieldInt(r, 6);
			gPDoor[d][pdOWorld] = DB_GetFieldInt(r, 7);
			gPDoor[d][pdIn]     = DB_GetFieldInt(r, 8) != 0;
			gPDoor[d][pdIX]     = DB_GetFieldFloat(r, 9);
			gPDoor[d][pdIY]     = DB_GetFieldFloat(r, 10);
			gPDoor[d][pdIZ]     = DB_GetFieldFloat(r, 11);
			gPDoor[d][pdIA]     = DB_GetFieldFloat(r, 12);
			gPDoor[d][pdIInt]   = DB_GetFieldInt(r, 13);
			gPDoor[d][pdIWorld] = DB_GetFieldInt(r, 14);
			gPDoor[d][pdMkOut] = -1;
			gPDoor[d][pdMkIn] = -1;
			PDoor_Build(d);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Doors] %d entrances placed in game.", gPDoorCount);
}

// Дом снесли, остальные дома сменили мир: пересобрать маркеры входов
stock PDoors_Rebuild()
{
	for (new d = 0; d < gPDoorCount; d++) PDoor_Build(d);
}

// ALT, /enter, /exit у общего входа (из Doors_Try). true — сработало.
stock bool:PDoor_Try(playerid)
{
	for (new d = 0; d < gPDoorCount; d++)
	{
		if (!gPDoor[d][pdIn]) continue;
		if (Marker_InRange(playerid, gPDoor[d][pdMkOut], DOOR_RANGE))
		{
			if (Law_TryLeave(playerid)) Door_SpawnAt(playerid, gPDoor[d][pdMkIn]);
			return true;
		}
		if (Marker_InRange(playerid, gPDoor[d][pdMkIn], DOOR_RANGE))
		{
			if (Law_TryLeave(playerid)) Door_SpawnAt(playerid, gPDoor[d][pdMkOut]);
			return true;
		}
	}
	return false;
}

stock bool:Cmd_AddDoor(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new name[32];
	if (!RestParam(params, 0, name))
	{
		SendClientMessage(playerid, COLOR_GREY, "Использование: /adddoor [название] — вход в здание там, где ты стоишь (у двери, спиной к ней)");
		SendClientMessage(playerid, COLOR_GREY, "Потом зайди в интерьер (/gotoint) и поставь точку внутри: /setdoor [№]. Список: /doors");
		return true;
	}
	if (gPDoorCount >= MAX_PDOORS) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит входов (%d).", MAX_PDOORS);

	new d = gPDoorCount++;
	gPDoor[d][pdName][0] = '\0';
	strcat(gPDoor[d][pdName], name, 32);
	GetPlayerPos(playerid, gPDoor[d][pdOX], gPDoor[d][pdOY], gPDoor[d][pdOZ]);
	GetPlayerFacingAngle(playerid, gPDoor[d][pdOA]);
	gPDoor[d][pdOInt] = GetPlayerInterior(playerid);
	gPDoor[d][pdOWorld] = PDoor_EncWorld(GetPlayerVirtualWorld(playerid));
	gPDoor[d][pdIn] = false;
	gPDoor[d][pdMkOut] = -1;
	gPDoor[d][pdMkIn] = -1;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO doors (name) VALUES ('%q')", name));
	gPDoor[d][pdDbId] = DB_LastInsertId();
	PDoor_Save(d);
	PDoor_ForgetMarkers(gPDoor[d][pdDbId]);
	PDoor_Build(d);
	SendClientMessage(playerid, COLOR_ADMIN, "Вход №%d «%s» поставлен. Теперь зайди в интерьер (/gotoint) и введи /setdoor %d у двери, спиной к ней.",
		gPDoor[d][pdDbId], name, gPDoor[d][pdDbId]);
	return true;
}

stock bool:Cmd_SetDoor(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, dbid, side[8];
	if (!NextIntParam(params, pos, dbid))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setdoor [№] — точка внутри там, где ты стоишь; /setdoor [№] out — переставить вход. Номера: /doors");
	new d = PDoor_FindByDbId(dbid);
	if (d == -1) return SendClientMessage(playerid, COLOR_GREY, "Входа с таким номером нет. Список: /doors");
	new bool:outside = false;
	if (NextParam(params, pos, side))
	{
		if (!strcmp(side, "out", true)) outside = true;
		else if (strcmp(side, "in", true)) return SendClientMessage(playerid, COLOR_GREY, "Сторона: out (вход) или in (точка внутри).");
	}
	new Float:x, Float:y, Float:z, Float:a;
	GetPlayerPos(playerid, x, y, z);
	GetPlayerFacingAngle(playerid, a);
	if (outside)
	{
		gPDoor[d][pdOX] = x; gPDoor[d][pdOY] = y; gPDoor[d][pdOZ] = z; gPDoor[d][pdOA] = a;
		gPDoor[d][pdOInt] = GetPlayerInterior(playerid);
		gPDoor[d][pdOWorld] = PDoor_EncWorld(GetPlayerVirtualWorld(playerid));
	}
	else
	{
		gPDoor[d][pdIn] = true;
		gPDoor[d][pdIX] = x; gPDoor[d][pdIY] = y; gPDoor[d][pdIZ] = z; gPDoor[d][pdIA] = a;
		gPDoor[d][pdIInt] = GetPlayerInterior(playerid);
		gPDoor[d][pdIWorld] = PDoor_EncWorld(GetPlayerVirtualWorld(playerid));
	}
	PDoor_Save(d);
	PDoor_ForgetMarkers(dbid);
	PDoor_Build(d);
	if (outside) SendClientMessage(playerid, COLOR_ADMIN, "Вход №%d «%s»: точка снаружи переставлена.", dbid, gPDoor[d][pdName]);
	else SendClientMessage(playerid, COLOR_ADMIN, "Вход №%d «%s»: точка внутри сохранена, вход работает (ALT у стрелки).", dbid, gPDoor[d][pdName]);
	return true;
}

stock bool:Cmd_DelDoor(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, dbid;
	if (!NextIntParam(params, pos, dbid)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /deldoor [№] (номера: /doors)");
	new d = PDoor_FindByDbId(dbid);
	if (d == -1) return SendClientMessage(playerid, COLOR_GREY, "Входа с таким номером нет. Список: /doors");
	if (gPDoor[d][pdMkOut] != -1) Marker_Destroy(gPDoor[d][pdMkOut]);
	if (gPDoor[d][pdMkIn] != -1) Marker_Destroy(gPDoor[d][pdMkIn]);
	PDoor_ForgetMarkers(dbid);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM doors WHERE id = %d", dbid));
	SendClientMessage(playerid, COLOR_ADMIN, "Вход №%d «%s» убран. Если внутри стоят дома, перенеси или снеси их (/delhouse).", dbid, gPDoor[d][pdName]);
	new last = gPDoorCount - 1;
	if (d != last) gPDoor[d] = gPDoor[last];
	gPDoorCount--;
	return true;
}

stock bool:Cmd_Doors(playerid)
{
	if (!Admin_Check(playerid, 3)) return true;
	if (gPDoorCount == 0) return SendClientMessage(playerid, COLOR_GREY, "Входов, поставленных в игре, нет. Добавить: /adddoor [название]");
	SendClientMessage(playerid, COLOR_YELLOW, "Входы в здания (/adddoor):");
	for (new d = 0; d < gPDoorCount; d++)
	{
		if (gPDoor[d][pdIn])
			SendClientMessage(playerid, COLOR_WHITE, " №%d — %s (%.0f, %.0f), внутри интерьер %d, мир %d", gPDoor[d][pdDbId], gPDoor[d][pdName],
				gPDoor[d][pdOX], gPDoor[d][pdOY], gPDoor[d][pdIInt], gPDoor[d][pdIWorld]);
		else
			SendClientMessage(playerid, COLOR_WHITE, " №%d — %s (%.0f, %.0f), точки внутри ещё нет: /setdoor %d", gPDoor[d][pdDbId], gPDoor[d][pdName],
				gPDoor[d][pdOX], gPDoor[d][pdOY], gPDoor[d][pdDbId]);
	}
	return true;
}
