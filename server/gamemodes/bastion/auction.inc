// =====================================================================
//  auction.inc — аукцион домов: /auction (/auctions), /bid, /unauction.
//
//  Владелец у своего дома: /auction [стартовая цена] — выставить дом на
//  торги на AUC_HOURS часов. Дом не продать и не сдать, пока идут торги.
//  Любой игрок: /bid [№ дома] [сумма] — ставка. Шаг не меньше AUC_STEP%
//  от текущей (или стартовой) цены, деньги сразу списываются и ждут.
//  Перебили — деньги возвращаются сразу. Ставка в последние AUC_EXTEND_MIN
//  минут продлевает торги на столько же («антиснайпер»).
//
//  Конец торгов: дом переходит победителю, продавцу идёт сумма минус
//  AUC_TAX% в казну штата. Ставок не было — дом остаётся владельцу.
//  Снять с торгов до первой ставки: /unauction.
//
//  Государство само выставляет на торги дома, отобранные за долг по налогу
//  (houses.inc): стартовая цена — половина цены дома, деньги идут в казну.
//
//  /auction без параметров — список активных торгов.
//  Хранится: таблица auctions (дом, продавец, цены, лидер, конец).
// =====================================================================

#define AUC_MAX             30
#define AUC_HOURS           12
#define AUC_STEP            5       // % шага ставки
#define AUC_TAX             5       // % в казну с проданного дома
#define AUC_EXTEND_MIN      10      // ставка в последние N минут продлевает торги
#define AUC_MIN_PRICE       1000

enum E_AUC
{
	aUsed,
	aDb,                // номер в таблице auctions
	aHouse,             // слот дома
	aSeller,            // id аккаунта продавца (0 — государство)
	aSellerName[MAX_PLAYER_NAME],
	aStart,             // стартовая цена
	aBid,               // текущая ставка (0 — ставок не было)
	aBidder,            // id аккаунта лидера
	aBidderName[MAX_PLAYER_NAME],
	aEnds
}
static Auc[AUC_MAX][E_AUC];
static gAucList[MAX_PLAYERS][AUC_MAX];

stock Auction_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS auctions (id INTEGER PRIMARY KEY AUTOINCREMENT, house INTEGER, seller INTEGER, seller_name TEXT, start INTEGER, bid INTEGER DEFAULT 0, bidder INTEGER DEFAULT 0, bidder_name TEXT DEFAULT '', ends INTEGER, state INTEGER DEFAULT 0)"));
	for (new a = 0; a < AUC_MAX; a++) Auc[a][aUsed] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, house, seller, seller_name, start, bid, bidder, bidder_name, ends FROM auctions WHERE state = 0");
	if (r == DBResult:0) return;
	new rows = DB_GetRowCount(r), a = 0;
	for (new i = 0; i < rows && a < AUC_MAX; i++)
	{
		new h = House_SlotByDbId(DB_GetFieldInt(r, 1));
		if (h != -1)
		{
			Auc[a][aUsed] = 1;
			Auc[a][aDb] = DB_GetFieldInt(r, 0);
			Auc[a][aHouse] = h;
			Auc[a][aSeller] = DB_GetFieldInt(r, 2);
			DB_GetFieldString(r, 3, Auc[a][aSellerName], MAX_PLAYER_NAME);
			Auc[a][aStart] = DB_GetFieldInt(r, 4);
			Auc[a][aBid] = DB_GetFieldInt(r, 5);
			Auc[a][aBidder] = DB_GetFieldInt(r, 6);
			DB_GetFieldString(r, 7, Auc[a][aBidderName], MAX_PLAYER_NAME);
			Auc[a][aEnds] = DB_GetFieldInt(r, 8);
			a++;
		}
		else DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE auctions SET state = 2 WHERE id = %d", DB_GetFieldInt(r, 0)));
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	// Ставки живут только в деньгах игроков: после перезапуска вернуть их лидерам и начать торги со стартовой цены
	for (new i = 0; i < AUC_MAX; i++)
	{
		if (!Auc[i][aUsed] || Auc[i][aBid] == 0) continue;
		Auction_Refund(Auc[i][aBidder], Auc[i][aBid], "аукцион: сервер перезапущен");
		Auc[i][aBid] = 0;
		Auc[i][aBidder] = 0;
		Auc[i][aBidderName][0] = '\0';
		Auction_Save(i);
	}
	printf("[Auction] Loaded %d auctions.", a);
}

static Auction_Save(a)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE auctions SET bid = %d, bidder = %d, bidder_name = '%q', ends = %d WHERE id = %d",
		Auc[a][aBid], Auc[a][aBidder], Auc[a][aBidderName], Auc[a][aEnds], Auc[a][aDb]));
}

// Вернуть деньги игроку: в игре — на руки, иначе прямо в базу аккаунта
static Auction_Refund(accountId, amount, const reason[])
{
	if (accountId <= 0 || amount <= 0) return;
	new p = Auction_PlayerByDb(accountId);
	if (p != INVALID_PLAYER_ID)
	{
		GiveMoney(p, amount, reason);
		Account_Save(p);
		SendClientMessage(p, COLOR_WHITE, "Возврат $%d (%s).", amount, reason);
		return;
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET money = money + %d WHERE id = %d", amount, accountId));
}

static Auction_PlayerByDb(accountId)
{
	if (accountId <= 0) return INVALID_PLAYER_ID;
	for (new i = 0; i < MAX_PLAYERS; i++)
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pDbId] == accountId) return i;
	return INVALID_PLAYER_ID;
}

// Аукцион дома (слот), -1 — нет
stock Auction_OfHouse(h)
{
	for (new a = 0; a < AUC_MAX; a++) if (Auc[a][aUsed] && Auc[a][aHouse] == h) return a;
	return -1;
}

stock bool:Auction_IsOnSale(h) { return Auction_OfHouse(h) != -1; }

static Auction_Free()
{
	for (new a = 0; a < AUC_MAX; a++) if (!Auc[a][aUsed]) return a;
	return -1;
}

// Минимальная следующая ставка
static Auction_NextBid(a)
{
	if (Auc[a][aBid] == 0) return Auc[a][aStart];
	new step = Auc[a][aBid] * AUC_STEP / 100;
	if (step < 100) step = 100;
	return Auc[a][aBid] + step;
}

static Auction_Left(dest[], size, ends)
{
	new left = ends - gettime();
	if (left < 0) left = 0;
	if (left >= 3600) format(dest, size, "%d ч. %d мин.", left / 3600, (left % 3600) / 60);
	else format(dest, size, "%d мин.", (left + 59) / 60);
}

// Начать торги. seller = 0 — от государства
static Auction_Open(h, seller, const sellerName[], start, bool:announce = true)
{
	new a = Auction_Free();
	if (a == -1) return -1;
	Auc[a][aUsed] = 1;
	Auc[a][aHouse] = h;
	Auc[a][aSeller] = seller;
	Auc[a][aSellerName][0] = '\0';
	strcat(Auc[a][aSellerName], sellerName, MAX_PLAYER_NAME);
	Auc[a][aStart] = start;
	Auc[a][aBid] = 0;
	Auc[a][aBidder] = 0;
	Auc[a][aBidderName][0] = '\0';
	Auc[a][aEnds] = gettime() + AUC_HOURS * 3600;
	new DBResult:r = DB_ExecuteQuery(gDB, "INSERT INTO auctions (house, seller, seller_name, start, ends) VALUES (%d, %d, '%q', %d, %d)",
		House[h][hDbId], seller, Auc[a][aSellerName], start, Auc[a][aEnds]);
	DB_FreeResultSet(r);
	r = DB_ExecuteQuery(gDB, "SELECT last_insert_rowid()");
	if (r != DBResult:0) { if (DB_GetRowCount(r) > 0) Auc[a][aDb] = DB_GetFieldInt(r, 0); DB_FreeResultSet(r); }
	if (announce)
		SendClientMessageToAll(COLOR_ORANGE, "[Аукцион] Дом №%d выставлен на торги: стартовая цена $%d, %d ч. Ставка: /bid %d [сумма]. Все торги: /auction",
			House[h][hDbId], start, AUC_HOURS, House[h][hDbId]);
	return a;
}

// Дом отобрало государство за долг (houses.inc): на торги с половины цены
stock Auction_StateHouse(h)
{
	if (Auction_OfHouse(h) != -1) return;
	new start = House[h][hPrice] / 2;
	if (start < AUC_MIN_PRICE) start = AUC_MIN_PRICE;
	Auction_Open(h, 0, "Штат", start);
}

static Auction_Close(a)
{
	new h = Auc[a][aHouse], msg[200];
	if (Auc[a][aBid] > 0 && Auc[a][aBidder] > 0)
	{
		new winner = Auction_PlayerByDb(Auc[a][aBidder]);
		// победитель за это время набрал лимит домов — деньги назад, дом остаётся продавцу
		if (winner != INVALID_PLAYER_ID && House_CountOwned(winner) >= Props_MaxHouses(winner))
		{
			Auction_Refund(Auc[a][aBidder], Auc[a][aBid], "аукцион: лимит домов");
			format(msg, sizeof msg, "[Аукцион] Дом №%d не продан: у победителя лимит домов. Деньги возвращены.", House[h][hDbId]);
			SendClientMessageToAll(COLOR_ORANGE, "%s", msg);
		}
		else
		{
			new tax = Auc[a][aBid] * AUC_TAX / 100, pay = Auc[a][aBid] - tax;
			Econ_ToTreasury(tax);
			if (Auc[a][aSeller] > 0) Auction_Refund(Auc[a][aSeller], pay, "аукцион: продажа дома");
			else Econ_ToTreasury(pay);
			House_GiveToAccount(h, Auc[a][aBidder], Auc[a][aBidderName]);
			if (winner != INVALID_PLAYER_ID)
			{
				SendClientMessage(winner, COLOR_GREEN, "[Аукцион] Ты выиграл торги! Дом №%d твой за $%d. Войти: /enter, меню: /house", House[h][hDbId], Auc[a][aBid]);
				Account_SetNormalSpawn(winner);
			}
			new seller = Auction_PlayerByDb(Auc[a][aSeller]);
			if (seller != INVALID_PLAYER_ID)
				SendClientMessage(seller, COLOR_GREEN, "[Аукцион] Дом №%d продан за $%d. Тебе: $%d (налог %d%%).", House[h][hDbId], Auc[a][aBid], pay, AUC_TAX);
			format(msg, sizeof msg, "[Аукцион] Дом №%d продан: %s за $%d.", House[h][hDbId], Auc[a][aBidderName], Auc[a][aBid]);
			SendClientMessageToAll(COLOR_ORANGE, "%s", msg);
		}
	}
	else
	{
		new seller = Auction_PlayerByDb(Auc[a][aSeller]);
		if (seller != INVALID_PLAYER_ID) SendClientMessage(seller, COLOR_GREY, "[Аукцион] Торги по дому №%d кончились без ставок. Дом остался у тебя.", House[h][hDbId]);
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE auctions SET state = 1, bid = %d, bidder = %d, bidder_name = '%q' WHERE id = %d",
		Auc[a][aBid], Auc[a][aBidder], Auc[a][aBidderName], Auc[a][aDb]));
	Auc[a][aUsed] = 0;
	House_Refresh(h);
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
static Auction_List(playerid)
{
	new text[2000], line[180], left[24], n = 0;
	strcat(text, "Дом\tЦена\tЛидер\tОсталось\n");
	for (new a = 0; a < AUC_MAX; a++)
	{
		if (!Auc[a][aUsed]) continue;
		new h = Auc[a][aHouse];
		Auction_Left(left, sizeof left, Auc[a][aEnds]);
		new lead[MAX_PLAYER_NAME + 4];
		if (Auc[a][aBid] > 0) format(lead, sizeof lead, "%s", Auc[a][aBidderName]);
		else format(lead, sizeof lead, "ставок нет");
		format(line, sizeof line, "№%d (%s)\t$%d\t%s\t%s\n", House[h][hDbId], Auc[a][aSellerName],
			(Auc[a][aBid] > 0) ? Auc[a][aBid] : Auc[a][aStart], lead, left);
		strcat(text, line);
		gAucList[playerid][n] = a;
		n++;
	}
	for (new i = n; i < AUC_MAX; i++) gAucList[playerid][i] = -1;
	if (n == 0) { SendClientMessage(playerid, COLOR_GREY, "[Аукцион] Сейчас торгов нет. Выставить свой дом: /auction [цена] у дома."); return; }
	ShowPlayerDialog(playerid, DIALOG_AUCTION_LIST, DIALOG_STYLE_TABLIST_HEADERS, "Аукцион домов", "%s", "Подробнее", "Закрыть", text);
}

static Auction_ShowOne(playerid, a)
{
	if (a < 0 || a >= AUC_MAX || !Auc[a][aUsed]) return;
	new h = Auc[a][aHouse], left[24], title[64], bid[24], lead[MAX_PLAYER_NAME + 4];
	Auction_Left(left, sizeof left, Auc[a][aEnds]);
	if (Auc[a][aBid] > 0)
	{
		format(bid, sizeof bid, "$%d", Auc[a][aBid]);
		format(lead, sizeof lead, "%s", Auc[a][aBidderName]);
	}
	else
	{
		format(bid, sizeof bid, "ставок нет");
		format(lead, sizeof lead, "-");
	}
	format(title, sizeof title, "Аукцион: дом №%d", House[h][hDbId]);
	ShowPlayerDialog(playerid, DIALOG_AUCTION_ONE, DIALOG_STYLE_MSGBOX, title,
		"{FFFFFF}Продавец: {33CCFF}%s{FFFFFF}\nЦена дома: $%d (%s)\nСтартовая цена: $%d\nТекущая ставка: {FFFF00}%s{FFFFFF}\nЛидер: %s\nДо конца: {FFFF00}%s{FFFFFF}\n\nСледующая ставка от {33AA33}$%d{FFFFFF}: /bid %d %d\nМетка на карте: /gps > Дома",
		"Закрыть", "", Auc[a][aSellerName], House[h][hPrice], House_KindName(h), Auc[a][aStart],
		bid, lead, left, Auction_NextBid(a), House[h][hDbId], Auction_NextBid(a));
}

static Auction_Start(playerid, const params[])
{
	if (strlen(params) == 0) { Auction_List(playerid); return; }
	if (!IsNumeric(params)) { SendClientMessage(playerid, COLOR_GREY, "Используй: /auction [стартовая цена] — стоя у своего дома. Список торгов: /auction"); return; }
	new h = House_GetNearby(playerid);
	if (h == -1) { SendClientMessage(playerid, COLOR_GREY, "Подойди к своему дому."); return; }
	if (House[h][hOwner] != Player[playerid][pDbId]) { SendClientMessage(playerid, COLOR_GREY, "Это не твой дом."); return; }
	if (Auction_OfHouse(h) != -1) { SendClientMessage(playerid, COLOR_GREY, "Этот дом уже на торгах. Снять: /unauction"); return; }
	new start = strval(params);
	if (start < AUC_MIN_PRICE) { SendClientMessage(playerid, COLOR_GREY, "Стартовая цена от $%d.", AUC_MIN_PRICE); return; }
	if (Auction_Free() == -1) { SendClientMessage(playerid, COLOR_GREY, "Слишком много торгов сразу. Попробуй позже."); return; }
	new name[MAX_PLAYER_NAME];
	GetPlayerName(playerid, name, sizeof name);
	new a = Auction_Open(h, Player[playerid][pDbId], name, start);
	if (a == -1) return;
	SendClientMessage(playerid, COLOR_GREEN, "[Аукцион] Дом №%d выставлен на %d ч. Пока идут торги, дом нельзя продать или сдать.", House[h][hDbId], AUC_HOURS);
}

static Auction_Cancel(playerid)
{
	new h = House_GetNearby(playerid);
	if (h == -1) h = House_GetOwned(playerid);
	if (h == -1) { SendClientMessage(playerid, COLOR_GREY, "У тебя нет дома на торгах."); return; }
	new a = Auction_OfHouse(h);
	if (a == -1 || Auc[a][aSeller] != Player[playerid][pDbId]) { SendClientMessage(playerid, COLOR_GREY, "Этот дом не на твоих торгах."); return; }
	if (Auc[a][aBid] > 0) { SendClientMessage(playerid, COLOR_GREY, "Ставка уже сделана: торги не отменить."); return; }
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE auctions SET state = 2 WHERE id = %d", Auc[a][aDb]));
	Auc[a][aUsed] = 0;
	SendClientMessage(playerid, COLOR_WHITE, "[Аукцион] Дом №%d снят с торгов.", House[h][hDbId]);
}

static Auction_Bid(playerid, const params[])
{
	if (Debts_BlockBuy(playerid)) return;
	new pos = 0, dbid, sum;
	if (!NextIntParam(params, pos, dbid) || !NextIntParam(params, pos, sum))
	{
		SendClientMessage(playerid, COLOR_GREY, "Используй: /bid [№ дома] [сумма]. Список торгов: /auction");
		return;
	}
	new h = House_SlotByDbId(dbid), a = (h == -1) ? -1 : Auction_OfHouse(h);
	if (a == -1) { SendClientMessage(playerid, COLOR_GREY, "Этот дом не на торгах. Список: /auction"); return; }
	if (Auc[a][aSeller] == Player[playerid][pDbId]) { SendClientMessage(playerid, COLOR_GREY, "Нельзя торговаться за свой дом."); return; }
	if (Auc[a][aBidder] == Player[playerid][pDbId]) { SendClientMessage(playerid, COLOR_GREY, "Ты и так лидер торгов."); return; }
	if (House_CountOwned(playerid) >= Props_MaxHouses(playerid))
	{
		SendClientMessage(playerid, COLOR_GREY, "У тебя уже %d дом(а) — это лимит. Продай один (/sellhouse) или повысь VIP.", House_CountOwned(playerid));
		return;
	}
	new need = Auction_NextBid(a);
	if (sum < need) { SendClientMessage(playerid, COLOR_GREY, "Ставка должна быть от $%d (шаг %d%%).", need, AUC_STEP); return; }
	if (GetMoney(playerid) < sum) { SendClientMessage(playerid, COLOR_GREY, "Не хватает денег: нужно $%d.", sum); return; }

	// вернуть деньги прошлому лидеру
	if (Auc[a][aBid] > 0) Auction_Refund(Auc[a][aBidder], Auc[a][aBid], "аукцион: ставку перебили");
	GiveMoney(playerid, -sum, "аукцион: ставка");
	Account_Save(playerid);
	Auc[a][aBid] = sum;
	Auc[a][aBidder] = Player[playerid][pDbId];
	Auc[a][aBidderName][0] = '\0';
	GetPlayerName(playerid, Auc[a][aBidderName], MAX_PLAYER_NAME);
	// ставка под конец — продлить торги
	if (Auc[a][aEnds] - gettime() < AUC_EXTEND_MIN * 60)
	{
		Auc[a][aEnds] = gettime() + AUC_EXTEND_MIN * 60;
		SendClientMessageToAll(COLOR_ORANGE, "[Аукцион] Дом №%d: торги продлены на %d мин.", dbid, AUC_EXTEND_MIN);
	}
	Auction_Save(a);
	new left[24];
	Auction_Left(left, sizeof left, Auc[a][aEnds]);
	SendClientMessageToAll(COLOR_ORANGE, "[Аукцион] Дом №%d: ставка $%d (%s). До конца %s. Перебить: /bid %d %d",
		dbid, sum, Auc[a][aBidderName], left, dbid, Auction_NextBid(a));
}

stock bool:Auction_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/auction", true) || !strcmp(cmd, "/auctions", true)) { Auction_Start(playerid, params); return true; }
	if (!strcmp(cmd, "/unauction", true)) { Auction_Cancel(playerid); return true; }
	if (!strcmp(cmd, "/bid", true)) { Auction_Bid(playerid, params); return true; }
	return false;
}

stock Auction_OnDialog(playerid, dialogid, response, listitem)
{
	switch (dialogid)
	{
		case DIALOG_AUCTION_LIST:
		{
			if (response && listitem >= 0 && listitem < AUC_MAX && gAucList[playerid][listitem] != -1)
				Auction_ShowOne(playerid, gAucList[playerid][listitem]);
			return 1;
		}
		case DIALOG_AUCTION_ONE: return 1;
	}
	return 0;
}

// Раз в минуту (rp.pwn)
stock Auction_OnMinute()
{
	new now = gettime();
	for (new a = 0; a < AUC_MAX; a++)
	{
		if (!Auc[a][aUsed]) continue;
		if (House[Auc[a][aHouse]][hDbId] == 0) { Auc[a][aUsed] = 0; continue; }
		if (now >= Auc[a][aEnds]) Auction_Close(a);
	}
}
