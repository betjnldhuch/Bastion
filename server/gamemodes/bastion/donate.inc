// =====================================================================
//  donate.inc — донат-меню: донат-монеты и что на них купить.
//
//  Монеты выдаёт главный админ (6+) после оплаты: /givedonate [id] [монеты] [комментарий].
//  Все выдачи пишутся в журнал админов. Оплату сервер не принимает сам —
//  её проверяет человек, а здесь только зачисление и трата.
//
//  Игрок: /donate — баланс и магазин.
//    игровые деньги (DONATE_MONEY_RATE $ за монету, пачками),
//    VIP на DONATE_VIP_DAYS дней — прибавка к PayDay, опыт x2, тег [VIP] в /vip;
//    любой скин навсегда (в сумку как одежда),
//    +1 уровень, снять предупреждения (/warn), смена номера телефона на свой.
//  /vip — статус VIP.
// =====================================================================

#define DONATE_MONEY_RATE       1000    // $ за одну монету
#define DONATE_VIP_DAYS         30
#define DONATE_VIP_PRICE        300
#define DONATE_VIP_PAYDAY       2000    // прибавка VIP к каждому PayDay
#define DONATE_SKIN_PRICE       150
#define DONATE_LEVEL_PRICE      200
#define DONATE_UNWARN_PRICE     100
#define DONATE_NUMBER_PRICE     150

static gDonate[MAX_PLAYERS];            // монеты
static gVipUntil[MAX_PLAYERS];
static gDonateSel[MAX_PLAYERS];

static const gDonMoneyPacks[] = { 10, 50, 100, 500 };   // пачки монет на обмен в деньги

stock Donate_Init()
{
	DB_AddColumn("accounts", "donate", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "vip_until", "INTEGER DEFAULT 0");
}

stock Donate_LoadPlayer(playerid)
{
	gDonate[playerid] = 0;
	gVipUntil[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT donate, vip_until FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		gDonate[playerid] = DB_GetFieldInt(r, 0);
		gVipUntil[playerid] = DB_GetFieldInt(r, 1);
	}
	DB_FreeResultSet(r);
	if (Donate_IsVip(playerid)) SendClientMessage(playerid, 0xFFD700FF, "VIP активен ещё %d дн. (/vip)", (gVipUntil[playerid] - gettime()) / 86400 + 1);
}

static Donate_Save(playerid)
{
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET donate = %d, vip_until = %d WHERE id = %d", gDonate[playerid], gVipUntil[playerid], Player[playerid][pDbId]));
}

stock bool:Donate_IsVip(playerid) { return gVipUntil[playerid] > gettime(); }

// Из PayDay (rp.pwn): прибавка VIP
stock Donate_PayDay(playerid)
{
	if (!Donate_IsVip(playerid)) return;
	Bank_Salary(playerid, DONATE_VIP_PAYDAY);
	Player[playerid][pExp]++;                   // опыт x2
	SendClientMessage(playerid, 0xFFD700FF, "VIP: прибавка $%d и двойной опыт.", DONATE_VIP_PAYDAY);
}

static bool:Donate_Spend(playerid, coins, const what[])
{
	if (gDonate[playerid] < coins) { SendClientMessage(playerid, COLOR_GREY, "Не хватает монет: нужно %d, у тебя %d.", coins, gDonate[playerid]); return false; }
	gDonate[playerid] -= coins;
	Donate_Save(playerid);
	new log[64];
	format(log, sizeof log, "донат: %s (-%d)", what, coins);
	Admin_Log(playerid, "donate_spend", playerid, log, coins);
	return true;
}

// ---------------------------------------------------------------------
stock bool:Cmd_Donate(playerid)
{
	new list[640], line[96];
	format(list, sizeof list, "Товар\tЦена\n");
	format(line, sizeof line, "Игровые деньги\t1 монета = $%d\n", DONATE_MONEY_RATE); strcat(list, line);
	format(line, sizeof line, "VIP на %d дней%s\t%d монет\n", DONATE_VIP_DAYS, Donate_IsVip(playerid) ? " (продлить)" : "", DONATE_VIP_PRICE); strcat(list, line);
	format(line, sizeof line, "Любой скин (в сумку)\t%d монет\n", DONATE_SKIN_PRICE); strcat(list, line);
	format(line, sizeof line, "+1 уровень\t%d монет\n", DONATE_LEVEL_PRICE); strcat(list, line);
	format(line, sizeof line, "Снять все предупреждения\t%d монет\n", DONATE_UNWARN_PRICE); strcat(list, line);
	format(line, sizeof line, "Свой номер телефона\t%d монет\n", DONATE_NUMBER_PRICE); strcat(list, line);
	new title[48];
	format(title, sizeof title, "Донат: у тебя %d монет", gDonate[playerid]);
	ShowPlayerDialog(playerid, DIALOG_DONATE, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Выбрать", "Закрыть", list);
	return true;
}

stock bool:Cmd_Vip(playerid)
{
	if (!Donate_IsVip(playerid)) return SendClientMessage(playerid, COLOR_GREY, "VIP нет. Купить: /donate");
	SendClientMessage(playerid, 0xFFD700FF, "VIP ещё %d дн.: +$%d и двойной опыт каждый PayDay. Донат-монет: %d.",
		(gVipUntil[playerid] - gettime()) / 86400 + 1, DONATE_VIP_PAYDAY, gDonate[playerid]);
	return true;
}

stock bool:Cmd_GiveDonate(playerid, const params[])
{
	if (!Admin_Check(playerid, 6)) return true;
	new pos = 0, coins, note[64];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, coins) || coins == 0 || coins < -100000 || coins > 100000)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /givedonate [id] [монеты] [комментарий: номер платежа]  (минус — забрать)");
	if (!RestParam(params, pos, note)) strcat(note, "-");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	if (gDonate[target] + coins < 0) coins = -gDonate[target];
	gDonate[target] += coins;
	Donate_Save(target);
	Admin_Log(playerid, "givedonate", target, note, coins);
	SendClientMessage(target, 0xFFD700FF, "%s монет: %d. Баланс: %d. Магазин: /donate", (coins > 0) ? "Зачислено" : "Списано", (coins > 0) ? coins : -coins, gDonate[target]);
	SendClientMessage(playerid, COLOR_ADMIN, "%s: %+d монет, теперь %d.", GetRPName(target), coins, gDonate[target]);
	return true;
}

static Donate_Buy(playerid, item)
{
	switch (item)
	{
		case 0:
		{
			new list[160], line[48];
			for (new i = 0; i < sizeof (gDonMoneyPacks); i++)
			{
				format(line, sizeof line, "%d монет\t$%d\n", gDonMoneyPacks[i], gDonMoneyPacks[i] * DONATE_MONEY_RATE);
				strcat(list, line);
			}
			ShowPlayerDialog(playerid, DIALOG_DONATE_MONEY, DIALOG_STYLE_TABLIST, "Обмен монет на деньги", "%s", "Обменять", "Назад", list);
		}
		case 1:
		{
			if (!Donate_Spend(playerid, DONATE_VIP_PRICE, "VIP")) return;
			new now = gettime();
			gVipUntil[playerid] = ((gVipUntil[playerid] > now) ? gVipUntil[playerid] : now) + DONATE_VIP_DAYS * 86400;
			Donate_Save(playerid);
			SendClientMessage(playerid, 0xFFD700FF, "VIP активирован: ещё %d дн. Спасибо за поддержку сервера!", (gVipUntil[playerid] - now) / 86400 + 1);
		}
		case 2:
		{
			ShowPlayerDialog(playerid, DIALOG_DONATE_INPUT, DIALOG_STYLE_INPUT, "Скин", "{FFFFFF}Номер скина (1-311). Он ляжет в сумку как одежда.", "Купить", "Назад");
		}
		case 3:
		{
			if (!Donate_Spend(playerid, DONATE_LEVEL_PRICE, "+1 уровень")) return;
			Player[playerid][pLevel]++;
			Player[playerid][pExp] = 0;
			SetPlayerScore(playerid, Player[playerid][pLevel]);
			Account_Save(playerid);
			SendClientMessage(playerid, 0xFFD700FF, "Уровень повышен до %d.", Player[playerid][pLevel]);
		}
		case 4:
		{
			if (Admin_GetWarns(playerid) <= 0) { SendClientMessage(playerid, COLOR_GREY, "Предупреждений нет."); return; }
			if (!Donate_Spend(playerid, DONATE_UNWARN_PRICE, "снять предупреждения")) return;
			Admin_ClearWarns(playerid);
			SendClientMessage(playerid, 0xFFD700FF, "Все предупреждения сняты.");
		}
		case 5:
		{
			if (Player[playerid][pPhone] == 0) { SendClientMessage(playerid, COLOR_GREY, "Сначала купи телефон в магазине 24/7."); return; }
			ShowPlayerDialog(playerid, DIALOG_DONATE_INPUT, DIALOG_STYLE_INPUT, "Свой номер", "{FFFFFF}Номер из 3-7 цифр, например 777 или 5550000:", "Купить", "Назад");
		}
	}
}

static Donate_Input(playerid, const input[])
{
	if (!IsNumeric(input) || input[0] == '\0') { SendClientMessage(playerid, COLOR_GREY, "Нужно ввести число."); return; }
	new v = strval(input);
	if (gDonateSel[playerid] == 2)
	{
		if (v < 1 || v > 311 || v == 74) { SendClientMessage(playerid, COLOR_GREY, "Скин от 1 до 311."); return; }
		if (Outfit_FreeSlots(playerid) <= 0) { SendClientMessage(playerid, COLOR_GREY, "В сумке нет места для одежды."); return; }
		if (!Donate_Spend(playerid, DONATE_SKIN_PRICE, "скин")) return;
		Outfit_Add(playerid, v);
		SendClientMessage(playerid, 0xFFD700FF, "Скин %d в сумке. Надеть: /inv.", v);
	}
	else if (gDonateSel[playerid] == 5)
	{
		new len = strlen(input);
		if (len < 3 || len > 7 || input[0] == '0') { SendClientMessage(playerid, COLOR_GREY, "Номер из 3-7 цифр, не с нуля."); return; }
		new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id FROM accounts WHERE phone = %d LIMIT 1", v);
		new bool:busy = false;
		if (r != DBResult:0) { busy = DB_GetRowCount(r) > 0; DB_FreeResultSet(r); }
		if (busy) { SendClientMessage(playerid, COLOR_GREY, "Этот номер занят."); return; }
		if (!Donate_Spend(playerid, DONATE_NUMBER_PRICE, "номер телефона")) return;
		Player[playerid][pPhone] = v;
		Account_Save(playerid);
		SendClientMessage(playerid, 0xFFD700FF, "Твой новый номер: %d.", v);
	}
}

stock Donate_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	switch (dialogid)
	{
		case DIALOG_DONATE:
		{
			if (!response) return 1;
			gDonateSel[playerid] = listitem;
			Donate_Buy(playerid, listitem);
			return 1;
		}
		case DIALOG_DONATE_MONEY:
		{
			if (!response) { Cmd_Donate(playerid); return 1; }
			if (listitem < 0 || listitem >= sizeof (gDonMoneyPacks)) return 1;
			new coins = gDonMoneyPacks[listitem];
			if (!Donate_Spend(playerid, coins, "обмен на деньги")) return 1;
			GiveMoney(playerid, coins * DONATE_MONEY_RATE, "донат: обмен монет");
			Account_Save(playerid);
			SendClientMessage(playerid, 0xFFD700FF, "Обмен: %d монет -> $%d.", coins, coins * DONATE_MONEY_RATE);
			return 1;
		}
		case DIALOG_DONATE_INPUT:
		{
			if (!response) { Cmd_Donate(playerid); return 1; }
			Donate_Input(playerid, inputtext);
			return 1;
		}
	}
	return 0;
}

stock bool:Donate_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/donate", true))     { Cmd_Donate(playerid); return true; }
	if (!strcmp(cmd, "/vip", true))        { Cmd_Vip(playerid); return true; }
	if (!strcmp(cmd, "/givedonate", true)) { Cmd_GiveDonate(playerid, params); return true; }
	return false;
}
