// =====================================================================
//  daily.inc — ежедневные задания: /daily.
//
//  Каждый день (сброс в 00:00 по МСК, DAILY_TZ_SHIFT) игрок получает
//  DAILY_TASKS случайных заданий из списка ниже: поработать, заработать,
//  получить зарплату, поесть, положить деньги в банк, заправиться,
//  потратить деньги в бизнесах. За каждое — деньги сразу при выполнении.
//  Все задания дня выполнены — бонус: деньги + опыт.
//
//  Серия: выполнил все задания несколько дней подряд — бонус больше на
//  DAILY_STREAK_PCT% за каждый день серии (до DAILY_STREAK_MAX дней).
//  Каждый DAILY_STREAK_MAX-й день серии — ещё DAILY_STREAK_COINS донат-монет.
//  Пропустил день — серия с начала.
//
//  Одно задание в день можно заменить на другое за DAILY_SWAP_PRICE.
//  Награды растут с уровнем: +DAILY_LEVEL_PCT% за уровень (до 20 уровня).
//
//  Окно: HTML «daily» в клиенте Bastion (ПК и APK), картинки ui/daily/<key>.png,
//  без клиента Bastion — обычный диалог.
//  Хранится в accounts: daily_day, daily_data, daily_flags, daily_streak, daily_last.
// =====================================================================

#define DAILY_TASKS         3
#define DAILY_TZ_SHIFT      10800       // сдвиг от UTC: сброс заданий в полночь по МСК
#define DAILY_BONUS_MONEY   2500        // за все задания дня
#define DAILY_BONUS_EXP     1
#define DAILY_STREAK_PCT    15          // +15% к бонусу за каждый день серии
#define DAILY_STREAK_MAX    7
#define DAILY_STREAK_COINS  2           // каждый 7-й день серии
#define DAILY_SWAP_PRICE    500
#define DAILY_LEVEL_PCT     5

enum
{
	DT_JOB,         // выполнить заданий на работе
	DT_JOBMONEY,    // заработать на работе, $
	DT_PAYDAY,      // получить зарплату (PayDay)
	DT_EAT,         // поесть
	DT_DEPOSIT,     // положить в банк, $
	DT_FUEL,        // заправить литров на АЗС
	DT_SPEND,       // потратить в бизнесах, $
	DT_COUNT
}

enum E_DAILY_TYPE
{
	dtKey[10],          // картинка: ui/daily/<key>.png
	dtTitle[48],        // %d — сколько нужно
	dtHint[120],
	dtMin,
	dtMax,
	dtStep,             // need округляется до шага
	dtReward
}

static const gDailyTypes[DT_COUNT][E_DAILY_TYPE] =
{
	{ "job",     "Выполни %d заданий на работе",   "Любая работа: /getjob у значка работы, затем /work.",               8,    15,   1,    900 },
	{ "money",   "Заработай $%d на работе",       "Считаются деньги за задания на любой работе.",                      1500, 3500, 500,  800 },
	{ "payday",  "Получи %d зарплаты",              "Зарплата (PayDay) приходит в начале каждого часа, если ты в игре.", 2,    3,    1,    700 },
	{ "food",    "Поешь %d раза",                   "Купи еду в 24/7 или закусочной, или съешь из сумки (/inv).",         2,    3,    1,    300 },
	{ "bank",    "Положи в банк $%d",               "Банкомат или зал банка, ALT, «Пополнить счёт».",                     2000, 5000, 1000, 400 },
	{ "fuel",    "Заправь %d л бензина",            "Любая АЗС: заедь на заправку и нажми H.",                            20,   40,   10,   400 },
	{ "shop",    "Потрать $%d в бизнесах",          "Покупки в магазинах, закусочных, барах и других бизнесах города.",   1000, 3000, 500,  500 }
};

static gDailyDay[MAX_PLAYERS];
static gDailyType[MAX_PLAYERS][DAILY_TASKS];
static gDailyNeed[MAX_PLAYERS][DAILY_TASKS];
static gDailyProg[MAX_PLAYERS][DAILY_TASKS];
static gDailyFlags[MAX_PLAYERS];        // биты 0..2 — задание выполнено, бит 4 — бонус получен, бит 5 — замена использована
static gDailyStreak[MAX_PLAYERS];
static gDailyLast[MAX_PLAYERS];         // последний день, когда выполнены все задания
static bool:gDailyLoaded[MAX_PLAYERS];

#define DAILY_F_BONUS   16
#define DAILY_F_SWAP    32

stock Daily_Init()
{
	DB_AddColumn("accounts", "daily_day", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "daily_data", "TEXT DEFAULT ''");
	DB_AddColumn("accounts", "daily_flags", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "daily_streak", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "daily_last", "INTEGER DEFAULT 0");
}

static Daily_Today()
{
	return (gettime() + DAILY_TZ_SHIFT) / 86400;
}

static Daily_Save(playerid)
{
	if (Player[playerid][pDbId] == 0 || !gDailyLoaded[playerid]) return;
	new data[96];
	format(data, sizeof data, "%d:%d:%d,%d:%d:%d,%d:%d:%d",
		gDailyType[playerid][0], gDailyNeed[playerid][0], gDailyProg[playerid][0],
		gDailyType[playerid][1], gDailyNeed[playerid][1], gDailyProg[playerid][1],
		gDailyType[playerid][2], gDailyNeed[playerid][2], gDailyProg[playerid][2]);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET daily_day = %d, daily_data = '%s', daily_flags = %d, daily_streak = %d, daily_last = %d WHERE id = %d",
		gDailyDay[playerid], data, gDailyFlags[playerid], gDailyStreak[playerid], gDailyLast[playerid], Player[playerid][pDbId]));
}

// Разобрать "t:n:p,t:n:p,t:n:p". false — строка битая
static bool:Daily_Parse(playerid, const s[])
{
	new nums[DAILY_TASKS * 3], cnt = 0, val = 0, bool:any = false;
	for (new i = 0; ; i++)
	{
		new c = s[i];
		if (c >= '0' && c <= '9') { val = val * 10 + (c - '0'); any = true; continue; }
		if (any && cnt < sizeof nums) nums[cnt++] = val;
		val = 0; any = false;
		if (c == '\0') break;
	}
	if (cnt != sizeof nums) return false;
	for (new t = 0; t < DAILY_TASKS; t++)
	{
		if (nums[t * 3] < 0 || nums[t * 3] >= DT_COUNT || nums[t * 3 + 1] < 1) return false;
		gDailyType[playerid][t] = nums[t * 3];
		gDailyNeed[playerid][t] = nums[t * 3 + 1];
		gDailyProg[playerid][t] = nums[t * 3 + 2];
	}
	return true;
}

static Daily_RandomNeed(type)
{
	new mn = gDailyTypes[type][dtMin], mx = gDailyTypes[type][dtMax], st = gDailyTypes[type][dtStep];
	new steps = (mx - mn) / st;
	return mn + random(steps + 1) * st;
}

static bool:Daily_HasType(playerid, type, except = -1)
{
	for (new t = 0; t < DAILY_TASKS; t++) if (t != except && gDailyType[playerid][t] == type) return true;
	return false;
}

// Случайный тип, которого ещё нет среди заданий (кроме слота except)
static Daily_PickType(playerid, except = -1, avoid = -1)
{
	new pool[DT_COUNT], n = 0;
	for (new d = 0; d < DT_COUNT; d++)
		if (d != avoid && !Daily_HasType(playerid, d, except)) pool[n++] = d;
	return n ? pool[random(n)] : 0;
}

static Daily_Roll(playerid, day)
{
	for (new t = 0; t < DAILY_TASKS; t++) gDailyType[playerid][t] = -1;
	for (new t = 0; t < DAILY_TASKS; t++)
	{
		gDailyType[playerid][t] = Daily_PickType(playerid, t);
		gDailyNeed[playerid][t] = Daily_RandomNeed(gDailyType[playerid][t]);
		gDailyProg[playerid][t] = 0;
	}
	gDailyDay[playerid] = day;
	gDailyFlags[playerid] = 0;
	// пропущен хотя бы один день — серия с начала
	if (gDailyLast[playerid] < day - 1) gDailyStreak[playerid] = 0;
	Daily_Save(playerid);
}

static Daily_Reward(playerid, base)
{
	new lvl = Player[playerid][pLevel];
	if (lvl > 20) lvl = 20;
	return base + base * lvl * DAILY_LEVEL_PCT / 100;
}

static Daily_BonusMoney(playerid, streak)
{
	if (streak > DAILY_STREAK_MAX) streak = DAILY_STREAK_MAX;
	new base = Daily_Reward(playerid, DAILY_BONUS_MONEY);
	return base + base * (streak - 1) * DAILY_STREAK_PCT / 100;
}

// Новый день: выдать задания. true — задания обновились
static bool:Daily_Check(playerid)
{
	if (!gDailyLoaded[playerid]) return false;
	new day = Daily_Today();
	if (gDailyDay[playerid] == day) return false;
	Daily_Roll(playerid, day);
	return true;
}

static Daily_Title(dest[], size, type, need)
{
	format(dest, size, gDailyTypes[type][dtTitle], need);
}

static Daily_DoneCount(playerid)
{
	new c = 0;
	for (new t = 0; t < DAILY_TASKS; t++) if (gDailyFlags[playerid] & (1 << t)) c++;
	return c;
}

// Время до новых заданий: "чч:мм"
static Daily_Left(dest[], size)
{
	new left = 86400 - (gettime() + DAILY_TZ_SHIFT) % 86400;
	format(dest, size, "%d:%02d", left / 3600, (left % 3600) / 60);
}

static Daily_AllDone(playerid)
{
	new streak = (gDailyLast[playerid] == gDailyDay[playerid] - 1) ? gDailyStreak[playerid] + 1 : 1;
	gDailyStreak[playerid] = streak;
	gDailyLast[playerid] = gDailyDay[playerid];
	gDailyFlags[playerid] |= DAILY_F_BONUS;
	new money = Daily_BonusMoney(playerid, streak);
	GiveMoney(playerid, money, "ежедневные задания: бонус");
	Player[playerid][pExp] += DAILY_BONUS_EXP;
	SendClientMessage(playerid, COLOR_GREEN, "[Ежедневно] Все задания дня выполнены! Бонус: $%d и +%d опыта. Серия: %d дн.", money, DAILY_BONUS_EXP, streak);
	if (streak % DAILY_STREAK_MAX == 0 && DAILY_STREAK_COINS > 0)
	{
		Donate_AddCoins(playerid, DAILY_STREAK_COINS);
		SendClientMessage(playerid, COLOR_GREEN, "[Ежедневно] %d дней подряд! Награда: %d донат-монет.", streak, DAILY_STREAK_COINS);
	}
	else if (streak < DAILY_STREAK_MAX)
		SendClientMessage(playerid, COLOR_WHITE, "Заходи завтра: бонус вырастет ещё на %d%%. На %d-й день подряд — донат-монеты.", DAILY_STREAK_PCT, DAILY_STREAK_MAX);
	GameTextForPlayer(playerid, "~g~Daily bonus +$%d", 4000, 1, money);
	PlayerPlaySound(playerid, 1183, 0.0, 0.0, 0.0);
	Season_OnDailyAll(playerid);   // сезонный пропуск (season.inc)
}

// Прогресс по заданию типа type: +add
static Daily_Progress(playerid, type, add, bool:quiet = false)
{
	if (!Player[playerid][pLogged] || !gDailyLoaded[playerid] || add <= 0) return;
	if (Daily_Check(playerid)) SendClientMessage(playerid, COLOR_YELLOW, "[Ежедневно] Новый день — новые задания: /daily");
	for (new t = 0; t < DAILY_TASKS; t++)
	{
		if (gDailyType[playerid][t] != type || (gDailyFlags[playerid] & (1 << t))) continue;
		gDailyProg[playerid][t] += add;
		new title[64];
		Daily_Title(title, sizeof title, type, gDailyNeed[playerid][t]);
		if (gDailyProg[playerid][t] >= gDailyNeed[playerid][t])
		{
			gDailyProg[playerid][t] = gDailyNeed[playerid][t];
			gDailyFlags[playerid] |= (1 << t);
			new money = Daily_Reward(playerid, gDailyTypes[type][dtReward]);
			GiveMoney(playerid, money, "ежедневное задание");
			PlayerPlaySound(playerid, 1058, 0.0, 0.0, 0.0);
			SendClientMessage(playerid, COLOR_GREEN, "[Ежедневно] Выполнено: %s. Награда: $%d. Готово %d/%d.", title, money, Daily_DoneCount(playerid), DAILY_TASKS);
			Season_OnDaily(playerid);   // сезонный пропуск (season.inc)
			if (Daily_DoneCount(playerid) == DAILY_TASKS && !(gDailyFlags[playerid] & DAILY_F_BONUS)) Daily_AllDone(playerid);
		}
		else if (!quiet)
			SendClientMessage(playerid, COLOR_YELLOW, "[Ежедневно] %s: %d/%d.", title, gDailyProg[playerid][t], gDailyNeed[playerid][t]);
		Daily_Save(playerid);
		return;
	}
}

// ---------------------------------------------------------------------
//  Окно
// ---------------------------------------------------------------------
stock Daily_Show(playerid)
{
	Daily_Check(playerid);
	new left[12], streak = gDailyStreak[playerid];
	Daily_Left(left, sizeof left);
	// серия на сегодня: если вчера всё выполнено — продолжается
	new next = (gDailyLast[playerid] == gDailyDay[playerid]) ? streak : ((gDailyLast[playerid] == gDailyDay[playerid] - 1) ? streak + 1 : 1);
	if (gDailyLast[playerid] < gDailyDay[playerid] - 1) streak = 0;
	if (UI_Has(playerid))
	{
		new json[1400], item[300], title[64], hint[160];
		format(json, sizeof json, "{\"streak\":%d,\"next\":%d,\"max\":%d,\"bonus\":%d,\"got\":%d,\"left\":\"%s\",\"swap\":%d,\"swapPrice\":%d,\"coins\":%d,\"tasks\":[",
			streak, next, DAILY_STREAK_MAX, Daily_BonusMoney(playerid, next), (gDailyFlags[playerid] & DAILY_F_BONUS) ? 1 : 0, left,
			(gDailyFlags[playerid] & DAILY_F_SWAP) ? 0 : 1, DAILY_SWAP_PRICE, DAILY_STREAK_COINS);
		for (new t = 0; t < DAILY_TASKS; t++)
		{
			new type = gDailyType[playerid][t];
			Daily_Title(title, sizeof title, type, gDailyNeed[playerid][t]);
			UI_JsonEscape(hint, gDailyTypes[type][dtHint]);
			format(item, sizeof item, "%s[\"%s\",\"%s\",\"%s\",%d,%d,%d,%d]", t ? "," : "", gDailyTypes[type][dtKey], title, hint,
				gDailyProg[playerid][t], gDailyNeed[playerid][t], Daily_Reward(playerid, gDailyTypes[type][dtReward]), (gDailyFlags[playerid] & (1 << t)) ? 1 : 0);
			strcat(json, item);
		}
		strcat(json, "]}");
		// Ответ: 1/(10+t) — заменить задание t; 0 — закрыть
		UI_Show(playerid, DIALOG_DAILY_UI, DIALOG_STYLE_LIST, "daily", json, "Понятно", "Закрыть");
		return;
	}
	new text[1200], line[200], title[64];
	for (new t = 0; t < DAILY_TASKS; t++)
	{
		new type = gDailyType[playerid][t];
		Daily_Title(title, sizeof title, type, gDailyNeed[playerid][t]);
		if (gDailyFlags[playerid] & (1 << t))
			format(line, sizeof line, "{33AA33}+ %s\t{33AA33}готово\n", title);
		else
			format(line, sizeof line, "{FFFFFF}%s\t{FFFF00}%d/%d · $%d\n", title, gDailyProg[playerid][t], gDailyNeed[playerid][t], Daily_Reward(playerid, gDailyTypes[type][dtReward]));
		strcat(text, line);
	}
	format(line, sizeof line, "{AAAAAA}Бонус за все: $%d · серия %d/%d дн. · новые через %s\t{AAAAAA}%s",
		Daily_BonusMoney(playerid, next), streak, DAILY_STREAK_MAX, left, (gDailyFlags[playerid] & DAILY_F_BONUS) ? "получен" : "");
	strcat(text, line);
	ShowPlayerDialog(playerid, DIALOG_DAILY, DIALOG_STYLE_TABLIST, "Ежедневные задания", "%s", "Заменить", "Закрыть", text);
}

static Daily_AskSwap(playerid, t)
{
	if (t < 0 || t >= DAILY_TASKS) return;
	if (gDailyFlags[playerid] & (1 << t)) { SendClientMessage(playerid, COLOR_GREY, "Это задание уже выполнено."); return; }
	if (gDailyFlags[playerid] & DAILY_F_SWAP) { SendClientMessage(playerid, COLOR_GREY, "Сегодня ты уже менял задание. Новые — после полуночи."); return; }
	SetPVarInt(playerid, "daily_swap", t);
	new title[64];
	Daily_Title(title, sizeof title, gDailyType[playerid][t], gDailyNeed[playerid][t]);
	ShowPlayerDialog(playerid, DIALOG_DAILY_SWAP, DIALOG_STYLE_MSGBOX, "Замена задания",
		"{FFFFFF}Заменить «%s» на другое случайное задание?\nЦена: {FFFF00}$%d{FFFFFF}. Менять можно одно задание в день.", "Заменить", "Отмена", title, DAILY_SWAP_PRICE);
}

static Daily_Swap(playerid, t)
{
	if (t < 0 || t >= DAILY_TASKS || (gDailyFlags[playerid] & (1 << t)) || (gDailyFlags[playerid] & DAILY_F_SWAP)) return;
	if (GetMoney(playerid) < DAILY_SWAP_PRICE) { SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", DAILY_SWAP_PRICE); return; }
	GiveMoney(playerid, -DAILY_SWAP_PRICE, "ежедневные задания: замена");
	new old = gDailyType[playerid][t];
	gDailyType[playerid][t] = Daily_PickType(playerid, t, old);
	gDailyNeed[playerid][t] = Daily_RandomNeed(gDailyType[playerid][t]);
	gDailyProg[playerid][t] = 0;
	gDailyFlags[playerid] |= DAILY_F_SWAP;
	Daily_Save(playerid);
	new title[64];
	Daily_Title(title, sizeof title, gDailyType[playerid][t], gDailyNeed[playerid][t]);
	SendClientMessage(playerid, COLOR_GREEN, "[Ежедневно] Новое задание: %s.", title);
	Daily_Show(playerid);
}

stock Daily_OnDialog(playerid, dialogid, response, listitem)
{
	switch (dialogid)
	{
		case DIALOG_DAILY_UI:
		{
			if (response && listitem >= 10 && listitem < 10 + DAILY_TASKS) Daily_Swap(playerid, listitem - 10);
			return 1;
		}
		case DIALOG_DAILY:
		{
			if (response) Daily_AskSwap(playerid, listitem);
			return 1;
		}
		case DIALOG_DAILY_SWAP:
		{
			if (response) Daily_Swap(playerid, GetPVarInt(playerid, "daily_swap"));
			else Daily_Show(playerid);
			return 1;
		}
	}
	return 0;
}

stock bool:Daily_OnCommand(playerid, const cmd[])
{
	if (!strcmp(cmd, "/daily", true) || !strcmp(cmd, "/tasks", true))
	{
		if (!Player[playerid][pLogged]) return true;
		Daily_Show(playerid);
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------
//  Вход, выход
// ---------------------------------------------------------------------
stock Daily_OnLogin(playerid)
{
	gDailyLoaded[playerid] = false;
	gDailyDay[playerid] = 0;
	gDailyFlags[playerid] = 0;
	gDailyStreak[playerid] = 0;
	gDailyLast[playerid] = 0;
	new data[96], bool:ok = false;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT daily_day, daily_data, daily_flags, daily_streak, daily_last FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			gDailyDay[playerid] = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 1, data, sizeof data);
			gDailyFlags[playerid] = DB_GetFieldInt(r, 2);
			gDailyStreak[playerid] = DB_GetFieldInt(r, 3);
			gDailyLast[playerid] = DB_GetFieldInt(r, 4);
			ok = Daily_Parse(playerid, data);
		}
		DB_FreeResultSet(r);
	}
	gDailyLoaded[playerid] = true;
	if (!ok) gDailyDay[playerid] = 0;      // нет заданий или строка битая — выдать заново
	Daily_Check(playerid);
	new done = Daily_DoneCount(playerid);
	if (done < DAILY_TASKS)
		SendClientMessage(playerid, COLOR_YELLOW, "[Ежедневно] Задания на сегодня: выполнено %d/%d, бонус за все $%d. Открыть: /daily", done, DAILY_TASKS,
			Daily_BonusMoney(playerid, (gDailyLast[playerid] == gDailyDay[playerid] - 1) ? gDailyStreak[playerid] + 1 : 1));
}

stock Daily_OnDisconnect(playerid)
{
	gDailyLoaded[playerid] = false;
}

// ---------------------------------------------------------------------
//  События из других модулей
// ---------------------------------------------------------------------
stock Daily_OnJobTask(playerid, pay)        // Job_Pay (jobs.inc)
{
	Daily_Progress(playerid, DT_JOB, 1);
	Daily_Progress(playerid, DT_JOBMONEY, pay, true);
}
stock Daily_OnPayDay(playerid)              { Daily_Progress(playerid, DT_PAYDAY, 1); }
stock Daily_OnEat(playerid)                 { Daily_Progress(playerid, DT_EAT, 1); }
stock Daily_OnDeposit(playerid, amount)     { Daily_Progress(playerid, DT_DEPOSIT, amount); }
stock Daily_OnFuel(playerid, liters)        { Daily_Progress(playerid, DT_FUEL, liters); }
stock Daily_OnSpend(playerid, amount)       { Daily_Progress(playerid, DT_SPEND, amount, true); }

// Новый день наступил, пока игрок в игре (из PayDay)
stock Daily_HourCheck(playerid)
{
	if (Daily_Check(playerid))
		SendClientMessage(playerid, COLOR_YELLOW, "[Ежедневно] Новый день — новые задания: /daily");
}
