// =====================================================================
//  logs.inc — общий журнал всех событий сервера.
//
//  Пишется в отдельную базу scriptfiles/logs.db (таблица logs), чтобы
//  rp.db и её копии не разрастались. Что попадает в журнал:
//    conn    — подключение, вход в аккаунт, неверный пароль, выход (с IP)
//    chat    — обычный чат
//    cmd     — все команды с текстом (/me, /pm, /f, /r, админ-команды...)
//    money   — каждое изменение наличных (сумма, причина, остаток)
//    bank    — операции по счёту в банке
//    item    — вещи в сумке: получил / потратил
//    admin   — действия администрации (то же, что /alog)
//    kill    — убийства, смерти, ранения от игрока
//    dmg     — урон от игрока игроку
//    veh     — сел за руль машины
//    faction — журнал фракций, family — журнал семей
//    ac      — срабатывания античита
//
//  Записи копятся в памяти и пишутся пачкой раз в LOG_FLUSH_MS одной
//  транзакцией — сервер не тормозит даже при большом онлайне.
//  Старые записи удаляются сами (сроки ниже), понемногу, без лагов.
//
//  Поиск:
//    в игре (админ LOG_VIEW_LEVEL+): /logs [ник|id|*] [тип|*] [текст]
//    на сайте/форуме: страница /logs (фильтры по нику, типу, дате, тексту, IP)
// =====================================================================

#define LOG_DB_FILE         "logs.db"   // лежит в папке scriptfiles
#define LOG_BUF_ROWS        128         // сколько записей держать в памяти до записи
#define LOG_TEXT_LEN        192
#define LOG_FLUSH_MS        2000
#define LOG_KEEP_DAYS       90          // всё, кроме чата и урона
#define LOG_KEEP_CHAT_DAYS  30          // chat и cmd
#define LOG_KEEP_DMG_DAYS   7           // dmg
#define LOG_CLEAN_CHUNK     3000        // сколько старых записей удалять за раз
#define LOG_VIEW_LEVEL      3           // с какого уровня админки /logs
#define LOG_VIEW_ROWS       30
#define LOG_VIEW_SCAN       300000      // поиск по тексту в игре — только по последним N записям

enum
{
	LOG_CONN,
	LOG_CHAT,
	LOG_CMD,
	LOG_MONEY,
	LOG_BANK,
	LOG_ITEM,
	LOG_ADMIN,
	LOG_KILL,
	LOG_DMG,
	LOG_VEH,
	LOG_FACTION,
	LOG_FAMILY,
	LOG_AC,
	LOG_TYPES
}

static const gLogTypeKeys[LOG_TYPES][] = { "conn", "chat", "cmd", "money", "bank", "item", "admin", "kill", "dmg", "veh", "faction", "family", "ac" };

new DB:gLogDB;
static gLogTz = 0;                      // смещение местного времени от UTC, секунд
static gLogLoginAt[MAX_PLAYERS];
static gLogCleanStage = 0;
static gLogCleanAt = 0;

static gLbCount = 0;
static gLbTime[LOG_BUF_ROWS];
static gLbType[LOG_BUF_ROWS];
static gLbAcc[LOG_BUF_ROWS];
static gLbName[LOG_BUF_ROWS][MAX_PLAYER_NAME];
static gLbTAcc[LOG_BUF_ROWS];
static gLbTName[LOG_BUF_ROWS][MAX_PLAYER_NAME];
static gLbAmount[LOG_BUF_ROWS];
static gLbIp[LOG_BUF_ROWS][16];
static gLbText[LOG_BUF_ROWS][LOG_TEXT_LEN];

forward Log_FlushTimer();

// ---------------------------------------------------------------------
//  Запуск и остановка
// ---------------------------------------------------------------------
stock Log_Init()
{
	gLogDB = DB_Open(LOG_DB_FILE);
	if (gLogDB == DB:0)
	{
		printf("[Logs] Cannot open %s - logging is off", LOG_DB_FILE);
		return;
	}
	// WAL: сайт читает журнал, пока сервер пишет, и никто никого не ждёт
	DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "PRAGMA journal_mode = WAL"));
	DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "PRAGMA synchronous = NORMAL"));
	DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "PRAGMA busy_timeout = 3000"));
	DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "CREATE TABLE IF NOT EXISTS logs (id INTEGER PRIMARY KEY AUTOINCREMENT, time INTEGER NOT NULL, type TEXT NOT NULL, acc INTEGER DEFAULT 0, name TEXT DEFAULT '', tacc INTEGER DEFAULT 0, tname TEXT DEFAULT '', amount INTEGER DEFAULT 0, ip TEXT DEFAULT '', text TEXT DEFAULT '')"));
	DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "CREATE INDEX IF NOT EXISTS logs_time ON logs (time)"));
	DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "CREATE INDEX IF NOT EXISTS logs_name ON logs (name COLLATE NOCASE, id)"));
	DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "CREATE INDEX IF NOT EXISTS logs_tname ON logs (tname COLLATE NOCASE, id)"));
	DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "CREATE INDEX IF NOT EXISTS logs_type ON logs (type, id)"));
	DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "CREATE INDEX IF NOT EXISTS logs_ip ON logs (ip, id)"));

	new h, m, s;
	gettime(h, m, s);
	gLogTz = (h * 3600 + m * 60 + s) - (gettime() % 86400);
	if (gLogTz > 43200) gLogTz -= 86400;
	if (gLogTz < -43200) gLogTz += 86400;
	gLogTz = ((gLogTz + (gLogTz >= 0 ? 450 : -450)) / 900) * 900;   // до 15 минут

	gLogCleanStage = 1;
	gLogCleanAt = gettime();
	SetTimer("Log_FlushTimer", LOG_FLUSH_MS, true);
	Log_Push(LOG_CONN, 0, "", 0, "", 0, "", "сервер запущен");
	print("[Logs] logs.db opened.");
}

stock Log_Exit()
{
	if (gLogDB == DB:0) return;
	Log_Push(LOG_CONN, 0, "", 0, "", 0, "", "сервер остановлен");
	Log_Flush();
	DB_Close(gLogDB);
	gLogDB = DB:0;
}

public Log_FlushTimer()
{
	Log_Flush();
	if (gLogCleanStage == 0 && gettime() - gLogCleanAt >= 3600)
	{
		gLogCleanStage = 1;
		gLogCleanAt = gettime();
	}
	if (gLogCleanStage > 0) Log_CleanStep();
	return 1;
}

// Записать накопленное одной транзакцией
stock Log_Flush()
{
	if (gLbCount == 0 || gLogDB == DB:0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "BEGIN"));
	for (new i = 0; i < gLbCount; i++)
	{
		DB_FreeResultSet(DB_ExecuteQuery(gLogDB,
			"INSERT INTO logs (time, type, acc, name, tacc, tname, amount, ip, text) VALUES (%d, '%s', %d, '%q', %d, '%q', %d, '%q', '%q')",
			gLbTime[i], gLogTypeKeys[gLbType[i]], gLbAcc[i], gLbName[i], gLbTAcc[i], gLbTName[i], gLbAmount[i], gLbIp[i], gLbText[i]));
	}
	DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "COMMIT"));
	gLbCount = 0;
}

// Удалить порцию старых записей. Стадии: 1 — урон, 2 — чат и команды, 3 — всё остальное
static Log_CleanStep()
{
	new now = gettime(), DBResult:r;
	switch (gLogCleanStage)
	{
		case 1: DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "DELETE FROM logs WHERE id IN (SELECT id FROM logs WHERE type = 'dmg' AND time < %d LIMIT %d)", now - LOG_KEEP_DMG_DAYS * 86400, LOG_CLEAN_CHUNK));
		case 2: DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "DELETE FROM logs WHERE id IN (SELECT id FROM logs WHERE type IN ('chat', 'cmd') AND time < %d LIMIT %d)", now - LOG_KEEP_CHAT_DAYS * 86400, LOG_CLEAN_CHUNK));
		default: DB_FreeResultSet(DB_ExecuteQuery(gLogDB, "DELETE FROM logs WHERE id IN (SELECT id FROM logs WHERE time < %d LIMIT %d)", now - LOG_KEEP_DAYS * 86400, LOG_CLEAN_CHUNK));
	}
	new removed = 0;
	r = DB_ExecuteQuery(gLogDB, "SELECT changes()");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) removed = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (removed < LOG_CLEAN_CHUNK)
	{
		gLogCleanStage++;
		if (gLogCleanStage > 3) gLogCleanStage = 0;
	}
}

// ---------------------------------------------------------------------
//  Запись
// ---------------------------------------------------------------------
stock Log_Push(type, acc, const name[], tacc, const tname[], amount, const ip[], const text[])
{
	if (gLogDB == DB:0 || type < 0 || type >= LOG_TYPES) return;
	if (gLbCount >= LOG_BUF_ROWS) Log_Flush();
	new i = gLbCount++;
	gLbTime[i] = gettime();
	gLbType[i] = type;
	gLbAcc[i] = acc;
	gLbTAcc[i] = tacc;
	gLbAmount[i] = amount;
	gLbName[i][0] = '\0';
	gLbTName[i][0] = '\0';
	gLbIp[i][0] = '\0';
	gLbText[i][0] = '\0';
	strcat(gLbName[i], name, MAX_PLAYER_NAME);
	strcat(gLbTName[i], tname, MAX_PLAYER_NAME);
	strcat(gLbIp[i], ip, 16);
	strcat(gLbText[i], text, LOG_TEXT_LEN);
}

// Событие игрока (и, если есть, второго игрока: кому, кого)
stock Log_Player(type, playerid, targetid, amount, const text[])
{
	new name[MAX_PLAYER_NAME], tname[MAX_PLAYER_NAME], ip[16], acc = 0, tacc = 0;
	if (playerid >= 0 && playerid < MAX_PLAYERS && IsPlayerConnected(playerid))
	{
		GetPlayerName(playerid, name, sizeof name);
		GetPlayerIp(playerid, ip, sizeof ip);
		acc = Player[playerid][pDbId];
	}
	if (targetid >= 0 && targetid < MAX_PLAYERS && IsPlayerConnected(targetid))
	{
		GetPlayerName(targetid, tname, sizeof tname);
		tacc = Player[targetid][pDbId];
	}
	Log_Push(type, acc, name, tacc, tname, amount, ip, text);
}

// Событие по номеру аккаунта (банк): ник берём у игрока в игре или из базы
stock Log_Account(type, acc, amount, const text[])
{
	if (acc <= 0) return;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pDbId] == acc)
		{
			Log_Player(type, i, INVALID_PLAYER_ID, amount, text);
			return;
		}
	}
	new name[MAX_PLAYER_NAME];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT name FROM accounts WHERE id = %d", acc);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) DB_GetFieldString(r, 0, name, sizeof name);
		DB_FreeResultSet(r);
	}
	Log_Push(type, acc, name, 0, "", amount, "", text);
}

// ---------------------------------------------------------------------
//  События из rp.pwn и модулей
// ---------------------------------------------------------------------
stock Log_OnConnect(playerid)
{
	gLogLoginAt[playerid] = 0;
	Log_Player(LOG_CONN, playerid, INVALID_PLAYER_ID, 0, IsPlayerMobile[playerid] ? ("подключился (телефон)") : ("подключился"));
}

stock Log_OnLogin(playerid)
{
	gLogLoginAt[playerid] = gettime();
	new text[96];
	format(text, sizeof text, "вошёл в аккаунт: ур. %d, нал. $%d, банк $%d", Player[playerid][pLevel], Player[playerid][pMoney], Player[playerid][pBank]);
	Log_Player(LOG_CONN, playerid, INVALID_PLAYER_ID, Player[playerid][pMoney], text);
}

stock Log_OnBadPassword(playerid, tries)
{
	new text[48];
	format(text, sizeof text, "неверный пароль (попытка %d)", tries);
	Log_Player(LOG_CONN, playerid, INVALID_PLAYER_ID, 0, text);
}

stock Log_OnDisconnect(playerid, reason)
{
	new text[96];
	static const why[][] = { "вылет", "вышел", "кик/бан" };
	if (gLogLoginAt[playerid] > 0)
		format(text, sizeof text, "отключился (%s), в игре %d мин., нал. $%d", (reason >= 0 && reason <= 2) ? why[reason] : ("?"),
			(gettime() - gLogLoginAt[playerid]) / 60, Player[playerid][pMoney]);
	else
		format(text, sizeof text, "отключился (%s) без входа в аккаунт", (reason >= 0 && reason <= 2) ? why[reason] : ("?"));
	Log_Player(LOG_CONN, playerid, INVALID_PLAYER_ID, 0, text);
	gLogLoginAt[playerid] = 0;
}

stock Log_OnText(playerid, const text[])
{
	Log_Player(LOG_CHAT, playerid, INVALID_PLAYER_ID, 0, text);
}

// Команда целиком. У команд с паролями параметры не пишем.
stock Log_OnCommand(playerid, const cmdtext[])
{
	new cmd[32], pos = 0;
	NextParam(cmdtext, pos, cmd);
	if (!strcmp(cmd, "/alogin", true) || !strcmp(cmd, "/apass", true) || !strcmp(cmd, "/changepass", true) || !strcmp(cmd, "/password", true))
	{
		if (cmdtext[pos] != '\0') strcat(cmd, " ***");
		Log_Player(LOG_CMD, playerid, INVALID_PLAYER_ID, 0, cmd);
		return;
	}
	// /pm 5 текст, /sms ... — если первый параметр id игрока, он идёт во второй ник
	new target = INVALID_PLAYER_ID, p = pos, buf[16];
	if (NextParam(cmdtext, p, buf) && IsNumeric(buf))
	{
		new id = strval(buf);
		if (id >= 0 && id < MAX_PLAYERS && IsPlayerConnected(id)) target = id;
	}
	Log_Player(LOG_CMD, playerid, target, 0, cmdtext);
}

stock Log_OnMoney(playerid, amount, const reason[])
{
	if (amount == 0) return;
	new text[LOG_TEXT_LEN];
	format(text, sizeof text, "%s%d$ %s (на руках $%d)", amount > 0 ? ("+") : (""), amount, reason[0] != '\0' ? reason : ("без причины"), Player[playerid][pMoney]);
	Log_Player(LOG_MONEY, playerid, INVALID_PLAYER_ID, amount, text);
}

stock Log_OnBank(account, amount, const reason[])
{
	new text[LOG_TEXT_LEN];
	format(text, sizeof text, "%s%d$ банк: %s", amount > 0 ? ("+") : (""), amount, reason);
	Log_Account(LOG_BANK, account, amount, text);
}

stock Log_OnItem(playerid, const item[], amount, total)
{
	new text[96];
	format(text, sizeof text, "%s%d %s (стало %d)", amount > 0 ? ("+") : (""), amount, item, total);
	Log_Player(LOG_ITEM, playerid, INVALID_PLAYER_ID, amount, text);
}

stock Log_OnAdmin(const admin[], adminDb, const action[], const target[], targetDb, const reason[], value)
{
	new text[LOG_TEXT_LEN];
	if (reason[0] != '\0') format(text, sizeof text, "%s %s (%d) — %s", action, target, value, reason);
	else format(text, sizeof text, "%s %s (%d)", action, target, value);
	Log_Push(LOG_ADMIN, adminDb, admin, targetDb, target, value, "", text);
}

stock Log_OnDeath(playerid, killerid, WEAPON:reason)
{
	new text[96], weapon[32];
	GetWeaponName(reason, weapon, sizeof weapon);
	if (weapon[0] == '\0') format(weapon, sizeof weapon, "%d", _:reason);
	if (killerid != INVALID_PLAYER_ID && IsPlayerConnected(killerid))
	{
		format(text, sizeof text, "убил игрока (%s)", weapon);
		Log_Player(LOG_KILL, killerid, playerid, _:reason, text);
	}
	else
	{
		format(text, sizeof text, "погиб (%s)", weapon);
		Log_Player(LOG_KILL, playerid, INVALID_PLAYER_ID, _:reason, text);
	}
}

stock Log_OnDamage(playerid, issuerid, Float:amount, WEAPON:weaponid, bodypart, bool:wounded)
{
	if (issuerid == INVALID_PLAYER_ID || !IsPlayerConnected(issuerid)) return;
	new text[96], weapon[32];
	GetWeaponName(weaponid, weapon, sizeof weapon);
	if (weapon[0] == '\0') format(weapon, sizeof weapon, "%d", _:weaponid);
	format(text, sizeof text, "урон %.1f (%s, часть тела %d)", amount, weapon, bodypart);
	Log_Player(LOG_DMG, issuerid, playerid, floatround(amount), text);
	if (wounded)
	{
		format(text, sizeof text, "ранил игрока (%s)", weapon);
		Log_Player(LOG_KILL, issuerid, playerid, _:weaponid, text);
	}
}

stock Log_OnDriver(playerid)
{
	new v = GetPlayerVehicleID(playerid), text[64];
	if (v == 0) return;
	format(text, sizeof text, "сел за руль: модель %d, машина %d", GetVehicleModel(v), v);
	Log_Player(LOG_VEH, playerid, INVALID_PLAYER_ID, GetVehicleModel(v), text);
}

stock Log_OnFaction(f, const name[], const text[])
{
	new line[LOG_TEXT_LEN];
	format(line, sizeof line, "[%s] %s", name, text);
	Log_Push(LOG_FACTION, 0, "", 0, "", f, "", line);
}

stock Log_OnFamily(famDb, const name[], const text[])
{
	new line[LOG_TEXT_LEN];
	format(line, sizeof line, "[%s] %s", name, text);
	Log_Push(LOG_FAMILY, 0, "", 0, "", famDb, "", line);
}

stock Log_OnAntiCheat(playerid, const what[])
{
	Log_Player(LOG_AC, playerid, INVALID_PLAYER_ID, 0, what);
}

// ---------------------------------------------------------------------
//  Поиск в игре: /logs [ник|id|*] [тип|*] [текст]
// ---------------------------------------------------------------------
// Дата из unix-времени в местном часовом поясе: "15.11 20:07"
static Log_FormatDate(ts, out[], size = sizeof (out))
{
	new t = ts + gLogTz;
	new days = t / 86400, sec = t % 86400;
	new z = days + 719468;
	new era = z / 146097;
	new doe = z - era * 146097;
	new yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	new doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	new mp = (5 * doy + 2) / 153;
	new d = doy - (153 * mp + 2) / 5 + 1;
	new m = mp < 10 ? mp + 3 : mp - 9;
	format(out, size, "%02d.%02d %02d:%02d", d, m, sec / 3600, (sec % 3600) / 60);
}

// В ник и текст запроса не пускаем кавычки: ' в тексте становится «любой символ»
static Log_Sanitize(str[])
{
	for (new i = 0; str[i] != '\0'; i++)
	{
		if (str[i] == '\'' || str[i] == '\\') str[i] = '_';
	}
}

stock bool:Cmd_Logs(playerid, const params[])
{
	if (!Admin_Check(playerid, LOG_VIEW_LEVEL)) return true;
	if (gLogDB == DB:0) return SendClientMessage(playerid, COLOR_RED, "Журнал выключен: не открылась база logs.db.");

	new pos = 0, who[MAX_PLAYER_NAME], kind[16], text[64];
	NextParam(params, pos, who);
	NextParam(params, pos, kind);
	RestParam(params, pos, text);
	if (who[0] == '\0')
	{
		SendClientMessage(playerid, COLOR_GREY, "Использование: /logs [ник|id|*] [тип|*] [текст]. Пример: /logs Ivan_Petrov money, /logs * chat читер");
		SendClientMessage(playerid, COLOR_GREY, "Типы: conn chat cmd money bank item admin kill dmg veh faction family ac. Полный поиск — на сайте, страница /logs.");
	}
	if (!strcmp(who, "*") || !strcmp(who, "-")) who[0] = '\0';
	else if (IsNumeric(who))
	{
		new id = strval(who);
		if (id < 0 || id >= MAX_PLAYERS || !IsPlayerConnected(id)) return SendClientMessage(playerid, COLOR_GREY, "Игрока с таким id нет.");
		GetPlayerName(id, who, sizeof who);
	}
	new type = -1;
	if (kind[0] != '\0' && strcmp(kind, "*") && strcmp(kind, "-"))
	{
		for (new t = 0; t < LOG_TYPES; t++) if (!strcmp(kind, gLogTypeKeys[t], true)) type = t;
		if (type == -1) return SendClientMessage(playerid, COLOR_GREY, "Нет такого типа. Типы: conn chat cmd money bank item admin kill dmg veh faction family ac");
	}
	Log_Sanitize(who);
	Log_Sanitize(text);
	Log_Flush();    // чтобы видеть и самые свежие записи

	new where[256], part[96];
	strcat(where, "1");
	if (who[0] != '\0') { format(part, sizeof part, " AND (name = '%s' COLLATE NOCASE OR tname = '%s' COLLATE NOCASE)", who, who); strcat(where, part); }
	if (type != -1) { format(part, sizeof part, " AND type = '%s'", gLogTypeKeys[type]); strcat(where, part); }
	if (text[0] != '\0')
	{
		// Поиск по тексту без ника и типа просматривает таблицу целиком — ограничиваем последними записями
		format(part, sizeof part, " AND text LIKE '%%%s%%' AND id > (SELECT IFNULL(MAX(id), 0) FROM logs) - %d", text, LOG_VIEW_SCAN);
		strcat(where, part);
	}

	new DBResult:r = DB_ExecuteQuery(gLogDB, "SELECT time, type, name, tname, text FROM logs WHERE %s ORDER BY id DESC LIMIT %d", where, LOG_VIEW_ROWS);
	if (r == DBResult:0) return SendClientMessage(playerid, COLOR_RED, "Ошибка чтения журнала.");

	new out[3900], line[200], when[16], tkey[12], name[MAX_PLAYER_NAME], tname[MAX_PLAYER_NAME], body[LOG_TEXT_LEN];
	new rows = DB_GetRowCount(r);
	for (new i = 0; i < rows; i++)
	{
		Log_FormatDate(DB_GetFieldInt(r, 0), when);
		DB_GetFieldString(r, 1, tkey, sizeof tkey);
		DB_GetFieldString(r, 2, name, sizeof name);
		DB_GetFieldString(r, 3, tname, sizeof tname);
		DB_GetFieldString(r, 4, body, sizeof body);
		if (strlen(body) > 90) body[90] = '\0';
		if (tname[0] != '\0') format(line, sizeof line, "{AFAFAF}%s {FFCC66}%s {FFFFFF}%s -> %s: %s\n", when, tkey, name, tname, body);
		else format(line, sizeof line, "{AFAFAF}%s {FFCC66}%s {FFFFFF}%s %s\n", when, tkey, name, body);
		if (strlen(out) + strlen(line) >= sizeof out - 1) break;
		strcat(out, line);
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	if (rows == 0) strcat(out, "Ничего не найдено.");
	Admin_ShowInfo(playerid, "Журнал сервера", out);
	return true;
}
