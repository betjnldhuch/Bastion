// =====================================================================
//  places.inc — точки новых систем, которые админ расставляет сам.
//
//  Одна таблица places (вид, место) для всего: торговцы наркотиками, места
//  рыбалки, грядки и амбар фермы, мусорные баки и свалка, камеры и работы
//  тюрьмы, стойка больницы, церковь, ларёк лотереи, рабочие машины.
//
//  Админ 4+:
//    /place [вид]      — поставить точку там, где стоишь (в машине — точку машины)
//    /places [вид]     — список точек этого вида (без вида — сколько каких)
//    /unplace          — удалить ближайшую точку (до 5 м)
//    /gotoplace [№]    — телепорт к точке
//  Точки с маркером (торговец, больница, церковь, лотерея, база работы) двигаются и в /markers.
// =====================================================================

#define MAX_PLACES          400
#define PLACE_REF           3000000     // ref маркера точки: PLACE_REF + номер в базе
#define DRUG_DEALER_SKIN    29          // скин торговца наркотиками

enum
{
	PL_DEALER,      // торговец наркотиками (drugs.inc)
	PL_FISH,        // место рыбалки
	PL_FIELD,       // грядка фермы
	PL_BARN,        // амбар фермы (сдать урожай)
	PL_BIN,         // мусорный бак
	PL_DUMP,        // свалка (разгрузить мусоровоз)
	PL_VAULT,       // хранилище банка для инкассатора (сдать деньги)
	PL_CAR,         // рабочая машина: в /place car [работа] (fisher/farmer/collector/garbage)
	PL_CELL,        // камера тюрьмы
	PL_PWORK,       // работа заключённых
	PL_ESCAPE,      // лаз для побега
	PL_RELEASE,     // выход из тюрьмы на свободу
	PL_PGUARD,      // раздевалка/пост охраны (вход для охраны)
	PL_CLINIC,      // стойка больницы: лечение, страховка, медкарта
	PL_CHURCH,      // церковь: свадьба, развод
	PL_LOTTERY,     // ларёк лотереи
	PL_KPZ,         // камера КПЗ в LSPD (короткие сроки); нет ни одной — старые камеры из кода
	PL_KINDS
}
static const gPlaceKey[PL_KINDS][] =
{
	"dealer", "fish", "field", "barn", "bin", "dump", "vault", "car",
	"cell", "pwork", "escape", "release", "pguard", "clinic", "church", "lottery", "kpz"
};
static const gPlaceName[PL_KINDS][] =
{
	"Торговец наркотиками", "Место рыбалки", "Грядка фермы", "Амбар фермы", "Мусорный бак", "Свалка",
	"Хранилище банка", "Рабочая машина", "Камера тюрьмы", "Работа в тюрьме", "Лаз для побега",
	"Выход из тюрьмы", "Пост охраны тюрьмы", "Стойка больницы", "Церковь", "Лотерея", "Камера КПЗ"
};
// Нужен ли значок с надписью (остальные точки видны только как метка задания)
static const bool:gPlaceMarker[PL_KINDS] =
{
	true, false, false, true, false, true, true, false,
	false, false, false, false, true, true, true, true, false
};
static const gPlaceModel[PL_KINDS] = { 0, 0, 0, 1318, 0, 1318, 1274, 0, 0, 0, 0, 0, 1247, 1240, 1239, 1274, 0 };

enum E_PLACE
{
	bool:plUsed,
	plDbId,
	plKind,
	plArg,                  // для PL_CAR — работа (JOB_*)
	Float:plX, Float:plY, Float:plZ, Float:plA,
	plWorld, plInt,
	plMk,
	plActor
}
new gPlace[MAX_PLACES][E_PLACE];

// ---------------------------------------------------------------------
//  Места по умолчанию (ставятся один раз, при первом запуске; дальше — /place и /markers)
// ---------------------------------------------------------------------
enum E_PLACE_DEF { pdKind, pdArg, Float:pdX, Float:pdY, Float:pdZ, Float:pdA }
static const gPlaceDefs[][E_PLACE_DEF] =
{
	{ PL_DEALER, 0, 2167.5, -1671.9, 15.08, 225.0 },     // Ganton, у переулка
	{ PL_FISH, 0, 836.4, -2063.2, 12.87, 180.0 },        // пирс Санта-Марии
	{ PL_FISH, 0, 843.0, -2063.2, 12.87, 180.0 },
	{ PL_FISH, 0, 830.0, -2063.2, 12.87, 180.0 },
	{ PL_FISH, 0, 836.4, -2085.0, 12.87, 180.0 },
	{ PL_FIELD, 0, -1095.0, -1215.0, 129.2, 0.0 },       // ферма EasterBoard (Flint County)
	{ PL_FIELD, 0, -1080.0, -1235.0, 129.2, 0.0 },
	{ PL_FIELD, 0, -1060.0, -1250.0, 129.2, 0.0 },
	{ PL_FIELD, 0, -1040.0, -1230.0, 129.2, 0.0 },
	{ PL_FIELD, 0, -1020.0, -1215.0, 129.2, 0.0 },
	{ PL_FIELD, 0, -1005.0, -1240.0, 129.2, 0.0 },
	{ PL_BARN, 0, -1061.0, -1195.0, 129.22, 0.0 },
	{ PL_BIN, 0, 2491.0, -1680.0, 13.34, 0.0 },          // баки по Лос-Сантосу
	{ PL_BIN, 0, 2230.0, -1715.0, 13.5, 0.0 },
	{ PL_BIN, 0, 1944.0, -1765.0, 13.38, 0.0 },
	{ PL_BIN, 0, 1830.0, -1835.0, 13.5, 0.0 },
	{ PL_BIN, 0, 1975.0, -2035.0, 13.5, 0.0 },
	{ PL_BIN, 0, 2400.0, -1975.0, 13.5, 0.0 },
	{ PL_BIN, 0, 1540.0, -1670.0, 13.5, 0.0 },
	{ PL_BIN, 0, 1185.0, -1320.0, 13.5, 0.0 },
	{ PL_BIN, 0, 2215.0, -1155.0, 25.7, 0.0 },
	{ PL_BIN, 0, 1005.0, -930.0, 42.1, 0.0 },
	{ PL_BIN, 0, 2120.0, -1150.0, 24.1, 0.0 },
	{ PL_BIN, 0, 2100.0, -1810.0, 13.5, 0.0 },
	{ PL_DUMP, 0, 2186.0, -1990.0, 13.55, 0.0 },         // Willowfield, у свалки машин
	{ PL_VAULT, 0, 1461.5, -1021.0, 23.83, 0.0 },        // банк в Market
	{ PL_CLINIC, 0, 1172.6, -1323.4, 15.4, 270.0 },      // у входа в All Saints
	{ PL_CHURCH, 0, 2232.4, -1333.0, 23.98, 90.0 },      // церковь Jefferson
	{ PL_LOTTERY, 0, 1484.0, -1740.0, 13.55, 180.0 },    // у мэрии
	{ PL_CAR, JOB_COLLECTOR * 1000 + 428, 1476.0, -1030.0, 23.8, 90.0 },     // Securicar у банка
	{ PL_CAR, JOB_COLLECTOR * 1000 + 428, 1476.0, -1036.0, 23.8, 90.0 },
	{ PL_CAR, JOB_GARBAGE * 1000 + 408, 2200.0, -1985.0, 13.6, 0.0 },        // мусоровозы
	{ PL_CAR, JOB_GARBAGE * 1000 + 408, 2207.0, -1985.0, 13.6, 0.0 }
};

// ---------------------------------------------------------------------
stock Place_Kind(const key[])
{
	for (new k = 0; k < PL_KINDS; k++) if (!strcmp(key, gPlaceKey[k], true)) return k;
	return -1;
}

stock Place_KindName(kind)
{
	new s[40];
	if (kind >= 0 && kind < PL_KINDS) strcat(s, gPlaceName[kind]);
	return s;
}

static Place_Label(i)
{
	new text[MK_TEXT_LEN];
	switch (gPlace[i][plKind])
	{
		case PL_DEALER:  strcat(text, "Мутный тип\n{FFFFFF}ALT — поговорить");
		case PL_BARN:    strcat(text, "Амбар фермы\n{FFFFFF}сюда несут урожай");
		case PL_DUMP:    strcat(text, "Свалка\n{FFFFFF}разгрузка мусоровоза");
		case PL_VAULT:   strcat(text, "Хранилище банка\n{FFFFFF}инкассаторы сдают деньги");
		case PL_PGUARD:  strcat(text, "Пост охраны тюрьмы\n{FFFFFF}ALT — список заключённых");
		case PL_CLINIC:  strcat(text, "Регистратура больницы\n{FFFFFF}ALT — лечение, страховка, медкарта");
		case PL_CHURCH:  strcat(text, "Церковь\n{FFFFFF}ALT — свадьба и развод");
		case PL_LOTTERY: strcat(text, "Лотерея штата\n{FFFFFF}ALT — купить билет");
		default:         strcat(text, gPlaceName[gPlace[i][plKind]]);
	}
	return text;
}

static Place_Build(i)
{
	if (gPlace[i][plMk] != -1) { Marker_Destroy(gPlace[i][plMk]); gPlace[i][plMk] = -1; }
	if (gPlace[i][plActor] != INVALID_ACTOR_ID) { DestroyActor(gPlace[i][plActor]); gPlace[i][plActor] = INVALID_ACTOR_ID; }
	if (!gPlace[i][plUsed]) return;
	new k = gPlace[i][plKind];
	if (gPlaceMarker[k])
	{
		new gps = GPS_OTHER;
		if (k == PL_DEALER) gps = GPS_NONE;
		else if (k == PL_CLINIC || k == PL_CHURCH) gps = GPS_GOV;
		else if (k == PL_LOTTERY) gps = GPS_FUN;
		else if (k == PL_PGUARD) gps = GPS_FACTIONS;
		gPlace[i][plMk] = Marker_Create(MK_LIFE, PLACE_REF + gPlace[i][plDbId], gPlaceName[k], gPlaceModel[k],
			gPlace[i][plX], gPlace[i][plY], gPlace[i][plZ], gPlace[i][plWorld], gPlace[i][plInt], gps, gPlace[i][plA]);
		Marker_SetLabel(gPlace[i][plMk], Place_Label(i), (k == PL_DEALER) ? 0x9ACD32FF : COLOR_ORANGE, 12.0, (k == PL_DEALER) ? 1.2 : 0.6);
		gPlace[i][plA] = Marker_GetAngle(gPlace[i][plMk]);
	}
	if (k == PL_DEALER)
	{
		gPlace[i][plActor] = CreateActor(DRUG_DEALER_SKIN, gPlace[i][plX], gPlace[i][plY], gPlace[i][plZ], gPlace[i][plA]);
		if (gPlace[i][plActor] != INVALID_ACTOR_ID)
		{
			SetActorVirtualWorld(gPlace[i][plActor], gPlace[i][plWorld]);
			SetActorInvulnerable(gPlace[i][plActor], true);
		}
	}
}

static Place_AddMem(dbid, kind, arg, Float:x, Float:y, Float:z, Float:a, world, interior)
{
	for (new i = 0; i < MAX_PLACES; i++)
	{
		if (gPlace[i][plUsed]) continue;
		gPlace[i][plUsed] = true;
		gPlace[i][plDbId] = dbid;
		gPlace[i][plKind] = kind;
		gPlace[i][plArg] = arg;
		gPlace[i][plX] = x; gPlace[i][plY] = y; gPlace[i][plZ] = z; gPlace[i][plA] = a;
		gPlace[i][plWorld] = world; gPlace[i][plInt] = interior;
		Place_Build(i);
		return i;
	}
	return -1;
}

static Place_Insert(kind, arg, Float:x, Float:y, Float:z, Float:a, world, interior)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO places (kind, arg, x, y, z, a, world, interior) VALUES (%d, %d, %f, %f, %f, %f, %d, %d)",
		kind, arg, x, y, z, a, world, interior));
	new dbid = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT last_insert_rowid()");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) dbid = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return Place_AddMem(dbid, kind, arg, x, y, z, a, world, interior);
}

stock Places_Init()
{
	for (new i = 0; i < MAX_PLACES; i++)
	{
		gPlace[i][plUsed] = false;
		gPlace[i][plMk] = -1;
		gPlace[i][plActor] = INVALID_ACTOR_ID;
	}
	new bool:fresh = !DB_TableExists("places");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS places (id INTEGER PRIMARY KEY AUTOINCREMENT, kind INTEGER NOT NULL, arg INTEGER DEFAULT 0, x REAL, y REAL, z REAL, a REAL DEFAULT 0, world INTEGER DEFAULT 0, interior INTEGER DEFAULT 0)"));
	if (fresh)
	{
		for (new d = 0; d < sizeof (gPlaceDefs); d++)
			Place_Insert(gPlaceDefs[d][pdKind], gPlaceDefs[d][pdArg], gPlaceDefs[d][pdX], gPlaceDefs[d][pdY], gPlaceDefs[d][pdZ], gPlaceDefs[d][pdA], 0, 0);
		print("[Places] default places created.");
		return;
	}
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, kind, arg, x, y, z, a, world, interior FROM places ORDER BY id");
	if (r == DBResult:0) return;
	new rows = DB_GetRowCount(r);
	for (new n = 0; n < rows; n++)
	{
		new kind = DB_GetFieldInt(r, 1);
		if (kind >= 0 && kind < PL_KINDS)
			Place_AddMem(DB_GetFieldInt(r, 0), kind, DB_GetFieldInt(r, 2), DB_GetFieldFloat(r, 3), DB_GetFieldFloat(r, 4),
				DB_GetFieldFloat(r, 5), DB_GetFieldFloat(r, 6), DB_GetFieldInt(r, 7), DB_GetFieldInt(r, 8));
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	printf("[Places] %d places.", rows);
}

// Есть ли таблица (для первого запуска)
stock bool:DB_TableExists(const table[])
{
	new bool:ok = false;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT name FROM sqlite_master WHERE type = 'table' AND name = '%s'", table);
	if (r != DBResult:0)
	{
		ok = DB_GetRowCount(r) > 0;
		DB_FreeResultSet(r);
	}
	return ok;
}

// Маркер точки двигали в /markers
stock Place_OnMarkerMoved(ref, Float:x, Float:y, Float:z)
{
	for (new i = 0; i < MAX_PLACES; i++)
	{
		if (!gPlace[i][plUsed] || PLACE_REF + gPlace[i][plDbId] != ref) continue;
		gPlace[i][plX] = x; gPlace[i][plY] = y; gPlace[i][plZ] = z;
		if (gPlace[i][plMk] != -1)
		{
			gPlace[i][plA] = Marker_GetAngle(gPlace[i][plMk]);
			gPlace[i][plWorld] = Marker_GetWorld(gPlace[i][plMk]);
			gPlace[i][plInt] = Marker_GetInterior(gPlace[i][plMk]);
		}
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE places SET x = %f, y = %f, z = %f, a = %f, world = %d, interior = %d WHERE id = %d",
			x, y, z, gPlace[i][plA], gPlace[i][plWorld], gPlace[i][plInt], gPlace[i][plDbId]));
		if (gPlace[i][plActor] != INVALID_ACTOR_ID)
		{
			SetActorPos(gPlace[i][plActor], x, y, z);
			SetActorFacingAngle(gPlace[i][plActor], gPlace[i][plA]);
			SetActorVirtualWorld(gPlace[i][plActor], gPlace[i][plWorld]);
		}
		return;
	}
}

// ---------------------------------------------------------------------
//  Для модулей
// ---------------------------------------------------------------------
stock Place_Count(kind, arg = -1)
{
	new n = 0;
	for (new i = 0; i < MAX_PLACES; i++)
		if (gPlace[i][plUsed] && gPlace[i][plKind] == kind && (arg == -1 || gPlace[i][plArg] == arg)) n++;
	return n;
}

// Случайная точка вида (кроме except); -1 — нет ни одной
stock Place_Random(kind, except = -1, arg = -1)
{
	new list[MAX_PLACES], n = 0;
	for (new i = 0; i < MAX_PLACES; i++)
		if (gPlace[i][plUsed] && gPlace[i][plKind] == kind && i != except && (arg == -1 || gPlace[i][plArg] == arg)) list[n++] = i;
	if (n == 0) return (except != -1 && gPlace[except][plUsed]) ? except : -1;
	return list[random(n)];
}

// Ближайшая точка вида в радиусе; -1 — нет
stock Place_Nearby(playerid, kind, Float:range)
{
	new best = -1, Float:bd = range;
	new world = GetPlayerVirtualWorld(playerid), interior = GetPlayerInterior(playerid);
	for (new i = 0; i < MAX_PLACES; i++)
	{
		if (!gPlace[i][plUsed] || gPlace[i][plKind] != kind) continue;
		if (gPlace[i][plWorld] != world || gPlace[i][plInt] != interior) continue;
		new Float:d = GetPlayerDistanceFromPoint(playerid, gPlace[i][plX], gPlace[i][plY], gPlace[i][plZ]);
		if (d < bd) { bd = d; best = i; }
	}
	return best;
}

// Первая точка вида (для камер, выхода и т.п.)
stock Place_First(kind)
{
	for (new i = 0; i < MAX_PLACES; i++) if (gPlace[i][plUsed] && gPlace[i][plKind] == kind) return i;
	return -1;
}

// Поставить игрока в точку (с интерьером и миром)
stock Place_Teleport(playerid, i)
{
	if (i < 0 || i >= MAX_PLACES || !gPlace[i][plUsed]) return;
	SetPlayerInterior(playerid, gPlace[i][plInt]);
	SetPlayerVirtualWorld(playerid, gPlace[i][plWorld]);
	SetPlayerPos(playerid, gPlace[i][plX], gPlace[i][plY], gPlace[i][plZ]);
	SetPlayerFacingAngle(playerid, gPlace[i][plA]);
	SetCameraBehindPlayer(playerid);
}

// ---------------------------------------------------------------------
//  Команды админа
// ---------------------------------------------------------------------
static Places_KindsList(playerid)
{
	new line[144] = "Виды:";
	for (new k = 0; k < PL_KINDS; k++)
	{
		new part[24];
		format(part, sizeof part, " %s", gPlaceKey[k]);
		if (strlen(line) + strlen(part) > 120) { SendClientMessage(playerid, COLOR_GREY, "%s", line); line[0] = '\0'; }
		strcat(line, part);
	}
	SendClientMessage(playerid, COLOR_GREY, "%s", line);
}

stock bool:Cmd_Place(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, key[16], job[16];
	if (!NextParam(params, pos, key))
	{
		SendClientMessage(playerid, COLOR_GREY, "Использование: /place [вид]   (в машине: /place car [fisher|farmer|collector|garbage])");
		Places_KindsList(playerid);
		return true;
	}
	new kind = Place_Kind(key);
	if (kind == -1) { Places_KindsList(playerid); return true; }
	new arg = 0;
	new Float:x, Float:y, Float:z, Float:a;
	if (kind == PL_CAR)
	{
		new v = GetPlayerVehicleID(playerid);
		if (v == 0) return SendClientMessage(playerid, COLOR_GREY, "Сядь в машину нужной модели и поставь её как надо.");
		if (!NextParam(params, pos, job) || (arg = WorkLife_JobByKey(job)) == JOB_NONE)
			return SendClientMessage(playerid, COLOR_GREY, "Использование: /place car [fisher|farmer|collector|garbage]");
		GetVehiclePos(v, x, y, z);
		GetVehicleZAngle(v, a);
		arg = arg * 1000 + GetVehicleModel(v);     // работа и модель в одном числе
	}
	else
	{
		GetPlayerPos(playerid, x, y, z);
		GetPlayerFacingAngle(playerid, a);
	}
	new i = Place_Insert(kind, arg, x, y, z, a, GetPlayerVirtualWorld(playerid), GetPlayerInterior(playerid));
	if (i == -1) return SendClientMessage(playerid, COLOR_GREY, "Слишком много точек (%d).", MAX_PLACES);
	if (kind == PL_CAR) WorkLife_SpawnCar(i);
	new log[48];
	format(log, sizeof log, "%s #%d", gPlaceKey[kind], gPlace[i][plDbId]);
	Admin_Log(playerid, "place", INVALID_PLAYER_ID, log);
	SendClientMessage(playerid, COLOR_ADMIN, "Точка «%s» №%d поставлена. Всего таких: %d.", gPlaceName[kind], gPlace[i][plDbId], Place_Count(kind));
	return true;
}

stock bool:Cmd_Places(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new pos = 0, key[16];
	if (!NextParam(params, pos, key))
	{
		SendClientMessage(playerid, COLOR_ADMIN, "Точки систем (/places [вид] — список):");
		for (new k = 0; k < PL_KINDS; k++)
			SendClientMessage(playerid, COLOR_WHITE, " %s — %s: %d", gPlaceKey[k], gPlaceName[k], Place_Count(k));
		return true;
	}
	new kind = Place_Kind(key);
	if (kind == -1) { Places_KindsList(playerid); return true; }
	new Float:px, Float:py, Float:pz, n = 0;
	GetPlayerPos(playerid, px, py, pz);
	SendClientMessage(playerid, COLOR_ADMIN, "%s (телепорт: /gotoplace [№]):", gPlaceName[kind]);
	for (new i = 0; i < MAX_PLACES && n < 30; i++)
	{
		if (!gPlace[i][plUsed] || gPlace[i][plKind] != kind) continue;
		new extra[24];
		if (kind == PL_CAR) format(extra, sizeof extra, " (%s, модель %d)", WorkLife_JobKey(gPlace[i][plArg] / 1000), gPlace[i][plArg] % 1000);
		SendClientMessage(playerid, COLOR_WHITE, " №%d%s — %d м, инт %d, мир %d", gPlace[i][plDbId], extra,
			floatround(GetPlayerDistanceFromPoint(playerid, gPlace[i][plX], gPlace[i][plY], gPlace[i][plZ])), gPlace[i][plInt], gPlace[i][plWorld]);
		n++;
	}
	if (n == 0) SendClientMessage(playerid, COLOR_GREY, " Нет ни одной. Поставить: /place %s", gPlaceKey[kind]);
	return true;
}

stock bool:Cmd_Unplace(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new best = -1, Float:bd = 5.0;
	new v = GetPlayerVehicleID(playerid);
	for (new i = 0; i < MAX_PLACES; i++)
	{
		if (!gPlace[i][plUsed]) continue;
		new Float:d = GetPlayerDistanceFromPoint(playerid, gPlace[i][plX], gPlace[i][plY], gPlace[i][plZ]);
		if (d < bd) { bd = d; best = i; }
	}
	if (v != 0 && best == -1) best = WorkLife_PlaceOfCar(v);
	if (best == -1) return SendClientMessage(playerid, COLOR_GREY, "Рядом (5 м) нет точек. Список: /places");
	new kind = gPlace[best][plKind];
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM places WHERE id = %d", gPlace[best][plDbId]));
	if (gPlace[best][plMk] != -1) { Marker_Destroy(gPlace[best][plMk], true); gPlace[best][plMk] = -1; }
	if (kind == PL_CAR) WorkLife_RemoveCar(best);
	gPlace[best][plUsed] = false;
	Place_Build(best);
	SendClientMessage(playerid, COLOR_ADMIN, "Точка «%s» №%d удалена.", gPlaceName[kind], gPlace[best][plDbId]);
	return true;
}

stock bool:Cmd_GotoPlace(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new pos = 0, id;
	if (!NextIntParam(params, pos, id)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /gotoplace [№]  (номера: /places [вид])");
	for (new i = 0; i < MAX_PLACES; i++)
	{
		if (!gPlace[i][plUsed] || gPlace[i][plDbId] != id) continue;
		Teleport_SaveBack(playerid);
		Place_Teleport(playerid, i);
		return SendClientMessage(playerid, COLOR_ADMIN, "Ты у точки «%s» №%d.", gPlaceName[gPlace[i][plKind]], id);
	}
	return SendClientMessage(playerid, COLOR_GREY, "Нет точки с таким номером.");
}
