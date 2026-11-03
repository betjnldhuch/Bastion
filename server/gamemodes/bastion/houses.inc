// =====================================================================
//  houses.inc — дома: покупка, продажа, вход/выход, замок.
//
//  Дома хранятся в таблице houses. При первом запуске создаются
//  несколько домов на Grove Street. Админ (4+) может добавить дом
//  там, где стоит: /createhouse [цена] [тип интерьера 1-5]
// =====================================================================

#define MAX_HOUSES          200
#define HOUSE_WORLD_OFFSET  1000    // вирт. мир интерьера = 1000 + номер дома

enum E_HOUSE
{
	hDbId,
	hOwner,                     // id аккаунта владельца (0 = продаётся)
	hOwnerName[MAX_PLAYER_NAME],
	hPrice,
	Float:hX, Float:hY, Float:hZ,       // вход с улицы
	Float:hIX, Float:hIY, Float:hIZ,    // точка внутри
	hInterior,
	bool:hLocked,
	hPickup,
	Text3D:hLabel
}
new House[MAX_HOUSES][E_HOUSE];
new gHouseCount = 0;

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
stock Houses_Load()
{
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

	r = DB_ExecuteQuery(gDB, "SELECT h.id, h.owner, h.price, h.x, h.y, h.z, h.ix, h.iy, h.iz, h.interior, h.locked, IFNULL(a.name, '') FROM houses h LEFT JOIN accounts a ON a.id = h.owner ORDER BY h.id");
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
			House[h][hPickup] = INVALID_PICKUP;
			House[h][hLabel]  = INVALID_3DTEXT_ID;
			House_Refresh(h);
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

// Пересоздать иконку и надпись над домом (после покупки/продажи)
stock House_Refresh(h)
{
	if (House[h][hPickup] != INVALID_PICKUP) DestroyPickup(House[h][hPickup]);
	if (House[h][hLabel] != INVALID_3DTEXT_ID) Delete3DTextLabel(House[h][hLabel]);

	if (House[h][hOwner] == 0)
	{
		// 1273 — зелёный домик (продаётся)
		House[h][hPickup] = CreatePickup(1273, 1, House[h][hX], House[h][hY], House[h][hZ], 0);
		House[h][hLabel] = Create3DTextLabel("{33AA33}Дом №%d продаётся\n{FFFFFF}Цена: ${FFFF00}%d\n{FFFFFF}/buyhouse — купить",
			COLOR_WHITE, House[h][hX], House[h][hY], House[h][hZ] + 0.6, 15.0, 0, true,
			House[h][hDbId], House[h][hPrice]);
	}
	else
	{
		// 1272 — синий домик (занят)
		House[h][hPickup] = CreatePickup(1272, 1, House[h][hX], House[h][hY], House[h][hZ], 0);
		House[h][hLabel] = Create3DTextLabel("{33CCFF}Дом №%d\n{FFFFFF}Владелец: %s\n/enter — войти",
			COLOR_WHITE, House[h][hX], House[h][hY], House[h][hZ] + 0.6, 15.0, 0, true,
			House[h][hDbId], House[h][hOwnerName]);
	}
}

// Дом рядом с игроком на улице (-1 = нет)
stock House_GetNearby(playerid, Float:range = 2.5)
{
	if (GetPlayerVirtualWorld(playerid) != 0) return -1;
	for (new h = 0; h < gHouseCount; h++)
	{
		if (IsPlayerInRangeOfPoint(playerid, range, House[h][hX], House[h][hY], House[h][hZ])) return h;
	}
	return -1;
}

// Дом, которым владеет игрок (-1 = нет)
stock House_GetOwned(playerid)
{
	if (Player[playerid][pDbId] == 0) return -1;
	for (new h = 0; h < gHouseCount; h++)
	{
		if (House[h][hOwner] == Player[playerid][pDbId]) return h;
	}
	return -1;
}

// Для спавна у дома (вызывается из accounts.inc)
stock Houses_GetSpawn(playerid, &Float:x, &Float:y, &Float:z, &Float:a)
{
	new h = House_GetOwned(playerid);
	if (h == -1) return 0;
	x = House[h][hX] + 1.0;
	y = House[h][hY];
	z = House[h][hZ];
	a = 0.0;
	return 1;
}

stock House_Save(h)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE houses SET owner = %d, locked = %d WHERE id = %d",
		House[h][hOwner], House[h][hLocked] ? 1 : 0, House[h][hDbId]));
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Cmd_BuyHouse(playerid)
{
	new h = House_GetNearby(playerid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "Рядом нет дома. Подойди к зелёному домику.");
	if (House[h][hOwner] != 0) return SendClientMessage(playerid, COLOR_GREY, "Этот дом уже куплен.");
	if (House_GetOwned(playerid) != -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя уже есть дом. Сначала продай его: /sellhouse");
	if (GetMoney(playerid) < House[h][hPrice]) return SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", House[h][hPrice]);

	GiveMoney(playerid, -House[h][hPrice]);
	House[h][hOwner] = Player[playerid][pDbId];
	House[h][hLocked] = false;
	GetPlayerName(playerid, House[h][hOwnerName], MAX_PLAYER_NAME);
	House_Save(h);
	House_Refresh(h);
	Account_Save(playerid);
	Account_SetNormalSpawn(playerid);   // теперь спавн у дома

	SendClientMessage(playerid, COLOR_GREEN, "Поздравляем! Ты купил дом №%d за $%d. Войти: /enter, замок: /lock", House[h][hDbId], House[h][hPrice]);
	return true;
}

stock bool:Cmd_SellHouse(playerid)
{
	new h = House_GetOwned(playerid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет дома.");
	if (House_GetNearby(playerid) != h) return SendClientMessage(playerid, COLOR_GREY, "Подойди к своему дому.");

	new refund = House[h][hPrice] / 2;
	GiveMoney(playerid, refund);
	House[h][hOwner] = 0;
	House[h][hOwnerName][0] = '\0';
	House[h][hLocked] = false;
	House_Save(h);
	House_Refresh(h);
	Account_Save(playerid);
	Account_SetNormalSpawn(playerid);

	SendClientMessage(playerid, COLOR_GREEN, "Ты продал дом государству за $%d (половина цены).", refund);
	return true;
}

stock bool:Cmd_Enter(playerid)
{
	new h = House_GetNearby(playerid);
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "Рядом нет входа.");
	if (House[h][hOwner] == 0) return SendClientMessage(playerid, COLOR_GREY, "Дом продаётся и закрыт. Купить: /buyhouse");
	if (House[h][hLocked] && House[h][hOwner] != Player[playerid][pDbId])
		return SendClientMessage(playerid, COLOR_GREY, "Дверь заперта.");

	SetPlayerInterior(playerid, House[h][hInterior]);
	SetPlayerVirtualWorld(playerid, HOUSE_WORLD_OFFSET + h);
	SetPlayerPos(playerid, House[h][hIX], House[h][hIY], House[h][hIZ]);
	SetCameraBehindPlayer(playerid);
	Player[playerid][pHouseInside] = h;
	SendClientMessage(playerid, COLOR_WHITE, "Выйти из дома: /exit (стоя у двери)");
	return true;
}

stock bool:Cmd_Exit(playerid)
{
	new h = Player[playerid][pHouseInside];
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "Ты не в доме.");
	if (!IsPlayerInRangeOfPoint(playerid, 3.0, House[h][hIX], House[h][hIY], House[h][hIZ]))
		return SendClientMessage(playerid, COLOR_GREY, "Подойди к двери (туда, где ты появился).");

	SetPlayerInterior(playerid, 0);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerPos(playerid, House[h][hX], House[h][hY], House[h][hZ]);
	SetCameraBehindPlayer(playerid);
	Player[playerid][pHouseInside] = -1;
	return true;
}

stock bool:Cmd_Lock(playerid)
{
	// Сначала пробуем дом, потом машину (vehicles.inc)
	new h = House_GetOwned(playerid);
	if (h != -1 && (House_GetNearby(playerid) == h || Player[playerid][pHouseInside] == h))
	{
		House[h][hLocked] = !House[h][hLocked];
		House_Save(h);
		if (House[h][hLocked]) SendClientMessage(playerid, COLOR_WHITE, "Дом {FF6347}закрыт{FFFFFF}.");
		else SendClientMessage(playerid, COLOR_WHITE, "Дом {33AA33}открыт{FFFFFF}.");
		return true;
	}
	return Vehicles_ToggleLock(playerid);
}

stock bool:Cmd_CreateHouse(playerid, const params[])
{
	new pos = 0, price, type;
	if (!NextIntParam(params, pos, price) || !NextIntParam(params, pos, type) || type < 1 || type > sizeof (gHouseIntId) || price < 1)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /createhouse [цена] [тип 1-5]");
	if (gHouseCount >= MAX_HOUSES) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит домов (MAX_HOUSES).");

	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	Houses_InsertDefault(x, y, z, price, type);

	new h = gHouseCount++;
	new t = type - 1;
	House[h][hDbId] = DB_LastInsertId();
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
	House[h][hPickup] = INVALID_PICKUP;
	House[h][hLabel] = INVALID_3DTEXT_ID;
	House_Refresh(h);

	SendClientMessage(playerid, COLOR_GREEN, "Дом №%d создан. Цена $%d, интерьер %d.", House[h][hDbId], price, type);
	return true;
}
