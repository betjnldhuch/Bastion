// =====================================================================
//  bank.inc — банк и банкоматы.
//
//  У каждого игрока есть банковский счёт. Номер счёта = номер аккаунта.
//  Деньги на счёте в безопасности: их не теряешь при смерти, можно
//  перевести другому игроку (даже если он не в сети) и снять в любом банкомате.
//
//  /bank (или /atm, или ALT) у банкомата — окно:
//    баланс и номер счёта, положить, снять (с комиссией BANK_ATM_FEE_PCT),
//    перевод игроку по id, перевод по номеру счёта, история операций.
//  PayDay: зарплата приходит на счёт (PAYDAY_TO_BANK), на остаток
//    начисляются проценты (BANK_INTEREST_PERMILLE, не больше BANK_INTEREST_MAX).
//  Все операции пишутся в таблицу bank_log.
//
//  Админы (4+): /addatm — банкомат там, где стоишь (аппарат появится
//  перед тобой), /delatm — удалить ближайший.
// =====================================================================

#define MAX_ATMS            50
#define ATM_RANGE           2.5
#define ATM_OBJECT_MODEL    19324       // модель банкомата
#define BANK_HISTORY_ROWS   10

// Типы записей в bank_log
enum
{
	BANKLOG_NONE,
	BANKLOG_DEPOSIT,        // положил наличные
	BANKLOG_WITHDRAW,       // снял наличные
	BANKLOG_TRANSFER_OUT,   // перевёл другому
	BANKLOG_TRANSFER_IN,    // получил перевод
	BANKLOG_SALARY,         // зарплата (PayDay)
	BANKLOG_INTEREST,       // проценты на остаток
	BANKLOG_FEE             // комиссия
}

static const gBankLogNames[][] =
{
	"-",
	"Пополнение",
	"Снятие",
	"Перевод",
	"Получен перевод",
	"Зарплата",
	"Проценты",
	"Комиссия"
};

enum E_ATM
{
	aDbId,
	Float:aX,
	Float:aY,
	Float:aZ,
	Float:aA,           // угол; меньше 0 — без объекта (только значок)
	aMarker,           // маркер (markers.inc)
	aObject
}
new Atm[MAX_ATMS][E_ATM];
new gAtmCount = 0;

// Банкоматы по умолчанию (координаты примерные, без объекта — значок $)
static const Float:gAtmDefaults[][3] =
{
	{ 1462.0, -1011.0, 26.84 },     // банк, Commerce
	{ 2501.0, -1683.0, 13.45 },     // Grove Street, у спавна
	{ 1540.0, -1670.0, 13.55 },     // у LSPD
	{ 1185.0, -1323.0, 13.57 },     // у больницы All Saints
	{ 1940.6, -1772.7, 13.4 }       // заправка Idlewood
};

stock Atm_Create(dbid, Float:x, Float:y, Float:z, Float:a = -1.0)
{
	if (gAtmCount >= MAX_ATMS) return -1;
	new n = gAtmCount++;
	Atm[n][aDbId] = dbid;
	Atm[n][aX] = x;
	Atm[n][aY] = y;
	Atm[n][aZ] = z;
	Atm[n][aA] = a;
	Atm[n][aObject] = INVALID_OBJECT_ID;
	// Без аппарата — значок $, с аппаратом — только надпись
	new model = 1274;
	if (a >= 0.0) model = 0;
	Atm[n][aMarker] = Marker_Create(MK_ATM, dbid, "Банкомат", model, Atm[n][aX], Atm[n][aY], Atm[n][aZ], 0, 0, GPS_ATM, (a >= 0.0) ? a : 0.0);
	Marker_SetLabel(Atm[n][aMarker], "{33AA33}Банкомат\n{FFFFFF}ALT — открыть счёт");
	if (a >= 0.0)
	{
		Atm[n][aA] = Marker_GetAngle(Atm[n][aMarker]);
		Atm_BuildObject(n);
	}
	return n;
}

// Аппарат стоит в метре перед точкой, лицом к игроку
static Atm_BuildObject(n)
{
	if (Atm[n][aObject] != INVALID_OBJECT_ID) DestroyObject(Atm[n][aObject]);
	new Float:a = Atm[n][aA];
	new Float:ox = Atm[n][aX] - floatsin(a, degrees) * 1.0;
	new Float:oy = Atm[n][aY] + floatcos(a, degrees) * 1.0;
	Atm[n][aObject] = CreateObject(ATM_OBJECT_MODEL, ox, oy, Atm[n][aZ] - 0.35, 0.0, 0.0, a + 180.0, 150.0);
}

stock Atm_Destroy(n)
{
	Marker_Destroy(Atm[n][aMarker], true);
	if (Atm[n][aObject] != INVALID_OBJECT_ID) DestroyObject(Atm[n][aObject]);
	Atm[n][aObject] = INVALID_OBJECT_ID;
}

// Админ перенёс маркер банкомата (/markers): аппарат встаёт перед новым местом
stock Atm_OnMarkerMoved(dbid, Float:x, Float:y, Float:z, Float:a)
{
	for (new n = 0; n < gAtmCount; n++)
	{
		if (Atm[n][aDbId] != dbid) continue;
		Atm[n][aX] = x;
		Atm[n][aY] = y;
		Atm[n][aZ] = z;
		if (Atm[n][aA] >= 0.0)
		{
			Atm[n][aA] = a;
			Atm_BuildObject(n);
		}
		return;
	}
}

stock Bank_Init()
{
	new count = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM atms");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (count == 0)
	{
		for (new i = 0; i < sizeof (gAtmDefaults); i++)
		{
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO atms (x, y, z, a) VALUES (%f, %f, %f, -1.0)",
				gAtmDefaults[i][0], gAtmDefaults[i][1], gAtmDefaults[i][2]));
		}
	}

	r = DB_ExecuteQuery(gDB, "SELECT id, x, y, z, a FROM atms ORDER BY id");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			Atm_Create(DB_GetFieldInt(r, 0), DB_GetFieldFloat(r, 1), DB_GetFieldFloat(r, 2), DB_GetFieldFloat(r, 3), DB_GetFieldFloat(r, 4));
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Bank] Loaded %d ATMs.", gAtmCount);
}

stock Atm_GetNearby(playerid)
{
	if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return -1;
	for (new n = 0; n < gAtmCount; n++)
	{
		if (IsPlayerInRangeOfPoint(playerid, ATM_RANGE, Atm[n][aX], Atm[n][aY], Atm[n][aZ])) return n;
	}
	return -1;
}

// ---------------------------------------------------------------------
//  Счета
// ---------------------------------------------------------------------

// Записать операцию в историю
stock Bank_Log(account, type, amount, other = 0)
{
	if (account <= 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO bank_log (account, type, amount, other, date) VALUES (%d, %d, %d, %d, %d)",
		account, type, amount, other, gettime()));
}

// Запись в общий журнал денег (economy.inc, /moneylog). cash — наличные на руках.
stock Bank_MoneyLog(account, amount, cash, const reason[])
{
	if (account <= 0 || amount == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO money_log (time, account, amount, cash, reason) VALUES (%d, %d, %d, %d, 'банк: %q')",
		gettime(), account, amount, cash, reason));
}

// Комиссия от суммы (в процентах), не меньше $1, если процент не 0
stock Bank_Fee(amount, pct)
{
	if (pct <= 0 || amount <= 0) return 0;
	new fee = amount / 100 * pct + (amount % 100) * pct / 100;
	return (fee < 1) ? 1 : fee;
}

// Игрок в сети с этим номером аккаунта, или INVALID_PLAYER_ID
stock Bank_FindOnline(dbid)
{
	if (dbid <= 0) return INVALID_PLAYER_ID;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pDbId] == dbid) return i;
	}
	return INVALID_PLAYER_ID;
}

// Зачислить на счёт игрока в сети (с проверкой лимита). true — получилось.
stock bool:Bank_Credit(playerid, amount, type, other = 0)
{
	if (amount <= 0) return false;
	if (Player[playerid][pBank] > BANK_MAX_BALANCE - amount) return false;
	Player[playerid][pBank] += amount;
	Bank_Log(Player[playerid][pDbId], type, amount, other);
	if (type == BANKLOG_SALARY) Bank_MoneyLog(Player[playerid][pDbId], amount, Player[playerid][pMoney], "зарплата на счёт");
	else if (type == BANKLOG_INTEREST) Bank_MoneyLog(Player[playerid][pDbId], amount, Player[playerid][pMoney], "проценты");
	return true;
}

// Зарплата на PayDay (из rp.pwn). Идёт на счёт или наличными — см. PAYDAY_TO_BANK.
stock Bank_Salary(playerid, amount)
{
	if (amount <= 0) return;
#if PAYDAY_TO_BANK
	if (Bank_Credit(playerid, amount, BANKLOG_SALARY)) return;
#endif
	GiveMoney(playerid, amount, "зарплата (PayDay)");  // счёт переполнен или выключено — наличными
}

// Проценты на остаток (из PayDay, после зарплаты)
stock Bank_PayDay(playerid)
{
#if BANK_INTEREST_PERMILLE > 0
	if (Player[playerid][pBank] > 0)
	{
		new interest = Player[playerid][pBank] / 1000 * BANK_INTEREST_PERMILLE;
		if (interest > BANK_INTEREST_MAX) interest = BANK_INTEREST_MAX;
		if (interest > 0 && Bank_Credit(playerid, interest, BANKLOG_INTEREST))
			SendClientMessage(playerid, COLOR_WHITE, "Проценты по счёту: $%d.", interest);
	}
#endif
#if PAYDAY_TO_BANK
	SendClientMessage(playerid, COLOR_WHITE, "Зарплата зачислена на счёт. Баланс: $%d.", Player[playerid][pBank]);
#endif
}

// ---------------------------------------------------------------------
//  Окна
// ---------------------------------------------------------------------
stock Bank_ShowMenu(playerid)
{
	ShowPlayerDialog(playerid, DIALOG_BANK, DIALOG_STYLE_LIST, "Bastion Bank",
		"Баланс и номер счёта\nПоложить деньги\nСнять деньги\nПеревести игроку (по id)\nПеревести по номеру счёта\nИстория операций\nОплатить налоги ($%d)\nОплатить штрафы ($%d)",
		"Выбрать", "Закрыть", Debts_Get(playerid, DEBT_TAX), Debts_Get(playerid, DEBT_FINE));
}

stock Bank_ShowHistory(playerid)
{
	new body[1024], line[96], name[MAX_PLAYER_NAME], when[16];
	format(body, sizeof (body), "{FFFFFF}Счёт №%d. Баланс: {33AA33}$%d{FFFFFF}\n\n", Player[playerid][pDbId], Player[playerid][pBank]);

	new DBResult:r = DB_ExecuteQuery(gDB,
		"SELECT l.type, l.amount, IFNULL(a.name, ''), strftime('%q', l.date, 'unixepoch', 'localtime') FROM bank_log l LEFT JOIN accounts a ON a.id = l.other WHERE l.account = %d ORDER BY l.id DESC LIMIT %d",
		"%d.%m %H:%M", Player[playerid][pDbId], BANK_HISTORY_ROWS);
	new rows = 0;
	if (r != DBResult:0)
	{
		rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new type = DB_GetFieldInt(r, 0);
			new amount = DB_GetFieldInt(r, 1);
			DB_GetFieldString(r, 2, name, sizeof (name));
			DB_GetFieldString(r, 3, when, sizeof (when));
			if (type < 0 || type >= sizeof (gBankLogNames)) type = 0;
			new bool:minus = (type == BANKLOG_WITHDRAW || type == BANKLOG_TRANSFER_OUT || type == BANKLOG_FEE);
			if (minus) format(line, sizeof (line), "%s  {FF6347}-$%d{FFFFFF}  %s", when, amount, gBankLogNames[type]);
			else format(line, sizeof (line), "%s  {33AA33}+$%d{FFFFFF}  %s", when, amount, gBankLogNames[type]);
			if (name[0] != '\0')
			{
				for (new c = 0; name[c] != '\0'; c++) if (name[c] == '_') name[c] = ' ';
				strcat(line, " — ");
				strcat(line, name);
			}
			strcat(line, "\n");
			strcat(body, line);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	if (rows == 0) strcat(body, "Операций пока не было.");
	ShowPlayerDialog(playerid, DIALOG_BANK_HISTORY, DIALOG_STYLE_MSGBOX, "История операций", "%s", "Назад", "", body);
}

// Перевод со счёта игрока на счёт с номером dbid. Сам проверяет всё.
stock Bank_Transfer(playerid, dbid, amount)
{
	if (dbid <= 0 || dbid == Player[playerid][pDbId])
		return SendClientMessage(playerid, COLOR_GREY, "Неверный получатель.");

	new fee = Bank_Fee(amount, BANK_TRANSFER_FEE_PCT);
	if (amount < 1 || amount > BANK_MAX_TRANSFER || amount + fee > Player[playerid][pBank])
		return SendClientMessage(playerid, COLOR_GREY, "Неверная сумма. На счёте $%d, комиссия $%d, максимум за раз $%d.",
			Player[playerid][pBank], fee, BANK_MAX_TRANSFER);

	new target = Bank_FindOnline(dbid);
	new tname[MAX_PLAYER_NAME];
	if (target != INVALID_PLAYER_ID)
	{
		if (!Bank_Credit(target, amount, BANKLOG_TRANSFER_IN, Player[playerid][pDbId]))
			return SendClientMessage(playerid, COLOR_GREY, "У получателя переполнен счёт.");
		Account_Save(target);
		format(tname, sizeof (tname), "%s", GetRPName(target));
	}
	else
	{
		// Получатель не в сети — меняем прямо в базе
		new bool:ok = false;
		new DBResult:r = DB_ExecuteQuery(gDB, "SELECT bank, name FROM accounts WHERE id = %d", dbid);
		if (r != DBResult:0)
		{
			if (DB_GetRowCount(r) > 0)
			{
				ok = (DB_GetFieldInt(r, 0) <= BANK_MAX_BALANCE - amount);
				DB_GetFieldString(r, 1, tname, sizeof (tname));
				if (!ok) SendClientMessage(playerid, COLOR_GREY, "У получателя переполнен счёт.");
			}
			else SendClientMessage(playerid, COLOR_GREY, "Счёта №%d не существует.", dbid);
			DB_FreeResultSet(r);
		}
		if (!ok) return 1;
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET bank = bank + %d WHERE id = %d", amount, dbid));
		Bank_Log(dbid, BANKLOG_TRANSFER_IN, amount, Player[playerid][pDbId]);
		Bank_MoneyLog(dbid, amount, 0, "перевод от счёта (игрок не в сети)");
		for (new i = 0; tname[i] != '\0'; i++) if (tname[i] == '_') tname[i] = ' ';
	}

	Player[playerid][pBank] -= amount + fee;
	Bank_Log(Player[playerid][pDbId], BANKLOG_TRANSFER_OUT, amount, dbid);
	if (fee > 0) Bank_Log(Player[playerid][pDbId], BANKLOG_FEE, fee);
	new reason[64];
	format(reason, sizeof (reason), "перевод -> %s (комиссия $%d)", tname, fee);
	Bank_MoneyLog(Player[playerid][pDbId], -(amount + fee), Player[playerid][pMoney], reason);
	if (target != INVALID_PLAYER_ID)
	{
		format(reason, sizeof (reason), "перевод <- %s", GetRPName(playerid));
		Bank_MoneyLog(dbid, amount, Player[target][pMoney], reason);
	}
	Account_Save(playerid);

	SendClientMessage(playerid, COLOR_GREEN, "Ты перевёл $%d на счёт №%d (%s). Комиссия $%d. На счёте: $%d.",
		amount, dbid, tname, fee, Player[playerid][pBank]);
	if (target != INVALID_PLAYER_ID)
		SendClientMessage(target, COLOR_GREEN, "%s перевёл тебе на счёт $%d.", GetRPName(playerid), amount);
	return 1;
}

// Обработка окон банка (из OnDialogResponse). Возвращает 1, если окно было наше.
stock Bank_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid != DIALOG_BANK && dialogid != DIALOG_BANK_DEPOSIT && dialogid != DIALOG_BANK_WITHDRAW
		&& dialogid != DIALOG_BANK_TRANSFER && dialogid != DIALOG_BANK_TRANSFER_ACC && dialogid != DIALOG_BANK_HISTORY) return 0;
	if (dialogid == DIALOG_BANK_HISTORY)
	{
		if (Atm_GetNearby(playerid) != -1) Bank_ShowMenu(playerid);
		return 1;
	}
	if (!response)
	{
		if (dialogid != DIALOG_BANK) Bank_ShowMenu(playerid);   // "Назад" — в меню
		return 1;
	}
	if (Atm_GetNearby(playerid) == -1)
	{
		SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от банкомата.");
		return 1;
	}

	switch (dialogid)
	{
		case DIALOG_BANK:
		{
			switch (listitem)
			{
				case 0:
				{
					SendClientMessage(playerid, COLOR_GREEN, "Счёт №%d. На счёте: $%d. Наличные: $%d.",
						Player[playerid][pDbId], Player[playerid][pBank], GetMoney(playerid));
					Bank_ShowMenu(playerid);
				}
				case 1: ShowPlayerDialog(playerid, DIALOG_BANK_DEPOSIT, DIALOG_STYLE_INPUT, "Положить деньги",
					"{FFFFFF}Наличные: $%d\nСколько положить на счёт?", "Положить", "Назад", GetMoney(playerid));
				case 2: ShowPlayerDialog(playerid, DIALOG_BANK_WITHDRAW, DIALOG_STYLE_INPUT, "Снять деньги",
					"{FFFFFF}На счёте: $%d\nКомиссия банкомата: %d из каждых $100 (не меньше $1).\nСколько снять?", "Снять", "Назад",
					Player[playerid][pBank], BANK_ATM_FEE_PCT);
				case 3: ShowPlayerDialog(playerid, DIALOG_BANK_TRANSFER, DIALOG_STYLE_INPUT, "Перевод игроку",
					"{FFFFFF}На счёте: $%d\nВведи id игрока и сумму через пробел, например: 5 1000", "Перевести", "Назад", Player[playerid][pBank]);
				case 4: ShowPlayerDialog(playerid, DIALOG_BANK_TRANSFER_ACC, DIALOG_STYLE_INPUT, "Перевод по номеру счёта",
					"{FFFFFF}На счёте: $%d\nВведи номер счёта и сумму через пробел, например: 12 1000\nПолучатель может быть не в игре.", "Перевести", "Назад", Player[playerid][pBank]);
				case 5: Bank_ShowHistory(playerid);
				case 6: { Debts_Pay(playerid, DEBT_TAX); Bank_ShowMenu(playerid); }     // debts.inc
				case 7: { Debts_Pay(playerid, DEBT_FINE); Bank_ShowMenu(playerid); }
			}
		}
		case DIALOG_BANK_DEPOSIT:
		{
			new amount = strval(inputtext);
			if (!IsNumeric(inputtext) || amount < 1 || amount > GetMoney(playerid))
			{
				SendClientMessage(playerid, COLOR_GREY, "Неверная сумма. У тебя наличными $%d.", GetMoney(playerid));
				Bank_ShowMenu(playerid);
				return 1;
			}
			if (!Bank_Credit(playerid, amount, BANKLOG_DEPOSIT))
			{
				SendClientMessage(playerid, COLOR_GREY, "Счёт переполнен (максимум $%d).", BANK_MAX_BALANCE);
				return 1;
			}
			GiveMoney(playerid, -amount, "банк: пополнение счёта");
			Account_Save(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Ты положил $%d. На счёте: $%d.", amount, Player[playerid][pBank]);
		}
		case DIALOG_BANK_WITHDRAW:
		{
			new amount = strval(inputtext);
			new fee = Bank_Fee(amount, BANK_ATM_FEE_PCT);
			if (!IsNumeric(inputtext) || amount < 1 || amount > Player[playerid][pBank] || amount + fee > Player[playerid][pBank])
			{
				SendClientMessage(playerid, COLOR_GREY, "Неверная сумма. На счёте $%d (комиссия $%d).", Player[playerid][pBank], fee);
				Bank_ShowMenu(playerid);
				return 1;
			}
			Player[playerid][pBank] -= amount + fee;
			GiveMoney(playerid, amount, "банк: снятие со счёта");
			if (fee > 0) Bank_MoneyLog(Player[playerid][pDbId], -fee, Player[playerid][pMoney], "комиссия банкомата");
			Bank_Log(Player[playerid][pDbId], BANKLOG_WITHDRAW, amount);
			if (fee > 0) Bank_Log(Player[playerid][pDbId], BANKLOG_FEE, fee);
			Account_Save(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Ты снял $%d (комиссия $%d). На счёте: $%d.", amount, fee, Player[playerid][pBank]);
		}
		case DIALOG_BANK_TRANSFER:
		{
			new pos = 0, amount;
			new target = NextPlayerParam(inputtext, pos);
			if (target == INVALID_PLAYER_ID || !NextIntParam(inputtext, pos, amount))
			{
				SendClientMessage(playerid, COLOR_GREY, "Нужно ввести id игрока и сумму, например: 5 1000");
				Bank_ShowMenu(playerid);
				return 1;
			}
			if (target == playerid || !Player[target][pLogged])
				return SendClientMessage(playerid, COLOR_GREY, "Неверный получатель.");
			Bank_Transfer(playerid, Player[target][pDbId], amount);
		}
		case DIALOG_BANK_TRANSFER_ACC:
		{
			new pos = 0, account, amount;
			if (!NextIntParam(inputtext, pos, account) || !NextIntParam(inputtext, pos, amount))
			{
				SendClientMessage(playerid, COLOR_GREY, "Нужно ввести номер счёта и сумму, например: 12 1000");
				Bank_ShowMenu(playerid);
				return 1;
			}
			Bank_Transfer(playerid, account, amount);
		}
	}
	return 1;
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Cmd_Bank(playerid)
{
	if (Atm_GetNearby(playerid) == -1)
		return SendClientMessage(playerid, COLOR_GREY, "Подойди к банкомату (зелёный значок $ или аппарат).");
	Bank_ShowMenu(playerid);
	return true;
}

stock bool:Cmd_AddAtm(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	if (GetPlayerInterior(playerid) != 0) return SendClientMessage(playerid, COLOR_GREY, "Банкомат можно ставить только на улице.");
	if (gAtmCount >= MAX_ATMS) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит банкоматов.");
	new Float:x, Float:y, Float:z, Float:a;
	GetPlayerPos(playerid, x, y, z);
	GetPlayerFacingAngle(playerid, a);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO atms (x, y, z, a) VALUES (%f, %f, %f, %f)", x, y, z, a));
	Atm_Create(DB_LastInsertId(), x, y, z, a);
	SendClientMessage(playerid, COLOR_ADMIN, "Банкомат поставлен перед тобой. Удалить: /delatm");
	return true;
}

stock bool:Cmd_DelAtm(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new n = Atm_GetNearby(playerid);
	if (n == -1) return SendClientMessage(playerid, COLOR_GREY, "Подойди к банкомату.");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM atms WHERE id = %d", Atm[n][aDbId]));
	Atm_Destroy(n);
	new last = gAtmCount - 1;
	if (n != last) Atm[n] = Atm[last];
	gAtmCount--;
	SendClientMessage(playerid, COLOR_ADMIN, "Банкомат удалён.");
	return true;
}
