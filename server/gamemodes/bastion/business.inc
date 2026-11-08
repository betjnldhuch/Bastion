// =====================================================================
//  business.inc — бизнесы: магазины, бары, одежда, оружие.
//
//  Любой игрок может купить товар: подойди к значку бизнеса и введи /buy.
//  Бизнес можно купить (/buybiz). Тогда владельцу в кассу идёт
//  BIZ_SALE_SHARE% от каждой покупки и BIZ_PAYDAY_INCOME каждый PayDay.
//  Забрать деньги из кассы: /bizcash (стоя у своего бизнеса).
//
//  Типы: 1 — магазин 24/7, 2 — бар, 3 — одежда, 4 — оружейный.
//  Админы (4+): /createbiz [тип] [цена] [название], /delbiz
// =====================================================================

#define BIZ_RANGE           2.5
#define GUNS_MIN_LEVEL      2           // с какого уровня продают оружие

enum
{
	BIZ_NONE,
	BIZ_SHOP,       // 1 — 24/7
	BIZ_BAR,        // 2 — бар
	BIZ_CLOTHES,    // 3 — одежда
	BIZ_GUNS        // 4 — оружейный
}
#define BIZ_TYPES           5

static const gBizTypeNames[BIZ_TYPES][24] = { "-", "Магазин 24/7", "Бар", "Магазин одежды", "Оружейный магазин" };

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
	bPickup,
	Text3D:bLabel
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
	if (Biz[b][bPickup] != INVALID_PICKUP) DestroyPickup(Biz[b][bPickup]);
	if (Biz[b][bLabel] != INVALID_3DTEXT_ID) Delete3DTextLabel(Biz[b][bLabel]);

	Biz[b][bPickup] = CreatePickup(1239, 1, Biz[b][bX], Biz[b][bY], Biz[b][bZ], 0);
	if (Biz[b][bOwner] == 0)
	{
		Biz[b][bLabel] = Create3DTextLabel("{FF9900}%s\n{FFFFFF}%s\nALT — купить товар\nБизнес продаётся: ${FFFF00}%d{FFFFFF} (/buybiz)",
			COLOR_WHITE, Biz[b][bX], Biz[b][bY], Biz[b][bZ] + 0.6, 15.0, 0, true,
			Biz[b][bName], Biz_GetTypeName(Biz[b][bType]), Biz[b][bPrice]);
	}
	else
	{
		Biz[b][bLabel] = Create3DTextLabel("{FF9900}%s\n{FFFFFF}%s\nALT — купить товар\nВладелец: %s",
			COLOR_WHITE, Biz[b][bX], Biz[b][bY], Biz[b][bZ] + 0.6, 15.0, 0, true,
			Biz[b][bName], Biz_GetTypeName(Biz[b][bType]), Biz[b][bOwnerName]);
	}
}

stock Biz_Add(dbid, const name[], type, owner, const ownerName[], price, Float:x, Float:y, Float:z, till)
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
	Biz[b][bPickup] = INVALID_PICKUP;
	Biz[b][bLabel] = INVALID_3DTEXT_ID;
	Biz_Refresh(b);
	return b;
}

stock Biz_Save(b)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE businesses SET owner = %d, till = %d WHERE id = %d",
		Biz[b][bOwner], Biz[b][bTill], Biz[b][bDbId]));
}

stock Business_Init()
{
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

	r = DB_ExecuteQuery(gDB, "SELECT b.id, b.name, b.type, b.owner, IFNULL(a.name, ''), b.price, b.x, b.y, b.z, b.till FROM businesses b LEFT JOIN accounts a ON a.id = b.owner ORDER BY b.id");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		new name[32], owner[MAX_PLAYER_NAME];
		for (new i = 0; i < rows; i++)
		{
			DB_GetFieldString(r, 1, name, sizeof name);
			DB_GetFieldString(r, 4, owner, sizeof owner);
			Biz_Add(DB_GetFieldInt(r, 0), name, DB_GetFieldInt(r, 2), DB_GetFieldInt(r, 3), owner, DB_GetFieldInt(r, 5),
				DB_GetFieldFloat(r, 6), DB_GetFieldFloat(r, 7), DB_GetFieldFloat(r, 8), DB_GetFieldInt(r, 9));
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Business] Loaded %d businesses.", gBizCount);
}

stock Biz_GetNearby(playerid)
{
	if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return -1;
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
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /createbiz [тип 1-4] [цена] [название]  (1 24/7, 2 бар, 3 одежда, 4 оружие)");
	if (GetPlayerInterior(playerid) != 0) return SendClientMessage(playerid, COLOR_GREY, "Бизнес можно ставить только на улице.");
	if (gBizCount >= MAX_BUSINESSES) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит бизнесов (MAX_BUSINESSES).");

	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO businesses (name, type, price, x, y, z) VALUES ('%q', %d, %d, %f, %f, %f)",
		name, type, price, x, y, z));
	Biz_Add(DB_LastInsertId(), name, type, 0, "", price, x, y, z, 0);
	SendClientMessage(playerid, COLOR_ADMIN, "Бизнес «%s» (%s) создан, цена $%d.", name, Biz_GetTypeName(type), price);
	return true;
}

stock bool:Cmd_DelBiz(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new b = Biz_GetNearby(playerid);
	if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "Подойди к бизнесу.");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM businesses WHERE id = %d", Biz[b][bDbId]));
	DestroyPickup(Biz[b][bPickup]);
	Delete3DTextLabel(Biz[b][bLabel]);
	SendClientMessage(playerid, COLOR_ADMIN, "Бизнес «%s» удалён.", Biz[b][bName]);
	new last = gBizCount - 1;
	if (b != last) Biz[b] = Biz[last];
	gBizCount--;
	return true;
}
