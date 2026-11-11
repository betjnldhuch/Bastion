// =====================================================================
//  debts.inc — долги игрока перед штатом: налоги и штрафы.
//
//  Это два отдельных счёта (в базе accounts.tax_debt и accounts.fine_debt):
//    * налоговый — сюда каждый PayDay начисляется налог на машину;
//    * штрафной — сюда полицейский выписывает штраф: /fine [id] [сумма] [причина].
//  Сами собой деньги не списываются. Игрок платит со счёта в банке:
//  банкомат > «Оплатить налоги» / «Оплатить штрафы». Деньги уходят в казну.
//
//  Что будет, если не платить (считается сумма обоих счетов):
//    * от DEBT_LIMIT_BUY    — нельзя купить дом, машину и бизнес;
//    * от DEBT_LIMIT_WANTED — на PayDay должника объявляют в розыск (1 звезда).
//
//  /debts — посмотреть свои долги.
// =====================================================================

#define DEBT_LIMIT_BUY      20000
#define DEBT_LIMIT_WANTED   50000
#define FINE_MIN            100
#define FINE_MAX            5000
#define DEBT_MAX            10000000

static gDebt[MAX_PLAYERS][2];

stock Debts_Init()
{
	DB_AddColumn("accounts", "tax_debt", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "fine_debt", "INTEGER DEFAULT 0");
}

stock Debts_OnConnect(playerid)
{
	gDebt[playerid][DEBT_TAX] = 0;
	gDebt[playerid][DEBT_FINE] = 0;
}

// После входа в аккаунт (accounts.inc)
stock Debts_OnLogin(playerid)
{
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT tax_debt, fine_debt FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		gDebt[playerid][DEBT_TAX] = DB_GetFieldInt(r, 0);
		gDebt[playerid][DEBT_FINE] = DB_GetFieldInt(r, 1);
	}
	DB_FreeResultSet(r);
	if (Debts_Total(playerid) > 0)
		SendClientMessage(playerid, COLOR_ORANGE, "У тебя долг перед штатом: налоги $%d, штрафы $%d. Оплатить: банкомат. Подробнее: /debts",
			gDebt[playerid][DEBT_TAX], gDebt[playerid][DEBT_FINE]);
}

static Debts_Save(playerid)
{
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET tax_debt = %d, fine_debt = %d WHERE id = %d",
		gDebt[playerid][DEBT_TAX], gDebt[playerid][DEBT_FINE], Player[playerid][pDbId]));
}

stock Debts_Get(playerid, kind) { return gDebt[playerid][kind]; }
stock Debts_Total(playerid) { return gDebt[playerid][DEBT_TAX] + gDebt[playerid][DEBT_FINE]; }

// Начислить долг (kind: DEBT_TAX или DEBT_FINE)
stock Debts_Add(playerid, kind, amount)
{
	if (amount <= 0) return;
	gDebt[playerid][kind] += amount;
	if (gDebt[playerid][kind] > DEBT_MAX) gDebt[playerid][kind] = DEBT_MAX;
	Debts_Save(playerid);
}

// Покупка дома, машины, бизнеса: 1 — нельзя, долг слишком большой (игроку уже написано почему)
stock Debts_BlockBuy(playerid)
{
	if (Debts_Total(playerid) < DEBT_LIMIT_BUY) return 0;
	SendClientMessage(playerid, COLOR_RED, "Покупка недоступна: у тебя долг перед штатом $%d (налоги и штрафы). Оплати его в банкомате.", Debts_Total(playerid));
	return 1;
}

// На PayDay (rp.pwn): напоминание и розыск для злостных должников
stock Debts_PayDay(playerid)
{
	new total = Debts_Total(playerid);
	if (total <= 0) return;
	SendClientMessage(playerid, COLOR_ORANGE, "Долг перед штатом: налоги $%d, штрафы $%d. Оплатить: банкомат.", gDebt[playerid][DEBT_TAX], gDebt[playerid][DEBT_FINE]);
	if (total >= DEBT_LIMIT_WANTED && Player[playerid][pWanted] < 1 && Player[playerid][pJail] <= 0)
	{
		Player[playerid][pWanted] = 1;
		SetPlayerWantedLevel(playerid, 1);
		SendClientMessage(playerid, COLOR_RED, "Ты объявлен в розыск за неуплату долга штату ($%d).", total);
		new msg[144];
		format(msg, sizeof msg, "[Диспетчер] %s [%d] объявлен в розыск: долг штату $%d.", GetRPName(playerid), playerid, total);
		Faction_Message(FACTION_POLICE, Faction_GetColor(FACTION_POLICE), msg);
	}
	else if (total >= DEBT_LIMIT_BUY)
		SendClientMessage(playerid, COLOR_ORANGE, "Пока долг больше $%d, ты не можешь покупать дом, машину и бизнес. От $%d — розыск.", DEBT_LIMIT_BUY, DEBT_LIMIT_WANTED);
}

// Оплатить со счёта в банке, сколько хватит (из окна банкомата, bank.inc)
stock Debts_Pay(playerid, kind)
{
	new debt = gDebt[playerid][kind];
	if (debt <= 0)
	{
		SendClientMessage(playerid, COLOR_GREEN, (kind == DEBT_TAX) ? ("Долгов по налогам нет.") : ("Неоплаченных штрафов нет."));
		return 0;
	}
	new pay = (Player[playerid][pBank] < debt) ? Player[playerid][pBank] : debt;
	if (pay <= 0)
	{
		SendClientMessage(playerid, COLOR_GREY, "На счёте в банке нет денег. Долг: $%d. Сначала положи деньги на счёт.", debt);
		return 0;
	}
	Player[playerid][pBank] -= pay;
	gDebt[playerid][kind] -= pay;
	Econ_ToTreasury(pay);
	Bank_MoneyLog(Player[playerid][pDbId], -pay, Player[playerid][pMoney], (kind == DEBT_TAX) ? ("оплата налогов") : ("оплата штрафов"));
	Debts_Save(playerid);
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "%s: оплачено $%d со счёта. Осталось долга: $%d. На счёте: $%d.",
		(kind == DEBT_TAX) ? ("Налоги") : ("Штрафы"), pay, gDebt[playerid][kind], Player[playerid][pBank]);
	return pay;
}

stock bool:Cmd_Debts(playerid)
{
	new text[600];
	format(text, sizeof text,
		"{FFFFFF}Налоговый счёт: {FF9900}$%d\n{FFFFFF}Штрафной счёт: {FF9900}$%d\n{FFFFFF}Всего: $%d\n\nОплатить можно в любом банкомате со счёта в банке.\n\n{FFFF00}Если не платить\n{FFFFFF}От $%d — нельзя купить дом, машину и бизнес.\nОт $%d — розыск.",
		gDebt[playerid][DEBT_TAX], gDebt[playerid][DEBT_FINE], Debts_Total(playerid), DEBT_LIMIT_BUY, DEBT_LIMIT_WANTED);
	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "Долги перед штатом", "%s", "Закрыть", "", text);
	return true;
}

// /fine [id] [сумма] [причина] — полицейский на смене выписывает штраф
stock bool:Cmd_Fine(playerid, const params[])
{
	if (!Faction_Need(playerid, FACTION_POLICE, true)) return true;
	new pos = 0, amount, reason[64];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, amount) || amount < FINE_MIN || amount > FINE_MAX || !RestParam(params, pos, reason))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /fine [id] [сумма $%d-%d] [причина]", FINE_MIN, FINE_MAX);
	if (target == playerid || !Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Неверный игрок.");
	if (!IsPlayerNearPlayer(playerid, target, 8.0)) return SendClientMessage(playerid, COLOR_GREY, "Игрок слишком далеко.");

	Debts_Add(target, DEBT_FINE, amount);
	new msg[144];
	format(msg, sizeof msg, "%s выписывает штраф %s.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(target, COLOR_RED, "Штраф $%d от %s. Причина: %s. Оплатить: банкомат. Всего штрафов: $%d.", amount, GetRPName(playerid), reason, gDebt[target][DEBT_FINE]);
	format(msg, sizeof msg, "[Диспетчер] %s оштрафовал %s на $%d. Причина: %s", GetRPName(playerid), GetRPName(target), amount, reason);
	Faction_Message(FACTION_POLICE, Faction_GetColor(FACTION_POLICE), msg);
	return true;
}
