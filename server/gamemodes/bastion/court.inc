// =====================================================================
//  court.inc — суд и адвокаты: /lawyer, /free, /appeal, /cases.
//
//  Адвокат. Лицензию покупает любой игрок с уровня LAWYER_MIN_LEVEL:
//  /lawyer (цена LAWYER_LICENSE). Адвокат пишет /free [id] [гонорар]
//  заключённому (КПЗ или тюрьма) со сроком до LAWYER_MAX_TERM минут.
//  Тот видит окно: гонорар адвокату + залог штату (COURT_BAIL_PER_MIN за
//  каждую оставшуюся минуту). Согласился и хватило денег — выходит на
//  свободу. Залог идёт в казну штата. Число выигранных дел копится
//  (accounts.lawyer_cases) и видно в /lawyer.
//
//  Суд. Заключённый подаёт апелляцию: /appeal [текст] (одна открытая на
//  игрока, раз в COURT_APPEAL_COOLDOWN минут). Судьи — сотрудники
//  правительства ранга COURT_JUDGE_RANK+ и админы COURT_JUDGE_ADMIN+.
//  Судья: /cases — список открытых дел, в деле решение:
//    оправдать (свобода), скостить половину срока, оставить в силе.
//  Заключённый вышел из игры — дело ждёт его. Решения пишутся в court_cases.
// =====================================================================

#define LAWYER_LICENSE          25000
#define LAWYER_MIN_LEVEL        4
#define LAWYER_MAX_TERM         60      // минут: больше — только через суд
#define LAWYER_FEE_MIN          500
#define LAWYER_FEE_MAX          50000
#define LAWYER_OFFER_COOLDOWN   120     // секунд между предложениями одному заключённому
#define COURT_BAIL_PER_MIN      150     // залог штату, $ за минуту срока
#define COURT_JUDGE_RANK        6
#define COURT_JUDGE_ADMIN       3
#define COURT_APPEAL_COOLDOWN   30      // минут
#define COURT_LIST_MAX          20

static bool:gLawyer[MAX_PLAYERS];
static gLawyerCases[MAX_PLAYERS];
static gFreeFrom[MAX_PLAYERS] = { INVALID_PLAYER_ID, ... };    // какой адвокат предложил
static gFreeFee[MAX_PLAYERS];
static gFreeAt[MAX_PLAYERS];            // когда было последнее предложение (gettime)
static gAppealAt[MAX_PLAYERS];
static gCaseList[MAX_PLAYERS][COURT_LIST_MAX];  // номера дел в окне /cases
static gCaseSel[MAX_PLAYERS];

stock Court_Init()
{
	DB_AddColumn("accounts", "lawyer", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "lawyer_cases", "INTEGER DEFAULT 0");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS court_cases (id INTEGER PRIMARY KEY AUTOINCREMENT, date INTEGER, player_id INTEGER, player TEXT, term INTEGER, text TEXT, status INTEGER DEFAULT 0, judge TEXT DEFAULT '', verdict TEXT DEFAULT '', vdate INTEGER DEFAULT 0)"));
}

stock Court_OnLogin(playerid)
{
	gLawyer[playerid] = false;
	gLawyerCases[playerid] = 0;
	gFreeFrom[playerid] = INVALID_PLAYER_ID;
	gFreeAt[playerid] = 0;
	gAppealAt[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT lawyer, lawyer_cases FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		gLawyer[playerid] = DB_GetFieldInt(r, 0) != 0;
		gLawyerCases[playerid] = DB_GetFieldInt(r, 1);
	}
	DB_FreeResultSet(r);
	// Решения суда, пока игрок был не в игре, уже применены к сроку при решении — тут только напоминание
	r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM court_cases WHERE player_id = %d AND status = 0", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0 && DB_GetFieldInt(r, 0) > 0)
			SendClientMessage(playerid, COLOR_YELLOW, "[Суд] Твоя апелляция ждёт решения судьи.");
		DB_FreeResultSet(r);
	}
}

stock Court_OnDisconnect(playerid)
{
	for (new i = 0; i < MAX_PLAYERS; i++) if (gFreeFrom[i] == playerid) gFreeFrom[i] = INVALID_PLAYER_ID;
	gFreeFrom[playerid] = INVALID_PLAYER_ID;
}

stock bool:Court_IsJudge(playerid)
{
	if (Player[playerid][pAdmin] >= COURT_JUDGE_ADMIN) return true;
	new f = Player[playerid][pFaction];
	return Faction_IsGov(f) && Player[playerid][pRank] >= COURT_JUDGE_RANK;
}

static Court_MessageJudges(const text[])
{
	for (new i = 0; i < MAX_PLAYERS; i++)
		if (IsPlayerConnected(i) && Player[i][pLogged] && Court_IsJudge(i)) SendClientMessage(i, COLOR_YELLOW, "%s", text);
}

// ---------------------------------------------------------------------
//  Адвокат
// ---------------------------------------------------------------------
static Court_SaveLawyer(playerid)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET lawyer = %d, lawyer_cases = %d WHERE id = %d",
		gLawyer[playerid] ? 1 : 0, gLawyerCases[playerid], Player[playerid][pDbId]));
}

static Cmd_Lawyer(playerid)
{
	if (gLawyer[playerid])
	{
		ShowPlayerDialog(playerid, DIALOG_LAWYER_INFO, DIALOG_STYLE_MSGBOX, "Адвокат",
			"{FFFFFF}У тебя есть лицензия адвоката.\nВыиграно дел: {FFFF00}%d{FFFFFF}\n\n/free [id] [гонорар] — вытащить заключённого (срок до %d мин.)\nГонорар от $%d до $%d, плюс клиент платит залог штату $%d за минуту срока.\n\nДела больше %d минут решает суд: заключённый пишет /appeal.",
			"Закрыть", "", gLawyerCases[playerid], LAWYER_MAX_TERM, LAWYER_FEE_MIN, LAWYER_FEE_MAX, COURT_BAIL_PER_MIN, LAWYER_MAX_TERM);
		return;
	}
	ShowPlayerDialog(playerid, DIALOG_LAWYER_BUY, DIALOG_STYLE_MSGBOX, "Лицензия адвоката",
		"{FFFFFF}Адвокат вытаскивает заключённых из КПЗ и тюрьмы за гонорар (/free).\n\nЛицензия: {FFFF00}$%d{FFFFFF}, с %d уровня. Купить?",
		"Купить", "Отмена", LAWYER_LICENSE, LAWYER_MIN_LEVEL);
}

static Court_BuyLicense(playerid)
{
	if (gLawyer[playerid]) return;
	if (Player[playerid][pLevel] < LAWYER_MIN_LEVEL) { SendClientMessage(playerid, COLOR_GREY, "Лицензию адвоката дают с %d уровня.", LAWYER_MIN_LEVEL); return; }
	if (Player[playerid][pJail] > 0) { SendClientMessage(playerid, COLOR_GREY, "Из-за решётки лицензию не оформить."); return; }
	if (GetMoney(playerid) < LAWYER_LICENSE) { SendClientMessage(playerid, COLOR_GREY, "Не хватает денег: нужно $%d.", LAWYER_LICENSE); return; }
	GiveMoney(playerid, -LAWYER_LICENSE, "лицензия адвоката");
	Econ_ToTreasury(LAWYER_LICENSE);
	gLawyer[playerid] = true;
	Court_SaveLawyer(playerid);
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Ты получил лицензию адвоката. Помогай заключённым: /free [id] [гонорар]");
}

static Court_Bail(playerid)
{
	return Player[playerid][pJail] * COURT_BAIL_PER_MIN;
}

static Cmd_Free(playerid, const params[])
{
	if (!gLawyer[playerid]) { SendClientMessage(playerid, COLOR_GREY, "Ты не адвокат. Лицензия: /lawyer"); return; }
	if (Player[playerid][pJail] > 0) { SendClientMessage(playerid, COLOR_GREY, "Сначала выберись сам."); return; }
	new pos = 0, fee;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid || !NextIntParam(params, pos, fee))
	{
		SendClientMessage(playerid, COLOR_GREY, "Используй: /free [id] [гонорар $%d-%d]", LAWYER_FEE_MIN, LAWYER_FEE_MAX);
		return;
	}
	if (!Player[target][pLogged] || Player[target][pJail] <= 0) { SendClientMessage(playerid, COLOR_GREY, "Этот человек не в заключении."); return; }
	if (fee < LAWYER_FEE_MIN || fee > LAWYER_FEE_MAX) { SendClientMessage(playerid, COLOR_GREY, "Гонорар от $%d до $%d.", LAWYER_FEE_MIN, LAWYER_FEE_MAX); return; }
	if (Player[target][pJail] > LAWYER_MAX_TERM)
	{
		SendClientMessage(playerid, COLOR_GREY, "Срок больше %d мин. Такое дело решает только суд: пусть подаёт /appeal.", LAWYER_MAX_TERM);
		return;
	}
	if (gettime() - gFreeAt[target] < LAWYER_OFFER_COOLDOWN) { SendClientMessage(playerid, COLOR_GREY, "Этому заключённому недавно уже предлагали защиту. Подожди."); return; }
	gFreeFrom[target] = playerid;
	gFreeFee[target] = fee;
	gFreeAt[target] = gettime();
	new bail = Court_Bail(target);
	ShowPlayerDialog(target, DIALOG_LAWYER_OFFER, DIALOG_STYLE_MSGBOX, "Адвокат",
		"{FFFFFF}Адвокат {33CCFF}%s{FFFFFF} (выиграно дел: %d) предлагает вытащить тебя.\n\nОсталось сидеть: %d мин.\nГонорар адвокату: {FFFF00}$%d{FFFFFF}\nЗалог штату: {FFFF00}$%d{FFFFFF}\nИтого: {FFFF00}$%d\n\n{FFFFFF}Согласен?",
		"Да", "Нет", GetRPName(playerid), gLawyerCases[playerid], Player[target][pJail], fee, bail, fee + bail);
	SendClientMessage(playerid, COLOR_WHITE, "Ты предложил защиту %s: гонорар $%d, залог $%d.", GetRPName(target), fee, bail);
}

static Court_AcceptFree(playerid)
{
	new lawyer = gFreeFrom[playerid], fee = gFreeFee[playerid];
	gFreeFrom[playerid] = INVALID_PLAYER_ID;
	if (lawyer == INVALID_PLAYER_ID || !IsPlayerConnected(lawyer) || !gLawyer[lawyer]) { SendClientMessage(playerid, COLOR_GREY, "Адвокат уже ушёл."); return; }
	if (Player[playerid][pJail] <= 0) return;
	new bail = Court_Bail(playerid);
	if (GetMoney(playerid) < fee + bail) { SendClientMessage(playerid, COLOR_GREY, "Не хватает денег: нужно $%d.", fee + bail); SendClientMessage(lawyer, COLOR_GREY, "%s не хватает денег на защиту.", GetRPName(playerid)); return; }
	GiveMoney(playerid, -(fee + bail), "адвокат и залог");
	GiveMoney(lawyer, fee, "гонорар адвоката");
	Econ_ToTreasury(bail);
	gLawyerCases[lawyer]++;
	Court_SaveLawyer(lawyer);
	Account_Save(lawyer);
	new term = Player[playerid][pJail];
	Jail_Release(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Адвокат %s добился освобождения под залог. Ты на свободе.", GetRPName(lawyer));
	SendClientMessage(lawyer, COLOR_GREEN, "Дело выиграно: %s на свободе. Гонорар $%d. Всего дел: %d.", GetRPName(playerid), fee, gLawyerCases[lawyer]);
	new msg[160];
	format(msg, sizeof msg, "[Диспетчер] Адвокат %s освободил %s под залог (оставалось %d мин.).", GetRPName(lawyer), GetRPName(playerid), term);
	Faction_MessageLaw(msg);
}

// ---------------------------------------------------------------------
//  Суд
// ---------------------------------------------------------------------
static Cmd_Appeal(playerid, const params[])
{
	if (Player[playerid][pJail] <= 0) { SendClientMessage(playerid, COLOR_GREY, "Апелляцию подают из заключения."); return; }
	new text[100];
	strmid(text, params, 0, sizeof text - 1);
	new len = strlen(text);
	if (len < 5) { SendClientMessage(playerid, COLOR_GREY, "Используй: /appeal [почему тебя надо отпустить]"); return; }
	new left = gAppealAt[playerid] + COURT_APPEAL_COOLDOWN * 60 - gettime();
	if (left > 0) { SendClientMessage(playerid, COLOR_GREY, "Новую апелляцию можно подать через %d мин.", (left + 59) / 60); return; }
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM court_cases WHERE player_id = %d AND status = 0", Player[playerid][pDbId]);
	new open = 0;
	if (r != DBResult:0) { if (DB_GetRowCount(r) > 0) open = DB_GetFieldInt(r, 0); DB_FreeResultSet(r); }
	if (open > 0) { SendClientMessage(playerid, COLOR_GREY, "Твоя апелляция уже ждёт решения судьи."); return; }
	gAppealAt[playerid] = gettime();
	new name[MAX_PLAYER_NAME];
	GetPlayerName(playerid, name, sizeof name);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO court_cases (date, player_id, player, term, text) VALUES (%d, %d, '%q', %d, '%q')",
		gettime(), Player[playerid][pDbId], name, Player[playerid][pJail], text));
	SendClientMessage(playerid, COLOR_GREEN, "[Суд] Апелляция подана. Судья рассмотрит её, решение придёт в чат.");
	new msg[200];
	format(msg, sizeof msg, "[Суд] Новая апелляция: %s (срок %d мин.): %s. Рассмотреть: /cases", GetRPName(playerid), Player[playerid][pJail], text);
	Court_MessageJudges(msg);
}

static Cmd_Cases(playerid)
{
	if (!Court_IsJudge(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Дела рассматривают судьи (правительство, ранг %d+).", COURT_JUDGE_RANK); return; }
	new text[2200], line[200], name[MAX_PLAYER_NAME], reason[100], n = 0;
	strcat(text, "Заключённый\tСрок\tТекст\n");
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, player, term, text FROM court_cases WHERE status = 0 ORDER BY id LIMIT %d", COURT_LIST_MAX);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			gCaseList[playerid][n] = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 1, name, sizeof name);
			DB_GetFieldString(r, 3, reason, sizeof reason);
			if (strlen(reason) > 40) { reason[37] = '.'; reason[38] = '.'; reason[39] = '.'; reason[40] = '\0'; }
			format(line, sizeof line, "%s\t%d мин.\t%s\n", name, DB_GetFieldInt(r, 2), reason);
			strcat(text, line);
			n++;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	for (new i = n; i < COURT_LIST_MAX; i++) gCaseList[playerid][i] = 0;
	if (n == 0) { SendClientMessage(playerid, COLOR_GREY, "[Суд] Открытых дел нет."); return; }
	ShowPlayerDialog(playerid, DIALOG_COURT_CASES, DIALOG_STYLE_TABLIST_HEADERS, "Суд: открытые дела", "%s", "Открыть", "Закрыть", text);
}

static Court_ShowCase(playerid, caseid)
{
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT player, term, text, status FROM court_cases WHERE id = %d", caseid);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) == 0 || DB_GetFieldInt(r, 3) != 0) { DB_FreeResultSet(r); SendClientMessage(playerid, COLOR_GREY, "Дело уже закрыто."); return; }
	new name[MAX_PLAYER_NAME], text[100];
	DB_GetFieldString(r, 0, name, sizeof name);
	DB_GetFieldString(r, 2, text, sizeof text);
	new term = DB_GetFieldInt(r, 1);
	DB_FreeResultSet(r);
	gCaseSel[playerid] = caseid;
	new caption[80];
	format(caption, sizeof caption, "Дело №%d: %s, %d мин.", caseid, name, term);
	SendClientMessage(playerid, COLOR_WHITE, "[Суд] Дело №%d, %s (срок %d мин.): «%s»", caseid, name, term, text);
	ShowPlayerDialog(playerid, DIALOG_COURT_VERDICT, DIALOG_STYLE_LIST, caption,
		"Оправдать: выпустить на свободу\nСкостить половину срока\nОставить приговор в силе", "Вынести", "Назад");
}

static Court_PlayerByDb(dbid)
{
	for (new i = 0; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pDbId] == dbid) return i;
	return INVALID_PLAYER_ID;
}

static Court_Verdict(playerid, caseid, kind)
{
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT player_id, player, status FROM court_cases WHERE id = %d", caseid);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) == 0 || DB_GetFieldInt(r, 2) != 0) { DB_FreeResultSet(r); SendClientMessage(playerid, COLOR_GREY, "Дело уже закрыто."); return; }
	new dbid = DB_GetFieldInt(r, 0), name[MAX_PLAYER_NAME];
	DB_GetFieldString(r, 1, name, sizeof name);
	DB_FreeResultSet(r);

	new target = Court_PlayerByDb(dbid), verdict[48], msg[200], judge[MAX_PLAYER_NAME];
	GetPlayerName(playerid, judge, sizeof judge);
	switch (kind)
	{
		case 0:
		{
			strcat(verdict, "оправдан");
			if (target != INVALID_PLAYER_ID) { if (Player[target][pJail] > 0) Jail_Release(target); }
			else DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET jail = 0 WHERE id = %d", dbid));
		}
		case 1:
		{
			strcat(verdict, "срок скошен вдвое");
			if (target != INVALID_PLAYER_ID) { if (Player[target][pJail] > 1) { Player[target][pJail] = (Player[target][pJail] + 1) / 2; Account_Save(target); } }
			else DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET jail = (jail + 1) / 2 WHERE id = %d AND jail > 1", dbid));
		}
		default: strcat(verdict, "приговор оставлен в силе");
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE court_cases SET status = %d, judge = '%q', verdict = '%q', vdate = %d WHERE id = %d",
		kind + 1, judge, verdict, gettime(), caseid));
	format(msg, sizeof msg, "[Суд] Дело №%d (%s): %s. Судья %s.", caseid, name, verdict, GetRPName(playerid));
	Court_MessageJudges(msg);
	Faction_MessageLaw(msg);
	if (target != INVALID_PLAYER_ID)
	{
		SendClientMessage(target, kind == 2 ? COLOR_RED : COLOR_GREEN, "[Суд] Решение по твоей апелляции: %s.", verdict);
		if (kind == 1 && Player[target][pJail] > 0) SendClientMessage(target, COLOR_WHITE, "Осталось сидеть: %d мин.", Player[target][pJail]);
	}
	Admin_Log(playerid, "court", target == INVALID_PLAYER_ID ? playerid : target, verdict, caseid);
}

stock Court_OnDialog(playerid, dialogid, response, listitem)
{
	switch (dialogid)
	{
		case DIALOG_LAWYER_INFO: return 1;
		case DIALOG_LAWYER_BUY:  { if (response) Court_BuyLicense(playerid); return 1; }
		case DIALOG_LAWYER_OFFER:
		{
			if (response) Court_AcceptFree(playerid);
			else
			{
				new l = gFreeFrom[playerid];
				gFreeFrom[playerid] = INVALID_PLAYER_ID;
				if (l != INVALID_PLAYER_ID && IsPlayerConnected(l)) SendClientMessage(l, COLOR_GREY, "%s отказался от защиты.", GetRPName(playerid));
			}
			return 1;
		}
		case DIALOG_COURT_CASES:
		{
			if (!response || listitem < 0 || listitem >= COURT_LIST_MAX || gCaseList[playerid][listitem] == 0) return 1;
			if (!Court_IsJudge(playerid)) return 1;
			Court_ShowCase(playerid, gCaseList[playerid][listitem]);
			return 1;
		}
		case DIALOG_COURT_VERDICT:
		{
			if (!response) { Cmd_Cases(playerid); return 1; }
			if (!Court_IsJudge(playerid) || listitem < 0 || listitem > 2) return 1;
			Court_Verdict(playerid, gCaseSel[playerid], listitem);
			return 1;
		}
	}
	return 0;
}

stock bool:Court_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/lawyer", true))  { Cmd_Lawyer(playerid); return true; }
	if (!strcmp(cmd, "/free", true))    { Cmd_Free(playerid, params); return true; }
	if (!strcmp(cmd, "/appeal", true))  { Cmd_Appeal(playerid, params); return true; }
	if (!strcmp(cmd, "/cases", true))   { Cmd_Cases(playerid); return true; }
	return false;
}
