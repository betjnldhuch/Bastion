// =====================================================================
//  business.inc — бизнесы: магазины, бары, одежда, оружие.
//
//  Любой игрок может купить товар: подойди к значку бизнеса и введи /buy.
//  Бизнес можно купить (/buybiz). Тогда владельцу в кассу идёт
//  BIZ_SALE_SHARE% от каждой покупки и BIZ_PAYDAY_INCOME каждый PayDay.
//  Забрать деньги из кассы: /bizcash (стоя у своего бизнеса).
//
//  Типы: 1 — магазин 24/7, 2 — бар, 3 — одежда, 4 — оружейный, 5 — казино (casino.inc).
//  Админы (4+): /createbiz [тип] [цена] [название], /delbiz
//
//  Интерьер: у каждого бизнеса стандартный интерьер GTA по его типу (временное решение).
//  ALT или /enter у значка — войти, внутри ALT в зале — магазин, ALT у двери или /exit — выйти.
//  У каждого бизнеса свой виртуальный мир (BIZ_WORLD_OFFSET + номер).
//  /setbizint [№] — своя точка внутри (стоя в интерьере), /setbizint [№] def|off — стандартный / без интерьера,
//  /setbizint [№] out — переставить вход снаружи (стоя у двери спиной к ней).
//  Вход и выход — как у зданий фракций: стрелки-маркеры с надписью, оба двигаются в /markers,
//  игрок появляется на шаг перед маркером лицом от двери. /bizlock — закрыть вход (владелец или админ 4+).
// =====================================================================

#define BIZ_RANGE           2.5
#define BIZ_WORLD_OFFSET    5000        // вирт. мир интерьера = 5000 + номер бизнеса в базе
#define BIZ_EXIT_RANGE      1.5         // как близко к стрелке внутри срабатывает выход
#define BIZ_IN_REF          1000000     // ref маркера выхода внутри: BIZ_IN_REF + номер бизнеса в базе
#define GUNS_MIN_LEVEL      2           // с какого уровня продают оружие

enum
{
	BIZ_NONE,
	BIZ_SHOP,       // 1 — 24/7
	BIZ_BAR,        // 2 — бар
	BIZ_CLOTHES,    // 3 — одежда
	BIZ_GUNS,       // 4 — оружейный
	BIZ_CASINO      // 5 — казино (casino.inc)
}
#define BIZ_TYPES           6

static const gBizTypeNames[BIZ_TYPES][24] = { "-", "Магазин 24/7", "Бар", "Магазин одежды", "Оружейный магазин", "Казино" };

// Стандартные интерьеры по типу бизнеса: номер интерьера и точка у двери (x, y, z, угол)
static const gBizIntId[BIZ_TYPES] = { 0, 17, 11, 15, 1, 10 };
static const Float:gBizIntPos[BIZ_TYPES][4] =
{
	{ 0.0, 0.0, 0.0, 0.0 },
	{ -25.88, -185.87, 1003.55, 0.0 },      // 24/7
	{ 501.98, -69.15, 998.76, 180.0 },      // бар Ten Green Bottles
	{ 207.74, -109.02, 1005.13, 0.0 },      // Binco
	{ 286.15, -40.64, 1001.52, 0.0 },       // Ammu-Nation
	{ 2018.95, 1017.09, 996.88, 90.0 }      // казино Four Dragons
};

// Что можно купить
enum
{
	ITEM_PHONE,     // телефон
	ITEM_HEAL,      // +здоровье (value = сколько)
	ITEM_WEAPON,    // оружие (value = id оружия, value2 = патроны)
	ITEM_ARMOUR,    // бронежилет
	ITEM_SKIN,      // одежда (value = скин)
	ITEM_FOOD,      // еда в баре сразу (value = +HP, value2 = +% сытости)
	ITEM_INV        // вещь в инвентарь (value = INV_* из needs.inc, value2 = штук)
}

enum E_BIZ_ITEM
{
	biType,
	biName[24],
	biPrice,
	biKind,
	biValue,
	biValue2
}
static const gBizItems[][E_BIZ_ITEM] =
{
	{ BIZ_SHOP,    "Телефон",           PHONE_PRICE, ITEM_PHONE,  0,  0   },
	{ BIZ_SHOP,    "Аптечка (в сумку)", 150,  ITEM_INV,    INV_MEDKIT, 1 },
	{ BIZ_SHOP,    "Бургер (в сумку)",  60,   ITEM_INV,    INV_BURGER, 1 },
	{ BIZ_SHOP,    "Хот-дог (в сумку)", 30,   ITEM_INV,    INV_HOTDOG, 1 },
	{ BIZ_SHOP,    "Ремкомплект",       400,  ITEM_INV,    INV_REPAIRKIT, 1 },
	{ BIZ_SHOP,    "Пицца (в сумку)",   90,   ITEM_INV,    INV_PIZZA, 1 },
	{ BIZ_SHOP,    "Вода (в сумку)",    10,   ITEM_INV,    INV_WATER, 1 },
	{ BIZ_SHOP,    "Пиво (в сумку)",    25,   ITEM_INV,    INV_BEER, 1 },
	{ BIZ_SHOP,    "Сигареты (10 шт.)", 40,   ITEM_INV,    INV_CIGARETTES, 10 },
	{ BIZ_SHOP,    "Бинт",              50,   ITEM_INV,    INV_BANDAGE, 1 },
	{ BIZ_SHOP,    "Цветы (в сумку)",   50,   ITEM_INV,    INV_FLOWERS, 1 },
	{ BIZ_SHOP,    "Игральные кости",   30,   ITEM_INV,    INV_DICE, 1 },
	{ BIZ_SHOP,    "Маска",             500,  ITEM_INV,    INV_MASK, 1 },
	{ BIZ_SHOP,    "Бита",              200,  ITEM_WEAPON, 5,  1   },
	{ BIZ_BAR,     "Пиво (+10 HP)",     20,   ITEM_FOOD,   10, 5   },
	{ BIZ_BAR,     "Виски (+25 HP)",    50,   ITEM_FOOD,   25, 5   },
	{ BIZ_BAR,     "Обед (+60 HP, сытость)", 120, ITEM_FOOD, 60, 60 },
	{ BIZ_CLOTHES, "Скин 7",            300,  ITEM_SKIN,   7,  0   },
	{ BIZ_CLOTHES, "Скин 23",           300,  ITEM_SKIN,   23, 0   },
	{ BIZ_CLOTHES, "Скин 29",           300,  ITEM_SKIN,   29, 0   },
	{ BIZ_CLOTHES, "Скин 46",           400,  ITEM_SKIN,   46, 0   },
	{ BIZ_CLOTHES, "Скин 72",           300,  ITEM_SKIN,   72, 0   },
	{ BIZ_CLOTHES, "Скин 12 (жен.)",    300,  ITEM_SKIN,   12, 0   },
	{ BIZ_CLOTHES, "Скин 40 (жен.)",    300,  ITEM_SKIN,   40, 0   },
	{ BIZ_CLOTHES, "Скин 93 (жен.)",    400,  ITEM_SKIN,   93, 0   },
	{ BIZ_GUNS,    "Пистолет Colt",     500,  ITEM_WEAPON, 22, 60  },
	{ BIZ_GUNS,    "Desert Eagle",      1500, ITEM_WEAPON, 24, 35  },
	{ BIZ_GUNS,    "Дробовик",          2000, ITEM_WEAPON, 25, 30  },
	{ BIZ_GUNS,    "MP5",               3000, ITEM_WEAPON, 29, 120 },
	{ BIZ_GUNS,    "Бронежилет",        1000, ITEM_ARMOUR, 100, 0  }
};

enum E_BIZ
{
	bDbId,
	bName[32],
	bType,
	bOwner,                     // id аккаунта владельца (0 = продаётся)
	bOwnerName[MAX_PLAYER_NAME],
	bPrice,
	Float:bX,
	Float:bY,
	Float:bZ,
	bTill,                      // деньги в кассе
	Float:bA,                   // куда смотрит вход (-1 — не задано: выход прямо на значок, как раньше)
	bool:bLocked,               // закрыт: войти может только владелец
	bMarker,                    // маркер (markers.inc)
	bIntMode,                   // как в базе: -1 стандартный по типу, 0 без интерьера, иначе своя точка
	bInt,                       // интерьер (0 — входа нет, торговля у значка)
	Float:bIX, Float:bIY, Float:bIZ, Float:bIA,   // точка внутри у двери
	bExitMk                     // маркер-стрелка у двери внутри (markers.inc)
}
new Biz[MAX_BUSINESSES][E_BIZ];
new gBizCount = 0;

static gBuyBiz[MAX_PLAYERS];            // у какого бизнеса открыто окно покупки
static gBuyList[MAX_PLAYERS][20];       // строка окна -> номер товара

// Бизнесы по умолчанию (координаты примерные)
enum E_BIZ_DEF
{
	bdName[32],
	bdType,
	bdPrice,
	Float:bdX,
	Float:bdY,
	Float:bdZ
}
static const gBizDefaults[][E_BIZ_DEF] =
{
	{ "24/7 Idlewood",     BIZ_SHOP,    40000, 1833.0, -1842.6, 13.58 },
	{ "Ten Green Bottles", BIZ_BAR,     30000, 2309.9, -1643.4, 14.83 },
	{ "Binco Ganton",      BIZ_CLOTHES, 35000, 2244.4, -1665.5, 15.48 },
	{ "Ammu-Nation",       BIZ_GUNS,    80000, 1368.4, -1279.1, 13.55 }
};

stock Biz_GetTypeName(type)
{
	new name[24];
	if (type > BIZ_NONE && type < BIZ_TYPES) strcat(name, gBizTypeNames[type]);
	else strcat(name, "-");
	return name;
}

stock Biz_Refresh(b)
{
	new text[MK_TEXT_LEN];
	new act[32] = "ALT — купить товар";
	if (Biz[b][bInt] != 0)
	{
		act[0] = '\0';
		strcat(act, Biz[b][bLocked] ? ("{FF6347}Закрыто{FFFFFF}") : ("ALT — войти"));
	}
	if (Biz[b][bOwner] == 0)
	{
		format(text, sizeof text, "{FF9900}%s\n{FFFFFF}%s\n%s\nБизнес продаётся: ${FFFF00}%d{FFFFFF} (/buybiz)",
			Biz[b][bName], Biz_GetTypeName(Biz[b][bType]), act, Biz[b][bPrice]);
	}
	else
	{
		format(text, sizeof text, "{FF9900}%s\n{FFFFFF}%s\n%s\nВладелец: %s",
			Biz[b][bName], Biz_GetTypeName(Biz[b][bType]), act, Biz[b][bOwnerName]);
	}
	if (Biz[b][bMarker] == -1)
	{
		new gps = GPS_SHOPS;
		if (Biz[b][bType] == BIZ_BAR) gps = GPS_FUN;
		new Float:a = (Biz[b][bA] < 0.0) ? 0.0 : Biz[b][bA];
		Biz[b][bMarker] = Marker_Create(MK_BIZ, Biz[b][bDbId], Biz[b][bName], 1239, Biz[b][bX], Biz[b][bY], Biz[b][bZ], 0, 0, gps, a);
	}
	Marker_SetLabel(Biz[b][bMarker], text);
}

// Админ перенёс маркер бизнеса (/markers)
stock Biz_OnMarkerMoved(dbid, Float:x, Float:y, Float:z)
{
	// стрелка выхода внутри: точка становится своей (интерьер тот же)
	if (dbid >= BIZ_IN_REF)
	{
		new bi = Biz_FindByDbId(dbid - BIZ_IN_REF);
		if (bi == -1) return;
		Biz[bi][bIX] = x; Biz[bi][bIY] = y; Biz[bi][bIZ] = z;
		Biz[bi][bIntMode] = 1;
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE businesses SET imode = 1, iint = %d, ix = %f, iy = %f, iz = %f, ia = %f WHERE id = %d",
			Biz[bi][bInt], x, y, z, Biz[bi][bIA], Biz[bi][bDbId]));
		return;
	}
	for (new b = 0; b < gBizCount; b++)
	{
		if (Biz[b][bDbId] != dbid) continue;
		Biz[b][bX] = x;
		Biz[b][bY] = y;
		Biz[b][bZ] = z;
		return;
	}
}

stock Biz_Add(dbid, const name[], type, owner, const ownerName[], price, Float:x, Float:y, Float:z, till, Float:a = -1.0, bool:locked = false)
{
	if (gBizCount >= MAX_BUSINESSES) return -1;
	new b = gBizCount++;
	Biz[b][bDbId] = dbid;
	Biz[b][bName][0] = '\0';
	strcat(Biz[b][bName], name, 32);
	Biz[b][bType] = type;
	Biz[b][bOwner] = owner;
	Biz[b][bOwnerName][0] = '\0';
	strcat(Biz[b][bOwnerName], ownerName, MAX_PLAYER_NAME);
	Biz[b][bPrice] = price;
	Biz[b][bX] = x;
	Biz[b][bY] = y;
	Biz[b][bZ] = z;
	Biz[b][bTill] = till;
	Biz[b][bA] = a;
	Biz[b][bLocked] = locked;
	Biz[b][bMarker] = -1;
	Biz[b][bExitMk] = -1;
	Biz_ApplyInt(b, -1, 0, 0.0, 0.0, 0.0, 0.0);     // стандартный интерьер по типу
	Biz_Refresh(b);
	return b;
}

// Интерьер бизнеса. mode: -1 — стандартный по типу, 0 — без интерьера, 1 — своя точка (interior, x, y, z, a)
stock Biz_ApplyInt(b, mode, interior, Float:x, Float:y, Float:z, Float:a)
{
	new t = Biz[b][bType];
	if (mode < 0 && (t <= BIZ_NONE || t >= BIZ_TYPES)) mode = 0;
	if (mode > 0 && interior == 0) mode = 0;
	Biz[b][bIntMode] = mode;
	if (mode < 0)
	{
		Biz[b][bInt] = gBizIntId[t];
		Biz[b][bIX] = gBizIntPos[t][0];
		Biz[b][bIY] = gBizIntPos[t][1];
		Biz[b][bIZ] = gBizIntPos[t][2];
		Biz[b][bIA] = gBizIntPos[t][3];
	}
	else if (mode == 0) Biz[b][bInt] = 0;
	else
	{
		Biz[b][bInt] = interior;
		Biz[b][bIX] = x;
		Biz[b][bIY] = y;
		Biz[b][bIZ] = z;
		Biz[b][bIA] = a;
	}
	if (Biz[b][bExitMk] != -1) Marker_Destroy(Biz[b][bExitMk]);
	Biz[b][bExitMk] = -1;
	if (Biz[b][bInt] != 0)
	{
		new name[32];
		strcat(name, Biz[b][bName], 22);
		strcat(name, ", выход");
		new Float:ix = Biz[b][bIX], Float:iy = Biz[b][bIY], Float:iz = Biz[b][bIZ];
		Biz[b][bExitMk] = Marker_Create(MK_BIZ, BIZ_IN_REF + Biz[b][bDbId], name, DOOR_PICKUP_MODEL, ix, iy, iz,
			BIZ_WORLD_OFFSET + Biz[b][bDbId], Biz[b][bInt], GPS_NONE, Biz[b][bIA]);
		Biz[b][bIX] = ix; Biz[b][bIY] = iy; Biz[b][bIZ] = iz;
		Marker_SetLabel(Biz[b][bExitMk], "{FFFFFF}ALT — выйти", COLOR_WHITE, 10.0);
	}
}

// Выход внутри поставлен заново или бизнес удалён: забыть правки стрелки из /markers
static Biz_ForgetExitMarker(dbid)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM markers WHERE type = %d AND ref = %d", MK_BIZ, BIZ_IN_REF + dbid));
}

// Игрок внутри интерьера бизнеса b
stock bool:Biz_IsInside(playerid, b)
{
	return (Biz[b][bInt] != 0 && GetPlayerVirtualWorld(playerid) == BIZ_WORLD_OFFSET + Biz[b][bDbId]);
}

stock Biz_Enter(playerid, b)
{
	if (Biz[b][bLocked] && Biz[b][bOwner] != Player[playerid][pDbId] && Player[playerid][pAdmin] < 4)
	{
		SendClientMessage(playerid, COLOR_GREY, "«%s» закрыт.", Biz[b][bName]);
		return;
	}
	Door_SpawnAt(playerid, Biz[b][bExitMk]);     // шаг перед стрелкой внутри, лицом в зал (doors.inc)
	Player[playerid][pHouseInside] = -1;
	SendClientMessage(playerid, COLOR_WHITE, "%s. Купить товар: ALT в зале или /buy. Выйти: ALT у двери или /exit.", Biz[b][bName]);
}

stock Biz_Exit(playerid, b)
{
	// вход с заданным направлением — шаг перед дверью лицом на улицу; старые бизнесы — прямо на значок
	if (Biz[b][bA] >= 0.0 && Marker_IsValid(Biz[b][bMarker])) Door_SpawnAt(playerid, Biz[b][bMarker]);
	else
	{
		new Float:x = Biz[b][bX], Float:y = Biz[b][bY], Float:z = Biz[b][bZ];
		Marker_GetPos(Biz[b][bMarker], x, y, z);
		SetPlayerInterior(playerid, 0);
		SetPlayerVirtualWorld(playerid, 0);
		SetPlayerPos(playerid, x, y, z);
		SetCameraBehindPlayer(playerid);
	}
	Player[playerid][pHouseInside] = -1;
}

// /gotobiz [№] — ко входу бизнеса, /gotobizint [№] — внутрь (админ 3+), /bizlist — все бизнесы
static Biz_FromParam(playerid, const params[], const usage[])
{
	new pos = 0, dbid;
	if (!NextIntParam(params, pos, dbid)) { SendClientMessage(playerid, COLOR_GREY, "%s", usage); return -1; }
	new b = Biz_FindByDbId(dbid);
	if (b == -1) SendClientMessage(playerid, COLOR_GREY, "Бизнеса с таким номером нет. Список: /bizlist");
	return b;
}

stock bool:Cmd_GotoBiz(playerid, const params[], bool:inside)
{
	if (!Admin_Check(playerid, 3)) return true;
	new b = Biz_FromParam(playerid, params, inside ? ("Использование: /gotobizint [№] — внутрь бизнеса. Список: /bizlist") : ("Использование: /gotobiz [№] — ко входу бизнеса. Список: /bizlist"));
	if (b == -1) return true;
	if (inside && Biz[b][bInt] == 0) return SendClientMessage(playerid, COLOR_GREY, "У «%s» нет интерьера. Ко входу: /gotobiz %d", Biz[b][bName], Biz[b][bDbId]);
	Teleport_SaveBack(playerid);
	if (IsPlayerInAnyVehicle(playerid)) RemovePlayerFromVehicle(playerid);
	if (inside) Door_SpawnAt(playerid, Biz[b][bExitMk]);
	else
	{
		new Float:x = Biz[b][bX], Float:y = Biz[b][bY], Float:z = Biz[b][bZ];
		Marker_GetPos(Biz[b][bMarker], x, y, z);
		SetPlayerInterior(playerid, 0);
		SetPlayerVirtualWorld(playerid, 0);
		SetPlayerPos(playerid, x + 1.0, y, z);
		SetCameraBehindPlayer(playerid);
	}
	Player[playerid][pHouseInside] = -1;
	SendClientMessage(playerid, COLOR_ADMIN, "«%s» (№%d)%s. Назад: /back", Biz[b][bName], Biz[b][bDbId], inside ? (", внутри") : (""));
	return true;
}

stock bool:Cmd_BizList(playerid)
{
	if (!Admin_Check(playerid, 3)) return true;
	if (gBizCount == 0) return SendClientMessage(playerid, COLOR_GREY, "Бизнесов нет.");
	SendClientMessage(playerid, COLOR_YELLOW, "Бизнесы (№ — название, тип):");
	new line[144], part[64];
	for (new b = 0; b < gBizCount; b++)
	{
		format(part, sizeof part, "%d %s (%s)%s   ", Biz[b][bDbId], Biz[b][bName], Biz_GetTypeName(Biz[b][bType]), Biz[b][bLocked] ? (" закр.") : (""));
		if (strlen(line) + strlen(part) >= sizeof (line) - 1)
		{
			SendClientMessage(playerid, COLOR_WHITE, "%s", line);
			line[0] = '\0';
		}
		strcat(line, part);
	}
	if (line[0] != '\0') SendClientMessage(playerid, COLOR_WHITE, "%s", line);
	return true;
}

// /bizlock — закрыть или открыть вход (владелец у своего бизнеса или админ 4+ у любого)
stock bool:Cmd_BizLock(playerid)
{
	new b = Biz_GetNearby(playerid);
	if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "Подойди к бизнесу (или зайди внутрь).");
	if (Biz[b][bOwner] != Player[playerid][pDbId] && Player[playerid][pAdmin] < 4) return SendClientMessage(playerid, COLOR_GREY, "Это не твой бизнес.");
	if (Biz[b][bInt] == 0) return SendClientMessage(playerid, COLOR_GREY, "У этого бизнеса нет интерьера: торговля идёт у значка.");
	Biz[b][bLocked] = !Biz[b][bLocked];
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE businesses SET locked = %d WHERE id = %d", Biz[b][bLocked] ? 1 : 0, Biz[b][bDbId]));
	Biz_Refresh(b);
	if (Biz[b][bLocked]) SendClientMessage(playerid, COLOR_ORANGE, "«%s» закрыт: войти можешь только ты. Открыть: /bizlock", Biz[b][bName]);
	else SendClientMessage(playerid, COLOR_GREEN, "«%s» открыт для покупателей.", Biz[b][bName]);
	return true;
}

// Дверь бизнеса: снаружи у значка — войти, внутри у двери — выйти. true — сработало.
stock bool:Biz_TryDoor(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return false;
	new b = Biz_GetNearby(playerid);
	if (b == -1 || Biz[b][bInt] == 0) return false;
	if (Biz_IsInside(playerid, b))
	{
		if (!Marker_InRange(playerid, Biz[b][bExitMk], BIZ_EXIT_RANGE)) return false;
		Biz_Exit(playerid, b);
		return true;
	}
	Biz_Enter(playerid, b);
	return true;
}

stock Biz_FindByDbId(dbid)
{
	for (new b = 0; b < gBizCount; b++)
	{
		if (Biz[b][bDbId] == dbid) return b;
	}
	return -1;
}

stock Biz_Save(b)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE businesses SET owner = %d, till = %d WHERE id = %d",
		Biz[b][bOwner], Biz[b][bTill], Biz[b][bDbId]));
}

stock Business_Init()
{
	DB_AddColumn("businesses", "imode", "INTEGER DEFAULT -1");
	DB_AddColumn("businesses", "iint", "INTEGER DEFAULT 0");
	DB_AddColumn("businesses", "ix", "REAL DEFAULT 0");
	DB_AddColumn("businesses", "iy", "REAL DEFAULT 0");
	DB_AddColumn("businesses", "iz", "REAL DEFAULT 0");
	DB_AddColumn("businesses", "ia", "REAL DEFAULT 0");
	DB_AddColumn("businesses", "oa", "REAL DEFAULT -1");
	DB_AddColumn("businesses", "locked", "INTEGER DEFAULT 0");

	new count = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM businesses");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (count == 0)
	{
		for (new i = 0; i < sizeof (gBizDefaults); i++)
		{
			DB_FreeResultSet(DB_ExecuteQuery(gDB,
				"INSERT INTO businesses (name, type, price, x, y, z) VALUES ('%q', %d, %d, %f, %f, %f)",
				gBizDefaults[i][bdName], gBizDefaults[i][bdType], gBizDefaults[i][bdPrice],
				gBizDefaults[i][bdX], gBizDefaults[i][bdY], gBizDefaults[i][bdZ]));
		}
	}

	r = DB_ExecuteQuery(gDB, "SELECT b.id, b.name, b.type, b.owner, IFNULL(a.name, ''), b.price, b.x, b.y, b.z, b.till, b.imode, b.iint, b.ix, b.iy, b.iz, b.ia, b.oa, b.locked FROM businesses b LEFT JOIN accounts a ON a.id = b.owner ORDER BY b.id");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		new name[32], owner[MAX_PLAYER_NAME];
		for (new i = 0; i < rows; i++)
		{
			DB_GetFieldString(r, 1, name, sizeof name);
			DB_GetFieldString(r, 4, owner, sizeof owner);
			new b = Biz_Add(DB_GetFieldInt(r, 0), name, DB_GetFieldInt(r, 2), DB_GetFieldInt(r, 3), owner, DB_GetFieldInt(r, 5),
				DB_GetFieldFloat(r, 6), DB_GetFieldFloat(r, 7), DB_GetFieldFloat(r, 8), DB_GetFieldInt(r, 9),
				DB_GetFieldFloat(r, 16), DB_GetFieldInt(r, 17) != 0);
			if (b != -1 && DB_GetFieldInt(r, 10) >= 0)
			{
				Biz_ApplyInt(b, DB_GetFieldInt(r, 10), DB_GetFieldInt(r, 11),
					DB_GetFieldFloat(r, 12), DB_GetFieldFloat(r, 13), DB_GetFieldFloat(r, 14), DB_GetFieldFloat(r, 15));
				Biz_Refresh(b);
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Business] Loaded %d businesses.", gBizCount);
}

stock Biz_GetNearby(playerid)
{
	// внутри интерьера бизнеса: весь зал считается «рядом»
	new world = GetPlayerVirtualWorld(playerid);
	if (world >= BIZ_WORLD_OFFSET)
	{
		new inside = Biz_FindByDbId(world - BIZ_WORLD_OFFSET);
		if (inside != -1 && Biz[inside][bInt] == 0) inside = -1;
		return inside;
	}
	if (GetPlayerInterior(playerid) != 0 || world != 0) return -1;
	for (new b = 0; b < gBizCount; b++)
	{
		if (IsPlayerInRangeOfPoint(playerid, BIZ_RANGE, Biz[b][bX], Biz[b][bY], Biz[b][bZ])) return b;
	}
	return -1;
}

stock Biz_GetOwned(playerid)
{
	if (Player[playerid][pDbId] == 0) return -1;
	for (new b = 0; b < gBizCount; b++)
	{
		if (Biz[b][bOwner] == Player[playerid][pDbId]) return b;
	}
	return -1;
}

// Раз в PayDay (из rp.pwn): доход в кассы
stock Business_PayDay()
{
	for (new b = 0; b < gBizCount; b++)
	{
		if (Biz[b][bOwner] == 0) continue;
		Biz[b][bTill] += BIZ_PAYDAY_INCOME;
		Biz_Save(b);
	}
}

// Окно покупки (из OnDialogResponse)
stock Business_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid != DIALOG_BIZ_BUY) return 0;
	if (!response || listitem < 0 || listitem >= sizeof (gBuyList[])) return 1;

	new b = gBuyBiz[playerid];
	if (b < 0 || b >= gBizCount || Biz_GetNearby(playerid) != b)
	{
		SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от магазина.");
		return 1;
	}
	new item = gBuyList[playerid][listitem];
	if (item < 0 || item >= sizeof (gBizItems) || gBizItems[item][biType] != Biz[b][bType]) return 1;

	new price = gBizItems[item][biPrice];
	if (GetMoney(playerid) < price)
	{
		SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", price);
		return 1;
	}

	switch (gBizItems[item][biKind])
	{
		case ITEM_PHONE:
		{
			if (Player[playerid][pPhone] != 0)
			{
				SendClientMessage(playerid, COLOR_GREY, "У тебя уже есть телефон (номер %d).", Player[playerid][pPhone]);
				return 1;
			}
		}
		case ITEM_SKIN:
		{
			if (Outfit_FreeSlots(playerid) == 0)
			{
				SendClientMessage(playerid, COLOR_GREY, "В сумке нет места для одежды. Выброси или передай старую (/inv).");
				return 1;
			}
		}
		case ITEM_INV:
		{
			if (Inv_Count(playerid, gBizItems[item][biValue]) + gBizItems[item][biValue2] > Inv_GetMax(gBizItems[item][biValue]))
			{
				SendClientMessage(playerid, COLOR_GREY, "В сумке нет места: больше %d штук не унести (/inv).", Inv_GetMax(gBizItems[item][biValue]));
				return 1;
			}
		}
		case ITEM_WEAPON, ITEM_ARMOUR:
		{
			if (Biz[b][bType] == BIZ_GUNS && gBizItems[item][biKind] == ITEM_WEAPON && !Licenses_Has(playerid, LIC_GUN))
			{
				SendClientMessage(playerid, COLOR_GREY, "Оружие продают только с лицензией. Сдать на неё можно в автошколе у мэрии.");
				return 1;
			}
			if (Biz[b][bType] == BIZ_GUNS && Player[playerid][pLevel] < GUNS_MIN_LEVEL)
			{
				SendClientMessage(playerid, COLOR_GREY, "Оружие продают с %d уровня.", GUNS_MIN_LEVEL);
				return 1;
			}
		}
	}

	GiveMoney(playerid, -price);
	switch (gBizItems[item][biKind])
	{
		case ITEM_PHONE: Phone_Give(playerid);
		case ITEM_HEAL:
		{
			new Float:hp;
			GetPlayerHealth(playerid, hp);
			hp += float(gBizItems[item][biValue]);
			if (hp > 100.0) hp = 100.0;
			SetPlayerHealth(playerid, hp);
		}
		case ITEM_WEAPON: GivePlayerWeapon(playerid, WEAPON:gBizItems[item][biValue], gBizItems[item][biValue2]);
		case ITEM_ARMOUR: SetPlayerArmour(playerid, float(gBizItems[item][biValue]));
		case ITEM_INV:
		{
			Inv_Add(playerid, gBizItems[item][biValue], gBizItems[item][biValue2]);
			SendClientMessage(playerid, COLOR_WHITE, "%s в сумке. Открыть: /inv", Inv_GetName(gBizItems[item][biValue]));
		}
		case ITEM_FOOD:
		{
			new Float:hp;
			GetPlayerHealth(playerid, hp);
			hp += float(gBizItems[item][biValue]);
			if (hp > 100.0) hp = 100.0;
			SetPlayerHealth(playerid, hp);
			Needs_Feed(playerid, gBizItems[item][biValue2]);
			SendClientMessage(playerid, COLOR_WHITE, "Сытость: %d%%.", Needs_GetHunger(playerid));
		}
		case ITEM_SKIN:
		{
			Outfit_Add(playerid, gBizItems[item][biValue]);    // одежда в сумку (needs.inc)
			SendClientMessage(playerid, COLOR_WHITE, "Одежда (скин %d) в сумке. Надеть: /inv", gBizItems[item][biValue]);
		}
	}

	if (Biz[b][bOwner] != 0)
	{
		Biz[b][bTill] += price * BIZ_SALE_SHARE / 100;
		Biz_Save(b);
	}
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Ты купил: %s за $%d.", gBizItems[item][biName], price);
	return 1;
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Cmd_Buy(playerid)
{
	new b = Biz_GetNearby(playerid);
	if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "Подойди к значку магазина (буква i).");
	if (Biz[b][bInt] != 0 && !Biz_IsInside(playerid, b)) return SendClientMessage(playerid, COLOR_GREY, "Торгуют внутри. Войти: ALT или /enter у значка.");
	if (Biz[b][bType] == BIZ_CASINO) return Casino_Show(playerid, b);     // игры (casino.inc)

	new list[1024] = "Товар\tЦена\n", line[64], n = 0;
	for (new i = 0; i < sizeof (gBizItems) && n < sizeof (gBuyList[]); i++)
	{
		if (gBizItems[i][biType] != Biz[b][bType]) continue;
		format(line, sizeof line, "%s\t$%d\n", gBizItems[i][biName], gBizItems[i][biPrice]);
		strcat(list, line);
		gBuyList[playerid][n++] = i;
	}
	if (n == 0) return SendClientMessage(playerid, COLOR_GREY, "Здесь пока ничего не продают.");
	gBuyBiz[playerid] = b;
	ShowPlayerDialog(playerid, DIALOG_BIZ_BUY, DIALOG_STYLE_TABLIST_HEADERS, Biz[b][bName], "%s", "Купить", "Закрыть", list);
	return true;
}

stock bool:Cmd_BuyBiz(playerid)
{
	if (Debts_BlockBuy(playerid)) return true;      // большой долг штату (debts.inc)
	new b = Biz_GetNearby(playerid);
	if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "Подойди к значку бизнеса.");
	if (Biz[b][bOwner] != 0) return SendClientMessage(playerid, COLOR_GREY, "У этого бизнеса уже есть владелец.");
	if (Biz_GetOwned(playerid) != -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя уже есть бизнес. Продать: /sellbiz");
	if (GetMoney(playerid) < Biz[b][bPrice]) return SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", Biz[b][bPrice]);

	GiveMoney(playerid, -Biz[b][bPrice], "покупка бизнеса");
	Biz[b][bOwner] = Player[playerid][pDbId];
	Biz[b][bTill] = 0;
	GetPlayerName(playerid, Biz[b][bOwnerName], MAX_PLAYER_NAME);
	Biz_Save(b);
	Biz_Refresh(b);
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Поздравляем! «%s» теперь твой. Касса: /bizcash, информация: /bizinfo", Biz[b][bName]);
	return true;
}

stock bool:Cmd_SellBiz(playerid)
{
	new b = Biz_GetOwned(playerid);
	if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет бизнеса.");
	if (Biz_GetNearby(playerid) != b) return SendClientMessage(playerid, COLOR_GREY, "Подойди к своему бизнесу.");

	new refund = Biz[b][bPrice] / 2 + Biz[b][bTill];
	GiveMoney(playerid, refund, "продажа бизнеса");
	Biz[b][bOwner] = 0;
	Biz[b][bOwnerName][0] = '\0';
	Biz[b][bTill] = 0;
	Biz_Save(b);
	Biz_Refresh(b);
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Ты продал бизнес государству. Получено $%d (половина цены + касса).", refund);
	return true;
}

stock bool:Cmd_BizInfo(playerid)
{
	new b = Biz_GetOwned(playerid);
	if (b == -1) b = Biz_GetNearby(playerid);
	if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет бизнеса, и рядом его тоже нет.");
	SendClientMessage(playerid, COLOR_ORANGE, "%s (%s), №%d", Biz[b][bName], Biz_GetTypeName(Biz[b][bType]), Biz[b][bDbId]);
	if (Biz[b][bOwner] == 0) SendClientMessage(playerid, COLOR_WHITE, "Продаётся за $%d.", Biz[b][bPrice]);
	else SendClientMessage(playerid, COLOR_WHITE, "Владелец: %s.", Biz[b][bOwnerName]);
	if (Biz[b][bOwner] != 0 && Biz[b][bOwner] == Player[playerid][pDbId])
		SendClientMessage(playerid, COLOR_WHITE, "В кассе: $%d. Забрать: /bizcash (у бизнеса).", Biz[b][bTill]);
	return true;
}

stock bool:Cmd_BizCash(playerid)
{
	new b = Biz_GetOwned(playerid);
	if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет бизнеса.");
	if (Biz_GetNearby(playerid) != b) return SendClientMessage(playerid, COLOR_GREY, "Подойди к своему бизнесу.");
	if (Biz[b][bTill] <= 0) return SendClientMessage(playerid, COLOR_GREY, "Касса пуста.");
	new cash = Biz[b][bTill];
	Biz[b][bTill] = 0;
	GiveMoney(playerid, cash, "касса бизнеса");
	Biz_Save(b);
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Ты забрал из кассы $%d.", cash);
	return true;
}

stock bool:Cmd_CreateBiz(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, type, price, name[32];
	if (!NextIntParam(params, pos, type) || type <= BIZ_NONE || type >= BIZ_TYPES
		|| !NextIntParam(params, pos, price) || price < 1 || !RestParam(params, pos, name))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /createbiz [тип 1-5] [цена] [название]  (1 24/7, 2 бар, 3 одежда, 4 оружие, 5 казино)");
	if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return SendClientMessage(playerid, COLOR_GREY, "Бизнес можно ставить только на улице.");
	if (gBizCount >= MAX_BUSINESSES) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит бизнесов (MAX_BUSINESSES).");

	new Float:x, Float:y, Float:z, Float:a;
	GetPlayerPos(playerid, x, y, z);
	GetPlayerFacingAngle(playerid, a);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO businesses (name, type, price, x, y, z, oa) VALUES ('%q', %d, %d, %f, %f, %f, %f)",
		name, type, price, x, y, z, a));
	Biz_Add(DB_LastInsertId(), name, type, 0, "", price, x, y, z, 0, a);
	SendClientMessage(playerid, COLOR_ADMIN, "Бизнес «%s» (%s) создан, цена $%d. Ставь его у двери, спиной к ней: выходят сюда лицом на улицу.", name, Biz_GetTypeName(type), price);
	return true;
}

stock bool:Cmd_DelBiz(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new b = Biz_GetNearby(playerid);
	if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "Подойди к бизнесу.");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM businesses WHERE id = %d", Biz[b][bDbId]));
	// кто был внутри — на улицу (пока маркеры ещё есть)
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Biz_IsInside(i, b)) Biz_Exit(i, b);
	}
	Marker_Destroy(Biz[b][bMarker], true);
	if (Biz[b][bExitMk] != -1) Marker_Destroy(Biz[b][bExitMk], true);
	SendClientMessage(playerid, COLOR_ADMIN, "Бизнес «%s» удалён.", Biz[b][bName]);
	new last = gBizCount - 1;
	if (b != last) Biz[b] = Biz[last];
	gBizCount--;
	return true;
}

// /setbizint [№ бизнеса] — точка внутри там, где стоишь (в интерьере); def — стандартный по типу; off — без интерьера
stock bool:Cmd_SetBizInt(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, dbid, word[8];
	if (!NextIntParam(params, pos, dbid))
	{
		SendClientMessage(playerid, COLOR_GREY, "Использование: /setbizint [№ бизнеса] — точка у двери внутри там, где ты стоишь (в интерьере)");
		SendClientMessage(playerid, COLOR_GREY, "/setbizint [№] def — стандартный интерьер по типу, /setbizint [№] off — без интерьера. №: /bizinfo");
		SendClientMessage(playerid, COLOR_GREY, "/setbizint [№] out — переставить вход снаружи сюда (у двери, спиной к ней)");
		return true;
	}
	new b = Biz_FindByDbId(dbid);
	if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "Бизнеса с таким номером нет. Номер: /bizinfo у бизнеса.");

	new mode = 1, interior = GetPlayerInterior(playerid);
	new Float:x, Float:y, Float:z, Float:a;
	if (NextParam(params, pos, word))
	{
		if (!strcmp(word, "def", true)) mode = -1;
		else if (!strcmp(word, "off", true)) mode = 0;
		else if (!strcmp(word, "out", true))
		{
			if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return SendClientMessage(playerid, COLOR_GREY, "Вход ставится на улице.");
			GetPlayerPos(playerid, Biz[b][bX], Biz[b][bY], Biz[b][bZ]);
			GetPlayerFacingAngle(playerid, Biz[b][bA]);
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE businesses SET x = %f, y = %f, z = %f, oa = %f WHERE id = %d",
				Biz[b][bX], Biz[b][bY], Biz[b][bZ], Biz[b][bA], Biz[b][bDbId]));
			Marker_Destroy(Biz[b][bMarker], true);     // с правками /markers: место теперь новое
			Biz[b][bMarker] = -1;
			Biz_Refresh(b);
			return SendClientMessage(playerid, COLOR_ADMIN, "«%s»: вход переставлен сюда.", Biz[b][bName]);
		}
		else return SendClientMessage(playerid, COLOR_GREY, "Второе слово: def (стандартный интерьер), off (без интерьера) или out (вход снаружи).");
	}
	if (mode == 1)
	{
		if (interior == 0) return SendClientMessage(playerid, COLOR_GREY, "Встань внутри интерьера у двери (/gotoint) или добавь def / off.");
		GetPlayerPos(playerid, x, y, z);
		GetPlayerFacingAngle(playerid, a);
	}
	// кто внутри — на улицу: точка и мир могут смениться
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Biz_IsInside(i, b)) Biz_Exit(i, b);
	}
	Biz_ForgetExitMarker(Biz[b][bDbId]);
	Biz_ApplyInt(b, mode, interior, x, y, z, a);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE businesses SET imode = %d, iint = %d, ix = %f, iy = %f, iz = %f, ia = %f WHERE id = %d",
		Biz[b][bIntMode], interior, x, y, z, a, Biz[b][bDbId]));
	Biz_Refresh(b);
	if (mode == 1) SendClientMessage(playerid, COLOR_ADMIN, "«%s»: вход теперь ведёт сюда (интерьер %d).", Biz[b][bName], interior);
	else if (mode == -1) SendClientMessage(playerid, COLOR_ADMIN, "«%s»: стандартный интерьер по типу.", Biz[b][bName]);
	else SendClientMessage(playerid, COLOR_ADMIN, "«%s»: без интерьера, торговля у значка.", Biz[b][bName]);
	return true;
}
