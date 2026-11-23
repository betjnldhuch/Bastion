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
//  Игрокам:  /bizmenu (/biz) — меню своего бизнеса, /biztypes — виды бизнесов и цены.
//  Меню: информация, касса, закупка товара, цены, название, вход, продажа игроку / штату.
//
//  Все цифры ниже — настройки экономики, их можно менять не трогая код.
// =====================================================================

#define BIZ_WHOLESALE_PCT   50          // закупочная цена товара, % от базовой розничной
#define BIZ_SALE_TAX        10          // налог с каждой продажи, %
#define BIZ_MARKUP_MIN      70          // границы цен владельца, % от базовых
#define BIZ_MARKUP_MAX      150
#define BIZ_START_STOCK     2000        // товар на складе при покупке бизнеса у штата, $
#define BIZ_SERVICE_RANGE   80.0        // в каком радиусе АЗС / СТО считаются «своими» для бизнеса
#define BIZ_RENAME_PRICE    1000        // смена названия
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
	{ 25000,  100, 10000 }      // 14 лотерея
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
static gMenuActs[MAX_PLAYERS][10];
static gStyle[MAX_PLAYERS];                     // стиль боя
static gStyleAcc[MAX_PLAYERS];                  // для какого аккаунта он загружен
static gOfferSeller[MAX_PLAYERS];               // предложение купить бизнес: аккаунт продавца,
static gOfferBiz[MAX_PLAYERS];                  // номер бизнеса в базе
static gOfferPrice[MAX_PLAYERS];                // и цена

enum
{
	BXA_INFO,
	BXA_STATEMENT,
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

// Цена товара в бизнесе b с наценкой владельца
stock BizX_Price(b, base)
{
	if (Biz[b][bOwner] == 0 || Biz[b][bMarkup] == 100) return base;
	new price = base * Biz[b][bMarkup] / 100;
	return (price < 1) ? 1 : price;
}

stock BizX_StockMax(b)
{
	new t = Biz[b][bType];
	return (t > BIZ_NONE && t < BIZ_TYPES) ? gBizCfg[t][bcStockMax] : 0;
}

stock BizX_Save(b)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE businesses SET till = %d, markup = %d, stock = %d, earned = %d WHERE id = %d",
		Biz[b][bTill], Biz[b][bMarkup], Biz[b][bStock], Biz[b][bEarned], Biz[b][bDbId]));
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
	if (b < 0 || b >= gBizCount || Biz[b][bOwner] == 0) return;
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
	if (BizX_UsesStock(b) && Biz[b][bStock] <= 0) return;      // пустой магазин клиентов не держит
	BizX_AddTill(b, gBizCfg[t][bcPayDay]);
	BizX_Save(b);
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
	DB_AddColumn("accounts", "fstyle", "INTEGER DEFAULT 4");
	BizS_Init();
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

	r = DB_ExecuteQuery(gDB, "SELECT id, markup, stock, earned FROM businesses");
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
	if (b == -1 || Player[playerid][pDbId] == 0 || Biz[b][bOwner] != Player[playerid][pDbId]) return -1;
	return b;
}

static bool:BizX_Near(playerid, b)
{
	if (Biz_GetNearby(playerid) == b) return true;
	SendClientMessage(playerid, COLOR_GREY, "Для этого подойди к своему бизнесу.");
	return false;
}

stock bool:Cmd_BizMenu(playerid)
{
	new b = Biz_GetOwned(playerid);
	if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет бизнеса. Купить: ALT у значка бизнеса, который продаётся. Виды бизнесов: /biztypes");
	gMenuBiz[playerid] = Biz[b][bDbId];

	new list[512], line[96], n = 0;
	strcat(list, "Действие\tСейчас\n");
	gMenuActs[playerid][n++] = BXA_INFO; strcat(list, "Информация\t \n");
	gMenuActs[playerid][n++] = BXA_STATEMENT; strcat(list, "Финансовая выписка\tпоказать: /showbiz [id]\n");
	format(line, sizeof line, "Забрать деньги из кассы\t{33AA33}$%d\n", Biz[b][bTill]);
	gMenuActs[playerid][n++] = BXA_CASH; strcat(list, line);
	if (BizX_UsesStock(b))
	{
		format(line, sizeof line, "Закупить товар\t%s$%d{FFFFFF} из $%d\n", (Biz[b][bStock] > 0) ? ("{FFFFFF}") : ("{FF6347}"), Biz[b][bStock], gBizCfg[Biz[b][bType]][bcStockMax]);
		gMenuActs[playerid][n++] = BXA_STOCK; strcat(list, line);
		format(line, sizeof line, "Цены\t%d%%\n", Biz[b][bMarkup]);
		gMenuActs[playerid][n++] = BXA_MARKUP; strcat(list, line);
	}
	format(line, sizeof line, "Сменить название\t$%d\n", BIZ_RENAME_PRICE);
	gMenuActs[playerid][n++] = BXA_NAME; strcat(list, line);
	if (Biz[b][bInt] != 0)
	{
		gMenuActs[playerid][n++] = BXA_LOCK;
		strcat(list, Biz[b][bLocked] ? ("Открыть вход\t{FF6347}закрыто\n") : ("Закрыть вход\t{33AA33}открыто\n"));
	}
	gMenuActs[playerid][n++] = BXA_SELLTO; strcat(list, "Продать игроку\t \n");
	format(line, sizeof line, "Продать штату\t$%d\n", Biz[b][bPrice] / 2 + Biz[b][bTill]);
	gMenuActs[playerid][n++] = BXA_SELL; strcat(list, line);
	for (new i = n; i < sizeof (gMenuActs[]); i++) gMenuActs[playerid][i] = -1;

	ShowPlayerDialog(playerid, DIALOG_BIZX_MENU, DIALOG_STYLE_TABLIST_HEADERS, Biz_IconCaption(Biz[b][bType], Biz[b][bName]), "%s", "Выбрать", "Закрыть", list);
	return true;
}

static BizX_ShowInfo(playerid, b)
{
	new text[900], line[160], t = Biz[b][bType];
	format(text, sizeof text, "{FF9900}%s{FFFFFF}, №%d\nВид: %s\nЦена у штата: $%d\n\nВ кассе: {33AA33}$%d{FFFFFF}\nЗаработано за всё время: $%d\nДоход за PayDay: $%d\n",
		Biz[b][bName], Biz[b][bDbId], Biz_GetTypeName(t), Biz[b][bPrice], Biz[b][bTill], Biz[b][bEarned], gBizCfg[t][bcPayDay]);
	if (BizX_UsesStock(b))
	{
		format(line, sizeof line, "Товар на складе: $%d из $%d\nЦены: %d%% от обычных\n\n", Biz[b][bStock], gBizCfg[t][bcStockMax], Biz[b][bMarkup]);
		strcat(text, line);
		format(line, sizeof line, "{AFAFAF}С каждой покупки в кассу идёт её цена минус налог %d%%,\nа со склада уходит %d%% обычной цены товара.\n", BIZ_SALE_TAX, BIZ_WHOLESALE_PCT);
		strcat(text, line);
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
				case BXA_CASH:
				{
					Cmd_BizCash(playerid);
				}
				case BXA_STOCK:
				{
					new room = gBizCfg[Biz[b][bType]][bcStockMax] - Biz[b][bStock];
					if (room <= 0) { SendClientMessage(playerid, COLOR_GREY, "Склад полон."); return 1; }
					ShowPlayerDialog(playerid, DIALOG_BIZX_STOCK, DIALOG_STYLE_INPUT, "Закупка товара",
						"{FFFFFF}На складе товара на {FFFF00}$%d{FFFFFF}, поместится ещё на {FFFF00}$%d{FFFFFF}.\nТовар на $100 даёт продаж примерно на $%d.\nПлатишь наличными, у тебя: $%d\n\nНа какую сумму закупить?",
						"Закупить", "Назад", Biz[b][bStock], room, 10000 / BIZ_WHOLESALE_PCT, GetMoney(playerid));
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
			new sum = strval(inputtext), room = gBizCfg[Biz[b][bType]][bcStockMax] - Biz[b][bStock];
			if (sum > room) sum = room;
			if (sum < 1) { SendClientMessage(playerid, COLOR_GREY, "Склад полон или сумма слишком мала."); return 1; }
			if (GetMoney(playerid) < sum) { SendClientMessage(playerid, COLOR_GREY, "Не хватает наличных. Нужно $%d.", sum); return 1; }
			GiveMoney(playerid, -sum, "закупка товара для бизнеса");
			Biz[b][bStock] += sum;
			BizX_Save(b);
			Account_Save(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Товар закуплен на $%d. На складе теперь на $%d.", sum, Biz[b][bStock]);
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
				"Купить", "Отказаться", GetRPName(playerid), Biz[b][bName], Biz_GetTypeName(Biz[b][bType]), price, Biz[b][bStock], gBizCfg[Biz[b][bType]][bcPayDay]);
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
