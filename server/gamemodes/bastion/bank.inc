// =====================================================================
//  bank.inc — банк и банкоматы.
//
//  Деньги на счёте в безопасности: их можно перевести другому игроку
//  и снять в любом банкомате.
//
//  /bank (или /atm) у банкомата — окно: баланс, положить, снять, перевести.
//  Админы (4+): /addatm — банкомат там, где стоишь, /delatm — удалить ближайший.
// =====================================================================

#define MAX_ATMS            50
#define ATM_RANGE           2.5

enum E_ATM
{
	aDbId,
	Float:aX,
	Float:aY,
	Float:aZ,
	aPickup,
	Text3D:aLabel
}
new Atm[MAX_ATMS][E_ATM];
new gAtmCount = 0;

// Банкоматы по умолчанию (координаты примерные)
static const Float:gAtmDefaults[][3] =
{
	{ 1462.0, -1011.0, 26.84 },     // банк, Commerce
	{ 2501.0, -1683.0, 13.45 },     // Grove Street, у спавна
	{ 1540.0, -1670.0, 13.55 },     // у LSPD
	{ 1185.0, -1323.0, 13.57 },     // у больницы All Saints
	{ 1940.6, -1772.7, 13.4 }       // заправка Idlewood
};

stock Atm_Create(dbid, Float:x, Float:y, Float:z)
{
	if (gAtmCount >= MAX_ATMS) return -1;
	new a = gAtmCount++;
	Atm[a][aDbId] = dbid;
	Atm[a][aX] = x;
	Atm[a][aY] = y;
	Atm[a][aZ] = z;
	Atm[a][aPickup] = CreatePickup(1274, 1, x, y, z, 0);
	Atm[a][aLabel] = Create3DTextLabel("{33AA33}Банкомат\n{FFFFFF}ALT — открыть счёт", COLOR_WHITE, x, y, z + 0.6, 15.0, 0, true);
	return a;
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
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO atms (x, y, z) VALUES (%f, %f, %f)",
				gAtmDefaults[i][0], gAtmDefaults[i][1], gAtmDefaults[i][2]));
		}
	}

	r = DB_ExecuteQuery(gDB, "SELECT id, x, y, z FROM atms ORDER BY id");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			Atm_Create(DB_GetFieldInt(r, 0), DB_GetFieldFloat(r, 1), DB_GetFieldFloat(r, 2), DB_GetFieldFloat(r, 3));
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Bank] Loaded %d ATMs.", gAtmCount);
}

stock Atm_GetNearby(playerid)
{
	if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return -1;
	for (new a = 0; a < gAtmCount; a++)
	{
		if (IsPlayerInRangeOfPoint(playerid, ATM_RANGE, Atm[a][aX], Atm[a][aY], Atm[a][aZ])) return a;
	}
	return -1;
}

stock Bank_ShowMenu(playerid)
{
	ShowPlayerDialog(playerid, DIALOG_BANK, DIALOG_STYLE_LIST, "Bastion Bank",
		"Баланс\nПоложить деньги\nСнять деньги\nПеревести другому игроку", "Выбрать", "Закрыть");
}

// Обработка окон банка (из OnDialogResponse). Возвращает 1, если окно было наше.
stock Bank_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid != DIALOG_BANK && dialogid != DIALOG_BANK_DEPOSIT && dialogid != DIALOG_BANK_WITHDRAW && dialogid != DIALOG_BANK_TRANSFER) return 0;
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
					SendClientMessage(playerid, COLOR_GREEN, "На счёте: $%d. Наличные: $%d.", Player[playerid][pBank], GetMoney(playerid));
					Bank_ShowMenu(playerid);
				}
				case 1: ShowPlayerDialog(playerid, DIALOG_BANK_DEPOSIT, DIALOG_STYLE_INPUT, "Положить деньги",
					"{FFFFFF}Наличные: $%d\nСколько положить на счёт?", "Положить", "Назад", GetMoney(playerid));
				case 2: ShowPlayerDialog(playerid, DIALOG_BANK_WITHDRAW, DIALOG_STYLE_INPUT, "Снять деньги",
					"{FFFFFF}На счёте: $%d\nСколько снять?", "Снять", "Назад", Player[playerid][pBank]);
				case 3: ShowPlayerDialog(playerid, DIALOG_BANK_TRANSFER, DIALOG_STYLE_INPUT, "Перевод",
					"{FFFFFF}На счёте: $%d\nВведи id игрока и сумму через пробел, например: 5 1000", "Перевести", "Назад", Player[playerid][pBank]);
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
			GiveMoney(playerid, -amount);
			Player[playerid][pBank] += amount;
			Account_Save(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Ты положил $%d. На счёте: $%d.", amount, Player[playerid][pBank]);
		}
		case DIALOG_BANK_WITHDRAW:
		{
			new amount = strval(inputtext);
			if (!IsNumeric(inputtext) || amount < 1 || amount > Player[playerid][pBank])
			{
				SendClientMessage(playerid, COLOR_GREY, "Неверная сумма. На счёте $%d.", Player[playerid][pBank]);
				Bank_ShowMenu(playerid);
				return 1;
			}
			Player[playerid][pBank] -= amount;
			GiveMoney(playerid, amount);
			Account_Save(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Ты снял $%d. На счёте: $%d.", amount, Player[playerid][pBank]);
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
			{
				SendClientMessage(playerid, COLOR_GREY, "Неверный получатель.");
				return 1;
			}
			if (amount < 1 || amount > BANK_MAX_TRANSFER || amount > Player[playerid][pBank])
			{
				SendClientMessage(playerid, COLOR_GREY, "Неверная сумма. На счёте $%d, максимум за раз $%d.", Player[playerid][pBank], BANK_MAX_TRANSFER);
				return 1;
			}
			Player[playerid][pBank] -= amount;
			Player[target][pBank] += amount;
			Bank_LogTransfer(playerid, target, amount);
			Account_Save(playerid);
			Account_Save(target);
			SendClientMessage(playerid, COLOR_GREEN, "Ты перевёл $%d игроку %s. На счёте: $%d.", amount, GetRPName(target), Player[playerid][pBank]);
			SendClientMessage(target, COLOR_GREEN, "%s перевёл тебе на счёт $%d.", GetRPName(playerid), amount);
		}
	}
	return 1;
}

// Перевод в журнал денег (economy.inc): наличные не меняются, пишем отдельно
stock Bank_LogTransfer(playerid, target, amount)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO money_log (time, account, amount, cash, reason) VALUES (%d, %d, %d, %d, 'банк: перевод -> %q')",
		gettime(), Player[playerid][pDbId], -amount, Player[playerid][pMoney], GetRPName(target)));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO money_log (time, account, amount, cash, reason) VALUES (%d, %d, %d, %d, 'банк: перевод <- %q')",
		gettime(), Player[target][pDbId], amount, Player[target][pMoney], GetRPName(playerid)));
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Cmd_Bank(playerid)
{
	if (Atm_GetNearby(playerid) == -1)
		return SendClientMessage(playerid, COLOR_GREY, "Подойди к банкомату (зелёный значок $).");
	Bank_ShowMenu(playerid);
	return true;
}

stock bool:Cmd_AddAtm(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	if (GetPlayerInterior(playerid) != 0) return SendClientMessage(playerid, COLOR_GREY, "Банкомат можно ставить только на улице.");
	if (gAtmCount >= MAX_ATMS) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит банкоматов.");
	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO atms (x, y, z) VALUES (%f, %f, %f)", x, y, z));
	Atm_Create(DB_LastInsertId(), x, y, z);
	SendClientMessage(playerid, COLOR_ADMIN, "Банкомат поставлен.");
	return true;
}

stock bool:Cmd_DelAtm(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new a = Atm_GetNearby(playerid);
	if (a == -1) return SendClientMessage(playerid, COLOR_GREY, "Подойди к банкомату.");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM atms WHERE id = %d", Atm[a][aDbId]));
	DestroyPickup(Atm[a][aPickup]);
	Delete3DTextLabel(Atm[a][aLabel]);
	new last = gAtmCount - 1;
	if (a != last) Atm[a] = Atm[last];
	gAtmCount--;
	SendClientMessage(playerid, COLOR_ADMIN, "Банкомат удалён.");
	return true;
}
