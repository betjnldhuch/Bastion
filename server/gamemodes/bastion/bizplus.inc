// =====================================================================
//  bizplus.inc — новые виды бизнесов и управление для владельца.
//
//  Типы бизнесов и товары лежат в business.inc. Здесь то, что у них общее:
//
//  Склад. Товар владелец закупает сам (меню бизнеса > «Закупить товар»).
//    Склад считается в долларах по закупочной цене. Каждая покупка снимает
//    со склада BIZ_WHOLESALE_PCT% базовой цены товара, а в кассу идёт то, что
//    заплатил покупатель, минус налог BIZ_SALE_TAX%. Склад пуст — магазин
//    работает дальше (товар даёт штат), но владелец с продаж ничего не получает.
//  Наценка. Владелец ставит цены от BIZ_MARKUP_MIN до BIZ_MARKUP_MAX% базовых.
//  PayDay. Каждый тип приносит свой доход (таблица gBizCfg), если на складе есть товар.
//  АЗС и автомастерская получают выручку с заправок / СТО в радиусе BIZ_SERVICE_RANGE.
//  Спортзал учит стилям боя (стиль хранится в accounts.fstyle), лотерея продаёт билеты.
//
//  Поставки с заводов (factions.inc): ящик товара нужного вида пополняет склад на goods_value,
//    водителю из кассы — goods_pay за ящик. Закупка в меню («из воздуха») дороже на biz_air%.
//  Рынок: если на складах всех бизнесов одного товара меньше mkt_low%, цены у них выше на mkt_up%,
//    если больше mkt_high% — ниже на mkt_down%. Все цифры — /srvset. Игрокам: /market, /contracts.
//
//  Игрокам:  /bizmenu (/biz) — меню своего бизнеса, /biztypes — виды бизнесов и цены.
//  Меню: информация, касса, закупка товара, цены, название, вход, продажа игроку / штату.
//
//  Все цифры ниже — настройки экономики, их можно менять не трогая код.
// =====================================================================

#define BIZ_WHOLESALE_PCT   50          // закупочная цена товара, % от базовой розничной
#define BIZ_SALE_TAX        Cfg(CFG_BIZ_TAX)    // налог с каждой продажи, % (/srvset)
#define CONTRACT_ROWS       40          // строк в /contracts
#define DELIVER_RANGE       25.0        // как близко ко входу бизнеса разгружать товар
#define BIZ_MARKUP_MIN      Cfg(CFG_BIZ_MARKUP_MIN) // границы цен владельца, % от базовых (по умолчанию 70, /srvset)
#define BIZ_MARKUP_MAX      Cfg(CFG_BIZ_MARKUP_MAX) // по умолчанию 150, /srvset
#define BIZ_START_STOCK     Cfg(CFG_BIZ_START_STOCK) // товар на складе при покупке бизнеса у штата, $ (по умолчанию 2000, /srvset)
#define BIZ_SERVICE_RANGE   80.0        // в каком радиусе АЗС / СТО считаются «своими» для бизнеса
#define BIZ_RENAME_PRICE    Cfg(CFG_BIZ_RENAME) // смена названия (по умолчанию 1000, /srvset)
#define BIZ_SELLTO_RANGE    5.0         // как близко должен стоять покупатель бизнеса
#define BIZ_SELLTO_MAX      100000000   // потолок цены при продаже игроку

// Лотерея: шансы из 1000 на билет
#define LOTTERY_JACKPOT     5           // главный приз (цена билета x множитель билета)
#define LOTTERY_BIG         50          // цена билета x5
#define LOTTERY_SMALL       200         // возврат цены билета

// По типам бизнеса: стандартная цена (/createbiz ... 0), доход в кассу за PayDay, вместимость склада ($)
enum E_BIZ_CFG
{
	bcPrice,
	bcPayDay,
	bcStockMax
}
static const gBizCfg[BIZ_TYPES][E_BIZ_CFG] =
{
	{ 0,      0,   0     },
	{ 40000,  200, 20000 },     // 1 магазин 24/7
	{ 30000,  150, 15000 },     // 2 бар
	{ 35000,  150, 15000 },     // 3 одежда
	{ 80000,  300, 40000 },     // 4 оружейный
	{ 150000, 300, 0     },     // 5 казино (склад не нужен)
	{ 120000, 250, 40000 },     // 6 АЗС
	{ 100000, 250, 40000 },     // 7 автомастерская
	{ 45000,  180, 15000 },     // 8 закусочная
	{ 40000,  150, 15000 },     // 9 аптека
	{ 90000,  250, 20000 },     // 10 ночной клуб
	{ 50000,  150, 20000 },     // 11 спортзал
	{ 35000,  150, 20000 },     // 12 хозтовары
	{ 45000,  150, 20000 },     // 13 электроника
	{ 25000,  100, 10000 },     // 14 лотерея
	{ 60000,  200, 0     }      // 15 тир (склад не нужен: продаёт услуги)
};

// Новые бизнесы, которые появляются один раз при обновлении (координаты примерные, двигаются в /markers)
enum E_BIZX_DEF
{
	xdName[32],
	xdType,
	Float:xdX,
	Float:xdY,
	Float:xdZ
}
static const gBizXDefaults[][E_BIZX_DEF] =
{
	{ "АЗС Idlewood",          BIZ_FUEL,     1928.58, -1776.29, 13.55 },
	{ "Автомастерская Temple", BIZ_STO,      1048.00, -1030.00, 32.10 },
	{ "Burger Shot Marina",    BIZ_FASTFOOD, 810.49,  -1616.19, 13.55 },
	{ "Пиццерия Idlewood",     BIZ_FASTFOOD, 2105.49, -1806.49, 13.55 },
	{ "Клуб Alhambra",         BIZ_CLUB,     1836.99, -1682.45, 13.32 },
	{ "Спортзал Ganton",       BIZ_GYM,      2229.92, -1721.26, 13.56 }
};

static gMenuBiz[MAX_PLAYERS];                   // номер бизнеса в базе, для которого открыто меню
static gMenuActs[MAX_PLAYERS][16];
static gStyle[MAX_PLAYERS];                     // стиль боя
static gStyleAcc[MAX_PLAYERS];                  // для какого аккаунта он загружен
static gOfferSeller[MAX_PLAYERS];               // предложение купить бизнес: аккаунт продавца,
static gOfferBiz[MAX_PLAYERS];                  // номер бизнеса в базе
static gOfferPrice[MAX_PLAYERS];                // и цена
static gCtrMap[MAX_PLAYERS][CONTRACT_ROWS];     // строка /contracts: бизнес или 1000 + завод

enum
{
	BXA_INFO,
	BXA_STATEMENT,
	BXA_HISTORY,
	BXA_TAX,
	BXA_SELLER,
	BXA_STAFF,
	BXA_UPG,
	BXA_CASH,
	BXA_STOCK,
	BXA_MARKUP,
	BXA_NAME,
	BXA_LOCK,
	BXA_SELLTO,
	BXA_SELL
}

// ---------------------------------------------------------------------
//  Настройки и расчёты
// ---------------------------------------------------------------------
stock BizX_DefaultPrice(type)
{
	return (type > BIZ_NONE && type < BIZ_TYPES) ? gBizCfg[type][bcPrice] : 0;
}

stock bool:BizX_UsesStock(b)
{
	new t = Biz[b][bType];
	return (t > BIZ_NONE && t < BIZ_TYPES && gBizCfg[t][bcStockMax] > 0);
}

// Какой товар с заводов нужен бизнесу этого типа (-1 — никакой)
stock Prod_OfBizType(t)
{
	switch (t)
	{
		case BIZ_FUEL: return PROD_FUEL;
		case BIZ_SHOP, BIZ_BAR, BIZ_FASTFOOD, BIZ_CLUB: return PROD_FOOD;
		case BIZ_STO: return PROD_PARTS;
		case BIZ_CLOTHES, BIZ_GUNS, BIZ_PHARMACY, BIZ_GYM, BIZ_HARDWARE, BIZ_ELECTRO, BIZ_LOTTERY: return PROD_GOODS;
	}
	return -1;
}

// Сколько товара на складах всех бизнесов с владельцем, % от вместимости (-1 — таких бизнесов нет)
stock Market_Fill(prod)
{
	new sum = 0, cap = 0;
	for (new b = 0; b < gBizCount; b++)
	{
		if (Biz[b][bOwner] == 0 || !BizX_UsesStock(b) || Prod_OfBizType(Biz[b][bType]) != prod) continue;
		sum += Biz[b][bStock];
		cap += BizX_StockMax(b);
	}
	if (cap <= 0) return -1;
	new unit = cap / 100;
	return (sum >= cap) ? 100 : (sum / ((unit < 1) ? 1 : unit));
}

// Поправка цен от рынка, %: + дефицит, - избыток
stock Market_PricePct(prod)
{
	if (prod < 0) return 0;
	new fill = Market_Fill(prod);
	if (fill < 0) return 0;
	if (fill < Cfg(CFG_MKT_LOW)) return Cfg(CFG_MKT_UP);
	if (fill > Cfg(CFG_MKT_HIGH)) return -Cfg(CFG_MKT_DOWN);
	return 0;
}

// Цена товара в бизнесе b с наценкой владельца и поправкой рынка
stock BizX_Price(b, base)
{
	new pct = (Biz[b][bOwner] == 0) ? 100 : Biz[b][bMarkup];
	pct += Market_PricePct(Prod_OfBizType(Biz[b][bType]));
	if (pct == 100) return base;
	new price = base * pct / 100;
	return (price < 1) ? 1 : price;
}

// Сколько списывается со склада за продажу товара с базовой ценой base
stock BizX_StockCost(base) { return base * BIZ_WHOLESALE_PCT / 100; }

stock BizX_StockMax(b)
{
	new t = Biz[b][bType];
	if (t <= BIZ_NONE || t >= BIZ_TYPES) return 0;
	return BizW_HasUpg(b, UPG_STOCK) ? gBizCfg[t][bcStockMax] * 2 : gBizCfg[t][bcStockMax];
}

// Доход за PayDay с учётом рекламы
stock BizX_PayDayIncome(b)
{
	new t = Biz[b][bType];
	if (t <= BIZ_NONE || t >= BIZ_TYPES) return 0;
	return BizW_HasUpg(b, UPG_ADS) ? gBizCfg[t][bcPayDay] * (100 + UPG_ADS_PCT) / 100 : gBizCfg[t][bcPayDay];
}

stock BizX_Save(b)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE businesses SET till = %d, markup = %d, stock = %d, earned = %d, tax_debt = %d, tax_pd = %d, upg = %d, sellmin = %d, comm = %d WHERE id = %d",
		Biz[b][bTill], Biz[b][bMarkup], Biz[b][bStock], Biz[b][bEarned], Biz[b][bTaxDebt], Biz[b][bTaxPD], Biz[b][bUpg], Biz[b][bSellMin], Biz[b][bComm], Biz[b][bDbId]));
}

static BizX_AddTill(b, amount)
{
	if (amount <= 0) return;
	if (Biz[b][bTill] > cellmax - amount) amount = cellmax - Biz[b][bTill];
	Biz[b][bTill] += amount;
	if (Biz[b][bEarned] <= cellmax - amount) Biz[b][bEarned] += amount;
	BizS_Log(b, amount);         // доход по дням для выписки (bizstat.inc)
}

// Продажа в бизнесе b: paid — заплатил покупатель, base — базовая цена (по ней списывается склад)
stock BizX_Sale(b, paid, base)
{
	if (b < 0 || b >= gBizCount || Biz[b][bOwner] == 0 || BizW_Closed(b)) return;
	if (BizX_UsesStock(b))
	{
		new cost = base * BIZ_WHOLESALE_PCT / 100;
		if (Biz[b][bStock] < cost) return;      // склад пуст: товар дал штат, владельцу ничего
		Biz[b][bStock] -= cost;
	}
	BizX_AddTill(b, paid * (100 - BIZ_SALE_TAX) / 100);
	BizX_Save(b);
}

// Заправка или СТО рядом с бизнесом нужного типа: выручка идёт ему как обычная продажа
stock BizX_ServiceSale(playerid, type, amount)
{
	if (amount <= 0 || GetPlayerVirtualWorld(playerid) != 0) return;
	new best = -1, Float:bestDist = BIZ_SERVICE_RANGE, Float:d;
	for (new b = 0; b < gBizCount; b++)
	{
		if (Biz[b][bType] != type || Biz[b][bOwner] == 0 || Biz[b][bOInt] != 0) continue;
		d = GetPlayerDistanceFromPoint(playerid, Biz[b][bX], Biz[b][bY], Biz[b][bZ]);
		if (d < bestDist) { bestDist = d; best = b; }
	}
	if (best != -1) BizX_Sale(best, amount, amount);
}

// Раз в PayDay для бизнеса с владельцем (из Business_PayDay)
stock BizX_PayDay(b)
{
	new t = Biz[b][bType];
	if (t <= BIZ_NONE || t >= BIZ_TYPES) return;
	// пустой или закрытый за долги магазин клиентов не держит
	if (!BizW_Closed(b) && !(BizX_UsesStock(b) && Biz[b][bStock] <= 0)) BizX_AddTill(b, BizX_PayDayIncome(b));
	BizX_Save(b);
	BizW_PayDayTax(b);          // налог (bizwork.inc)
}

// Бизнес куплен у штата: цены обычные, на складе стартовый товар
stock BizX_OnBought(b)
{
	Biz[b][bMarkup] = 100;
	if (BizX_UsesStock(b) && Biz[b][bStock] < BIZ_START_STOCK) Biz[b][bStock] = BIZ_START_STOCK;
	BizX_Save(b);
}

// ---------------------------------------------------------------------
//  Запуск: новые колонки, новые бизнесы (один раз), загрузка склада и цен
// ---------------------------------------------------------------------
stock BizX_Init()
{
	DB_AddColumn("businesses", "markup", "INTEGER DEFAULT 100");
	DB_AddColumn("businesses", "stock", "INTEGER DEFAULT " #BIZ_START_STOCK);
	DB_AddColumn("businesses", "earned", "INTEGER DEFAULT 0");
	DB_AddColumn("businesses", "tax_debt", "INTEGER DEFAULT 0");
	DB_AddColumn("businesses", "tax_pd", "INTEGER DEFAULT 0");
	DB_AddColumn("businesses", "upg", "INTEGER DEFAULT 0");
	DB_AddColumn("businesses", "sellmin", "INTEGER DEFAULT 100");
	DB_AddColumn("businesses", "comm", "INTEGER DEFAULT 10");
	DB_AddColumn("accounts", "fstyle", "INTEGER DEFAULT 4");
	BizS_Init();
	BizW_Init();
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS bizx_meta (k TEXT PRIMARY KEY, v INTEGER)"));

	// новые бизнесы добавляются один раз; удалённые админом заново не появятся
	new bool:seeded = false;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT v FROM bizx_meta WHERE k = 'seed1'");
	if (r != DBResult:0)
	{
		seeded = (DB_GetRowCount(r) > 0);
		DB_FreeResultSet(r);
	}
	if (!seeded)
	{
		for (new i = 0; i < sizeof (gBizXDefaults); i++)
		{
			new price = BizX_DefaultPrice(gBizXDefaults[i][xdType]);
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO businesses (name, type, price, x, y, z) VALUES ('%q', %d, %d, %f, %f, %f)",
				gBizXDefaults[i][xdName], gBizXDefaults[i][xdType], price,
				gBizXDefaults[i][xdX], gBizXDefaults[i][xdY], gBizXDefaults[i][xdZ]));
			Biz_Add(DB_LastInsertId(), gBizXDefaults[i][xdName], gBizXDefaults[i][xdType], 0, "", price,
				gBizXDefaults[i][xdX], gBizXDefaults[i][xdY], gBizXDefaults[i][xdZ], 0);
		}
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO bizx_meta (k, v) VALUES ('seed1', 1)"));
	}
	// тир (вид 15) — один раз
	r = DB_ExecuteQuery(gDB, "SELECT v FROM bizx_meta WHERE k = 'seed_range'");
	seeded = false;
	if (r != DBResult:0)
	{
		seeded = (DB_GetRowCount(r) > 0);
		DB_FreeResultSet(r);
	}
	if (!seeded)
	{
		new price = BizX_DefaultPrice(BIZ_SHOOTRANGE);
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO businesses (name, type, price, x, y, z) VALUES ('%q', %d, %d, %f, %f, %f)",
			"Тир Ammu-Nation Willowfield", BIZ_SHOOTRANGE, price, 2400.50, -1981.90, 13.55));
		Biz_Add(DB_LastInsertId(), "Тир Ammu-Nation Willowfield", BIZ_SHOOTRANGE, 0, "", price, 2400.50, -1981.90, 13.55, 0);
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO bizx_meta (k, v) VALUES ('seed_range', 1)"));
	}

	r = DB_ExecuteQuery(gDB, "SELECT id, markup, stock, earned, tax_debt, tax_pd, upg, sellmin, comm FROM businesses");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new b = Biz_FindByDbId(DB_GetFieldInt(r, 0));
			if (b != -1)
			{
				Biz[b][bMarkup] = DB_GetFieldInt(r, 1);
				if (Biz[b][bMarkup] < BIZ_MARKUP_MIN || Biz[b][bMarkup] > BIZ_MARKUP_MAX) Biz[b][bMarkup] = 100;
				Biz[b][bStock] = DB_GetFieldInt(r, 2);
				Biz[b][bEarned] = DB_GetFieldInt(r, 3);
				Biz[b][bTaxDebt] = DB_GetFieldInt(r, 4);
				Biz[b][bTaxPD] = DB_GetFieldInt(r, 5);
				Biz[b][bUpg] = DB_GetFieldInt(r, 6);
				Biz[b][bSellMin] = DB_GetFieldInt(r, 7);
				Biz[b][bComm] = DB_GetFieldInt(r, 8);
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	for (new i = 0; i < MAX_PLAYERS; i++) { gStyleAcc[i] = 0; gOfferSeller[i] = 0; }
}

// ---------------------------------------------------------------------
//  Спортзал: стиль боя
// ---------------------------------------------------------------------
stock BizX_GetStyle(playerid)
{
	new acc = Player[playerid][pDbId];
	if (acc == 0) return 4;
	if (gStyleAcc[playerid] != acc)
	{
		gStyle[playerid] = 4;
		new DBResult:r = DB_ExecuteQuery(gDB, "SELECT fstyle FROM accounts WHERE id = %d", acc);
		if (r != DBResult:0)
		{
			if (DB_GetRowCount(r) > 0) gStyle[playerid] = DB_GetFieldInt(r, 0);
			DB_FreeResultSet(r);
		}
		if (gStyle[playerid] != 5 && gStyle[playerid] != 6 && gStyle[playerid] != 7 && gStyle[playerid] != 15) gStyle[playerid] = 4;
		gStyleAcc[playerid] = acc;
	}
	return gStyle[playerid];
}

stock BizX_SetStyle(playerid, style)
{
	BizX_GetStyle(playerid);
	gStyle[playerid] = style;
	SetPlayerFightingStyle(playerid, FIGHT_STYLE:style);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET fstyle = %d WHERE id = %d", style, Player[playerid][pDbId]));
	SendClientMessage(playerid, COLOR_WHITE, "Тренер показал приёмы. Новый стиль боя останется с тобой, сменить можно в любом спортзале.");
}

// Из OnPlayerSpawn
stock BizX_OnSpawn(playerid)
{
	new style = BizX_GetStyle(playerid);
	if (style != 4) SetPlayerFightingStyle(playerid, FIGHT_STYLE:style);
}

// ---------------------------------------------------------------------
//  Лотерея: билет уже оплачен, призы платит штат
// ---------------------------------------------------------------------
stock BizX_Lottery(playerid, price, mult)
{
	new roll = random(1000), win = 0;
	if (roll < LOTTERY_JACKPOT) win = price * mult;
	else if (roll < LOTTERY_JACKPOT + LOTTERY_BIG) win = price * 5;
	else if (roll < LOTTERY_JACKPOT + LOTTERY_BIG + LOTTERY_SMALL) win = price;
	if (win == 0)
	{
		SendClientMessage(playerid, COLOR_GREY, "Ты стёр защитный слой: билет без выигрыша.");
		return;
	}
	GiveMoney(playerid, win, "выигрыш в лотерею");
	SendClientMessage(playerid, COLOR_GREEN, "Билет выиграл! Твой приз: $%d.", win);
	if (roll < LOTTERY_JACKPOT)
	{
		new msg[112];
		format(msg, sizeof msg, "%s срывает главный приз лотереи: $%d!", GetRPName(playerid), win);
		SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	}
}

// ---------------------------------------------------------------------
//  Меню владельца
// ---------------------------------------------------------------------
// Бизнес из открытого меню, если он всё ещё принадлежит игроку (иначе -1)
static BizX_Mine(playerid)
{
	new b = Biz_FindByDbId(gMenuBiz[playerid]);
	if (b == -1 || Player[playerid][pDbId] == 0 || Biz[b][bOwner] == 0) return -1;
	if (Biz[b][bOwner] == Player[playerid][pDbId]) return b;
	new sb;
	if (BizW_RoleOf(playerid, sb) == BW_DEPUTY && sb == b) return b;     // заместитель (bizwork.inc)
	return -1;
}

static bool:BizX_Near(playerid, b)
{
	if (Biz_GetNearby(playerid) == b) return true;
	SendClientMessage(playerid, COLOR_GREY, "Для этого подойди к своему бизнесу.");
	return false;
}

stock bool:Cmd_BizMenu(playerid)
{
	new b = Biz_GetOwned(playerid), bool:own = true;
	if (b == -1)
	{
		// заместитель управляет бизнесом владельца, кроме кассы и продажи
		if (BizW_RoleOf(playerid, b) != BW_DEPUTY) b = -1;
		own = false;
	}
	if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет бизнеса. Купить: ALT у значка бизнеса, который продаётся. Виды бизнесов: /biztypes");
	gMenuBiz[playerid] = Biz[b][bDbId];

	new list[1024], line[112], n = 0;
	strcat(list, "Действие\tСейчас\n");
	gMenuActs[playerid][n++] = BXA_INFO; strcat(list, "Информация\t \n");
	gMenuActs[playerid][n++] = BXA_STATEMENT; strcat(list, "Финансовая выписка\tпоказать: /showbiz [id]\n");
	gMenuActs[playerid][n++] = BXA_HISTORY; strcat(list, "История дохода\tпо дням\n");
	if (own)
	{
		format(line, sizeof line, "Забрать деньги из кассы\t{33AA33}$%d\n", Biz[b][bTill]);
		gMenuActs[playerid][n++] = BXA_CASH; strcat(list, line);
	}
	if (Biz[b][bTaxDebt] > 0) format(line, sizeof line, "Налог\t{FF6347}долг $%d\n", Biz[b][bTaxDebt]);
	else format(line, sizeof line, "Налог\t$%d за PayDay\n", BizW_Tax(b));
	gMenuActs[playerid][n++] = BXA_TAX; strcat(list, line);
	if (BizX_UsesStock(b))
	{
		format(line, sizeof line, "Закупить товар\t%s$%d{FFFFFF} из $%d\n", (Biz[b][bStock] > 0) ? ("{FFFFFF}") : ("{FF6347}"), Biz[b][bStock], BizX_StockMax(b));
		gMenuActs[playerid][n++] = BXA_STOCK; strcat(list, line);
		format(line, sizeof line, "Цены у кассы\t%d%%\n", Biz[b][bMarkup]);
		gMenuActs[playerid][n++] = BXA_MARKUP; strcat(list, line);
		format(line, sizeof line, "Продавцы: цены и процент\tот %d%%, продавцу %d%%\n", BizW_SellMin(b), Biz[b][bComm]);
		gMenuActs[playerid][n++] = BXA_SELLER; strcat(list, line);
	}
	format(line, sizeof line, "Сотрудники\t%d\n", BizW_StaffCount(b, 0));
	gMenuActs[playerid][n++] = BXA_STAFF; strcat(list, line);
	if (own)
	{
		format(line, sizeof line, "Улучшения\t%d из %d\n", BizW_UpgCount(b), UPG_COUNT);
		gMenuActs[playerid][n++] = BXA_UPG; strcat(list, line);
		format(line, sizeof line, "Сменить название\t$%d\n", BIZ_RENAME_PRICE);
		gMenuActs[playerid][n++] = BXA_NAME; strcat(list, line);
	}
	if (Biz[b][bInt] != 0)
	{
		gMenuActs[playerid][n++] = BXA_LOCK;
		strcat(list, Biz[b][bLocked] ? ("Открыть вход\t{FF6347}закрыто\n") : ("Закрыть вход\t{33AA33}открыто\n"));
	}
	if (own)
	{
		gMenuActs[playerid][n++] = BXA_SELLTO; strcat(list, "Продать игроку\t \n");
		format(line, sizeof line, "Продать штату\t$%d\n", Biz[b][bPrice] / 2 + Biz[b][bTill]);
		gMenuActs[playerid][n++] = BXA_SELL; strcat(list, line);
	}
	for (new i = n; i < sizeof (gMenuActs[]); i++) gMenuActs[playerid][i] = -1;

	ShowPlayerDialog(playerid, DIALOG_BIZX_MENU, DIALOG_STYLE_TABLIST_HEADERS, Biz_IconCaption(Biz[b][bType], Biz[b][bName]), "%s", "Выбрать", "Закрыть", list);
	return true;
}

static BizX_ShowInfo(playerid, b)
{
	new text[900], line[160], t = Biz[b][bType];
	format(text, sizeof text, "{FF9900}%s{FFFFFF}, №%d\nВид: %s\nЦена у штата: $%d\n\nВ кассе: {33AA33}$%d{FFFFFF}\nЗаработано за всё время: $%d\nДоход за PayDay: $%d\n",
		Biz[b][bName], Biz[b][bDbId], Biz_GetTypeName(t), Biz[b][bPrice], Biz[b][bTill], Biz[b][bEarned], BizX_PayDayIncome(b));
	if (BizX_UsesStock(b))
	{
		format(line, sizeof line, "Товар на складе: $%d из $%d\nЦены: %d%% от обычных\n\n", Biz[b][bStock], BizX_StockMax(b), Biz[b][bMarkup]);
		strcat(text, line);
		format(line, sizeof line, "{AFAFAF}С каждой покупки в кассу идёт её цена минус налог %d%%,\nа со склада уходит %d%% обычной цены товара.\n", BIZ_SALE_TAX, BIZ_WHOLESALE_PCT);
		strcat(text, line);
		new pr = Prod_OfBizType(t);
		if (pr >= 0)
		{
			format(line, sizeof line, "Товар с заводов: %s. Ящик даёт склада на $%d, водителю $%d из кассы.\n", gProdName[pr], Cfg(CFG_GOODS_VALUE), Cfg(CFG_GOODS_PAY));
			strcat(text, line);
		}
		strcat(text, "Склад пуст — магазин работает, но ты ничего не получаешь,\nи дохода за PayDay тоже нет.\n");
		if (t == BIZ_FUEL) strcat(text, "АЗС получает ещё и выручку с заправок рядом.\n");
		if (t == BIZ_STO) strcat(text, "Мастерская получает ещё и выручку с СТО рядом.\n");
	}
	else strcat(text, "\n{AFAFAF}Этому бизнесу товар не нужен.\n");
	ShowPlayerDialog(playerid, DIALOG_BIZX_INFO, DIALOG_STYLE_MSGBOX, Biz_IconCaption(t, "Мой бизнес"), "%s", "Назад", "Закрыть", text);
}

// /biztypes — виды бизнесов (всем)
stock bool:Cmd_BizTypes(playerid)
{
	new list[900] = "Вид\tЦена у штата\tДоход за PayDay\n", line[72];
	for (new t = BIZ_NONE + 1; t < BIZ_TYPES; t++)
	{
		format(line, sizeof line, "%s\t$%d\t$%d\n", Biz_GetTypeName(t), gBizCfg[t][bcPrice], gBizCfg[t][bcPayDay]);
		strcat(list, line);
	}
	gMenuBiz[playerid] = 0;
	ShowPlayerDialog(playerid, DIALOG_BIZX_INFO, DIALOG_STYLE_TABLIST_HEADERS, "Виды бизнесов", "%s", "Закрыть", "", list);
	return true;
}

// Передать бизнес покупателю (сделка между игроками)
static BizX_Transfer(b, seller, buyer, price)
{
	new till = Biz[b][bTill];
	GiveMoney(buyer, -price, "покупка бизнеса у игрока");
	GiveMoney(seller, price + till, "продажа бизнеса игроку");
	Biz[b][bTill] = 0;
	Biz[b][bOwner] = Player[buyer][pDbId];
	GetPlayerName(buyer, Biz[b][bOwnerName], MAX_PLAYER_NAME);
	Biz[b][bLocked] = false;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE businesses SET locked = 0 WHERE id = %d", Biz[b][bDbId]));
	Biz_Save(b);
	Biz_Refresh(b);
	Account_Save(buyer);
	Account_Save(seller);
	SendClientMessage(seller, COLOR_GREEN, "Ты продал «%s» игроку %s за $%d. Касса ($%d) осталась тебе.", Biz[b][bName], GetRPName(buyer), price, till);
	SendClientMessage(buyer, COLOR_GREEN, "«%s» теперь твой. Управление: ALT у бизнеса или /bizmenu", Biz[b][bName]);
}

stock BizX_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid < DIALOG_BIZX_MENU || dialogid > DIALOG_BIZX_OFFER) return 0;

	// предложение купить бизнес у другого игрока
	if (dialogid == DIALOG_BIZX_OFFER)
	{
		new sellerDb = gOfferSeller[playerid], price = gOfferPrice[playerid];
		gOfferSeller[playerid] = 0;
		if (sellerDb == 0) return 1;
		new seller = INVALID_PLAYER_ID;
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pDbId] == sellerDb) { seller = i; break; }
		}
		if (seller == INVALID_PLAYER_ID) { SendClientMessage(playerid, COLOR_GREY, "Продавец вышел из игры."); return 1; }
		new b = Biz_FindByDbId(gOfferBiz[playerid]);
		if (!response)
		{
			SendClientMessage(seller, COLOR_GREY, "%s отказался покупать бизнес.", GetRPName(playerid));
			return 1;
		}
		if (b == -1 || Biz[b][bOwner] != sellerDb) { SendClientMessage(playerid, COLOR_GREY, "Этот бизнес уже не продаётся."); return 1; }
		new Float:x, Float:y, Float:z;
		GetPlayerPos(seller, x, y, z);
		if (!IsPlayerInRangeOfPoint(playerid, BIZ_SELLTO_RANGE, x, y, z) || GetPlayerVirtualWorld(playerid) != GetPlayerVirtualWorld(seller))
		{
			SendClientMessage(playerid, COLOR_GREY, "Продавец отошёл.");
			return 1;
		}
		if (Debts_BlockBuy(playerid)) return 1;
		if (Biz_GetOwned(playerid) != -1) { SendClientMessage(playerid, COLOR_GREY, "У тебя уже есть бизнес."); return 1; }
		if (GetMoney(playerid) < price)
		{
			SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", price);
			SendClientMessage(seller, COLOR_GREY, "У %s не хватило денег на бизнес.", GetRPName(playerid));
			return 1;
		}
		BizX_Transfer(b, seller, playerid, price);
		return 1;
	}

	if (dialogid == DIALOG_BIZX_INFO)
	{
		if (response && gMenuBiz[playerid] != 0 && BizX_Mine(playerid) != -1) Cmd_BizMenu(playerid);
		return 1;
	}

	new b = BizX_Mine(playerid);
	if (b == -1) return 1;
	// касса, название и продажа — только владельцу
	if ((dialogid == DIALOG_BIZX_NAME || dialogid == DIALOG_BIZX_SELL || dialogid == DIALOG_BIZX_SELLTO) && Biz[b][bOwner] != Player[playerid][pDbId]) return 1;
	if (!response)
	{
		if (dialogid != DIALOG_BIZX_MENU) Cmd_BizMenu(playerid);    // «Назад» из любого окна
		return 1;
	}

	switch (dialogid)
	{
		case DIALOG_BIZX_MENU:
		{
			if (listitem < 0 || listitem >= sizeof (gMenuActs[])) return 1;
			switch (gMenuActs[playerid][listitem])
			{
				case BXA_INFO: BizX_ShowInfo(playerid, b);
				case BXA_STATEMENT: BizS_ShowOwn(playerid, b);
				case BXA_HISTORY: BizS_ShowHistory(playerid, b);
				case BXA_TAX: BizW_ShowTax(playerid, b);
				case BXA_SELLER: BizW_ShowSellerCfg(playerid, b);
				case BXA_STAFF: BizW_ShowStaff(playerid, b);
				case BXA_UPG: BizW_ShowUpg(playerid, b);
				case BXA_CASH:
				{
					Cmd_BizCash(playerid);
				}
				case BXA_STOCK:
				{
					new room = BizX_StockMax(b) - Biz[b][bStock];
					if (room <= 0) { SendClientMessage(playerid, COLOR_GREY, "Склад полон."); return 1; }
					ShowPlayerDialog(playerid, DIALOG_BIZX_STOCK, DIALOG_STYLE_INPUT, "Закупка товара",
						"{FFFFFF}На складе товара на {FFFF00}$%d{FFFFFF}, поместится ещё на {FFFF00}$%d{FFFFFF}.\nТовар на $100 даёт продаж примерно на $%d.\n{FF9900}У оптовиков дороже на %d%%: товар на $100 стоит $%d.\n{AFAFAF}Дешевле — поставки с заводов: водители везут их сами (/contracts).\n{FFFFFF}Платишь наличными, у тебя: $%d\n\nНа сколько товара закупить ($ на складе)?",
						"Закупить", "Назад", Biz[b][bStock], room, 10000 / BIZ_WHOLESALE_PCT, Cfg(CFG_BIZ_AIR), 100 + Cfg(CFG_BIZ_AIR), GetMoney(playerid));
				}
				case BXA_MARKUP:
				{
					ShowPlayerDialog(playerid, DIALOG_BIZX_MARKUP, DIALOG_STYLE_INPUT, "Цены",
						"{FFFFFF}Сейчас цены: {FFFF00}%d%%{FFFFFF} от обычных.\nДороже — больше прибыль с покупки, но покупатели уйдут к соседям.\n\nВведи новые цены в процентах, от %d до %d:",
						"Поставить", "Назад", Biz[b][bMarkup], BIZ_MARKUP_MIN, BIZ_MARKUP_MAX);
				}
				case BXA_NAME:
				{
					ShowPlayerDialog(playerid, DIALOG_BIZX_NAME, DIALOG_STYLE_INPUT, "Название",
						"{FFFFFF}Сейчас: {FF9900}%s{FFFFFF}\nСмена названия стоит ${FFFF00}%d{FFFFFF}.\n\nНовое название (от 3 до 30 символов):",
						"Сменить", "Назад", Biz[b][bName], BIZ_RENAME_PRICE);
				}
				case BXA_LOCK:
				{
					if (BizX_Near(playerid, b)) Cmd_BizLock(playerid);
				}
				case BXA_SELLTO:
				{
					ShowPlayerDialog(playerid, DIALOG_BIZX_SELLTO, DIALOG_STYLE_INPUT, "Продать игроку",
						"{FFFFFF}Покупатель должен стоять рядом с тобой.\nКасса останется тебе, товар на складе перейдёт ему.\n\nВведи id игрока и цену через пробел, например: {FFFF00}12 50000",
						"Предложить", "Назад");
				}
				case BXA_SELL:
				{
					ShowPlayerDialog(playerid, DIALOG_BIZX_SELL, DIALOG_STYLE_MSGBOX, "Продать штату",
						"{FFFFFF}Штат выкупит «%s» за половину цены.\nВместе с кассой ты получишь ${FFFF00}%d{FFFFFF}.\nТовар на складе пропадёт.\n\nПродать?",
						"Продать", "Назад", Biz[b][bName], Biz[b][bPrice] / 2 + Biz[b][bTill]);
				}
			}
			return 1;
		}
		case DIALOG_BIZX_STOCK:
		{
			if (!BizX_UsesStock(b) || !BizX_Near(playerid, b)) return 1;
			if (!IsNumeric(inputtext) || strlen(inputtext) > 9) { SendClientMessage(playerid, COLOR_GREY, "Нужно ввести сумму числом."); return 1; }
			new sum = strval(inputtext), room = BizX_StockMax(b) - Biz[b][bStock];
			if (sum > room) sum = room;
			if (sum < 1) { SendClientMessage(playerid, COLOR_GREY, "Склад полон или сумма слишком мала."); return 1; }
			new cost = sum + sum / 100 * Cfg(CFG_BIZ_AIR) + (sum % 100) * Cfg(CFG_BIZ_AIR) / 100;
			if (GetMoney(playerid) < cost) { SendClientMessage(playerid, COLOR_GREY, "Не хватает наличных. Нужно $%d.", cost); return 1; }
			GiveMoney(playerid, -cost, "закупка товара для бизнеса");
			Biz[b][bStock] += sum;
			BizX_Save(b);
			Account_Save(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Товар на $%d закуплен за $%d. На складе теперь на $%d.", sum, cost, Biz[b][bStock]);
			Cmd_BizMenu(playerid);
			return 1;
		}
		case DIALOG_BIZX_MARKUP:
		{
			if (!IsNumeric(inputtext) || strlen(inputtext) > 4) { SendClientMessage(playerid, COLOR_GREY, "Нужно ввести число."); return 1; }
			new pct = strval(inputtext);
			if (pct < BIZ_MARKUP_MIN || pct > BIZ_MARKUP_MAX)
			{
				SendClientMessage(playerid, COLOR_GREY, "Цены можно поставить от %d до %d%%.", BIZ_MARKUP_MIN, BIZ_MARKUP_MAX);
				return 1;
			}
			Biz[b][bMarkup] = pct;
			BizX_Save(b);
			SendClientMessage(playerid, COLOR_GREEN, "Цены в «%s» теперь %d%% от обычных.", Biz[b][bName], pct);
			Cmd_BizMenu(playerid);
			return 1;
		}
		case DIALOG_BIZX_NAME:
		{
			new name[32], len = strlen(inputtext);
			if (len < 3 || len > 30) { SendClientMessage(playerid, COLOR_GREY, "Название — от 3 до 30 символов."); return 1; }
			for (new i = 0; i < len; i++)
			{
				if (inputtext[i] < ' ' && inputtext[i] > 0 || inputtext[i] == '{' || inputtext[i] == '}' || inputtext[i] == '%' || inputtext[i] == '~')
				{
					SendClientMessage(playerid, COLOR_GREY, "В названии нельзя использовать { } %% ~ и переносы строк.");
					return 1;
				}
			}
			if (GetMoney(playerid) < BIZ_RENAME_PRICE) { SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", BIZ_RENAME_PRICE); return 1; }
			strcat(name, inputtext, sizeof name);
			GiveMoney(playerid, -BIZ_RENAME_PRICE, "смена названия бизнеса");
			Biz[b][bName][0] = '\0';
			strcat(Biz[b][bName], name, 32);
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE businesses SET name = '%q' WHERE id = %d", name, Biz[b][bDbId]));
			Biz_Refresh(b);
			Account_Save(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Бизнес теперь называется «%s».", name);
			return 1;
		}
		case DIALOG_BIZX_SELL:
		{
			Cmd_SellBiz(playerid);
			if (Biz[b][bOwner] == 0)
			{
				Biz[b][bMarkup] = 100;
				Biz[b][bStock] = 0;
				BizX_Save(b);
				BizW_Reset(b);
			}
			return 1;
		}
		case DIALOG_BIZX_SELLTO:
		{
			new pos = 0, price;
			new target = NextPlayerParam(inputtext, pos);
			if (target == INVALID_PLAYER_ID || !NextIntParam(inputtext, pos, price))
			{
				SendClientMessage(playerid, COLOR_GREY, "Введи id игрока и цену через пробел, например: 12 50000");
				return 1;
			}
			if (target == playerid || !IsPlayerConnected(target) || !Player[target][pLogged]) { SendClientMessage(playerid, COLOR_GREY, "Такого игрока нет."); return 1; }
			if (price < 1 || price > BIZ_SELLTO_MAX) { SendClientMessage(playerid, COLOR_GREY, "Цена — от $1 до $%d.", BIZ_SELLTO_MAX); return 1; }
			new Float:x, Float:y, Float:z;
			GetPlayerPos(playerid, x, y, z);
			if (!IsPlayerInRangeOfPoint(target, BIZ_SELLTO_RANGE, x, y, z) || GetPlayerVirtualWorld(target) != GetPlayerVirtualWorld(playerid))
			{
				SendClientMessage(playerid, COLOR_GREY, "Покупатель должен стоять рядом с тобой.");
				return 1;
			}
			if (Biz_GetOwned(target) != -1) { SendClientMessage(playerid, COLOR_GREY, "У этого игрока уже есть бизнес."); return 1; }
			gOfferSeller[target] = Player[playerid][pDbId];
			gOfferBiz[target] = Biz[b][bDbId];
			gOfferPrice[target] = price;
			ShowPlayerDialog(target, DIALOG_BIZX_OFFER, DIALOG_STYLE_MSGBOX, "Покупка бизнеса",
				"{FFFFFF}%s предлагает тебе купить бизнес\n{FF9900}%s{FFFFFF} (%s)\nза ${FFFF00}%d{FFFFFF}.\n\nТовар на складе: $%d. Доход за PayDay: $%d.\n\nКупить?",
				"Купить", "Отказаться", GetRPName(playerid), Biz[b][bName], Biz_GetTypeName(Biz[b][bType]), price, Biz[b][bStock], BizX_PayDayIncome(b));
			SendClientMessage(playerid, COLOR_WHITE, "Ты предложил %s купить «%s» за $%d. Ждём ответа.", GetRPName(target), Biz[b][bName], price);
			return 1;
		}
	}
	return 1;
}

// fuel.inc подключается раньше business.inc и типов бизнеса ещё не знает
stock BizX_FuelSale(playerid, amount)
{
	BizX_ServiceSale(playerid, BIZ_FUEL, amount);
}

// ---------------------------------------------------------------------
//  Поставки с заводов: разгрузка у бизнеса, /contracts, /market
// ---------------------------------------------------------------------
// За рулём с товаром у входа бизнеса (из Factory_TryUnload). 1 — сработало.
stock BizX_TryDeliver(playerid, v, bool:verbose)
{
	new prod = gVehCargoProd[v], best = -1, Float:bestDist = DELIVER_RANGE, Float:d;
	if (GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0)
	{
		if (verbose) SendClientMessage(playerid, COLOR_GREY, "Товар разгружают на улице, у входа в бизнес.");
		return 0;
	}
	for (new b = 0; b < gBizCount; b++)
	{
		if (Biz[b][bOInt] != 0 || Biz[b][bOWorld] != 0) continue;
		d = GetPlayerDistanceFromPoint(playerid, Biz[b][bX], Biz[b][bY], Biz[b][bZ]);
		if (d < bestDist) { bestDist = d; best = b; }
	}
	if (best == -1)
	{
		if (verbose) SendClientMessage(playerid, COLOR_GREY, "Рядом нет бизнеса. В кузове: %s, ящиков %d. Кому нужно: /contracts", gProdName[prod], gVehCargo[v] / CRATE_MATS);
		return 0;
	}
	new b = best;
	if (Prod_OfBizType(Biz[b][bType]) != prod || !BizX_UsesStock(b))
	{
		if (verbose) SendClientMessage(playerid, COLOR_GREY, "«%s» не берёт %s. Кому нужно: /contracts", Biz[b][bName], gProdName[prod]);
		return verbose ? 1 : 0;
	}
	if (Biz[b][bOwner] == 0) { SendClientMessage(playerid, COLOR_GREY, "У «%s» нет владельца: товар принять некому.", Biz[b][bName]); return 1; }
	new value = Cfg(CFG_GOODS_VALUE), pay = Cfg(CFG_GOODS_PAY);
	new n = gVehCargo[v] / CRATE_MATS;
	new room = (BizX_StockMax(b) - Biz[b][bStock]) / value;
	if (room <= 0) { SendClientMessage(playerid, COLOR_GREY, "Склад «%s» полон.", Biz[b][bName]); return 1; }
	if (n > room) n = room;
	if (pay > 0 && n > Biz[b][bTill] / pay) n = Biz[b][bTill] / pay;
	if (n <= 0) { SendClientMessage(playerid, COLOR_GREY, "В кассе «%s» не хватает денег заплатить за товар ($%d за ящик).", Biz[b][bName], pay); return 1; }
	Biz[b][bStock] += n * value;
	Biz[b][bTill] -= n * pay;
	gVehCargo[v] -= n * CRATE_MATS;
	BizX_Save(b);
	if (n * pay > 0) GiveMoney(playerid, n * pay, "доставка товара в бизнес");
	SendClientMessage(playerid, COLOR_GREEN, "Разгружено в «%s»: %d ящ. (%s). Оплата: $%d. В кузове осталось: %d.", Biz[b][bName], n, gProdName[prod], n * pay, gVehCargo[v] / CRATE_MATS);
	return 1;
}

stock bool:Cmd_Contracts(playerid)
{
	new body[3600], line[128], n = 0, pay = Cfg(CFG_GOODS_PAY), value = Cfg(CFG_GOODS_VALUE);
	strcat(body, "Куда\tТовар\tЯщиков\tЦена\n");
	for (new b = 0; b < gBizCount && n < CONTRACT_ROWS; b++)
	{
		new pr = Prod_OfBizType(Biz[b][bType]);
		if (pr < 0 || Biz[b][bOwner] == 0 || !BizX_UsesStock(b) || Biz[b][bOInt] != 0 || Biz[b][bOWorld] != 0) continue;
		new need = (BizX_StockMax(b) - Biz[b][bStock]) / value;
		if (pay > 0 && need > Biz[b][bTill] / pay) need = Biz[b][bTill] / pay;
		if (need <= 0 || Biz[b][bStock] * 100 > BizX_StockMax(b) * 60) continue;     // берут, когда склад меньше 60%
		format(line, sizeof line, "{FFFFFF}%s\t%s\t%d\t{33AA33}+$%d\n", Biz[b][bName], gProdName[pr], need, pay);
		if (strlen(body) + strlen(line) >= sizeof body) break;
		strcat(body, line);
		gCtrMap[playerid][n++] = b;
	}
	new nb = n;
	for (new i = 0; i < MAX_FACTORIES && n < CONTRACT_ROWS; i++)
	{
		if (!Factory[i][fcUsed] || Factory[i][fcProd] == PROD_MATS || Factory[i][fcStock] < CRATE_MATS) continue;
		format(line, sizeof line, "{AFAFAF}Завод %s\t%s\t%d\t{FF9900}-$%d\n", Factory[i][fcName], gProdName[Factory[i][fcProd]], Factory[i][fcStock] / CRATE_MATS, Cfg(CFG_GOODS_PRICE));
		if (strlen(body) + strlen(line) >= sizeof body) break;
		strcat(body, line);
		gCtrMap[playerid][n++] = 1000 + i;
	}
	for (new k = n; k < CONTRACT_ROWS; k++) gCtrMap[playerid][k] = -1;
	if (n == 0) return SendClientMessage(playerid, COLOR_GREY, "Сейчас заказов нет: склады бизнесов полны, а на заводах нет товара.");
	if (nb == 0) SendClientMessage(playerid, COLOR_GREY, "Бизнесам товар сейчас не нужен, но на заводах он есть.");
	ShowPlayerDialog(playerid, DIALOG_CONTRACTS, DIALOG_STYLE_TABLIST_HEADERS, "Поставки: выбери точку для GPS", "%s", "GPS", "Закрыть", body);
	return true;
}

stock bool:Cmd_Market(playerid)
{
	new body[1200], line[160];
	strcat(body, "Товар\tНа складах\tЦены\tЗаводов\n");
	for (new p = PROD_FUEL; p < PROD_COUNT; p++)
	{
		new fill = Market_Fill(p), pct = Market_PricePct(p), fc = 0;
		for (new i = 0; i < MAX_FACTORIES; i++)
		{
			if (Factory[i][fcUsed] && !Factory[i][fcClosed] && Faction_IsValid(Factory[i][fcOwner]) && Factory[i][fcProd] == p) fc++;
		}
		new st[24], pr[24];
		if (fill < 0) format(st, sizeof st, "{AFAFAF}-");
		else format(st, sizeof st, "%s%d%%", (fill < Cfg(CFG_MKT_LOW)) ? ("{FF6347}") : ("{FFFFFF}"), fill);
		if (pct > 0) format(pr, sizeof pr, "{FF6347}+%d%% дефицит", pct);
		else if (pct < 0) format(pr, sizeof pr, "{33AA33}%d%% избыток", pct);
		else format(pr, sizeof pr, "{FFFFFF}обычные");
		format(line, sizeof line, "%s\t%s\t%s\t{FFFFFF}%d\n", gProdName[p], st, pr, fc);
		strcat(body, line);
	}
	ShowPlayerDialog(playerid, DIALOG_MARKET, DIALOG_STYLE_TABLIST_HEADERS, "Рынок города", "%s", "Поставки", "Закрыть", body);
	return true;
}

stock Market_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid == DIALOG_MARKET)
	{
		if (response) Cmd_Contracts(playerid);
		return 1;
	}
	if (dialogid != DIALOG_CONTRACTS) return 0;
	if (!response || listitem < 0 || listitem >= CONTRACT_ROWS || gCtrMap[playerid][listitem] == -1) return 1;
	new k = gCtrMap[playerid][listitem];
	if (k >= 1000)
	{
		new i = k - 1000;
		if (i >= MAX_FACTORIES || !Factory[i][fcUsed]) return 1;
		new p = (gFcSet[i][FP_STORE] && gFcI[i][FP_STORE] == 0 && gFcW[i][FP_STORE] == 0) ? FP_STORE : FP_OWNER;
		Gps_SetPoint(playerid, gFcX[i][p], gFcY[i][p], gFcZ[i][p], Factory[i][fcName]);
		SendClientMessage(playerid, COLOR_WHITE, "GPS: завод «%s». Склад ящиков — ALT, ящик в грузовик или фургон сзади — ALT.", Factory[i][fcName]);
		return 1;
	}
	if (k >= gBizCount) return 1;
	Gps_SetPoint(playerid, Biz[k][bX], Biz[k][bY], Biz[k][bZ], Biz[k][bName]);
	SendClientMessage(playerid, COLOR_WHITE, "GPS: «%s». Подъедь ко входу и разгрузи товар: H или /unload.", Biz[k][bName]);
	return 1;
}
