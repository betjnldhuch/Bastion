// =====================================================================
//  dev.inc — инструменты разработки прямо в игре (админ DEV_LEVEL+).
//
//  Координаты:
//    /pos             — где я стою (или где моя машина), в чат
//    /pos [название]  — то же + сохранить точку. Все точки пишутся в файл
//                       scriptfiles/bastion_points.txt — его можно просто
//                       переслать разработчику.
//    /points          — список сохранённых точек (выбрать — телепорт)
//    /delpoint [название]
//    /dev             — живые координаты на экране (вкл/выкл)
//
//  Маппинг (свои объекты на карте, хранятся в таблице map_objects):
//    /mo new [модель]  — поставить объект перед собой и сразу двигать
//    /mo sel           — выбрать объект мышью (ПК)
//    /mo edit [№]      — двигать объект мышью (без номера — ближайший)
//    /mo move x y z    — сдвинуть на x y z метров (удобно с телефона)
//    /mo rot x y z     — повернуть на x y z градусов
//    /mo here          — перенести объект к себе под ноги
//    /mo copy [№]      — копия рядом   /mo del [№] — удалить
//    /mo near          — объекты рядом с номерами
//    /mo remove [радиус] — убрать стандартные здания GTA вокруг себя
//    /mo unremove      — отменить последнее удаление зданий (видно после перезахода)
//    /mo export        — выгрузить карту в scriptfiles/map_export.txt
//  Перекраска объектов (текстуры, цвет, надписи) — /mt, см. textures.inc
//  /mo без параметров — справка.
// =====================================================================

#define DEV_LEVEL           4
#define MAX_MAP_OBJECTS     900
#define MAX_MAP_REMOVALS    300
#define MAP_DRAW_DISTANCE   300.0
#define DEV_POINTS_FILE     "bastion_points.txt"
#define MAP_EXPORT_FILE     "map_export.txt"


static gMapDbId[MAX_MAP_OBJECTS];
static gMapObj[MAX_MAP_OBJECTS] = { INVALID_OBJECT_ID, ... };
static gMapCount = 0;

enum E_MAP_REMOVAL
{
	mrDbId,
	mrModel,
	Float:mrX, Float:mrY, Float:mrZ, Float:mrR
}
static gMapRemoval[MAX_MAP_REMOVALS][E_MAP_REMOVAL];
static gMapRemovalCount = 0;

static gEditSlot[MAX_PLAYERS] = { -1, ... };    // объект, который игрок сейчас двигает
static bool:gDevHud[MAX_PLAYERS];
static PlayerText:gDevTD[MAX_PLAYERS] = { INVALID_PLAYER_TEXT_DRAW, ... };
static gPointList[MAX_PLAYERS][40];             // строка окна -> id точки

// ---------------------------------------------------------------------
//  Запуск
// ---------------------------------------------------------------------
stock Dev_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS dev_points (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, x REAL, y REAL, z REAL, a REAL, interior INTEGER, world INTEGER, author TEXT, time INTEGER)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS map_objects (id INTEGER PRIMARY KEY AUTOINCREMENT, model INTEGER, x REAL, y REAL, z REAL, rx REAL, ry REAL, rz REAL)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS map_removals (id INTEGER PRIMARY KEY AUTOINCREMENT, model INTEGER, x REAL, y REAL, z REAL, radius REAL)"));

	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, model, x, y, z, rx, ry, rz FROM map_objects ORDER BY id");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows && gMapCount < MAX_MAP_OBJECTS; i++)
		{
			new o = CreateObject(DB_GetFieldInt(r, 1), DB_GetFieldFloat(r, 2), DB_GetFieldFloat(r, 3), DB_GetFieldFloat(r, 4),
				DB_GetFieldFloat(r, 5), DB_GetFieldFloat(r, 6), DB_GetFieldFloat(r, 7), MAP_DRAW_DISTANCE);
			if (o != INVALID_OBJECT_ID)
			{
				gMapDbId[gMapCount] = DB_GetFieldInt(r, 0);
				gMapObj[gMapCount] = o;
				gMapCount++;
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}

	r = DB_ExecuteQuery(gDB, "SELECT id, model, x, y, z, radius FROM map_removals ORDER BY id");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows && gMapRemovalCount < MAX_MAP_REMOVALS; i++)
		{
			new m = gMapRemovalCount++;
			gMapRemoval[m][mrDbId] = DB_GetFieldInt(r, 0);
			gMapRemoval[m][mrModel] = DB_GetFieldInt(r, 1);
			gMapRemoval[m][mrX] = DB_GetFieldFloat(r, 2);
			gMapRemoval[m][mrY] = DB_GetFieldFloat(r, 3);
			gMapRemoval[m][mrZ] = DB_GetFieldFloat(r, 4);
			gMapRemoval[m][mrR] = DB_GetFieldFloat(r, 5);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Dev] Map: %d objects, %d removed buildings.", gMapCount, gMapRemovalCount);
	Tex_Init();           // перекраска объектов (textures.inc)
	SetTimer("Dev_HudTimer", 500, true);
}

// Из OnPlayerConnect: убрать удалённые здания GTA
stock Dev_OnConnect(playerid)
{
	gEditSlot[playerid] = -1;
	gDevHud[playerid] = false;
	gDevTD[playerid] = INVALID_PLAYER_TEXT_DRAW;
	for (new m = 0; m < gMapRemovalCount; m++)
	{
		RemoveBuildingForPlayer(playerid, gMapRemoval[m][mrModel], gMapRemoval[m][mrX], gMapRemoval[m][mrY], gMapRemoval[m][mrZ], gMapRemoval[m][mrR]);
	}
}

static bool:Dev_Check(playerid)
{
	return Admin_Check(playerid, DEV_LEVEL);
}

// ---------------------------------------------------------------------
//  Координаты
// ---------------------------------------------------------------------
static Dev_GetPos(playerid, &Float:x, &Float:y, &Float:z, &Float:a)
{
	new v = GetPlayerVehicleID(playerid);
	if (v != 0)
	{
		GetVehiclePos(v, x, y, z);
		GetVehicleZAngle(v, a);
		return v;
	}
	GetPlayerPos(playerid, x, y, z);
	GetPlayerFacingAngle(playerid, a);
	return 0;
}

stock bool:Cmd_Pos(playerid, const params[])
{
	if (!Dev_Check(playerid)) return true;
	new Float:x, Float:y, Float:z, Float:a;
	new v = Dev_GetPos(playerid, x, y, z, a);
	new interior = GetPlayerInterior(playerid), world = GetPlayerVirtualWorld(playerid);

	new name[32], pos = 0;
	while (params[pos] == ' ') pos++;
	strmid(name, params[pos], 0, sizeof name - 1);

	if (v != 0) SendClientMessage(playerid, COLOR_YELLOW, "Машина (модель %d): %.2f, %.2f, %.2f  угол %.1f  инт %d  мир %d", GetVehicleModel(v), x, y, z, a, interior, world);
	else SendClientMessage(playerid, COLOR_YELLOW, "Позиция: %.2f, %.2f, %.2f  угол %.1f  инт %d  мир %d", x, y, z, a, interior, world);
	if (name[0] == '\0')
	{
		SendClientMessage(playerid, COLOR_GREY, "Сохранить с названием: /pos [название], например /pos ферма_штаб");
		return true;
	}

	new author[MAX_PLAYER_NAME];
	GetPlayerName(playerid, author, sizeof author);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO dev_points (name, x, y, z, a, interior, world, author, time) VALUES ('%q', %f, %f, %f, %f, %d, %d, '%q', %d)",
		name, x, y, z, a, interior, world, author, gettime()));

	new line[160];
	if (v != 0) format(line, sizeof line, "%s: %.2f, %.2f, %.2f, %.1f (инт %d, мир %d) машина\r\n", name, x, y, z, a, interior, world);
	else format(line, sizeof line, "%s: %.2f, %.2f, %.2f, %.1f (инт %d, мир %d)\r\n", name, x, y, z, a, interior, world);
	new File:f = fopen(DEV_POINTS_FILE, io_append);
	if (f)
	{
		fwrite(f, line);
		fclose(f);
	}
	SendClientMessage(playerid, COLOR_GREEN, "Точка «%s» сохранена. Все точки: /points и файл scriptfiles/%s", name, DEV_POINTS_FILE);
	return true;
}

stock bool:Cmd_Points(playerid)
{
	if (!Dev_Check(playerid)) return true;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, name, x, y, z FROM dev_points ORDER BY id DESC LIMIT 40");
	if (r == DBResult:0) return true;
	new rows = DB_GetRowCount(r);
	if (rows == 0)
	{
		DB_FreeResultSet(r);
		return SendClientMessage(playerid, COLOR_GREY, "Точек пока нет. Сохранить: /pos [название]");
	}
	new list[2400] = "Название\tКоординаты\n", line[96], name[32];
	for (new i = 0; i < rows; i++)
	{
		gPointList[playerid][i] = DB_GetFieldInt(r, 0);
		DB_GetFieldString(r, 1, name, sizeof name);
		format(line, sizeof line, "%s\t%.1f, %.1f, %.1f\n", name, DB_GetFieldFloat(r, 2), DB_GetFieldFloat(r, 3), DB_GetFieldFloat(r, 4));
		strcat(list, line);
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	ShowPlayerDialog(playerid, DIALOG_DEV_POINTS, DIALOG_STYLE_TABLIST_HEADERS, "Точки (выбери — телепорт)", "%s", "Туда", "Закрыть", list);
	return true;
}

stock bool:Cmd_DelPoint(playerid, const params[])
{
	if (!Dev_Check(playerid)) return true;
	new pos = 0, name[32];
	if (!NextParam(params, pos, name)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /delpoint [название]");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM dev_points WHERE name = '%q'", name));
	SendClientMessage(playerid, COLOR_WHITE, "Точка «%s» удалена из списка (в файле строка остаётся).", name);
	return true;
}

// Живые координаты на экране
stock bool:Cmd_Dev(playerid)
{
	if (!Dev_Check(playerid)) return true;
	gDevHud[playerid] = !gDevHud[playerid];
	if (gDevHud[playerid])
	{
		gDevTD[playerid] = CreatePlayerTextDraw(playerid, 5.0, 240.0, "...");
		PlayerTextDrawLetterSize(playerid, gDevTD[playerid], 0.25, 1.0);
		PlayerTextDrawFont(playerid, gDevTD[playerid], TEXT_DRAW_FONT_1);
		PlayerTextDrawAlignment(playerid, gDevTD[playerid], TEXT_DRAW_ALIGN_LEFT);
		PlayerTextDrawColour(playerid, gDevTD[playerid], 0xFFFF66FF);
		PlayerTextDrawSetOutline(playerid, gDevTD[playerid], 1);
		PlayerTextDrawShow(playerid, gDevTD[playerid]);
		SendClientMessage(playerid, COLOR_GREEN, "Координаты на экране включены. Выключить: /dev. Справка по маппингу: /mo");
	}
	else
	{
		if (gDevTD[playerid] != INVALID_PLAYER_TEXT_DRAW) PlayerTextDrawDestroy(playerid, gDevTD[playerid]);
		gDevTD[playerid] = INVALID_PLAYER_TEXT_DRAW;
		SendClientMessage(playerid, COLOR_WHITE, "Координаты на экране выключены.");
	}
	return true;
}

forward Dev_HudTimer();
public Dev_HudTimer()
{
	new Float:x, Float:y, Float:z, Float:a;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!gDevHud[i] || !IsPlayerConnected(i) || gDevTD[i] == INVALID_PLAYER_TEXT_DRAW) continue;
		Dev_GetPos(i, x, y, z, a);
		PlayerTextDrawSetString(i, gDevTD[i], "X %.2f~n~Y %.2f~n~Z %.2f~n~A %.1f~n~Int %d  W %d",
			x, y, z, a, GetPlayerInterior(i), GetPlayerVirtualWorld(i));
	}
	return 1;
}

// ---------------------------------------------------------------------
//  Маппинг
// ---------------------------------------------------------------------
static Map_SlotByObject(objectid)
{
	for (new s = 0; s < gMapCount; s++)
	{
		if (gMapObj[s] == objectid) return s;
	}
	return -1;
}

// Для textures.inc
stock Map_Count() return gMapCount;
stock Map_ObjectOf(s) return gMapObj[s];
stock Map_DbIdOf(s) return gMapDbId[s];

stock Map_ObjectByDbId(dbid)
{
	for (new s = 0; s < gMapCount; s++)
	{
		if (gMapDbId[s] == dbid) return gMapObj[s];
	}
	return INVALID_OBJECT_ID;
}

// Объект, который игрок двигает, или ближайший в 30 м
stock Map_SelectedSlot(playerid)
{
	new pos = 0;
	return Map_PickSlot(playerid, "", pos);
}

// Пересоздать объект на том же месте (перекраску в SA-MP не снять иначе)
stock Map_Rebuild(s)
{
	new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz;
	new model = GetObjectModel(gMapObj[s]);
	GetObjectPos(gMapObj[s], x, y, z);
	GetObjectRot(gMapObj[s], rx, ry, rz);
	new o = CreateObject(model, x, y, z, rx, ry, rz, MAP_DRAW_DISTANCE);
	if (o == INVALID_OBJECT_ID) return 0;
	DestroyObject(gMapObj[s]);
	gMapObj[s] = o;
	Tex_ApplyFor(gMapDbId[s], o);
	return 1;
}

// Номер из параметра (как в /mo near, с 1) или ближайший объект в 30 м
static Map_PickSlot(playerid, const params[], &pos)
{
	new num;
	if (NextIntParam(params, pos, num))
	{
		if (num >= 1 && num <= gMapCount) return num - 1;
		return -1;
	}
	if (gEditSlot[playerid] != -1 && gEditSlot[playerid] < gMapCount) return gEditSlot[playerid];
	new best = -1, Float:bestDist = 30.0, Float:x, Float:y, Float:z;
	for (new s = 0; s < gMapCount; s++)
	{
		GetObjectPos(gMapObj[s], x, y, z);
		new Float:d = GetPlayerDistanceFromPoint(playerid, x, y, z);
		if (d < bestDist)
		{
			bestDist = d;
			best = s;
		}
	}
	return best;
}

static Map_Save(s)
{
	new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz;
	GetObjectPos(gMapObj[s], x, y, z);
	GetObjectRot(gMapObj[s], rx, ry, rz);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE map_objects SET x = %f, y = %f, z = %f, rx = %f, ry = %f, rz = %f WHERE id = %d",
		x, y, z, rx, ry, rz, gMapDbId[s]));
}

static Map_Create(model, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz)
{
	if (gMapCount >= MAX_MAP_OBJECTS) return -1;
	new o = CreateObject(model, x, y, z, rx, ry, rz, MAP_DRAW_DISTANCE);
	if (o == INVALID_OBJECT_ID) return -1;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO map_objects (model, x, y, z, rx, ry, rz) VALUES (%d, %f, %f, %f, %f, %f, %f)",
		model, x, y, z, rx, ry, rz));
	new s = gMapCount++;
	gMapDbId[s] = DB_LastInsertId();
	gMapObj[s] = o;
	return s;
}

static Map_StartEdit(playerid, s)
{
	gEditSlot[playerid] = s;
	BeginObjectEditing(playerid, gMapObj[s]);
	SendClientMessage(playerid, COLOR_WHITE, "Объект №%d (модель %d). Двигай мышью, дискета — сохранить, ESC — отмена. С телефона: /mo move /mo rot",
		s + 1, GetObjectModel(gMapObj[s]));
}

static Map_Help(playerid)
{
	new text[1400];
	strcat(text, "{FFFF00}Координаты\n{FFFFFF}/pos — где я   /pos [название] — сохранить точку (файл scriptfiles/bastion_points.txt)\n");
	strcat(text, "/points — список точек   /delpoint [название]   /dev — координаты на экране\n\n");
	strcat(text, "{FFFF00}Объекты\n{FFFFFF}/mo new [модель] — поставить перед собой   /mo sel — выбрать мышью\n");
	strcat(text, "/mo edit [№] — двигать мышью   /mo move x y z — сдвинуть   /mo rot x y z — повернуть\n");
	strcat(text, "/mo here — к себе под ноги   /mo copy [№] — копия   /mo del [№] — удалить   /mo near — что рядом\n\n");
	strcat(text, "{FFFF00}Здания GTA и выгрузка\n{FFFFFF}/mo remove [радиус] — убрать здания вокруг   /mo unremove — вернуть последние\n");
	strcat(text, "/mo export — выгрузить карту в scriptfiles/map_export.txt\n\n");
	strcat(text, "{FFFF00}Перекраска\n{FFFFFF}/mt — текстуры, цвет и надписи на объекте (справка: /mt help)\n\n");
	strcat(text, "{FFFF00}Маркеры и GPS\n{FFFFFF}/markers (/mk) — у маркера: перенести, вид, значок на карте, раздел GPS; вдали — своё место или список всех\n\n");
	strcat(text, "{AFAFAF}Без номера команда берёт объект, который ты сейчас двигаешь, или ближайший.\nМодели объектов: dev.prineside.com/ru/gtasa_samp_model_id/");
	ShowPlayerDialog(playerid, DIALOG_DEV_HELP, DIALOG_STYLE_MSGBOX, "Разработка в игре", "%s", "Закрыть", "", text);
}

stock bool:Cmd_Mo(playerid, const params[])
{
	if (!Dev_Check(playerid)) return true;
	new pos = 0, sub[16];
	if (!NextParam(params, pos, sub))
	{
		Map_Help(playerid);
		return true;
	}

	if (!strcmp(sub, "new", true))
	{
		new model;
		if (!NextIntParam(params, pos, model) || model < 300 || model > 20000)
			return SendClientMessage(playerid, COLOR_GREY, "Использование: /mo new [модель], например /mo new 1225 (бочка)");
		new Float:x, Float:y, Float:z, Float:a;
		GetPlayerPos(playerid, x, y, z);
		GetPlayerFacingAngle(playerid, a);
		x += 3.0 * -floatsin(a, degrees);
		y += 3.0 * floatcos(a, degrees);
		new s = Map_Create(model, x, y, z, 0.0, 0.0, a);
		if (s == -1) return SendClientMessage(playerid, COLOR_RED, "Не удалось создать объект (неверная модель или лимит %d).", MAX_MAP_OBJECTS);
		Admin_Log(playerid, "mo new", INVALID_PLAYER_ID, "", model);
		Map_StartEdit(playerid, s);
		return true;
	}
	if (!strcmp(sub, "sel", true))
	{
		BeginObjectSelecting(playerid);
		return SendClientMessage(playerid, COLOR_WHITE, "Кликни по объекту мышью. ESC — отмена.");
	}
	if (!strcmp(sub, "near", true))
	{
		new Float:x, Float:y, Float:z, found = 0;
		SendClientMessage(playerid, COLOR_YELLOW, "Объекты в 50 м:");
		for (new s = 0; s < gMapCount && found < 15; s++)
		{
			GetObjectPos(gMapObj[s], x, y, z);
			new Float:d = GetPlayerDistanceFromPoint(playerid, x, y, z);
			if (d > 50.0) continue;
			SendClientMessage(playerid, COLOR_WHITE, " №%d — модель %d, %.0f м", s + 1, GetObjectModel(gMapObj[s]), d);
			found++;
		}
		if (found == 0) SendClientMessage(playerid, COLOR_GREY, " Рядом нет объектов.");
		return true;
	}
	if (!strcmp(sub, "remove", true))
	{
		new radius = 10;
		NextIntParam(params, pos, radius);
		if (radius < 1 || radius > 200) return SendClientMessage(playerid, COLOR_GREY, "Радиус от 1 до 200 метров.");
		if (gMapRemovalCount >= MAX_MAP_REMOVALS) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит удалений (%d).", MAX_MAP_REMOVALS);
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO map_removals (model, x, y, z, radius) VALUES (-1, %f, %f, %f, %f)", x, y, z, float(radius)));
		new m = gMapRemovalCount++;
		gMapRemoval[m][mrDbId] = DB_LastInsertId();
		gMapRemoval[m][mrModel] = -1;
		gMapRemoval[m][mrX] = x;
		gMapRemoval[m][mrY] = y;
		gMapRemoval[m][mrZ] = z;
		gMapRemoval[m][mrR] = float(radius);
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (IsPlayerConnected(i)) RemoveBuildingForPlayer(i, -1, x, y, z, float(radius));
		}
		Admin_Log(playerid, "mo remove", INVALID_PLAYER_ID, "", radius);
		return SendClientMessage(playerid, COLOR_GREEN, "Здания GTA в радиусе %d м убраны для всех. Отменить: /mo unremove", radius);
	}
	if (!strcmp(sub, "unremove", true))
	{
		if (gMapRemovalCount == 0) return SendClientMessage(playerid, COLOR_GREY, "Удалений нет.");
		gMapRemovalCount--;
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM map_removals WHERE id = %d", gMapRemoval[gMapRemovalCount][mrDbId]));
		Admin_Log(playerid, "mo unremove", INVALID_PLAYER_ID, "", 0);
		return SendClientMessage(playerid, COLOR_WHITE, "Последнее удаление зданий отменено. Здания вернутся после перезахода в игру.");
	}
	if (!strcmp(sub, "export", true))
	{
		Map_Export(playerid);
		return true;
	}

	// Дальше — команды для одного объекта
	new s = Map_PickSlot(playerid, params, pos);
	if (s == -1) return SendClientMessage(playerid, COLOR_GREY, "Объект не найден. Номера объектов рядом: /mo near");

	if (!strcmp(sub, "edit", true))
	{
		Map_StartEdit(playerid, s);
		return true;
	}
	if (!strcmp(sub, "move", true) || !strcmp(sub, "rot", true))
	{
		new Float:d[3], tmp[16];
		for (new i = 0; i < 3; i++)
		{
			if (!NextParam(params, pos, tmp)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /mo %s x y z, например /mo %s 0 0 0.5", sub, sub);
			d[i] = floatstr(tmp);
		}
		new Float:x, Float:y, Float:z;
		if (sub[0] == 'm' || sub[0] == 'M')
		{
			GetObjectPos(gMapObj[s], x, y, z);
			SetObjectPos(gMapObj[s], x + d[0], y + d[1], z + d[2]);
		}
		else
		{
			GetObjectRot(gMapObj[s], x, y, z);
			SetObjectRot(gMapObj[s], x + d[0], y + d[1], z + d[2]);
		}
		Map_Save(s);
		gEditSlot[playerid] = s;
		GetObjectPos(gMapObj[s], x, y, z);
		new Float:rx, Float:ry, Float:rz;
		GetObjectRot(gMapObj[s], rx, ry, rz);
		return SendClientMessage(playerid, COLOR_WHITE, "№%d: %.2f, %.2f, %.2f  поворот %.1f, %.1f, %.1f", s + 1, x, y, z, rx, ry, rz);
	}
	if (!strcmp(sub, "here", true))
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		SetObjectPos(gMapObj[s], x, y, z - 1.0);
		Map_Save(s);
		gEditSlot[playerid] = s;
		return SendClientMessage(playerid, COLOR_WHITE, "Объект №%d перенесён к тебе. Отойди, чтобы не застрять.", s + 1);
	}
	if (!strcmp(sub, "copy", true))
	{
		new Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz;
		GetObjectPos(gMapObj[s], x, y, z);
		GetObjectRot(gMapObj[s], rx, ry, rz);
		new c = Map_Create(GetObjectModel(gMapObj[s]), x + 1.0, y, z, rx, ry, rz);
		if (c == -1) return SendClientMessage(playerid, COLOR_RED, "Лимит объектов (%d).", MAX_MAP_OBJECTS);
		Tex_OnMapCopy(gMapDbId[s], gMapDbId[c], gMapObj[c]);
		Map_StartEdit(playerid, c);
		return true;
	}
	if (!strcmp(sub, "del", true))
	{
		new model = GetObjectModel(gMapObj[s]);
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM map_objects WHERE id = %d", gMapDbId[s]));
		Tex_OnMapDelete(gMapDbId[s]);
		DestroyObject(gMapObj[s]);
		gMapCount--;
		// последний объект на место удалённого (его номер меняется)
		if (s != gMapCount)
		{
			gMapObj[s] = gMapObj[gMapCount];
			gMapDbId[s] = gMapDbId[gMapCount];
		}
		gMapObj[gMapCount] = INVALID_OBJECT_ID;
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (gEditSlot[i] == s || gEditSlot[i] == gMapCount) gEditSlot[i] = -1;
		}
		Admin_Log(playerid, "mo del", INVALID_PLAYER_ID, "", model);
		return SendClientMessage(playerid, COLOR_WHITE, "Объект №%d (модель %d) удалён.", s + 1, model);
	}
	Map_Help(playerid);
	return true;
}

static Map_Export(playerid)
{
	new File:f = fopen(MAP_EXPORT_FILE, io_write);
	if (!f)
	{
		SendClientMessage(playerid, COLOR_RED, "Не удалось создать файл scriptfiles/%s.", MAP_EXPORT_FILE);
		return 0;
	}
	new line[160], Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz;
	fwrite(f, "// Bastion map export (/mo export)\r\n// RemoveBuildingForPlayer - in OnPlayerConnect\r\nnew tmpobjid;\r\n");
	for (new m = 0; m < gMapRemovalCount; m++)
	{
		format(line, sizeof line, "RemoveBuildingForPlayer(playerid, %d, %.4f, %.4f, %.4f, %.2f);\r\n",
			gMapRemoval[m][mrModel], gMapRemoval[m][mrX], gMapRemoval[m][mrY], gMapRemoval[m][mrZ], gMapRemoval[m][mrR]);
		fwrite(f, line);
	}
	for (new s = 0; s < gMapCount; s++)
	{
		GetObjectPos(gMapObj[s], x, y, z);
		GetObjectRot(gMapObj[s], rx, ry, rz);
		if (Tex_Export(f, gMapDbId[s], true) > 0)
		{
			format(line, sizeof line, "tmpobjid = CreateObject(%d, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f);\r\n", GetObjectModel(gMapObj[s]), x, y, z, rx, ry, rz);
			fwrite(f, line);
			Tex_Export(f, gMapDbId[s]);
		}
		else
		{
			format(line, sizeof line, "CreateObject(%d, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f);\r\n", GetObjectModel(gMapObj[s]), x, y, z, rx, ry, rz);
			fwrite(f, line);
		}
	}
	fclose(f);
	SendClientMessage(playerid, COLOR_GREEN, "Выгружено: %d объектов и %d удалений зданий в scriptfiles/%s.", gMapCount, gMapRemovalCount, MAP_EXPORT_FILE);
	return 1;
}

// ---------------------------------------------------------------------
//  Колбэки (из rp.pwn)
// ---------------------------------------------------------------------
stock Dev_OnEditObject(playerid, objectid, EDIT_RESPONSE:response, Float:x, Float:y, Float:z, Float:rx, Float:ry, Float:rz)
{
	new s = Map_SlotByObject(objectid);
	if (s == -1) return 0;
	if (response == EDIT_RESPONSE_FINAL)
	{
		SetObjectPos(objectid, x, y, z);
		SetObjectRot(objectid, rx, ry, rz);
		Map_Save(s);
		SendClientMessage(playerid, COLOR_GREEN, "Объект №%d сохранён: %.2f, %.2f, %.2f  поворот %.1f, %.1f, %.1f", s + 1, x, y, z, rx, ry, rz);
	}
	else if (response == EDIT_RESPONSE_CANCEL)
	{
		// вернуть как было у всех (клиент мог сдвинуть объект у себя)
		new Float:ox, Float:oy, Float:oz, Float:orx, Float:ory, Float:orz;
		GetObjectPos(objectid, ox, oy, oz);
		GetObjectRot(objectid, orx, ory, orz);
		SetObjectPos(objectid, ox, oy, oz);
		SetObjectRot(objectid, orx, ory, orz);
	}
	return 1;
}

stock Dev_OnSelectObject(playerid, SELECT_OBJECT:type, objectid)
{
	if (type != SELECT_OBJECT_GLOBAL_OBJECT) return 0;
	new s = Map_SlotByObject(objectid);
	if (s == -1)
	{
		EndObjectEditing(playerid);
		SendClientMessage(playerid, COLOR_GREY, "Это не объект маппинга (стандартные здания убираются через /mo remove).");
		return 1;
	}
	Map_StartEdit(playerid, s);
	return 1;
}

stock Dev_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid == DIALOG_DEV_HELP) return 1;
	if (dialogid != DIALOG_DEV_POINTS) return 0;
	if (!response || listitem < 0 || listitem >= 40) return 1;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT x, y, z, a, interior, world FROM dev_points WHERE id = %d", gPointList[playerid][listitem]);
	if (r == DBResult:0) return 1;
	if (DB_GetRowCount(r) > 0)
	{
		Teleport_SaveBack(playerid);
		Teleport_Player(playerid, DB_GetFieldFloat(r, 0), DB_GetFieldFloat(r, 1), DB_GetFieldFloat(r, 2), DB_GetFieldFloat(r, 3),
			DB_GetFieldInt(r, 4), DB_GetFieldInt(r, 5));
	}
	DB_FreeResultSet(r);
	return 1;
}
