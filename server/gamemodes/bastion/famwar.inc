// =====================================================================
//  famwar.inc — войны семей по расписанию: /famwar, /famwars, /famtop.
//
//  1. Глава или заместитель семьи вызывает другую семью:
//     /famwar [название семьи] [ставка]. Ставка списывается из общака
//     (/fambank) и ждёт. Ставка от FAMWAR_MIN_STAKE до FAMWAR_MAX_STAKE.
//  2. Глава или заместитель той семьи в течение FAMWAR_OFFER_MIN минут
//     пишет /famwar accept (или /famwar decline). Ставка списывается и у них.
//     Не приняли — ставка возвращается.
//  3. Война идёт в ближайший слот расписания (gFamWarHours, по времени
//     сервера), но не раньше чем через FAMWAR_LEAD_MIN минут. Длится
//     FAMWAR_MINUTES минут. За 10 минут до начала — напоминание семьям.
//  4. Очки: убийство (или ранение) участника вражеской семьи — +1 в любом
//     месте карты. В конце больше очков — победа: общак победителя
//     получает обе ставки минус FAMWAR_TAX% налога. Ничья — ставки
//     возвращаются. Победы и поражения идут в рейтинг (/famtop).
//  У семьи может быть только одна назначенная или идущая война.
//  Перезапуск сервера посреди войны — ничья, ставки возвращаются.
//
//  Хранится: таблица famwars, families.war_wins / war_losses.
// =====================================================================

#define FAMWAR_MAX              20
#define FAMWAR_MIN_STAKE        10000
#define FAMWAR_MAX_STAKE        5000000
#define FAMWAR_OFFER_MIN        5
#define FAMWAR_LEAD_MIN         30
#define FAMWAR_MINUTES          30
#define FAMWAR_TAX              10
#define FAMWAR_MIN_RANK         5       // заместитель (5) и глава (6)

static const gFamWarHours[] = { 18, 20, 22 };

enum { FW_FREE, FW_OFFER, FW_SCHEDULED, FW_ACTIVE }

enum E_FAMWAR
{
	fwState,
	fwDb,               // номер в таблице famwars
	fwA,                // номер семьи в базе (вызвала)
	fwB,                // номер семьи в базе (приняла)
	fwStake,
	fwStart,            // unix-время начала (для FW_OFFER — когда истекает вызов)
	fwScoreA,
	fwScoreB,
	bool:fwWarned
}
static FamWar[FAMWAR_MAX][E_FAMWAR];

stock FamWar_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS famwars (id INTEGER PRIMARY KEY AUTOINCREMENT, a INTEGER, b INTEGER, stake INTEGER, start INTEGER, state INTEGER, score_a INTEGER DEFAULT 0, score_b INTEGER DEFAULT 0, winner INTEGER DEFAULT 0)"));
	DB_AddColumn("families", "war_wins", "INTEGER DEFAULT 0");
	DB_AddColumn("families", "war_losses", "INTEGER DEFAULT 0");
	for (new w = 0; w < FAMWAR_MAX; w++) FamWar[w][fwState] = FW_FREE;

	// Назначенные войны — восстановить; шедшие во время выключения — ничья
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, a, b, stake, start, state FROM famwars WHERE state IN (%d, %d)", FW_SCHEDULED, FW_ACTIVE);
	if (r == DBResult:0) return;
	new n = DB_GetRowCount(r), w = 0;
	for (new i = 0; i < n && w < FAMWAR_MAX; i++)
	{
		FamWar[w][fwDb] = DB_GetFieldInt(r, 0);
		FamWar[w][fwA] = DB_GetFieldInt(r, 1);
		FamWar[w][fwB] = DB_GetFieldInt(r, 2);
		FamWar[w][fwStake] = DB_GetFieldInt(r, 3);
		FamWar[w][fwStart] = DB_GetFieldInt(r, 4);
		FamWar[w][fwScoreA] = 0;
		FamWar[w][fwScoreB] = 0;
		FamWar[w][fwWarned] = false;
		FamWar[w][fwState] = DB_GetFieldInt(r, 5);
		DB_SelectNextRow(r);
		w++;
	}
	DB_FreeResultSet(r);
	for (new i = 0; i < FAMWAR_MAX; i++)
	{
		if (FamWar[i][fwState] == FW_ACTIVE) FamWar_Finish(i, true);
		// семья распалась, пока сервер был выключен
		else if (FamWar[i][fwState] == FW_SCHEDULED && (Family_SlotByDb(FamWar[i][fwA]) == -1 || Family_SlotByDb(FamWar[i][fwB]) == -1)) FamWar_Finish(i, true);
	}
}

static FamWar_Name(dbid)
{
	new name[32], s = Family_SlotByDb(dbid);
	if (s == -1) strcat(name, "(распалась)");
	else strcat(name, Family_NameOf(s));
	return name;
}

static FamWar_Msg(dbid, const text[])
{
	new s = Family_SlotByDb(dbid);
	if (s != -1) Family_Msg(s, text);
}

static FamWar_Refund(dbid, amount)
{
	new s = Family_SlotByDb(dbid);
	if (s != -1 && amount > 0) Family_BankAdd(s, amount);
}

// Война, в которой участвует семья (кроме FW_FREE), -1 — нет
static FamWar_Of(dbid)
{
	if (dbid <= 0) return -1;
	for (new w = 0; w < FAMWAR_MAX; w++)
		if (FamWar[w][fwState] != FW_FREE && (FamWar[w][fwA] == dbid || FamWar[w][fwB] == dbid)) return w;
	return -1;
}

static FamWar_FreeSlot()
{
	for (new w = 0; w < FAMWAR_MAX; w++) if (FamWar[w][fwState] == FW_FREE) return w;
	return -1;
}

// Ближайший слот расписания не раньше, чем через FAMWAR_LEAD_MIN минут (unix-время)
static FamWar_NextSlot()
{
	new h, m, sec;
	gettime(h, m, sec);
	new now = gettime(), dayStart = now - (h * 3600 + m * 60 + sec);
	for (new d = 0; d < 2; d++)
	{
		for (new i = 0; i < sizeof gFamWarHours; i++)
		{
			new t = dayStart + d * 86400 + gFamWarHours[i] * 3600;
			if (t - now >= FAMWAR_LEAD_MIN * 60) return t;
		}
	}
	return dayStart + 2 * 86400 + gFamWarHours[0] * 3600;
}

// "сегодня в 20:00" / "завтра в 18:00"
static FamWar_When(dest[], size, t)
{
	new h, m, sec;
	gettime(h, m, sec);
	new now = gettime(), dayStart = now - (h * 3600 + m * 60 + sec);
	new diff = t - dayStart, day = diff / 86400, hh = (diff % 86400) / 3600, mm = (diff % 3600) / 60;
	format(dest, size, "%s в %02d:%02d", day == 0 ? "сегодня" : (day == 1 ? "завтра" : "послезавтра"), hh, mm);
}

static FamWar_SaveState(w)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE famwars SET state = %d, start = %d, score_a = %d, score_b = %d WHERE id = %d",
		FamWar[w][fwState], FamWar[w][fwStart], FamWar[w][fwScoreA], FamWar[w][fwScoreB], FamWar[w][fwDb]));
}

// Конец войны. draw — ничья принудительно (рестарт, распад семьи)
static FamWar_Finish(w, bool:draw = false)
{
	new a = FamWar[w][fwA], b = FamWar[w][fwB], stake = FamWar[w][fwStake], winner = 0, msg[200];
	if (!draw && FamWar[w][fwState] == FW_ACTIVE && FamWar[w][fwScoreA] != FamWar[w][fwScoreB])
		winner = (FamWar[w][fwScoreA] > FamWar[w][fwScoreB]) ? a : b;
	if (winner)
	{
		new loser = (winner == a) ? b : a, prize = stake * 2 - stake * 2 * FAMWAR_TAX / 100;
		FamWar_Refund(winner, prize);
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE families SET war_wins = war_wins + 1 WHERE id = %d", winner));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE families SET war_losses = war_losses + 1 WHERE id = %d", loser));
		format(msg, sizeof msg, "[Война семей] %s победила %s со счётом %d:%d! В общак победителя: $%d.",
			FamWar_Name(winner), FamWar_Name(loser), (winner == a) ? FamWar[w][fwScoreA] : FamWar[w][fwScoreB], (winner == a) ? FamWar[w][fwScoreB] : FamWar[w][fwScoreA], prize);
		SendClientMessageToAll(COLOR_ORANGE, "%s", msg);
	}
	else
	{
		if (FamWar[w][fwState] != FW_OFFER) FamWar_Refund(b, stake);
		FamWar_Refund(a, stake);
		if (FamWar[w][fwState] == FW_ACTIVE)
		{
			format(msg, sizeof msg, "[Война семей] %s и %s: ничья %d:%d. Ставки вернулись в общаки.", FamWar_Name(a), FamWar_Name(b), FamWar[w][fwScoreA], FamWar[w][fwScoreB]);
			FamWar_Msg(a, msg); FamWar_Msg(b, msg);
		}
	}
	if (FamWar[w][fwDb])
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE famwars SET state = 9, score_a = %d, score_b = %d, winner = %d WHERE id = %d",
			FamWar[w][fwScoreA], FamWar[w][fwScoreB], winner, FamWar[w][fwDb]));
	FamWar[w][fwState] = FW_FREE;
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
static bool:FamWar_CanManage(playerid)
{
	if (Family_Slot(playerid) == -1) { SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь в семье."); return false; }
	if (Family_Rank(playerid) < FAMWAR_MIN_RANK) { SendClientMessage(playerid, COLOR_GREY, "Войны объявляет глава или заместитель семьи."); return false; }
	return true;
}

static FamWar_Challenge(playerid, const params[])
{
	if (!FamWar_CanManage(playerid)) return;
	// последнее слово — ставка, всё до него — название семьи
	new len = strlen(params);
	while (len > 0 && params[len - 1] == ' ') len--;
	new sp = len - 1;
	while (sp >= 0 && params[sp] != ' ') sp--;
	if (sp <= 0) { SendClientMessage(playerid, COLOR_GREY, "Используй: /famwar [название семьи] [ставка]. Принять вызов: /famwar accept"); return; }
	new name[32], num[16];
	strmid(name, params, 0, sp);
	strmid(num, params, sp + 1, len);
	if (!IsNumeric(num)) { SendClientMessage(playerid, COLOR_GREY, "Используй: /famwar [название семьи] [ставка]"); return; }
	new stake = strval(num);
	if (stake < FAMWAR_MIN_STAKE || stake > FAMWAR_MAX_STAKE) { SendClientMessage(playerid, COLOR_GREY, "Ставка от $%d до $%d.", FAMWAR_MIN_STAKE, FAMWAR_MAX_STAKE); return; }
	new my = Family_Slot(playerid), other = Family_FindByName(name);
	if (other == -1) { SendClientMessage(playerid, COLOR_GREY, "Семья «%s» не найдена. Список: /famtop", name); return; }
	if (other == my) { SendClientMessage(playerid, COLOR_GREY, "Нельзя воевать со своей семьёй."); return; }
	new myDb = Family_DbOf(my), otherDb = Family_DbOf(other);
	if (FamWar_Of(myDb) != -1) { SendClientMessage(playerid, COLOR_GREY, "У твоей семьи уже есть вызов или война: /famwars"); return; }
	if (FamWar_Of(otherDb) != -1) { SendClientMessage(playerid, COLOR_GREY, "У этой семьи уже есть вызов или война."); return; }
	if (Family_BankOf(my) < stake) { SendClientMessage(playerid, COLOR_GREY, "В общаке не хватает: $%d из $%d (/fambank).", Family_BankOf(my), stake); return; }
	// кто-то из руководства той семьи должен быть в игре, чтобы принять
	new bool:boss = false;
	for (new i = 0; i < MAX_PLAYERS; i++)
		if (IsPlayerConnected(i) && Player[i][pLogged] && Family_Slot(i) == other && Family_Rank(i) >= FAMWAR_MIN_RANK) { boss = true; break; }
	if (!boss) { SendClientMessage(playerid, COLOR_GREY, "Сейчас в игре нет главы или заместителя этой семьи."); return; }
	new w = FamWar_FreeSlot();
	if (w == -1) { SendClientMessage(playerid, COLOR_GREY, "Слишком много войн сразу. Попробуй позже."); return; }

	Family_BankAdd(my, -stake);
	FamWar[w][fwState] = FW_OFFER;
	FamWar[w][fwDb] = 0;
	FamWar[w][fwA] = myDb;
	FamWar[w][fwB] = otherDb;
	FamWar[w][fwStake] = stake;
	FamWar[w][fwStart] = gettime() + FAMWAR_OFFER_MIN * 60;
	FamWar[w][fwScoreA] = 0;
	FamWar[w][fwScoreB] = 0;
	FamWar[w][fwWarned] = false;
	new msg[200], when[32];
	FamWar_When(when, sizeof when, FamWar_NextSlot());
	format(msg, sizeof msg, "[Война семей] %s вызывает вашу семью на войну! Ставка $%d, начало %s. Глава или заместитель: /famwar accept или /famwar decline (%d мин.)",
		Family_NameOf(my), stake, when, FAMWAR_OFFER_MIN);
	Family_Msg(other, msg);
	format(msg, sizeof msg, "[Война семей] %s вызвал семью %s на войну. Ставка $%d снята из общака и ждёт ответа.", GetRPName(playerid), Family_NameOf(other), stake);
	Family_Msg(my, msg);
}

static FamWar_Answer(playerid, bool:accept)
{
	if (!FamWar_CanManage(playerid)) return;
	new myDb = Family_DbOf(Family_Slot(playerid)), w = -1;
	for (new i = 0; i < FAMWAR_MAX; i++) if (FamWar[i][fwState] == FW_OFFER && FamWar[i][fwB] == myDb) { w = i; break; }
	if (w == -1) { SendClientMessage(playerid, COLOR_GREY, "Вашей семье никто не бросал вызов."); return; }
	new msg[200], my = Family_Slot(playerid);
	if (!accept)
	{
		format(msg, sizeof msg, "[Война семей] %s отказалась от войны. Ставка вернулась в общак.", Family_NameOf(my));
		FamWar_Msg(FamWar[w][fwA], msg);
		format(msg, sizeof msg, "[Война семей] %s отклонил вызов семьи %s.", GetRPName(playerid), FamWar_Name(FamWar[w][fwA]));
		Family_Msg(my, msg);
		FamWar_Finish(w, true);
		return;
	}
	if (Family_BankOf(my) < FamWar[w][fwStake]) { SendClientMessage(playerid, COLOR_GREY, "В общаке не хватает на ставку: нужно $%d (/fambank).", FamWar[w][fwStake]); return; }
	if (Family_SlotByDb(FamWar[w][fwA]) == -1) { FamWar_Finish(w, true); SendClientMessage(playerid, COLOR_GREY, "Та семья уже распалась."); return; }
	Family_BankAdd(my, -FamWar[w][fwStake]);
	FamWar[w][fwState] = FW_SCHEDULED;
	FamWar[w][fwStart] = FamWar_NextSlot();
	new DBResult:r = DB_ExecuteQuery(gDB, "INSERT INTO famwars (a, b, stake, start, state) VALUES (%d, %d, %d, %d, %d)",
		FamWar[w][fwA], FamWar[w][fwB], FamWar[w][fwStake], FamWar[w][fwStart], FW_SCHEDULED);
	DB_FreeResultSet(r);
	r = DB_ExecuteQuery(gDB, "SELECT last_insert_rowid()");
	if (r != DBResult:0) { if (DB_GetRowCount(r) > 0) FamWar[w][fwDb] = DB_GetFieldInt(r, 0); DB_FreeResultSet(r); }
	new when[32];
	FamWar_When(when, sizeof when, FamWar[w][fwStart]);
	format(msg, sizeof msg, "[Война семей] %s против %s: %s, %d минут. Ставка $%d с каждой стороны. Счёт: /famwars",
		FamWar_Name(FamWar[w][fwA]), FamWar_Name(FamWar[w][fwB]), when, FAMWAR_MINUTES, FamWar[w][fwStake]);
	SendClientMessageToAll(COLOR_ORANGE, "%s", msg);
}

static FamWar_List(playerid)
{
	new text[1600], line[160], when[32], n = 0;
	strcat(text, "Семьи\tСчёт\tСтавка\tКогда\n");
	for (new w = 0; w < FAMWAR_MAX; w++)
	{
		if (FamWar[w][fwState] != FW_SCHEDULED && FamWar[w][fwState] != FW_ACTIVE) continue;
		if (FamWar[w][fwState] == FW_ACTIVE) format(when, sizeof when, "{FF6347}идёт, ещё %d мин.", (FamWar[w][fwStart] + FAMWAR_MINUTES * 60 - gettime() + 59) / 60);
		else FamWar_When(when, sizeof when, FamWar[w][fwStart]);
		format(line, sizeof line, "%s — %s\t%d:%d\t$%d\t%s\n", FamWar_Name(FamWar[w][fwA]), FamWar_Name(FamWar[w][fwB]),
			FamWar[w][fwScoreA], FamWar[w][fwScoreB], FamWar[w][fwStake], when);
		strcat(text, line);
		n++;
	}
	if (n == 0)
	{
		new h1[64];
		format(h1, sizeof h1, "Войн нет\t\t\tчасы войн:");
		for (new i = 0; i < sizeof gFamWarHours; i++) { format(line, sizeof line, " %02d:00", gFamWarHours[i]); strcat(h1, line); }
		strcat(text, h1);
	}
	ShowPlayerDialog(playerid, DIALOG_FAMWARS, DIALOG_STYLE_TABLIST_HEADERS, "Войны семей", "%s", "Закрыть", "", text);
}

static FamWar_Top(playerid)
{
	new text[1800], line[120], name[32], n = 0;
	strcat(text, "#\tСемья\tПобеды\tПоражения\n");
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT name, war_wins, war_losses FROM families ORDER BY war_wins DESC, war_losses ASC, id ASC LIMIT 15");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			DB_GetFieldString(r, 0, name, sizeof name);
			format(line, sizeof line, "%d\t%s\t{33AA33}%d\t{FF6347}%d\n", i + 1, name, DB_GetFieldInt(r, 1), DB_GetFieldInt(r, 2));
			strcat(text, line);
			n++;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	if (n == 0) strcat(text, "-\tСемей пока нет\t\t");
	ShowPlayerDialog(playerid, DIALOG_FAMWARS, DIALOG_STYLE_TABLIST_HEADERS, "Рейтинг семей", "%s", "Закрыть", "", text);
}

stock bool:FamWar_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/famwar", true))
	{
		if (!strcmp(params, "accept", true) || !strcmp(params, "да", true)) FamWar_Answer(playerid, true);
		else if (!strcmp(params, "decline", true) || !strcmp(params, "нет", true)) FamWar_Answer(playerid, false);
		else FamWar_Challenge(playerid, params);
		return true;
	}
	if (!strcmp(cmd, "/famwars", true)) { FamWar_List(playerid); return true; }
	if (!strcmp(cmd, "/famtop", true))  { FamWar_Top(playerid); return true; }
	return false;
}

// ---------------------------------------------------------------------
//  Таймер раз в минуту и убийства
// ---------------------------------------------------------------------
stock FamWar_OnMinute()
{
	new now = gettime(), msg[200];
	for (new w = 0; w < FAMWAR_MAX; w++)
	{
		switch (FamWar[w][fwState])
		{
			case FW_OFFER:
			{
				if (now < FamWar[w][fwStart]) continue;
				format(msg, sizeof msg, "[Война семей] %s не ответила на вызов. Ставка вернулась в общак.", FamWar_Name(FamWar[w][fwB]));
				FamWar_Msg(FamWar[w][fwA], msg);
				FamWar_Finish(w, true);
			}
			case FW_SCHEDULED:
			{
				if (Family_SlotByDb(FamWar[w][fwA]) == -1 || Family_SlotByDb(FamWar[w][fwB]) == -1) { FamWar_Finish(w, true); continue; }
				if (!FamWar[w][fwWarned] && FamWar[w][fwStart] - now <= 600)
				{
					FamWar[w][fwWarned] = true;
					format(msg, sizeof msg, "[Война семей] Через 10 минут война: %s против %s. Собирайтесь!", FamWar_Name(FamWar[w][fwA]), FamWar_Name(FamWar[w][fwB]));
					FamWar_Msg(FamWar[w][fwA], msg); FamWar_Msg(FamWar[w][fwB], msg);
				}
				if (now < FamWar[w][fwStart]) continue;
				FamWar[w][fwState] = FW_ACTIVE;
				FamWar[w][fwStart] = now;
				FamWar_SaveState(w);
				format(msg, sizeof msg, "[Война семей] Началась война: %s против %s! %d минут, очко за каждого убитого врага. Счёт: /famwars",
					FamWar_Name(FamWar[w][fwA]), FamWar_Name(FamWar[w][fwB]), FAMWAR_MINUTES);
				SendClientMessageToAll(COLOR_ORANGE, "%s", msg);
			}
			case FW_ACTIVE:
			{
				if (now >= FamWar[w][fwStart] + FAMWAR_MINUTES * 60) FamWar_Finish(w);
			}
		}
	}
}

// Из OnPlayerDeath и ранения (rp.pwn), рядом с Turfs_OnDeath
stock FamWar_OnKill(playerid, killerid)
{
	if (killerid == INVALID_PLAYER_ID || killerid == playerid || !IsPlayerConnected(killerid)) return;
	new ks = Family_Slot(killerid), vs = Family_Slot(playerid);
	if (ks == -1 || vs == -1 || ks == vs) return;
	new kdb = Family_DbOf(ks), vdb = Family_DbOf(vs);
	for (new w = 0; w < FAMWAR_MAX; w++)
	{
		if (FamWar[w][fwState] != FW_ACTIVE) continue;
		if (FamWar[w][fwA] == kdb && FamWar[w][fwB] == vdb) FamWar[w][fwScoreA]++;
		else if (FamWar[w][fwB] == kdb && FamWar[w][fwA] == vdb) FamWar[w][fwScoreB]++;
		else continue;
		FamWar_SaveState(w);
		new msg[160];
		format(msg, sizeof msg, "[Война] %s убил %s. Счёт: %s %d — %d %s", GetRPName(killerid), GetRPName(playerid),
			FamWar_Name(FamWar[w][fwA]), FamWar[w][fwScoreA], FamWar[w][fwScoreB], FamWar_Name(FamWar[w][fwB]));
		FamWar_Msg(FamWar[w][fwA], msg); FamWar_Msg(FamWar[w][fwB], msg);
		GameTextForPlayer(killerid, "~r~+1", 1500, 4);
		return;
	}
}
