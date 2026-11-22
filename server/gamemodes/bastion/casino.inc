// =====================================================================
//  casino.inc — казино и лотерея штата.
//
//  Казино — бизнес типа 5 (/createbiz 5 [цена] [название]). Внутри ALT (или /buy):
//    рулетка на цвет (x2), рулетка на число 0-36 (x36), кости против крупье (x2).
//  Ставка от CASINO_MIN_BET до CASINO_MAX_BET. С каждой проигранной ставки
//  CASINO_OWNER_PCT% идёт владельцу в кассу (без владельца — в казну штата).
//
//  Лотерея — ларёк (/place lottery), ALT: билет за LOTTO_PRICE с числом 1-LOTTO_MAX.
//  Розыгрыш каждый день в LOTTO_HOUR:00. Угадавшие делят джекпот (деньги на счёт
//  в банке, даже если не в игре). Никто не угадал — джекпот копится.
//  /lotto — джекпот и твои билеты.
// =====================================================================

#define CASINO_MIN_BET      100
#define CASINO_MAX_BET      50000
#define CASINO_OWNER_PCT    20
#define CASINO_SPIN_SEC     3

#define LOTTO_PRICE         200
#define LOTTO_MAX           50
#define LOTTO_TICKETS       5           // билетов на одного игрока в розыгрыш
#define LOTTO_HOUR          20
#define LOTTO_JACKPOT_PCT   80          // % цены билета в джекпот, остальное — в казну
#define LOTTO_START         10000       // стартовый джекпот

enum
{
	CG_NONE,
	CG_COLOR,       // рулетка на цвет
	CG_NUMBER,      // рулетка на число
	CG_DICE         // кости
}

static gCasBiz[MAX_PLAYERS] = { -1, ... };
static gCasGame[MAX_PLAYERS];
static gCasBet[MAX_PLAYERS];
static gCasPick[MAX_PLAYERS];           // цвет (0 красное, 1 чёрное) или число
static gCasSpin[MAX_PLAYERS];           // секунд до результата
static gLottoJackpot = LOTTO_START;
static gLottoDraw = 1;                  // номер текущего розыгрыша
static gLottoLastDay = -1;

static const gRouletteRed[] = { 1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36 };

stock Casino_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS lottery (id INTEGER PRIMARY KEY, jackpot INTEGER, draw INTEGER, lastday INTEGER DEFAULT -1)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS lottery_tickets (draw INTEGER, owner INTEGER, number INTEGER)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR IGNORE INTO lottery (id, jackpot, draw) VALUES (1, %d, 1)", LOTTO_START));
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT jackpot, draw, lastday FROM lottery WHERE id = 1");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			gLottoJackpot = DB_GetFieldInt(r, 0);
			gLottoDraw = DB_GetFieldInt(r, 1);
			gLottoLastDay = DB_GetFieldInt(r, 2);
		}
		DB_FreeResultSet(r);
	}
	SetTimer("Casino_OnSecond", 1000, true);
}

static Lotto_Save()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE lottery SET jackpot = %d, draw = %d, lastday = %d WHERE id = 1", gLottoJackpot, gLottoDraw, gLottoLastDay));
}

// ---------------------------------------------------------------------
//  Казино
// ---------------------------------------------------------------------
stock bool:Casino_Show(playerid, b)
{
	if (gCasSpin[playerid] > 0) return SendClientMessage(playerid, COLOR_GREY, "Дождись результата.");
	gCasBiz[playerid] = b;
	ShowPlayerDialog(playerid, DIALOG_CASINO, DIALOG_STYLE_TABLIST_HEADERS, Biz_IconCaption(BIZ_CASINO, Biz[b][bName]),
		"Игра\tВыигрыш\nРулетка: красное / чёрное\tx2\nРулетка: число 0-36\tx36\nКости против крупье\tx2", "Играть", "Уйти");
	return true;
}

static Casino_AskBet(playerid)
{
	ShowPlayerDialog(playerid, DIALOG_CASINO_BET, DIALOG_STYLE_INPUT, "Ставка",
		"{FFFFFF}Сколько ставишь? От $%d до $%d.\nНаличных: $%d", "Ставка", "Назад", CASINO_MIN_BET, CASINO_MAX_BET, GetMoney(playerid));
}

static bool:Casino_IsRed(n)
{
	for (new i = 0; i < sizeof (gRouletteRed); i++) if (gRouletteRed[i] == n) return true;
	return false;
}

// Деньги проигравшего: часть владельцу, без владельца — в казну
static Casino_HouseWins(b, bet)
{
	new share = bet * CASINO_OWNER_PCT / 100;
	if (b >= 0 && Biz[b][bOwner] != 0)
	{
		Biz[b][bTill] += share;
		Biz_Save(b);
	}
	else Econ_ToTreasury(share);
}

static Casino_Result(playerid)
{
	new b = gCasBiz[playerid], bet = gCasBet[playerid], win = 0;
	if (b < 0 || !Biz_IsInside(playerid, b) && Biz_GetNearby(playerid) != b)
	{
		SendClientMessage(playerid, COLOR_GREY, "Ты ушёл от стола: ставка сгорела.");
		Casino_HouseWins(b, bet);
		return;
	}
	new msg[144];
	switch (gCasGame[playerid])
	{
		case CG_COLOR, CG_NUMBER:
		{
			new n = random(37);
			new col[16];
			if (n == 0) strcat(col, "зеро");
			else if (Casino_IsRed(n)) strcat(col, "красное");
			else strcat(col, "чёрное");
			if (gCasGame[playerid] == CG_COLOR && n != 0 && (Casino_IsRed(n) == (gCasPick[playerid] == 0))) win = bet * 2;
			if (gCasGame[playerid] == CG_NUMBER && n == gCasPick[playerid]) win = bet * 36;
			format(msg, sizeof msg, "Крупье: шарик остановился на %d (%s).", n, col);
		}
		case CG_DICE:
		{
			new p1 = random(6) + 1, p2 = random(6) + 1, d1 = random(6) + 1, d2 = random(6) + 1;
			if (p1 + p2 > d1 + d2) win = bet * 2;
			else if (p1 + p2 == d1 + d2) win = bet;     // ничья — ставка возвращается
			format(msg, sizeof msg, "Кости: у тебя %d+%d=%d, у крупье %d+%d=%d.", p1, p2, p1 + p2, d1, d2, d1 + d2);
		}
	}
	SendClientMessage(playerid, COLOR_WHITE, "%s", msg);
	if (win > 0)
	{
		GiveMoney(playerid, win, "выигрыш в казино");
		if (win > bet) GameTextForPlayer(playerid, "~g~WIN!", 2000, 1);
		SendClientMessage(playerid, COLOR_GREEN, (win == bet) ? ("Ничья: ставка $%d возвращена.") : ("Выигрыш: $%d!"), win);
	}
	else
	{
		Casino_HouseWins(b, bet);
		GameTextForPlayer(playerid, "~r~lose", 1500, 1);
		SendClientMessage(playerid, COLOR_GREY, "Ставка $%d проиграна. Ещё раз: ALT.", bet);
	}
	Account_Save(playerid);
}

static Casino_Start(playerid)
{
	new bet = gCasBet[playerid];
	if (GetMoney(playerid) < bet) return SendClientMessage(playerid, COLOR_GREY, "Не хватает наличных.");
	GiveMoney(playerid, -bet, "ставка в казино");
	gCasSpin[playerid] = CASINO_SPIN_SEC;
	new msg[96];
	if (gCasGame[playerid] == CG_DICE)
	{
		format(msg, sizeof msg, "%s бросает кости, ставка $%d.", GetRPName(playerid), bet);
		GameTextForPlayer(playerid, "~y~rolling...", CASINO_SPIN_SEC * 1000, 4);
	}
	else
	{
		format(msg, sizeof msg, "%s делает ставку $%d. Крупье запускает рулетку.", GetRPName(playerid), bet);
		GameTextForPlayer(playerid, "~y~spinning...", CASINO_SPIN_SEC * 1000, 4);
	}
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return 1;
}

forward Casino_OnSecond();
public Casino_OnSecond()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gCasSpin[i] <= 0) continue;
		if (!IsPlayerConnected(i)) { gCasSpin[i] = 0; continue; }
		if (--gCasSpin[i] == 0) Casino_Result(i);
	}
}

// ---------------------------------------------------------------------
//  Лотерея
// ---------------------------------------------------------------------
static Lotto_MyTickets(playerid, list[], size)
{
	list[0] = '\0';
	new n = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT number FROM lottery_tickets WHERE draw = %d AND owner = %d", gLottoDraw, Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		n = DB_GetRowCount(r);
		for (new i = 0; i < n; i++)
		{
			new part[8];
			format(part, sizeof part, "%s%d", (i > 0) ? ", " : "", DB_GetFieldInt(r, 0));
			strcat(list, part, size);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	return n;
}

stock bool:Cmd_Lotto(playerid)
{
	new list[64];
	new n = Lotto_MyTickets(playerid, list, sizeof list);
	SendClientMessage(playerid, COLOR_YELLOW, "Лотерея: джекпот $%d, розыгрыш каждый день в %d:00. Билет $%d — у ларька лотереи.", gLottoJackpot, LOTTO_HOUR, LOTTO_PRICE);
	if (n > 0) SendClientMessage(playerid, COLOR_WHITE, "Твои билеты: %s", list);
	return true;
}

static bool:Lotto_TryAlt(playerid)
{
	if (Place_Nearby(playerid, PL_LOTTERY, 2.5) == -1) return false;
	new list[64];
	new n = Lotto_MyTickets(playerid, list, sizeof list);
	if (n == 0) strcat(list, "нет");
	ShowPlayerDialog(playerid, DIALOG_LOTTO, DIALOG_STYLE_INPUT, "Лотерея штата",
		"{FFFFFF}Джекпот: {33AA33}$%d{FFFFFF}\nРозыгрыш каждый день в %d:00.\nТвои билеты: %s\n\nВведи число от 1 до %d. Билет стоит $%d.",
		"Купить", "Закрыть", gLottoJackpot, LOTTO_HOUR, list, LOTTO_MAX, LOTTO_PRICE);
	return true;
}

static Lotto_Buy(playerid, const input[])
{
	if (!IsNumeric(input)) return SendClientMessage(playerid, COLOR_GREY, "Нужно ввести число.");
	new num = strval(input);
	if (num < 1 || num > LOTTO_MAX) return SendClientMessage(playerid, COLOR_GREY, "Число от 1 до %d.", LOTTO_MAX);
	new list[64];
	if (Lotto_MyTickets(playerid, list, sizeof list) >= LOTTO_TICKETS) return SendClientMessage(playerid, COLOR_GREY, "Не больше %d билетов на розыгрыш.", LOTTO_TICKETS);
	if (GetMoney(playerid) < LOTTO_PRICE) return SendClientMessage(playerid, COLOR_GREY, "Не хватает денег.");
	GiveMoney(playerid, -LOTTO_PRICE, "лотерейный билет");
	Account_Save(playerid);
	new jp = LOTTO_PRICE * LOTTO_JACKPOT_PCT / 100;
	gLottoJackpot += jp;
	Econ_ToTreasury(LOTTO_PRICE - jp);
	Lotto_Save();
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO lottery_tickets (draw, owner, number) VALUES (%d, %d, %d)", gLottoDraw, Player[playerid][pDbId], num));
	return SendClientMessage(playerid, COLOR_GREEN, "Билет с числом %d куплен. Розыгрыш в %d:00, джекпот уже $%d.", num, LOTTO_HOUR, gLottoJackpot);
}

static Lotto_Draw()
{
	new num = random(LOTTO_MAX) + 1;
	new owners[50], n = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT DISTINCT owner FROM lottery_tickets WHERE draw = %d AND number = %d LIMIT 50", gLottoDraw, num);
	if (r != DBResult:0)
	{
		n = DB_GetRowCount(r);
		for (new i = 0; i < n; i++) { owners[i] = DB_GetFieldInt(r, 0); DB_SelectNextRow(r); }
		DB_FreeResultSet(r);
	}
	new msg[144];
	if (n == 0)
	{
		format(msg, sizeof msg, "[Лотерея] Выпало число %d. Никто не угадал, джекпот $%d переходит на завтра.", num, gLottoJackpot);
		SendClientMessageToAll(COLOR_YELLOW, msg);
	}
	else
	{
		new prize = gLottoJackpot / n;
		for (new i = 0; i < n; i++)
		{
			new online = Bank_FindOnline(owners[i]);
			if (online != INVALID_PLAYER_ID)
			{
				Player[online][pBank] += prize;
				Account_Save(online);
				SendClientMessage(online, COLOR_GREEN, "Твой билет выиграл в лотерею! $%d зачислены на счёт.", prize);
			}
			else DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET bank = bank + %d WHERE id = %d", prize, owners[i]));
			Bank_Log(owners[i], BANKLOG_TRANSFER_IN, prize);
		}
		format(msg, sizeof msg, "[Лотерея] Выпало число %d! Победителей: %d, каждый получает $%d.", num, n, prize);
		SendClientMessageToAll(COLOR_YELLOW, msg);
		gLottoJackpot = LOTTO_START;
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM lottery_tickets WHERE draw <= %d", gLottoDraw));
	gLottoDraw++;
	Lotto_Save();
}

// Раз в минуту (из OnMinuteTimer)
stock Casino_OnMinute()
{
	new hour, minute, year, month, day;
	gettime(hour, minute);
	getdate(year, month, day);
	if (hour == LOTTO_HOUR && minute == 0 && gLottoLastDay != day)
	{
		gLottoLastDay = day;
		Lotto_Draw();
	}
	else if (hour == LOTTO_HOUR - 1 && minute == 30)
		SendClientMessageToAll(COLOR_YELLOW, "[Лотерея] Через 30 минут розыгрыш! Билеты — в ларьке лотереи (/gps), джекпот: /lotto");
}

// ---------------------------------------------------------------------
stock Casino_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	switch (dialogid)
	{
		case DIALOG_CASINO:
		{
			if (!response) return 1;
			gCasGame[playerid] = listitem + 1;
			if (gCasGame[playerid] == CG_COLOR)
				ShowPlayerDialog(playerid, DIALOG_CASINO_PICK, DIALOG_STYLE_LIST, "На что ставишь?", "{FF4040}Красное\n{AAAAAA}Чёрное", "Дальше", "Назад");
			else if (gCasGame[playerid] == CG_NUMBER)
				ShowPlayerDialog(playerid, DIALOG_CASINO_PICK, DIALOG_STYLE_INPUT, "На какое число?", "{FFFFFF}Число от 0 до 36:", "Дальше", "Назад");
			else Casino_AskBet(playerid);
			return 1;
		}
		case DIALOG_CASINO_PICK:
		{
			if (!response) return Casino_Show(playerid, gCasBiz[playerid]);
			if (gCasGame[playerid] == CG_COLOR) gCasPick[playerid] = listitem;
			else
			{
				if (!IsNumeric(inputtext) || strval(inputtext) < 0 || strval(inputtext) > 36)
				{
					SendClientMessage(playerid, COLOR_GREY, "Число от 0 до 36.");
					return 1;
				}
				gCasPick[playerid] = strval(inputtext);
			}
			Casino_AskBet(playerid);
			return 1;
		}
		case DIALOG_CASINO_BET:
		{
			if (!response) return Casino_Show(playerid, gCasBiz[playerid]);
			if (!IsNumeric(inputtext)) { SendClientMessage(playerid, COLOR_GREY, "Нужно ввести число."); return 1; }
			new bet = strval(inputtext);
			if (bet < CASINO_MIN_BET || bet > CASINO_MAX_BET) { SendClientMessage(playerid, COLOR_GREY, "Ставка от $%d до $%d.", CASINO_MIN_BET, CASINO_MAX_BET); return 1; }
			gCasBet[playerid] = bet;
			Casino_Start(playerid);
			return 1;
		}
		case DIALOG_LOTTO:
		{
			if (response) Lotto_Buy(playerid, inputtext);
			return 1;
		}
	}
	return 0;
}

stock bool:Casino_TryAlt(playerid)
{
	return Lotto_TryAlt(playerid);
}

stock Casino_OnDisconnect(playerid)
{
	gCasSpin[playerid] = 0;
	gCasBiz[playerid] = -1;
}

stock bool:Casino_OnCommand(playerid, const cmd[])
{
	if (!strcmp(cmd, "/lotto", true)) { Cmd_Lotto(playerid); return true; }
	return false;
}
