// =====================================================================
//  donate.inc — донат-меню: донат-монеты и что на них купить.
//
//  Монеты выдаёт главный админ (6+) после оплаты: /givedonate [id] [монеты] [комментарий].
//  Все выдачи пишутся в журнал админов. Оплату сервер не принимает сам —
//  её проверяет человек, а здесь только зачисление и трата.
//  За крупное пополнение монеты начисляются с бонусом (gDonTopupFrom/gDonTopupPct).
//
//  Игрок: /donate — баланс и магазин:
//    0 игровые деньги (DONATE_MONEY_RATE $ за монету, пачками; за крупную пачку бонус),
//    1 VIP четырёх уровней на VIP_DAYS дней (vip.inc),
//    2 любой скин навсегда (в сумку как одежда),
//    3 +1 уровень, 4 снять предупреждения (/warn), 5 смена номера телефона на свой,
//    6 кейс со случайным призом, 7 все лицензии сразу, 8 смена ника,
//    9 стартовый набор (один раз на аккаунт), 10 промокод (/promo [код]).
//  /vip — статус и привилегии VIP.
//  Админ 6+: /addpromo, /delpromo, /promos, /setvip.
//
//  На клиенте Bastion (UI_Has) /donate открывает HTML-окно "donate":
//  ответ приходит в DIALOG_DONATE_UI текстом "товар аргумент"
//  (например "0 2" — пачка №2, "1 3" — VIP Gold, "2 105" — скин 105, "5 777" — номер).
//  После покупки окно открывается снова с итогом и новым балансом.
// =====================================================================

#include "bastion/vip.inc"

// --- Цены в донат-монетах (цены VIP — в vip.inc) ---
#define DONATE_MONEY_RATE       1000    // $ за одну монету
#define DONATE_SKIN_PRICE       150
#define DONATE_LEVEL_PRICE      200
#define DONATE_UNWARN_PRICE     100
#define DONATE_NUMBER_PRICE     150
#define DONATE_CASE_PRICE       50
#define DONATE_LIC_PRICE        100     // все лицензии сразу
#define DONATE_NAME_PRICE       200     // смена ника
#define DONATE_STARTER_PRICE    100     // стартовый набор, один раз на аккаунт
#define DONATE_STARTER_MONEY    50000   // ...в нём деньги,
#define DONATE_STARTER_VIP      1       // ...уровень VIP
#define DONATE_STARTER_DAYS     7       // ...на столько дней, плюс лицензия на авто

enum
{
	DON_MONEY,
	DON_VIP,
	DON_SKIN,
	DON_LEVEL,
	DON_UNWARN,
	DON_NUMBER,
	DON_CASE,
	DON_LIC,
	DON_NAME,
	DON_STARTER,
	DON_PROMO,
	DON_COUNT
}

static gDonate[MAX_PLAYERS];            // монеты
static bool:gDonStarter[MAX_PLAYERS];   // стартовый набор уже куплен
static gDonateSel[MAX_PLAYERS];
static gDonateNote[MAX_PLAYERS][128];    // итог последней покупки — показать в HTML-окне
static bool:gDonateNoteOk[MAX_PLAYERS];
static gPromoTry[MAX_PLAYERS];           // защита от перебора промокодов

// Пачки монет на обмен в деньги и бонус к сумме за пачку, %
static const gDonMoneyPacks[] = { 10, 50, 100, 500, 1000 };
static const gDonMoneyBonus[] = { 0,  5,  10,  20,  30 };

// Бонус монет при пополнении (/givedonate): от стольких монет за раз — столько % сверху
static const gDonTopupFrom[] = { 500, 1000, 3000 };
static const gDonTopupPct[]  = { 10,  20,   30 };

// Кейс: шанс (вес), вид приза, количество, доп. число. Виды: 0 деньги $, 1 монеты, 2 VIP (уровень, дней), 3 редкий скин
enum E_CASE { caseWeight, caseKind, caseA, caseB }
static const gDonCase[][E_CASE] =
{
	{ 30, 0, 20000,  0 },
	{ 20, 0, 50000,  0 },
	{ 8,  0, 150000, 0 },
	{ 15, 1, 25,     0 },
	{ 5,  1, 100,    0 },
	{ 1,  1, 300,    0 },
	{ 10, 2, 1,      7 },
	{ 4,  2, 3,      7 },
	{ 7,  3, 0,      0 }
};
static const gDonCaseSkins[] = { 264, 167, 149, 203, 204, 249, 294, 295 };

stock Donate_Init()
{
	DB_AddColumn("accounts", "donate", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "donate_starter", "INTEGER DEFAULT 0");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS promo (code TEXT PRIMARY KEY COLLATE NOCASE, coins INTEGER DEFAULT 0, vip_level INTEGER DEFAULT 0, vip_days INTEGER DEFAULT 0, uses INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS promo_used (code TEXT NOT NULL COLLATE NOCASE, account INTEGER NOT NULL, PRIMARY KEY (code, account))"));
	Vip_Init();
}

stock Donate_LoadPlayer(playerid)
{
	gDonate[playerid] = 0;
	gDonStarter[playerid] = false;
	gDonateNote[playerid][0] = '\0';
	gPromoTry[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT donate, donate_starter FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			gDonate[playerid] = DB_GetFieldInt(r, 0);
			gDonStarter[playerid] = DB_GetFieldInt(r, 1) != 0;
		}
		DB_FreeResultSet(r);
	}
	Vip_LoadPlayer(playerid);
}

static Donate_Save(playerid)
{
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET donate = %d, donate_starter = %d WHERE id = %d", gDonate[playerid], _:gDonStarter[playerid], Player[playerid][pDbId]));
}

stock bool:Donate_IsVip(playerid) { return Vip_GetLevel(playerid) > 0; }
stock Donate_GetCoins(playerid) { return gDonate[playerid]; }

// Начислить монеты без админа (бонус VIP, кейс, промокод)
stock Donate_AddCoins(playerid, coins)
{
	gDonate[playerid] += coins;
	if (gDonate[playerid] < 0) gDonate[playerid] = 0;
	Donate_Save(playerid);
}

// Из PayDay (rp.pwn): прибавка VIP
stock Donate_PayDay(playerid)
{
	Vip_PayDay(playerid);
}

// Сообщение о покупке: в чат и в окно (если оно HTML)
static Donate_Say(playerid, bool:ok, const text[])
{
	SendClientMessage(playerid, ok ? 0xFFD700FF : COLOR_GREY, "%s", text);
	format(gDonateNote[playerid], sizeof gDonateNote[], "%s", text);
	gDonateNoteOk[playerid] = ok;
}

static bool:Donate_Spend(playerid, coins, const what[])
{
	new msg[96];
	if (gDonate[playerid] < coins)
	{
		format(msg, sizeof msg, "Не хватает монет: нужно %d, у тебя %d.", coins, gDonate[playerid]);
		Donate_Say(playerid, false, msg);
		return false;
	}
	gDonate[playerid] -= coins;
	Donate_Save(playerid);
	new log[64];
	format(log, sizeof log, "донат: %s (-%d)", what, coins);
	Admin_Log(playerid, "donate_spend", playerid, log, coins);
	return true;
}

static bool:Donate_HasAllLic(playerid)
{
	for (new l = 1; l < LIC_COUNT; l++) if (!Licenses_Has(playerid, l)) return false;
	return true;
}

// ---------------------------------------------------------------------
// HTML-окно (клиент Bastion). page: "" — магазин, "vip" — сразу уровни VIP
static Donate_ShowUI(playerid, const page[] = "")
{
	new json[2600], note[192], packs[64], bonus[48], tiers[1200], part[24];
	UI_JsonEscape(note, gDonateNote[playerid]);
	for (new i = 0; i < sizeof (gDonMoneyPacks); i++)
	{
		format(part, sizeof part, "%s%d", i ? "," : "", gDonMoneyPacks[i]);
		strcat(packs, part);
		format(part, sizeof part, "%s%d", i ? "," : "", gDonMoneyBonus[i]);
		strcat(bonus, part);
	}
	Vip_Json(tiers);
	new level = Vip_GetLevel(playerid);
	// prices[1] и vipPayday — для старых версий окна, где VIP был один (Silver)
	format(json, sizeof json,
		"{\"coins\":%d,\"vip\":%d,\"vipLevel\":%d,\"rate\":%d,\"packs\":[%s],\"packBonus\":[%s],\"vipDays\":%d,\"vipPayday\":2000,\"tiers\":[%s],\"prices\":[0,%d,%d,%d,%d,%d,%d,%d,%d,%d,0],\"warns\":%d,\"phone\":%d,\"lics\":%d,\"starter\":%d,\"starterMoney\":%d,\"starterVip\":%d,\"starterDays\":%d,\"page\":\"%s\",\"note\":\"%s\",\"ok\":%d}",
		gDonate[playerid], Vip_DaysLeft(playerid), level, DONATE_MONEY_RATE, packs, bonus, VIP_DAYS, tiers,
		Vip_LevelPrice(2), DONATE_SKIN_PRICE, DONATE_LEVEL_PRICE, DONATE_UNWARN_PRICE, DONATE_NUMBER_PRICE,
		DONATE_CASE_PRICE, DONATE_LIC_PRICE, DONATE_NAME_PRICE, DONATE_STARTER_PRICE,
		Admin_GetWarns(playerid), _:(Player[playerid][pPhone] != 0), _:Donate_HasAllLic(playerid), _:(!gDonStarter[playerid]),
		DONATE_STARTER_MONEY, DONATE_STARTER_VIP, DONATE_STARTER_DAYS, page, note, _:gDonateNoteOk[playerid]);
	gDonateNote[playerid][0] = '\0';
	UI_Show(playerid, DIALOG_DONATE_UI, DIALOG_STYLE_INPUT, "donate", json, "Купить", "Закрыть");
}

stock bool:Cmd_Donate(playerid)
{
	if (UI_Has(playerid)) { Donate_ShowUI(playerid); return true; }
	new list[900], line[144];
	format(list, sizeof list, "Товар\tЦена\n");
	format(line, sizeof line, "Игровые деньги\t1 монета = $%d\n", DONATE_MONEY_RATE); strcat(list, line);
	format(line, sizeof line, "{FFD700}VIP: Bronze, Silver, Gold, Platinum, Diamond%s\tот %d монет\n", Donate_IsVip(playerid) ? ((Vip_GetLevel(playerid) >= VIP_MAX) ? " (продлить)" : " (продлить, улучшить)") : "", Vip_LevelPrice(1)); strcat(list, line);
	format(line, sizeof line, "Любой скин (в сумку)\t%d монет\n", DONATE_SKIN_PRICE); strcat(list, line);
	format(line, sizeof line, "+1 уровень\t%d монет\n", DONATE_LEVEL_PRICE); strcat(list, line);
	format(line, sizeof line, "Снять все предупреждения\t%d монет\n", DONATE_UNWARN_PRICE); strcat(list, line);
	format(line, sizeof line, "Свой номер телефона\t%d монет\n", DONATE_NUMBER_PRICE); strcat(list, line);
	format(line, sizeof line, "Кейс со случайным призом\t%d монет\n", DONATE_CASE_PRICE); strcat(list, line);
	format(line, sizeof line, "Все лицензии сразу\t%d монет\n", DONATE_LIC_PRICE); strcat(list, line);
	format(line, sizeof line, "Смена ника\t%d монет\n", DONATE_NAME_PRICE); strcat(list, line);
	format(line, sizeof line, "%sСтартовый набор: $%d, VIP %s на %d дн., права\t%d монет\n", gDonStarter[playerid] ? "{777777}" : "", DONATE_STARTER_MONEY, Vip_LevelName(DONATE_STARTER_VIP), DONATE_STARTER_DAYS, DONATE_STARTER_PRICE);
	strcat(list, line);
	strcat(list, "Ввести промокод\tбесплатно\n");
	new title[48];
	format(title, sizeof title, "Донат: у тебя %d монет", gDonate[playerid]);
	ShowPlayerDialog(playerid, DIALOG_DONATE, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Выбрать", "Закрыть", list);
	return true;
}

// Покупка VIP: на клиенте Bastion — страница VIP в окне доната, иначе список уровней
stock Donate_ShowVip(playerid)
{
	if (UI_Has(playerid)) { Donate_ShowUI(playerid, "vip"); return; }
	new list[400], line[96], cur = Vip_GetLevel(playerid);
	strcat(list, "Уровень\tЦена\tСрок\n");
	for (new l = 1; l <= VIP_MAX; l++)
	{
		format(line, sizeof line, "VIP %s%s\t%d монет\t%d дней\n", Vip_LevelName(l), (l == cur) ? " (продлить)" : ((cur > 0 && l > cur) ? " (улучшить)" : ""), Vip_LevelPrice(l), VIP_DAYS);
		strcat(list, line);
	}
	ShowPlayerDialog(playerid, DIALOG_DONATE_VIP, DIALOG_STYLE_TABLIST_HEADERS, "Купить VIP. Что даёт каждый уровень: /vip", "%s", "Купить", "Назад", list);
}

stock bool:Cmd_Vip(playerid)
{
	if (UI_Has(playerid)) Donate_ShowUI(playerid, "vip");
	else Vip_ShowMenu(playerid);
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
	new bonus = 0;
	if (coins > 0)
	{
		for (new i = sizeof (gDonTopupFrom) - 1; i >= 0; i--)
		{
			if (coins >= gDonTopupFrom[i]) { bonus = coins * gDonTopupPct[i] / 100; break; }
		}
	}
	if (gDonate[target] + coins < 0) coins = -gDonate[target];
	gDonate[target] += coins + bonus;
	Donate_Save(target);
	Admin_Log(playerid, "givedonate", target, note, coins);
	if (bonus > 0) Admin_Log(playerid, "givedonate", target, "бонус за пополнение", bonus);
	SendClientMessage(target, 0xFFD700FF, "%s монет: %d. Баланс: %d. Магазин: /donate", (coins > 0) ? "Зачислено" : "Списано", (coins > 0) ? coins : -coins, gDonate[target]);
	if (bonus > 0) SendClientMessage(target, 0xFFD700FF, "Бонус за пополнение: +%d монет.", bonus);
	SendClientMessage(playerid, COLOR_ADMIN, "%s: %s%d монет (бонус +%d), теперь %d.", GetRPName(target), (coins >= 0) ? "+" : "", coins, bonus, gDonate[target]);   // %+d в Pawn нет
	return true;
}

static Donate_Exchange(playerid, pack)
{
	if (pack < 0 || pack >= sizeof (gDonMoneyPacks)) return;
	new coins = gDonMoneyPacks[pack];
	new money = coins * DONATE_MONEY_RATE;
	money += money / 100 * gDonMoneyBonus[pack];
	if (!Donate_Spend(playerid, coins, "обмен на деньги")) return;
	GiveMoney(playerid, money, "донат: обмен монет");
	Account_Save(playerid);
	new msg[96];
	format(msg, sizeof msg, "Обмен: %d монет -> $%d.", coins, money);
	Donate_Say(playerid, true, msg);
}

static Donate_BuyVip(playerid, level)
{
	if (level < 1 || level > VIP_MAX) return;
	new cur = Vip_GetLevel(playerid), msg[128];
	if (cur > level)
	{
		format(msg, sizeof msg, "У тебя уже VIP %s — он выше. Продли его или дождись конца срока.", Vip_LevelName(cur));
		Donate_Say(playerid, false, msg);
		return;
	}
	format(msg, sizeof msg, "VIP %s", Vip_LevelName(level));
	if (!Donate_Spend(playerid, Vip_LevelPrice(level), msg)) return;
	Vip_Grant(playerid, level, VIP_DAYS);
	format(msg, sizeof msg, "VIP %s активен ещё %d дн. Привилегии: /vip. Спасибо за поддержку сервера!", Vip_LevelName(level), Vip_DaysLeft(playerid));
	Donate_Say(playerid, true, msg);
}

static Donate_OpenCase(playerid)
{
	if (!Donate_Spend(playerid, DONATE_CASE_PRICE, "кейс")) return;
	new total = 0, pick = 0;
	for (new i = 0; i < sizeof (gDonCase); i++) total += gDonCase[i][caseWeight];
	new roll = random(total);
	for (new i = 0; i < sizeof (gDonCase); i++)
	{
		if (roll < gDonCase[i][caseWeight]) { pick = i; break; }
		roll -= gDonCase[i][caseWeight];
	}
	new kind = gDonCase[pick][caseKind], a = gDonCase[pick][caseA], b = gDonCase[pick][caseB], msg[128];
	if (kind == 3 && Outfit_FreeSlots(playerid) <= 0) { kind = 0; a = DONATE_CASE_PRICE * DONATE_MONEY_RATE; }   // сумка полная — деньгами
	switch (kind)
	{
		case 0:
		{
			GiveMoney(playerid, a, "донат: кейс");
			Account_Save(playerid);
			format(msg, sizeof msg, "Кейс: тебе выпало $%d!", a);
		}
		case 1:
		{
			Donate_AddCoins(playerid, a);
			format(msg, sizeof msg, "Кейс: тебе выпало %d донат-монет!", a);
		}
		case 2:
		{
			Vip_Grant(playerid, a, b);
			format(msg, sizeof msg, "Кейс: тебе выпал VIP %s на %d дн.! Сейчас у тебя VIP %s, ещё %d дн.", Vip_LevelName(a), b, Vip_LevelName(Vip_GetLevel(playerid)), Vip_DaysLeft(playerid));
		}
		default:
		{
			new skin = gDonCaseSkins[random(sizeof (gDonCaseSkins))];
			Outfit_Add(playerid, skin);
			format(msg, sizeof msg, "Кейс: тебе выпал редкий скин %d! Он в сумке: /inv.", skin);
		}
	}
	Admin_Log(playerid, "donate_case", playerid, msg, a);
	Donate_Say(playerid, true, msg);
}

// Покупки без ввода
static Donate_BuySimple(playerid, item)
{
	new msg[128];
	switch (item)
	{
		case DON_LEVEL:
		{
			if (!Donate_Spend(playerid, DONATE_LEVEL_PRICE, "+1 уровень")) return;
			Player[playerid][pLevel]++;
			Player[playerid][pExp] = 0;
			SetPlayerScore(playerid, Player[playerid][pLevel]);
			Account_Save(playerid);
			format(msg, sizeof msg, "Уровень повышен до %d.", Player[playerid][pLevel]);
			Donate_Say(playerid, true, msg);
		}
		case DON_UNWARN:
		{
			if (Admin_GetWarns(playerid) <= 0) { Donate_Say(playerid, false, "Предупреждений нет."); return; }
			if (!Donate_Spend(playerid, DONATE_UNWARN_PRICE, "снять предупреждения")) return;
			Admin_ClearWarns(playerid);
			Donate_Say(playerid, true, "Все предупреждения сняты.");
		}
		case DON_CASE: Donate_OpenCase(playerid);
		case DON_LIC:
		{
			if (Donate_HasAllLic(playerid)) { Donate_Say(playerid, false, "У тебя уже есть все лицензии."); return; }
			if (!Donate_Spend(playerid, DONATE_LIC_PRICE, "все лицензии")) return;
			for (new l = 1; l < LIC_COUNT; l++) Licenses_Give(playerid, l);
			Donate_Say(playerid, true, "Все лицензии выданы: авто, мото, лодки, оружие. Посмотреть: /licenses");
		}
		case DON_STARTER:
		{
			if (gDonStarter[playerid]) { Donate_Say(playerid, false, "Стартовый набор покупается один раз."); return; }
			if (!Donate_Spend(playerid, DONATE_STARTER_PRICE, "стартовый набор")) return;
			gDonStarter[playerid] = true;
			Donate_Save(playerid);
			GiveMoney(playerid, DONATE_STARTER_MONEY, "донат: стартовый набор");
			Vip_Grant(playerid, DONATE_STARTER_VIP, DONATE_STARTER_DAYS);
			Licenses_Give(playerid, LIC_CAR);
			Account_Save(playerid);
			format(msg, sizeof msg, "Стартовый набор: $%d, VIP %s на %d дн. и лицензия на авто.", DONATE_STARTER_MONEY, Vip_LevelName(DONATE_STARTER_VIP), DONATE_STARTER_DAYS);
			Donate_Say(playerid, true, msg);
		}
	}
}

// Смена ника: Имя_Фамилия, свободный. Аккаунт остаётся тот же.
static Donate_Rename(playerid, const input[])
{
	new len = strlen(input), old[MAX_PLAYER_NAME], msg[112];
	GetPlayerName(playerid, old, sizeof old);
	new under = 0, bool:bad = (len < 5 || len > 20);
	for (new i = 0; i < len; i++)
	{
		if (input[i] == '_') under++;
		else if (!((input[i] >= 'a' && input[i] <= 'z') || (input[i] >= 'A' && input[i] <= 'Z'))) bad = true;
	}
	if (bad || under != 1 || !IsRPName(input)) { Donate_Say(playerid, false, "Ник пишется как Имя_Фамилия, латиницей, до 20 букв."); return; }
	if (!strcmp(old, input, true)) { Donate_Say(playerid, false, "Это твой нынешний ник."); return; }
	new bool:busy = false;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT (SELECT COUNT(*) FROM accounts WHERE name = '%q' COLLATE NOCASE) + (SELECT COUNT(*) FROM bans WHERE name = '%q' COLLATE NOCASE)", input, input);
	if (r != DBResult:0) { busy = DB_GetFieldInt(r, 0) > 0; DB_FreeResultSet(r); }
	if (busy) { Donate_Say(playerid, false, "Этот ник занят."); return; }
	if (gDonate[playerid] < DONATE_NAME_PRICE) { Donate_Spend(playerid, DONATE_NAME_PRICE, "смена ника"); return; }
	if (SetPlayerName(playerid, input) != 1) { Donate_Say(playerid, false, "Этот ник сейчас занят или не подходит."); return; }
	if (!Donate_Spend(playerid, DONATE_NAME_PRICE, "смена ника")) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET name = '%q' WHERE id = %d", input, Player[playerid][pDbId]));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE marriages SET a_name = '%q' WHERE a = %d", input, Player[playerid][pDbId]));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE marriages SET b_name = '%q' WHERE b = %d", input, Player[playerid][pDbId]));
	format(msg, sizeof msg, "ник: %s -> %s", old, input);
	Admin_Log(playerid, "donate_rename", playerid, msg, 0);
	format(msg, sizeof msg, "Ник изменён: %s. Заходи на сервер под новым ником, пароль прежний.", input);
	Donate_Say(playerid, true, msg);
}

// Промокод: один раз на аккаунт, пока есть активации
static Donate_Promo(playerid, const input[])
{
	new len = strlen(input), msg[128];
	if (len < 3 || len > 24) { Donate_Say(playerid, false, "Промокод — от 3 до 24 символов."); return; }
	if (gPromoTry[playerid] > gettime()) { Donate_Say(playerid, false, "Не так быстро. Попробуй через несколько секунд."); return; }
	gPromoTry[playerid] = gettime() + 5;
	new coins, level, days, uses = -1;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT coins, vip_level, vip_days, uses FROM promo WHERE code = '%q'", input);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			coins = DB_GetFieldInt(r, 0);
			level = DB_GetFieldInt(r, 1);
			days = DB_GetFieldInt(r, 2);
			uses = DB_GetFieldInt(r, 3);
		}
		DB_FreeResultSet(r);
	}
	if (uses == -1) { Donate_Say(playerid, false, "Такого промокода нет."); return; }
	if (uses <= 0) { Donate_Say(playerid, false, "Этот промокод уже закончился."); return; }
	new bool:used = false;
	r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM promo_used WHERE code = '%q' AND account = %d", input, Player[playerid][pDbId]);
	if (r != DBResult:0) { used = DB_GetFieldInt(r, 0) > 0; DB_FreeResultSet(r); }
	if (used) { Donate_Say(playerid, false, "Ты уже вводил этот промокод."); return; }
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO promo_used (code, account) VALUES ('%q', %d)", input, Player[playerid][pDbId]));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE promo SET uses = uses - 1 WHERE code = '%q'", input));
	if (coins > 0) Donate_AddCoins(playerid, coins);
	if (level >= 1 && level <= VIP_MAX && days > 0) Vip_Grant(playerid, level, days);
	if (coins > 0 && level > 0 && days > 0) format(msg, sizeof msg, "Промокод принят: +%d монет и VIP %s на %d дн.", coins, Vip_LevelName(level), days);
	else if (level > 0 && days > 0) format(msg, sizeof msg, "Промокод принят: VIP %s на %d дн.", Vip_LevelName(level), days);
	else format(msg, sizeof msg, "Промокод принят: +%d монет.", coins);
	Admin_Log(playerid, "promo", playerid, input, coins);
	Donate_Say(playerid, true, msg);
}

stock bool:Cmd_Promo(playerid, const params[])
{
	new code[32];
	if (!RestParam(params, 0, code)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /promo [код]");
	Donate_Promo(playerid, code);
	gDonateNote[playerid][0] = '\0';
	return true;
}

stock bool:Cmd_AddPromo(playerid, const params[])
{
	if (!Admin_Check(playerid, 6)) return true;
	new pos = 0, code[32], coins, uses, level, days;
	if (!NextParam(params, pos, code) || !NextIntParam(params, pos, coins) || !NextIntParam(params, pos, uses) || coins < 0 || coins > 100000 || uses < 1 || uses > 100000)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /addpromo [код] [монеты] [активаций] [уровень VIP 0-5] [дней VIP]");
	NextIntParam(params, pos, level);
	NextIntParam(params, pos, days);
	new len = strlen(code);
	if (len < 3 || len > 24) return SendClientMessage(playerid, COLOR_GREY, "Код — от 3 до 24 символов, без пробелов.");
	if (level < 0 || level > VIP_MAX || days < 0 || days > 365 || (level > 0 && days == 0) || (coins == 0 && level == 0))
		return SendClientMessage(playerid, COLOR_GREY, "Промокод должен давать монеты или VIP: уровень 1-5 и дни 1-365.");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO promo (code, coins, vip_level, vip_days, uses) VALUES ('%q', %d, %d, %d, %d)", code, coins, level, days, uses));
	Admin_Log(playerid, "addpromo", playerid, code, coins);
	SendClientMessage(playerid, COLOR_ADMIN, "Промокод %s: %d монет, VIP %s на %d дн., активаций: %d. Игроки вводят: /promo %s", code, coins, Vip_LevelName(level), days, uses, code);
	return true;
}

stock bool:Cmd_DelPromo(playerid, const params[])
{
	if (!Admin_Check(playerid, 6)) return true;
	new code[32];
	if (!RestParam(params, 0, code)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /delpromo [код]");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM promo WHERE code = '%q'", code));
	Admin_Log(playerid, "delpromo", playerid, code, 0);
	SendClientMessage(playerid, COLOR_ADMIN, "Промокод %s удалён.", code);
	return true;
}

stock bool:Cmd_Promos(playerid)
{
	if (!Admin_Check(playerid, 6)) return true;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT code, coins, vip_level, vip_days, uses FROM promo ORDER BY code LIMIT 30");
	if (r == DBResult:0) return true;
	new n = DB_GetRowCount(r), code[32];
	if (n == 0) SendClientMessage(playerid, COLOR_GREY, "Промокодов нет. Создать: /addpromo");
	for (new i = 0; i < n; i++)
	{
		DB_GetFieldString(r, 0, code, sizeof code);
		SendClientMessage(playerid, COLOR_ADMIN, "%s: %d монет, VIP %s на %d дн., осталось активаций: %d", code, DB_GetFieldInt(r, 1), Vip_LevelName(DB_GetFieldInt(r, 2)), DB_GetFieldInt(r, 3), DB_GetFieldInt(r, 4));
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	return true;
}

// Обычные окна: что показать после выбора товара
static Donate_Buy(playerid, item)
{
	switch (item)
	{
		case DON_MONEY:
		{
			new list[256], line[64];
			for (new i = 0; i < sizeof (gDonMoneyPacks); i++)
			{
				new money = gDonMoneyPacks[i] * DONATE_MONEY_RATE;
				money += money / 100 * gDonMoneyBonus[i];
				if (gDonMoneyBonus[i] > 0) format(line, sizeof line, "%d монет\t$%d\t{33AA33}+%d%%\n", gDonMoneyPacks[i], money, gDonMoneyBonus[i]);
				else format(line, sizeof line, "%d монет\t$%d\t \n", gDonMoneyPacks[i], money);
				strcat(list, line);
			}
			ShowPlayerDialog(playerid, DIALOG_DONATE_MONEY, DIALOG_STYLE_TABLIST, "Обмен монет на деньги", "%s", "Обменять", "Назад", list);
		}
		case DON_VIP: Donate_ShowVip(playerid);
		case DON_LEVEL, DON_UNWARN, DON_CASE, DON_LIC, DON_STARTER: Donate_BuySimple(playerid, item);
		case DON_SKIN:
		{
			ShowPlayerDialog(playerid, DIALOG_DONATE_INPUT, DIALOG_STYLE_INPUT, "Скин", "{FFFFFF}Номер скина (1-311). Он ляжет в сумку как одежда.", "Купить", "Назад");
		}
		case DON_NUMBER:
		{
			if (Player[playerid][pPhone] == 0) { Donate_Say(playerid, false, "Сначала купи телефон в магазине 24/7."); return; }
			ShowPlayerDialog(playerid, DIALOG_DONATE_INPUT, DIALOG_STYLE_INPUT, "Свой номер", "{FFFFFF}Номер из 3-7 цифр, например 777 или 5550000:", "Купить", "Назад");
		}
		case DON_NAME:
		{
			ShowPlayerDialog(playerid, DIALOG_DONATE_INPUT, DIALOG_STYLE_INPUT, "Смена ника", "{FFFFFF}Новый ник в формате Имя_Фамилия, латиницей.\nДом, машины, деньги и всё остальное останутся у тебя.", "Купить", "Назад");
		}
		case DON_PROMO:
		{
			ShowPlayerDialog(playerid, DIALOG_DONATE_INPUT, DIALOG_STYLE_INPUT, "Промокод", "{FFFFFF}Введи промокод:", "Ввести", "Назад");
		}
	}
}

// Покупки с вводом: скин, номер телефона, ник, промокод
static Donate_Input(playerid, item, const input[])
{
	if (item == DON_NAME) { Donate_Rename(playerid, input); return; }
	if (item == DON_PROMO) { Donate_Promo(playerid, input); return; }
	if (input[0] == '\0' || !IsNumeric(input)) { Donate_Say(playerid, false, "Нужно ввести число."); return; }
	new v = strval(input), msg[96];
	if (item == DON_SKIN)
	{
		if (v < 1 || v > 311 || v == 74) { Donate_Say(playerid, false, "Скин от 1 до 311."); return; }
		if (Outfit_FreeSlots(playerid) <= 0) { Donate_Say(playerid, false, "В сумке нет места для одежды."); return; }
		if (!Donate_Spend(playerid, DONATE_SKIN_PRICE, "скин")) return;
		Outfit_Add(playerid, v);
		format(msg, sizeof msg, "Скин %d в сумке. Надеть: /inv.", v);
		Donate_Say(playerid, true, msg);
	}
	else if (item == DON_NUMBER)
	{
		if (Player[playerid][pPhone] == 0) { Donate_Say(playerid, false, "Сначала купи телефон в магазине 24/7."); return; }
		new len = strlen(input);
		if (len < 3 || len > 7 || input[0] == '0') { Donate_Say(playerid, false, "Номер из 3-7 цифр, не с нуля."); return; }
		new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id FROM accounts WHERE phone = %d LIMIT 1", v);
		new bool:busy = false;
		if (r != DBResult:0) { busy = DB_GetRowCount(r) > 0; DB_FreeResultSet(r); }
		if (busy) { Donate_Say(playerid, false, "Этот номер занят."); return; }
		if (!Donate_Spend(playerid, DONATE_NUMBER_PRICE, "номер телефона")) return;
		Player[playerid][pPhone] = v;
		Account_Save(playerid);
		format(msg, sizeof msg, "Твой новый номер: %d.", v);
		Donate_Say(playerid, true, msg);
	}
}

// Ответ HTML-окна: "товар аргумент"
static Donate_OnUI(playerid, const text[])
{
	new item = strval(text), arg[32], sp = strfind(text, " ");
	if (sp != -1) strmid(arg, text, sp + 1, strlen(text), sizeof arg);
	new bool:vipPage = false;
	switch (item)
	{
		case DON_MONEY: Donate_Exchange(playerid, strval(arg));
		case DON_VIP: { Donate_BuyVip(playerid, (arg[0] == '\0') ? 2 : strval(arg)); vipPage = true; }   // без аргумента — старое окно, там VIP был Silver
		case DON_LEVEL, DON_UNWARN, DON_CASE, DON_LIC, DON_STARTER: Donate_BuySimple(playerid, item);
		case DON_SKIN, DON_NUMBER, DON_NAME, DON_PROMO: Donate_Input(playerid, item, arg);
		default: return;
	}
	Donate_ShowUI(playerid, vipPage ? "vip" : "");
}

stock Donate_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (Vip_OnDialog(playerid, dialogid, response, listitem)) return 1;
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
			Donate_Exchange(playerid, listitem);
			return 1;
		}
		case DIALOG_DONATE_VIP:
		{
			if (!response) { Cmd_Donate(playerid); return 1; }
			Donate_BuyVip(playerid, listitem + 1);
			return 1;
		}
		case DIALOG_DONATE_INPUT:
		{
			if (!response) { Cmd_Donate(playerid); return 1; }
			Donate_Input(playerid, gDonateSel[playerid], inputtext);
			return 1;
		}
		case DIALOG_DONATE_UI:
		{
			if (!response) return 1;
			Donate_OnUI(playerid, inputtext);
			return 1;
		}
	}
	return 0;
}

stock bool:Donate_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/donate", true))     { Cmd_Donate(playerid); return true; }
	if (!strcmp(cmd, "/vip", true))        { Cmd_Vip(playerid); return true; }
	if (!strcmp(cmd, "/promo", true))      { Cmd_Promo(playerid, params); return true; }
	if (!strcmp(cmd, "/givedonate", true)) { Cmd_GiveDonate(playerid, params); return true; }
	if (!strcmp(cmd, "/addpromo", true))   { Cmd_AddPromo(playerid, params); return true; }
	if (!strcmp(cmd, "/delpromo", true))   { Cmd_DelPromo(playerid, params); return true; }
	if (!strcmp(cmd, "/promos", true))     { Cmd_Promos(playerid); return true; }
	return Vip_OnCommand(playerid, cmd, params);
}
