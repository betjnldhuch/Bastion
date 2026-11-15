// =====================================================================
//  fmanage.inc — управление фракцией: права по рангам и личные права,
//  карточка участника, журнал с фильтром.
//
//  Окно: /fr > «Управление» (или /lmenu). Открывается у лидера, зама
//  и у всех, кому выдано хоть одно право руководства.
//
//  Как работают права:
//    - лидер (ранг 10) может всё, это не отключается;
//    - у каждого ранга 1..9 свой набор прав (таблица faction_perms).
//      Зам (ранг 9) по умолчанию может всё, что лидер;
//    - поверх ранга у человека могут быть личные права: «вкл», «выкл»
//      или «как у ранга» (accounts.fperm_on и fperm_off — битовые маски);
//    - меняют права только лидер и зам. Лидер — рангам 1..9 и всем, кроме
//      себя. Зам — рангам 1..8 и людям ниже себя, и только те права,
//      которые есть у него самого. Лидер выключил право заму — оно сразу
//      пропало и у зама, и зам больше не может его раздавать.
//    - трогать равного или старшего нельзя: зам не уволит лидера,
//      не выдаст ему выговор и т.п.
//
//  Журнал: faction_logs.kind — FLOG_* (кадры, деньги, выговоры, права),
//  фильтр по виду записи и по имени игрока.
// =====================================================================

// Номера прав FP_* и виды журнала FLOG_* — в начале factions.inc.
// Права рядового участника (с ними «Управление» не открывается)
#define FP_MEMBER_MASK  ((1 << FP_BANK_IN) | (1 << FP_STORE_TAKE) | (1 << FP_VEHICLE) | (1 << FP_DOORS))

#define FDEPUTY_RANK    (MAX_RANK - 1)
#define FNOTE_LEN       64
#define FM_GRAY         ("{777777}")

static const gFPermName[FP_COUNT][32] =
{
	"Принимать",
	"Увольнять",
	"Менять ранг (ниже своего)",
	"Выговоры: выдать и снять",
	"Общак: положить",
	"Общак: снять",
	"Склад: закупать",
	"Склад: брать снаряжение",
	"Транспорт организации",
	"Двери и ворота",
	"Раздавать спавны",
	"Взводы",
	"Ранги, зарплаты, скины",
	"Журнал",
	"Завод: передать и закрыть"
};

new gFRankPerm[MAX_FACTIONS][MAX_RANK];         // права рангов (индекс: ранг - 1)
static gFPermOn[MAX_PLAYERS];                   // личные права игрока: включено
static gFPermOff[MAX_PLAYERS];                  // личные права игрока: выключено
static gFmRank[MAX_PLAYERS];                    // какой ранг правит игрок в «Права рангов»
static gFmOn[MAX_PLAYERS];                      // личные права выбранного участника (копия для окна)
static gFmOff[MAX_PLAYERS];
static gFmTRank[MAX_PLAYERS];                   // ранг выбранного участника
static gFmLogKind[MAX_PLAYERS];                 // фильтр журнала
static gFmLogName[MAX_PLAYERS][MAX_PLAYER_NAME];

// ---------------------------------------------------------------------
//  Запуск, вход, вступление
// ---------------------------------------------------------------------
stock FPerm_DefaultMask(rank, storeRank)
{
	if (rank >= FDEPUTY_RANK) return FP_ALL;
	new m = (1 << FP_BANK_IN) | (1 << FP_VEHICLE) | (1 << FP_DOORS);
	if (rank >= storeRank) m |= (1 << FP_STORE_TAKE);
	return m;
}

// Права рангов по умолчанию (новая фракция или сброс)
stock FPerm_ResetFaction(f)
{
	if (f < 0 || f >= MAX_FACTIONS) return;
	for (new rk = 1; rk <= MAX_RANK; rk++) gFRankPerm[f][rk - 1] = FPerm_DefaultMask(rk, 2);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM faction_perms WHERE faction = %d", f));
}

stock FPerm_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS faction_perms (faction INTEGER NOT NULL, rank INTEGER NOT NULL, mask INTEGER DEFAULT 0, PRIMARY KEY (faction, rank))"));
	DB_AddColumn("accounts", "fperm_on", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "fperm_off", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "fjoin", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "last_seen", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "fnote", "TEXT");
	DB_AddColumn("faction_logs", "kind", "INTEGER DEFAULT 0");

	for (new f = 0; f < MAX_FACTIONS; f++)
	{
		new store = Faction[f][fStoreRank];
		if (store < 1 || store > MAX_RANK) store = 2;
		for (new rk = 1; rk <= MAX_RANK; rk++) gFRankPerm[f][rk - 1] = FPerm_DefaultMask(rk, store);
	}
	new loaded = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT faction, rank, mask FROM faction_perms");
	if (r != DBResult:0)
	{
		for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
		{
			new f = DB_GetFieldInt(r, 0), rk = DB_GetFieldInt(r, 1);
			if (f >= 0 && f < MAX_FACTIONS && rk >= 1 && rk < MAX_RANK)
			{
				gFRankPerm[f][rk - 1] = DB_GetFieldInt(r, 2) & FP_ALL;
				loaded++;
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Factions] Loaded %d rank permission rows.", loaded);
}

stock FPerm_OnLogin(playerid)
{
	gFPermOn[playerid] = 0;
	gFPermOff[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT fperm_on, fperm_off FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			gFPermOn[playerid] = DB_GetFieldInt(r, 0) & FP_ALL;
			gFPermOff[playerid] = DB_GetFieldInt(r, 1) & FP_ALL;
		}
		DB_FreeResultSet(r);
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET last_seen = %d WHERE id = %d", gettime(), Player[playerid][pDbId]));
}

stock FPerm_OnDisconnect(playerid)
{
	if (Player[playerid][pLogged] && Player[playerid][pDbId] != 0)
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET last_seen = %d WHERE id = %d", gettime(), Player[playerid][pDbId]));
	gFPermOn[playerid] = 0;
	gFPermOff[playerid] = 0;
}

// Игрок сменил фракцию (вступил или ушёл): личные права и заметка — с нуля
stock FPerm_OnFactionChange(playerid, f)
{
	gFPermOn[playerid] = 0;
	gFPermOff[playerid] = 0;
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET fperm_on = 0, fperm_off = 0, fnote = NULL, fjoin = %d WHERE id = %d",
		(f == FACTION_NONE) ? 0 : gettime(), Player[playerid][pDbId]));
}

// Уволен оффлайн: то же самое прямо в базе
stock FPerm_ClearOffline(dbid)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET fperm_on = 0, fperm_off = 0, fnote = NULL, fjoin = 0 WHERE id = %d", dbid));
}

// ---------------------------------------------------------------------
//  Проверки
// ---------------------------------------------------------------------
stock FPerm_MaskOf(f, rank, on, off)
{
	if (!Faction_IsValid(f) || rank < 1) return 0;
	if (rank >= MAX_RANK) return FP_ALL;
	return (gFRankPerm[f][rank - 1] | on) & ~off & FP_ALL;
}

stock FPerm_Mask(playerid)
{
	return FPerm_MaskOf(Player[playerid][pFaction], Player[playerid][pRank], gFPermOn[playerid], gFPermOff[playerid]);
}

// Есть ли у игрока право perm в своей фракции (1/0)
stock FPerm_Has(playerid, perm)
{
	return (FPerm_Mask(playerid) >> perm) & 1;
}

// То же, но с сообщением игроку
stock FPerm_Need(playerid, perm)
{
	if (!Faction_IsValid(Player[playerid][pFaction]))
	{
		SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь во фракции.");
		return 0;
	}
	if (FPerm_Has(playerid, perm)) return 1;
	SendClientMessage(playerid, COLOR_GREY, "У тебя нет права «%s». Права выдают лидер и зам: /fr > Управление.", gFPermName[perm]);
	return 0;
}

// Открывается ли «Управление»: лидер, зам или есть право руководства
stock FPerm_IsManager(playerid)
{
	if (!Faction_IsValid(Player[playerid][pFaction])) return 0;
	if (Player[playerid][pRank] >= FDEPUTY_RANK) return 1;
	return (FPerm_Mask(playerid) & ~FP_MEMBER_MASK) != 0;
}

// Права меняют только лидер и зам
stock FPerm_CanEdit(playerid)
{
	return (Faction_IsValid(Player[playerid][pFaction]) && Player[playerid][pRank] >= FDEPUTY_RANK);
}

// ---------------------------------------------------------------------
//  Окно «Управление»
// ---------------------------------------------------------------------
stock bool:Cmd_LMenu(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) { SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь во фракции."); return true; }
	if (!FPerm_IsManager(playerid))
	{
		SendClientMessage(playerid, COLOR_GREY, "Управление доступно лидеру, заму и тем, кому они выдали права.");
		return true;
	}
	new body[1200], line[128], title[64];
	strcat(body, "Раздел\tЧто там\n");
	format(line, sizeof line, "Состав и карточки участников\t{AFAFAF}%d чел., в т.ч. оффлайн\n", Faction_CountAll(f));
	strcat(body, line);
	if (FPerm_CanEdit(playerid)) strcat(body, "Права рангов\t{AFAFAF}что может каждый ранг\n");
	else strcat(body, "{777777}Права рангов\t{777777}лидер и зам\n");
	strcat(body, "Ранги, зарплаты и скины\t\n");
	format(line, sizeof line, "Общак\t{33AA33}$%d {AFAFAF}(на базе)\n", Faction[f][fBank]);
	strcat(body, line);
	format(line, sizeof line, "Склад\t{AFAFAF}материалов %d (на базе)\n", Faction[f][fMats]);
	strcat(body, line);
	strcat(body, "Взводы\t{AFAFAF}/squads, /setsquad\n");
	strcat(body, "Журнал\t{AFAFAF}с фильтром\n");
	format(line, sizeof line, "Мои права\t{AFAFAF}%d из %d\n", FPerm_CountBits(FPerm_Mask(playerid)), FP_COUNT);
	strcat(body, line);
	format(title, sizeof title, "Управление — %s", Faction_GetName(f));
	ShowPlayerDialog(playerid, DIALOG_LMENU, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Выбрать", "Закрыть", body);
	return true;
}

stock FPerm_CountBits(m)
{
	new n = 0;
	for (new p = 0; p < FP_COUNT; p++) if ((m >> p) & 1) n++;
	return n;
}

static FM_OnMenu(playerid, listitem)
{
	if (!FPerm_IsManager(playerid)) return;
	switch (listitem)
	{
		case 0: Faction_ShowMembers(playerid);
		case 1:
		{
			if (!FPerm_CanEdit(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Права рангов меняют только лидер и зам."); Cmd_LMenu(playerid); return; }
			FM_ShowRanks(playerid);
		}
		case 2:
		{
			if (!FPerm_Need(playerid, FP_RANKS)) { Cmd_LMenu(playerid); return; }
			Faction_ShowRanks(playerid);
		}
		case 3: Cmd_FBank(playerid);
		case 4: Cmd_FStorage(playerid);
		case 5: Cmd_Squads(playerid);
		case 6:
		{
			if (!FPerm_Need(playerid, FP_LOG)) { Cmd_LMenu(playerid); return; }
			FM_ShowLogMenu(playerid);
		}
		case 7: FM_ShowMyPerms(playerid);
	}
}

static FM_ShowMyPerms(playerid)
{
	new text[1400], line[96], m = FPerm_Mask(playerid);
	for (new p = 0; p < FP_COUNT; p++)
	{
		format(line, sizeof line, "%s%s\n", ((m >> p) & 1) ? ("{33AA33}[+] {FFFFFF}") : ("{777777}[  ] "), gFPermName[p]);
		strcat(text, line);
	}
	if (Player[playerid][pRank] >= MAX_RANK) strcat(text, "\n{AFAFAF}Ты лидер: у тебя все права.");
	ShowPlayerDialog(playerid, DIALOG_FM_INFO, DIALOG_STYLE_MSGBOX, "Мои права", "%s", "Назад", "", text);
	SetPVarInt(playerid, "fm_info_back", 0);
}

// ---------------------------------------------------------------------
//  Права рангов
// ---------------------------------------------------------------------
static FM_ShowRanks(playerid)
{
	new f = Player[playerid][pFaction];
	new body[1200], line[96];
	strcat(body, "Ранг\tПрав\n");
	for (new rk = 1; rk < Player[playerid][pRank] && rk < MAX_RANK; rk++)
	{
		format(line, sizeof line, "%d. %s\t%d из %d\n", rk, Faction_GetRankName(f, rk), FPerm_CountBits(gFRankPerm[f][rk - 1]), FP_COUNT);
		strcat(body, line);
	}
	ShowPlayerDialog(playerid, DIALOG_FM_RANKS, DIALOG_STYLE_TABLIST_HEADERS, "Права рангов", "%s", "Открыть", "Назад", body);
}

static FM_ShowRankPerms(playerid)
{
	new f = Player[playerid][pFaction], rk = gFmRank[playerid];
	new body[1600], line[112], my = FPerm_Mask(playerid);
	strcat(body, "Право\tРангу\n");
	for (new p = 0; p < FP_COUNT; p++)
	{
		new bool:on = ((gFRankPerm[f][rk - 1] >> p) & 1) != 0;
		new bool:can = ((my >> p) & 1) != 0;
		format(line, sizeof line, "%s%s\t%s\n", can ? ("{FFFFFF}") : ("{777777}"), gFPermName[p], on ? ("{33AA33}вкл") : ("{FF6347}выкл"));
		strcat(body, line);
	}
	new title[64];
	format(title, sizeof title, "Права: %d. %s", rk, Faction_GetRankName(f, rk));
	ShowPlayerDialog(playerid, DIALOG_FM_RANKPERMS, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Вкл/выкл", "Назад", body);
}

static FM_ToggleRankPerm(playerid, p)
{
	new f = Player[playerid][pFaction], rk = gFmRank[playerid];
	if (!FPerm_CanEdit(playerid) || rk < 1 || rk >= Player[playerid][pRank] || rk >= MAX_RANK) return;
	if (!FPerm_Has(playerid, p))
	{
		SendClientMessage(playerid, COLOR_GREY, "Нельзя менять право, которого нет у тебя самого: «%s».", gFPermName[p]);
		return;
	}
	gFRankPerm[f][rk - 1] ^= (1 << p);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO faction_perms (faction, rank, mask) VALUES (%d, %d, %d)", f, rk, gFRankPerm[f][rk - 1]));
	new bool:on = ((gFRankPerm[f][rk - 1] >> p) & 1) != 0;
	new msg[144];
	format(msg, sizeof msg, "%s %s рангу %d право «%s».", GetRPName(playerid), on ? ("выдал") : ("забрал у"), rk, gFPermName[p]);
	Faction_Log(f, msg, FLOG_PERM);
}

// ---------------------------------------------------------------------
//  Карточка участника
// ---------------------------------------------------------------------
// Прочитать участника своей фракции (онлайн или оффлайн). 0 — уже не в твоей фракции.
static FM_GetMember(playerid, dbid, name[], &rank, size = sizeof (name))
{
	new faction;
	if (!Faction_GetMember(dbid, name, rank, faction, size) || faction != Player[playerid][pFaction])
	{
		SendClientMessage(playerid, COLOR_GREY, "Этот человек уже не в твоей фракции.");
		return 0;
	}
	return 1;
}

// Можно ли что-то делать с участником ранга rank (только с младшими)
static FM_CanTouch(playerid, dbid, rank)
{
	if (dbid == Player[playerid][pDbId]) { SendClientMessage(playerid, COLOR_GREY, "С собой так нельзя."); return 0; }
	if (rank >= Player[playerid][pRank]) { SendClientMessage(playerid, COLOR_GREY, "Нельзя управлять равным или старшим по рангу."); return 0; }
	return 1;
}

// Действие с выбранным участником: есть право и участник младше
stock FM_CanAct(playerid, perm, name[], &rank, size = sizeof (name))
{
	if (!FPerm_Need(playerid, perm)) return 0;
	if (!FM_GetMember(playerid, gLSelMember[playerid], name, rank, size)) return 0;
	return FM_CanTouch(playerid, gLSelMember[playerid], rank);
}

static FM_LoadMemberPerms(playerid, dbid)
{
	gFmOn[playerid] = 0;
	gFmOff[playerid] = 0;
	new online = Faction_FindOnline(dbid);
	if (online != INVALID_PLAYER_ID)
	{
		gFmOn[playerid] = gFPermOn[online];
		gFmOff[playerid] = gFPermOff[online];
		return;
	}
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT fperm_on, fperm_off FROM accounts WHERE id = %d", dbid);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			gFmOn[playerid] = DB_GetFieldInt(r, 0) & FP_ALL;
			gFmOff[playerid] = DB_GetFieldInt(r, 1) & FP_ALL;
		}
		DB_FreeResultSet(r);
	}
}

// Взвод участника (онлайн — из памяти, оффлайн — из базы). 0 — без взвода.
static FM_MemberSquad(f, dbid)
{
	new online = Faction_FindOnline(dbid);
	if (online != INVALID_PLAYER_ID) return gSquad[online];
	new q = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT fsquad FROM accounts WHERE id = %d", dbid);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			new v = DB_GetFieldInt(r, 0);
			if (v / 10 == f && v % 10 >= 1 && v % 10 <= MAX_SQUADS) q = v % 10;
		}
		DB_FreeResultSet(r);
	}
	return q;
}

static FM_FormatAgo(ts, dest[], size)
{
	if (ts <= 0) { format(dest, size, "неизвестно"); return; }
	new diff = gettime() - ts;
	if (diff < 0) diff = 0;
	if (diff < 3600) format(dest, size, "%d мин. назад", diff / 60);
	else if (diff < 86400) format(dest, size, "%d ч. назад", diff / 3600);
	else format(dest, size, "%d дн. назад", diff / 86400);
}

stock Faction_ShowMemberActions(playerid)
{
	new f = Player[playerid][pFaction];
	new name[MAX_PLAYER_NAME], rank, dbid = gLSelMember[playerid];
	if (!FM_GetMember(playerid, dbid, name, rank)) return;
	gFmTRank[playerid] = rank;
	FM_LoadMemberPerms(playerid, dbid);
	new q = FM_MemberSquad(f, dbid);
	new my = FPerm_Mask(playerid);
	new bool:younger = (rank < Player[playerid][pRank] && dbid != Player[playerid][pDbId]);
	new body[900], line[128], sq[24];
	if (q > 0) { if (!gSquadsLoaded) Squads_LoadNames(); strcat(sq, gSquadName[f][q - 1], 24); }
	else strcat(sq, "нет");

	strcat(body, "Информация\n");
	format(line, sizeof line, "%sИзменить ранг {AFAFAF}(сейчас %d. %s)\n", (younger && ((my >> FP_RANK) & 1)) ? ("") : FM_GRAY, rank, Faction_GetRankName(f, rank));
	strcat(body, line);
	format(line, sizeof line, "%sВыдать выговор {AFAFAF}(%d/%d)\n", (younger && ((my >> FP_WARN) & 1)) ? ("") : FM_GRAY, Faction_GetWarns(dbid), FACTION_MAX_WARNS);
	strcat(body, line);
	format(line, sizeof line, "%sСнять выговор\n", (younger && ((my >> FP_WARN) & 1)) ? ("") : FM_GRAY);
	strcat(body, line);
	format(line, sizeof line, "%sВзвод {AFAFAF}(%s)\n", (younger && ((my >> FP_SQUADS) & 1)) ? ("") : FM_GRAY, sq);
	strcat(body, line);
	format(line, sizeof line, "%sЛичные права\n", (younger && FPerm_CanEdit(playerid)) ? ("") : FM_GRAY);
	strcat(body, line);
	format(line, sizeof line, "%sЗаметка\n", FPerm_CanEdit(playerid) ? ("") : FM_GRAY);
	strcat(body, line);
	format(line, sizeof line, "%sУволить\n", (younger && ((my >> FP_FIRE) & 1)) ? ("{FF6347}") : FM_GRAY);
	strcat(body, line);

	new title[96];
	format(title, sizeof title, "%s — %d. %s", name, rank, Faction_GetRankName(f, rank));
	ShowPlayerDialog(playerid, DIALOG_LMEMBER_ACT, DIALOG_STYLE_LIST, title, "%s", "Выбрать", "Назад", body);
}

static FM_ShowCardInfo(playerid)
{
	new f = Player[playerid][pFaction];
	new name[MAX_PLAYER_NAME], rank, dbid = gLSelMember[playerid];
	if (!FM_GetMember(playerid, dbid, name, rank)) return;
	new joined = 0, seen = 0, note[FNOTE_LEN];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT fjoin, last_seen, fnote FROM accounts WHERE id = %d", dbid);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			joined = DB_GetFieldInt(r, 0);
			seen = DB_GetFieldInt(r, 1);
			DB_GetFieldString(r, 2, note, sizeof note);
		}
		DB_FreeResultSet(r);
	}
	new q = FM_MemberSquad(f, dbid);
	new online = Faction_FindOnline(dbid);
	new text[1600], line[160], d1[24], d2[24];
	FM_FormatAgo(joined, d1, sizeof d1);
	FM_FormatAgo(seen, d2, sizeof d2);
	format(line, sizeof line, "{AFAFAF}Имя: {FFFFFF}%s\n", name);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Ранг: {FFFFFF}%d. %s\n", rank, Faction_GetRankName(f, rank));
	strcat(text, line);
	if (q > 0)
	{
		if (!gSquadsLoaded) Squads_LoadNames();
		format(line, sizeof line, "{AFAFAF}Взвод: {FFFFFF}%s\n", gSquadName[f][q - 1]);
	}
	else format(line, sizeof line, "{AFAFAF}Взвод: {FFFFFF}нет\n");
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Вступил: {FFFFFF}%s\n", d1);
	strcat(text, line);
	if (online != INVALID_PLAYER_ID) format(line, sizeof line, "{AFAFAF}Статус: {33AA33}в игре [%d]%s\n", online, Player[online][pDuty] ? (", на смене") : (""));
	else format(line, sizeof line, "{AFAFAF}Последний вход: {FFFFFF}%s\n", d2);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Выговоры: {FFFFFF}%d / %d\n", Faction_GetWarns(dbid), FACTION_MAX_WARNS);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Заметка: {FFFFFF}%s\n", (note[0] != '\0') ? note : ("нет"));
	strcat(text, line);

	new m = FPerm_MaskOf(f, rank, gFmOn[playerid], gFmOff[playerid]);
	format(line, sizeof line, "\n{AFAFAF}Права (%d из %d):\n", FPerm_CountBits(m), FP_COUNT);
	strcat(text, line);
	for (new p = 0; p < FP_COUNT; p++)
	{
		if (!((m >> p) & 1)) continue;
		format(line, sizeof line, "{FFFFFF}• %s%s\n", gFPermName[p], (((gFmOn[playerid] >> p) & 1) && rank < MAX_RANK) ? (" {AFAFAF}(лично)") : (""));
		strcat(text, line);
	}
	SetPVarInt(playerid, "fm_info_back", 1);
	ShowPlayerDialog(playerid, DIALOG_FM_INFO, DIALOG_STYLE_MSGBOX, "Карточка участника", "%s", "Назад", "", text);
}

static FM_ShowPersonal(playerid)
{
	new f = Player[playerid][pFaction], rank = gFmTRank[playerid];
	new body[2000], line[128];
	strcat(body, "Право\tСостояние\n");
	for (new p = 0; p < FP_COUNT; p++)
	{
		new st[48];
		if ((gFmOn[playerid] >> p) & 1) format(st, sizeof st, "{33AA33}вкл лично");
		else if ((gFmOff[playerid] >> p) & 1) format(st, sizeof st, "{FF6347}выкл лично");
		else format(st, sizeof st, "{AFAFAF}как у ранга: %s", ((gFRankPerm[f][rank - 1] >> p) & 1) ? ("вкл") : ("выкл"));
		format(line, sizeof line, "%s%s\t%s\n", FPerm_Has(playerid, p) ? ("{FFFFFF}") : ("{777777}"), gFPermName[p], st);
		strcat(body, line);
	}
	ShowPlayerDialog(playerid, DIALOG_FM_PERSONAL, DIALOG_STYLE_TABLIST_HEADERS, "Личные права (нажми: вкл > выкл > как у ранга)", "%s", "Сменить", "Назад", body);
}

static FM_TogglePersonal(playerid, p)
{
	new f = Player[playerid][pFaction];
	new name[MAX_PLAYER_NAME], rank, dbid = gLSelMember[playerid];
	if (!FPerm_CanEdit(playerid)) return 0;
	if (!FM_GetMember(playerid, dbid, name, rank) || !FM_CanTouch(playerid, dbid, rank)) return 0;
	if (rank >= MAX_RANK) return 0;
	if (!FPerm_Has(playerid, p))
	{
		SendClientMessage(playerid, COLOR_GREY, "Нельзя менять право, которого нет у тебя самого: «%s».", gFPermName[p]);
		return 1;
	}
	FM_LoadMemberPerms(playerid, dbid);
	new bit = (1 << p), what[24];
	if (gFmOn[playerid] & bit) { gFmOn[playerid] &= ~bit; gFmOff[playerid] |= bit; format(what, sizeof what, "выкл"); }
	else if (gFmOff[playerid] & bit) { gFmOff[playerid] &= ~bit; format(what, sizeof what, "как у ранга"); }
	else { gFmOn[playerid] |= bit; format(what, sizeof what, "вкл"); }
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET fperm_on = %d, fperm_off = %d WHERE id = %d", gFmOn[playerid], gFmOff[playerid], dbid));
	new online = Faction_FindOnline(dbid);
	if (online != INVALID_PLAYER_ID)
	{
		gFPermOn[online] = gFmOn[playerid];
		gFPermOff[online] = gFmOff[playerid];
		SendClientMessage(online, Faction_GetColor(f), "%s изменил твоё право «%s»: %s. Твои права: /fr > Управление > Мои права.", GetRPName(playerid), gFPermName[p], what);
	}
	new msg[144];
	format(msg, sizeof msg, "%s: право «%s» для %s — %s.", GetRPName(playerid), gFPermName[p], name, what);
	Faction_Log(f, msg, FLOG_PERM);
	return 1;
}

// Лидер/зам снял права у ранга или человека: если у зама пропало право, он не раздаёт его дальше —
// это проверяется при каждом действии (FPerm_Has), отдельно чистить ничего не надо.

static FM_SetSquad(playerid, q)
{
	new f = Player[playerid][pFaction];
	new name[MAX_PLAYER_NAME], rank, dbid = gLSelMember[playerid];
	if (!FM_CanAct(playerid, FP_SQUADS, name, rank)) return;
	if (!gSquadsLoaded) Squads_LoadNames();
	new online = Faction_FindOnline(dbid);
	if (online != INVALID_PLAYER_ID) Squad_Set(online, q);
	else DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET fsquad = %d WHERE id = %d", (q > 0) ? (f * 10 + q) : 0, dbid));
	new msg[144];
	if (q == 0) format(msg, sizeof msg, "%s убрал %s из взвода.", GetRPName(playerid), name);
	else format(msg, sizeof msg, "%s перевёл %s во взвод «%s».", GetRPName(playerid), name, gSquadName[f][q - 1]);
	Faction_Log(f, msg, FLOG_STAFF);
	if (online != INVALID_PLAYER_ID) SendClientMessage(online, Faction_GetColor(f), "%s", msg);
	SendClientMessage(playerid, COLOR_GREEN, "Готово.");
}

static FM_SetNote(playerid, const text[])
{
	new name[MAX_PLAYER_NAME], rank, dbid = gLSelMember[playerid];
	if (!FPerm_CanEdit(playerid)) return;
	if (!FM_GetMember(playerid, dbid, name, rank)) return;
	if (text[0] == '\0' || !strcmp(text, "-"))
	{
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET fnote = NULL WHERE id = %d", dbid));
		SendClientMessage(playerid, COLOR_GREEN, "Заметка о %s удалена.", name);
		return;
	}
	new note[FNOTE_LEN];
	if (!RestParam(text, 0, note) || !Faction_IsGoodName(note, 1, FNOTE_LEN - 1))
	{
		SendClientMessage(playerid, COLOR_GREY, "Заметка: до %d символов, без фигурных скобок и знака процента.", FNOTE_LEN - 1);
		return;
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET fnote = '%q' WHERE id = %d", note, dbid));
	SendClientMessage(playerid, COLOR_GREEN, "Заметка о %s сохранена. Её видят лидер и замы в карточке.", name);
}

static FM_OnCard(playerid, listitem)
{
	new f = Player[playerid][pFaction];
	new name[MAX_PLAYER_NAME], rank, msg[144];
	switch (listitem)
	{
		case 0: FM_ShowCardInfo(playerid);
		case 1:
		{
			if (!FM_CanAct(playerid, FP_RANK, name, rank)) { Faction_ShowMemberActions(playerid); return; }
			ShowPlayerDialog(playerid, DIALOG_LMEMBER_RANK, DIALOG_STYLE_INPUT, "Ранг",
				"Новый ранг для %s (1-%d)\nСейчас: %d. %s", "Сохранить", "Назад",
				name, Player[playerid][pRank] - 1, rank, Faction_GetRankName(f, rank));
		}
		case 2:
		{
			if (!FM_CanAct(playerid, FP_WARN, name, rank)) { Faction_ShowMemberActions(playerid); return; }
			ShowPlayerDialog(playerid, DIALOG_LMEMBER_WARN, DIALOG_STYLE_INPUT, "Выговор",
				"Выговор для %s. Напиши причину.\nНа %d-м выговоре игрок будет уволен.", "Выдать", "Назад",
				name, FACTION_MAX_WARNS);
		}
		case 3:
		{
			if (!FM_CanAct(playerid, FP_WARN, name, rank)) { Faction_ShowMemberActions(playerid); return; }
			new warns = Faction_GetWarns(gLSelMember[playerid]);
			if (warns <= 0) { SendClientMessage(playerid, COLOR_GREY, "У него нет выговоров."); Faction_ShowMemberActions(playerid); return; }
			Faction_SetWarns(gLSelMember[playerid], warns - 1);
			format(msg, sizeof msg, "%s снял выговор с %s (%d/%d).", GetRPName(playerid), name, warns - 1, FACTION_MAX_WARNS);
			Faction_Log(f, msg, FLOG_WARN);
			SendClientMessage(playerid, COLOR_GREEN, "Выговор снят. Осталось: %d/%d.", warns - 1, FACTION_MAX_WARNS);
			new target = Faction_FindOnline(gLSelMember[playerid]);
			if (target != INVALID_PLAYER_ID) SendClientMessage(target, COLOR_GREEN, "%s снял с тебя выговор (%d/%d).", GetRPName(playerid), warns - 1, FACTION_MAX_WARNS);
			Faction_ShowMemberActions(playerid);
		}
		case 4:
		{
			if (!FM_CanAct(playerid, FP_SQUADS, name, rank)) { Faction_ShowMemberActions(playerid); return; }
			if (!gSquadsLoaded) Squads_LoadNames();
			new list[400], line[48];
			for (new q = 0; q < MAX_SQUADS; q++)
			{
				format(line, sizeof line, "%d — %s\n", q + 1, gSquadName[f][q]);
				strcat(list, line);
			}
			ShowPlayerDialog(playerid, DIALOG_FM_SQUAD, DIALOG_STYLE_INPUT, "Взвод",
				"Номер взвода для %s (0 — без взвода):\n\n%s", "Сохранить", "Назад", name, list);
		}
		case 5:
		{
			if (!FPerm_CanEdit(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Личные права выдают только лидер и зам."); Faction_ShowMemberActions(playerid); return; }
			if (!FM_GetMember(playerid, gLSelMember[playerid], name, rank) || !FM_CanTouch(playerid, gLSelMember[playerid], rank)) { Faction_ShowMemberActions(playerid); return; }
			FM_LoadMemberPerms(playerid, gLSelMember[playerid]);
			gFmTRank[playerid] = rank;
			FM_ShowPersonal(playerid);
		}
		case 6:
		{
			if (!FPerm_CanEdit(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Заметки пишут только лидер и зам."); Faction_ShowMemberActions(playerid); return; }
			if (!FM_GetMember(playerid, gLSelMember[playerid], name, rank)) return;
			ShowPlayerDialog(playerid, DIALOG_FM_NOTE, DIALOG_STYLE_INPUT, "Заметка",
				"Заметка о %s (видят лидер и замы, до %d символов).\nЧтобы удалить, введи «-».", "Сохранить", "Назад", name, FNOTE_LEN - 1);
		}
		case 7:
		{
			if (!FM_CanAct(playerid, FP_FIRE, name, rank)) { Faction_ShowMemberActions(playerid); return; }
			Faction_Fire(playerid, gLSelMember[playerid], name, "решение руководства");
		}
	}
}

// ---------------------------------------------------------------------
//  Журнал с фильтром
// ---------------------------------------------------------------------
static FM_ShowLogMenu(playerid)
{
	ShowPlayerDialog(playerid, DIALOG_FM_LOGMENU, DIALOG_STYLE_LIST, "Журнал",
		"Все записи\nКадры: приём, увольнение, ранги, взводы\nДеньги и склад\nВыговоры\nПрава\nПо игроку (ввести ник)",
		"Показать", "Назад");
}

stock Faction_ShowLogs(playerid)
{
	new f = Player[playerid][pFaction];
	new body[3600], line[160];
	new DBResult:r;
	if (gFmLogKind[playerid] == -1)
		r = DB_ExecuteQuery(gDB, "SELECT text FROM faction_logs WHERE faction = %d AND text LIKE '%%%q%%' ORDER BY id DESC LIMIT %d", f, gFmLogName[playerid], FACTION_LOG_SHOW);
	else if (gFmLogKind[playerid] > 0)
		r = DB_ExecuteQuery(gDB, "SELECT text FROM faction_logs WHERE faction = %d AND kind = %d ORDER BY id DESC LIMIT %d", f, gFmLogKind[playerid], FACTION_LOG_SHOW);
	else
		r = DB_ExecuteQuery(gDB, "SELECT text FROM faction_logs WHERE faction = %d ORDER BY id DESC LIMIT %d", f, FACTION_LOG_SHOW);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			DB_GetFieldString(r, 0, line, sizeof line);
			strcat(body, line);
			strcat(body, "\n");
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	if (body[0] == '\0') strcat(body, "Записей нет.");
	ShowPlayerDialog(playerid, DIALOG_LLOGS, DIALOG_STYLE_MSGBOX, "Журнал (последние записи)", "%s", "Назад", "", body);
}

// ---------------------------------------------------------------------
//  Ответы окон. Вызывается из Factions_OnDialog. 1 — окно обработано.
// ---------------------------------------------------------------------
stock FM_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	switch (dialogid)
	{
		case DIALOG_LMENU:
		{
			if (response) FM_OnMenu(playerid, listitem);
		}
		case DIALOG_LMEMBER_ACT:
		{
			if (!response) { Faction_ShowMembers(playerid); return 1; }
			FM_OnCard(playerid, listitem);
		}
		case DIALOG_FM_INFO:
		{
			if (GetPVarInt(playerid, "fm_info_back") == 1) Faction_ShowMemberActions(playerid);
			else Cmd_LMenu(playerid);
		}
		case DIALOG_FM_RANKS:
		{
			if (!response) { Cmd_LMenu(playerid); return 1; }
			if (!FPerm_CanEdit(playerid)) return 1;
			new rk = listitem + 1;
			if (rk < 1 || rk >= Player[playerid][pRank] || rk >= MAX_RANK) return 1;
			gFmRank[playerid] = rk;
			FM_ShowRankPerms(playerid);
		}
		case DIALOG_FM_RANKPERMS:
		{
			if (!response) { FM_ShowRanks(playerid); return 1; }
			if (listitem < 0 || listitem >= FP_COUNT) return 1;
			FM_ToggleRankPerm(playerid, listitem);
			FM_ShowRankPerms(playerid);
		}
		case DIALOG_FM_PERSONAL:
		{
			if (!response) { Faction_ShowMemberActions(playerid); return 1; }
			if (listitem < 0 || listitem >= FP_COUNT) return 1;
			if (FM_TogglePersonal(playerid, listitem)) FM_ShowPersonal(playerid);
		}
		case DIALOG_FM_SQUAD:
		{
			if (!response) { Faction_ShowMemberActions(playerid); return 1; }
			new q;
			if (!IsNumeric(inputtext) || (q = strval(inputtext)) < 0 || q > MAX_SQUADS)
			{
				SendClientMessage(playerid, COLOR_GREY, "Номер взвода от 0 до %d.", MAX_SQUADS);
				Faction_ShowMemberActions(playerid);
				return 1;
			}
			FM_SetSquad(playerid, q);
			Faction_ShowMemberActions(playerid);
		}
		case DIALOG_FM_NOTE:
		{
			if (response) FM_SetNote(playerid, inputtext);
			Faction_ShowMemberActions(playerid);
		}
		case DIALOG_FM_LOGMENU:
		{
			if (!response) { Cmd_LMenu(playerid); return 1; }
			if (!FPerm_Need(playerid, FP_LOG)) return 1;
			if (listitem == 5)
			{
				ShowPlayerDialog(playerid, DIALOG_FM_LOGNAME, DIALOG_STYLE_INPUT, "Журнал по игроку",
					"Ник игрока (или его часть):", "Показать", "Назад");
				return 1;
			}
			gFmLogKind[playerid] = listitem;     // 0 — все, дальше FLOG_*
			Faction_ShowLogs(playerid);
		}
		case DIALOG_FM_LOGNAME:
		{
			if (!response) { FM_ShowLogMenu(playerid); return 1; }
			new name[MAX_PLAYER_NAME];
			if (!RestParam(inputtext, 0, name) || !Faction_IsGoodName(name, 2, MAX_PLAYER_NAME - 1))
			{
				SendClientMessage(playerid, COLOR_GREY, "Введи ник или его часть (от 2 символов).");
				FM_ShowLogMenu(playerid);
				return 1;
			}
			gFmLogName[playerid][0] = '\0';
			strcat(gFmLogName[playerid], name, MAX_PLAYER_NAME);
			gFmLogKind[playerid] = -1;
			Faction_ShowLogs(playerid);
		}
		case DIALOG_LLOGS:
		{
			FM_ShowLogMenu(playerid);
		}
		default: return 0;
	}
	return 1;
}
