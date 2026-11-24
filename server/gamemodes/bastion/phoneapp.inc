// =====================================================================
//  phoneapp.inc — телефон с приложениями (HTML-окно "phone", ui.inc).
//
//  /phone или N (когда нет звонка и предложения) — достать телефон.
//  Приложения: Звонки, SMS, Контакты, Банк, Карта, Такси, Объявления,
//  911, Работы, Имущество, Документы, Настройки (обои — в клиенте).
//  Окно общее для ПК (bastion_ui.asi) и телефона (APK): assets/ui/index.html.
//
//  Окно отвечает в DIALOG_PHONE текстом "команда аргументы":
//    open <app>          открыть приложение (сервер шлёт его данные)
//    call <номер> / answer / hang
//    chat <номер> / sms <номер> <текст>
//    cadd <номер> <имя> / cdel <номер>
//    pay <номер телефона> <сумма>
//    gcat <раздел> / gpick <n> / gact <-1|-2|-3>
//    taxi / 911 <текст> / ad <текст> / doc <0-3> / cmd <houses|cars|quitjob|licenses>
//  Без HTML-окон (старый клиент) /phone пишет список команд.
//
//  Таблицы: phone_sms (переписка, хранится и для игроков не в сети),
//  phone_contacts (контакты по аккаунту), phone_calls (недавние звонки).
//  Объявления хранятся в памяти до перезапуска.
// =====================================================================

#define PHONE_AD_PRICE      300     // цена объявления
#define PHONE_AD_COOLDOWN   120     // секунд между объявлениями одного игрока
#define PHONE_MAX_ADS       15
#define PHONE_MAX_CONTACTS  40
#define PHONE_JSON_SIZE     4000

static gPhApp[MAX_PLAYERS][12];         // открытое приложение
static gPhPeer[MAX_PLAYERS];            // номер в открытой переписке / раздел карты
static gPhNote[MAX_PLAYERS][128];
static bool:gPhOk[MAX_PLAYERS];
static gPhCallRow[MAX_PLAYERS];         // строка phone_calls текущего исходящего звонка
static gPhAdLast[MAX_PLAYERS];

enum E_PH_AD
{
	adNum,
	adTime,
	adName[MAX_PLAYER_NAME],
	adText[100]
}
static gPhAds[PHONE_MAX_ADS][E_PH_AD];
static gPhAdCount;

stock PhoneApp_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS phone_sms (id INTEGER PRIMARY KEY AUTOINCREMENT, src INTEGER, dst INTEGER, text TEXT, date INTEGER, seen INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS phone_sms_dst ON phone_sms (dst, seen)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS phone_sms_src ON phone_sms (src)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS phone_contacts (owner INTEGER, number INTEGER, name TEXT, UNIQUE (owner, number))"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS phone_calls (id INTEGER PRIMARY KEY AUTOINCREMENT, src INTEGER, dst INTEGER, date INTEGER, ok INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS phone_calls_src ON phone_calls (src)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS phone_calls_dst ON phone_calls (dst)"));
}

stock PhoneApp_OnDisconnect(playerid)
{
	gPhApp[playerid][0] = '\0';
	gPhNote[playerid][0] = '\0';
	gPhPeer[playerid] = 0;
	gPhCallRow[playerid] = 0;
	gPhAdLast[playerid] = 0;
}

// После входа: сколько непрочитанных SMS
stock PhoneApp_OnLogin(playerid)
{
	if (Player[playerid][pPhone] == 0) return;
	new n = PhoneApp_Unread(playerid);
	if (n > 0) SendClientMessage(playerid, COLOR_YELLOW, "Пока тебя не было, пришло SMS: %d. Открой телефон: /phone или N", n);
}

static PhoneApp_Note(playerid, const note[], bool:ok = false)
{
	format(gPhNote[playerid], sizeof gPhNote[], "%s", note);
	gPhOk[playerid] = ok;
}

static PhoneApp_Unread(playerid)
{
	new n = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM phone_sms WHERE dst = %d AND seen = 0", Player[playerid][pPhone]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) n = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return n;
}

// Имя номера в контактах игрока ("" — нет)
stock PhoneApp_ContactName(playerid, number, dest[], size = sizeof dest)
{
	dest[0] = '\0';
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT name FROM phone_contacts WHERE owner = %d AND number = %d", Player[playerid][pDbId], number);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) DB_GetFieldString(r, 0, dest, size);
		DB_FreeResultSet(r);
	}
}

// Номер есть у какого-нибудь аккаунта (для SMS тем, кто не в сети)
static PhoneApp_NumberAccount(number)
{
	new id = 0;
	if (number <= 0) return 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id FROM accounts WHERE phone = %d LIMIT 1", number);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) id = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return id;
}

static bool:PhoneApp_IsOpen(playerid)
{
	return (GetPlayerDialogID(playerid) == DIALOG_PHONE);
}

// ---------------------------------------------------------------------
//  Звонки: журнал и обновление окна (вызывается из phone.inc)
// ---------------------------------------------------------------------
stock PhoneApp_OnCallStart(caller, target)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO phone_calls (src, dst, date) VALUES (%d, %d, %d)", Player[caller][pPhone], Player[target][pPhone], gettime()));
	gPhCallRow[caller] = DB_LastInsertId();
	PhoneApp_OnCallChange(target);
}

stock PhoneApp_OnAnswer(caller, target)
{
	if (gPhCallRow[caller] > 0) DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE phone_calls SET ok = 1 WHERE id = %d", gPhCallRow[caller]));
	gPhCallRow[caller] = 0;
	PhoneApp_OnCallChange(caller);
	PhoneApp_OnCallChange(target);
}

// Звонок начался или закончился: если телефон открыт — показать экран звонка
stock PhoneApp_OnCallChange(playerid)
{
	if (!IsPlayerConnected(playerid) || !PhoneApp_IsOpen(playerid)) return;
	PhoneApp_Show(playerid, Player[playerid][pCallState] != CALL_NONE ? ("call") : ("calls"));
}

// Пришло SMS: если открыта эта переписка или список — обновить
stock PhoneApp_OnSms(target, fromNumber)
{
	if (!PhoneApp_IsOpen(target)) return;
	if (!strcmp(gPhApp[target], "chat") && gPhPeer[target] == fromNumber) PhoneApp_Show(target, "chat");
	else if (!strcmp(gPhApp[target], "sms") || !strcmp(gPhApp[target], "home")) PhoneApp_Show(target, gPhApp[target]);
}

// SMS на номер (в сети или нет). true — отправлено.
stock PhoneApp_SendSms(playerid, number, const text[])
{
	if (!Phone_Need(playerid)) return 0;
	if (text[0] == '\0') return 0;
	if (number == Player[playerid][pPhone]) { SendClientMessage(playerid, COLOR_GREY, "Это твой номер."); return 0; }
	if (Admin_IsMuted(playerid)) { SendClientMessage(playerid, COLOR_GREY, "У тебя мут, писать нельзя."); return 0; }
	if (GetMoney(playerid) < SMS_PRICE) { SendClientMessage(playerid, COLOR_GREY, "Не хватает денег на SMS ($%d).", SMS_PRICE); return 0; }

	new target = Phone_FindByNumber(number);
	if (target == INVALID_PLAYER_ID && PhoneApp_NumberAccount(number) == 0)
	{
		SendClientMessage(playerid, COLOR_GREY, "Номер %d не обслуживается.", number);
		return 0;
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO phone_sms (src, dst, text, date) VALUES (%d, %d, '%q', %d)", Player[playerid][pPhone], number, text, gettime()));
	GiveMoney(playerid, -SMS_PRICE, "SMS");
	SendClientMessage(playerid, COLOR_YELLOW, "SMS на %d: %s ($%d)", number, text, SMS_PRICE);
	if (target != INVALID_PLAYER_ID)
	{
		new name[32];
		PhoneApp_ContactName(target, Player[playerid][pPhone], name);
		if (name[0]) SendClientMessage(target, COLOR_YELLOW, "SMS от %s (%d): %s", name, Player[playerid][pPhone], text);
		else SendClientMessage(target, COLOR_YELLOW, "SMS от %d: %s", Player[playerid][pPhone], text);
		PlayerPlaySound(target, 1052, 0.0, 0.0, 0.0);
		Fbi_OnSms(playerid, target, text);
		PhoneApp_OnSms(target, Player[playerid][pPhone]);
	}
	return 1;
}

// ---------------------------------------------------------------------
//  Данные приложений
// ---------------------------------------------------------------------
static PhoneApp_JsonCalls(playerid, json[])
{
	new me = Player[playerid][pPhone], part[160], name[32], when[16];
	strcat(json, ",\"rec\":[", PHONE_JSON_SIZE);
	new DBResult:r = DB_ExecuteQuery(gDB,
		"SELECT p.src, p.dst, p.ok, IFNULL(c.name, ''), strftime('%q', p.date, 'unixepoch', 'localtime') FROM phone_calls p LEFT JOIN phone_contacts c ON c.owner = %d AND c.number = (CASE WHEN p.src = %d THEN p.dst ELSE p.src END) WHERE p.src = %d OR p.dst = %d ORDER BY p.id DESC LIMIT 15",
		"%d.%m %H:%M", Player[playerid][pDbId], me, me, me);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new src = DB_GetFieldInt(r, 0), dst = DB_GetFieldInt(r, 1), ok = DB_GetFieldInt(r, 2);
			DB_GetFieldString(r, 3, part, sizeof part);
			UI_JsonEscape(name, part);
			DB_GetFieldString(r, 4, when, sizeof when);
			format(part, sizeof part, "%s[%d,\"%s\",\"%s\",%d,%d]", i ? "," : "", src == me ? dst : src, name, when, src == me ? 0 : 1, ok);
			strcat(json, part, PHONE_JSON_SIZE);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	strcat(json, "]", PHONE_JSON_SIZE);
}

static PhoneApp_JsonSms(playerid, json[])
{
	new me = Player[playerid][pPhone], part[200], name[32], text[100], when[16], raw[100];
	strcat(json, ",\"conv\":[", PHONE_JSON_SIZE);
	new DBResult:r = DB_ExecuteQuery(gDB,
		"SELECT s.peer, s.text, strftime('%q', s.date, 'unixepoch', 'localtime'), s.unread, IFNULL(c.name, '') FROM (SELECT CASE WHEN src = %d THEN dst ELSE src END AS peer, MAX(id) AS mid, text, date, SUM(CASE WHEN dst = %d AND seen = 0 THEN 1 ELSE 0 END) AS unread FROM phone_sms WHERE src = %d OR dst = %d GROUP BY peer) s LEFT JOIN phone_contacts c ON c.owner = %d AND c.number = s.peer ORDER BY s.mid DESC LIMIT 15",
		"%d.%m %H:%M", me, me, me, me, Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new peer = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 1, raw, 48);
			UI_JsonEscape(text, raw);
			DB_GetFieldString(r, 2, when, sizeof when);
			new unread = DB_GetFieldInt(r, 3);
			DB_GetFieldString(r, 4, raw, sizeof raw);
			UI_JsonEscape(name, raw);
			format(part, sizeof part, "%s[%d,\"%s\",\"%s\",\"%s\",%d]", i ? "," : "", peer, name, text, when, unread);
			strcat(json, part, PHONE_JSON_SIZE);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	strcat(json, "]", PHONE_JSON_SIZE);
}

static PhoneApp_JsonChat(playerid, json[])
{
	new me = Player[playerid][pPhone], peer = gPhPeer[playerid], part[260], text[200], raw[110], when[16];
	PhoneApp_ContactName(playerid, peer, raw);
	UI_JsonEscape(text, raw);
	format(part, sizeof part, ",\"peer\":%d,\"pname\":\"%s\",\"online\":%d,\"price\":%d,\"msgs\":[", peer, text, _:(Phone_FindByNumber(peer) != INVALID_PLAYER_ID), SMS_PRICE);
	strcat(json, part, PHONE_JSON_SIZE);
	new DBResult:r = DB_ExecuteQuery(gDB,
		"SELECT src, text, strftime('%q', date, 'unixepoch', 'localtime') FROM phone_sms WHERE (src = %d AND dst = %d) OR (src = %d AND dst = %d) ORDER BY id DESC LIMIT 20",
		"%d.%m %H:%M", me, peer, peer, me);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r), n = 0;
		for (new i = 0; i < rows; i++)
		{
			new mine = (DB_GetFieldInt(r, 0) == me);
			DB_GetFieldString(r, 1, raw, sizeof raw);
			UI_JsonEscape(text, raw);
			DB_GetFieldString(r, 2, when, sizeof when);
			format(part, sizeof part, "%s[%d,\"%s\",\"%s\"]", n ? "," : "", mine, text, when);
			if (strlen(json) + strlen(part) + 8 >= PHONE_JSON_SIZE) break;
			strcat(json, part, PHONE_JSON_SIZE);
			n++;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	strcat(json, "]", PHONE_JSON_SIZE);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE phone_sms SET seen = 1 WHERE dst = %d AND src = %d AND seen = 0", me, peer));
}

static PhoneApp_JsonContacts(playerid, json[])
{
	new part[96], name[48], raw[32];
	strcat(json, ",\"cont\":[", PHONE_JSON_SIZE);
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT number, name FROM phone_contacts WHERE owner = %d ORDER BY name COLLATE NOCASE LIMIT %d", Player[playerid][pDbId], PHONE_MAX_CONTACTS);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			DB_GetFieldString(r, 1, raw, sizeof raw);
			UI_JsonEscape(name, raw);
			format(part, sizeof part, "%s[%d,\"%s\"]", i ? "," : "", DB_GetFieldInt(r, 0), name);
			strcat(json, part, PHONE_JSON_SIZE);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	strcat(json, "]", PHONE_JSON_SIZE);
}

static PhoneApp_JsonBank(playerid, json[])
{
	new part[128], name[MAX_PLAYER_NAME], when[16];
	format(part, sizeof part, ",\"acc\":%d,\"fee\":%d,\"hist\":[", Player[playerid][pDbId], BANK_TRANSFER_FEE_PCT);
	strcat(json, part, PHONE_JSON_SIZE);
	new DBResult:r = DB_ExecuteQuery(gDB,
		"SELECT l.type, l.amount, IFNULL(a.name, ''), strftime('%q', l.date, 'unixepoch', 'localtime') FROM bank_log l LEFT JOIN accounts a ON a.id = l.other WHERE l.account = %d ORDER BY l.id DESC LIMIT 10",
		"%d.%m %H:%M", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			DB_GetFieldString(r, 2, name, sizeof name);
			for (new c = 0; name[c] != '\0'; c++) if (name[c] == '_') name[c] = ' ';
			DB_GetFieldString(r, 3, when, sizeof when);
			format(part, sizeof part, "%s[%d,%d,\"%s\",\"%s\"]", i ? "," : "", DB_GetFieldInt(r, 0), DB_GetFieldInt(r, 1), name, when);
			strcat(json, part, PHONE_JSON_SIZE);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	strcat(json, "]", PHONE_JSON_SIZE);
}

static PhoneApp_JsonNews(json[])
{
	new part[260], text[200], name[MAX_PLAYER_NAME], now = gettime();
	format(part, sizeof part, ",\"price\":%d,\"ads\":[", PHONE_AD_PRICE);
	strcat(json, part, PHONE_JSON_SIZE);
	// новые сверху
	for (new k = 0; k < gPhAdCount; k++)
	{
		new i = gPhAdCount - 1 - k;
		UI_JsonEscape(text, gPhAds[i][adText]);
		format(name, sizeof name, "%s", gPhAds[i][adName]);
		for (new c = 0; name[c] != '\0'; c++) if (name[c] == '_') name[c] = ' ';
		format(part, sizeof part, "%s[\"%s\",\"%s\",%d,%d]", k ? "," : "", text, name, gPhAds[i][adNum], (now - gPhAds[i][adTime]) / 60);
		strcat(json, part, PHONE_JSON_SIZE);
	}
	strcat(json, "]", PHONE_JSON_SIZE);
}

static PhoneApp_JsonPlaces(playerid, json[], cat)
{
	new list[2600], part[96];
	Gps_PhoneList(playerid, cat, list, sizeof list, 30);
	format(part, sizeof part, ",\"cat\":%d,\"cname\":\"%s\",\"places\":", cat, Gps_CatName(cat));
	strcat(json, part, PHONE_JSON_SIZE);
	strcat(json, list, PHONE_JSON_SIZE);
}

// Номер раздела GPS «Работы» (gps.inc, gGpsCatNames)
#define PHONE_GPS_JOBS      2

// Показать телефон с приложением app
stock PhoneApp_Show(playerid, const app[])
{
	if (!UI_Has(playerid)) return;
	format(gPhApp[playerid], sizeof gPhApp[], "%s", app);

	new json[PHONE_JSON_SIZE], note[200], cname[32], cnameJs[48], h, m, s;
	gettime(h, m, s);
	UI_JsonEscape(note, gPhNote[playerid]);
	new cs = Player[playerid][pCallState], cn = 0, other = Player[playerid][pCallWith];
	if (cs != CALL_NONE && other != INVALID_PLAYER_ID && IsPlayerConnected(other))
	{
		cn = Player[other][pPhone];
		PhoneApp_ContactName(playerid, cn, cname);
		UI_JsonEscape(cnameJs, cname);
	}
	format(json, sizeof json,
		"{\"app\":\"%s\",\"num\":%d,\"time\":\"%02d:%02d\",\"cash\":%d,\"bank\":%d,\"unread\":%d,\"cs\":%d,\"cn\":%d,\"cname\":\"%s\",\"gps\":%d,\"note\":\"%s\",\"ok\":%d",
		app, Player[playerid][pPhone], h, m, GetMoney(playerid), Player[playerid][pBank], PhoneApp_Unread(playerid),
		cs, cn, cnameJs, _:Gps_IsOn(playerid), note, _:gPhOk[playerid]);
	gPhNote[playerid][0] = '\0';
	gPhOk[playerid] = false;

	if (!strcmp(app, "calls")) PhoneApp_JsonCalls(playerid, json);
	else if (!strcmp(app, "sms")) PhoneApp_JsonSms(playerid, json);
	else if (!strcmp(app, "chat")) PhoneApp_JsonChat(playerid, json);
	else if (!strcmp(app, "contacts")) PhoneApp_JsonContacts(playerid, json);
	else if (!strcmp(app, "bank")) PhoneApp_JsonBank(playerid, json);
	else if (!strcmp(app, "news")) PhoneApp_JsonNews(json);
	else if (!strcmp(app, "map"))
	{
		new cats[600];
		Gps_PhoneCats(playerid, cats, sizeof cats);
		strcat(json, ",\"cats\":", sizeof json);
		strcat(json, cats, sizeof json);
	}
	else if (!strcmp(app, "places")) PhoneApp_JsonPlaces(playerid, json, gPhPeer[playerid]);
	else if (!strcmp(app, "jobs"))
	{
		new part[96], jname[32];
		if (Player[playerid][pJob] != 0) format(jname, sizeof jname, "%s", Job_GetName(Player[playerid][pJob]));
		format(part, sizeof part, ",\"job\":\"%s\",\"working\":%d", jname, Player[playerid][pWorking] ? 1 : 0);
		strcat(json, part, sizeof json);
		PhoneApp_JsonPlaces(playerid, json, PHONE_GPS_JOBS);
	}
	else if (!strcmp(app, "property"))
	{
		new part[96], h2 = House_GetOwned(playerid), rent = 0;
		if (h2 == -1) { h2 = House_GetRented(playerid); rent = (h2 != -1); }
		format(part, sizeof part, ",\"house\":%d,\"rent\":%d,\"car\":%d", h2 == -1 ? 0 : House[h2][hDbId], rent,
			gPlayerVeh[playerid] != INVALID_VEHICLE_ID ? GetVehicleModel(gPlayerVeh[playerid]) : 0);
		strcat(json, part, sizeof json);
	}
	else if (!strcmp(app, "docs"))
	{
		new part[64];
		format(part, sizeof part, ",\"docs\":[%d,%d,%d,%d]", _:(Docs_Has(playerid, INV_PASSPORT) != 0), _:(Docs_Has(playerid, INV_MEDCARD) != 0),
			_:(Docs_Has(playerid, INV_LICENSES) != 0), _:(Docs_Has(playerid, INV_SHOOTING) != 0));
		strcat(json, part, sizeof json);
	}
	else if (!strcmp(app, "taxi"))
	{
		new part[48], taxis = 0;
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (i != playerid && IsPlayerConnected(i) && Player[i][pWorking] && Player[i][pJob] == JOB_TAXI) taxis++;
		}
		format(part, sizeof part, ",\"taxis\":%d,\"fare\":%d", taxis, TAXI_FARE);
		strcat(json, part, sizeof json);
	}
	strcat(json, "}", sizeof json);
	UI_Show(playerid, DIALOG_PHONE, DIALOG_STYLE_INPUT, "phone", json, "OK", "Закрыть");
}

// /phone и N
stock Cmd_Phone(playerid)
{
	if (!Phone_Need(playerid)) return 1;
	if (Player[playerid][pCuffed]) return SendClientMessage(playerid, COLOR_GREY, "В наручниках телефон не достать.");
	if (!UI_Has(playerid))
	{
		SendClientMessage(playerid, COLOR_WHITE, "Твой номер: %d. Команды: /call /sms /number /911 /taxi /ad /gps /bank", Player[playerid][pPhone]);
		return 1;
	}
	PhoneApp_Show(playerid, Player[playerid][pCallState] != CALL_NONE ? ("call") : ("home"));
	return 1;
}

// /ad [текст] — объявление (и из приложения «Объявления»)
stock Cmd_Ad(playerid, const params[])
{
	if (!Phone_Need(playerid)) return 0;
	new text[100];
	if (!RestParam(params, 0, text) || strlen(text) < 5)
	{
		SendClientMessage(playerid, COLOR_GREY, "Использование: /ad [текст объявления] ($%d)", PHONE_AD_PRICE);
		return 0;
	}
	if (Admin_IsMuted(playerid)) { SendClientMessage(playerid, COLOR_GREY, "У тебя мут, писать нельзя."); return 0; }
	new now = gettime();
	if (gPhAdLast[playerid] && now - gPhAdLast[playerid] < PHONE_AD_COOLDOWN)
	{
		SendClientMessage(playerid, COLOR_GREY, "Следующее объявление можно через %d сек.", PHONE_AD_COOLDOWN - (now - gPhAdLast[playerid]));
		return 0;
	}
	if (GetMoney(playerid) < PHONE_AD_PRICE) { SendClientMessage(playerid, COLOR_GREY, "Объявление стоит $%d.", PHONE_AD_PRICE); return 0; }
	for (new i = 0; text[i] != '\0'; i++) if (text[i] == '{' || text[i] == '}') text[i] = ' ';
	GiveMoney(playerid, -PHONE_AD_PRICE, "объявление");
	gPhAdLast[playerid] = now;

	if (gPhAdCount == PHONE_MAX_ADS)
	{
		for (new i = 1; i < PHONE_MAX_ADS; i++) gPhAds[i - 1] = gPhAds[i];
		gPhAdCount--;
	}
	new i = gPhAdCount++;
	gPhAds[i][adNum] = Player[playerid][pPhone];
	gPhAds[i][adTime] = now;
	GetPlayerName(playerid, gPhAds[i][adName], MAX_PLAYER_NAME);
	format(gPhAds[i][adText], 100, "%s", text);

	new msg[160];
	format(msg, sizeof msg, "[Объявление] %s. Тел.: %d", text, Player[playerid][pPhone]);
	for (new p = 0; p < MAX_PLAYERS; p++)
	{
		if (IsPlayerConnected(p) && Player[p][pLogged]) SendClientMessage(p, 0x7FD4FFFF, "%s", msg);
	}
	return 1;
}

// ---------------------------------------------------------------------
//  Ответ окна
// ---------------------------------------------------------------------
static bool:PhoneApp_IsApp(const app[])
{
	static const apps[][] = { "home", "calls", "sms", "contacts", "bank", "map", "taxi", "news", "911", "jobs", "property", "docs", "settings" };
	for (new i = 0; i < sizeof apps; i++) if (!strcmp(app, apps[i])) return true;
	return false;
}

stock PhoneApp_OnDialog(playerid, dialogid, response, const inputtext[])
{
	if (dialogid != DIALOG_PHONE) return 0;
	if (!response || Player[playerid][pPhone] == 0) { gPhApp[playerid][0] = '\0'; return 1; }

	new cmd[12], pos = 0, num, rest[110];
	NextParam(inputtext, pos, cmd);

	if (!strcmp(cmd, "open"))
	{
		new app[12];
		NextParam(inputtext, pos, app);
		PhoneApp_Show(playerid, PhoneApp_IsApp(app) ? app : ("home"));
	}
	else if (!strcmp(cmd, "call"))
	{
		new p[16];
		NextParam(inputtext, pos, p);
		Cmd_Call(playerid, p);
		PhoneApp_Show(playerid, Player[playerid][pCallState] != CALL_NONE ? ("call") : ("calls"));
	}
	else if (!strcmp(cmd, "answer"))
	{
		if (Player[playerid][pCallState] == CALL_INCOMING) Cmd_PickUp(playerid);
		PhoneApp_Show(playerid, Player[playerid][pCallState] != CALL_NONE ? ("call") : ("calls"));
	}
	else if (!strcmp(cmd, "hang"))
	{
		if (Player[playerid][pCallState] != CALL_NONE) Cmd_HangUp(playerid);
		PhoneApp_Show(playerid, "calls");
	}
	else if (!strcmp(cmd, "chat"))
	{
		if (!NextIntParam(inputtext, pos, num) || num <= 0) PhoneApp_Show(playerid, "sms");
		else
		{
			gPhPeer[playerid] = num;
			PhoneApp_Show(playerid, "chat");
		}
	}
	else if (!strcmp(cmd, "sms"))
	{
		if (NextIntParam(inputtext, pos, num) && num > 0 && RestParam(inputtext, pos, rest))
		{
			gPhPeer[playerid] = num;
			if (!PhoneApp_SendSms(playerid, num, rest)) PhoneApp_Note(playerid, "SMS не отправлено, причина в чате.");
		}
		PhoneApp_Show(playerid, "chat");
	}
	else if (!strcmp(cmd, "cadd"))
	{
		if (!NextIntParam(inputtext, pos, num) || num <= 0 || !RestParam(inputtext, pos, rest))
			PhoneApp_Note(playerid, "Нужны номер и имя.");
		else
		{
			rest[24] = '\0';
			new cnt = 0;
			new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM phone_contacts WHERE owner = %d", Player[playerid][pDbId]);
			if (r != DBResult:0) { if (DB_GetRowCount(r) > 0) cnt = DB_GetFieldInt(r, 0); DB_FreeResultSet(r); }
			if (cnt >= PHONE_MAX_CONTACTS) PhoneApp_Note(playerid, "Контактов слишком много, удали лишние.");
			else
			{
				DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO phone_contacts (owner, number, name) VALUES (%d, %d, '%q')", Player[playerid][pDbId], num, rest));
				PhoneApp_Note(playerid, "Контакт сохранён.", true);
			}
		}
		PhoneApp_Show(playerid, "contacts");
	}
	else if (!strcmp(cmd, "cdel"))
	{
		if (NextIntParam(inputtext, pos, num))
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM phone_contacts WHERE owner = %d AND number = %d", Player[playerid][pDbId], num));
		PhoneApp_Show(playerid, "contacts");
	}
	else if (!strcmp(cmd, "pay"))
	{
		new amount;
		if (!NextIntParam(inputtext, pos, num) || !NextIntParam(inputtext, pos, amount) || amount <= 0)
			PhoneApp_Note(playerid, "Введи номер телефона и сумму.");
		else
		{
			new dbid = PhoneApp_NumberAccount(num);
			if (dbid == 0) PhoneApp_Note(playerid, "Номер не найден.");
			else
			{
				new before = Player[playerid][pBank];
				Bank_Transfer(playerid, dbid, amount);
				if (Player[playerid][pBank] < before) PhoneApp_Note(playerid, "Перевод выполнен.", true);
				else PhoneApp_Note(playerid, "Перевод не прошёл, причина в чате.");
			}
		}
		PhoneApp_Show(playerid, "bank");
	}
	else if (!strcmp(cmd, "gcat"))
	{
		if (!NextIntParam(inputtext, pos, num)) num = 0;
		gPhPeer[playerid] = num;
		PhoneApp_Show(playerid, "places");
	}
	else if (!strcmp(cmd, "gpick"))
	{
		// метка ставится — телефон убираем, чтобы видеть карту/радар
		if (!NextIntParam(inputtext, pos, num) || !Gps_PhonePick(playerid, num))
		{
			PhoneApp_Note(playerid, "Место не найдено.");
			PhoneApp_Show(playerid, "map");
		}
	}
	else if (!strcmp(cmd, "gact"))
	{
		if (NextIntParam(inputtext, pos, num) && num >= -3 && num <= -1) Gps_PhoneAct(playerid, num);
		if (num == -1) PhoneApp_Show(playerid, "map");
	}
	else if (!strcmp(cmd, "taxi"))
	{
		new taxis = 0;
		for (new i = 0; i < MAX_PLAYERS; i++)
			if (i != playerid && IsPlayerConnected(i) && Player[i][pWorking] && Player[i][pJob] == JOB_TAXI) taxis++;
		if (IsPlayerInAnyVehicle(playerid)) PhoneApp_Note(playerid, "Выйди из машины, потом вызывай.");
		else if (taxis == 0) PhoneApp_Note(playerid, "Сейчас нет таксистов на смене.");
		else
		{
			Cmd_Taxi(playerid);
			PhoneApp_Note(playerid, "Вызов принят. Жди такси на месте.", true);
		}
		PhoneApp_Show(playerid, "taxi");
	}
	else if (!strcmp(cmd, "911"))
	{
		RestParam(inputtext, pos, rest);
		Cmd_911(playerid, rest);
		PhoneApp_Note(playerid, "Вызов передан диспетчеру, ответ в чате.", true);
		PhoneApp_Show(playerid, "911");
	}
	else if (!strcmp(cmd, "ad"))
	{
		RestParam(inputtext, pos, rest);
		if (Cmd_Ad(playerid, rest)) PhoneApp_Note(playerid, "Объявление опубликовано.", true);
		else PhoneApp_Note(playerid, "Объявление не опубликовано, причина в чате.");
		PhoneApp_Show(playerid, "news");
	}
	else if (!strcmp(cmd, "doc"))
	{
		if (!NextIntParam(inputtext, pos, num) || num < 0 || num > 3) num = 0;
		new item = INV_PASSPORT + num;
		if (Docs_Has(playerid, item)) Docs_Use(playerid, item);
		else
		{
			PhoneApp_Note(playerid, "Этого документа у тебя нет.");
			PhoneApp_Show(playerid, "docs");
		}
	}
	else if (!strcmp(cmd, "cmd"))
	{
		new what[12];
		NextParam(inputtext, pos, what);
		if (!strcmp(what, "houses")) Cmd_MyHouses(playerid);
		else if (!strcmp(what, "cars")) Cmd_Cars(playerid);
		else if (!strcmp(what, "licenses")) Cmd_Licenses(playerid);
		else if (!strcmp(what, "quitjob")) { Cmd_QuitJob(playerid); PhoneApp_Show(playerid, "jobs"); }
	}
	else PhoneApp_Show(playerid, "home");
	return 1;
}
