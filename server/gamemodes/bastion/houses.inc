// =====================================================================
//  houses.inc — дома: покупка, продажа, вход/выход, замок,
//  налог, аренда комнат, шкаф с оружием и сейф.
//
//  Дома хранятся в таблице houses. При первом запуске создаются
//  несколько домов на Grove Street. Админ (4+) может добавить дом
//  там, где стоит: /createhouse [цена] [тип интерьера 1-5]
//  Вход можно ставить и внутри здания (общежитие: двери комнат в коридоре интерьера).
//  /sethouseint [№] — своя точка внутри дома (стоя в нужном интерьере),
//  /delhouse [№] — снести дом (без номера — тот, что рядом), /gotohouse [№] — телепорт к дому.
//
//  Счёт дома (houses.balance): с него каждый PayDay списывается налог
//  (0.1% цены, не меньше HOUSE_TAX_MIN), на него же приходит плата
//  жильцов. Пополнить: /housepay [сумма] откуда угодно. Если долг
//  больше налога за HOUSE_DEBT_HOURS часов — дом уходит государству.
//
//  Аренда: владелец ставит цену /setrent [сумма в час] (0 — закрыть),
//  игрок у двери пишет /rent. Жилец платит каждый PayDay, пока он
//  в игре, может входить в закрытый дом и появляется у него.
//  /unrent — съехать, владельцу: /tenants /evict [id] /evictall.
//
//  Меню дома: /house или ALT внутри дома (не у двери). Шкаф на
//  HOUSE_WEAPON_SLOTS оружий и сейф с деньгами — только владельцу.
// =====================================================================

#define MAX_HOUSES          200
#define HOUSE_WORLD_OFFSET  1000    // вирт. мир интерьера = 1000 + номер дома

#define HOUSE_TAX_MIN       20      // налог в час не меньше этой суммы
#define HOUSE_DEBT_HOURS    168     // сколько часов долга терпит государство (7 дней)
#define HOUSE_RENT_MAX      1000    // максимальная аренда в час
#define HOUSE_WEAPON_SLOTS  5       // мест в шкафу
#define HOUSE_SAFE_MAX      500000  // максимум денег в сейфе
#define HOUSE_STORE_RANGE   1.5     // как близко к шкафу открываются шкаф и сейф


enum E_HOUSE
{
	hDbId,
	hOwner,                     // id аккаунта владельца (0 = продаётся)
	hOwnerName[MAX_PLAYER_NAME],
	hPrice,
	Float:hX, Float:hY, Float:hZ,       // вход с улицы
	Float:hIX, Float:hIY, Float:hIZ,    // точка внутри
	hInterior,
	hOutInt, hOutWorld,                 // где стоит вход (0, 0 — улица; иначе коридор общежития и т.п.)
	bool:hLocked,
	hRent,                      // аренда в час (0 = не сдаётся)
	hBalance,                   // счёт дома (может быть в минусе — долг по налогу)
	hSafe,                      // деньги в сейфе
	hMarker,                    // маркер входа (markers.inc)
	bool:hHasStore,             // шкаф поставлен (/makestorage)
	Float:hSX, Float:hSY, Float:hSZ,    // где стоит шкаф (внутри дома)
	Text3D:hStoreLabel          // надпись «Шкаф» над ним
}
new House[MAX_HOUSES][E_HOUSE];

forward bool:Doors_Try(playerid);     // doors.inc подключён позже
forward bool:Biz_TryDoor(playerid);
forward bool:House_AtStore(playerid, h);
forward bool:House_IsInside(playerid, h);   // business.inc: вход и выход из интерьера бизнеса
new gHouseCount = 0;

static gRentHouse[MAX_PLAYERS] = { -1, ... };          // какой дом снимает игрок (-1 = нет)
static gMainHouse[MAX_PLAYERS];                         // id (в базе) главного дома: у него спавн. 0 — первый из своих
static gHouseGunSlot[MAX_PLAYERS][HOUSE_WEAPON_SLOTS];  // пункты списка "взять оружие"

// Готовые интерьеры для /createhouse (номер интерьера и координаты внутри)
static const gHouseIntId[] = { 1, 2, 3, 5, 8 };
static const Float:gHouseIntPos[][3] =
{
	{ 223.0731, 1287.0825, 1082.1406 },     // 1: маленький дом
	{ 2468.8411, -1698.2228, 1013.5078 },   // 2: дом Райдера
	{ 2496.0549, -1693.9263, 1014.7422 },   // 3: дом CJ
	{ 2233.8032, -1115.2614, 1050.8828 },   // 4: квартира
	{ 2365.2183, -1135.5964, 1050.8826 }    // 5: большой дом
};

// ---------------------------------------------------------------------
//  Новые поля базы (старая база дополняется сама)
// ---------------------------------------------------------------------
static Houses_Init()
{
	DB_AddColumn("houses", "rent", "INTEGER DEFAULT 0");
	DB_AddColumn("houses", "balance", "INTEGER DEFAULT 0");
	DB_AddColumn("houses", "safe", "INTEGER DEFAULT 0");
	DB_AddColumn("houses", "oint", "INTEGER DEFAULT 0");
	DB_AddColumn("houses", "oworld", "INTEGER DEFAULT 0");
	DB_AddColumn("houses", "has_st", "INTEGER DEFAULT 0");
	DB_AddColumn("houses", "stx", "REAL DEFAULT 0");
	DB_AddColumn("houses", "sty", "REAL DEFAULT 0");
	DB_AddColumn("houses", "stz", "REAL DEFAULT 0");
	DB_AddColumn("accounts", "rent_house", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "main_house", "INTEGER DEFAULT 0");   // главный дом, когда домов несколько
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS house_weapons (house INTEGER NOT NULL, slot INTEGER NOT NULL, weapon INTEGER, ammo INTEGER, PRIMARY KEY (house, slot))"));
}

// ---------------------------------------------------------------------
stock Houses_Load()
{
	Houses_Init();

	// Если домов нет совсем — создаём стартовые
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM houses");
	new count = 0;
	if (r != DBResult:0)
	{
		count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (count == 0)
	{
		Houses_InsertDefault(2495.3, -1690.9, 14.77, 50000, 3);   // дом CJ
		Houses_InsertDefault(2459.4, -1691.0, 13.55, 35000, 2);   // дом Райдера
		Houses_InsertDefault(2514.3, -1691.6, 14.05, 25000, 1);
		print("[Houses] Default houses created.");
	}

	r = DB_ExecuteQuery(gDB, "SELECT h.id, h.owner, h.price, h.x, h.y, h.z, h.ix, h.iy, h.iz, h.interior, h.locked, IFNULL(a.name, ''), h.rent, h.balance, h.safe, h.oint, h.oworld, h.has_st, h.stx, h.sty, h.stz FROM houses h LEFT JOIN accounts a ON a.id = h.owner ORDER BY h.id");
	if (r == DBResult:0) return;

	if (DB_GetRowCount(r) > 0)
	{
		do
		{
			if (gHouseCount >= MAX_HOUSES) break;
			new h = gHouseCount++;
			House[h][hDbId]     = DB_GetFieldInt(r, 0);
			House[h][hOwner]    = DB_GetFieldInt(r, 1);
			House[h][hPrice]    = DB_GetFieldInt(r, 2);
			House[h][hX]        = DB_GetFieldFloat(r, 3);
			House[h][hY]        = DB_GetFieldFloat(r, 4);
			House[h][hZ]        = DB_GetFieldFloat(r, 5);
			House[h][hIX]       = DB_GetFieldFloat(r, 6);
			House[h][hIY]       = DB_GetFieldFloat(r, 7);
			House[h][hIZ]       = DB_GetFieldFloat(r, 8);
			House[h][hInterior] = DB_GetFieldInt(r, 9);
			House[h][hLocked]   = (DB_GetFieldInt(r, 10) != 0);
			DB_GetFieldString(r, 11, House[h][hOwnerName], MAX_PLAYER_NAME);
			House[h][hRent]     = DB_GetFieldInt(r, 12);
			House[h][hBalance]  = DB_GetFieldInt(r, 13);
			House[h][hSafe]     = DB_GetFieldInt(r, 14);
			House[h][hOutInt]   = DB_GetFieldInt(r, 15);
			House[h][hOutWorld] = DB_GetFieldInt(r, 16);
			House[h][hHasStore] = (DB_GetFieldInt(r, 17) != 0);
			House[h][hSX]       = DB_GetFieldFloat(r, 18);
			House[h][hSY]       = DB_GetFieldFloat(r, 19);
			House[h][hSZ]       = DB_GetFieldFloat(r, 20);
			House[h][hStoreLabel] = Text3D:INVALID_3DTEXT_ID;
			House[h][hMarker] = -1;
			House_Refresh(h);
			House_BuildStore(h);
		}
		while (DB_SelectNextRow(r));
	}
	DB_FreeResultSet(r);
	printf("[Houses] Loaded %d houses.", gHouseCount);
}

stock Houses_InsertDefault(Float:x, Float:y, Float:z, price, type)
{
	new t = type - 1;
	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"INSERT INTO houses (price, x, y, z, ix, iy, iz, interior) VALUES (%d, %f, %f, %f, %f, %f, %f, %d)",
		price, x, y, z, gHouseIntPos[t][0], gHouseIntPos[t][1], gHouseIntPos[t][2], gHouseIntId[t]));
}

// Номер дома в массиве по id в базе (-1 = нет)
static House_FindByDbId(dbid)
{
	if (dbid <= 0) return -1;
	for (new h = 0; h < gHouseCount; h++)
	{
		if (House[h][hDbId] == dbid) return h;
	}
	return -1;
}

// Для doors.inc: мир интерьера дома по id в базе (-1 = дома нет) и дом по миру (-1 = это не дом)
stock House_WorldByDbId(dbid)
{
	new h = House_FindByDbId(dbid);
	return (h == -1) ? -1 : HOUSE_WORLD_OFFSET + h;
}
stock House_DbIdByWorld(world)
{
	new h = world - HOUSE_WORLD_OFFSET;
	if (h < 0 || h >= gHouseCount) return 0;
	return House[h][hDbId];
}
stock House_ByWorld(world)
{
	new h = world - HOUSE_WORLD_OFFSET;
	return (h >= 0 && h < gHouseCount) ? h : -1;
}

// Игрок онлайн с этим аккаунтом (INVALID_PLAYER_ID = нет)
static House_FindOnline(accountId)
{
	if (accountId <= 0) return INVALID_PLAYER_ID;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pDbId] == accountId) return i;
	}
	return INVALID_PLAYER_ID;
}

// Налог в час
stock House_GetTax(h)
{
	new tax = House[h][hPrice] / 1000;
	if (tax < HOUSE_TAX_MIN) tax = HOUSE_TAX_MIN;
	return tax;
}

// Пересоздать иконку и надпись над домом (после покупки/продажи)
stock House_Refresh(h)
{
	new text[MK_TEXT_LEN], model;
	if (House[h][hOwner] == 0)
	{
		model = 1273;   // зелёный домик (продаётся)
		format(text, sizeof text, "{33AA33}Дом №%d продаётся\n{FFFFFF}Цена: ${FFFF00}%d\n{FFFFFF}Налог: $%d в час\nALT — купить",
			House[h][hDbId], House[h][hPrice], House_GetTax(h));
	}
	else if (House[h][hRent] > 0)
	{
		model = 1272;   // синий домик (занят)
		format(text, sizeof text, "{33CCFF}Дом №%d\n{FFFFFF}Владелец: %s\n{FFFF00}Сдаются комнаты: $%d в час (/rent)\n{FFFFFF}ALT — войти",
			House[h][hDbId], House[h][hOwnerName], House[h][hRent]);
	}
	else
	{
		model = 1272;
		format(text, sizeof text, "{33CCFF}Дом №%d\n{FFFFFF}Владелец: %s\nALT — войти",
			House[h][hDbId], House[h][hOwnerName]);
	}

	if (House[h][hMarker] == -1)
	{
		new name[32];
		format(name, sizeof name, "Дом №%d", House[h][hDbId]);
		House[h][hMarker] = Marker_Create(MK_HOUSE, House[h][hDbId], name, model, House[h][hX], House[h][hY], House[h][hZ],
			House[h][hOutWorld], House[h][hOutInt]);
	}
	else Marker_SetDefaultModel(House[h][hMarker], model);
	Marker_SetLabel(House[h][hMarker], text);
}

// Админ перенёс маркер дома (/markers)
stock House_OnMarkerMoved(dbid, Float:x, Float:y, Float:z)
{
	for (new h = 0; h < gHouseCount; h++)
	{
		if (House[h][hDbId] != dbid) continue;
		House[h][hX] = x;
		House[h][hY] = y;
		House[h][hZ] = z;
		return;
	}
}

// Дом рядом с игроком на улице (-1 = нет)
stock House_GetNearby(playerid, Float:range = 2.5)
{
	new world = GetPlayerVirtualWorld(playerid), interior = GetPlayerInterior(playerid);
	for (new h = 0; h < gHouseCount; h++)
	{
		if (House[h][hOutWorld] != world || House[h][hOutInt] != interior) continue;
		if (IsPlayerInRangeOfPoint(playerid, range, House[h][hX], House[h][hY], House[h][hZ])) return h;
	}
	return -1;
}

// Сколько домов у игрока
stock House_CountOwned(playerid)
{
	if (Player[playerid][pDbId] == 0) return 0;
	new n = 0;
	for (new h = 0; h < gHouseCount; h++)
	{
		if (House[h][hOwner] == Player[playerid][pDbId]) n++;
	}
	return n;
}

// Главный дом игрока: у него спавн, он же показывается в /stats (-1 = домов нет).
// Выбирается в /myhouses; пока не выбран (или тот дом продан) — первый из своих
stock House_GetMain(playerid)
{
	if (Player[playerid][pDbId] == 0) return -1;
	new first = -1;
	for (new h = 0; h < gHouseCount; h++)
	{
		if (House[h][hOwner] != Player[playerid][pDbId]) continue;
		if (House[h][hDbId] == gMainHouse[playerid]) return h;
		if (first == -1) first = h;
	}
	return first;
}

// Сделать дом h главным (property.inc, /myhouses)
stock House_SetMain(playerid, h)
{
	gMainHouse[playerid] = House[h][hDbId];
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET main_house = %d WHERE id = %d", gMainHouse[playerid], Player[playerid][pDbId]));
}

// Дом, которым владеет игрок (-1 = нет). Домов может быть несколько, поэтому команды
// (/house, /sellhouse, /housepay, /lock...) работают с тем, где игрок находится:
// сначала дом, внутри которого он стоит, потом дом, у двери которого стоит, иначе главный
stock House_GetOwned(playerid)
{
	if (Player[playerid][pDbId] == 0) return -1;
	new h = Player[playerid][pHouseInside];
	if (h >= 0 && h < gHouseCount && House[h][hOwner] == Player[playerid][pDbId] && House_IsInside(playerid, h)) return h;
	h = House_GetNearby(playerid);
	if (h != -1 && House[h][hOwner] == Player[playerid][pDbId]) return h;
	return House_GetMain(playerid);
}

// Игрок на самом деле стоит внутри дома h. Отметка pHouseInside одна не годится: после больницы,
// тюрьмы или телепорта админом она остаётся, и меню дома (сейф, шкаф) открывалось бы откуда угодно
stock bool:House_IsInside(playerid, h)
{
	if (h == -1 || Player[playerid][pHouseInside] != h) return false;
	return (GetPlayerVirtualWorld(playerid) == HOUSE_WORLD_OFFSET + h);
}

// Дом, который снимает игрок (-1 = нет)
stock House_GetRented(playerid)
{
	return gRentHouse[playerid];
}

// Для спавна у дома (вызывается из accounts.inc): свой дом важнее съёмного
stock Houses_GetSpawn(playerid, &Float:x, &Float:y, &Float:z, &Float:a)
{
	new h = House_GetMain(playerid);
	if (h == -1) h = Family_SpouseHouse(playerid);     // дом супруга (marriage.inc)
	if (h == -1) h = gRentHouse[playerid];
	if (h == -1) h = Family_HouseOf(playerid);         // семейный дом (families.inc)
	if (h == -1) return 0;
	x = House[h][hX];
	if (House[h][hOutInt] == 0) x += 1.0;      // на улице — на шаг от значка; в коридоре рядом может быть стена
	y = House[h][hY];
	z = House[h][hZ];
	a = 0.0;
	return 1;
}

// Из OnPlayerSpawn: дом стоит внутри здания (общежитие) — вернуть игроку интерьер и мир
stock Houses_OnSpawn(playerid)
{
	new h = House_GetMain(playerid);
	if (h == -1) h = Family_SpouseHouse(playerid);
	if (h == -1) h = gRentHouse[playerid];
	if (h == -1) h = Family_HouseOf(playerid);
	if (h == -1 || (House[h][hOutInt] == 0 && House[h][hOutWorld] == 0)) return;
	if (!IsPlayerInRangeOfPoint(playerid, 3.0, House[h][hX], House[h][hY], House[h][hZ])) return;
	SetPlayerInterior(playerid, House[h][hOutInt]);
	SetPlayerVirtualWorld(playerid, House[h][hOutWorld]);
}

// Вывести игрока из дома к его входу
static House_PutOutside(playerid, h)
{
	SetPlayerInterior(playerid, House[h][hOutInt]);
	SetPlayerVirtualWorld(playerid, House[h][hOutWorld]);
	SetPlayerPos(playerid, House[h][hX], House[h][hY], House[h][hZ]);
	SetCameraBehindPlayer(playerid);
	Player[playerid][pHouseInside] = -1;
}

// Всех, кто внутри дома, — к входу (дом сносят или меняют интерьер)
static House_EmptyInside(h)
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && House_IsInside(i, h)) House_PutOutside(i, h);
	}
}

stock House_Save(h)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE houses SET owner = %d, locked = %d, rent = %d, balance = %d, safe = %d WHERE id = %d",
		House[h][hOwner], House[h][hLocked] ? 1 : 0, House[h][hRent], House[h][hBalance], House[h][hSafe], House[h][hDbId]));
}

// После входа в аккаунт (из Vehicles_LoadForPlayer): какой дом снимает
stock Houses_LoadPlayer(playerid)
{
	gRentHouse[playerid] = -1;
	gMainHouse[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT rent_house, main_house FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	new dbid = 0;
	if (DB_GetRowCount(r) > 0)
	{
		dbid = DB_GetFieldInt(r, 0);
		gMainHouse[playerid] = DB_GetFieldInt(r, 1);
	}
	DB_FreeResultSet(r);
	if (dbid == 0) return;

	new h = House_FindByDbId(dbid);
	if (h == -1 || House[h][hOwner] == 0 || House[h][hOwner] == Player[playerid][pDbId])
	{
		House_SetRent(playerid, -1);
		return;
	}
	gRentHouse[playerid] = h;
}

// Записать, какой дом снимает игрок (-1 = съехал)
static House_SetRent(playerid, h)
{
	gRentHouse[playerid] = h;
	new dbid = 0;
	if (h != -1) dbid = House[h][hDbId];
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET rent_house = %d WHERE id = %d", dbid, Player[playerid][pDbId]));
}

// Выселить всех жильцов дома (и онлайн, и оффлайн)
static House_EvictAll(h, const reason[])
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET rent_house = 0 WHERE rent_house = %d", House[h][hDbId]));
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || gRentHouse[i] != h) continue;
		gRentHouse[i] = -1;
		SendClientMessage(i, COLOR_ORANGE, "Ты больше не снимаешь комнату в доме №%d: %s", House[h][hDbId], reason);
		Account_SetNormalSpawn(i);
	}
}

// Отдать оружие из шкафа игроку (при продаже дома)
static House_GiveStoredWeapons(h, playerid)
{
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT weapon, ammo FROM house_weapons WHERE house = %d", House[h][hDbId]);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			GivePlayerWeapon(playerid, WEAPON:DB_GetFieldInt(r, 0), DB_GetFieldInt(r, 1));
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM house_weapons WHERE house = %d", House[h][hDbId]));
}

// Освободить дом (продажа или долг): владельца нет, всё обнулено
static House_Free(h)
{
	House_EvictAll(h, "дом сменил владельца.");
	House[h][hOwner] = 0;
	House[h][hOwnerName][0] = '\0';
	House[h][hLocked] = false;
	House[h][hRent] = 0;
	House[h][hBalance] = 0;
	House[h][hSafe] = 0;
	House_Save(h);
	House_Refresh(h);
}

// ---------------------------------------------------------------------
//  PayDay (из rp.pwn): аренда с жильцов онлайн, потом налог со всех домов
// ---------------------------------------------------------------------
stock Houses_PayDay()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		new h = gRentHouse[i];
		if (h == -1 || !IsPlayerConnected(i) || !Player[i][pLogged]) continue;
		new rent = House[h][hRent];
		if (GetMoney(i) < rent)
		{
			House_SetRent(i, -1);
			Account_SetNormalSpawn(i);
			SendClientMessage(i, COLOR_RED, "Нечем платить за аренду ($%d). Тебя выселили из дома №%d.", rent, House[h][hDbId]);
			continue;
		}
		GiveMoney(i, -rent, "аренда комнаты");
		House[h][hBalance] += rent;
		Account_Save(i);
		SendClientMessage(i, COLOR_WHITE, "Аренда комнаты в доме №%d: -$%d.", House[h][hDbId], rent);
	}

	for (new h = 0; h < gHouseCount; h++)
	{
		if (House[h][hOwner] == 0) continue;
		new tax = House_GetTax(h);
		House[h][hBalance] -= tax;
		Econ_ToTreasury(tax);           // налог в казну (economy.inc)
		new owner = House_FindOnline(House[h][hOwner]);

		if (House[h][hBalance] < -tax * HOUSE_DEBT_HOURS)
		{
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM house_weapons WHERE house = %d", House[h][hDbId]));
			if (owner != INVALID_PLAYER_ID)
				SendClientMessage(owner, COLOR_RED, "Долг по налогу на дом №%d слишком большой. Дом забрало государство.", House[h][hDbId]);
			printf("[Houses] House %d seized for tax debt (owner %d).", House[h][hDbId], House[h][hOwner]);
			House_Free(h);
			if (owner != INVALID_PLAYER_ID) Account_SetNormalSpawn(owner);
			continue;
		}
		House_Save(h);

		if (owner == INVALID_PLAYER_ID) continue;
		if (House[h][hBalance] < 0)
			SendClientMessage(owner, COLOR_ORANGE, "Налог на дом: -$%d. Долг $%d. Оплати: /housepay [сумма], иначе через %d ч. дом заберут.",
				tax, -House[h][hBalance], (House[h][hBalance] + tax * HOUSE_DEBT_HOURS) / tax);
		else
			SendClientMessage(owner, COLOR_WHITE, "Налог на дом: -$%d. На счёте дома $%d.", tax, House[h][hBalance]);
	}
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Cmd_BuyHouse(playerid)
{
	if (Debts_BlockBuy(playerid)) return true;      // большой долг штату (debts.inc)
	new h = House_GetNearby(playerid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "Рядом нет дома. Подойди к зелёному домику.");
	if (House[h][hOwner] != 0) return SendClientMessage(playerid, COLOR_GREY, "Этот дом уже куплен.");
	if (House_CountOwned(playerid) >= Props_MaxHouses(playerid))     // лимит домов (property.inc)
		return SendClientMessage(playerid, COLOR_GREY, "У тебя уже %d дом(а) — это твой лимит. Продай один (/sellhouse) или повысь VIP. Список: /myhouses", House_CountOwned(playerid));
	if (GetMoney(playerid) < House[h][hPrice]) return SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", House[h][hPrice]);

	GiveMoney(playerid, -House[h][hPrice], "покупка дома");
	if (gRentHouse[playerid] != -1) House_SetRent(playerid, -1);   // съезжает со съёмной комнаты
	House[h][hOwner] = Player[playerid][pDbId];
	House[h][hLocked] = false;
	House[h][hRent] = 0;
	House[h][hBalance] = 0;
	House[h][hSafe] = 0;
	GetPlayerName(playerid, House[h][hOwnerName], MAX_PLAYER_NAME);
	House_Save(h);
	House_Refresh(h);
	Account_Save(playerid);
	Account_SetNormalSpawn(playerid);   // теперь спавн у дома

	SendClientMessage(playerid, COLOR_GREEN, "Поздравляем! Ты купил дом №%d за $%d. Войти: /enter, замок: /lock, меню: /house", House[h][hDbId], House[h][hPrice]);
	SendClientMessage(playerid, COLOR_WHITE, "Налог $%d в час списывается со счёта дома. Пополнить: /housepay [сумма]", House_GetTax(h));
	if (House_CountOwned(playerid) > 1)
		SendClientMessage(playerid, COLOR_WHITE, "Домов у тебя теперь %d из %d. Список и выбор дома для спавна: /myhouses", House_CountOwned(playerid), Props_MaxHouses(playerid));
	return true;
}

stock bool:Cmd_SellHouse(playerid)
{
	new h = House_GetOwned(playerid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет дома.");
	if (House_GetNearby(playerid) != h) return SendClientMessage(playerid, COLOR_GREY, "Подойди к своему дому.");

	// половина цены + сейф + остаток на счёте (или минус долг по налогу)
	new refund = House[h][hPrice] / 2 + House[h][hSafe] + House[h][hBalance];
	if (refund < 0) refund = 0;
	House_GiveStoredWeapons(h, playerid);
	GiveMoney(playerid, refund, "продажа дома");
	House_Free(h);
	Account_Save(playerid);
	Account_SetNormalSpawn(playerid);

	SendClientMessage(playerid, COLOR_GREEN, "Ты продал дом государству. Получено $%d (половина цены, сейф и счёт дома). Оружие из шкафа у тебя.", refund);
	return true;
}

stock bool:Cmd_Enter(playerid)
{
	if (Doors_Try(playerid)) return true;     // здания с интерьером (doors.inc)
	if (Biz_TryDoor(playerid)) return true;   // интерьер бизнеса
	new h = House_GetNearby(playerid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "Рядом нет входа.");
	if (House[h][hOwner] == 0) return SendClientMessage(playerid, COLOR_GREY, "Дом продаётся и закрыт. Купить: /buyhouse");
	if (House[h][hLocked] && House[h][hOwner] != Player[playerid][pDbId] && gRentHouse[playerid] != h && !Family_IsSpouseOf(playerid, House[h][hOwner]) && !Family_IsHouseMember(playerid, h))
	{
		if (House[h][hRent] > 0)
			return SendClientMessage(playerid, COLOR_GREY, "Дверь заперта. Здесь сдаются комнаты: /rent ($%d в час).", House[h][hRent]);
		return SendClientMessage(playerid, COLOR_GREY, "Дверь заперта.");
	}

	SetPlayerInterior(playerid, House[h][hInterior]);
	SetPlayerVirtualWorld(playerid, HOUSE_WORLD_OFFSET + h);
	SetPlayerPos(playerid, House[h][hIX], House[h][hIY], House[h][hIZ]);
	SetCameraBehindPlayer(playerid);
	Player[playerid][pHouseInside] = h;
	if (House[h][hOwner] == Player[playerid][pDbId])
		SendClientMessage(playerid, COLOR_WHITE, "Выйти: /exit у двери. Шкаф и сейф: ALT у шкафа (/makestorage). Налог и аренда: /house");
	else
		SendClientMessage(playerid, COLOR_WHITE, "Выйти из дома: /exit (стоя у двери)");
	return true;
}

stock bool:Cmd_Exit(playerid)
{
	if (Doors_Try(playerid)) return true;     // здания с интерьером (doors.inc)
	if (Biz_TryDoor(playerid)) return true;   // интерьер бизнеса
	new h = Player[playerid][pHouseInside];
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "Ты не в доме.");
	if (!House_IsInside(playerid, h))
	{
		Player[playerid][pHouseInside] = -1;    // отметка осталась после телепорта или больницы
		return SendClientMessage(playerid, COLOR_GREY, "Ты не в доме.");
	}
	if (!IsPlayerInRangeOfPoint(playerid, 3.0, House[h][hIX], House[h][hIY], House[h][hIZ]))
		return SendClientMessage(playerid, COLOR_GREY, "Подойди к двери (туда, где ты появился).");

	House_PutOutside(playerid, h);
	return true;
}

stock bool:Cmd_Lock(playerid)
{
	// Сначала пробуем дом, потом машину (vehicles.inc)
	new h = House_GetOwned(playerid);
	if (h == -1) h = Family_SpouseHouse(playerid);     // общий дом супругов (marriage.inc)
	if (h == -1 || (House_GetNearby(playerid) != h && !House_IsInside(playerid, h))) { new fh = Family_HouseOf(playerid); if (fh != -1) h = fh; }   // семейный дом
	if (h != -1 && (House_GetNearby(playerid) == h || House_IsInside(playerid, h)))
	{
		House[h][hLocked] = !House[h][hLocked];
		House_Save(h);
		if (House[h][hLocked]) SendClientMessage(playerid, COLOR_WHITE, "Дом {FF6347}закрыт{FFFFFF}.");
		else SendClientMessage(playerid, COLOR_WHITE, "Дом {33AA33}открыт{FFFFFF}.");
		return true;
	}
	return Vehicles_ToggleLock(playerid);
}

// ---------------------------------------------------------------------
//  Налог и аренда
// ---------------------------------------------------------------------
static bool:House_Deposit(playerid, h, amount)
{
	if (amount < 1 || amount > 1000000) return SendClientMessage(playerid, COLOR_GREY, "Сумма от $1 до $1000000.");
	if (GetMoney(playerid) < amount) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет столько денег.");
	GiveMoney(playerid, -amount, "на счёт дома");
	House[h][hBalance] += amount;
	House_Save(h);
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Счёт дома пополнен на $%d. Теперь на счёте $%d (хватит примерно на %d ч.).",
		amount, House[h][hBalance], House[h][hBalance] > 0 ? House[h][hBalance] / House_GetTax(h) : 0);
	return true;
}

static bool:House_SetRentPrice(playerid, h, price)
{
	if (price < 0 || price > HOUSE_RENT_MAX) return SendClientMessage(playerid, COLOR_GREY, "Аренда от $0 (не сдавать) до $%d в час.", HOUSE_RENT_MAX);
	House[h][hRent] = price;
	House_Save(h);
	House_Refresh(h);
	if (price == 0)
	{
		House_EvictAll(h, "владелец больше не сдаёт комнаты.");
		SendClientMessage(playerid, COLOR_WHITE, "Комнаты больше не сдаются, жильцы выселены.");
	}
	else SendClientMessage(playerid, COLOR_GREEN, "Комнаты сдаются за $%d в час. Плата идёт на счёт дома.", price);
	return true;
}

stock bool:Cmd_HousePay(playerid, const params[])
{
	new h = House_GetOwned(playerid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет дома.");
	new pos = 0, amount;
	if (!NextIntParam(params, pos, amount) || amount < 1)
	{
		SendClientMessage(playerid, COLOR_WHITE, "Счёт дома №%d: $%d. Налог $%d в час.", House[h][hDbId], House[h][hBalance], House_GetTax(h));
		return SendClientMessage(playerid, COLOR_GREY, "Пополнить: /housepay [сумма]");
	}
	return House_Deposit(playerid, h, amount);
}

stock bool:Cmd_SetRent(playerid, const params[])
{
	new h = House_GetOwned(playerid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет дома.");
	new pos = 0, price;
	if (!NextIntParam(params, pos, price)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /setrent [сумма в час] (0 — не сдавать)");
	return House_SetRentPrice(playerid, h, price);
}

stock bool:Cmd_Rent(playerid)
{
	new h = House_GetNearby(playerid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "Подойди к двери дома, где сдаются комнаты.");
	if (House[h][hOwner] == 0 || House[h][hRent] <= 0) return SendClientMessage(playerid, COLOR_GREY, "В этом доме комнаты не сдаются.");
	if (House[h][hOwner] == Player[playerid][pDbId]) return SendClientMessage(playerid, COLOR_GREY, "Это твой дом.");
	if (House_GetOwned(playerid) != -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя есть свой дом.");
	if (gRentHouse[playerid] == h) return SendClientMessage(playerid, COLOR_GREY, "Ты уже снимаешь здесь комнату.");
	if (GetMoney(playerid) < House[h][hRent]) return SendClientMessage(playerid, COLOR_GREY, "Нужно $%d за первый час.", House[h][hRent]);

	GiveMoney(playerid, -House[h][hRent], "аренда комнаты");
	House[h][hBalance] += House[h][hRent];
	House_Save(h);
	House_SetRent(playerid, h);
	Account_Save(playerid);
	Account_SetNormalSpawn(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Ты снял комнату в доме №%d за $%d в час (оплата каждый PayDay). Съехать: /unrent", House[h][hDbId], House[h][hRent]);

	new owner = House_FindOnline(House[h][hOwner]);
	if (owner != INVALID_PLAYER_ID) SendClientMessage(owner, COLOR_BLUE, "%s снял комнату в твоём доме.", GetRPName(playerid));
	return true;
}

stock bool:Cmd_Unrent(playerid)
{
	new h = gRentHouse[playerid];
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "Ты не снимаешь комнату.");
	House_SetRent(playerid, -1);
	Account_SetNormalSpawn(playerid);
	SendClientMessage(playerid, COLOR_WHITE, "Ты съехал из дома №%d.", House[h][hDbId]);
	return true;
}

stock bool:Cmd_Tenants(playerid)
{
	new h = House_GetOwned(playerid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет дома.");
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT name FROM accounts WHERE rent_house = %d ORDER BY name LIMIT 20", House[h][hDbId]);
	if (r == DBResult:0) return true;
	new rows = DB_GetRowCount(r);
	if (rows == 0) SendClientMessage(playerid, COLOR_WHITE, "Жильцов нет. Аренда: $%d в час (/setrent).", House[h][hRent]);
	else
	{
		SendClientMessage(playerid, COLOR_YELLOW, "Жильцы дома №%d (аренда $%d в час):", House[h][hDbId], House[h][hRent]);
		new name[MAX_PLAYER_NAME];
		for (new i = 0; i < rows; i++)
		{
			DB_GetFieldString(r, 0, name, sizeof name);
			SendClientMessage(playerid, COLOR_WHITE, " %s", name);
			DB_SelectNextRow(r);
		}
		SendClientMessage(playerid, COLOR_GREY, "Выселить: /evict [id] (онлайн) или /evictall (всех).");
	}
	DB_FreeResultSet(r);
	return true;
}

stock bool:Cmd_Evict(playerid, const params[])
{
	new h = House_GetOwned(playerid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет дома.");
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /evict [id]   Всех сразу: /evictall");
	if (gRentHouse[target] != h) return SendClientMessage(playerid, COLOR_GREY, "Этот игрок не живёт в твоём доме.");
	House_SetRent(target, -1);
	Account_SetNormalSpawn(target);
	SendClientMessage(target, COLOR_ORANGE, "Владелец выселил тебя из дома №%d.", House[h][hDbId]);
	SendClientMessage(playerid, COLOR_WHITE, "%s выселен.", GetRPName(target));
	return true;
}

stock bool:Cmd_EvictAll(playerid)
{
	new h = House_GetOwned(playerid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет дома.");
	House_EvictAll(h, "владелец выселил всех жильцов.");
	SendClientMessage(playerid, COLOR_WHITE, "Все жильцы выселены.");
	return true;
}

// ---------------------------------------------------------------------
//  Меню дома: шкаф, сейф, счёт, аренда
// ---------------------------------------------------------------------
stock bool:Cmd_House(playerid)
{
	new h = House_GetOwned(playerid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет дома.");
	if (!House_IsInside(playerid, h)) return SendClientMessage(playerid, COLOR_GREY, "Меню дома открывается внутри твоего дома.");

	ShowPlayerDialog(playerid, DIALOG_HOUSE_MENU, DIALOG_STYLE_LIST, "Дом",
		"Шкаф: положить оружие из рук\nШкаф: взять оружие\nСейф: положить деньги (сейчас $%d)\nСейф: взять деньги\nСчёт дома: пополнить (сейчас $%d, налог $%d/час)\nСчёт дома: снять деньги\nАренда: изменить цену (сейчас $%d/час)\nЖильцы",
		"Выбрать", "Закрыть", House[h][hSafe], House[h][hBalance], House_GetTax(h), House[h][hRent]);
	return true;
}

// ALT внутри дома (keys.inc): меню открывается только у шкафа. 1 — сработало
stock House_TryMenu(playerid)
{
	new h = Player[playerid][pHouseInside];
	if (!House_IsInside(playerid, h) || House[h][hOwner] != Player[playerid][pDbId]) return 0;
	if (!House[h][hHasStore])
	{
		SendClientMessage(playerid, COLOR_GREY, "Шкафа в доме ещё нет. Встань, где он должен быть, и введи /makestorage.");
		return 1;
	}
	if (!House_AtStore(playerid, h)) return 0;
	Cmd_House(playerid);
	return 1;
}

// Игрок стоит у шкафа своего дома h
stock bool:House_AtStore(playerid, h)
{
	if (!House[h][hHasStore]) return false;
	return IsPlayerInRangeOfPoint(playerid, HOUSE_STORE_RANGE, House[h][hSX], House[h][hSY], House[h][hSZ]);
}

// Надпись над шкафом (пересоздаётся, когда шкаф переставили или дом сменил мир)
stock House_BuildStore(h)
{
	if (House[h][hStoreLabel] != Text3D:INVALID_3DTEXT_ID)
	{
		Delete3DTextLabel(House[h][hStoreLabel]);
		House[h][hStoreLabel] = Text3D:INVALID_3DTEXT_ID;
	}
	if (!House[h][hHasStore]) return;
	House[h][hStoreLabel] = Create3DTextLabel("{FFFF66}Шкаф\n{FFFFFF}ALT — шкаф и сейф", COLOR_WHITE,
		House[h][hSX], House[h][hSY], House[h][hSZ] + 0.3, 6.0, HOUSE_WORLD_OFFSET + h, true);
}

// /makestorage — поставить шкаф (с сейфом) там, где стоишь. Владелец — в своём доме, админ 4+ — в любом
stock bool:Cmd_MakeStorage(playerid)
{
	new h = Player[playerid][pHouseInside];
	if (!House_IsInside(playerid, h)) return SendClientMessage(playerid, COLOR_GREY, "Встань внутри дома там, где должен быть шкаф.");
	if (House[h][hOwner] != Player[playerid][pDbId] && Player[playerid][pAdmin] < 4)
		return SendClientMessage(playerid, COLOR_GREY, "Шкаф ставит только владелец дома.");
	GetPlayerPos(playerid, House[h][hSX], House[h][hSY], House[h][hSZ]);
	House[h][hHasStore] = true;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE houses SET has_st = 1, stx = %f, sty = %f, stz = %f WHERE id = %d",
		House[h][hSX], House[h][hSY], House[h][hSZ], House[h][hDbId]));
	House_BuildStore(h);
	SendClientMessage(playerid, COLOR_GREEN, "Шкаф поставлен здесь. Шкаф и сейф открываются ALT рядом с ним, остальное меню дома: /house.");
	return true;
}

// Шкаф и сейф выбраны в /house не у шкафа
stock House_StoreFar(playerid)
{
	return SendClientMessage(playerid, COLOR_GREY, "Шкаф и сейф открываются только у шкафа (надпись «Шкаф»). Нет шкафа: /makestorage.");
}

static House_PutGun(playerid, h)
{
	if (Player[playerid][pDuty])
	{
		SendClientMessage(playerid, COLOR_GREY, "Служебное оружие в шкаф класть нельзя. Сначала сними форму.");
		return 0;
	}
	new WEAPON:weapon = GetPlayerWeapon(playerid);
	new ammo = GetPlayerAmmo(playerid);
	if (weapon == WEAPON:0 || ammo <= 0)
	{
		SendClientMessage(playerid, COLOR_GREY, "Возьми оружие в руки (колесо мыши), потом выбери этот пункт.");
		return 0;
	}

	new used[HOUSE_WEAPON_SLOTS];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT slot FROM house_weapons WHERE house = %d", House[h][hDbId]);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new s = DB_GetFieldInt(r, 0);
			if (s >= 0 && s < HOUSE_WEAPON_SLOTS) used[s] = 1;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	new slot = -1;
	for (new s = 0; s < HOUSE_WEAPON_SLOTS; s++)
	{
		if (!used[s]) { slot = s; break; }
	}
	if (slot == -1)
	{
		SendClientMessage(playerid, COLOR_GREY, "Шкаф полный (%d мест).", HOUSE_WEAPON_SLOTS);
		return 0;
	}

	RemovePlayerWeapon(playerid, weapon);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO house_weapons (house, slot, weapon, ammo) VALUES (%d, %d, %d, %d)",
		House[h][hDbId], slot, _:weapon, ammo));
	new name[32];
	GetWeaponName(weapon, name, sizeof name);
	SendClientMessage(playerid, COLOR_GREEN, "%s (%d патр.) убран в шкаф.", name, ammo);
	return 1;
}

static House_ShowGuns(playerid, h)
{
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT slot, weapon, ammo FROM house_weapons WHERE house = %d ORDER BY slot", House[h][hDbId]);
	if (r == DBResult:0) return 0;
	new rows = DB_GetRowCount(r);
	if (rows == 0)
	{
		DB_FreeResultSet(r);
		SendClientMessage(playerid, COLOR_GREY, "В шкафу пусто.");
		return 0;
	}
	new list[256], line[48], name[32];
	for (new i = 0; i < rows && i < HOUSE_WEAPON_SLOTS; i++)
	{
		gHouseGunSlot[playerid][i] = DB_GetFieldInt(r, 0);
		GetWeaponName(WEAPON:DB_GetFieldInt(r, 1), name, sizeof name);
		format(line, sizeof line, "%s (%d патр.)\n", name, DB_GetFieldInt(r, 2));
		strcat(list, line);
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	ShowPlayerDialog(playerid, DIALOG_HOUSE_TAKEGUN, DIALOG_STYLE_LIST, "Шкаф", "%s", "Взять", "Назад", list);
	return 1;
}

static House_TakeGun(playerid, h, listitem)
{
	if (listitem < 0 || listitem >= HOUSE_WEAPON_SLOTS) return 0;
	new slot = gHouseGunSlot[playerid][listitem];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT weapon, ammo FROM house_weapons WHERE house = %d AND slot = %d", House[h][hDbId], slot);
	if (r == DBResult:0) return 0;
	if (DB_GetRowCount(r) == 0)
	{
		DB_FreeResultSet(r);
		return 0;
	}
	new WEAPON:weapon = WEAPON:DB_GetFieldInt(r, 0), ammo = DB_GetFieldInt(r, 1);
	DB_FreeResultSet(r);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM house_weapons WHERE house = %d AND slot = %d", House[h][hDbId], slot));
	GivePlayerWeapon(playerid, weapon, ammo);
	new name[32];
	GetWeaponName(weapon, name, sizeof name);
	SendClientMessage(playerid, COLOR_GREEN, "Ты взял из шкафа %s (%d патр.).", name, ammo);
	return 1;
}

// Ответы на окна дома (из OnDialogResponse)
stock Houses_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid < DIALOG_HOUSE_MENU || dialogid > DIALOG_HOUSE_RENT) return 0;
	new h = House_GetOwned(playerid);
	if (!House_IsInside(playerid, h)) return 1;

	if (dialogid == DIALOG_HOUSE_MENU)
	{
		if (!response) return 1;
		switch (listitem)
		{
			case 0: { if (House_AtStore(playerid, h)) House_PutGun(playerid, h); else House_StoreFar(playerid); }
			case 1: { if (House_AtStore(playerid, h)) House_ShowGuns(playerid, h); else House_StoreFar(playerid); }
			case 2: if (!House_AtStore(playerid, h)) House_StoreFar(playerid); else ShowPlayerDialog(playerid, DIALOG_HOUSE_SAFE_IN, DIALOG_STYLE_INPUT, "Сейф", "{FFFFFF}Сколько положить в сейф?\nВ сейфе: $%d, у тебя: $%d", "Положить", "Назад", House[h][hSafe], GetMoney(playerid));
			case 3: if (!House_AtStore(playerid, h)) House_StoreFar(playerid); else ShowPlayerDialog(playerid, DIALOG_HOUSE_SAFE_OUT, DIALOG_STYLE_INPUT, "Сейф", "{FFFFFF}Сколько взять из сейфа?\nВ сейфе: $%d", "Взять", "Назад", House[h][hSafe]);
			case 4: ShowPlayerDialog(playerid, DIALOG_HOUSE_PAY, DIALOG_STYLE_INPUT, "Счёт дома", "{FFFFFF}С этого счёта платится налог $%d в час, сюда же приходит аренда.\nНа счёте: $%d\n\nСколько положить?", "Пополнить", "Назад", House_GetTax(h), House[h][hBalance]);
			case 5: ShowPlayerDialog(playerid, DIALOG_HOUSE_WITHDRAW, DIALOG_STYLE_INPUT, "Счёт дома", "{FFFFFF}На счёте: $%d\n\nСколько снять?", "Снять", "Назад", House[h][hBalance]);
			case 6: ShowPlayerDialog(playerid, DIALOG_HOUSE_RENT, DIALOG_STYLE_INPUT, "Аренда", "{FFFFFF}Цена комнаты в час (0 — не сдавать, максимум $%d).\nСейчас: $%d", "Готово", "Назад", HOUSE_RENT_MAX, House[h][hRent]);
			case 7: Cmd_Tenants(playerid);
		}
		return 1;
	}

	if (!response)
	{
		Cmd_House(playerid);
		return 1;
	}
	if (dialogid == DIALOG_HOUSE_TAKEGUN)
	{
		House_TakeGun(playerid, h, listitem);
		return 1;
	}

	new amount = strval(inputtext);
	switch (dialogid)
	{
		case DIALOG_HOUSE_SAFE_IN:
		{
			if (amount < 1 || GetMoney(playerid) < amount) SendClientMessage(playerid, COLOR_GREY, "Неверная сумма.");
			else if (House[h][hSafe] + amount > HOUSE_SAFE_MAX) SendClientMessage(playerid, COLOR_GREY, "В сейф помещается не больше $%d.", HOUSE_SAFE_MAX);
			else
			{
				GiveMoney(playerid, -amount, "в сейф дома");
				House[h][hSafe] += amount;
				House_Save(h);
				Account_Save(playerid);
				SendClientMessage(playerid, COLOR_GREEN, "Ты положил в сейф $%d. В сейфе $%d.", amount, House[h][hSafe]);
			}
		}
		case DIALOG_HOUSE_SAFE_OUT:
		{
			if (amount < 1 || amount > House[h][hSafe]) SendClientMessage(playerid, COLOR_GREY, "Неверная сумма.");
			else
			{
				House[h][hSafe] -= amount;
				GiveMoney(playerid, amount, "из сейфа дома");
				House_Save(h);
				Account_Save(playerid);
				SendClientMessage(playerid, COLOR_GREEN, "Ты взял из сейфа $%d. Осталось $%d.", amount, House[h][hSafe]);
			}
		}
		case DIALOG_HOUSE_PAY: House_Deposit(playerid, h, amount);
		case DIALOG_HOUSE_WITHDRAW:
		{
			if (amount < 1 || amount > House[h][hBalance]) SendClientMessage(playerid, COLOR_GREY, "Неверная сумма (на счёте $%d).", House[h][hBalance]);
			else
			{
				House[h][hBalance] -= amount;
				GiveMoney(playerid, amount, "со счёта дома");
				House_Save(h);
				Account_Save(playerid);
				SendClientMessage(playerid, COLOR_GREEN, "Ты снял со счёта дома $%d. Осталось $%d.", amount, House[h][hBalance]);
			}
		}
		case DIALOG_HOUSE_RENT: House_SetRentPrice(playerid, h, amount);
	}
	return 1;
}

stock bool:Cmd_CreateHouse(playerid, const params[])
{
	new pos = 0, price, type;
	if (!NextIntParam(params, pos, price) || !NextIntParam(params, pos, type) || type < 1 || type > sizeof (gHouseIntId) || price < 1)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /createhouse [цена] [тип 1-5]");
	if (gHouseCount >= MAX_HOUSES) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит домов (MAX_HOUSES).");
	// вход можно ставить и внутри здания (коридор общежития), но не внутри другого дома или бизнеса
	new oint = GetPlayerInterior(playerid), oworld = GetPlayerVirtualWorld(playerid);
	if (oworld >= HOUSE_WORLD_OFFSET) return SendClientMessage(playerid, COLOR_GREY, "Здесь нельзя: ты внутри дома или бизнеса.");

	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	Houses_InsertDefault(x, y, z, price, type);

	new h = gHouseCount++;
	new t = type - 1;
	House[h][hDbId] = DB_LastInsertId();
	House[h][hOutInt] = oint;
	House[h][hOutWorld] = oworld;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE houses SET oint = %d, oworld = %d WHERE id = %d", oint, oworld, House[h][hDbId]));
	House[h][hOwner] = 0;
	House[h][hOwnerName][0] = '\0';
	House[h][hPrice] = price;
	House[h][hX] = x;
	House[h][hY] = y;
	House[h][hZ] = z;
	House[h][hIX] = gHouseIntPos[t][0];
	House[h][hIY] = gHouseIntPos[t][1];
	House[h][hIZ] = gHouseIntPos[t][2];
	House[h][hInterior] = gHouseIntId[t];
	House[h][hLocked] = false;
	House[h][hRent] = 0;
	House[h][hBalance] = 0;
	House[h][hSafe] = 0;
	House[h][hHasStore] = false;
	House[h][hStoreLabel] = Text3D:INVALID_3DTEXT_ID;
	House[h][hMarker] = -1;
	House_Refresh(h);

	SendClientMessage(playerid, COLOR_GREEN, "Дом №%d создан. Цена $%d, интерьер %d. Свой интерьер: /sethouseint %d, снести: /delhouse %d", House[h][hDbId], price, type, House[h][hDbId], House[h][hDbId]);
	return true;
}

// Дом по номеру из команды; без номера — тот, у входа или внутри которого стоит админ (-1 = нет)
static House_FromParam(playerid, const params[])
{
	new pos = 0, dbid;
	if (NextIntParam(params, pos, dbid)) return House_FindByDbId(dbid);
	new h = House_GetNearby(playerid);
	if (h == -1 && House_IsInside(playerid, Player[playerid][pHouseInside])) h = Player[playerid][pHouseInside];
	return h;
}

// /delhouse [№] — снести дом (уровень админа проверяется в commands.inc)
stock bool:Cmd_DelHouse(playerid, const params[])
{
	new h = House_FromParam(playerid, params);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "Использование: /delhouse [№ дома] (без номера — дом, у входа которого ты стоишь)");
	new dbid = House[h][hDbId];

	// владельцу возвращается полная цена, сейф и остаток счёта
	if (House[h][hOwner] != 0)
	{
		new refund = House[h][hPrice] + House[h][hSafe];
		if (House[h][hBalance] > 0) refund += House[h][hBalance];
		new owner = House_FindOnline(House[h][hOwner]);
		if (owner != INVALID_PLAYER_ID)
		{
			House_GiveStoredWeapons(h, owner);
			GiveMoney(owner, refund, "дом снесён администрацией");
			Account_Save(owner);
			SendClientMessage(owner, COLOR_ORANGE, "Администрация снесла твой дом №%d. Возвращено $%d, оружие из шкафа у тебя.", dbid, refund);
		}
		else DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET bank = bank + %d WHERE id = %d", refund, House[h][hOwner]));
		SendClientMessage(playerid, COLOR_ADMIN, "Владельцу (%s) возвращено $%d.", House[h][hOwnerName], refund);
	}
	new ownerId = House[h][hOwner];

	House_EmptyInside(h);
	House_EvictAll(h, "дом снесён.");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM house_weapons WHERE house = %d", dbid));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM houses WHERE id = %d", dbid));
	Marker_Destroy(House[h][hMarker], true);
	House[h][hHasStore] = false;
	House_BuildStore(h);    // убрать надпись шкафа

	// последний дом переезжает на освободившееся место: мир его интерьера меняется
	new last = gHouseCount - 1;
	if (h != last)
	{
		House_EmptyInside(last);
		House[h] = House[last];
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (gRentHouse[i] == last) gRentHouse[i] = h;
		}
		House_BuildStore(h);    // надпись шкафа — в новом мире интерьера
	}
	gHouseCount--;
	PDoors_Rebuild();       // входы из /adddoor внутри домов тоже переезжают в новый мир

	new owner = House_FindOnline(ownerId);
	if (owner != INVALID_PLAYER_ID) Account_SetNormalSpawn(owner);
	SendClientMessage(playerid, COLOR_ADMIN, "Дом №%d снесён.", dbid);
	return true;
}

// /sethouseint [№] — точка внутри дома там, где стоит админ (в интерьере)
stock bool:Cmd_SetHouseInt(playerid, const params[])
{
	new pos = 0, dbid;
	if (!NextIntParam(params, pos, dbid))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /sethouseint [№ дома] — стоя в нужном интерьере у двери (попасть: /gotoint или /gotocoord)");
	new h = House_FindByDbId(dbid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "Дома с таким номером нет.");
	new interior = GetPlayerInterior(playerid);
	if (interior == 0) return SendClientMessage(playerid, COLOR_GREY, "Встань внутри интерьера у двери.");

	House_EmptyInside(h);
	GetPlayerPos(playerid, House[h][hIX], House[h][hIY], House[h][hIZ]);
	House[h][hInterior] = interior;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE houses SET ix = %f, iy = %f, iz = %f, interior = %d WHERE id = %d",
		House[h][hIX], House[h][hIY], House[h][hIZ], interior, dbid));
	SendClientMessage(playerid, COLOR_ADMIN, "Дом №%d: вход теперь ведёт сюда (интерьер %d).", dbid, interior);
	return true;
}

// /gotohouse [№] — телепорт ко входу дома
stock bool:Cmd_GotoHouse(playerid, const params[])
{
	new pos = 0, dbid;
	if (!NextIntParam(params, pos, dbid)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /gotohouse [№ дома]");
	new h = House_FindByDbId(dbid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "Дома с таким номером нет.");
	if (IsPlayerInAnyVehicle(playerid)) RemovePlayerFromVehicle(playerid);
	House_PutOutside(playerid, h);
	SendClientMessage(playerid, COLOR_ADMIN, "Ты у дома №%d.", dbid);
	return true;
}
