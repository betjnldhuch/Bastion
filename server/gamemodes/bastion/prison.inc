// =====================================================================
//  prison.inc — федеральная тюрьма и её охрана (организация типа «тюрьма»).
//
//  Как запустить:
//    1. /createfaction 6 [название] — организация охраны, лидер: /makeleader.
//       Вход для охраны: /setfint (как у любой организации), раздевалка /setfduty.
//    2. Точки (админ 4+, places.inc): /place cell — камеры (можно несколько),
//       /place pwork — места работ заключённых, /place escape — лаз для побега,
//       /place release — где выпускают на свободу, /place pguard — пост охраны.
//  Пока нет ни одной камеры, все сидят в камере LSPD, как раньше.
//
//  Срок от PRISON_MIN_TERM минут — заключённого везут в тюрьму, меньше — КПЗ LSPD.
//  Заключённый: /pwork у места работ — работа, каждые PRISON_WORK_STEPS заданий -1 минута срока.
//    /escape у лаза — побег: PRISON_ESCAPE_SEC секунд копать, охрану и полицию оповещают.
//  Охрана (на смене): /prisoners — список, /pextend [id] [мин] [причина] — добавить срок (ранг 2+),
//    /preduce [id] [мин] — скостить (ранг 7+), /cuff /uncuff /frisk /seize — как у полиции.
// =====================================================================

#define PRISON_MIN_TERM         20      // с какого срока сажают в тюрьму, а не в КПЗ
#define PRISON_RANGE            120.0   // дальше от камер и работ — значит сбежал без /escape: вернуть
#define PRISON_WORK_STEPS       3
#define PRISON_ESCAPE_SEC       90
#define PRISON_ESCAPE_FAIL_ADD  15      // минут сверху за неудачный побег
#define PRISON_ESCAPE_COOLDOWN  1800
#define PRISON_EXTEND_MAX       30

static bool:gInmate[MAX_PLAYERS];
static gPWorkStep[MAX_PLAYERS];          // 0 — не работает, иначе сколько сделано + 1
static gPWorkPlace[MAX_PLAYERS] = { -1, ... };
static gPWorkBusy[MAX_PLAYERS];
static gEscapeLeft[MAX_PLAYERS];
static gEscapeLast[MAX_PLAYERS];

stock Prison_Init()
{
	DB_AddColumn("accounts", "prison", "INTEGER DEFAULT 0");
	SetTimer("Prison_OnSecond", 1000, true);
}

stock Prison_LoadPlayer(playerid)
{
	gInmate[playerid] = false;
	gPWorkStep[playerid] = 0;
	gEscapeLeft[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT prison FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0) gInmate[playerid] = DB_GetFieldInt(r, 0) != 0;
	DB_FreeResultSet(r);
	if (Player[playerid][pJail] <= 0) gInmate[playerid] = false;
}

static Prison_SaveFlag(playerid)
{
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET prison = %d WHERE id = %d", gInmate[playerid] ? 1 : 0, Player[playerid][pDbId]));
}

stock bool:Prison_Ready() { return Place_First(PL_CELL) != -1; }
stock bool:Prison_IsInmate(playerid) { return gInmate[playerid]; }

stock bool:Faction_IsPrison(f)
{
	return (f > FACTION_NONE && f < MAX_FACTIONS && Faction[f][fType] == FTYPE_PRISON);
}

stock bool:Prison_IsGuardOnDuty(playerid)
{
	return Faction_IsPrison(Player[playerid][pFaction]) && Player[playerid][pDuty];
}

static Prison_MessageGuards(const msg[])
{
	for (new i = 0; i < MAX_PLAYERS; i++)
		if (IsPlayerConnected(i) && Player[i][pLogged] && Faction_IsPrison(Player[i][pFaction])) SendClientMessage(i, 0xD2691EFF, "%s", msg);
}

// ---------------------------------------------------------------------
//  Посадка и освобождение (из Jail_PutInCell / Jail_Release, factions.inc)
// ---------------------------------------------------------------------
// true — посадили в тюрьму (а не в КПЗ)
stock bool:Prison_TryPutInCell(playerid)
{
	if (!Prison_Ready()) { gInmate[playerid] = false; return false; }
	if (!gInmate[playerid] && Player[playerid][pJail] < PRISON_MIN_TERM) return false;
	new first = !gInmate[playerid];
	gInmate[playerid] = true;
	Prison_SaveFlag(playerid);
	Prison_StopWork(playerid);
	new c = Place_Random(PL_CELL);
	Place_Teleport(playerid, c);
	ResetPlayerWeapons(playerid);
	Player[playerid][pHouseInside] = -1;
	if (first)
	{
		SendClientMessage(playerid, 0xD2691EFF, "Тебя этапировали в федеральную тюрьму. Работа: /pwork (скостить срок), побег: /escape у лаза.");
		new msg[144];
		format(msg, sizeof msg, "[Тюрьма] Поступил заключённый %s [%d], срок %d мин.", GetRPName(playerid), playerid, Player[playerid][pJail]);
		Prison_MessageGuards(msg);
	}
	return true;
}

// true — выпустили через ворота тюрьмы
stock bool:Prison_TryRelease(playerid)
{
	if (!gInmate[playerid]) return false;
	gInmate[playerid] = false;
	Prison_SaveFlag(playerid);
	Prison_StopWork(playerid);
	gEscapeLeft[playerid] = 0;
	new r = Place_First(PL_RELEASE);
	if (r == -1) return false;          // выхода нет — выпустят у LSPD, как раньше
	Place_Teleport(playerid, r);
	SendClientMessage(playerid, COLOR_GREEN, "Срок закончился. Ворота тюрьмы открываются: ты на свободе.");
	Account_Save(playerid);
	return true;
}

// Раз в минуту для каждого заключённого (из Factions_OnMinute). true — заключённый тюрьмы
stock bool:Prison_OnMinute(playerid)
{
	if (!gInmate[playerid]) return false;
	if (!Prison_Ready()) { gInmate[playerid] = false; Prison_SaveFlag(playerid); return false; }
	if (gEscapeLeft[playerid] > 0) return true;
	// ушёл далеко от камер и работ (или в чужой интерьер) — вернуть в камеру
	new bool:inside = false;
	for (new i = 0; i < MAX_PLACES && !inside; i++)
	{
		if (!gPlace[i][plUsed] || (gPlace[i][plKind] != PL_CELL && gPlace[i][plKind] != PL_PWORK && gPlace[i][plKind] != PL_ESCAPE)) continue;
		if (GetPlayerInterior(playerid) == gPlace[i][plInt] && GetPlayerVirtualWorld(playerid) == gPlace[i][plWorld]
			&& IsPlayerInRangeOfPoint(playerid, PRISON_RANGE, gPlace[i][plX], gPlace[i][plY], gPlace[i][plZ])) inside = true;
	}
	if (!inside) Prison_TryPutInCell(playerid);
	return true;
}

// ---------------------------------------------------------------------
//  Работа заключённых: /pwork
// ---------------------------------------------------------------------
static Prison_StopWork(playerid)
{
	if (gPWorkStep[playerid] == 0) return;
	gPWorkStep[playerid] = 0;
	gPWorkBusy[playerid] = 0;
	if (IsPlayerConnected(playerid))
	{
		DisablePlayerCheckpoint(playerid);
		TogglePlayerControllable(playerid, true);
	}
}

static Prison_NextWork(playerid)
{
	gPWorkPlace[playerid] = Place_Random(PL_PWORK, gPWorkPlace[playerid]);
	new i = gPWorkPlace[playerid];
	DisablePlayerCheckpoint(playerid);
	SetPlayerCheckpoint(playerid, gPlace[i][plX], gPlace[i][plY], gPlace[i][plZ], 1.5);
}

stock bool:Cmd_PWork(playerid)
{
	if (!gInmate[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Работают только заключённые тюрьмы.");
	if (gPWorkStep[playerid] != 0)
	{
		Prison_StopWork(playerid);
		return SendClientMessage(playerid, COLOR_WHITE, "Ты закончил работу.");
	}
	if (Place_First(PL_PWORK) == -1) return SendClientMessage(playerid, COLOR_GREY, "В этой тюрьме работы нет (админ: /place pwork).");
	if (Player[playerid][pJail] <= 1) return SendClientMessage(playerid, COLOR_GREY, "Тебе и так скоро на свободу.");
	gPWorkStep[playerid] = 1;
	gPWorkPlace[playerid] = -1;
	Prison_NextWork(playerid);
	return SendClientMessage(playerid, COLOR_GREEN, "Работа: иди к маркеру и сделай дело. Каждые %d заданий — минус минута срока. Закончить: /pwork", PRISON_WORK_STEPS);
}

// Из OnPlayerEnterCheckpoint. true — точка работы заключённого
stock bool:Prison_OnEnterCheckpoint(playerid)
{
	if (gPWorkStep[playerid] == 0) return false;
	if (gPWorkBusy[playerid] > 0) return true;
	DisablePlayerCheckpoint(playerid);
	gPWorkBusy[playerid] = 4;
	TogglePlayerControllable(playerid, false);
	switch (random(3))
	{
		case 0: ApplyAnimation(playerid, "BOMBER", "BOM_Plant_Loop", 4.1, true, false, false, false, 0);
		case 1: ApplyAnimation(playerid, "CASINO", "cards_loop", 4.1, true, false, false, false, 0);
		default: ApplyAnimation(playerid, "GRAFFITI", "spraycan_fire", 4.1, true, false, false, false, 0);
	}
	return true;
}

static Prison_WorkDone(playerid)
{
	TogglePlayerControllable(playerid, true);
	ClearAnimations(playerid);
	gPWorkStep[playerid]++;
	if ((gPWorkStep[playerid] - 1) % PRISON_WORK_STEPS == 0 && Player[playerid][pJail] > 1)
	{
		Player[playerid][pJail]--;
		Account_Save(playerid);
		SendClientMessage(playerid, COLOR_GREEN, "Хорошо работаешь: срок -1 мин. Осталось %d мин.", Player[playerid][pJail]);
	}
	if (Player[playerid][pJail] <= 1) { Prison_StopWork(playerid); return; }
	Prison_NextWork(playerid);
}

// ---------------------------------------------------------------------
//  Побег: /escape у лаза
// ---------------------------------------------------------------------
stock bool:Cmd_Escape(playerid)
{
	if (!gInmate[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Ты не в тюрьме.");
	if (gEscapeLeft[playerid] > 0) return SendClientMessage(playerid, COLOR_GREY, "Ты уже копаешь.");
	if (Place_Nearby(playerid, PL_ESCAPE, 3.0) == -1) return SendClientMessage(playerid, COLOR_GREY, "Здесь не выбраться. Нужен лаз.");
	new now = gettime();
	if (now - gEscapeLast[playerid] < PRISON_ESCAPE_COOLDOWN) return SendClientMessage(playerid, COLOR_GREY, "Охрана ещё начеку после прошлой попытки. Подожди %d мин.", (PRISON_ESCAPE_COOLDOWN - (now - gEscapeLast[playerid])) / 60 + 1);
	gEscapeLast[playerid] = now;
	Prison_StopWork(playerid);
	gEscapeLeft[playerid] = PRISON_ESCAPE_SEC;
	ApplyAnimation(playerid, "BOMBER", "BOM_Plant_Loop", 4.1, true, false, false, false, 0);
	SendClientMessage(playerid, COLOR_RED, "Ты начал копать лаз. Не уходи %d сек. Если охрана заметит — к сроку добавят.", PRISON_ESCAPE_SEC);
	new msg[144];
	format(msg, sizeof msg, "[Тюрьма] Тревога! %s [%d] пытается сбежать через лаз.", GetRPName(playerid), playerid);
	Prison_MessageGuards(msg);
	return true;
}

static Prison_EscapeFail(playerid, const why[])
{
	gEscapeLeft[playerid] = 0;
	ClearAnimations(playerid);
	Player[playerid][pJail] += PRISON_ESCAPE_FAIL_ADD;
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_RED, "Побег сорван (%s). К сроку +%d мин., всего %d мин.", why, PRISON_ESCAPE_FAIL_ADD, Player[playerid][pJail]);
	new msg[144];
	format(msg, sizeof msg, "[Тюрьма] Побег %s сорван (%s).", GetRPName(playerid), why);
	Prison_MessageGuards(msg);
}

static Prison_EscapeTick(playerid)
{
	if (Player[playerid][pCuffed]) { Prison_EscapeFail(playerid, "надели наручники"); return; }
	if (Wound_IsWounded(playerid)) { Prison_EscapeFail(playerid, "ранен"); return; }
	if (Place_Nearby(playerid, PL_ESCAPE, 4.0) == -1) { Prison_EscapeFail(playerid, "отошёл от лаза"); return; }
	if (--gEscapeLeft[playerid] > 0)
	{
		if (gEscapeLeft[playerid] % 15 == 0) GameTextForPlayer(playerid, "~y~digging...", 2000, 4);
		return;
	}
	ClearAnimations(playerid);
	gInmate[playerid] = false;
	Prison_SaveFlag(playerid);
	Player[playerid][pJail] = 0;
	Player[playerid][pWanted] = 6;
	SetPlayerWantedLevel(playerid, 6);
	new r = Place_First(PL_RELEASE);
	if (r != -1)
	{
		Place_Teleport(playerid, r);
		new Float:x, Float:y, Float:z;
		GetPlayerPos(playerid, x, y, z);
		SetPlayerPos(playerid, x + 15.0 - float(random(30)), y + 15.0 - float(random(30)), z + 0.5);
	}
	else Jail_Release(playerid);
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_RED, "Ты выбрался! Теперь ты в розыске (6 зв.). Беги.");
	new msg[144];
	format(msg, sizeof msg, "[Тюрьма] ПОБЕГ! %s [%d] выбрался наружу. Объявлен в розыск: 6 зв.", GetRPName(playerid), playerid);
	Prison_MessageGuards(msg);
	Faction_MessageLaw(msg);
}

forward Prison_OnSecond();
public Prison_OnSecond()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !gInmate[i]) continue;
		if (gEscapeLeft[i] > 0) Prison_EscapeTick(i);
		if (gPWorkBusy[i] > 0 && --gPWorkBusy[i] == 0) Prison_WorkDone(i);
	}
}

// ---------------------------------------------------------------------
//  Охрана
// ---------------------------------------------------------------------
static bool:Prison_NeedGuard(playerid, rank = 1)
{
	if (!Faction_IsPrison(Player[playerid][pFaction])) { SendClientMessage(playerid, COLOR_GREY, "Команда для охраны тюрьмы."); return false; }
	if (!Player[playerid][pDuty]) { SendClientMessage(playerid, COLOR_GREY, "Сначала заступи на смену."); return false; }
	if (Player[playerid][pRank] < rank) { SendClientMessage(playerid, COLOR_GREY, "Нужен ранг %d или выше.", rank); return false; }
	return true;
}

stock bool:Cmd_Prisoners(playerid)
{
	if (!Faction_IsPrison(Player[playerid][pFaction]) && Player[playerid][pAdmin] < 2 && !Faction_IsLaw(Player[playerid][pFaction]))
		return SendClientMessage(playerid, COLOR_GREY, "Команда для охраны тюрьмы и полиции.");
	new list[1536] = "Заключённый\tСрок\tСейчас\n", line[96], n = 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || Player[i][pJail] <= 0) continue;
		new what[24];
		if (gEscapeLeft[i] > 0) strcat(what, "{FF4040}копает лаз");
		else if (gPWorkStep[i] != 0) strcat(what, "{33AA33}работает");
		else if (gInmate[i]) strcat(what, "в тюрьме");
		else strcat(what, "в КПЗ LSPD");
		format(line, sizeof line, "%s [%d]\t%d мин.\t%s\n", GetRPName(i), i, Player[i][pJail], what);
		strcat(list, line);
		n++;
	}
	if (n == 0) return SendClientMessage(playerid, COLOR_GREY, "Заключённых онлайн нет.");
	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_TABLIST_HEADERS, "Заключённые", "%s", "Закрыть", "", list);
	return true;
}

stock bool:Cmd_PExtend(playerid, const params[])
{
	if (!Prison_NeedGuard(playerid, 2)) return true;
	new pos = 0, mins, reason[64];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, mins) || !RestParam(params, pos, reason))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /pextend [id] [минуты 1-%d] [причина]", PRISON_EXTEND_MAX);
	if (mins < 1 || mins > PRISON_EXTEND_MAX) return SendClientMessage(playerid, COLOR_GREY, "От 1 до %d минут.", PRISON_EXTEND_MAX);
	if (!gInmate[target]) return SendClientMessage(playerid, COLOR_GREY, "Он не заключённый тюрьмы.");
	Player[target][pJail] += mins;
	Account_Save(target);
	SendClientMessage(target, COLOR_RED, "Охранник %s добавил тебе %d мин. срока. Причина: %s", GetRPName(playerid), mins, reason);
	new msg[144];
	format(msg, sizeof msg, "[Тюрьма] %s добавил %s %d мин. (%s). Теперь %d мин.", GetRPName(playerid), GetRPName(target), mins, reason, Player[target][pJail]);
	Prison_MessageGuards(msg);
	Faction_Log(Player[playerid][pFaction], msg[9]);
	return true;
}

stock bool:Cmd_PReduce(playerid, const params[])
{
	if (!Prison_NeedGuard(playerid, 7)) return true;
	new pos = 0, mins;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, mins) || mins < 1)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /preduce [id] [минуты]");
	if (!gInmate[target]) return SendClientMessage(playerid, COLOR_GREY, "Он не заключённый тюрьмы.");
	if (mins >= Player[target][pJail]) mins = Player[target][pJail] - 1;
	if (mins < 1) return SendClientMessage(playerid, COLOR_GREY, "Ему и так осталась минута.");
	Player[target][pJail] -= mins;
	Account_Save(target);
	SendClientMessage(target, COLOR_GREEN, "Начальство скостило тебе %d мин. Осталось %d мин.", mins, Player[target][pJail]);
	new msg[144];
	format(msg, sizeof msg, "[Тюрьма] %s скостил %s %d мин. Осталось %d мин.", GetRPName(playerid), GetRPName(target), mins, Player[target][pJail]);
	Prison_MessageGuards(msg);
	Faction_Log(Player[playerid][pFaction], msg[9]);
	return true;
}

stock bool:Prison_TryAlt(playerid)
{
	if (Place_Nearby(playerid, PL_PGUARD, 2.5) == -1) return false;
	Cmd_Prisoners(playerid);
	return true;
}

stock Prison_OnDisconnect(playerid)
{
	gPWorkStep[playerid] = 0;
	gPWorkBusy[playerid] = 0;
	if (gEscapeLeft[playerid] > 0)
	{
		// вышел из игры во время побега — побег сорван
		gEscapeLeft[playerid] = 0;
		Player[playerid][pJail] += PRISON_ESCAPE_FAIL_ADD;
	}
}

stock bool:Prison_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/pwork", true))     { Cmd_PWork(playerid); return true; }
	if (!strcmp(cmd, "/escape", true))    { Cmd_Escape(playerid); return true; }
	if (!strcmp(cmd, "/prisoners", true)) { Cmd_Prisoners(playerid); return true; }
	if (!strcmp(cmd, "/pextend", true))   { Cmd_PExtend(playerid, params); return true; }
	if (!strcmp(cmd, "/preduce", true))   { Cmd_PReduce(playerid, params); return true; }
	return false;
}
