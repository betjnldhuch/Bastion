// =====================================================================
//  markers.inc — все маркеры сервера (значок + надпись) в одном месте.
//
//  Модули ставят свои точки через Marker_Create: дома, бизнесы, банкоматы,
//  АЗС, СТО, раздевалки фракций, входы в здания, работы, автосалон,
//  автошкола. Админ (4+) правит любой маркер прямо в игре: /markers (gps.inc).
//  Правки (место, вид, высота, значок на карте, раздел и название в GPS)
//  хранятся в таблице markers и при запуске ложатся поверх координат из кода.
//
//  Для модуля:
//    m = Marker_Create(MK_..., ref, "Название", модель, x, y, z [, мир, интерьер, раздел GPS, угол]);
//        x, y, z передаются по ссылке: если админ двигал маркер — туда запишется новое место.
//    Marker_SetLabel(m, "текст", цвет, дальность);   Marker_InRange(playerid, m, радиус)
//    Marker_SetDefaultModel(m, модель);   Marker_Destroy(m, true) — удалить вместе с правками.
//  Когда админ двигает маркер, вызывается <Модуль>_OnMarkerMoved (см. Marker_OnMoved).
// =====================================================================

#define MAX_MARKERS         800

// Чей маркер (ref — номер точки внутри модуля)
enum
{
	MK_CUSTOM,      // свои точки админа; ref — порядковый номер
	MK_DOOR,        // вход в здание: ref = номер двери * 2, внутри — номер * 2 + 1
	MK_HOUSE,       // ref = id дома в базе
	MK_BIZ,         // ref = id бизнеса в базе
	MK_ATM,         // ref = id банкомата в базе
	MK_FUEL,        // ref = id АЗС в базе
	MK_GARAGE,      // ref = id СТО / штрафстоянки в базе
	MK_FACTION,     // ref = номер фракции
	MK_JOB,         // ref = номер работы (JOB_*)
	MK_DEALER,
	MK_SCHOOL,
	MK_LIFE,        // точки новых систем (places.inc): ref = PLACE_REF + номер, базы новых работ
	MK_RENTAL,      // прокат мопедов: ref = id точки в базе (rental.inc)
	MK_TYPES
}

// Разделы GPS
#define GPS_NONE        0
#define GPS_GOV         1
#define GPS_JOBS        2
#define GPS_SHOPS       3
#define GPS_TRANSPORT   4
#define GPS_FUEL        5
#define GPS_ATM         6
#define GPS_FACTIONS    7
#define GPS_FUN         8
#define GPS_OTHER       9
#define GPS_CATS        10

#define MK_TEXT_LEN     160

enum E_MARKER
{
	bool:mkUsed,
	mkType,
	mkRef,
	mkName[32],             // название для GPS и списков
	mkDefName[32],
	bool:mkNameSet,         // название задал админ
	mkDefModel,             // модель значка от модуля (0 — без значка)
	mkModel,                // модель от админа (-1 — как у модуля)
	Float:mkX, Float:mkY, Float:mkZ, Float:mkA,
	mkWorld, mkInterior,
	Float:mkDefX, Float:mkDefY, Float:mkDefZ, Float:mkDefA,
	mkDefWorld, mkDefInterior,
	bool:mkMoved,           // место отличается от кода
	Float:mkZOff,           // поднять / опустить значок
	mkIcon,                 // значок на карте (-1 — нет)
	mkGps, mkDefGps,        // раздел GPS (0 — не показывать)
	mkPickup,
	Text3D:mkLabel,
	mkText[MK_TEXT_LEN],
	mkTextColor,
	Float:mkTextDist,
	Float:mkTextZ,
	mkFaction,              // 0 — видят все; иначе только эта фракция (и админы): без значка, надпись у каждого своя
	mkPriv                  // слот своих надписей (-1 — нет)
}
new gMarker[MAX_MARKERS][E_MARKER];

// Маркеры «только для своих»: значок (пикап) виден всем, поэтому его нет;
// надпись создаётся отдельно каждому участнику фракции и главному админу.
#define MK_PRIV_MAX     40
#define MARKER_SEE_ALL  7       // с какого уровня админ видит чужие фракционные точки (главный админ)
static gMkPrivM[MK_PRIV_MAX] = { -1, ... };                 // какой маркер в слоте
static PlayerText3D:gMkPLabel[MAX_PLAYERS][MK_PRIV_MAX];
static bool:gMkPLabelOn[MAX_PLAYERS][MK_PRIV_MAX];

// Виды маркеров для меню /markers. msZ — сдвиг по высоте (цилиндры стоят на земле).
enum E_MK_STYLE
{
	msModel,
	msName[32],
	Float:msZ
}
stock const gMkStyles[][E_MK_STYLE] =
{
	{ 19197, "Жёлтый цилиндр",       -1.0 },
	{ 19198, "Жёлтый цилиндр, узкий", -1.0 },
	{ 19605, "Красный цилиндр",      -1.0 },
	{ 19606, "Зелёный цилиндр",      -1.0 },
	{ 19607, "Синий цилиндр",        -1.0 },
	{ 1318,  "Белая стрелка",         0.0 },
	{ 1559,  "Жёлтая стрелка",        0.0 },
	{ 1239,  "Значок «i»",            0.0 },
	{ 1274,  "Значок «$»",            0.0 },
	{ 1212,  "Пачка денег",           0.0 },
	{ 1272,  "Синий домик",           0.0 },
	{ 1273,  "Зелёный домик",         0.0 },
	{ 1275,  "Футболка",              0.0 },
	{ 1240,  "Сердце",                0.0 },
	{ 1242,  "Бронежилет",            0.0 },
	{ 1247,  "Звезда",                0.0 },
	{ 1254,  "Череп",                 0.0 },
	{ 1210,  "Портфель",              0.0 },
	{ 1650,  "Канистра",              0.0 }
};

// ---------------------------------------------------------------------
//  Запуск: таблица и свои точки админа
// ---------------------------------------------------------------------
stock Markers_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS markers (type INTEGER NOT NULL, ref INTEGER NOT NULL, x REAL, y REAL, z REAL, a REAL, world INTEGER DEFAULT 0, interior INTEGER DEFAULT 0, moved INTEGER DEFAULT 0, model INTEGER DEFAULT -1, zoff REAL DEFAULT 0, icon INTEGER DEFAULT -1, gps INTEGER DEFAULT -1, name TEXT DEFAULT '', label TEXT DEFAULT '', PRIMARY KEY (type, ref))"));
	DB_AddColumn("markers", "faction", "INTEGER DEFAULT 0");

	for (new m = 0; m < MAX_MARKERS; m++)
	{
		gMarker[m][mkUsed] = false;
		gMarker[m][mkPickup] = -1;
		gMarker[m][mkLabel] = INVALID_3DTEXT_ID;
	}

	new count = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT ref, x, y, z, a, world, interior FROM markers WHERE type = %d", MK_CUSTOM);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new Float:x = DB_GetFieldFloat(r, 1), Float:y = DB_GetFieldFloat(r, 2), Float:z = DB_GetFieldFloat(r, 3);
			if (Marker_Create(MK_CUSTOM, DB_GetFieldInt(r, 0), "Место", 0, x, y, z,
				DB_GetFieldInt(r, 5), DB_GetFieldInt(r, 6), GPS_OTHER, DB_GetFieldFloat(r, 4)) != -1) count++;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Markers] %d custom markers.", count);
}

// ---------------------------------------------------------------------
//  Создание и правка маркера
// ---------------------------------------------------------------------
stock Marker_Create(type, ref, const name[], model, &Float:x, &Float:y, &Float:z, world = 0, interior = 0, gps = GPS_NONE, Float:a = 0.0)
{
	new m = -1;
	for (new i = 0; i < MAX_MARKERS; i++)
	{
		if (!gMarker[i][mkUsed]) { m = i; break; }
	}
	if (m == -1)
	{
		printf("[Markers] MAX_MARKERS (%d) reached, marker %d:%d not created.", MAX_MARKERS, type, ref);
		return -1;
	}
	gMarker[m][mkUsed] = true;
	gMarker[m][mkType] = type;
	gMarker[m][mkRef] = ref;
	gMarker[m][mkDefName][0] = '\0';
	strcat(gMarker[m][mkDefName], name, 32);
	gMarker[m][mkName][0] = '\0';
	strcat(gMarker[m][mkName], name, 32);
	gMarker[m][mkNameSet] = false;
	gMarker[m][mkDefModel] = model;
	gMarker[m][mkModel] = -1;
	gMarker[m][mkX] = gMarker[m][mkDefX] = x;
	gMarker[m][mkY] = gMarker[m][mkDefY] = y;
	gMarker[m][mkZ] = gMarker[m][mkDefZ] = z;
	gMarker[m][mkA] = gMarker[m][mkDefA] = a;
	gMarker[m][mkWorld] = gMarker[m][mkDefWorld] = world;
	gMarker[m][mkInterior] = gMarker[m][mkDefInterior] = interior;
	gMarker[m][mkMoved] = false;
	gMarker[m][mkZOff] = 0.0;
	gMarker[m][mkIcon] = -1;
	gMarker[m][mkGps] = gMarker[m][mkDefGps] = gps;
	gMarker[m][mkPickup] = -1;
	gMarker[m][mkLabel] = INVALID_3DTEXT_ID;
	gMarker[m][mkText][0] = '\0';
	gMarker[m][mkTextColor] = COLOR_WHITE;
	gMarker[m][mkTextDist] = 15.0;
	gMarker[m][mkTextZ] = 0.6;
	gMarker[m][mkFaction] = 0;
	gMarker[m][mkPriv] = -1;

	Marker_LoadSaved(m);
	x = gMarker[m][mkX];
	y = gMarker[m][mkY];
	z = gMarker[m][mkZ];
	Marker_Build(m);
	return m;
}

static Marker_LoadSaved(m)
{
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT x, y, z, a, world, interior, moved, model, zoff, icon, gps, name, label, faction FROM markers WHERE type = %d AND ref = %d",
		gMarker[m][mkType], gMarker[m][mkRef]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		if (DB_GetFieldInt(r, 6) != 0)
		{
			gMarker[m][mkX] = DB_GetFieldFloat(r, 0);
			gMarker[m][mkY] = DB_GetFieldFloat(r, 1);
			gMarker[m][mkZ] = DB_GetFieldFloat(r, 2);
			gMarker[m][mkA] = DB_GetFieldFloat(r, 3);
			gMarker[m][mkWorld] = DB_GetFieldInt(r, 4);
			gMarker[m][mkInterior] = DB_GetFieldInt(r, 5);
			gMarker[m][mkMoved] = true;
		}
		gMarker[m][mkModel] = DB_GetFieldInt(r, 7);
		gMarker[m][mkZOff] = DB_GetFieldFloat(r, 8);
		gMarker[m][mkIcon] = DB_GetFieldInt(r, 9);
		new gps = DB_GetFieldInt(r, 10);
		if (gps >= 0 && gps < GPS_CATS) gMarker[m][mkGps] = gps;
		new name[32];
		DB_GetFieldString(r, 11, name, sizeof name);
		if (name[0] != '\0')
		{
			gMarker[m][mkName][0] = '\0';
			strcat(gMarker[m][mkName], name, 32);
			gMarker[m][mkNameSet] = true;
		}
		if (gMarker[m][mkType] == MK_CUSTOM)
			DB_GetFieldString(r, 12, gMarker[m][mkText], MK_TEXT_LEN);
		gMarker[m][mkFaction] = DB_GetFieldInt(r, 13);
		if (gMarker[m][mkFaction] < 0 || gMarker[m][mkFaction] >= MAX_FACTIONS) gMarker[m][mkFaction] = 0;
	}
	DB_FreeResultSet(r);
}

// Записать правки маркера в базу
stock Marker_Save(m)
{
	if (!Marker_IsValid(m)) return;
	new name[32], label[MK_TEXT_LEN];
	if (gMarker[m][mkNameSet] || gMarker[m][mkType] == MK_CUSTOM) strcat(name, gMarker[m][mkName]);
	if (gMarker[m][mkType] == MK_CUSTOM) strcat(label, gMarker[m][mkText]);
	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"INSERT OR REPLACE INTO markers (type, ref, x, y, z, a, world, interior, moved, model, zoff, icon, gps, name, label, faction) VALUES (%d, %d, %f, %f, %f, %f, %d, %d, %d, %d, %f, %d, %d, '%q', '%q', %d)",
		gMarker[m][mkType], gMarker[m][mkRef], gMarker[m][mkX], gMarker[m][mkY], gMarker[m][mkZ], gMarker[m][mkA],
		gMarker[m][mkWorld], gMarker[m][mkInterior], gMarker[m][mkMoved] ? 1 : 0, gMarker[m][mkModel], gMarker[m][mkZOff],
		gMarker[m][mkIcon], gMarker[m][mkGps], name, label, gMarker[m][mkFaction]));
}

// Забыть правки маркера (строка в базе)
stock Marker_Forget(m)
{
	if (!Marker_IsValid(m)) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM markers WHERE type = %d AND ref = %d", gMarker[m][mkType], gMarker[m][mkRef]));
}

stock Marker_IsValid(m)
{
	return (m >= 0 && m < MAX_MARKERS && gMarker[m][mkUsed]) ? 1 : 0;
}

// Модель значка с учётом правки админа (0 — без значка)
stock Marker_Model(m)
{
	if (gMarker[m][mkModel] >= 0) return gMarker[m][mkModel];
	return gMarker[m][mkDefModel];
}

stock Float:Marker_StyleZ(model)
{
	for (new i = 0; i < sizeof gMkStyles; i++)
	{
		if (gMkStyles[i][msModel] == model) return gMkStyles[i][msZ];
	}
	return 0.0;
}

// Пересоздать значок и надпись
stock Marker_Build(m)
{
	if (!Marker_IsValid(m)) return;
	if (gMarker[m][mkPickup] != -1)
	{
		DestroyPickup(gMarker[m][mkPickup]);
		gMarker[m][mkPickup] = -1;
	}
	new model = Marker_Model(m);
	if (model > 0 && gMarker[m][mkFaction] == 0)
	{
		gMarker[m][mkPickup] = CreatePickup(model, 1, gMarker[m][mkX], gMarker[m][mkY],
			gMarker[m][mkZ] + Marker_StyleZ(model) + gMarker[m][mkZOff], gMarker[m][mkWorld]);
	}
	Marker_BuildLabel(m);
}

stock Marker_BuildLabel(m)
{
	if (!Marker_IsValid(m)) return;
	if (gMarker[m][mkLabel] != INVALID_3DTEXT_ID)
	{
		Delete3DTextLabel(gMarker[m][mkLabel]);
		gMarker[m][mkLabel] = INVALID_3DTEXT_ID;
	}
	Marker_PrivSync(m);
	if (gMarker[m][mkFaction] != 0) return;     // только для своих: надписи у каждого свои
	if (gMarker[m][mkText][0] == '\0') return;
	gMarker[m][mkLabel] = Create3DTextLabel(gMarker[m][mkText], gMarker[m][mkTextColor],
		gMarker[m][mkX], gMarker[m][mkY], gMarker[m][mkZ] + gMarker[m][mkTextZ], gMarker[m][mkTextDist], gMarker[m][mkWorld], true);
}

// Надпись над маркером (уже готовый текст, без %)
stock Marker_SetLabel(m, const text[], color = COLOR_WHITE, Float:dist = 15.0, Float:height = 0.6)
{
	if (!Marker_IsValid(m)) return;
	gMarker[m][mkText][0] = '\0';
	strcat(gMarker[m][mkText], text, MK_TEXT_LEN);
	for (new i = 0; gMarker[m][mkText][i] != '\0'; i++)
	{
		if (gMarker[m][mkText][i] == '%') gMarker[m][mkText][i] = '#';
	}
	gMarker[m][mkTextColor] = color;
	gMarker[m][mkTextDist] = dist;
	gMarker[m][mkTextZ] = height;
	Marker_BuildLabel(m);
}

// Модуль сменил свой значок (например, дом продан). Если админ выбрал свой вид — он остаётся.
stock Marker_SetDefaultModel(m, model)
{
	if (!Marker_IsValid(m) || gMarker[m][mkDefModel] == model) return;
	gMarker[m][mkDefModel] = model;
	if (gMarker[m][mkModel] < 0) Marker_Build(m);
}

// Модуль сам перенёс точку (например, /setfduty)
stock Marker_SetPos(m, Float:x, Float:y, Float:z)
{
	if (!Marker_IsValid(m)) return;
	if (gMarker[m][mkX] == x && gMarker[m][mkY] == y && gMarker[m][mkZ] == z) return;
	gMarker[m][mkX] = x;
	gMarker[m][mkY] = y;
	gMarker[m][mkZ] = z;
	gMarker[m][mkMoved] = true;
	Marker_Build(m);
	Marker_Save(m);
	if (gMarker[m][mkIcon] >= 0) Markers_RefreshIcons();
}

stock Marker_Destroy(m, bool:forget = false)
{
	if (!Marker_IsValid(m)) return;
	if (forget) Marker_Forget(m);
	if (gMarker[m][mkPickup] != -1) DestroyPickup(gMarker[m][mkPickup]);
	if (gMarker[m][mkLabel] != INVALID_3DTEXT_ID) Delete3DTextLabel(gMarker[m][mkLabel]);
	gMarker[m][mkPickup] = -1;
	gMarker[m][mkLabel] = INVALID_3DTEXT_ID;
	gMarker[m][mkFaction] = 0;
	Marker_PrivSync(m);         // убрать свои надписи и освободить слот
	gMarker[m][mkUsed] = false;
	if (gMarker[m][mkIcon] >= 0) Markers_RefreshIcons();
}

// ---------------------------------------------------------------------
//  Маркеры «только для своих» (фракция)
// ---------------------------------------------------------------------
// Видит ли игрок маркер: обычный — все, фракционный — участники и админы 4+
stock bool:Marker_VisibleTo(playerid, m)
{
	if (!Marker_IsValid(m)) return false;
	new f = gMarker[m][mkFaction];
	return (f == 0 || Player[playerid][pFaction] == f || Player[playerid][pAdmin] >= MARKER_SEE_ALL);
}

stock Marker_GetFaction(m) { return Marker_IsValid(m) ? gMarker[m][mkFaction] : 0; }

// Модуль задаёт, чья точка (0 — общая). Не сохраняется: модуль ставит при каждой постройке.
stock Marker_SetFaction(m, f)
{
	if (!Marker_IsValid(m) || gMarker[m][mkFaction] == f) return;
	gMarker[m][mkFaction] = f;
	Marker_Build(m);
	if (gMarker[m][mkIcon] >= 0) Markers_RefreshIcons();
}

static Marker_PrivFor(playerid, s)
{
	if (gMkPLabelOn[playerid][s])
	{
		DeletePlayer3DTextLabel(playerid, gMkPLabel[playerid][s]);
		gMkPLabelOn[playerid][s] = false;
	}
	new m = gMkPrivM[s];
	if (m == -1 || !Marker_IsValid(m) || gMarker[m][mkFaction] == 0) return;
	if (!Player[playerid][pLogged] || !Marker_VisibleTo(playerid, m)) return;
	new text[MK_TEXT_LEN + 24];
	if (gMarker[m][mkText][0] != '\0') strcat(text, gMarker[m][mkText]);
	else if (Player[playerid][pAdmin] >= MARKER_SEE_ALL && Player[playerid][pFaction] != gMarker[m][mkFaction]) strcat(text, "{AAAAAA}(скрытая точка)");
	else return;
	gMkPLabel[playerid][s] = CreatePlayer3DTextLabel(playerid, text, gMarker[m][mkTextColor],
		gMarker[m][mkX], gMarker[m][mkY], gMarker[m][mkZ] + gMarker[m][mkTextZ], gMarker[m][mkTextDist]);
	gMkPLabelOn[playerid][s] = true;
}

// Пересоздать свои надписи маркера у всех (вызывается при постройке маркера)
static Marker_PrivSync(m)
{
	new s = gMarker[m][mkPriv];
	if (gMarker[m][mkFaction] != 0 && gMarker[m][mkUsed] && s == -1)
	{
		for (new i = 0; i < MK_PRIV_MAX; i++)
		{
			if (gMkPrivM[i] == -1) { s = i; break; }
		}
		if (s == -1) { printf("[Markers] MK_PRIV_MAX (%d) reached.", MK_PRIV_MAX); return; }
		gMkPrivM[s] = m;
		gMarker[m][mkPriv] = s;
	}
	if (s == -1) return;
	if (gMarker[m][mkFaction] == 0) gMkPrivM[s] = -1;   // снова общий: слот свободен (надписи уберёт цикл ниже)
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i)) Marker_PrivFor(i, s);
	}
	if (gMarker[m][mkFaction] == 0) gMarker[m][mkPriv] = -1;
}

// Вход, смена фракции или админки: показать игроку его фракционные надписи
stock Markers_SyncPlayer(playerid)
{
	for (new s = 0; s < MK_PRIV_MAX; s++) Marker_PrivFor(playerid, s);
	Markers_ShowIcons(playerid);
}

stock Markers_OnConnect(playerid)
{
	for (new s = 0; s < MK_PRIV_MAX; s++) gMkPLabelOn[playerid][s] = false;
}

// ---------------------------------------------------------------------
//  Для модулей: где маркер и стоит ли игрок рядом
// ---------------------------------------------------------------------
stock Marker_InRange(playerid, m, Float:range)
{
	if (!Marker_IsValid(m)) return 0;
	if (GetPlayerVirtualWorld(playerid) != gMarker[m][mkWorld] || GetPlayerInterior(playerid) != gMarker[m][mkInterior]) return 0;
	return IsPlayerInRangeOfPoint(playerid, range, gMarker[m][mkX], gMarker[m][mkY], gMarker[m][mkZ]) ? 1 : 0;
}

stock Marker_GetPos(m, &Float:x, &Float:y, &Float:z)
{
	if (!Marker_IsValid(m)) return 0;
	x = gMarker[m][mkX];
	y = gMarker[m][mkY];
	z = gMarker[m][mkZ];
	return 1;
}

stock Float:Marker_GetAngle(m)
{
	if (!Marker_IsValid(m)) return 0.0;
	return gMarker[m][mkA];
}

stock Marker_GetWorld(m)    { return Marker_IsValid(m) ? gMarker[m][mkWorld] : 0; }
stock Marker_GetInterior(m) { return Marker_IsValid(m) ? gMarker[m][mkInterior] : 0; }

// Админ перенёс маркер: сообщить модулю, чтобы он обновил свои координаты
stock Marker_OnMoved(m)
{
	if (!Marker_IsValid(m)) return;
	new ref = gMarker[m][mkRef];
	new Float:x = gMarker[m][mkX], Float:y = gMarker[m][mkY], Float:z = gMarker[m][mkZ];
	switch (gMarker[m][mkType])
	{
		case MK_HOUSE:   House_OnMarkerMoved(ref, x, y, z);
		case MK_BIZ:     Biz_OnMarkerMoved(ref, x, y, z);
		case MK_ATM:     Atm_OnMarkerMoved(ref, x, y, z, gMarker[m][mkA]);
		case MK_FUEL:    Fuel_OnMarkerMoved(ref, x, y, z);
		case MK_GARAGE:  Garage_OnMarkerMoved(ref, x, y, z);
		case MK_FACTION: Faction_OnMarkerMoved(ref, x, y, z);
		case MK_LIFE:    Place_OnMarkerMoved(ref, x, y, z);
		case MK_RENTAL:  Rental_OnMarkerMoved(ref, x, y, z);
		// Двери, работы, автосалон, автошкола и свои точки берут место прямо из маркера
	}
}
