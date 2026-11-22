// =====================================================================
//  docs.inc — документы в сумке: паспорт, медкарта, лицензии.
//
//  Документы лежат в сумке сами (needs.inc, Docs_Sync перед показом сумки):
//    паспорт — у всех с регистрации;
//    медкарта — после медосмотра в больнице или у медика (hospital.inc),
//      просроченная тоже лежит, но в ней написано «просрочена»;
//    лицензии — после первой лицензии (licenses.inc).
//  Документы нельзя выбросить или передать: «Использовать» — открыть
//  самому, «Передать» — показать игроку рядом (у него откроется окно).
//  Окно: HTML «document» в клиенте Bastion (index.html), иначе диалог.
//
//  Команды: /passport — свой паспорт, /showpass [id] — показать паспорт.
//  Медкарта и лицензии: /medcard, /showmedcard, /licenses, /showlic (как раньше).
// =====================================================================

#define DOC_SHOW_RANGE      3.0

static gDocRegDate[MAX_PLAYERS];

// Женские скины GTA SA (для графы «Пол» в паспорте)
static const gDocFemaleSkins[] = {
	9, 10, 11, 12, 13, 31, 38, 39, 40, 41, 53, 54, 55, 56, 63, 64, 65, 69, 75, 76, 77, 85, 87, 88, 89, 90, 91, 92, 93,
	129, 130, 131, 138, 139, 140, 141, 145, 148, 150, 151, 152, 157, 169, 172, 178, 190, 191, 192, 193, 194, 195, 196,
	197, 198, 199, 201, 205, 207, 211, 214, 215, 216, 218, 219, 224, 225, 226, 231, 232, 233, 237, 238, 243, 244, 245,
	246, 251, 256, 257, 263, 298, 306, 307, 308, 309
};

// После входа (Account_FinishLogin)
stock Docs_LoadPlayer(playerid)
{
	gDocRegDate[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT reg_date FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) gDocRegDate[playerid] = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	// старые аккаунты без даты регистрации: паспорт выдан сегодня
	if (gDocRegDate[playerid] <= 0)
	{
		gDocRegDate[playerid] = gettime();
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET reg_date = %d WHERE id = %d", gDocRegDate[playerid], Player[playerid][pDbId]));
	}
}

static bool:Docs_HasAnyLicense(playerid)
{
	for (new l = 1; l < LIC_COUNT; l++) if (Licenses_Has(playerid, l)) return true;
	return false;
}

// Какие документы лежат в сумке. Перед каждым показом сумки (needs.inc).
stock Docs_Sync(playerid)
{
	Inv_SetDoc(playerid, INV_PASSPORT, Player[playerid][pDbId] != 0);
	Inv_SetDoc(playerid, INV_MEDCARD, Hospital_MedcardUntil(playerid) > 0);
	Inv_SetDoc(playerid, INV_LICENSES, Docs_HasAnyLicense(playerid));
}

static bool:Docs_IsFemale(skin)
{
	for (new i = 0; i < sizeof gDocFemaleSkins; i++) if (gDocFemaleSkins[i] == skin) return true;
	return false;
}

// Дата из gettime(): "22.11.2026"
static Docs_Date(dest[], size, stamp)
{
	if (stamp <= 0) { format(dest, size, "—"); return; }
	// дни с 1970-01-01 в год/месяц/день (алгоритм civil_from_days)
	new z = stamp / 86400 + 719468;
	new era = z / 146097;
	new doe = z - era * 146097;
	new yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	new y = yoe + era * 400;
	new doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	new mp = (5 * doy + 2) / 153;
	new d = doy - (153 * mp + 2) / 5 + 1;
	new m = mp < 10 ? mp + 3 : mp - 9;
	if (m <= 2) y++;
	format(dest, size, "%02d.%02d.%d", d, m, y);
}

static Docs_Number(playerid, dest[], size)
{
	format(dest, size, "BS %06d", Player[playerid][pDbId]);
}

static Docs_DaysLeft(until)
{
	new left = until - gettime();
	if (left <= 0) return 0;
	return left / 86400 + 1;
}

// ---------------------------------------------------------------------
//  Окна документов. owner — чей документ, viewer — кто смотрит.
// ---------------------------------------------------------------------
static Docs_ShowPassport(owner, viewer)
{
	new num[16], issued[16], phone[16], e[6][64];
	Docs_Number(owner, num, sizeof num);
	Docs_Date(issued, sizeof issued, gDocRegDate[owner]);
	if (Player[owner][pPhone] == 0) strcat(phone, "нет");
	else format(phone, sizeof phone, "%d", Player[owner][pPhone]);
	new spouse[MAX_PLAYER_NAME];
	spouse = Marriage_GetSpouseName(owner);
	for (new i = 0; spouse[i]; i++) if (spouse[i] == '_') spouse[i] = ' ';
	new bool:female = Docs_IsFemale(Player[owner][pSkin]);
	new marital[MAX_PLAYER_NAME + 16];
	if (spouse[0]) format(marital, sizeof marital, "%s %s", female ? ("замужем за") : ("женат на"), spouse);
	else strcat(marital, female ? ("не замужем") : ("не женат"));

	if (UI_Has(viewer))
	{
		new json[900];
		UI_JsonEscape(e[0], GetRPName(owner));
		UI_JsonEscape(e[1], Job_GetName(Player[owner][pJob]));
		UI_JsonEscape(e[2], Faction_GetName(Player[owner][pFaction]));
		if (Player[owner][pFaction]) UI_JsonEscape(e[3], Faction_GetRankName(Player[owner][pFaction], Player[owner][pRank]));
		UI_JsonEscape(e[4], spouse);
		format(json, sizeof json,
			"{\"kind\":\"passport\",\"own\":%d,\"name\":\"%s\",\"num\":\"%s\",\"female\":%d,\"issued\":\"%s\",\"level\":%d,\"job\":\"%s\",\"faction\":\"%s\",\"rank\":\"%s\",\"spouse\":\"%s\",\"phone\":\"%s\",\"skin\":%d}",
			owner == viewer, e[0], num, female, issued, Player[owner][pLevel], e[1], e[2],
			e[3], e[4], phone, Player[owner][pSkin]);
		UI_Show(viewer, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "document", json, "Закрыть");
		return;
	}
	new faction[80];
	if (Player[owner][pFaction]) format(faction, sizeof faction, "%s (%s)", Faction_GetName(Player[owner][pFaction]), Faction_GetRankName(Player[owner][pFaction], Player[owner][pRank]));
	else strcat(faction, "нет");
	ShowPlayerDialog(viewer, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "{FF7A1A}BASTION {FFFFFF}· Паспорт",
		"{8C9098}ШТАТ SAN ANDREAS · ПАСПОРТ ГРАЖДАНИНА\n\n{8C9098}Номер: {FFFFFF}%s\n{8C9098}Имя: {FFFFFF}%s\n{8C9098}Пол: {FFFFFF}%s\n{8C9098}Выдан: {FFFFFF}%s, мэрия Лос-Сантоса\n{8C9098}Лет в штате (уровень): {FFFFFF}%d\n{8C9098}Работа: {FFFFFF}%s\n{8C9098}Фракция: {FFFFFF}%s\n{8C9098}Семейное положение: {FFFFFF}%s\n{8C9098}Телефон: {FFFFFF}%s",
		"Закрыть", "",
		num, GetRPName(owner), female ? "женский" : "мужской", issued, Player[owner][pLevel],
		Job_GetName(Player[owner][pJob]), faction, marital, phone);
}

static Docs_ShowMedcard(owner, viewer)
{
	new num[16], until[16], ins[16];
	Docs_Number(owner, num, sizeof num);
	new mu = Hospital_MedcardUntil(owner), iu = Hospital_InsuredUntil(owner);
	Docs_Date(until, sizeof until, mu);
	Docs_Date(ins, sizeof ins, iu);
	new bool:valid = mu > gettime(), bool:insured = iu > gettime();
	new addict = Drugs_GetAddict(owner);

	if (UI_Has(viewer))
	{
		new json[600], name[64];
		UI_JsonEscape(name, GetRPName(owner));
		format(json, sizeof json,
			"{\"kind\":\"medcard\",\"own\":%d,\"name\":\"%s\",\"num\":\"%s\",\"valid\":%d,\"until\":\"%s\",\"days\":%d,\"insured\":%d,\"insUntil\":\"%s\",\"addict\":%d,\"skin\":%d}",
			owner == viewer, name, num, valid, until, Docs_DaysLeft(mu), insured, ins, addict > 0, Player[owner][pSkin]);
		UI_Show(viewer, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "document", json, "Закрыть");
		return;
	}
	new concl[64], insline[48];
	if (valid) format(concl, sizeof concl, "{4CD07D}годен{FFFFFF}, до %s (ещё %d дн.)", until, Docs_DaysLeft(mu));
	else format(concl, sizeof concl, "{E5484D}просрочена{FFFFFF} (%s)", until);
	if (insured) format(insline, sizeof insline, "до %s", ins);
	else strcat(insline, "нет");
	ShowPlayerDialog(viewer, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "{FF7A1A}BASTION {FFFFFF}· Медкарта",
		"{8C9098}БОЛЬНИЦА ЛОС-САНТОСА · МЕДИЦИНСКАЯ КАРТА\n\n{8C9098}Номер: {FFFFFF}%s\n{8C9098}Пациент: {FFFFFF}%s\n{8C9098}Заключение: {FFFFFF}%s\n{8C9098}Страховка: {FFFFFF}%s\n{8C9098}Зависимость: {FFFFFF}%s",
		"Закрыть", "", num, GetRPName(owner), concl, insline, addict > 0 ? ("{E5484D}есть") : ("нет"));
}

static Docs_ShowLicenses(owner, viewer)
{
	new num[16];
	Docs_Number(owner, num, sizeof num);
	if (UI_Has(viewer))
	{
		new json[400], name[64];
		UI_JsonEscape(name, GetRPName(owner));
		format(json, sizeof json, "{\"kind\":\"licenses\",\"own\":%d,\"name\":\"%s\",\"num\":\"%s\",\"lic\":[%d,%d,%d,%d],\"skin\":%d}",
			owner == viewer, name, num, Licenses_Has(owner, LIC_CAR), Licenses_Has(owner, LIC_MOTO), Licenses_Has(owner, LIC_BOAT), Licenses_Has(owner, LIC_GUN), Player[owner][pSkin]);
		UI_Show(viewer, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "document", json, "Закрыть");
		return;
	}
	new yes[16] = "{4CD07D}есть", no[16] = "{E5484D}нет";
	ShowPlayerDialog(viewer, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "{FF7A1A}BASTION {FFFFFF}· Лицензии",
		"{8C9098}ШТАТ SAN ANDREAS · ЛИЦЕНЗИИ\n\n{8C9098}Номер: {FFFFFF}%s\n{8C9098}Владелец: {FFFFFF}%s\n\n{8C9098}Авто: %s\n{8C9098}Мото: %s\n{8C9098}Лодки: %s\n{8C9098}Оружие: %s",
		"Закрыть", "", num, GetRPName(owner),
		Licenses_Has(owner, LIC_CAR) ? yes : no, Licenses_Has(owner, LIC_MOTO) ? yes : no,
		Licenses_Has(owner, LIC_BOAT) ? yes : no, Licenses_Has(owner, LIC_GUN) ? yes : no);
}

static Docs_ShowItem(owner, viewer, item)
{
	switch (item)
	{
		case INV_PASSPORT: Docs_ShowPassport(owner, viewer);
		case INV_MEDCARD: Docs_ShowMedcard(owner, viewer);
		case INV_LICENSES: Docs_ShowLicenses(owner, viewer);
	}
}

static Docs_Title(item, dest[], size)
{
	switch (item)
	{
		case INV_PASSPORT: format(dest, size, "паспорт");
		case INV_MEDCARD: format(dest, size, "медкарту");
		default: format(dest, size, "лицензии");
	}
}

// Сумка > документ > «Использовать»: посмотреть свой (needs.inc)
stock Docs_Use(playerid, item)
{
	Docs_ShowItem(playerid, playerid, item);
	new what[16], msg[96];
	Docs_Title(item, what, sizeof what);
	format(msg, sizeof msg, "%s достаёт %s и смотрит в него.", GetRPName(playerid), what);
	if (item == INV_LICENSES) format(msg, sizeof msg, "%s достаёт лицензии и смотрит в них.", GetRPName(playerid));
	else if (item == INV_MEDCARD) format(msg, sizeof msg, "%s достаёт медкарту и смотрит в неё.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
}

// Показать документ игроку рядом. false и причина в why, если нельзя.
stock Docs_ShowTo(playerid, item, target, why[], whySize = sizeof why)
{
	if (target == INVALID_PLAYER_ID || target == playerid || !Player[target][pLogged])
	{
		format(why, whySize, "Нужно ввести id игрока рядом, например: 5");
		return 0;
	}
	if (!IsPlayerNearPlayer(playerid, target, DOC_SHOW_RANGE))
	{
		format(why, whySize, "Подойди ближе к игроку.");
		return 0;
	}
	Docs_ShowItem(playerid, target, item);
	new what[16], msg[128];
	Docs_Title(item, what, sizeof what);
	format(msg, sizeof msg, "%s показывает %s %s.", GetRPName(playerid), what, GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return 1;
}

stock bool:Cmd_Passport(playerid)
{
	Docs_Use(playerid, INV_PASSPORT);
	return true;
}

stock bool:Cmd_ShowPass(playerid, const params[])
{
	new pos = 0, why[96];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /showpass [id]");
	if (!Docs_ShowTo(playerid, INV_PASSPORT, target, why)) SendClientMessage(playerid, COLOR_GREY, "%s", why);
	return true;
}

stock bool:Docs_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/passport", true) || !strcmp(cmd, "/pass", true)) { Cmd_Passport(playerid); return true; }
	if (!strcmp(cmd, "/showpass", true))  { Cmd_ShowPass(playerid, params); return true; }
	return false;
}
