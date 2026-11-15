// =====================================================================
//  orgs.inc — панель владельца по всем организациям (/orgs) и спец-выговоры.
//
//  /orgs (админ ORGS_ADMIN_LEVEL+): список фракций и семей > организация > настройки:
//    - спец-выговоры: каким фракциям эта организация их выдаёт (номера через
//      запятую) и с какого ранга;
//    - департамент (/d): по типу (госструктуры), всегда или никогда;
//    - отключённые возможности: право выключается для всей организации,
//      даже у лидера (например, снимать деньги из общака).
//  Семьи: только отключённые возможности (families.inc, FamAdm_*).
//
//  Спец-выговор (/swarn [id] [причина]): выдаёт сотрудник другой организации,
//  у которой есть на это доступ (по умолчанию ФБР — полиции и армии).
//  Считается как обычный выговор (3 — увольнение), но снять его может только
//  организация, которая выдала (/unswarn [id]), или админ 4+.
//  Правительство: создай фракцию типа «гос. организация» (/createfaction)
//  и дай ей в /orgs спец-выговоры нужным госструктурам.
//  Данные: org_settings, faction_swarns.
// =====================================================================

#define ORGS_ADMIN_LEVEL    6
#define ORG_DEPT_AUTO       0
#define ORG_DEPT_ON         1
#define ORG_DEPT_OFF        2
#define ORG_LIST_MAX        (MAX_FACTIONS + 1)

static gOrgSpec[MAX_FACTIONS][96];      // кому выдаёт спец-выговоры: «1,7»
static gOrgSpecRank[MAX_FACTIONS];      // с какого ранга
static gOrgDept[MAX_FACTIONS];          // ORG_DEPT_*
static gOrgDisabled[MAX_FACTIONS];      // выключенные права FP_* для всей фракции
static gOrgSel[MAX_PLAYERS];            // выбранная фракция (или -(слот семьи + 1))
static gOrgMap[MAX_PLAYERS][ORG_LIST_MAX];

static Org_Defaults(f)
{
	gOrgSpec[f][0] = '\0';
	gOrgSpecRank[f] = MAX_RANK - 1;
	gOrgDept[f] = ORG_DEPT_AUTO;
	gOrgDisabled[f] = 0;
	if (f == FACTION_FBI) format(gOrgSpec[f], 96, "%d,%d", FACTION_POLICE, FACTION_ARMY);
}

stock Orgs_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS org_settings (faction INTEGER PRIMARY KEY, spec TEXT, spec_rank INTEGER DEFAULT 9, dept INTEGER DEFAULT 0, disabled INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS faction_swarns (id INTEGER PRIMARY KEY AUTOINCREMENT, account INTEGER NOT NULL, faction INTEGER NOT NULL, by_faction INTEGER NOT NULL, by_name TEXT, reason TEXT, date INTEGER DEFAULT 0)"));
	for (new f = 0; f < MAX_FACTIONS; f++) Org_Defaults(f);
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT faction, spec, spec_rank, dept, disabled FROM org_settings");
	if (r != DBResult:0)
	{
		for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
		{
			new f = DB_GetFieldInt(r, 0);
			if (f > 0 && f < MAX_FACTIONS)
			{
				DB_GetFieldString(r, 1, gOrgSpec[f], 96);
				gOrgSpecRank[f] = DB_GetFieldInt(r, 2);
				gOrgDept[f] = DB_GetFieldInt(r, 3);
				gOrgDisabled[f] = DB_GetFieldInt(r, 4) & FP_ALL;
				if (gOrgSpecRank[f] < 1 || gOrgSpecRank[f] > MAX_RANK) gOrgSpecRank[f] = MAX_RANK - 1;
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
}

static Org_Save(f)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO org_settings (faction, spec, spec_rank, dept, disabled) VALUES (%d, '%q', %d, %d, %d)",
		f, gOrgSpec[f], gOrgSpecRank[f], gOrgDept[f], gOrgDisabled[f]));
}

// Фракцию удалили или создали заново: настройки по умолчанию
stock Org_Reset(f)
{
	if (f <= 0 || f >= MAX_FACTIONS) return;
	Org_Defaults(f);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM org_settings WHERE faction = %d", f));
}

// Права, выключенные владельцем для всей фракции
stock Org_Disabled(f)
{
	if (f <= 0 || f >= MAX_FACTIONS) return 0;
	return gOrgDisabled[f];
}

// Видит ли фракция чат департамента /d
stock Org_InDept(f)
{
	if (!Faction_IsValid(f)) return 0;
	if (gOrgDept[f] == ORG_DEPT_ON) return 1;
	if (gOrgDept[f] == ORG_DEPT_OFF) return 0;
	return Faction_IsState(f) ? 1 : 0;
}

// Есть ли номер t в списке «1,7,12»
static Org_ListHas(const s[], t)
{
	new v = 0, bool:num = false;
	for (new i = 0; ; i++)
	{
		if (s[i] >= '0' && s[i] <= '9') { if (v < 100000) v = v * 10 + (s[i] - '0'); num = true; continue; }
		if (num && v == t) return 1;
		v = 0; num = false;
		if (s[i] == '\0') break;
	}
	return 0;
}

// Может ли фракция by выдавать спец-выговоры фракции t
stock Org_CanSpecWarn(by, t)
{
	if (!Faction_IsValid(by) || !Faction_IsValid(t) || by == t) return 0;
	return Org_ListHas(gOrgSpec[by], t);
}

// Сколько спец-выговоров у аккаунта в его нынешней фракции
stock Org_SpecCount(dbid, f)
{
	new n = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM faction_swarns WHERE account = %d AND faction = %d", dbid, f);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) n = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return n;
}

// Сменил фракцию или уволен: спец-выговоры уходят вместе с обычными
stock Org_ClearSpec(dbid)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM faction_swarns WHERE account = %d", dbid));
}

// ---------------------------------------------------------------------
//  /swarn [id] [причина], /unswarn [id]
// ---------------------------------------------------------------------
stock bool:Cmd_SWarn(playerid, const params[])
{
	new f = Player[playerid][pFaction], pos = 0, reason[64], msg[144];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !RestParam(params, pos, reason))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /swarn [id] [причина] — спец-выговор сотруднику другой организации");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Неверный игрок.");
	new tf = Player[target][pFaction];
	if (!Org_CanSpecWarn(f, tf)) return SendClientMessage(playerid, COLOR_GREY, "Твоя организация не выдаёт спец-выговоры этой организации.");
	if (Player[playerid][pRank] < gOrgSpecRank[f]) return SendClientMessage(playerid, COLOR_GREY, "Спец-выговоры выдают с ранга %d.", gOrgSpecRank[f]);
	if (!Faction_IsGoodName(reason, 2, 60)) return SendClientMessage(playerid, COLOR_GREY, "Причина: от 2 до 60 символов, без фигурных скобок и знака процента.");

	new tdb = Player[target][pDbId];
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO faction_swarns (account, faction, by_faction, by_name, reason, date) VALUES (%d, %d, %d, '%q', '%q', %d)",
		tdb, tf, f, GetRPName(playerid), reason, gettime()));
	new warns = Faction_GetWarns(tdb) + 1;
	format(msg, sizeof msg, "%s (%s) выдал спец-выговор %s (%d/%d). Причина: %s", GetRPName(playerid), Faction_GetName(f), GetRPName(target), warns, FACTION_MAX_WARNS, reason);
	Faction_Log(tf, msg, FLOG_WARN);
	Faction_Log(f, msg, FLOG_WARN);
	if (warns >= FACTION_MAX_WARNS)
	{
		Faction_SetMember(target, FACTION_NONE, 0);
		SendClientMessage(target, COLOR_RED, "%s (%s) выдал тебе спец-выговор: %d/%d — ты уволен из «%s». Причина: %s",
			GetRPName(playerid), Faction_GetName(f), warns, FACTION_MAX_WARNS, Faction_GetName(tf), reason);
		format(msg, sizeof msg, "[Фракция] %s уволен: %d/%d выговоров (спец-выговор от %s).", GetRPName(target), warns, FACTION_MAX_WARNS, Faction_GetName(f));
		Faction_Message(tf, Faction_GetColor(tf), msg);
		Faction_Log(tf, msg[10], FLOG_STAFF);
		SendClientMessage(playerid, COLOR_WHITE, "Спец-выговор выдан. %s уволен (%d/%d).", GetRPName(target), warns, FACTION_MAX_WARNS);
		return true;
	}
	Faction_SetWarns(tdb, warns);
	SendClientMessage(target, COLOR_RED, "%s (%s) выдал тебе спец-выговор (%d/%d). Причина: %s. Снять его может только %s.",
		GetRPName(playerid), Faction_GetName(f), warns, FACTION_MAX_WARNS, reason, Faction_GetName(f));
	SendClientMessage(playerid, COLOR_WHITE, "Спец-выговор выдан (%d/%d).", warns, FACTION_MAX_WARNS);
	return true;
}

stock bool:Cmd_UnSWarn(playerid, const params[])
{
	new f = Player[playerid][pFaction], pos = 0, msg[144];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !Player[target][pLogged])
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /unswarn [id] — снять спец-выговор, который выдала твоя организация");
	new tf = Player[target][pFaction], tdb = Player[target][pDbId];
	new bool:admin = (Player[playerid][pAdmin] >= 4);
	if (!admin)
	{
		if (!Org_CanSpecWarn(f, tf)) return SendClientMessage(playerid, COLOR_GREY, "Твоя организация не выдаёт спец-выговоры этой организации.");
		if (Player[playerid][pRank] < gOrgSpecRank[f]) return SendClientMessage(playerid, COLOR_GREY, "Спец-выговоры снимают с ранга %d.", gOrgSpecRank[f]);
	}
	new id = 0;
	new DBResult:r;
	if (admin) r = DB_ExecuteQuery(gDB, "SELECT id FROM faction_swarns WHERE account = %d AND faction = %d ORDER BY id DESC LIMIT 1", tdb, tf);
	else r = DB_ExecuteQuery(gDB, "SELECT id FROM faction_swarns WHERE account = %d AND faction = %d AND by_faction = %d ORDER BY id DESC LIMIT 1", tdb, tf, f);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) id = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (id == 0) return SendClientMessage(playerid, COLOR_GREY, "У этого игрока нет спец-выговоров от твоей организации.");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM faction_swarns WHERE id = %d", id));
	new warns = Faction_GetWarns(tdb) - 1;
	if (warns < 0) warns = 0;
	Faction_SetWarns(tdb, warns);
	format(msg, sizeof msg, "%s снял спец-выговор с %s (%d/%d).", GetRPName(playerid), GetRPName(target), warns, FACTION_MAX_WARNS);
	Faction_Log(tf, msg, FLOG_WARN);
	if (Faction_IsValid(f) && f != tf) Faction_Log(f, msg, FLOG_WARN);
	SendClientMessage(target, COLOR_GREEN, "%s снял с тебя спец-выговор (%d/%d).", GetRPName(playerid), warns, FACTION_MAX_WARNS);
	SendClientMessage(playerid, COLOR_GREEN, "Спец-выговор снят (%d/%d).", warns, FACTION_MAX_WARNS);
	return true;
}

// ---------------------------------------------------------------------
//  /orgs — панель владельца
// ---------------------------------------------------------------------
stock bool:Cmd_Orgs(playerid)
{
	if (!Admin_Check(playerid, ORGS_ADMIN_LEVEL)) return true;
	new body[3200], line[112], n = 0;
	strcat(body, "№\tОрганизация\tНастроено\n");
	for (new f = 1; f < MAX_FACTIONS && n < ORG_LIST_MAX - 1; f++)
	{
		if (!Faction_IsValid(f)) continue;
		format(line, sizeof line, "%d\t{%06x}%s\t{AFAFAF}%s%s%s\n", f, Faction_GetColor(f) >>> 8, Faction_GetName(f),
			(gOrgSpec[f][0] != '\0') ? ("спец-выговоры ") : (""), (gOrgDisabled[f] != 0) ? ("отключения ") : (""), (gOrgDept[f] != ORG_DEPT_AUTO) ? ("/d") : (""));
		strcat(body, line);
		gOrgMap[playerid][n++] = f;
	}
	format(line, sizeof line, "»\t{FFFF00}Семьи\t{AFAFAF}%d шт.\n", FamAdm_Count());
	strcat(body, line);
	gOrgMap[playerid][n++] = 0;
	for (new i = n; i < ORG_LIST_MAX; i++) gOrgMap[playerid][i] = -1;
	ShowPlayerDialog(playerid, DIALOG_ORGS, DIALOG_STYLE_TABLIST_HEADERS, "Все организации", "%s", "Открыть", "Закрыть", body);
	return true;
}

static Org_ShowFaction(playerid)
{
	new f = gOrgSel[playerid];
	if (!Faction_IsValid(f)) { Cmd_Orgs(playerid); return; }
	new body[1000], line[192], spec[96];
	if (gOrgSpec[f][0] != '\0') strcat(spec, gOrgSpec[f]); else strcat(spec, "никому");
	format(line, sizeof line, "Информация\t{AFAFAF}состав %d, общак $%d\n", Faction_CountAll(f), Faction[f][fBank]);
	strcat(body, line);
	format(line, sizeof line, "Спец-выговоры: кому\t{FFFFFF}%s\n", spec);
	strcat(body, line);
	format(line, sizeof line, "Спец-выговоры: с ранга\t{FFFFFF}%d\n", gOrgSpecRank[f]);
	strcat(body, line);
	format(line, sizeof line, "Департамент (/d)\t{FFFFFF}%s\n",
		(gOrgDept[f] == ORG_DEPT_ON) ? ("всегда") : ((gOrgDept[f] == ORG_DEPT_OFF) ? ("никогда") : (Faction_IsState(f) ? ("по типу: да") : ("по типу: нет"))));
	strcat(body, line);
	format(line, sizeof line, "Отключённые возможности\t{FFFFFF}%d\n", FPerm_CountBits(gOrgDisabled[f]));
	strcat(body, line);
	strcat(body, "{FF6347}Сбросить настройки\t\n");
	new title[64];
	format(title, sizeof title, "%d. %s", f, Faction_GetName(f));
	ShowPlayerDialog(playerid, DIALOG_ORGS_F, DIALOG_STYLE_TABLIST, title, "%s", "Выбрать", "Назад", body);
}

static Org_ShowInfo(playerid)
{
	new f = gOrgSel[playerid];
	new text[1200], line[160], name[MAX_PLAYER_NAME], leaders[128];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT name FROM accounts WHERE faction = %d AND frank >= %d ORDER BY frank DESC LIMIT 4", f, MAX_RANK - 1);
	if (r != DBResult:0)
	{
		for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
		{
			DB_GetFieldString(r, 0, name, sizeof name);
			if (leaders[0] != '\0') strcat(leaders, ", ");
			strcat(leaders, name);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	if (leaders[0] == '\0') strcat(leaders, "нет");
	format(line, sizeof line, "{AFAFAF}Фракция: {FFFFFF}%d. %s\n{AFAFAF}Лидер и замы: {FFFFFF}%s\n", f, Faction_GetName(f), leaders);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Состав: {FFFFFF}%d\n{AFAFAF}Общак: {33AA33}$%d\n{AFAFAF}Материалы: {FFFFFF}%d\n", Faction_CountAll(f), Faction[f][fBank], Faction[f][fMats]);
	strcat(text, line);
	strcat(text, "\n{AFAFAF}Кто выдаёт спец-выговоры этой фракции: {FFFFFF}");
	new any = 0;
	for (new b = 1; b < MAX_FACTIONS; b++)
	{
		if (!Org_CanSpecWarn(b, f)) continue;
		format(line, sizeof line, "%s%s", any ? (", ") : (""), Faction_GetName(b));
		strcat(text, line);
		any = 1;
	}
	if (!any) strcat(text, "никто");
	ShowPlayerDialog(playerid, DIALOG_ORGS_INFO, DIALOG_STYLE_MSGBOX, "Информация", "%s", "Назад", "", text);
}

static Org_ShowDisabled(playerid)
{
	new f = gOrgSel[playerid], body[1400], line[96];
	strcat(body, "Возможность\tСостояние\n");
	for (new p = 0; p < FP_COUNT; p++)
	{
		format(line, sizeof line, "%s\t%s\n", FPerm_Name(p), ((gOrgDisabled[f] >> p) & 1) ? ("{FF6347}отключена") : ("{33AA33}работает"));
		strcat(body, line);
	}
	ShowPlayerDialog(playerid, DIALOG_ORGS_FDIS, DIALOG_STYLE_TABLIST_HEADERS, "Отключить для всей фракции (и лидера)", "%s", "Вкл/выкл", "Назад", body);
}

static Org_ShowFamilies(playerid)
{
	new body[3600], line[112], n = 0, name[32], disabled, db;
	strcat(body, "Семья\tОтключено\n");
	for (new s = 0, total = FamAdm_SlotCount(); s < total && n < ORG_LIST_MAX; s++)
	{
		if (!FamAdm_Slot(s, name, db, disabled)) continue;
		format(line, sizeof line, "%s\t%d\n", name, FPerm_CountBits(disabled));
		if (strlen(body) + strlen(line) >= sizeof body) break;
		strcat(body, line);
		gOrgMap[playerid][n++] = s;
	}
	for (new i = n; i < ORG_LIST_MAX; i++) gOrgMap[playerid][i] = -1;
	if (n == 0) { SendClientMessage(playerid, COLOR_GREY, "Семей пока нет."); Cmd_Orgs(playerid); return; }
	ShowPlayerDialog(playerid, DIALOG_ORGS_FAMS, DIALOG_STYLE_TABLIST_HEADERS, "Семьи", "%s", "Открыть", "Назад", body);
}

static Org_ShowFamDisabled(playerid)
{
	new s = -gOrgSel[playerid] - 1, name[32], db, disabled;
	if (!FamAdm_Slot(s, name, db, disabled)) { Org_ShowFamilies(playerid); return; }
	new body[1000], line[96];
	strcat(body, "Возможность\tСостояние\n");
	for (new p = 0, cnt = FamAdm_PermCount(); p < cnt; p++)
	{
		format(line, sizeof line, "%s\t%s\n", FamAdm_PermName(p), ((disabled >> p) & 1) ? ("{FF6347}отключена") : ("{33AA33}работает"));
		strcat(body, line);
	}
	ShowPlayerDialog(playerid, DIALOG_ORGS_FAMDIS, DIALOG_STYLE_TABLIST_HEADERS, name, "%s", "Вкл/выкл", "Назад", body);
}

stock Orgs_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid < DIALOG_ORGS || dialogid > DIALOG_ORGS_FAMDIS) return 0;
	if (Player[playerid][pAdmin] < ORGS_ADMIN_LEVEL) return 1;
	new f = gOrgSel[playerid];
	switch (dialogid)
	{
		case DIALOG_ORGS:
		{
			if (!response || listitem < 0 || listitem >= ORG_LIST_MAX || gOrgMap[playerid][listitem] == -1) return 1;
			if (gOrgMap[playerid][listitem] == 0) { Org_ShowFamilies(playerid); return 1; }
			gOrgSel[playerid] = gOrgMap[playerid][listitem];
			Org_ShowFaction(playerid);
		}
		case DIALOG_ORGS_F:
		{
			if (!response) { Cmd_Orgs(playerid); return 1; }
			if (!Faction_IsValid(f)) return 1;
			switch (listitem)
			{
				case 0: Org_ShowInfo(playerid);
				case 1: ShowPlayerDialog(playerid, DIALOG_ORGS_FSPEC, DIALOG_STYLE_INPUT, "Спец-выговоры: кому",
					"Номера фракций через запятую, которым «%s» выдаёт спец-выговоры.\nНапример: 1,7. Пусто или 0 — никому. Номера: /flist\n\nСейчас: %s",
					"Сохранить", "Назад", Faction_GetName(f), (gOrgSpec[f][0] != '\0') ? gOrgSpec[f] : ("никому"));
				case 2: ShowPlayerDialog(playerid, DIALOG_ORGS_FSRANK, DIALOG_STYLE_INPUT, "Спец-выговоры: с ранга",
					"С какого ранга «%s» выдаёт спец-выговоры (1-%d)?\nСейчас: %d", "Сохранить", "Назад", Faction_GetName(f), MAX_RANK, gOrgSpecRank[f]);
				case 3: ShowPlayerDialog(playerid, DIALOG_ORGS_FDEPT, DIALOG_STYLE_LIST, "Департамент (/d)",
					"По типу (госструктуры — да, банды — нет)\nВсегда видит /d\nНикогда не видит /d", "Выбрать", "Назад");
				case 4: Org_ShowDisabled(playerid);
				case 5:
				{
					Org_Reset(f);
					SendClientMessage(playerid, COLOR_ADMIN, "Настройки «%s» сброшены.", Faction_GetName(f));
					Org_ShowFaction(playerid);
				}
			}
		}
		case DIALOG_ORGS_INFO: Org_ShowFaction(playerid);
		case DIALOG_ORGS_FSPEC:
		{
			if (response && Faction_IsValid(f))
			{
				// оставляем только существующие фракции, без самой себя
				new out[96], part[8], v = 0, bool:num = false, bad = 0;
				for (new i = 0; ; i++)
				{
					if (inputtext[i] >= '0' && inputtext[i] <= '9') { if (v < 100000) v = v * 10 + (inputtext[i] - '0'); num = true; continue; }
					if (num && v != 0)
					{
						if (v != f && Faction_IsValid(v) && !Org_ListHas(out, v) && strlen(out) < 88)
						{
							if (out[0] != '\0') strcat(out, ",");
							valstr(part, v);
							strcat(out, part);
						}
						else bad++;
					}
					v = 0; num = false;
					if (inputtext[i] == '\0') break;
				}
				gOrgSpec[f][0] = '\0';
				strcat(gOrgSpec[f], out, 96);
				Org_Save(f);
				if (bad > 0) SendClientMessage(playerid, COLOR_GREY, "Пропущено номеров: %d (нет такой фракции или это она сама).", bad);
				SendClientMessage(playerid, COLOR_ADMIN, "«%s» выдаёт спец-выговоры: %s.", Faction_GetName(f), (out[0] != '\0') ? out : ("никому"));
			}
			Org_ShowFaction(playerid);
		}
		case DIALOG_ORGS_FSRANK:
		{
			new rk;
			if (response && Faction_IsValid(f))
			{
				if (!IsNumeric(inputtext) || (rk = strval(inputtext)) < 1 || rk > MAX_RANK) SendClientMessage(playerid, COLOR_GREY, "Ранг от 1 до %d.", MAX_RANK);
				else { gOrgSpecRank[f] = rk; Org_Save(f); }
			}
			Org_ShowFaction(playerid);
		}
		case DIALOG_ORGS_FDEPT:
		{
			if (response && Faction_IsValid(f) && listitem >= 0 && listitem <= 2) { gOrgDept[f] = listitem; Org_Save(f); }
			Org_ShowFaction(playerid);
		}
		case DIALOG_ORGS_FDIS:
		{
			if (!response) { Org_ShowFaction(playerid); return 1; }
			if (!Faction_IsValid(f) || listitem < 0 || listitem >= FP_COUNT) return 1;
			gOrgDisabled[f] ^= (1 << listitem);
			Org_Save(f);
			new msg[144];
			format(msg, sizeof msg, "Администрация %s для всей фракции: «%s».", ((gOrgDisabled[f] >> listitem) & 1) ? ("отключила") : ("включила"), FPerm_Name(listitem));
			Faction_Log(f, msg, FLOG_PERM);
			Org_ShowDisabled(playerid);
		}
		case DIALOG_ORGS_FAMS:
		{
			if (!response) { Cmd_Orgs(playerid); return 1; }
			if (listitem < 0 || listitem >= ORG_LIST_MAX || gOrgMap[playerid][listitem] == -1) return 1;
			gOrgSel[playerid] = -(gOrgMap[playerid][listitem] + 1);
			Org_ShowFamDisabled(playerid);
		}
		case DIALOG_ORGS_FAMDIS:
		{
			if (!response) { Org_ShowFamilies(playerid); return 1; }
			if (f >= 0) return 1;
			FamAdm_Toggle(-f - 1, listitem);
			Org_ShowFamDisabled(playerid);
		}
	}
	return 1;
}
