// =====================================================================
//  season.inc — сезонный пропуск: /season (/pass).
//
//  Сезон длится SEASON_DAYS дней, потом сам начинается следующий
//  (проверка каждый PayDay). Игрок копит очки сезона (XP):
//    PayDay                  SEASON_XP_PAYDAY
//    задание на работе       SEASON_XP_JOB
//    ежедневное задание      SEASON_XP_DAILY  (daily.inc)
//    все ежедневные за день  SEASON_XP_DAILY_ALL
//  Каждые SEASON_XP_LEVEL очков — новый уровень, всего SEASON_LEVELS.
//
//  Две дорожки наград: бесплатная — у всех, премиум — за SEASON_PREMIUM_PRICE
//  донат-монет (/season > Купить). Награда выдаётся сама при получении уровня.
//  Купил премиум посреди сезона — сразу получаешь премиум-награды за все
//  пройденные уровни. Новый сезон: уровень, очки и премиум обнуляются.
//
//  Награды — таблицы gSeasonFree / gSeasonPrem ниже: { вид, значение, значение2 }.
//    SR_MONEY $  · SR_EXP опыт · SR_COINS монеты · SR_ITEM предмет, кол-во · SR_VIP уровень, дни
//
//  Админ 6+: /newseason [дней] — закончить текущий и начать новый сезон,
//            /seasonxp [id] [очки] — добавить очки игроку.
//  Окно: HTML «season» (ПК и APK), без клиента Bastion — диалог.
//  Хранится: таблица seasons; accounts.season_id, season_xp, season_prem, season_free_got, season_prem_got.
// =====================================================================

#define SEASON_DAYS             30
#define SEASON_LEVELS           30
#define SEASON_XP_LEVEL         600
#define SEASON_XP_PAYDAY        60
#define SEASON_XP_JOB           5
#define SEASON_XP_DAILY         100
#define SEASON_XP_DAILY_ALL     150
#define SEASON_PREMIUM_PRICE    300     // монет

enum { SR_NONE, SR_MONEY, SR_EXP, SR_COINS, SR_ITEM, SR_VIP }

static const gSeasonFree[SEASON_LEVELS][3] =
{
	{ SR_MONEY, 2000, 0 },        { SR_ITEM, INV_BURGER, 3 },   { SR_MONEY, 3000, 0 },        { SR_ITEM, INV_REPAIRKIT, 1 }, { SR_MONEY, 5000, 0 },
	{ SR_ITEM, INV_MEDKIT, 1 },   { SR_MONEY, 5000, 0 },        { SR_ITEM, INV_WATER, 5 },    { SR_MONEY, 7000, 0 },         { SR_EXP, 1, 0 },
	{ SR_MONEY, 7000, 0 },        { SR_ITEM, INV_PIZZA, 3 },    { SR_MONEY, 8000, 0 },        { SR_ITEM, INV_REPAIRKIT, 2 }, { SR_COINS, 10, 0 },
	{ SR_MONEY, 10000, 0 },       { SR_ITEM, INV_MEDKIT, 2 },   { SR_MONEY, 10000, 0 },       { SR_ITEM, INV_BURGER, 5 },    { SR_EXP, 1, 0 },
	{ SR_MONEY, 12000, 0 },       { SR_ITEM, INV_REPAIRKIT, 2 },{ SR_MONEY, 12000, 0 },       { SR_ITEM, INV_MEDKIT, 3 },    { SR_COINS, 15, 0 },
	{ SR_MONEY, 15000, 0 },       { SR_ITEM, INV_PIZZA, 5 },    { SR_MONEY, 20000, 0 },       { SR_EXP, 1, 0 },              { SR_MONEY, 30000, 0 }
};

static const gSeasonPrem[SEASON_LEVELS][3] =
{
	{ SR_MONEY, 10000, 0 },       { SR_COINS, 5, 0 },           { SR_MONEY, 10000, 0 },       { SR_ITEM, INV_MEDKIT, 3 },    { SR_VIP, 1, 3 },
	{ SR_MONEY, 15000, 0 },       { SR_COINS, 10, 0 },          { SR_MONEY, 15000, 0 },       { SR_ITEM, INV_REPAIRKIT, 3 }, { SR_EXP, 2, 0 },
	{ SR_MONEY, 20000, 0 },       { SR_COINS, 10, 0 },          { SR_MONEY, 20000, 0 },       { SR_ITEM, INV_MASK, 1 },      { SR_VIP, 2, 3 },
	{ SR_MONEY, 25000, 0 },       { SR_COINS, 15, 0 },          { SR_MONEY, 25000, 0 },       { SR_ITEM, INV_MEDKIT, 5 },    { SR_EXP, 2, 0 },
	{ SR_MONEY, 30000, 0 },       { SR_COINS, 15, 0 },          { SR_MONEY, 30000, 0 },       { SR_ITEM, INV_REPAIRKIT, 5 }, { SR_VIP, 2, 5 },
	{ SR_MONEY, 40000, 0 },       { SR_COINS, 20, 0 },          { SR_MONEY, 50000, 0 },       { SR_EXP, 3, 0 },              { SR_VIP, 3, 7 }
};

static gSeasonId;               // номер текущего сезона
static gSeasonEnds;             // когда кончается (unix)

static gSPSeason[MAX_PLAYERS];  // в каком сезоне копил очки
static gSPXp[MAX_PLAYERS];
static bool:gSPPrem[MAX_PLAYERS];
static gSPFreeGot[MAX_PLAYERS]; // биты: награда уровня выдана
static gSPPremGot[MAX_PLAYERS];
static bool:gSPLoaded[MAX_PLAYERS];

stock Season_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS seasons (id INTEGER PRIMARY KEY, starts INTEGER, ends INTEGER)"));
	DB_AddColumn("accounts", "season_id", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "season_xp", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "season_prem", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "season_free_got", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "season_prem_got", "INTEGER DEFAULT 0");
	gSeasonId = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, ends FROM seasons ORDER BY id DESC LIMIT 1");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) { gSeasonId = DB_GetFieldInt(r, 0); gSeasonEnds = DB_GetFieldInt(r, 1); }
		DB_FreeResultSet(r);
	}
	if (gSeasonId == 0) Season_Start(SEASON_DAYS);
}

static Season_Start(days)
{
	new now = gettime();
	gSeasonId++;
	gSeasonEnds = now + days * 86400;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO seasons (id, starts, ends) VALUES (%d, %d, %d)", gSeasonId, now, gSeasonEnds));
	printf("[season] начался сезон %d на %d дн.", gSeasonId, days);
}

static Season_Save(playerid)
{
	if (Player[playerid][pDbId] == 0 || !gSPLoaded[playerid]) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET season_id = %d, season_xp = %d, season_prem = %d, season_free_got = %d, season_prem_got = %d WHERE id = %d",
		gSPSeason[playerid], gSPXp[playerid], gSPPrem[playerid] ? 1 : 0, gSPFreeGot[playerid], gSPPremGot[playerid], Player[playerid][pDbId]));
}

// Игрок копил в прошлом сезоне — начать заново
static Season_Sync(playerid)
{
	if (gSPSeason[playerid] == gSeasonId) return;
	gSPSeason[playerid] = gSeasonId;
	gSPXp[playerid] = 0;
	gSPPrem[playerid] = false;
	gSPFreeGot[playerid] = 0;
	gSPPremGot[playerid] = 0;
	Season_Save(playerid);
}

static Season_Level(playerid)
{
	new l = gSPXp[playerid] / SEASON_XP_LEVEL;
	return (l > SEASON_LEVELS) ? SEASON_LEVELS : l;
}

static Season_DaysLeft()
{
	new left = gSeasonEnds - gettime();
	return (left <= 0) ? 0 : (left + 86399) / 86400;
}

// Текст награды: "$5000", "Аптечка x2", "VIP Bronze 3 дн."
static Season_RewardText(dest[], size, kind, v1, v2)
{
	switch (kind)
	{
		case SR_MONEY: format(dest, size, "$%d", v1);
		case SR_EXP:   format(dest, size, "+%d опыта", v1);
		case SR_COINS: format(dest, size, "%d монет", v1);
		case SR_ITEM:  format(dest, size, "%s x%d", Inv_GetName(v1), v2);
		case SR_VIP:   format(dest, size, "VIP %s %d дн.", Vip_LevelName(v1), v2);
		default:       format(dest, size, "-");
	}
}

static Season_RewardKey(kind)
{
	new k[8];
	switch (kind)
	{
		case SR_MONEY: strcat(k, "money");
		case SR_EXP:   strcat(k, "exp");
		case SR_COINS: strcat(k, "coins");
		case SR_ITEM:  strcat(k, "item");
		case SR_VIP:   strcat(k, "vip");
	}
	return k;
}

static Season_Give(playerid, kind, v1, v2)
{
	switch (kind)
	{
		case SR_MONEY: GiveMoney(playerid, v1, "сезонный пропуск");
		case SR_EXP:   Player[playerid][pExp] += v1;
		case SR_COINS: Donate_AddCoins(playerid, v1);
		case SR_ITEM:  if (!Inv_Add(playerid, v1, v2)) { GiveMoney(playerid, 1000 * v2, "сезонный пропуск: сумка полна"); SendClientMessage(playerid, COLOR_GREY, "Сумка полна: вместо предмета $%d.", 1000 * v2); }
		case SR_VIP:   Vip_Grant(playerid, v1, v2);
	}
}

// Выдать все положенные, но ещё не выданные награды
static Season_GiveDue(playerid)
{
	new lvl = Season_Level(playerid), text[48], bool:any = false;
	for (new l = 0; l < lvl; l++)
	{
		if (!(gSPFreeGot[playerid] & (1 << l)))
		{
			gSPFreeGot[playerid] |= (1 << l);
			Season_Give(playerid, gSeasonFree[l][0], gSeasonFree[l][1], gSeasonFree[l][2]);
			Season_RewardText(text, sizeof text, gSeasonFree[l][0], gSeasonFree[l][1], gSeasonFree[l][2]);
			SendClientMessage(playerid, COLOR_GREEN, "[Сезон] Уровень %d: %s.", l + 1, text);
			any = true;
		}
		if (gSPPrem[playerid] && !(gSPPremGot[playerid] & (1 << l)))
		{
			gSPPremGot[playerid] |= (1 << l);
			Season_Give(playerid, gSeasonPrem[l][0], gSeasonPrem[l][1], gSeasonPrem[l][2]);
			Season_RewardText(text, sizeof text, gSeasonPrem[l][0], gSeasonPrem[l][1], gSeasonPrem[l][2]);
			SendClientMessage(playerid, 0xFFD700FF, "[Сезон] Премиум, уровень %d: %s.", l + 1, text);
			any = true;
		}
	}
	if (any) { Season_Save(playerid); Account_Save(playerid); }
}

// Добавить очки сезона (из daily.inc, jobs.inc, PayDay)
stock Season_AddXp(playerid, xp)
{
	if (!gSPLoaded[playerid] || xp <= 0) return;
	Season_Sync(playerid);
	new before = Season_Level(playerid);
	gSPXp[playerid] += xp;
	new after = Season_Level(playerid);
	if (after > before)
	{
		PlayerPlaySound(playerid, 1183, 0.0, 0.0, 0.0);
		GameTextForPlayer(playerid, "~y~Season level %d", 4000, 1, after);
		SendClientMessage(playerid, COLOR_YELLOW, "[Сезон] Новый уровень сезонного пропуска: %d/%d. Награды: /season", after, SEASON_LEVELS);
		if (!gSPPrem[playerid] && after < SEASON_LEVELS)
			SendClientMessage(playerid, COLOR_GREY, "С премиум-пропуском за этот уровень было бы больше. Купить: /season (%d монет).", SEASON_PREMIUM_PRICE);
		Season_GiveDue(playerid);
	}
	else Season_Save(playerid);
}

static Season_BuyPremium(playerid)
{
	Season_Sync(playerid);
	if (gSPPrem[playerid]) { SendClientMessage(playerid, COLOR_GREY, "Премиум-пропуск этого сезона у тебя уже есть."); return; }
	if (Season_DaysLeft() < 1) { SendClientMessage(playerid, COLOR_GREY, "Сезон заканчивается. Купи премиум в следующем."); return; }
	if (!Donate_SpendCoins(playerid, SEASON_PREMIUM_PRICE, "премиум-пропуск сезона")) return;
	gSPPrem[playerid] = true;
	Season_Save(playerid);
	SendClientMessage(playerid, 0xFFD700FF, "[Сезон] Премиум-пропуск сезона %d куплен! Награды за пройденные уровни уже выданы.", gSeasonId);
	Season_GiveDue(playerid);
}

// ---------------------------------------------------------------------
//  Окно
// ---------------------------------------------------------------------
stock Season_Show(playerid)
{
	Season_Sync(playerid);
	new lvl = Season_Level(playerid);
	if (UI_Has(playerid))
	{
		new json[4000], item[120], t1[48], t2[48];
		format(json, sizeof json, "{\"id\":%d,\"days\":%d,\"lvl\":%d,\"max\":%d,\"xp\":%d,\"per\":%d,\"prem\":%d,\"price\":%d,\"coins\":%d,\"src\":[%d,%d,%d,%d],\"levels\":[",
			gSeasonId, Season_DaysLeft(), lvl, SEASON_LEVELS, gSPXp[playerid] % SEASON_XP_LEVEL, SEASON_XP_LEVEL, gSPPrem[playerid] ? 1 : 0,
			SEASON_PREMIUM_PRICE, Donate_GetCoins(playerid), SEASON_XP_PAYDAY, SEASON_XP_JOB, SEASON_XP_DAILY, SEASON_XP_DAILY_ALL);
		for (new l = 0; l < SEASON_LEVELS; l++)
		{
			Season_RewardText(t1, sizeof t1, gSeasonFree[l][0], gSeasonFree[l][1], gSeasonFree[l][2]);
			Season_RewardText(t2, sizeof t2, gSeasonPrem[l][0], gSeasonPrem[l][1], gSeasonPrem[l][2]);
			format(item, sizeof item, "%s[\"%s\",\"%s\",\"%s\",\"%s\"]", l ? "," : "", Season_RewardKey(gSeasonFree[l][0]), t1, Season_RewardKey(gSeasonPrem[l][0]), t2);
			strcat(json, item);
		}
		strcat(json, "]}");
		// Ответ: 1/1 — купить премиум; 0 — закрыть
		UI_Show(playerid, DIALOG_SEASON_UI, DIALOG_STYLE_LIST, "season", json, "Понятно", "Закрыть");
		return;
	}
	new text[3000], line[140], t1[48], t2[48];
	format(text, sizeof text, "Уровень\tБесплатно\tПремиум%s\n", gSPPrem[playerid] ? " (есть)" : "");
	for (new l = 0; l < SEASON_LEVELS; l++)
	{
		Season_RewardText(t1, sizeof t1, gSeasonFree[l][0], gSeasonFree[l][1], gSeasonFree[l][2]);
		Season_RewardText(t2, sizeof t2, gSeasonPrem[l][0], gSeasonPrem[l][1], gSeasonPrem[l][2]);
		format(line, sizeof line, "%s%d\t%s\t%s%s\n", (l < lvl) ? "{33AA33}" : (l == lvl ? "{FFFF00}" : "{FFFFFF}"), l + 1, t1, gSPPrem[playerid] ? "" : "{AAAAAA}", t2);
		strcat(text, line);
	}
	new cap[80];
	format(cap, sizeof cap, "Сезон %d · ур. %d/%d · %d/%d очков · %d дн.", gSeasonId, lvl, SEASON_LEVELS, gSPXp[playerid] % SEASON_XP_LEVEL, SEASON_XP_LEVEL, Season_DaysLeft());
	ShowPlayerDialog(playerid, DIALOG_SEASON, DIALOG_STYLE_TABLIST_HEADERS, cap, "%s", gSPPrem[playerid] ? "Закрыть" : "Премиум", gSPPrem[playerid] ? "" : "Закрыть", text);
}

stock Season_OnDialog(playerid, dialogid, response, listitem)
{
	switch (dialogid)
	{
		case DIALOG_SEASON_UI:
		{
			if (response && listitem == 1) { Season_BuyPremium(playerid); Season_Show(playerid); }
			return 1;
		}
		case DIALOG_SEASON:
		{
			if (response && !gSPPrem[playerid])
				ShowPlayerDialog(playerid, DIALOG_SEASON_BUY, DIALOG_STYLE_MSGBOX, "Премиум-пропуск",
					"{FFFFFF}Купить премиум-пропуск сезона за {FFD700}%d монет{FFFFFF}?\nУ тебя %d монет. Награды за уже пройденные уровни выдадутся сразу.", "Купить", "Отмена",
					SEASON_PREMIUM_PRICE, Donate_GetCoins(playerid));
			return 1;
		}
		case DIALOG_SEASON_BUY:
		{
			if (response) Season_BuyPremium(playerid);
			return 1;
		}
	}
	return 0;
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Season_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/season", true) || !strcmp(cmd, "/pass", true))
	{
		if (Player[playerid][pLogged]) Season_Show(playerid);
		return true;
	}
	if (!strcmp(cmd, "/newseason", true))
	{
		if (!Admin_Check(playerid, 6)) return true;
		new pos = 0, days;
		if (!NextIntParam(params, pos, days) || days < 1 || days > 120)
		{
			SendClientMessage(playerid, COLOR_GREY, "Используй: /newseason [дней 1-120]. Сейчас сезон %d, осталось %d дн.", gSeasonId, Season_DaysLeft());
			return true;
		}
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE seasons SET ends = %d WHERE id = %d", gettime(), gSeasonId));
		Season_Start(days);
		Admin_Log(playerid, "newseason", playerid, "", gSeasonId);
		Season_Announce();
		return true;
	}
	if (!strcmp(cmd, "/seasonxp", true))
	{
		if (!Admin_Check(playerid, 6)) return true;
		new pos = 0, xp;
		new target = NextPlayerParam(params, pos);
		if (target == INVALID_PLAYER_ID || !Player[target][pLogged] || !NextIntParam(params, pos, xp) || xp < 1)
		{
			SendClientMessage(playerid, COLOR_GREY, "Используй: /seasonxp [id] [очки]");
			return true;
		}
		Season_AddXp(target, xp);
		Admin_Log(playerid, "seasonxp", target, "", xp);
		SendClientMessage(playerid, COLOR_WHITE, "%s: +%d очков сезона (уровень %d).", GetRPName(target), xp, Season_Level(target));
		return true;
	}
	return false;
}

static Season_Announce()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !gSPLoaded[i]) continue;
		Season_Sync(i);
		SendClientMessage(i, COLOR_YELLOW, "[Сезон] Начался сезон %d! Новые награды за уровни: /season", gSeasonId);
	}
}

// ---------------------------------------------------------------------
//  Вход, выход, PayDay
// ---------------------------------------------------------------------
stock Season_OnLogin(playerid)
{
	gSPLoaded[playerid] = false;
	gSPSeason[playerid] = 0; gSPXp[playerid] = 0; gSPPrem[playerid] = false; gSPFreeGot[playerid] = 0; gSPPremGot[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT season_id, season_xp, season_prem, season_free_got, season_prem_got FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			gSPSeason[playerid] = DB_GetFieldInt(r, 0);
			gSPXp[playerid] = DB_GetFieldInt(r, 1);
			gSPPrem[playerid] = DB_GetFieldInt(r, 2) != 0;
			gSPFreeGot[playerid] = DB_GetFieldInt(r, 3);
			gSPPremGot[playerid] = DB_GetFieldInt(r, 4);
		}
		DB_FreeResultSet(r);
	}
	gSPLoaded[playerid] = true;
	Season_Sync(playerid);
	Season_GiveDue(playerid);
}

stock Season_OnDisconnect(playerid)
{
	gSPLoaded[playerid] = false;
}

// Раз в час (до выдачи PayDay игрокам): сезон кончился — начать новый
stock Season_HourCheck()
{
	if (gettime() < gSeasonEnds) return;
	Season_Start(SEASON_DAYS);
	Season_Announce();
}

stock Season_OnPayDay(playerid) { Season_AddXp(playerid, SEASON_XP_PAYDAY); }
stock Season_OnJobTask(playerid) { Season_AddXp(playerid, SEASON_XP_JOB); }
stock Season_OnDaily(playerid)   { Season_AddXp(playerid, SEASON_XP_DAILY); }
stock Season_OnDailyAll(playerid) { Season_AddXp(playerid, SEASON_XP_DAILY_ALL); }
