// =====================================================================
//  marriage.inc — свадьба, развод, общий дом и семейный счёт.
//
//  Церковь (/place church). Стоя в церкви рядом с партнёром: /marry [id] —
//  предложение (MARRY_PRICE платит тот, кто предлагает), партнёр принимает Y или /accept.
//  Всем приходит объявление о свадьбе.
//  Супруги: дом одного — общий (входить в закрытый, /lock, спавн у дома).
//  Семейный счёт: /sbank — баланс, /sbank put [сумма], /sbank take [сумма]
//    (у банкомата или дома). Деньги видят и снимают оба.
//  /divorce — развод в церкви (DIVORCE_PRICE), деньги семейного счёта делятся пополам.
//  /spouse — кто супруг и онлайн ли он.
// =====================================================================

#define MARRY_PRICE         5000
#define DIVORCE_PRICE       2000
#define MARRY_MIN_LEVEL     2
#define SBANK_MAX           50000000

static gSpouse[MAX_PLAYERS];                // аккаунт супруга (0 — нет)
static gSpouseName[MAX_PLAYERS][MAX_PLAYER_NAME];
static gMarriageId[MAX_PLAYERS];

stock Marriage_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS marriages (id INTEGER PRIMARY KEY AUTOINCREMENT, a INTEGER, b INTEGER, a_name TEXT, b_name TEXT, bank INTEGER DEFAULT 0, since INTEGER)"));
}

stock Marriage_LoadPlayer(playerid)
{
	gSpouse[playerid] = 0;
	gMarriageId[playerid] = 0;
	gSpouseName[playerid][0] = '\0';
	new me = Player[playerid][pDbId];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, a, b, a_name, b_name FROM marriages WHERE a = %d OR b = %d LIMIT 1", me, me);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		gMarriageId[playerid] = DB_GetFieldInt(r, 0);
		if (DB_GetFieldInt(r, 1) == me)
		{
			gSpouse[playerid] = DB_GetFieldInt(r, 2);
			DB_GetFieldString(r, 4, gSpouseName[playerid], MAX_PLAYER_NAME);
		}
		else
		{
			gSpouse[playerid] = DB_GetFieldInt(r, 1);
			DB_GetFieldString(r, 3, gSpouseName[playerid], MAX_PLAYER_NAME);
		}
	}
	DB_FreeResultSet(r);
}

stock Marriage_GetSpouse(playerid) { return gSpouse[playerid]; }

// Дом супруга (-1 — нет)
stock Family_SpouseHouse(playerid)
{
	if (gSpouse[playerid] == 0) return -1;
	for (new h = 0; h < gHouseCount; h++)
		if (House[h][hOwner] == gSpouse[playerid]) return h;
	return -1;
}

stock bool:Family_IsSpouseOf(playerid, dbid)
{
	return dbid != 0 && gSpouse[playerid] == dbid;
}

static Marriage_GetBank(playerid)
{
	new bank = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT bank FROM marriages WHERE id = %d", gMarriageId[playerid]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) bank = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return bank;
}

static bool:Marriage_AtChurch(playerid)
{
	return Place_Nearby(playerid, PL_CHURCH, 25.0) != -1;
}

// ---------------------------------------------------------------------
//  Свадьба
// ---------------------------------------------------------------------
stock bool:Cmd_Marry(playerid, const params[])
{
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /marry [id]  (в церкви, рядом с партнёром; $%d)", MARRY_PRICE);
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	if (!Marriage_AtChurch(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Жениться можно только в церкви (/gps).");
	if (!IsPlayerNearPlayer(playerid, target, 4.0)) return SendClientMessage(playerid, COLOR_GREY, "Партнёр должен стоять рядом.");
	if (gSpouse[playerid] != 0) return SendClientMessage(playerid, COLOR_GREY, "Ты уже в браке. Развод: /divorce");
	if (gSpouse[target] != 0) return SendClientMessage(playerid, COLOR_GREY, "Этот человек уже в браке.");
	if (Player[playerid][pLevel] < MARRY_MIN_LEVEL || Player[target][pLevel] < MARRY_MIN_LEVEL) return SendClientMessage(playerid, COLOR_GREY, "Жениться можно с %d уровня (обоим).", MARRY_MIN_LEVEL);
	if (GetMoney(playerid) < MARRY_PRICE) return SendClientMessage(playerid, COLOR_GREY, "Свадьба стоит $%d.", MARRY_PRICE);
	if (Player[target][pOfferType] != OFFER_NONE) return SendClientMessage(playerid, COLOR_GREY, "Ему уже что-то предложили, подожди.");
	Player[target][pOfferType] = OFFER_MARRY;
	Player[target][pOfferFrom] = playerid;
	Player[target][pOfferFromDb] = Player[playerid][pDbId];
	Player[target][pOfferValue] = 0;
	new msg[128];
	format(msg, sizeof msg, "%s опускается на одно колено и протягивает кольцо %s.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(target, COLOR_YELLOW, "%s делает тебе предложение руки и сердца! Согласиться: Y или /accept, отказать: N.", GetRPName(playerid));
	return true;
}

// Из Cmd_Accept (factions.inc)
stock Marry_Accept(playerid, from)
{
	if (gSpouse[playerid] != 0 || gSpouse[from] != 0) return SendClientMessage(playerid, COLOR_GREY, "Кто-то из вас уже в браке.");
	if (!IsPlayerNearPlayer(playerid, from, 5.0) || !Marriage_AtChurch(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Нужно стоять рядом в церкви.");
	if (GetMoney(from) < MARRY_PRICE) return SendClientMessage(playerid, COLOR_GREY, "У жениха (невесты) не хватает денег на свадьбу.");
	GiveMoney(from, -MARRY_PRICE, "свадьба");
	Econ_ToTreasury(MARRY_PRICE);
	new na[MAX_PLAYER_NAME], nb[MAX_PLAYER_NAME];
	GetPlayerName(from, na, sizeof na);
	GetPlayerName(playerid, nb, sizeof nb);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO marriages (a, b, a_name, b_name, bank, since) VALUES (%d, %d, '%q', '%q', 0, %d)",
		Player[from][pDbId], Player[playerid][pDbId], na, nb, gettime()));
	Marriage_LoadPlayer(from);
	Marriage_LoadPlayer(playerid);
	Account_Save(from);
	ApplyAnimation(playerid, "KISSING", "Playa_Kiss_02", 4.1, false, false, false, false, 0, SYNC_ALL);
	ApplyAnimation(from, "KISSING", "Playa_Kiss_01", 4.1, false, false, false, false, 0, SYNC_ALL);
	new msg[144];
	format(msg, sizeof msg, "[Свадьба] %s и %s поженились! Поздравляем!", GetRPName(from), GetRPName(playerid));
	SendClientMessageToAll(0xFF69B4FF, msg);
	SendClientMessage(playerid, COLOR_WHITE, "Дом супруга теперь и твой, семейный счёт: /sbank. Супруг: /spouse");
	SendClientMessage(from, COLOR_WHITE, "Дом супруга теперь и твой, семейный счёт: /sbank. Супруг: /spouse");
	return 1;
}

stock bool:Cmd_Divorce(playerid)
{
	if (gSpouse[playerid] == 0) return SendClientMessage(playerid, COLOR_GREY, "Ты не в браке.");
	if (!Marriage_AtChurch(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Развод оформляют в церкви (/gps).");
	if (GetMoney(playerid) < DIVORCE_PRICE) return SendClientMessage(playerid, COLOR_GREY, "Развод стоит $%d.", DIVORCE_PRICE);
	GiveMoney(playerid, -DIVORCE_PRICE, "развод");
	Econ_ToTreasury(DIVORCE_PRICE);
	new bank = Marriage_GetBank(playerid), half = bank / 2;
	new other = gSpouse[playerid];
	// семейный счёт пополам — на личные счета в банке
	Player[playerid][pBank] += bank - half;
	new online = Bank_FindOnline(other);
	if (online != INVALID_PLAYER_ID)
	{
		Player[online][pBank] += half;
		gSpouse[online] = 0;
		gMarriageId[online] = 0;
		gSpouseName[online][0] = '\0';
		Account_Save(online);
		SendClientMessage(online, COLOR_ORANGE, "%s подал(а) на развод. Брак расторгнут. Половина семейного счёта ($%d) — на твоём счёте в банке.", GetRPName(playerid), half);
	}
	else DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET bank = bank + %d WHERE id = %d", half, other));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM marriages WHERE id = %d", gMarriageId[playerid]));
	gSpouse[playerid] = 0;
	gMarriageId[playerid] = 0;
	gSpouseName[playerid][0] = '\0';
	Account_Save(playerid);
	return SendClientMessage(playerid, COLOR_WHITE, "Брак расторгнут. Твоя половина семейного счёта ($%d) — на счёте в банке.", bank - half);
}

stock bool:Cmd_Spouse(playerid)
{
	if (gSpouse[playerid] == 0) return SendClientMessage(playerid, COLOR_GREY, "Ты не в браке. Свадьба — в церкви: /marry [id].");
	new online = Bank_FindOnline(gSpouse[playerid]);
	new h = Family_SpouseHouse(playerid);
	new house[24];
	if (House_GetOwned(playerid) != -1) format(house, sizeof house, "№%d (твой)", House[House_GetOwned(playerid)][hDbId]);
	else if (h != -1) format(house, sizeof house, "№%d", House[h][hDbId]);
	else strcat(house, "нет");
	SendClientMessage(playerid, 0xFF69B4FF, "Супруг(а): %s%s. Общий дом: %s. Семейный счёт: $%d (/sbank).", gSpouseName[playerid],
		(online != INVALID_PLAYER_ID) ? " — в сети" : "", house, Marriage_GetBank(playerid));
	return true;
}

// ---------------------------------------------------------------------
//  Семейный счёт
// ---------------------------------------------------------------------
stock bool:Cmd_SBank(playerid, const params[])
{
	if (gSpouse[playerid] == 0) return SendClientMessage(playerid, COLOR_GREY, "Семейный счёт есть только у супругов.");
	new pos = 0, what[8], amount;
	new bank = Marriage_GetBank(playerid);
	if (!NextParam(params, pos, what))
		return SendClientMessage(playerid, 0xFF69B4FF, "Семейный счёт: $%d. Положить: /sbank put [сумма], снять: /sbank take [сумма] (у банкомата или дома).", bank);
	new h = Player[playerid][pHouseInside];
	new bool:home = (h != -1 && House_IsInside(playerid, h) && (House[h][hOwner] == Player[playerid][pDbId] || House[h][hOwner] == gSpouse[playerid]));
	if (!Bank_CanUse(playerid) && !home) return SendClientMessage(playerid, COLOR_GREY, "Нужно стоять у банкомата или быть в своём доме.");
	if (!NextIntParam(params, pos, amount) || amount < 1) return SendClientMessage(playerid, COLOR_GREY, "Использование: /sbank put|take [сумма]");
	new msg[96];
	if (!strcmp(what, "put", true))
	{
		if (GetMoney(playerid) < amount) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет столько наличных.");
		if (bank + amount > SBANK_MAX) return SendClientMessage(playerid, COLOR_GREY, "Счёт переполнен.");
		GiveMoney(playerid, -amount, "на семейный счёт");
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE marriages SET bank = bank + %d WHERE id = %d", amount, gMarriageId[playerid]));
		format(msg, sizeof msg, "%s положил(а) на семейный счёт $%d.", GetRPName(playerid), amount);
	}
	else if (!strcmp(what, "take", true))
	{
		if (bank < amount) return SendClientMessage(playerid, COLOR_GREY, "На семейном счёте только $%d.", bank);
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE marriages SET bank = bank - %d WHERE id = %d", amount, gMarriageId[playerid]));
		GiveMoney(playerid, amount, "с семейного счёта");
		format(msg, sizeof msg, "%s снял(а) с семейного счёта $%d.", GetRPName(playerid), amount);
	}
	else return SendClientMessage(playerid, COLOR_GREY, "Использование: /sbank put|take [сумма]");
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "%s Остаток: $%d.", msg, Marriage_GetBank(playerid));
	new online = Bank_FindOnline(gSpouse[playerid]);
	if (online != INVALID_PLAYER_ID) SendClientMessage(online, 0xFF69B4FF, "%s", msg);
	return true;
}

stock bool:Marriage_TryAlt(playerid)
{
	if (Place_Nearby(playerid, PL_CHURCH, 2.5) == -1) return false;
	if (gSpouse[playerid] == 0) SendClientMessage(playerid, 0xFF69B4FF, "Церковь. Свадьба: встань рядом с партнёром и введи /marry [id] ($%d).", MARRY_PRICE);
	else SendClientMessage(playerid, 0xFF69B4FF, "Церковь. Ты в браке с %s. Развод: /divorce ($%d).", gSpouseName[playerid], DIVORCE_PRICE);
	return true;
}

stock bool:Marriage_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/marry", true))   { Cmd_Marry(playerid, params); return true; }
	if (!strcmp(cmd, "/divorce", true)) { Cmd_Divorce(playerid); return true; }
	if (!strcmp(cmd, "/spouse", true))  { Cmd_Spouse(playerid); return true; }
	if (!strcmp(cmd, "/sbank", true))   { Cmd_SBank(playerid, params); return true; }
	return false;
}
