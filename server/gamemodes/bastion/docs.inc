// =====================================================================
//  docs.inc — документы в сумке: паспорт, медкарта, лицензии.
//
//  Документы лежат в сумке сами (needs.inc, Docs_Sync перед показом сумки):
//    паспорт — у всех с регистрации;
//    медкарта — после медосмотра в больнице или у медика (hospital.inc),
//      просроченная тоже лежит, но в ней написано «просрочена»;
//    лицензии — после первой лицензии (licenses.inc).
//  Документы нельзя выбросить или передать: «Использовать» — открыть
//  самому, кнопка «Передать» у них становится «Показать» (у игрока откроется окно).
//  Окно: HTML «document» в клиенте Bastion (index.html), иначе диалог.
//
//  Команды: /passport — свой паспорт; показать игроку рядом (откроется окно):
//  /showpass [id], /showmedcard [id], /showlic [id].
//
//  Паспорт новичку не дают сразу: его получают в мэрии у паспортного стола
//  (первый шаг квеста новичка, quest.inc). ALT у стола: игрок садится на стул,
//  камера снимает его лицо, в паспорт идёт скин на момент съёмки
//  (accounts.passport_skin). ПК-клиент bastion_ui.asi v4 по скрытому диалогу
//  "[bastion:photo]" сам снимает кадр в bastion_ui\ui\doc\skins\<скин>.bmp,
//  без него в паспорте общее фото (ui/doc/photo_m.png, photo_f.png).
//  Пересъёмка с паспортом — PASS_RETAKE_PRICE. Стол переносится через /markers.
// =====================================================================

#define DOC_SHOW_RANGE      3.0

#define PASS_DESK_X         1477.6      // паспортный стол — у мэрии, рядом с автошколой
#define PASS_DESK_Y         -1771.3
#define PASS_DESK_Z         18.8
#define PASS_DESK_A         180.0       // куда смотрит игрок на стуле (камера встаёт перед ним)
#define PASS_RETAKE_PRICE   500
#define PASS_CHAIR_MODEL    1714        // офисное кресло
#define PASS_PHOTO_PC_VER   4           // bastion_ui.asi с поддержкой "[bastion:photo]"

static gDocRegDate[MAX_PLAYERS];
static bool:gDocPass[MAX_PLAYERS];      // паспорт получен
static gDocPassSkin[MAX_PLAYERS];       // скин на фото, -1 — фото нет (старый паспорт)
static gDocPassDate[MAX_PLAYERS];       // дата выдачи (съёмки)
static bool:gDocFresh[MAX_PLAYERS];     // только что зарегистрировался: паспорта нет
static gPassMk = -1;
static gPhotoStage[MAX_PLAYERS];        // 0 — нет съёмки, 1 — сидит, 2 — снято
static gPhotoChair[MAX_PLAYERS] = { INVALID_OBJECT_ID, ... };
static PlayerText:gPhotoFlash[MAX_PLAYERS] = { PlayerText:INVALID_PLAYER_TEXT_DRAW, ... };
static bool:gPhotoPaid[MAX_PLAYERS];

// Женские скины GTA SA (для графы «Пол» в паспорте)
static const gDocFemaleSkins[] = {
	9, 10, 11, 12, 13, 31, 38, 39, 40, 41, 53, 54, 55, 56, 63, 64, 65, 69, 75, 76, 77, 85, 87, 88, 89, 90, 91, 92, 93,
	129, 130, 131, 138, 139, 140, 141, 145, 148, 150, 151, 152, 157, 169, 172, 178, 190, 191, 192, 193, 194, 195, 196,
	197, 198, 199, 201, 205, 207, 211, 214, 215, 216, 218, 219, 224, 225, 226, 231, 232, 233, 237, 238, 243, 244, 245,
	246, 251, 256, 257, 263, 298, 306, 307, 308, 309
};

// Из OnGameModeInit
stock Docs_Init()
{
	// у старых аккаунтов паспорт уже есть (DEFAULT 1), новичкам ставим 0 при регистрации
	DB_AddColumn("accounts", "passport", "INTEGER DEFAULT 1");
	DB_AddColumn("accounts", "passport_skin", "INTEGER DEFAULT -1");
	DB_AddColumn("accounts", "passport_date", "INTEGER DEFAULT 0");
	new Float:x = PASS_DESK_X, Float:y = PASS_DESK_Y, Float:z = PASS_DESK_Z;
	gPassMk = Marker_Create(MK_SCHOOL, 1, "Паспортный стол", 0, x, y, z, 0, 0, GPS_GOV, PASS_DESK_A);
	Marker_SetLabel(gPassMk, "{33CCFF}Паспортный стол\n{FFFFFF}ALT — сфотографироваться на паспорт");
}

// После выбора пола при регистрации (accounts.inc), до Account_FinishLogin
stock Docs_OnRegister(playerid)
{
	gDocFresh[playerid] = true;
}

stock Docs_OnDisconnect(playerid)
{
	Docs_PhotoCleanup(playerid);
	gDocFresh[playerid] = false;
}

stock bool:Docs_HasPassport(playerid) { return gDocPass[playerid]; }
stock Docs_PassDeskMarker() { return gPassMk; }
stock bool:Docs_IsNearPassDesk(playerid) { return Marker_InRange(playerid, gPassMk, 3.0) != 0; }

// После входа (Account_FinishLogin)
stock Docs_LoadPlayer(playerid)
{
	gDocRegDate[playerid] = 0;
	gDocPass[playerid] = true;
	gDocPassSkin[playerid] = -1;
	gDocPassDate[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT reg_date, passport, passport_skin, passport_date FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			gDocRegDate[playerid] = DB_GetFieldInt(r, 0);
			gDocPass[playerid] = DB_GetFieldInt(r, 1) != 0;
			gDocPassSkin[playerid] = DB_GetFieldInt(r, 2);
			gDocPassDate[playerid] = DB_GetFieldInt(r, 3);
		}
		DB_FreeResultSet(r);
	}
	if (gDocFresh[playerid])
	{
		// новичок: паспорт получит в мэрии (квест новичка)
		gDocFresh[playerid] = false;
		gDocPass[playerid] = false;
		gDocPassSkin[playerid] = -1;
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET passport = 0, passport_skin = -1 WHERE id = %d", Player[playerid][pDbId]));
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

// Есть ли документ (для меню ALT)
stock Docs_Has(playerid, item)
{
	switch (item)
	{
		case INV_PASSPORT: return Player[playerid][pDbId] != 0 && gDocPass[playerid];
		case INV_MEDCARD: return Hospital_MedcardUntil(playerid) > 0;
		case INV_LICENSES: return Docs_HasAnyLicense(playerid);
		case INV_SHOOTING: return Skills_RecordDate(playerid) > 0;
	}
	return 0;
}

// Какие документы лежат в сумке. Перед каждым показом сумки (needs.inc).
stock Docs_Sync(playerid)
{
	Inv_SetDoc(playerid, INV_PASSPORT, Player[playerid][pDbId] != 0 && gDocPass[playerid]);
	Inv_SetDoc(playerid, INV_MEDCARD, Hospital_MedcardUntil(playerid) > 0);
	Inv_SetDoc(playerid, INV_LICENSES, Docs_HasAnyLicense(playerid));
	Inv_SetDoc(playerid, INV_SHOOTING, Skills_RecordDate(playerid) > 0);
}

static bool:Docs_IsFemale(skin)
{
	for (new i = 0; i < sizeof gDocFemaleSkins; i++) if (gDocFemaleSkins[i] == skin) return true;
	return false;
}

// Дата из gettime(): "22.11.2026"
stock Docs_Date(dest[], size, stamp)
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
	new num[16], issued[16], e[5][64];
	Docs_Number(owner, num, sizeof num);
	Docs_Date(issued, sizeof issued, gDocRegDate[owner]);
	new spouse[MAX_PLAYER_NAME];
	spouse = Marriage_GetSpouseName(owner);
	for (new i = 0; spouse[i]; i++) if (spouse[i] == '_') spouse[i] = ' ';
	new photo = gDocPassSkin[owner];
	new bool:female = Docs_IsFemale(photo >= 0 ? photo : Player[owner][pSkin]);
	new marital[MAX_PLAYER_NAME + 16];
	if (spouse[0]) format(marital, sizeof marital, "%s %s", female ? ("замужем за") : ("женат на"), spouse);
	else strcat(marital, female ? ("не замужем") : ("не женат"));
	// прописка: свой дом, иначе снимаемая комната
	new home[40], h = House_GetOwned(owner);
	if (h != -1) format(home, sizeof home, "Лос-Сантос, дом №%d", House[h][hDbId]);
	else if ((h = House_GetRented(owner)) != -1) format(home, sizeof home, "Лос-Сантос, дом №%d (аренда)", House[h][hDbId]);
	else strcat(home, "без регистрации");
	// место работы: фракция с должностью, иначе работа
	new work[80];
	if (Player[owner][pFaction]) format(work, sizeof work, "%s, %s", Faction_GetName(Player[owner][pFaction]), Faction_GetRankName(Player[owner][pFaction], Player[owner][pRank]));
	else if (Player[owner][pJob]) format(work, sizeof work, "%s", Job_GetName(Player[owner][pJob]));
	else strcat(work, "не трудоустроен");

	if (UI_Has(viewer))
	{
		new json[900];
		UI_JsonEscape(e[0], GetRPName(owner));
		UI_JsonEscape(e[1], work);
		UI_JsonEscape(e[2], home);
		UI_JsonEscape(e[3], marital);
		format(json, sizeof json,
			"{\"kind\":\"passport\",\"own\":%d,\"name\":\"%s\",\"num\":\"%s\",\"female\":%d,\"issued\":\"%s\",\"years\":%d,\"work\":\"%s\",\"home\":\"%s\",\"marital\":\"%s\",\"skin\":%d,\"photo\":%d}",
			owner == viewer, e[0], num, female, issued, Player[owner][pLevel], e[1], e[2], e[3], photo, gDocPassDate[owner]);
		UI_Show(viewer, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "document", json, "Закрыть");
		return;
	}
	ShowPlayerDialog(viewer, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "{FF7A1A}BASTION {FFFFFF}· Паспорт",
		"{8C9098}ШТАТ SAN ANDREAS · ПАСПОРТ ГРАЖДАНИНА\n\n{8C9098}Номер: {FFFFFF}%s\n{8C9098}Фамилия, имя: {FFFFFF}%s\n{8C9098}Пол: {FFFFFF}%s\n{8C9098}Гражданин штата с: {FFFFFF}%s\n{8C9098}Лет в штате: {FFFFFF}%d\n{8C9098}Прописка: {FFFFFF}%s\n{8C9098}Семейное положение: {FFFFFF}%s\n{8C9098}Место работы: {FFFFFF}%s",
		"Закрыть", "",
		num, GetRPName(owner), female ? "женский" : "мужской", issued, Player[owner][pLevel], home, marital, work);
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

// Выписка из тира Ammu-Nation (/skills, skills.inc): навыки оружия в процентах
stock Docs_ShowShooting(owner, viewer)
{
	new num[16], today[16];
	Docs_Number(owner, num, sizeof num);
	Docs_Date(today, sizeof today, Skills_RecordDate(owner));
	if (UI_Has(viewer))
	{
		new json[700], name[64], list[400];
		UI_JsonEscape(name, GetRPName(owner));
		Skills_Json(owner, list);
		format(json, sizeof json, "{\"kind\":\"shooting\",\"own\":%d,\"name\":\"%s\",\"num\":\"%s\",\"date\":\"%s\",\"skin\":%d%s}",
			owner == viewer, name, num, today, Player[owner][pSkin], list);
		UI_Show(viewer, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "document", json, "Закрыть");
		return;
	}
	new text[700];
	Skills_Text(owner, text);
	ShowPlayerDialog(viewer, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "{FF7A1A}BASTION {FFFFFF}· Выписка из тира",
		"{8C9098}AMMU-NATION · ВЫПИСКА ИЗ ТИРА\n\n{8C9098}Номер: {FFFFFF}%s\n{8C9098}Стрелок: {FFFFFF}%s\n{8C9098}Дата: {FFFFFF}%s\n%s",
		"Закрыть", "", num, GetRPName(owner), today, text);
}

static Docs_ShowItem(owner, viewer, item)
{
	switch (item)
	{
		case INV_PASSPORT: Docs_ShowPassport(owner, viewer);
		case INV_MEDCARD: Docs_ShowMedcard(owner, viewer);
		case INV_LICENSES: Docs_ShowLicenses(owner, viewer);
		case INV_SHOOTING: Docs_ShowShooting(owner, viewer);
	}
}

static Docs_Title(item, dest[], size)
{
	switch (item)
	{
		case INV_PASSPORT: format(dest, size, "паспорт");
		case INV_MEDCARD: format(dest, size, "медкарту");
		case INV_SHOOTING: format(dest, size, "выписку из тира");
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
	else if (item == INV_SHOOTING) format(msg, sizeof msg, "%s достаёт выписку из тира и смотрит в неё.", GetRPName(playerid));
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
	if (!Docs_Has(playerid, INV_PASSPORT))
	{
		SendClientMessage(playerid, COLOR_GREY, "У тебя ещё нет паспорта. Его выдают в мэрии у паспортного стола (/quest, /gps).");
		return true;
	}
	Docs_Use(playerid, INV_PASSPORT);
	return true;
}

// ---------------------------------------------------------------------
//  Паспортный стол: съёмка на паспорт
// ---------------------------------------------------------------------
stock Docs_PassDesk(playerid)
{
	if (gPhotoStage[playerid]) return 1;
	if (IsPlayerInAnyVehicle(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Выйди из машины.");
	if (gDocPass[playerid])
	{
		ShowPlayerDialog(playerid, DIALOG_PASS_RETAKE, DIALOG_STYLE_MSGBOX, "Паспортный стол",
			"{FFFFFF}Паспорт у тебя уже есть.\nПереснять фото (например, после смены одежды) стоит {33AA33}$%d{FFFFFF}.", "Переснять", "Отмена", PASS_RETAKE_PRICE);
		return 1;
	}
	Docs_PhotoStart(playerid, false);
	return 1;
}

stock Docs_OnDialog(playerid, dialogid, response)
{
	if (dialogid != DIALOG_PASS_RETAKE) return 0;
	if (!response || !Docs_IsNearPassDesk(playerid) || gPhotoStage[playerid]) return 1;
	if (GetMoney(playerid) < PASS_RETAKE_PRICE)
	{
		SendClientMessage(playerid, COLOR_GREY, "Не хватает денег: нужно $%d.", PASS_RETAKE_PRICE);
		return 1;
	}
	Docs_PhotoStart(playerid, true);
	return 1;
}

// голова сидящего игрока и точка камеры перед ним
static Docs_PhotoHead(&Float:hx, &Float:hy, &Float:hz, &Float:cx, &Float:cy)
{
	new m = gPassMk;
	new Float:a = gMarker[m][mkA];
	hx = gMarker[m][mkX];
	hy = gMarker[m][mkY];
	hz = gMarker[m][mkZ] + 0.42;
	cx = hx - 1.15 * floatsin(a, degrees);
	cy = hy + 1.15 * floatcos(a, degrees);
}

static Docs_PhotoStart(playerid, bool:paid)
{
	if (!Marker_IsValid(gPassMk)) return;
	new m = gPassMk;
	new Float:x = gMarker[m][mkX], Float:y = gMarker[m][mkY], Float:z = gMarker[m][mkZ], Float:a = gMarker[m][mkA];
	gPhotoStage[playerid] = 1;
	gPhotoPaid[playerid] = paid;
	TogglePlayerControllable(playerid, false);
	SetPlayerPos(playerid, x, y, z);
	SetPlayerFacingAngle(playerid, a);
	gPhotoChair[playerid] = CreatePlayerObject(playerid, PASS_CHAIR_MODEL, x, y, z - 0.95, 0.0, 0.0, a + 180.0);
	ApplyAnimation(playerid, "PED", "SEAT_idle", 4.1, true, false, false, true, 0, SYNC_ALL);
	new Float:hx, Float:hy, Float:hz, Float:cx, Float:cy;
	Docs_PhotoHead(hx, hy, hz, cx, cy);
	SetPlayerCameraPos(playerid, cx, cy, hz + 0.05);
	SetPlayerCameraLookAt(playerid, hx, hy, hz, CAMERA_CUT);
	SendClientMessage(playerid, 0x33CCFFFF, "Фотограф: «Садись, смотри прямо в камеру... Не моргай!»");
	new msg[96];
	format(msg, sizeof msg, "%s садится перед фотографом паспортного стола.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SetTimerEx("Docs_PhotoShot", 2500, false, "i", playerid);
}

forward Docs_PhotoShot(playerid);
public Docs_PhotoShot(playerid)
{
	if (!IsPlayerConnected(playerid) || gPhotoStage[playerid] != 1) return 0;
	gPhotoStage[playerid] = 2;
	// ПК-клиент снимает кадр до вспышки (скрытый диалог, как "[hud]": id панели HUD, чтобы не сбить окна)
	if (!IsPlayerMobile[playerid] && gUiPcVersion[playerid] >= PASS_PHOTO_PC_VER)
		ShowPlayerDialog(playerid, HUD_DIALOG_ID, DIALOG_STYLE_MSGBOX, "[bastion:photo]", "%d", "OK", "", GetPlayerSkin(playerid));
	PlayerPlaySound(playerid, 1132, 0.0, 0.0, 0.0);
	SetTimerEx("Docs_PhotoFlash", 250, false, "i", playerid);
	SetTimerEx("Docs_PhotoDone", 1400, false, "i", playerid);
	return 1;
}

forward Docs_PhotoFlash(playerid);
public Docs_PhotoFlash(playerid)
{
	if (!IsPlayerConnected(playerid) || gPhotoStage[playerid] != 2) return 0;
	if (gPhotoFlash[playerid] == PlayerText:INVALID_PLAYER_TEXT_DRAW)
	{
		gPhotoFlash[playerid] = CreatePlayerTextDraw(playerid, -10.0, -10.0, "_");
		PlayerTextDrawLetterSize(playerid, gPhotoFlash[playerid], 0.0, 55.0);
		PlayerTextDrawTextSize(playerid, gPhotoFlash[playerid], 660.0, 0.0);
		PlayerTextDrawUseBox(playerid, gPhotoFlash[playerid], true);
		PlayerTextDrawBoxColour(playerid, gPhotoFlash[playerid], 0xFFFFFFDD);
	}
	PlayerTextDrawShow(playerid, gPhotoFlash[playerid]);
	return 1;
}

static Docs_PhotoCleanup(playerid)
{
	if (gPhotoFlash[playerid] != PlayerText:INVALID_PLAYER_TEXT_DRAW)
	{
		PlayerTextDrawDestroy(playerid, gPhotoFlash[playerid]);
		gPhotoFlash[playerid] = PlayerText:INVALID_PLAYER_TEXT_DRAW;
	}
	if (gPhotoChair[playerid] != INVALID_OBJECT_ID)
	{
		DestroyPlayerObject(playerid, gPhotoChair[playerid]);
		gPhotoChair[playerid] = INVALID_OBJECT_ID;
	}
	gPhotoStage[playerid] = 0;
}

forward Docs_PhotoDone(playerid);
public Docs_PhotoDone(playerid)
{
	if (!IsPlayerConnected(playerid) || gPhotoStage[playerid] != 2) return 0;
	Docs_PhotoCleanup(playerid);
	ClearAnimations(playerid, SYNC_ALL);
	SetCameraBehindPlayer(playerid);
	TogglePlayerControllable(playerid, true);
	if (gPhotoPaid[playerid])
	{
		if (GetMoney(playerid) < PASS_RETAKE_PRICE) return SendClientMessage(playerid, COLOR_GREY, "Не хватает денег на пересъёмку.");
		GiveMoney(playerid, -PASS_RETAKE_PRICE, "пересъёмка фото на паспорт");
	}
	new bool:first = !gDocPass[playerid];
	gDocPass[playerid] = true;
	gDocPassSkin[playerid] = GetPlayerSkin(playerid);
	gDocPassDate[playerid] = gettime();
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET passport = 1, passport_skin = %d, passport_date = %d WHERE id = %d",
		gDocPassSkin[playerid], gDocPassDate[playerid], Player[playerid][pDbId]));
	GameTextForPlayer(playerid, "~w~Passport", 2000, 1);
	if (first) SendClientMessage(playerid, COLOR_GREEN, "Паспорт готов! Он лежит в сумке (/inv), посмотреть: /passport, показать: /showpass [id].");
	else SendClientMessage(playerid, COLOR_GREEN, "Фото в паспорте обновлено ($%d). Посмотреть: /passport.", PASS_RETAKE_PRICE);
#if defined Quest_OnPassport
	Quest_OnPassport(playerid);     // квест новичка (quest.inc)
#endif
	return 1;
}

static Docs_CmdShow(playerid, item, const params[], const usage[])
{
	new pos = 0, why[96];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: %s [id]", usage);
	if (!Docs_Has(playerid, item))
		return SendClientMessage(playerid, COLOR_GREY, item == INV_PASSPORT ? ("У тебя нет паспорта, его выдают в мэрии.") : item == INV_MEDCARD ? ("У тебя нет медкарты, её дают в больнице.") : ("У тебя нет лицензий."));
	if (!Docs_ShowTo(playerid, item, target, why)) SendClientMessage(playerid, COLOR_GREY, "%s", why);
	return 1;
}

stock bool:Docs_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/passport", true) || !strcmp(cmd, "/pass", true)) { Cmd_Passport(playerid); return true; }
	if (!strcmp(cmd, "/showpass", true))    { Docs_CmdShow(playerid, INV_PASSPORT, params, cmd); return true; }
	if (!strcmp(cmd, "/showmedcard", true)) { Docs_CmdShow(playerid, INV_MEDCARD, params, cmd); return true; }
	if (!strcmp(cmd, "/showlic", true))     { Docs_CmdShow(playerid, INV_LICENSES, params, cmd); return true; }
	return false;
}
