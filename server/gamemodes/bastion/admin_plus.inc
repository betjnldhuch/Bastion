// =====================================================================
//  admin_plus.inc — развитие админки (всё через окна: ПК и телефон).
//
//  /amenu                 — общая панель: игроки, репорты, наказания,
//                           античит, телепорты, МП, рейтинг, лог.
//  /punish [id или ник]   — наказать по пункту правил: срок и причина
//                           подставляются сами, за повтор (30 дней) срок растёт.
//  /dm /db /sk /tk /rk /pg /mg /nrp /caps /flood /osk /oskrod /oska
//  /bag /uhod /rekl /cheat /obman /nick [id или ник] — то же одной командой.
//  /rules                 — правила и сроки наказаний (видят все).
//  /history [id или ник]  — история наказаний (игрок без параметра — своя).
//  /offwarn /offmute /offjail [ник] ... — наказания тем, кого нет в сети.
//  /acheck                — лента античита: следить, ТП, кик, бан, «ложное».
//  /aip [id или ник]      — другие аккаунты с тех же IP (с 3 уровня).
//  /atop                  — рейтинг админов за неделю и выполнение нормы.
//  /event                 — мероприятие: набор (/mp), заморозка, отсчёт, приз.
//  /ann [текст]           — объявление окном всем игрокам (с 3 уровня).
//  /cc                    — очистить чат (со 2 уровня).
//
//  Таблица admin_week (минуты в админке по неделям) создаётся сама.
// =====================================================================

#define RULE_REPEAT_DAYS        30      // за сколько дней считать повторы
#define ADMIN_NORM_MINUTES      420     // недельная норма: 7 часов в админке...
#define ADMIN_NORM_REPORTS      20      // ...и 20 ответов на репорты
#define AC_FEED_MAX             40      // записей в ленте античита
#define EVENT_MAX_PRIZE         1000000
#define AP_WEEK_SHIFT           270000  // 3 дня + 3 часа: неделя с понедельника 00:00 МСК
#define AP_PUNISH_ACTIONS       "'kick','mute','warn','ajail','ban','offban','offmute','offjail','offwarn','banip'"

enum { RT_MUTE, RT_AJAIL, RT_WARN, RT_KICK, RT_BAN }

enum E_RULE
{
	ruCmd[8],
	ruReason[20],       // причина в логе; по ней считаются повторы
	ruDesc[48],
	ruType,
	ruBase,             // срок за первый раз (мин. или дни для бана)
	ruStep,             // +за каждый повтор
	ruMax,
	ruLevel             // с какого уровня админки
}
static const gRules[][E_RULE] =
{
	{ "dm",     "DM",                 "Убийство или нападение без RP-причины",  RT_AJAIL, 30, 30, 180, 2 },
	{ "db",     "DB",                 "DriveBy: стрельба или давка из машины",  RT_AJAIL, 30, 30, 180, 2 },
	{ "sk",     "SK",                 "SpawnKill: убийство на спавне",          RT_AJAIL, 40, 40, 180, 2 },
	{ "tk",     "TK",                 "TeamKill: убийство своих",               RT_AJAIL, 30, 30, 180, 2 },
	{ "rk",     "RK",                 "RevengeKill: месть после смерти",        RT_AJAIL, 30, 30, 180, 2 },
	{ "pg",     "PG",                 "PowerGaming: нереальные действия",       RT_AJAIL, 20, 20, 120, 2 },
	{ "nrp",    "NonRP",              "NonRP поведение",                        RT_AJAIL, 20, 20, 120, 2 },
	{ "uhod",   "Уход от RP",         "Выход или AFK от ареста и погони",       RT_AJAIL, 60, 30, 240, 2 },
	{ "bag",    "Багоюз",             "Использование багов игры или мода",      RT_AJAIL, 60, 60, 300, 2 },
	{ "mg",     "MG",                 "MetaGaming: OOC-информация в IC",        RT_MUTE,  30, 30, 180, 1 },
	{ "caps",   "Капс",               "Капс в чате",                            RT_MUTE,  10, 10, 60,  1 },
	{ "flood",  "Флуд",               "Флуд и спам",                            RT_MUTE,  15, 15, 90,  1 },
	{ "osk",    "Оскорбление",        "Оскорбление игроков",                    RT_MUTE,  30, 30, 180, 1 },
	{ "oskrod", "Оск. родных",        "Оскорбление или упоминание родных",      RT_MUTE,  120, 120, 720, 1 },
	{ "oska",   "Оск. администрации", "Неуважение к администрации",             RT_MUTE,  60, 60, 360, 1 },
	{ "obman",  "Обман администрации","Ложь админам, ложные жалобы",            RT_WARN,  1, 0, 1,     2 },
	{ "nick",   "NonRP ник",          "Ник не по форме Имя_Фамилия",            RT_KICK,  0, 0, 0,     1 },
	{ "rekl",   "Реклама",            "Реклама других проектов",                RT_BAN,   7, 23, 30,   3 },
	{ "cheat",  "Читы",               "Сторонние программы (читы)",             RT_BAN,   30, 335, 365, 3 }
};

// Кого наказываем (онлайн или офлайн)
static gPunTarget[MAX_PLAYERS];                 // id в игре или INVALID_PLAYER_ID
static gPunDb[MAX_PLAYERS];
static gPunName[MAX_PLAYERS][MAX_PLAYER_NAME];
static gPunLevel[MAX_PLAYERS];                  // уровень админки цели
static gPunList[MAX_PLAYERS][sizeof gRules];

static gCheckTarget[MAX_PLAYERS];
static gCheckDb[MAX_PLAYERS];

// Лента античита (кольцо)
enum E_ACF
{
	acfTime,            // 0 — запись убрана («ложное»)
	acfDb,
	acfName[MAX_PLAYER_NAME],
	acfText[64],
	bool:acfMobile
}
static gAcf[AC_FEED_MAX][E_ACF];
static gAcfHead = 0;                            // куда писать следующую
static gAcfList[MAX_PLAYERS][AC_FEED_MAX];
static gAcfSel[MAX_PLAYERS];

// /atop
static gTopWeek[MAX_PLAYERS];

// Мероприятие
static bool:gEvActive = false;
static bool:gEvOpen = false;
static gEvHost = INVALID_PLAYER_ID;
static gEvName[48];
static gEvPrize = 0;
static bool:gEvPrizeGiven = false;
static Float:gEvX, Float:gEvY, Float:gEvZ, Float:gEvA;
static gEvInt, gEvWorld;
static bool:gEvIn[MAX_PLAYERS];
static bool:gEvFrozen[MAX_PLAYERS];
static Float:gEvBack[MAX_PLAYERS][4];
static gEvBackInt[MAX_PLAYERS];
static gEvBackWorld[MAX_PLAYERS];
static gEvBackHouse[MAX_PLAYERS];
#define EVENT_LIST_MAX 100
static gEvList[MAX_PLAYERS][EVENT_LIST_MAX];
static gEvSel[MAX_PLAYERS];

forward AP_Countdown(n);

// =====================================================================
//  Запуск и события (из admin.inc)
// =====================================================================
static bool:gApInit = false;
static AP_Init()
{
	if (gApInit) return;
	gApInit = true;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS admin_week (name TEXT NOT NULL COLLATE NOCASE, week INTEGER NOT NULL, minutes INTEGER DEFAULT 0, PRIMARY KEY (name, week))"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS reports_admin ON reports (admin COLLATE NOCASE, answer_date)"));
}

stock AdminPlus_OnConnect(playerid)
{
	AP_Init();
	gPunTarget[playerid] = INVALID_PLAYER_ID;
	gPunDb[playerid] = 0;
	gCheckTarget[playerid] = INVALID_PLAYER_ID;
	gEvIn[playerid] = false;
	gEvFrozen[playerid] = false;
}

stock AdminPlus_OnDisconnect(playerid)
{
	if (gEvIn[playerid]) { gEvIn[playerid] = false; gEvFrozen[playerid] = false; }
	if (gEvActive && gEvHost == playerid) AP_EventEnd(INVALID_PLAYER_ID);
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gPunTarget[i] == playerid) gPunTarget[i] = INVALID_PLAYER_ID;   // дальше — как офлайн
		if (gCheckTarget[i] == playerid) gCheckTarget[i] = INVALID_PLAYER_ID;
	}
}

static AP_Week(time)
{
	return (time + AP_WEEK_SHIFT) / 604800;
}

static AP_WeekStart(week)
{
	return week * 604800 - AP_WEEK_SHIFT;
}

stock AdminPlus_OnMinute()
{
	AP_Init();
	new week = AP_Week(gettime()), name[MAX_PLAYER_NAME];
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || Player[i][pAdmin] < 1) continue;
		GetPlayerName(i, name, sizeof name);
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR IGNORE INTO admin_week (name, week, minutes) VALUES ('%q', %d, 0)", name, week));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE admin_week SET minutes = minutes + 1 WHERE name = '%q' AND week = %d", name, week));
	}
}

// =====================================================================
//  Поиск цели: id, ник в игре или аккаунт в базе
// =====================================================================
static AP_FindOnline(const name[])
{
	new other[MAX_PLAYER_NAME];
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i)) continue;
		GetPlayerName(i, other, sizeof other);
		if (!strcmp(other, name, true)) return i;
	}
	return INVALID_PLAYER_ID;
}

// name — на входе ник, на выходе точное написание из базы
static bool:AP_FindAccount(name[], &accId, &level, size = sizeof (name))
{
	new bool:found = false;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, admin, name FROM accounts WHERE name = '%q' LIMIT 1", name);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			accId = DB_GetFieldInt(r, 0);
			level = DB_GetFieldInt(r, 1);
			DB_GetFieldString(r, 2, name, size);
			found = true;
		}
		DB_FreeResultSet(r);
	}
	return found;
}

static bool:AP_SelectTarget(playerid, const who[])
{
	new t = INVALID_PLAYER_ID;
	if (IsNumeric(who))
	{
		t = strval(who);
		if (t < 0 || t >= MAX_PLAYERS || !IsPlayerConnected(t))
		{
			SendClientMessage(playerid, COLOR_GREY, "Игрока с id %s нет в сети. Для офлайна укажи ник.", who);
			return false;
		}
	}
	else t = AP_FindOnline(who);

	if (t != INVALID_PLAYER_ID)
	{
		if (!Player[t][pLogged])
		{
			SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
			return false;
		}
		gPunTarget[playerid] = t;
		gPunDb[playerid] = Player[t][pDbId];
		GetPlayerName(t, gPunName[playerid], MAX_PLAYER_NAME);
		gPunLevel[playerid] = Admin_RealLevel(t);
		return true;
	}
	new name[MAX_PLAYER_NAME], acc = 0, level = 0;
	strcat(name, who);
	if (!AP_FindAccount(name, acc, level))
	{
		SendClientMessage(playerid, COLOR_GREY, "Аккаунт %s не найден.", who);
		return false;
	}
	gPunTarget[playerid] = INVALID_PLAYER_ID;
	gPunDb[playerid] = acc;
	gPunName[playerid][0] = '\0';
	strcat(gPunName[playerid], name, MAX_PLAYER_NAME);
	gPunLevel[playerid] = level;
	return true;
}

// =====================================================================
//  Наказания по правилам
// =====================================================================
static AP_PunText(type, value)
{
	new t[40];
	switch (type)
	{
		case RT_MUTE:  format(t, sizeof t, "мут %d мин.", value);
		case RT_AJAIL: format(t, sizeof t, "деморган %d мин.", value);
		case RT_WARN:  strcat(t, "предупреждение");
		case RT_KICK:  strcat(t, "кик");
		case RT_BAN:   format(t, sizeof t, "бан на %d дн.", value);
	}
	return t;
}

static AP_RuleValue(r, repeats)
{
	new v = gRules[r][ruBase] + gRules[r][ruStep] * repeats;
	if (v > gRules[r][ruMax]) v = gRules[r][ruMax];
	return v;
}

// Сколько раз за RULE_REPEAT_DAYS аккаунт уже наказан по этому пункту
static AP_RuleRepeats(accId, r)
{
	if (accId <= 0) return 0;
	new n = 0;
	new DBResult:q = DB_ExecuteQuery(gDB,
		"SELECT COUNT(*) FROM admin_log WHERE target_id = %d AND date > %d AND (reason = '%q' OR reason LIKE '%q (%%') AND action IN (" AP_PUNISH_ACTIONS ")",
		accId, gettime() - RULE_REPEAT_DAYS * 86400, gRules[r][ruReason], gRules[r][ruReason]);
	if (q != DBResult:0)
	{
		if (DB_GetRowCount(q) > 0) n = DB_GetFieldInt(q, 0);
		DB_FreeResultSet(q);
	}
	return n;
}

static AP_FindRule(const code[])
{
	for (new i = 0; i < sizeof gRules; i++)
		if (!strcmp(code, gRules[i][ruCmd], true)) return i;
	return -1;
}

static AP_ShowRuleMenu(playerid)
{
	new text[2400], line[128], title[64], n = 0;
	strcat(text, "Нарушение\tНаказание\n");
	for (new i = 0; i < sizeof gRules; i++)
	{
		if (Player[playerid][pAdmin] < gRules[i][ruLevel]) continue;
		new repeats = AP_RuleRepeats(gPunDb[playerid], i);
		format(line, sizeof line, "{FFFFFF}/%s  %s\t%s%s%s\n", gRules[i][ruCmd], gRules[i][ruReason],
			(repeats > 0) ? ("{FF9900}") : ("{AFAFAF}"), AP_PunText(gRules[i][ruType], AP_RuleValue(i, repeats)),
			(repeats > 0) ? (" (повтор)") : (""));
		strcat(text, line);
		gPunList[playerid][n++] = i;
	}
	format(title, sizeof title, "Наказать: %s%s", gPunName[playerid], (gPunTarget[playerid] == INVALID_PLAYER_ID) ? (" (офлайн)") : (""));
	ShowPlayerDialog(playerid, DIALOG_APLUS_PUNISH, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Наказать", "Отмена", text);
}

// Из /amenu (действия с игроком)
stock AdminPlus_PunishMenu(playerid, target)
{
	new who[8];
	valstr(who, target);
	if (AP_SelectTarget(playerid, who)) AP_ShowRuleMenu(playerid);
}

static AP_OffPunish(playerid, const name[], accId, type, value, const reason[])
{
	new admin[MAX_PLAYER_NAME], msg[144];
	GetPlayerName(playerid, admin, sizeof admin);
	switch (type)
	{
		case RT_MUTE:
		{
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET mute_pending = mute_pending + %d WHERE id = %d", value, accId));
			Admin_LogName(admin, Player[playerid][pDbId], "offmute", name, accId, reason, value);
			format(msg, sizeof msg, "[A] %s выдал офлайн-мут %s на %d мин. (начнётся при входе). Причина: %s", GetRPName(playerid), name, value, reason);
		}
		case RT_AJAIL:
		{
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET ajail = MIN(ajail + %d, %d) WHERE id = %d", value, ADMIN_MAX_AJAIL, accId));
			Admin_LogName(admin, Player[playerid][pDbId], "offjail", name, accId, reason, value);
			format(msg, sizeof msg, "[A] %s выдал офлайн-деморган %s на %d мин. Причина: %s", GetRPName(playerid), name, value, reason);
		}
		case RT_WARN:
		{
			new warns = 0;
			new DBResult:r = DB_ExecuteQuery(gDB, "SELECT warns FROM accounts WHERE id = %d", accId);
			if (r != DBResult:0)
			{
				if (DB_GetRowCount(r) > 0) warns = DB_GetFieldInt(r, 0);
				DB_FreeResultSet(r);
			}
			warns++;
			Admin_LogName(admin, Player[playerid][pDbId], "offwarn", name, accId, reason, warns);
			if (warns >= ADMIN_WARNS_FOR_BAN)
			{
				DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET warns = 0 WHERE id = %d", accId));
				new why[64];
				format(why, sizeof why, "%d/%d предупреждений (%s)", ADMIN_WARNS_FOR_BAN, ADMIN_WARNS_FOR_BAN, reason);
				Bans_Add(name, "", why, "Сервер", ADMIN_WARN_BAN_DAYS);
				Admin_LogName("Сервер", 0, "offban", name, accId, why, ADMIN_WARN_BAN_DAYS);
				format(msg, sizeof msg, "[A] %s выдал офлайн-варн %s (%d/%d) — автобан на %d дн. Причина: %s", GetRPName(playerid), name, warns, ADMIN_WARNS_FOR_BAN, ADMIN_WARN_BAN_DAYS, reason);
			}
			else
			{
				DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET warns = %d WHERE id = %d", warns, accId));
				format(msg, sizeof msg, "[A] %s выдал офлайн-варн %s (%d/%d). Причина: %s", GetRPName(playerid), name, warns, ADMIN_WARNS_FOR_BAN, reason);
			}
		}
		case RT_BAN:
		{
			Bans_Add(name, "", reason, admin, value);
			Admin_LogName(admin, Player[playerid][pDbId], "offban", name, accId, reason, value);
			format(msg, sizeof msg, "[A] %s забанил офлайн %s %s. Причина: %s", GetRPName(playerid), name, Admin_BanText(value), reason);
		}
		default: return SendClientMessage(playerid, COLOR_GREY, "%s не в сети — кикать некого.", name);
	}
	SendAdminMessage(COLOR_ADMIN, msg);
	return 1;
}

// Применить наказание к выбранной цели (gPun*)
static AP_Punish(playerid, type, value, const reason[])
{
	if (gPunDb[playerid] == Player[playerid][pDbId]) return SendClientMessage(playerid, COLOR_GREY, "Нельзя применить к себе.");
	if (gPunLevel[playerid] >= Player[playerid][pAdmin]) return SendClientMessage(playerid, COLOR_GREY, "Нельзя наказать админа своего уровня или выше.");
	new t = gPunTarget[playerid];
	if (t == INVALID_PLAYER_ID) return AP_OffPunish(playerid, gPunName[playerid], gPunDb[playerid], type, value, reason);
	if (!IsPlayerConnected(t) || Player[t][pDbId] != gPunDb[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Игрок вышел. Выдай офлайн: /punish %s", gPunName[playerid]);
	if (!Admin_CanPunish(playerid, t)) return 1;
	switch (type)
	{
		case RT_MUTE:  Admin_ApplyMute(playerid, t, value, reason);
		case RT_AJAIL: Admin_ApplyAJail(playerid, t, value, reason);
		case RT_WARN:  Admin_ApplyWarn(playerid, t, reason);
		case RT_KICK:  Admin_ApplyKick(playerid, t, reason);
		case RT_BAN:   Admin_ApplyBan(playerid, t, value, reason);
	}
	return 1;
}

static AP_ApplyRule(playerid, r)
{
	if (!Admin_Check(playerid, gRules[r][ruLevel])) return;
	new repeats = AP_RuleRepeats(gPunDb[playerid], r);
	new value = AP_RuleValue(r, repeats), reason[40];
	if (repeats > 0) format(reason, sizeof reason, "%s (повтор %d)", gRules[r][ruReason], repeats + 1);
	else strcat(reason, gRules[r][ruReason]);
	AP_Punish(playerid, gRules[r][ruType], value, reason);
}

// /punish [id или ник] [код]   и   /dm [id или ник]
static AP_CmdPunish(playerid, const params[], rule)
{
	new pos = 0, who[MAX_PLAYER_NAME], code[12];
	if (!NextParam(params, pos, who))
	{
		if (rule == -1) ShowPlayerDialog(playerid, DIALOG_APLUS_TARGET, DIALOG_STYLE_INPUT, "Наказать по правилам", "{FFFFFF}Введи id игрока или ник (если он не в сети):", "Далее", "Отмена");
		else SendClientMessage(playerid, COLOR_GREY, "Использование: /%s [id или ник] — %s, сейчас: %s", gRules[rule][ruCmd], gRules[rule][ruDesc], AP_PunText(gRules[rule][ruType], gRules[rule][ruBase]));
		return 1;
	}
	if (rule == -1 && NextParam(params, pos, code))
	{
		rule = AP_FindRule(code);
		if (rule == -1) return SendClientMessage(playerid, COLOR_GREY, "Нет пункта «%s». Список: /rules", code);
	}
	if (!AP_SelectTarget(playerid, who)) return 1;
	if (rule == -1) AP_ShowRuleMenu(playerid);
	else AP_ApplyRule(playerid, rule);
	return 1;
}

// /offwarn [ник] [причина]   /offmute /offjail [ник] [минуты] [причина]
static AP_CmdOff(playerid, const params[], type)
{
	new level = (type == RT_MUTE) ? 1 : 2;
	if (!Admin_Check(playerid, level)) return 1;
	new pos = 0, who[MAX_PLAYER_NAME], minutes = 0, reason[64];
	new bool:ok = NextParam(params, pos, who);
	if (ok && type != RT_WARN) ok = NextIntParam(params, pos, minutes);
	if (ok) ok = RestParam(params, pos, reason);
	if (!ok)
	{
		switch (type)
		{
			case RT_WARN:  SendClientMessage(playerid, COLOR_GREY, "Использование: /offwarn [ник] [причина]");
			case RT_MUTE:  SendClientMessage(playerid, COLOR_GREY, "Использование: /offmute [ник] [минуты] [причина] — мут начнётся при входе");
			default:       SendClientMessage(playerid, COLOR_GREY, "Использование: /offjail [ник] [минуты] [причина]");
		}
		return 1;
	}
	new maxv = (type == RT_MUTE) ? ADMIN_MAX_MUTE : ADMIN_MAX_AJAIL;
	if (type != RT_WARN && (minutes < 1 || minutes > maxv)) return SendClientMessage(playerid, COLOR_GREY, "Срок от 1 до %d минут.", maxv);
	if (!AP_SelectTarget(playerid, who)) return 1;
	return AP_Punish(playerid, type, minutes, reason);
}

// /rules — правила для всех
static AP_ShowRules(playerid)
{
	new text[2600], line[160];
	strcat(text, "Нарушение\tНаказание\tКоманда\n");
	for (new i = 0; i < sizeof gRules; i++)
	{
		format(line, sizeof line, "{FFFFFF}%s {AFAFAF}— %s\t{FFCC66}%s\t{AFAFAF}/%s\n", gRules[i][ruReason], gRules[i][ruDesc],
			AP_PunText(gRules[i][ruType], gRules[i][ruBase]), gRules[i][ruCmd]);
		strcat(text, line);
	}
	ShowPlayerDialog(playerid, DIALOG_APLUS_RULES, DIALOG_STYLE_TABLIST_HEADERS, "Правила и наказания (за повтор срок больше)", "%s", "Закрыть", "", text);
}

// =====================================================================
//  История наказаний
// =====================================================================
static AP_ActName(const action[], value, out[], size = sizeof (out))
{
	out[0] = '\0';
	if (!strcmp(action, "kick"))         strcat(out, "{FF9900}Кик", size);
	else if (!strcmp(action, "mute"))    format(out, size, "{FFCC66}Мут %d мин.", value);
	else if (!strcmp(action, "offmute")) format(out, size, "{FFCC66}Мут %d мин. (офлайн)", value);
	else if (!strcmp(action, "ajail"))   format(out, size, "{FF9900}Деморган %d мин.", value);
	else if (!strcmp(action, "offjail")) format(out, size, "{FF9900}Деморган %d мин. (офлайн)", value);
	else if (!strcmp(action, "warn"))    format(out, size, "{FF6347}Варн %d/%d", value, ADMIN_WARNS_FOR_BAN);
	else if (!strcmp(action, "offwarn")) format(out, size, "{FF6347}Варн %d/%d (офлайн)", value, ADMIN_WARNS_FOR_BAN);
	else if (!strcmp(action, "ban") || !strcmp(action, "offban") || !strcmp(action, "banip"))
	{
		if (value > 0) format(out, size, "{FF0000}Бан %d дн.", value);
		else strcat(out, "{FF0000}Бан навсегда", size);
	}
	else if (!strcmp(action, "unban"))   strcat(out, "{33CC33}Разбан", size);
	else if (!strcmp(action, "unmute"))  strcat(out, "{33CC33}Снят мут", size);
	else if (!strcmp(action, "unwarn"))  strcat(out, "{33CC33}Снят варн", size);
	else if (!strcmp(action, "unajail")) strcat(out, "{33CC33}Выпущен из деморгана", size);
	else strcat(out, action, size);
}

stock AdminPlus_ShowHistory(playerid, const name[])
{
	new DBResult:r = DB_ExecuteQuery(gDB,
		"SELECT date, admin, action, value, reason FROM admin_log WHERE target = '%q' COLLATE NOCASE AND action IN (" AP_PUNISH_ACTIONS ",'unban','unmute','unwarn','unajail') ORDER BY id DESC LIMIT 25", name);
	if (r == DBResult:0) return SendClientMessage(playerid, COLOR_RED, "Ошибка чтения базы.");
	new text[3600], line[180], admin[MAX_PLAYER_NAME], action[16], what[48], reason[64], title[64];
	new rows = DB_GetRowCount(r), now = gettime();
	strcat(text, "Когда\tНаказание\tАдминистратор\tПричина\n");
	for (new i = 0; i < rows; i++)
	{
		DB_GetFieldString(r, 1, admin, sizeof admin);
		DB_GetFieldString(r, 2, action, sizeof action);
		DB_GetFieldString(r, 4, reason, sizeof reason);
		AP_ActName(action, DB_GetFieldInt(r, 3), what);
		format(line, sizeof line, "{AFAFAF}%s назад\t%s\t{FFFFFF}%s\t{AFAFAF}%s\n", Admin_FormatTime(now - DB_GetFieldInt(r, 0)), what, admin, reason);
		if (strlen(text) + strlen(line) >= sizeof (text) - 1) break;
		strcat(text, line);
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	if (rows == 0) strcat(text, "{33CC33}Наказаний нет\t\t\t\n");
	format(title, sizeof title, "История наказаний: %s", name);
	ShowPlayerDialog(playerid, DIALOG_APLUS_HIST, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Закрыть", "", text);
	return 1;
}

static AP_CmdHistory(playerid, const params[])
{
	new pos = 0, who[MAX_PLAYER_NAME], name[MAX_PLAYER_NAME];
	if (!NextParam(params, pos, who) || Player[playerid][pAdmin] < 1)
	{
		GetPlayerName(playerid, name, sizeof name);
		return AdminPlus_ShowHistory(playerid, name);
	}
	if (IsNumeric(who))
	{
		new t = strval(who);
		if (t < 0 || t >= MAX_PLAYERS || !IsPlayerConnected(t)) return SendClientMessage(playerid, COLOR_GREY, "Игрока с id %s нет в сети. Укажи ник.", who);
		GetPlayerName(t, name, sizeof name);
	}
	else strcat(name, who);
	return AdminPlus_ShowHistory(playerid, name);
}

// /check -> кнопка «Действия»
stock AdminPlus_SetCheck(playerid, target)
{
	gCheckTarget[playerid] = target;
	gCheckDb[playerid] = Player[target][pDbId];
}

// =====================================================================
//  Лента античита
// =====================================================================
stock AdminPlus_OnAntiCheat(playerid, const what[])
{
	new i = gAcfHead;
	gAcfHead = (gAcfHead + 1) % AC_FEED_MAX;
	gAcf[i][acfTime] = gettime();
	gAcf[i][acfDb] = Player[playerid][pDbId];
	GetPlayerName(playerid, gAcf[i][acfName], MAX_PLAYER_NAME);
	gAcf[i][acfText][0] = '\0';
	strcat(gAcf[i][acfText], what, 64);
	gAcf[i][acfMobile] = IsPlayerMobile[playerid];
}

static AP_AcOnline(i)
{
	new other[MAX_PLAYER_NAME];
	for (new p = 0; p < MAX_PLAYERS; p++)
	{
		if (!IsPlayerConnected(p)) continue;
		if (gAcf[i][acfDb] != 0 && Player[p][pDbId] == gAcf[i][acfDb]) return p;
		GetPlayerName(p, other, sizeof other);
		if (gAcf[i][acfDb] == 0 && !strcmp(other, gAcf[i][acfName])) return p;
	}
	return INVALID_PLAYER_ID;
}

static AP_ShowAcFeed(playerid)
{
	new text[3600], line[160], n = 0, now = gettime();
	strcat(text, "Когда\tИгрок\tЧто\n");
	for (new k = 1; k <= AC_FEED_MAX; k++)
	{
		new i = (gAcfHead - k + AC_FEED_MAX) % AC_FEED_MAX;
		if (gAcf[i][acfTime] == 0) continue;
		new t = AP_AcOnline(i);
		new who[48];
		if (t != INVALID_PLAYER_ID) format(who, sizeof who, "{FFFFFF}%s[%d]%s", gAcf[i][acfName], t, gAcf[i][acfMobile] ? (" {AFAFAF}тел.") : (""));
		else format(who, sizeof who, "{AFAFAF}%s (вышел)", gAcf[i][acfName]);
		format(line, sizeof line, "{AFAFAF}%s назад\t%s\t{FF6347}%s\n", Admin_FormatTime(now - gAcf[i][acfTime]), who, gAcf[i][acfText]);
		if (strlen(text) + strlen(line) >= sizeof (text) - 1) break;
		strcat(text, line);
		gAcfList[playerid][n++] = i;
	}
	if (n == 0) return SendClientMessage(playerid, COLOR_ADMIN, "Античит пока ничего не заметил.");
	ShowPlayerDialog(playerid, DIALOG_APLUS_AC, DIALOG_STYLE_TABLIST_HEADERS, "Античит: последние срабатывания", "%s", "Выбрать", "Закрыть", text);
	return 1;
}

static AP_ShowAcActions(playerid)
{
	new i = gAcfSel[playerid], title[64];
	format(title, sizeof title, "%s: %s", gAcf[i][acfName], gAcf[i][acfText]);
	ShowPlayerDialog(playerid, DIALOG_APLUS_AC_ACT, DIALOG_STYLE_LIST, title,
		"Следить\nТелепорт к нему\nПроверить (/check)\nКикнуть (читы)\nНаказать по правилам\nИстория наказаний\n{AFAFAF}Ложное срабатывание (убрать из ленты)", "Выбрать", "Назад");
}

// =====================================================================
//  /aip — другие аккаунты с тех же IP (журнал logs.db)
// =====================================================================
static AP_CmdAip(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return 1;
	new pos = 0, who[MAX_PLAYER_NAME], name[MAX_PLAYER_NAME];
	if (!NextParam(params, pos, who)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /aip [id или ник] — другие аккаунты с тех же IP");
	if (IsNumeric(who))
	{
		new t = strval(who);
		if (t < 0 || t >= MAX_PLAYERS || !IsPlayerConnected(t)) return SendClientMessage(playerid, COLOR_GREY, "Игрока с id %s нет в сети. Укажи ник.", who);
		GetPlayerName(t, name, sizeof name);
	}
	else strcat(name, who);
	if (gLogDB == DB:0) return SendClientMessage(playerid, COLOR_RED, "Журнал logs.db не открыт.");
	Log_Flush();

	new DBResult:r = DB_ExecuteQuery(gLogDB,
		"SELECT name, ip, MAX(time) FROM logs WHERE type = 'conn' AND name <> '' AND name <> '%q' COLLATE NOCASE AND ip IN (SELECT ip FROM logs WHERE type = 'conn' AND name = '%q' COLLATE NOCASE AND ip <> '' GROUP BY ip ORDER BY MAX(id) DESC LIMIT 5) GROUP BY name ORDER BY 3 DESC LIMIT 30",
		name, name);
	if (r == DBResult:0) return SendClientMessage(playerid, COLOR_RED, "Ошибка чтения журнала.");
	new text[2600], line[128], other[MAX_PLAYER_NAME], ip[16], title[64], rows = DB_GetRowCount(r), now = gettime();
	strcat(text, "Аккаунт\tIP\tПоследний вход\n");
	for (new i = 0; i < rows; i++)
	{
		DB_GetFieldString(r, 0, other, sizeof other);
		DB_GetFieldString(r, 1, ip, sizeof ip);
		new online = AP_FindOnline(other);
		if (online != INVALID_PLAYER_ID) format(line, sizeof line, "{FFFFFF}%s [%d]\t{AFAFAF}%s\t{33CC33}онлайн\n", other, online, ip);
		else format(line, sizeof line, "{FFFFFF}%s\t{AFAFAF}%s\t{AFAFAF}%s назад\n", other, ip, Admin_FormatTime(now - DB_GetFieldInt(r, 2)));
		strcat(text, line);
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	if (rows == 0) strcat(text, "{33CC33}Других аккаунтов нет\t\t\n");
	format(title, sizeof title, "Аккаунты с IP %s", name);
	ShowPlayerDialog(playerid, DIALOG_ADMIN_INFO, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Закрыть", "", text);
	return 1;
}

// =====================================================================
//  /atop — рейтинг и норма за неделю
// =====================================================================
// Поля: 0 name, 1 admin, 2 минуты, 3 ответы, 4 «помог», 5 «не помог», 6 наказания
static DBResult:AP_TopQuery(week)
{
	new from = AP_WeekStart(week), to = from + 604800;
	return DB_ExecuteQuery(gDB,
		"SELECT a.name, a.admin, IFNULL((SELECT minutes FROM admin_week w WHERE w.name = a.name AND w.week = %d), 0), \
		(SELECT COUNT(*) FROM reports r WHERE r.admin = a.name AND r.status = 1 AND r.answer_date >= %d AND r.answer_date < %d), \
		(SELECT COUNT(*) FROM reports r WHERE r.admin = a.name AND r.rating = 1 AND r.answer_date >= %d AND r.answer_date < %d), \
		(SELECT COUNT(*) FROM reports r WHERE r.admin = a.name AND r.rating = -1 AND r.answer_date >= %d AND r.answer_date < %d), \
		(SELECT COUNT(*) FROM admin_log l WHERE l.admin = a.name AND l.date >= %d AND l.date < %d AND l.action IN (" AP_PUNISH_ACTIONS ")) \
		FROM accounts a WHERE a.admin > 0 ORDER BY 4 DESC, 3 DESC LIMIT 30",
		week, from, to, from, to, from, to, from, to);
}

static AP_ShowTop(playerid, week)
{
	gTopWeek[playerid] = week;
	new DBResult:r = AP_TopQuery(week);
	if (r == DBResult:0) return SendClientMessage(playerid, COLOR_RED, "Ошибка чтения базы.");
	new cur = AP_Week(gettime());
	new text[3600], line[160], name[MAX_PLAYER_NAME], title[64], rows = DB_GetRowCount(r);
	strcat(text, "Администратор\tЧасы · ответы · наказ.\tОценки\tНорма\n");
	strcat(text, (week == cur) ? ("{AFAFAF}« Показать прошлую неделю\t\t\t\n") : ("{AFAFAF}» Показать эту неделю\t\t\t\n"));
	for (new i = 0; i < rows && i < 30; i++)
	{
		DB_GetFieldString(r, 0, name, sizeof name);
		new minutes = DB_GetFieldInt(r, 2), answers = DB_GetFieldInt(r, 3);
		new bool:ok = (minutes >= ADMIN_NORM_MINUTES && answers >= ADMIN_NORM_REPORTS);
		format(line, sizeof line, "{FFFFFF}%d. %s {AFAFAF}(%d)\t{FFFFFF}%d.%d ч · %d · %d\t{33CC33}+%d {FF6347}-%d\t%s\n",
			i + 1, name, DB_GetFieldInt(r, 1), minutes / 60, (minutes % 60) / 6, answers, DB_GetFieldInt(r, 6),
			DB_GetFieldInt(r, 4), DB_GetFieldInt(r, 5), ok ? ("{33CC33}да") : ((week == cur) ? ("{FF9900}пока нет") : ("{FF6347}нет")));
		strcat(text, line);
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	format(title, sizeof title, "Рейтинг админов: %s (норма %d ч и %d ответов)", (week == cur) ? ("эта неделя") : ("прошлая неделя"), ADMIN_NORM_MINUTES / 60, ADMIN_NORM_REPORTS);
	ShowPlayerDialog(playerid, DIALOG_APLUS_TOP, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Подробнее", "Закрыть", text);
	return 1;
}

// Вошёл в админку: старшим — кто не выполнил норму прошлой недели
stock AdminPlus_OnWelcome(playerid)
{
	if (Player[playerid][pAdmin] < 5) return;
	new total = 0, failed = 0;
	new DBResult:r = AP_TopQuery(AP_Week(gettime()) - 1);
	if (r == DBResult:0) return;
	new rows = DB_GetRowCount(r);
	for (new i = 0; i < rows; i++)
	{
		total++;
		if (DB_GetFieldInt(r, 2) < ADMIN_NORM_MINUTES || DB_GetFieldInt(r, 3) < ADMIN_NORM_REPORTS) failed++;
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	if (failed > 0)
		SendClientMessage(playerid, COLOR_ORANGE, "[A] Прошлую неделю норму не выполнили: %d из %d админов. Подробно: /atop", failed, total);
}

// =====================================================================
//  Мероприятия: /event (админ), /mp (игрок)
// =====================================================================
static AP_EvCount()
{
	new n = 0;
	for (new i = 0; i < MAX_PLAYERS; i++) if (gEvIn[i] && IsPlayerConnected(i)) n++;
	return n;
}

static AP_EvPlace(i)
{
	if (IsPlayerInAnyVehicle(i)) RemovePlayerFromVehicle(i);
	SetPlayerInterior(i, gEvInt);
	SetPlayerVirtualWorld(i, gEvWorld);
	SetPlayerPos(i, gEvX + float(random(7) - 3), gEvY + float(random(7) - 3), gEvZ);
	SetPlayerFacingAngle(i, gEvA);
	SetCameraBehindPlayer(i);
	Player[i][pHouseInside] = -1;
}

static AP_EvReturn(i)
{
	if (!gEvIn[i]) return;
	gEvIn[i] = false;
	if (gEvFrozen[i]) { TogglePlayerControllable(i, true); gEvFrozen[i] = false; }
	if (!IsPlayerConnected(i)) return;
	if (Admin_IsInAJail(i)) return;     // за время МП успел попасть в деморган
	if (IsPlayerInAnyVehicle(i)) RemovePlayerFromVehicle(i);
	SetPlayerInterior(i, gEvBackInt[i]);
	SetPlayerVirtualWorld(i, gEvBackWorld[i]);
	SetPlayerPos(i, gEvBack[i][0], gEvBack[i][1], gEvBack[i][2]);
	SetPlayerFacingAngle(i, gEvBack[i][3]);
	SetCameraBehindPlayer(i);
	Player[i][pHouseInside] = gEvBackHouse[i];
	SendClientMessage(i, COLOR_YELLOW, "[МП] Ты возвращён на место.");
}

static AP_EventEnd(adminid)
{
	for (new i = 0; i < MAX_PLAYERS; i++) if (gEvIn[i]) AP_EvReturn(i);
	if (adminid != INVALID_PLAYER_ID)
	{
		SendClientMessageToAll(COLOR_YELLOW, "[МП] Мероприятие «%s» завершено. Спасибо за участие!", gEvName);
		Admin_Log(adminid, "event_end", INVALID_PLAYER_ID, gEvName);
	}
	else SendClientMessageToAll(COLOR_YELLOW, "[МП] Мероприятие «%s» отменено: ведущий вышел.", gEvName);
	gEvActive = false;
	gEvOpen = false;
	gEvHost = INVALID_PLAYER_ID;
}

static AP_ShowEvent(playerid)
{
	if (!gEvActive)
	{
		ShowPlayerDialog(playerid, DIALOG_APLUS_EV_NEW, DIALOG_STYLE_INPUT, "Новое мероприятие",
			"{FFFFFF}Участники появятся там, где ты стоишь сейчас.\n\nВведи приз в $ (0 — без приза) и название.\nНапример: {FFCC66}50000 Гонка на Infernus", "Создать", "Отмена");
		return 1;
	}
	new text[600], title[80];
	format(text, sizeof text, "Набор: %s (переключить)\nУчастники (%d)\nЗаморозить всех\nРазморозить всех\nСобрать всех на точку МП\nОтсчёт 3-2-1\nПеренести точку МП сюда\n{FF6347}Завершить МП (вернуть всех)",
		gEvOpen ? ("{33CC33}открыт{FFFFFF}") : ("{FF6347}закрыт{FFFFFF}"), AP_EvCount());
	format(title, sizeof title, "МП «%s», приз $%d", gEvName, gEvPrize);
	ShowPlayerDialog(playerid, DIALOG_APLUS_EVENT, DIALOG_STYLE_LIST, title, "%s", "Выбрать", "Закрыть", text);
	return 1;
}

static AP_ShowEvPlayers(playerid)
{
	new text[3000], line[80], n = 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!gEvIn[i] || !IsPlayerConnected(i)) continue;
		format(line, sizeof line, "%s [%d]%s\n", GetRPName(i), i, gEvFrozen[i] ? (" {AFAFAF}(заморожен)") : (""));
		if (strlen(text) + strlen(line) >= sizeof (text) - 1 || n >= EVENT_LIST_MAX) break;
		strcat(text, line);
		gEvList[playerid][n++] = i;
	}
	if (n == 0) { SendClientMessage(playerid, COLOR_GREY, "Участников пока нет."); AP_ShowEvent(playerid); return; }
	ShowPlayerDialog(playerid, DIALOG_APLUS_EV_PL, DIALOG_STYLE_LIST, "Участники МП", "%s", "Выбрать", "Назад", text);
}

static AP_CmdMp(playerid)
{
	if (gEvIn[playerid])
	{
		AP_EvReturn(playerid);
		SendClientMessage(playerid, COLOR_YELLOW, "[МП] Ты покинул мероприятие.");
		return 1;
	}
	if (!gEvActive) return SendClientMessage(playerid, COLOR_GREY, "Сейчас нет мероприятий.");
	if (!gEvOpen) return SendClientMessage(playerid, COLOR_GREY, "Набор на мероприятие закрыт.");
	if (Admin_IsInAJail(playerid) || Player[playerid][pJail] > 0) return SendClientMessage(playerid, COLOR_GREY, "Из тюрьмы на мероприятие нельзя.");
	if (Admin_IsSpectating(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Сначала выйди из слежки: /specoff");
	GetPlayerPos(playerid, gEvBack[playerid][0], gEvBack[playerid][1], gEvBack[playerid][2]);
	GetPlayerFacingAngle(playerid, gEvBack[playerid][3]);
	gEvBackInt[playerid] = GetPlayerInterior(playerid);
	gEvBackWorld[playerid] = GetPlayerVirtualWorld(playerid);
	gEvBackHouse[playerid] = Player[playerid][pHouseInside];
	gEvIn[playerid] = true;
	gEvFrozen[playerid] = false;
	AP_EvPlace(playerid);
	SendClientMessage(playerid, COLOR_YELLOW, "[МП] Ты на мероприятии «%s». Выйти: /mp", gEvName);
	if (IsPlayerConnected(gEvHost)) SendClientMessage(gEvHost, COLOR_GREY, "[МП] %s[%d] присоединился. Участников: %d", GetRPName(playerid), playerid, AP_EvCount());
	return 1;
}

public AP_Countdown(n)
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || (!gEvIn[i] && i != gEvHost)) continue;
		if (n > 0) { GameTextForPlayer(i, "~y~%d", 1100, 4, n); PlayerPlaySound(i, 1056, 0.0, 0.0, 0.0); }
		else { GameTextForPlayer(i, "~g~GO!", 1500, 4); PlayerPlaySound(i, 1057, 0.0, 0.0, 0.0); }
	}
	if (n > 0) SetTimerEx("AP_Countdown", 1000, false, "i", n - 1);
	return 1;
}

// =====================================================================
//  /ann, /cc
// =====================================================================
static AP_Announce(playerid, const text[])
{
	new body[300];
	format(body, sizeof body, "{FFFFFF}%s\n\n{AFAFAF}— %s, администрация", text, GetRPName(playerid));
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged]) continue;
		SendClientMessage(i, COLOR_YELLOW, "[Объявление] %s", text);
		ShowPlayerDialog(i, DIALOG_ADMIN_INFO, DIALOG_STYLE_MSGBOX, "Объявление администрации", "%s", "Понятно", "", body);
		PlayerPlaySound(i, 1150, 0.0, 0.0, 0.0);
	}
	Admin_Log(playerid, "ann", INVALID_PLAYER_ID, text);
}

static AP_ClearChat(playerid)
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i)) continue;
		for (new k = 0; k < 40; k++) SendClientMessage(i, COLOR_WHITE, " ");
		SendClientMessage(i, COLOR_GREY, "Чат очищен администратором %s.", GetRPName(playerid));
	}
	Admin_Log(playerid, "cc", INVALID_PLAYER_ID);
}

// =====================================================================
//  Общая панель /amenu
// =====================================================================
stock bool:Cmd_AMenuHub(playerid)
{
	if (!Admin_Check(playerid, 1)) return true;
	new online = 0, acf = 0, text[900];
	for (new i = 0; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && Player[i][pLogged]) online++;
	for (new i = 0; i < AC_FEED_MAX; i++) if (gAcf[i][acfTime] > gettime() - 3600) acf++;
	new open = Reports_CountOpen();
	format(text, sizeof text,
		"Игроки онлайн {AFAFAF}(%d)\n%sРепорты (%d)\n{FFFFFF}Наказать по правилам\n%sАнтичит за час (%d)\n{FFFFFF}Телепорты\nМероприятие%s\nОбъявление на экран\nОчистить чат\nРейтинг админов и норма\nМоя статистика\nЛог действий\nПравила и сроки наказаний\nВсе команды",
		online, (open > 0) ? ("{FFCC00}") : ("{FFFFFF}"), open, (acf > 0) ? ("{FF6347}") : ("{FFFFFF}"), acf, gEvActive ? (" {33CC33}(идёт)") : (""));
	ShowPlayerDialog(playerid, DIALOG_APLUS_HUB, DIALOG_STYLE_LIST, "Админ-панель", "%s", "Открыть", "Закрыть", text);
	return true;
}

// =====================================================================
//  Команды (из Admin_OnCommand)
// =====================================================================
stock AdminPlus_OnCommand(playerid, const cmd[], const params[])
{
	// Для всех игроков
	if (!strcmp(cmd, "/mp", true))      { AP_CmdMp(playerid); return 1; }
	if (!strcmp(cmd, "/rules", true))   { AP_ShowRules(playerid); return 1; }
	if (!strcmp(cmd, "/history", true)) { AP_CmdHistory(playerid, params); return 1; }

	if (Player[playerid][pAdmin] < 1) return 0;

	if (!strcmp(cmd, "/punish", true))  { if (Admin_Check(playerid, 1)) AP_CmdPunish(playerid, params, -1); return 1; }
	if (cmd[0] == '/')
	{
		new r = AP_FindRule(cmd[1]);
		if (r != -1) { if (Admin_Check(playerid, gRules[r][ruLevel])) AP_CmdPunish(playerid, params, r); return 1; }
	}
	if (!strcmp(cmd, "/offwarn", true)) { AP_CmdOff(playerid, params, RT_WARN); return 1; }
	if (!strcmp(cmd, "/offmute", true)) { AP_CmdOff(playerid, params, RT_MUTE); return 1; }
	if (!strcmp(cmd, "/offjail", true)) { AP_CmdOff(playerid, params, RT_AJAIL); return 1; }
	if (!strcmp(cmd, "/acheck", true))  { AP_ShowAcFeed(playerid); return 1; }
	if (!strcmp(cmd, "/aip", true))     { AP_CmdAip(playerid, params); return 1; }
	if (!strcmp(cmd, "/atop", true))    { AP_ShowTop(playerid, AP_Week(gettime())); return 1; }
	if (!strcmp(cmd, "/event", true))   { if (Admin_Check(playerid, 2)) AP_ShowEvent(playerid); return 1; }
	if (!strcmp(cmd, "/cc", true))      { if (Admin_Check(playerid, 2)) AP_ClearChat(playerid); return 1; }
	if (!strcmp(cmd, "/ann", true))
	{
		if (!Admin_Check(playerid, 3)) return 1;
		new text[128];
		if (!RestParam(params, 0, text))
			ShowPlayerDialog(playerid, DIALOG_APLUS_ANN, DIALOG_STYLE_INPUT, "Объявление", "{FFFFFF}Текст объявления — его увидят все игроки в окне:", "Отправить", "Отмена");
		else AP_Announce(playerid, text);
		return 1;
	}
	return 0;
}

// =====================================================================
//  Окна (из Admin_OnDialog)
// =====================================================================
stock AdminPlus_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid < DIALOG_APLUS_HUB || dialogid > DIALOG_APLUS_RULES) return 0;
	if (dialogid == DIALOG_APLUS_HIST || dialogid == DIALOG_APLUS_RULES) return 1;
	if (Player[playerid][pAdmin] < 1) return 1;

	switch (dialogid)
	{
		case DIALOG_APLUS_HUB:
		{
			if (!response) return 1;
			switch (listitem)
			{
				case 0: Cmd_AMenuPlayers(playerid);
				case 1: Cmd_Reports(playerid);
				case 2: AP_CmdPunish(playerid, "", -1);
				case 3: AP_ShowAcFeed(playerid);
				case 4: Cmd_Tp(playerid, "");
				case 5: if (Admin_Check(playerid, 2)) AP_ShowEvent(playerid);
				case 6: if (Admin_Check(playerid, 3)) ShowPlayerDialog(playerid, DIALOG_APLUS_ANN, DIALOG_STYLE_INPUT, "Объявление", "{FFFFFF}Текст объявления — его увидят все игроки в окне:", "Отправить", "Отмена");
				case 7: if (Admin_Check(playerid, 2)) AP_ClearChat(playerid);
				case 8: AP_ShowTop(playerid, AP_Week(gettime()));
				case 9: AdminAuth_OnCommand(playerid, "/astats", "");
				case 10: Cmd_ALog(playerid, "");
				case 11: AP_ShowRules(playerid);
				case 12: Cmd_AHelp(playerid);
			}
			return 1;
		}

		case DIALOG_APLUS_TARGET:
		{
			if (!response || inputtext[0] == '\0') return 1;
			new who[MAX_PLAYER_NAME];
			strcat(who, inputtext);
			if (AP_SelectTarget(playerid, who)) AP_ShowRuleMenu(playerid);
			return 1;
		}

		case DIALOG_APLUS_PUNISH:
		{
			if (!response || listitem < 0 || listitem >= sizeof gRules) return 1;
			AP_ApplyRule(playerid, gPunList[playerid][listitem]);
			return 1;
		}

		case DIALOG_APLUS_CHECK:
		{
			if (!response) return 1;
			new t = gCheckTarget[playerid];
			if (t == INVALID_PLAYER_ID || !IsPlayerConnected(t) || Player[t][pDbId] != gCheckDb[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Игрок уже вышел.");
			Admin_OpenActions(playerid, t);
			return 1;
		}

		case DIALOG_APLUS_AC:
		{
			if (!response || listitem < 0 || listitem >= AC_FEED_MAX) return 1;
			gAcfSel[playerid] = gAcfList[playerid][listitem];
			if (gAcf[gAcfSel[playerid]][acfTime] == 0) return AP_ShowAcFeed(playerid);
			AP_ShowAcActions(playerid);
			return 1;
		}

		case DIALOG_APLUS_AC_ACT:
		{
			if (!response) return AP_ShowAcFeed(playerid);
			new i = gAcfSel[playerid];
			if (listitem == 6)
			{
				gAcf[i][acfTime] = 0;
				SendClientMessage(playerid, COLOR_ADMIN, "Запись убрана из ленты.");
				return AP_ShowAcFeed(playerid);
			}
			if (listitem == 5) return AdminPlus_ShowHistory(playerid, gAcf[i][acfName]);
			if (listitem == 4)
			{
				if (AP_SelectTarget(playerid, gAcf[i][acfName])) AP_ShowRuleMenu(playerid);
				return 1;
			}
			new t = AP_AcOnline(i);
			if (t == INVALID_PLAYER_ID) { SendClientMessage(playerid, COLOR_GREY, "Игрок вышел. Можно наказать офлайн или посмотреть историю."); AP_ShowAcActions(playerid); return 1; }
			new p[8];
			valstr(p, t);
			switch (listitem)
			{
				case 0: Cmd_Spec(playerid, p);
				case 1: Cmd_Goto(playerid, p);
				case 2: Cmd_Check(playerid, p);
				case 3: if (Admin_CanPunish(playerid, t)) Admin_ApplyKick(playerid, t, "Читы (античит)");
			}
			return 1;
		}

		case DIALOG_APLUS_TOP:
		{
			if (!response) return 1;
			if (listitem == 0)
			{
				new cur = AP_Week(gettime());
				return AP_ShowTop(playerid, (gTopWeek[playerid] == cur) ? (cur - 1) : cur);
			}
			// Строку перечитываем из базы: так не нужно держать ники в памяти
			new DBResult:r = AP_TopQuery(gTopWeek[playerid]), name[MAX_PLAYER_NAME + 2];
			if (r == DBResult:0) return 1;
			new bool:found = (listitem - 1 < DB_GetRowCount(r));
			if (found)
			{
				for (new k = 0; k < listitem - 1; k++) DB_SelectNextRow(r);
				new nick[MAX_PLAYER_NAME];
				DB_GetFieldString(r, 0, nick, sizeof nick);
				format(name, sizeof name, " %s", nick);
			}
			DB_FreeResultSet(r);
			if (found) AdminAuth_OnCommand(playerid, "/astats", name);
			return 1;
		}

		case DIALOG_APLUS_ANN:
		{
			if (!response || inputtext[0] == '\0' || !Admin_Check(playerid, 3)) return 1;
			new text[128];
			strcat(text, inputtext);
			AP_Announce(playerid, text);
			return 1;
		}

		case DIALOG_APLUS_EV_NEW:
		{
			if (!response || inputtext[0] == '\0' || !Admin_Check(playerid, 2)) return 1;
			if (gEvActive) return AP_ShowEvent(playerid);
			new pos = 0, prize = 0, name[48];
			new p = 0, buf[16];
			if (NextParam(inputtext, p, buf) && IsNumeric(buf)) { prize = strval(buf); pos = p; }
			if (!RestParam(inputtext, pos, name)) strcat(name, "Мероприятие");
			if (prize < 0 || prize > EVENT_MAX_PRIZE) return SendClientMessage(playerid, COLOR_GREY, "Приз от 0 до $%d.", EVENT_MAX_PRIZE);
			if (prize > 0 && Player[playerid][pAdmin] < 3) return SendClientMessage(playerid, COLOR_GREY, "МП с призом — с 3 уровня. Укажи приз 0.");
			GetPlayerPos(playerid, gEvX, gEvY, gEvZ);
			GetPlayerFacingAngle(playerid, gEvA);
			gEvInt = GetPlayerInterior(playerid);
			gEvWorld = GetPlayerVirtualWorld(playerid);
			gEvName[0] = '\0';
			strcat(gEvName, name);
			gEvPrize = prize;
			gEvPrizeGiven = false;
			gEvActive = true;
			gEvOpen = true;
			gEvHost = playerid;
			Admin_Log(playerid, "event", INVALID_PLAYER_ID, gEvName, prize);
			if (prize > 0) SendClientMessageToAll(COLOR_YELLOW, "[МП] Администратор %s проводит мероприятие «%s». Приз: $%d. Участвовать: /mp", GetRPName(playerid), gEvName, prize);
			else SendClientMessageToAll(COLOR_YELLOW, "[МП] Администратор %s проводит мероприятие «%s». Участвовать: /mp", GetRPName(playerid), gEvName);
			AP_ShowEvent(playerid);
			return 1;
		}

		case DIALOG_APLUS_EVENT:
		{
			if (!response || !gEvActive) return 1;
			if (gEvHost != playerid && Player[playerid][pAdmin] < 4) return SendClientMessage(playerid, COLOR_GREY, "МП ведёт %s. Управлять может ведущий или админ 4+ уровня.", GetRPName(gEvHost));
			switch (listitem)
			{
				case 0:
				{
					gEvOpen = !gEvOpen;
					if (gEvOpen) SendClientMessageToAll(COLOR_YELLOW, "[МП] Набор на «%s» снова открыт: /mp", gEvName);
					else SendClientMessageToAll(COLOR_YELLOW, "[МП] Набор на «%s» закрыт. Участников: %d", gEvName, AP_EvCount());
					AP_ShowEvent(playerid);
				}
				case 1: AP_ShowEvPlayers(playerid);
				case 2, 3:
				{
					new bool:freeze = (listitem == 2);
					for (new i = 0; i < MAX_PLAYERS; i++)
					{
						if (!gEvIn[i] || !IsPlayerConnected(i)) continue;
						TogglePlayerControllable(i, !freeze);
						gEvFrozen[i] = freeze;
						SendClientMessage(i, COLOR_YELLOW, freeze ? ("[МП] Участники заморожены.") : ("[МП] Можно двигаться!"));
					}
					AP_ShowEvent(playerid);
				}
				case 4:
				{
					for (new i = 0; i < MAX_PLAYERS; i++) if (gEvIn[i] && IsPlayerConnected(i)) AP_EvPlace(i);
					AP_ShowEvent(playerid);
				}
				case 5: AP_Countdown(3);
				case 6:
				{
					GetPlayerPos(playerid, gEvX, gEvY, gEvZ);
					GetPlayerFacingAngle(playerid, gEvA);
					gEvInt = GetPlayerInterior(playerid);
					gEvWorld = GetPlayerVirtualWorld(playerid);
					SendClientMessage(playerid, COLOR_ADMIN, "Точка МП перенесена сюда.");
					AP_ShowEvent(playerid);
				}
				case 7: AP_EventEnd(playerid);
			}
			return 1;
		}

		case DIALOG_APLUS_EV_PL:
		{
			if (!response) return AP_ShowEvent(playerid);
			if (listitem < 0 || listitem >= EVENT_LIST_MAX) return 1;
			new t = gEvList[playerid][listitem];
			if (!IsPlayerConnected(t) || !gEvIn[t]) { AP_ShowEvPlayers(playerid); return 1; }
			gEvSel[playerid] = t;
			new title[48], text[160];
			format(title, sizeof title, "%s [%d]", GetRPName(t), t);
			if (gEvPrize > 0 && !gEvPrizeGiven) format(text, sizeof text, "Выдать приз ($%d) — победитель\nУбрать с МП (вернуть на место)", gEvPrize);
			else format(text, sizeof text, "{AFAFAF}Приз %s\n{FFFFFF}Убрать с МП (вернуть на место)", gEvPrizeGiven ? ("уже выдан") : ("не назначен"));
			ShowPlayerDialog(playerid, DIALOG_APLUS_EV_PA, DIALOG_STYLE_LIST, title, "%s", "Выбрать", "Назад", text);
			return 1;
		}

		case DIALOG_APLUS_EV_PA:
		{
			if (!response) { AP_ShowEvPlayers(playerid); return 1; }
			new t = gEvSel[playerid];
			if (!IsPlayerConnected(t) || !gEvIn[t]) { AP_ShowEvPlayers(playerid); return 1; }
			if (listitem == 0)
			{
				if (gEvPrize <= 0 || gEvPrizeGiven) { AP_ShowEvPlayers(playerid); return 1; }
				if (t == playerid) return SendClientMessage(playerid, COLOR_GREY, "Себе приз выдать нельзя.");
				gEvPrizeGiven = true;
				GiveMoney(t, gEvPrize, "приз мероприятия");
				Admin_Log(playerid, "event_prize", t, gEvName, gEvPrize);
				SendClientMessageToAll(COLOR_YELLOW, "[МП] Победитель «%s» — %s! Приз $%d.", gEvName, GetRPName(t), gEvPrize);
				AP_ShowEvent(playerid);
			}
			else
			{
				AP_EvReturn(t);
				SendClientMessage(playerid, COLOR_ADMIN, "%s убран с мероприятия.", GetRPName(t));
				AP_ShowEvPlayers(playerid);
			}
			return 1;
		}
	}
	return 1;
}
