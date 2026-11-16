// =====================================================================
//  admin.inc — система администрации.
//
//  Как стать первым админом:
//  1. В config.json задай свой rcon-пароль.
//  2. В игре введи: /rcon login ТВОЙ_ПАРОЛЬ
//  3. Введи: /makeadmin [свой id] 7
//  4. При следующих входах админка включается через /alogin (админ-пароль
//     придумываешь при первом /alogin или командой /apass).
//
//  Уровни: 1 — хелпер, 2 — модератор, 3 — администратор,
//          4 — старший администратор, 5 — куратор,
//          6 — заместитель главного, 7 — главный администратор.
//  Все команды по уровням: /ahelp
//
//  Что здесь есть:
//  - наказания с причинами и сроками: /kick /ban /offban /mute /warn /ajail
//    (бан, мут, варны и деморган хранятся в базе и переживают перезаход);
//  - лог всех действий админов в таблице admin_log, просмотр: /alog;
//  - репорты игроков: /report, у админов /reports и /ans;
//  - /a-чат, /admins, /check, /spec, /freeze, /slap;
//  - /amenu — панель через окна, удобная с телефона (не нужно вводить id);
//  - вход в админку по отдельному паролю /alogin, статистика /astats /alist
//    (admin_auth.inc).
//
//  Таблицы и новые поля создаются сами в Admin_Init (OnGameModeInit).
// =====================================================================

#define ADMIN_MAX_LEVEL         7       // главный администратор
#define ADMIN_IPBAN_LEVEL       6       // с какого уровня /banip и /unbanip

forward bool:Admin_IsOnDuty(playerid);   // admin_tools.inc
forward bool:Cmd_Backup(playerid);       // backup.inc
forward bool:AdminAuth_HasPassword(playerid);   // admin_auth.inc
forward AdminGive_OnCommand(playerid, const cmd[], const params[]);   // admin_give.inc
forward bool:Cmd_GiveGun(playerid, const params[]);
forward bool:Cmd_Disarm(playerid, const params[]);
forward bool:Cmd_SetArmour(playerid, const params[]);
forward bool:Cmd_AHeal(playerid, const params[]);
#define ADMIN_WARNS_FOR_BAN     3       // столько варнов — и автоматический бан...
#define ADMIN_WARN_BAN_DAYS     3       // ...на столько дней
#define ADMIN_MAX_MUTE          720     // максимум минут мута
#define ADMIN_MAX_AJAIL         720     // максимум минут деморгана
#define ADMIN_MAX_BAN_DAYS      3650

// Деморган (админ-тюрьма): та же камера, что и у полиции, но в отдельном мире
#define AJAIL_INTERIOR          6
#define AJAIL_WORLD             1001
#define AJAIL_X                 264.0
#define AJAIL_Y                 77.5
#define AJAIL_Z                 1001.04

#define REPORT_COOLDOWN         60      // секунд между репортами одного игрока
#define MAX_REPORTS             30      // открытых репортов одновременно

// Действия панели /amenu (порядок = порядок строк в окне)
enum
{
	AMENU_GOTO,
	AMENU_GETHERE,
	AMENU_SPEC,
	AMENU_CHECK,
	AMENU_FREEZE,
	AMENU_SLAP,
	AMENU_KICK,
	AMENU_MUTE,
	AMENU_WARN,
	AMENU_AJAIL,
	AMENU_BAN,
	AMENU_HEAL,
	AMENU_ARMOUR,
	AMENU_DISARM,
	AMENU_GIVEGUN
}

static gMuteUntil[MAX_PLAYERS];         // unix-время конца мута (0 = нет)
static gAJail[MAX_PLAYERS];             // минут деморгана осталось
static gWarns[MAX_PLAYERS];             // варны
static bool:gFrozen[MAX_PLAYERS];
static gReportLast[MAX_PLAYERS];        // когда игрок последний раз писал репорт

// Слежка (/spec): куда вернуть админа
static gSpecTarget[MAX_PLAYERS] = {INVALID_PLAYER_ID, ...};
static bool:gSpecRestore[MAX_PLAYERS];
static Float:gSpecPos[MAX_PLAYERS][3];
static gSpecInt[MAX_PLAYERS];
static gSpecWorld[MAX_PLAYERS];
static gSpecVeh[MAX_PLAYERS];
static gSpecTInt[MAX_PLAYERS];
static gSpecTWorld[MAX_PLAYERS];
static PLAYER_STATE:gSpecTState[MAX_PLAYERS];
static PlayerText:gSpecTd[MAX_PLAYERS] = {INVALID_PLAYER_TEXT_DRAW, ...};

// Панель /amenu
static gAMenuIds[MAX_PLAYERS][MAX_PLAYERS];   // строка списка -> id игрока
static gAMenuTarget[MAX_PLAYERS];
static gAMenuTargetDb[MAX_PLAYERS];
static gAMenuAction[MAX_PLAYERS];

// Открытые репорты (в памяти; копия с ответами — в таблице reports)
enum E_REPORT
{
	bool:rUsed,
	rDbId,              // номер репорта (id в таблице reports)
	rFrom,              // id игрока
	rFromDb,            // аккаунт игрока (защита от подмены id)
	rText[100],
	rTime,
	rTakenBy            // какой админ взял (INVALID_PLAYER_ID = никто)
}
static gReport[MAX_REPORTS][E_REPORT];
static gReportList[MAX_PLAYERS][MAX_REPORTS]; // строка окна /reports -> слот
static gReportSel[MAX_PLAYERS];               // выбранный слот репорта

// =====================================================================
//  Запуск: таблицы и поля в базе
// =====================================================================
stock Admin_Init()
{
	// Наказания игрока (в таблице accounts)
	DB_AddColumn("accounts", "mute_until", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "ajail", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "warns", "INTEGER DEFAULT 0");

	// Баны со сроком: until = 0 — навсегда
	DB_AddColumn("bans", "until", "INTEGER DEFAULT 0");
	DB_AddColumn("bans", "ip", "TEXT DEFAULT ''");

	// Баны по IP (/banip): until = 0 — навсегда
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS ip_bans (ip TEXT PRIMARY KEY, reason TEXT, admin TEXT, date INTEGER DEFAULT 0, until INTEGER DEFAULT 0)"));

	// Переход с 5 уровней админки на 7: прежний главный (5) становится главным (7).
	// Делается один раз — номер версии базы хранится в самой базе (PRAGMA user_version).
	new version = 0;
	new DBResult:vr = DB_ExecuteQuery(gDB, "PRAGMA user_version");
	if (vr != DBResult:0)
	{
		if (DB_GetRowCount(vr) > 0) version = DB_GetFieldInt(vr, 0);
		DB_FreeResultSet(vr);
	}
	if (version < 1)
	{
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET admin = 7 WHERE admin = 5"));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "PRAGMA user_version = 1"));
		print("[admin] Admin levels migrated: 5 -> 7");
	}

	AdminAuth_Init();       // поля админ-пароля и учёта времени (admin_auth.inc)

	// Лог действий админов
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS admin_log (id INTEGER PRIMARY KEY AUTOINCREMENT, date INTEGER, admin TEXT, admin_id INTEGER DEFAULT 0, action TEXT, target TEXT DEFAULT '', target_id INTEGER DEFAULT 0, value INTEGER DEFAULT 0, reason TEXT DEFAULT '')"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS admin_log_target ON admin_log (target COLLATE NOCASE, id)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS admin_log_admin ON admin_log (admin COLLATE NOCASE, id)"));

	// Репорты: status 0 — открыт, 1 — отвечен, 2 — закрыт без ответа
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS reports (id INTEGER PRIMARY KEY AUTOINCREMENT, date INTEGER, author TEXT, author_id INTEGER DEFAULT 0, text TEXT, status INTEGER DEFAULT 0, admin TEXT DEFAULT '', answer TEXT DEFAULT '', answer_date INTEGER DEFAULT 0)"));
	// Репорты, оставшиеся открытыми после перезапуска, закрываем
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE reports SET status = 2, answer = 'перезапуск сервера' WHERE status = 0"));

	for (new i = 0; i < MAX_REPORTS; i++) gReport[i][rUsed] = false;
	Teleports_Init();                   // admin_tools.inc
	SetTimer("Admin_SpecTick", 1000, true);
}

// =====================================================================
//  Уровни и общие помощники
// =====================================================================
stock Admin_LevelName(level)
{
	new name[28];
	switch (level)
	{
		case 1: strcat(name, "Хелпер");
		case 2: strcat(name, "Модератор");
		case 3: strcat(name, "Администратор");
		case 4: strcat(name, "Ст. администратор");
		case 5: strcat(name, "Куратор");
		case 6: strcat(name, "Зам. гл. администратора");
		case 7: strcat(name, "Гл. администратор");
		default: strcat(name, "Игрок");
	}
	return name;
}

// Проверка уровня. Пример: if (!Admin_Check(playerid, 3)) return true;
stock bool:Admin_Check(playerid, level)
{
	if (Player[playerid][pAdmin] >= level) return true;
	SendClientMessage(playerid, COLOR_GREY, "Недостаточно прав (нужен уровень админки %d — %s).", level, Admin_LevelName(level));
	return false;
}

// Можно ли наказать этого игрока (не себя и не админа своего уровня или выше)
stock bool:Admin_CanPunish(playerid, target)
{
	if (target == playerid)
	{
		SendClientMessage(playerid, COLOR_GREY, "Нельзя применить к себе.");
		return false;
	}
	if (Admin_RealLevel(target) >= Player[playerid][pAdmin])
	{
		SendClientMessage(playerid, COLOR_GREY, "Нельзя наказать админа своего уровня или выше.");
		return false;
	}
	return true;
}

stock Admin_PlatformTag(playerid)
{
	new tag[8];
	if (IsPlayerMobile[playerid]) strcat(tag, " [моб]");
	return tag;
}

// "3 дн. 4 ч." / "25 мин." из числа секунд
stock Admin_FormatTime(seconds)
{
	new text[32];
	if (seconds < 0) seconds = 0;
	if (seconds >= 86400) format(text, sizeof text, "%d дн. %d ч.", seconds / 86400, (seconds % 86400) / 3600);
	else if (seconds >= 3600) format(text, sizeof text, "%d ч. %d мин.", seconds / 3600, (seconds % 3600) / 60);
	else format(text, sizeof text, "%d мин.", (seconds + 59) / 60);
	return text;
}

// Окно-сообщение. На телефоне чат маленький, окно видно всегда.
stock Admin_ShowInfo(playerid, const title[], const text[])
{
	ShowPlayerDialog(playerid, DIALOG_ADMIN_INFO, DIALOG_STYLE_MSGBOX, title, "%s", "Закрыть", "", text);
}

// Разобрать "[число] остаток": если первое слово — число, оно попадает в value
static bool:Admin_ParseOptInt(const params[], pos, &value, rest[], size = sizeof (rest))
{
	new p = pos, buf[16];
	if (NextParam(params, p, buf) && IsNumeric(buf))
	{
		value = strval(buf);
		return RestParam(params, p, rest, size);
	}
	return RestParam(params, pos, rest, size);
}

// =====================================================================
//  Лог действий админов (таблица admin_log)
// =====================================================================
stock Admin_Log(adminid, const action[], target, const reason[] = "", value = 0)
{
	new admin[MAX_PLAYER_NAME], name[MAX_PLAYER_NAME], adminDb = 0, targetDb = 0;
	if (adminid != INVALID_PLAYER_ID)
	{
		GetPlayerName(adminid, admin, sizeof admin);
		adminDb = Player[adminid][pDbId];
	}
	else strcat(admin, "Сервер");
	if (target != INVALID_PLAYER_ID)
	{
		GetPlayerName(target, name, sizeof name);
		targetDb = Player[target][pDbId];
	}
	Admin_LogName(admin, adminDb, action, name, targetDb, reason, value);
}

stock Admin_LogName(const admin[], adminDb, const action[], const target[], targetDb, const reason[], value)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"INSERT INTO admin_log (date, admin, admin_id, action, target, target_id, value, reason) VALUES (%d, '%q', %d, '%q', '%q', %d, %d, '%q')",
		gettime(), admin, adminDb, action, target, targetDb, value, reason));
	Log_OnAdmin(admin, adminDb, action, target, targetDb, reason, value);
	printf("[admin] %s: %s %s (%d) %s", admin, action, target, value, reason);
}

// Для доната (donate.inc): сколько предупреждений и снять все
stock Admin_GetWarns(playerid) { return gWarns[playerid]; }
stock Admin_ClearWarns(playerid)
{
	gWarns[playerid] = 0;
	Admin_SaveState(playerid);
}

// Сохранить наказания игрока в базу сразу (не ждём Account_Save)
static Admin_SaveState(playerid)
{
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET mute_until = %d, ajail = %d, warns = %d WHERE id = %d",
		gMuteUntil[playerid], gAJail[playerid], gWarns[playerid], Player[playerid][pDbId]));
}

// =====================================================================
//  События игрока (вызываются из rp.pwn и accounts.inc)
// =====================================================================
stock Admin_OnConnect(playerid)
{
	gMuteUntil[playerid] = 0;
	gAJail[playerid] = 0;
	gWarns[playerid] = 0;
	gFrozen[playerid] = false;
	gReportLast[playerid] = 0;
	gSpecTarget[playerid] = INVALID_PLAYER_ID;
	gSpecRestore[playerid] = false;
	gAMenuTarget[playerid] = INVALID_PLAYER_ID;
	AdminTools_OnConnect(playerid);     // admin_tools.inc
	AdminAuth_OnConnect(playerid);      // admin_auth.inc
}

// После входа в аккаунт (accounts.inc, Account_FinishLogin)
stock Admin_OnLogin(playerid)
{
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT mute_until, ajail, warns FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			gMuteUntil[playerid] = DB_GetFieldInt(r, 0);
			gAJail[playerid] = DB_GetFieldInt(r, 1);
			gWarns[playerid] = DB_GetFieldInt(r, 2);
		}
		DB_FreeResultSet(r);
	}
	if (gMuteUntil[playerid] != 0 && gMuteUntil[playerid] <= gettime())
	{
		gMuteUntil[playerid] = 0;
		Admin_SaveState(playerid);
	}
	if (gMuteUntil[playerid] > 0)
		SendClientMessage(playerid, COLOR_RED, "У тебя мут: чат недоступен ещё %s", Admin_FormatTime(gMuteUntil[playerid] - gettime()));
	if (gWarns[playerid] > 0)
		SendClientMessage(playerid, COLOR_ORANGE, "Предупреждений: %d из %d.", gWarns[playerid], ADMIN_WARNS_FOR_BAN);

	// Админ: права включаются только после /alogin (admin_auth.inc)
	if (Player[playerid][pAdmin] > 0) AdminAuth_OnLogin(playerid);
}

// Админ получил права (после /alogin или сразу, если вход по паролю выключен)
stock Admin_Welcome(playerid)
{
	new open = Reports_CountOpen();
	SendClientMessage(playerid, COLOR_ADMIN, "Ты вошёл как %s (уровень %d). Команды: /ahelp, панель: /amenu.",
		Admin_LevelName(Player[playerid][pAdmin]), Player[playerid][pAdmin]);
	if (open > 0) SendClientMessage(playerid, COLOR_ADMIN, "Открытых репортов: %d — /reports", open);

	new msg[144];
	format(msg, sizeof msg, "[A] %s %s[%d]%s вошёл в админку.", Admin_LevelName(Player[playerid][pAdmin]),
		GetRPName(playerid), playerid, Admin_PlatformTag(playerid));
	SendAdminMessage(COLOR_ADMIN, msg);
}

stock Admin_OnDisconnect(playerid)
{
	// Кто следил за этим игроком — возвращаем на место
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gSpecTarget[i] == playerid && IsPlayerConnected(i))
		{
			SendClientMessage(i, COLOR_ADMIN, "Игрок вышел, слежка выключена.");
			Admin_StopSpec(i);
		}
		if (gAMenuTarget[i] == playerid) gAMenuTarget[i] = INVALID_PLAYER_ID;
	}
	// Его репорты закрываем, а взятые им — освобождаем
	for (new r = 0; r < MAX_REPORTS; r++)
	{
		if (!gReport[r][rUsed]) continue;
		if (gReport[r][rFrom] == playerid) Reports_Close(r, playerid, 2, "автор вышел");
		else if (gReport[r][rTakenBy] == playerid) gReport[r][rTakenBy] = INVALID_PLAYER_ID;
	}
	gSpecTarget[playerid] = INVALID_PLAYER_ID;
	gSpecRestore[playerid] = false;
	gFrozen[playerid] = false;
	gSpecTd[playerid] = INVALID_PLAYER_TEXT_DRAW;     // текстдравы игрока удаляются сами при выходе
}

// Из OnPlayerSpawn — после Factions_OnSpawn
stock Admin_OnSpawn(playerid)
{
	if (gSpecRestore[playerid])
	{
		gSpecRestore[playerid] = false;
		SetPlayerInterior(playerid, gSpecInt[playerid]);
		SetPlayerVirtualWorld(playerid, gSpecWorld[playerid]);
		SetPlayerPos(playerid, gSpecPos[playerid][0], gSpecPos[playerid][1], gSpecPos[playerid][2]);
	}
	if (gAJail[playerid] > 0)
	{
		AJail_PutInCell(playerid);
		SendClientMessage(playerid, COLOR_RED, "Ты в деморгане. Осталось: %d мин.", gAJail[playerid]);
	}
	if (gFrozen[playerid]) TogglePlayerControllable(playerid, false);
	AdminTools_OnSpawn(playerid);
}

stock bool:Admin_IsSpectating(playerid)
{
	return (gSpecTarget[playerid] != INVALID_PLAYER_ID);
}

stock bool:Admin_IsInAJail(playerid)
{
	return (gAJail[playerid] > 0);
}

// Из OnPlayerText: true — игрок в муте, сообщение не отправлять
stock bool:Admin_IsMuted(playerid)
{
	if (gMuteUntil[playerid] == 0) return false;
	new left = gMuteUntil[playerid] - gettime();
	if (left <= 0)
	{
		gMuteUntil[playerid] = 0;
		Admin_SaveState(playerid);
		SendClientMessage(playerid, COLOR_GREEN, "Мут закончился, чат снова доступен.");
		return false;
	}
	SendClientMessage(playerid, COLOR_RED, "У тебя мут. Осталось: %s", Admin_FormatTime(left));
	return true;
}

// Раз в минуту (OnMinuteTimer)
stock Admin_OnMinute()
{
	AdminAuth_OnMinute();       // время в админке (admin_auth.inc)
	new now = gettime();
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged]) continue;

		if (gMuteUntil[i] != 0 && gMuteUntil[i] <= now)
		{
			gMuteUntil[i] = 0;
			Admin_SaveState(i);
			SendClientMessage(i, COLOR_GREEN, "Мут закончился, чат снова доступен.");
		}

		if (gAJail[i] > 0)
		{
			gAJail[i]--;
			Admin_SaveState(i);
			if (gAJail[i] <= 0) AJail_Release(i);
			else
			{
				if (GetPlayerVirtualWorld(i) != AJAIL_WORLD || !IsPlayerInRangeOfPoint(i, 15.0, AJAIL_X, AJAIL_Y, AJAIL_Z))
					AJail_PutInCell(i);
				SendClientMessage(i, COLOR_GREY, "До выхода из деморгана: %d мин.", gAJail[i]);
			}
		}
	}
}

// =====================================================================
//  Баны (в базе). Проверка при входе — из accounts.inc
// =====================================================================
stock Bans_IsBanned(playerid, const name[])
{
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT reason, admin, until FROM bans WHERE name = '%q' LIMIT 1", name);
	if (r == DBResult:0) return 0;
	new banned = 0;
	if (DB_GetRowCount(r) > 0)
	{
		new reason[64], admin[MAX_PLAYER_NAME], until = DB_GetFieldInt(r, 2);
		DB_GetFieldString(r, 0, reason, sizeof reason);
		DB_GetFieldString(r, 1, admin, sizeof admin);
		if (until > 0 && until <= gettime())
		{
			// срок вышел — снимаем бан сам
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM bans WHERE name = '%q'", name));
			Admin_LogName("Сервер", 0, "unban (срок вышел)", name, 0, reason, 0);
		}
		else
		{
			new text[256], term[40];
			if (until > 0) format(term, sizeof term, "ещё %s", Admin_FormatTime(until - gettime()));
			else strcat(term, "навсегда");
			format(text, sizeof text, "{FF6347}Аккаунт заблокирован.\n\n{FFFFFF}Причина: %s\nАдминистратор: %s\nСрок: %s\n\nНе согласен — пиши жалобу на форуме.",
				reason, admin, term);
			SendClientMessage(playerid, COLOR_RED, "Ты забанен (%s). Причина: %s (админ %s).", term, reason, admin);
			Admin_ShowInfo(playerid, "Бан", text);
			KickWithDelay(playerid);
			banned = 1;
		}
	}
	DB_FreeResultSet(r);
	return banned;
}

// Бан по IP. Проверка при входе — из accounts.inc
stock Bans_IsIpBanned(playerid)
{
	new ip[16];
	GetPlayerIp(playerid, ip, sizeof ip);
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT reason, admin, until FROM ip_bans WHERE ip = '%q' LIMIT 1", ip);
	if (r == DBResult:0) return 0;
	new banned = 0;
	if (DB_GetRowCount(r) > 0)
	{
		new reason[64], admin[MAX_PLAYER_NAME], until = DB_GetFieldInt(r, 2);
		DB_GetFieldString(r, 0, reason, sizeof reason);
		DB_GetFieldString(r, 1, admin, sizeof admin);
		if (until > 0 && until <= gettime())
		{
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM ip_bans WHERE ip = '%q'", ip));
			Admin_LogName("Сервер", 0, "unbanip (срок вышел)", ip, 0, reason, 0);
		}
		else
		{
			new text[256], term[40];
			if (until > 0) format(term, sizeof term, "ещё %s", Admin_FormatTime(until - gettime()));
			else strcat(term, "навсегда");
			format(text, sizeof text, "{FF6347}Вход с твоего адреса заблокирован.\n\n{FFFFFF}Причина: %s\nАдминистратор: %s\nСрок: %s\n\nНе согласен — пиши жалобу на форуме.",
				reason, admin, term);
			SendClientMessage(playerid, COLOR_RED, "Твой IP забанен (%s). Причина: %s (админ %s).", term, reason, admin);
			Admin_ShowInfo(playerid, "Бан по IP", text);
			KickWithDelay(playerid);
			banned = 1;
		}
	}
	DB_FreeResultSet(r);
	return banned;
}

// ---------------------------------------------------------------------
//  Пауза на вход: после MAX_LOGIN_TRIES неверных паролей с этого IP
//  LOGIN_LOCK_SECONDS секунд не пускаем. Так пароль нельзя перебирать,
//  просто заходя снова. Хранится в памяти, перезапуск сервера её сбрасывает.
// ---------------------------------------------------------------------
static gLockIp[LOGIN_LOCK_SLOTS][16];
static gLockUntil[LOGIN_LOCK_SLOTS];

stock Login_Lock(playerid)
{
	new ip[16], slot = 0, now = gettime();
	GetPlayerIp(playerid, ip, sizeof ip);
	for (new i = 0; i < LOGIN_LOCK_SLOTS; i++)
	{
		if (gLockUntil[i] > now && !strcmp(gLockIp[i], ip) && ip[0] != '\0') { slot = i; break; }
		if (gLockUntil[i] < gLockUntil[slot]) slot = i;     // иначе занимаем самую старую запись
	}
	gLockIp[slot][0] = '\0';
	strcat(gLockIp[slot], ip, 16);
	gLockUntil[slot] = now + LOGIN_LOCK_SECONDS;
	printf("[login] ip %s locked for %d s: too many wrong passwords", ip, LOGIN_LOCK_SECONDS);
}

// Сколько секунд этому игроку ещё нельзя входить (0 — можно)
stock Login_LockLeft(playerid)
{
	new ip[16], now = gettime();
	GetPlayerIp(playerid, ip, sizeof ip);
	if (ip[0] == '\0') return 0;
	for (new i = 0; i < LOGIN_LOCK_SLOTS; i++)
	{
		if (gLockUntil[i] > now && !strcmp(gLockIp[i], ip)) return gLockUntil[i] - now;
	}
	return 0;
}

// Похоже ли на IPv4-адрес: только цифры и три точки
static bool:Admin_IsIp(const text[])
{
	new dots = 0, len = strlen(text);
	if (len < 7 || len > 15) return false;
	for (new i = 0; i < len; i++)
	{
		if (text[i] == '.') dots++;
		else if (text[i] < '0' || text[i] > '9') return false;
	}
	return (dots == 3);
}

// /banip [id или IP] [дни, 0 — навсегда] [причина]
stock bool:Cmd_BanIp(playerid, const params[])
{
	if (!Admin_Check(playerid, ADMIN_IPBAN_LEVEL)) return true;
	new pos = 0, first[24], ip[16], days = 0, reason[64];
	if (!NextParam(params, pos, first) || !Admin_ParseOptInt(params, pos, days, reason))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /banip [id игрока или IP] [дни, 0 — навсегда] [причина]");
	if (days < 0 || days > ADMIN_MAX_BAN_DAYS) return SendClientMessage(playerid, COLOR_GREY, "Срок от 0 до %d дней (0 — навсегда).", ADMIN_MAX_BAN_DAYS);

	if (IsNumeric(first))
	{
		new target = strval(first);
		if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target)) return SendClientMessage(playerid, COLOR_GREY, "Игрока с таким id нет на сервере.");
		if (!Admin_CanPunish(playerid, target)) return true;
		GetPlayerIp(target, ip, sizeof ip);
	}
	else if (Admin_IsIp(first)) strcat(ip, first, sizeof ip);
	else return SendClientMessage(playerid, COLOR_GREY, "Укажи id игрока на сервере или IP вида 12.34.56.78.");

	new admin[MAX_PLAYER_NAME], until = (days > 0) ? (gettime() + days * 86400) : 0;
	GetPlayerName(playerid, admin, sizeof admin);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO ip_bans (ip, reason, admin, date, until) VALUES ('%q', '%q', '%q', %d, %d)",
		ip, reason, admin, gettime(), until));
	Admin_LogName(admin, Player[playerid][pDbId], "banip", ip, 0, reason, days);

	new msg[144], kicked = 0, other[16], text[200];
	format(text, sizeof text, "{FF6347}Вход с твоего адреса заблокирован %s.\n\n{FFFFFF}Причина: %s\nАдминистратор: %s\n\nНе согласен — пиши жалобу на форуме.", Admin_BanText(days), reason, admin);
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || i == playerid) continue;
		GetPlayerIp(i, other, sizeof other);
		if (strcmp(other, ip) != 0 || other[0] == '\0') continue;
		if (Admin_RealLevel(i) >= Player[playerid][pAdmin]) continue;    // админа своего уровня и выше не трогаем
		Admin_ShowInfo(i, "Бан по IP", text);
		KickWithDelay(i);
		kicked++;
	}
	format(msg, sizeof msg, "[A] %s забанил IP %s %s (кикнуто игроков: %d). Причина: %s", GetRPName(playerid), ip, Admin_BanText(days), kicked, reason);
	SendAdminMessage(COLOR_ADMIN, msg);
	return true;
}

stock bool:Cmd_UnbanIp(playerid, const params[])
{
	if (!Admin_Check(playerid, ADMIN_IPBAN_LEVEL)) return true;
	new pos = 0, ip[24];
	if (!NextParam(params, pos, ip) || !Admin_IsIp(ip)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /unbanip [IP]");

	new found = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM ip_bans WHERE ip = '%q'", ip);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) found = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (found == 0) return SendClientMessage(playerid, COLOR_GREY, "IP %s не в бане.", ip);

	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM ip_bans WHERE ip = '%q'", ip));
	new admin[MAX_PLAYER_NAME], msg[144];
	GetPlayerName(playerid, admin, sizeof admin);
	Admin_LogName(admin, Player[playerid][pDbId], "unbanip", ip, 0, "", 0);
	format(msg, sizeof msg, "[A] %s снял бан с IP %s.", GetRPName(playerid), ip);
	SendAdminMessage(COLOR_ADMIN, msg);
	return true;
}

// Записать бан в базу. days = 0 — навсегда
static Bans_Add(const name[], const ip[], const reason[], const admin[], days)
{
	new until = (days > 0) ? (gettime() + days * 86400) : 0;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO bans (name, reason, admin, date, until, ip) VALUES ('%q', '%q', '%q', %d, %d, '%q')",
		name, reason, admin, gettime(), until, ip));
	new text[200];
	if (days > 0) format(text, sizeof text, "Аккаунт %s заблокирован на %d дн. Причина: %s. Администратор: %s.", name, days, reason, admin);
	else format(text, sizeof text, "Аккаунт %s заблокирован навсегда. Причина: %s. Администратор: %s.", name, reason, admin);
	Social_NotifyName(name, text);   // уведомление в ВК/ТГ (social.inc)
}

static Admin_BanText(days)
{
	new text[24];
	if (days > 0) format(text, sizeof text, "на %d дн.", days);
	else strcat(text, "навсегда");
	return text;
}

// Бан игрока онлайн (используется /ban и автобаном за варны)
static Admin_BanPlayer(adminid, target, days, const reason[])
{
	new name[MAX_PLAYER_NAME], admin[MAX_PLAYER_NAME], ip[16];
	GetPlayerName(target, name, sizeof name);
	GetPlayerIp(target, ip, sizeof ip);
	if (adminid != INVALID_PLAYER_ID) GetPlayerName(adminid, admin, sizeof admin);
	else strcat(admin, "Сервер");

	Bans_Add(name, ip, reason, admin, days);
	Admin_Log(adminid, "ban", target, reason, days);

	if (adminid != INVALID_PLAYER_ID)
		SendClientMessageToAll(COLOR_RED, "Администратор %s забанил %s %s. Причина: %s", GetRPName(adminid), GetRPName(target), Admin_BanText(days), reason);
	else
		SendClientMessageToAll(COLOR_RED, "%s забанен %s. Причина: %s", GetRPName(target), Admin_BanText(days), reason);

	new text[200];
	format(text, sizeof text, "{FF6347}Ты забанен %s.\n\n{FFFFFF}Причина: %s\nАдминистратор: %s\n\nНе согласен — пиши жалобу на форуме.", Admin_BanText(days), reason, admin);
	Admin_ShowInfo(target, "Бан", text);
	KickWithDelay(target);
}

stock bool:Cmd_Ban(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new pos = 0, days = 0, reason[64];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !Admin_ParseOptInt(params, pos, days, reason))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /ban [id] [дни, 0 — навсегда] [причина]");
	if (days < 0 || days > ADMIN_MAX_BAN_DAYS) return SendClientMessage(playerid, COLOR_GREY, "Срок от 0 до %d дней (0 — навсегда).", ADMIN_MAX_BAN_DAYS);
	if (days == 0 && Player[playerid][pAdmin] < 4) return SendClientMessage(playerid, COLOR_GREY, "Бан навсегда — с 4 уровня. Укажи срок в днях.");
	if (!Admin_CanPunish(playerid, target)) return true;
	Admin_BanPlayer(playerid, target, days, reason);
	return true;
}

// Бан по нику, когда игрока нет на сервере
stock bool:Cmd_OffBan(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new pos = 0, name[MAX_PLAYER_NAME], days = 0, reason[64];
	if (!NextParam(params, pos, name) || !Admin_ParseOptInt(params, pos, days, reason))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /offban [ник] [дни, 0 — навсегда] [причина]");
	if (days < 0 || days > ADMIN_MAX_BAN_DAYS) return SendClientMessage(playerid, COLOR_GREY, "Срок от 0 до %d дней (0 — навсегда).", ADMIN_MAX_BAN_DAYS);
	if (days == 0 && Player[playerid][pAdmin] < 4) return SendClientMessage(playerid, COLOR_GREY, "Бан навсегда — с 4 уровня. Укажи срок в днях.");

	// Игрок на сервере — обычный бан
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i)) continue;
		new other[MAX_PLAYER_NAME];
		GetPlayerName(i, other, sizeof other);
		if (!strcmp(other, name, true))
		{
			if (!Admin_CanPunish(playerid, i)) return true;
			Admin_BanPlayer(playerid, i, days, reason);
			return true;
		}
	}

	new adminLevel = -1, accId = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, admin, name FROM accounts WHERE name = '%q' LIMIT 1", name);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			accId = DB_GetFieldInt(r, 0);
			adminLevel = DB_GetFieldInt(r, 1);
			DB_GetFieldString(r, 2, name, sizeof name);   // точное написание ника
		}
		DB_FreeResultSet(r);
	}
	if (adminLevel < 0) return SendClientMessage(playerid, COLOR_GREY, "Аккаунт %s не найден.", name);
	if (adminLevel >= Player[playerid][pAdmin]) return SendClientMessage(playerid, COLOR_GREY, "Нельзя наказать админа своего уровня или выше.");

	new admin[MAX_PLAYER_NAME];
	GetPlayerName(playerid, admin, sizeof admin);
	Bans_Add(name, "", reason, admin, days);
	Admin_LogName(admin, Player[playerid][pDbId], "offban", name, accId, reason, days);
	SendClientMessage(playerid, COLOR_ADMIN, "Аккаунт %s забанен %s. Причина: %s", name, Admin_BanText(days), reason);
	return true;
}

stock bool:Cmd_Unban(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, name[MAX_PLAYER_NAME];
	if (!NextParam(params, pos, name)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /unban [ник]");

	new found = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM bans WHERE name = '%q'", name);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) found = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (found == 0) return SendClientMessage(playerid, COLOR_GREY, "%s не в бане.", name);

	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM bans WHERE name = '%q'", name));
	new admin[MAX_PLAYER_NAME];
	GetPlayerName(playerid, admin, sizeof admin);
	Admin_LogName(admin, Player[playerid][pDbId], "unban", name, 0, "", 0);
	SendClientMessage(playerid, COLOR_ADMIN, "Бан %s снят.", name);
	return true;
}

// =====================================================================
//  Кик, мут, варны, деморган
// =====================================================================
stock bool:Cmd_Kick(playerid, const params[])
{
	if (!Admin_Check(playerid, 1)) return true;
	new pos = 0, reason[64];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !RestParam(params, pos, reason))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /kick [id] [причина]");
	if (!Admin_CanPunish(playerid, target)) return true;

	Admin_Log(playerid, "kick", target, reason);
	SendClientMessageToAll(COLOR_RED, "Администратор %s кикнул %s. Причина: %s", GetRPName(playerid), GetRPName(target), reason);
	new text[160];
	format(text, sizeof text, "{FF6347}Тебя кикнули с сервера.\n\n{FFFFFF}Причина: %s\nАдминистратор: %s", reason, GetRPName(playerid));
	Admin_ShowInfo(target, "Кик", text);
	KickWithDelay(target);
	return true;
}

stock bool:Cmd_Mute(playerid, const params[])
{
	if (!Admin_Check(playerid, 1)) return true;
	new pos = 0, minutes, reason[64];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, minutes) || !RestParam(params, pos, reason))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /mute [id] [минуты] [причина]");
	if (minutes < 1 || minutes > ADMIN_MAX_MUTE) return SendClientMessage(playerid, COLOR_GREY, "Срок от 1 до %d минут.", ADMIN_MAX_MUTE);
	if (!Admin_CanPunish(playerid, target)) return true;

	gMuteUntil[target] = gettime() + minutes * 60;
	Admin_SaveState(target);
	Admin_Log(playerid, "mute", target, reason, minutes);
	SendClientMessageToAll(COLOR_RED, "Администратор %s выдал мут %s на %d мин. Причина: %s", GetRPName(playerid), GetRPName(target), minutes, reason);
	return true;
}

stock bool:Cmd_Unmute(playerid, const params[])
{
	if (!Admin_Check(playerid, 1)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /unmute [id]");
	if (gMuteUntil[target] == 0) return SendClientMessage(playerid, COLOR_GREY, "У игрока нет мута.");

	gMuteUntil[target] = 0;
	Admin_SaveState(target);
	Admin_Log(playerid, "unmute", target);
	SendClientMessage(target, COLOR_GREEN, "%s снял с тебя мут.", GetRPName(playerid));
	SendClientMessage(playerid, COLOR_ADMIN, "Мут с %s снят.", GetRPName(target));
	return true;
}

stock bool:Cmd_Warn(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0, reason[64];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !RestParam(params, pos, reason))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /warn [id] [причина]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	if (!Admin_CanPunish(playerid, target)) return true;

	gWarns[target]++;
	Admin_Log(playerid, "warn", target, reason, gWarns[target]);
	SendClientMessageToAll(COLOR_RED, "Администратор %s выдал предупреждение %s (%d/%d). Причина: %s",
		GetRPName(playerid), GetRPName(target), gWarns[target], ADMIN_WARNS_FOR_BAN, reason);

	if (gWarns[target] >= ADMIN_WARNS_FOR_BAN)
	{
		gWarns[target] = 0;
		Admin_SaveState(target);
		new why[64];
		format(why, sizeof why, "%d/%d предупреждений (%s)", ADMIN_WARNS_FOR_BAN, ADMIN_WARNS_FOR_BAN, reason);
		Admin_BanPlayer(INVALID_PLAYER_ID, target, ADMIN_WARN_BAN_DAYS, why);
	}
	else Admin_SaveState(target);
	return true;
}

stock bool:Cmd_Unwarn(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /unwarn [id]");
	if (gWarns[target] <= 0) return SendClientMessage(playerid, COLOR_GREY, "У игрока нет предупреждений.");

	gWarns[target]--;
	Admin_SaveState(target);
	Admin_Log(playerid, "unwarn", target, "", gWarns[target]);
	SendClientMessage(target, COLOR_GREEN, "%s снял с тебя одно предупреждение (%d/%d).", GetRPName(playerid), gWarns[target], ADMIN_WARNS_FOR_BAN);
	SendClientMessage(playerid, COLOR_ADMIN, "Предупреждений у %s: %d.", GetRPName(target), gWarns[target]);
	return true;
}

// Добавить минут деморгана без участия админа (метка задержания, law.inc).
// Игрок попадёт в камеру при следующем появлении.
stock Admin_AddAJail(playerid, minutes)
{
	if (minutes <= 0 || Player[playerid][pDbId] == 0) return;
	gAJail[playerid] += minutes;
	if (gAJail[playerid] > ADMIN_MAX_AJAIL) gAJail[playerid] = ADMIN_MAX_AJAIL;
	Admin_SaveState(playerid);
}

stock AJail_PutInCell(playerid)
{
	SetPlayerInterior(playerid, AJAIL_INTERIOR);
	SetPlayerVirtualWorld(playerid, AJAIL_WORLD);
	SetPlayerPos(playerid, AJAIL_X, AJAIL_Y, AJAIL_Z);
	ResetPlayerWeapons(playerid);
	Player[playerid][pHouseInside] = -1;
}

stock AJail_Release(playerid)
{
	gAJail[playerid] = 0;
	Admin_SaveState(playerid);
	if (Player[playerid][pJail] > 0)
	{
		Jail_PutInCell(playerid);   // ещё сидит в обычной тюрьме (factions.inc)
	}
	else
	{
		SetPlayerInterior(playerid, 0);
		SetPlayerVirtualWorld(playerid, 0);
		SetPlayerPos(playerid, SPAWN_X, SPAWN_Y, SPAWN_Z);
		SetPlayerFacingAngle(playerid, SPAWN_A);
	}
	SendClientMessage(playerid, COLOR_GREEN, "Ты вышел из деморгана. Соблюдай правила сервера.");
}

stock bool:Cmd_AJail(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0, minutes, reason[64];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, minutes) || !RestParam(params, pos, reason))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /ajail [id] [минуты] [причина]");
	if (minutes < 1 || minutes > ADMIN_MAX_AJAIL) return SendClientMessage(playerid, COLOR_GREY, "Срок от 1 до %d минут.", ADMIN_MAX_AJAIL);
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	if (!Admin_CanPunish(playerid, target)) return true;

	if (IsPlayerInAnyVehicle(target)) RemovePlayerFromVehicle(target);
	gAJail[target] = minutes;
	Admin_SaveState(target);
	AJail_PutInCell(target);
	Admin_Log(playerid, "ajail", target, reason, minutes);
	SendClientMessageToAll(COLOR_RED, "Администратор %s посадил %s в деморган на %d мин. Причина: %s", GetRPName(playerid), GetRPName(target), minutes, reason);
	return true;
}

stock bool:Cmd_UnAJail(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /unajail [id]");
	if (gAJail[target] <= 0) return SendClientMessage(playerid, COLOR_GREY, "Игрок не в деморгане.");

	Admin_Log(playerid, "unajail", target);
	AJail_Release(target);
	SendClientMessage(playerid, COLOR_ADMIN, "%s выпущен из деморгана.", GetRPName(target));
	return true;
}

// =====================================================================
//  Наблюдение и помощь: /check /spec /freeze /slap /goto /gethere
// =====================================================================
stock bool:Cmd_Check(playerid, const params[])
{
	if (!Admin_Check(playerid, 1)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /check [id]");

	new text[700], ip[32], mute[40], platform[16];
	if (Player[playerid][pAdmin] >= 3) GetPlayerIp(target, ip, sizeof ip);
	else strcat(ip, "скрыт (с 3 уровня)");
	if (gMuteUntil[target] > gettime()) format(mute, sizeof mute, "%s", Admin_FormatTime(gMuteUntil[target] - gettime()));
	else strcat(mute, "нет");
	switch (GetPlayerPlatform(target))
	{
		case PLATFORM_MOBILE: strcat(platform, "телефон");
		case PLATFORM_PC: strcat(platform, "ПК");
		default: strcat(platform, "определяется");
	}
	format(text, sizeof text,
		"{FFFFFF}Игрок: {33CCFF}%s [%d]\n{FFFFFF}Аккаунт №%d, уровень %d\nПлатформа: %s   Пинг: %d   FPS: %d\nIP: %s\n\nПредупреждения: %d/%d\nМут: %s\nДеморган: %d мин.   Тюрьма: %d мин.\nЗаморожен: %s\nАдминка: %s\n\nДеньги: $%d (банк $%d)",
		GetRPName(target), target, Player[target][pDbId], Player[target][pLevel],
		platform, GetPlayerPing(target), Fps_Get(target), ip,
		gWarns[target], ADMIN_WARNS_FOR_BAN, mute, gAJail[target], Player[target][pJail],
		gFrozen[target] ? ("да") : ("нет"), Admin_LevelName(Admin_RealLevel(target)),
		GetMoney(target), Player[target][pBank]);
	Admin_ShowInfo(playerid, "Проверка игрока", text);
	return true;
}

// ---------------------------------------------------------------------
//  Слежка (/spec). Раз в секунду (Admin_SpecTick) камера сама
//  переключается, если игрок сел в машину, вышел, сменил интерьер или
//  умер, а внизу экрана — HP, броня, скорость, пинг цели.
//  Листать игроков: /specnext /specprev или ЛКМ / ПКМ в слежке.
// ---------------------------------------------------------------------

static Admin_SpecApply(playerid)
{
	new target = gSpecTarget[playerid];
	gSpecTInt[playerid] = GetPlayerInterior(target);
	gSpecTWorld[playerid] = GetPlayerVirtualWorld(target);
	gSpecTState[playerid] = GetPlayerState(target);
	SetPlayerInterior(playerid, gSpecTInt[playerid]);
	SetPlayerVirtualWorld(playerid, gSpecTWorld[playerid]);
	gSpecVeh[playerid] = GetPlayerVehicleID(target);
	if (gSpecVeh[playerid] != 0) PlayerSpectateVehicle(playerid, gSpecVeh[playerid]);
	else PlayerSpectatePlayer(playerid, target);
}

static Admin_SpecHud(playerid)
{
	new target = gSpecTarget[playerid];
	if (gSpecTd[playerid] == INVALID_PLAYER_TEXT_DRAW)
	{
		gSpecTd[playerid] = CreatePlayerTextDraw(playerid, 320.0, 360.0, "_");
		PlayerTextDrawAlignment(playerid, gSpecTd[playerid], TEXT_DRAW_ALIGN_CENTER);
		PlayerTextDrawFont(playerid, gSpecTd[playerid], TEXT_DRAW_FONT_1);
		PlayerTextDrawLetterSize(playerid, gSpecTd[playerid], 0.26, 1.1);
		PlayerTextDrawColour(playerid, gSpecTd[playerid], 0xFFFFFFFF);
		PlayerTextDrawSetOutline(playerid, gSpecTd[playerid], 1);
		PlayerTextDrawSetProportional(playerid, gSpecTd[playerid], true);
	}
	new name[MAX_PLAYER_NAME], Float:hp, Float:armour, Float:vx, Float:vy, Float:vz, speed;
	GetPlayerName(target, name, sizeof name);
	GetPlayerHealth(target, hp);
	GetPlayerArmour(target, armour);
	if (gSpecVeh[playerid] != 0) GetVehicleVelocity(gSpecVeh[playerid], vx, vy, vz);
	else GetPlayerVelocity(target, vx, vy, vz);
	speed = floatround(floatsqroot(vx * vx + vy * vy + vz * vz) * 180.0);
	new text[200];
	format(text, sizeof text, "~p~%s [%d]%s~n~~w~HP %d  Armour %d  %d km/h  Ping %d  Int %d  VW %d~n~~g~/specnext  /specprev  /specoff",
		name, target, IsPlayerMobile[target] ? (" ~y~(mobile)") : (""), floatround(hp), floatround(armour), speed,
		GetPlayerPing(target), gSpecTInt[playerid], gSpecTWorld[playerid]);
	PlayerTextDrawSetString(playerid, gSpecTd[playerid], "%s", text);
	PlayerTextDrawShow(playerid, gSpecTd[playerid]);
}

static Admin_SpecHudHide(playerid)
{
	if (gSpecTd[playerid] == INVALID_PLAYER_TEXT_DRAW) return;
	PlayerTextDrawDestroy(playerid, gSpecTd[playerid]);
	gSpecTd[playerid] = INVALID_PLAYER_TEXT_DRAW;
}

forward Admin_SpecTick();
public Admin_SpecTick()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		new target = gSpecTarget[i];
		if (target == INVALID_PLAYER_ID || !IsPlayerConnected(i)) continue;
		if (!IsPlayerConnected(target)) { Admin_StopSpec(i); continue; }
		if (GetPlayerVehicleID(target) != gSpecVeh[i] || GetPlayerInterior(target) != gSpecTInt[i]
			|| GetPlayerVirtualWorld(target) != gSpecTWorld[i] || GetPlayerState(target) != gSpecTState[i])
		{
			Admin_SpecApply(i);
		}
		Admin_SpecHud(i);
	}
	return 1;
}

stock Admin_StopSpec(playerid)
{
	Admin_SpecHudHide(playerid);
	if (gSpecTarget[playerid] == INVALID_PLAYER_ID) return;
	gSpecTarget[playerid] = INVALID_PLAYER_ID;
	gSpecRestore[playerid] = true;      // вернём на место в Admin_OnSpawn
	TogglePlayerSpectating(playerid, false);
}

static bool:Admin_CanSpecTarget(playerid, target)
{
	return (target != playerid && IsPlayerConnected(target) && Player[target][pLogged] && gSpecTarget[target] == INVALID_PLAYER_ID);
}

stock bool:Admin_StartSpec(playerid, target)
{
	if (gSpecTarget[playerid] == INVALID_PLAYER_ID)
	{
		// запоминаем, куда вернуть
		GetPlayerPos(playerid, gSpecPos[playerid][0], gSpecPos[playerid][1], gSpecPos[playerid][2]);
		gSpecInt[playerid] = GetPlayerInterior(playerid);
		gSpecWorld[playerid] = GetPlayerVirtualWorld(playerid);
		TogglePlayerSpectating(playerid, true);
	}
	gSpecTarget[playerid] = target;
	Admin_SpecApply(playerid);
	Admin_SpecHud(playerid);
	Admin_Log(playerid, "spec", target);
	SendClientMessage(playerid, COLOR_ADMIN, "Слежка за %s%s. Листать: /specnext /specprev (или ЛКМ / ПКМ). Выключить: /specoff",
		GetRPName(target), Admin_PlatformTag(target));
	return true;
}

stock bool:Cmd_Spec(playerid, const params[])
{
	if (!Admin_Check(playerid, 1)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID)
	{
		if (gSpecTarget[playerid] != INVALID_PLAYER_ID) { Admin_StopSpec(playerid); return true; }
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /spec [id]   выключить: /specoff   (или /amenu > игрок > Следить)");
	}
	if (target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Нельзя следить за собой.");
	if (gSpecTarget[target] != INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Этот игрок сам сейчас в слежке.");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	return Admin_StartSpec(playerid, target);
}

// Следующий (dir = 1) или предыдущий (dir = -1) игрок
stock bool:Admin_SpecCycle(playerid, dir)
{
	if (gSpecTarget[playerid] == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Сначала начни слежку: /spec [id]");
	new id = gSpecTarget[playerid];
	for (new n = 0; n < MAX_PLAYERS; n++)
	{
		id += dir;
		if (id >= MAX_PLAYERS) id = 0;
		else if (id < 0) id = MAX_PLAYERS - 1;
		if (id == gSpecTarget[playerid]) break;
		if (Admin_CanSpecTarget(playerid, id)) return Admin_StartSpec(playerid, id);
	}
	return SendClientMessage(playerid, COLOR_GREY, "Других игроков для слежки нет.");
}

// Из OnPlayerKeyStateChange: в слежке ЛКМ — следующий, ПКМ — предыдущий
stock Admin_OnKey(playerid, newkeys, oldkeys)
{
	if (gSpecTarget[playerid] == INVALID_PLAYER_ID) return 0;
	if ((newkeys & _:KEY_FIRE) && !(oldkeys & _:KEY_FIRE)) { Admin_SpecCycle(playerid, 1); return 1; }
	if ((newkeys & _:KEY_HANDBRAKE) && !(oldkeys & _:KEY_HANDBRAKE)) { Admin_SpecCycle(playerid, -1); return 1; }
	return 0;
}

stock bool:Cmd_SpecOff(playerid)
{
	if (!Admin_Check(playerid, 1)) return true;
	if (gSpecTarget[playerid] == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Ты ни за кем не следишь.");
	Admin_StopSpec(playerid);
	return true;
}

stock bool:Cmd_Freeze(playerid, const params[], bool:freeze)
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID)
		return SendClientMessage(playerid, COLOR_GREY, freeze ? ("Использование: /freeze [id]") : ("Использование: /unfreeze [id]"));
	if (freeze && !Admin_CanPunish(playerid, target)) return true;

	gFrozen[target] = freeze;
	TogglePlayerControllable(target, !freeze);
	Admin_Log(playerid, freeze ? ("freeze") : ("unfreeze"), target);
	SendClientMessage(target, COLOR_ADMIN, freeze ? ("Администратор %s заморозил тебя.") : ("Администратор %s разморозил тебя."), GetRPName(playerid));
	SendClientMessage(playerid, COLOR_ADMIN, freeze ? ("%s заморожен.") : ("%s разморожен."), GetRPName(target));
	return true;
}

stock bool:Cmd_Slap(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /slap [id]");

	new Float:x, Float:y, Float:z, Float:hp;
	GetPlayerPos(target, x, y, z);
	SetPlayerPos(target, x, y, z + 5.0);
	GetPlayerHealth(target, hp);
	if (hp > 10.0) SetPlayerHealth(target, hp - 5.0);
	PlayerPlaySound(target, 1130, 0.0, 0.0, 0.0);
	Admin_Log(playerid, "slap", target);
	SendClientMessage(playerid, COLOR_ADMIN, "Ты подкинул %s.", GetRPName(target));
	return true;
}

stock bool:Cmd_Goto(playerid, const params[])
{
	if (!Admin_Check(playerid, 1)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /goto [id]");

	Teleport_SaveBack(playerid);        // admin_tools.inc: /back вернёт назад
	new Float:x, Float:y, Float:z;
	GetPlayerPos(target, x, y, z);
	SetPlayerInterior(playerid, GetPlayerInterior(target));
	SetPlayerVirtualWorld(playerid, GetPlayerVirtualWorld(target));
	SetPlayerPos(playerid, x + 1.0, y, z);
	Player[playerid][pHouseInside] = Player[target][pHouseInside];
	return true;
}

stock bool:Cmd_GetHere(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /gethere [id]");
	if (gAJail[target] > 0) return SendClientMessage(playerid, COLOR_GREY, "Игрок в деморгане. Сначала /unajail.");

	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	SetPlayerInterior(target, GetPlayerInterior(playerid));
	SetPlayerVirtualWorld(target, GetPlayerVirtualWorld(playerid));
	SetPlayerPos(target, x + 1.0, y, z);
	Player[target][pHouseInside] = Player[playerid][pHouseInside];
	Admin_Log(playerid, "gethere", target);
	SendClientMessage(target, COLOR_ADMIN, "Тебя телепортировал администратор %s.", GetRPName(playerid));
	return true;
}

// /spawn [id] — вернуть игрока на его место появления (застрял, упал под карту и т.п.)
stock bool:Cmd_ASpawn(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /spawn [id] — заспавнить игрока (на его месте появления)");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	if (gAJail[target] > 0) return SendClientMessage(playerid, COLOR_GREY, "Игрок в деморгане. Сначала /unajail.");
	new PLAYER_STATE:st = GetPlayerState(target);
	if (st == PLAYER_STATE_DRIVER || st == PLAYER_STATE_PASSENGER)
	{
		new Float:x, Float:y, Float:z;
		GetPlayerPos(target, x, y, z);
		SetPlayerPos(target, x, y, z + 1.5);    // из машины: иначе спавн не сработает
	}
	SpawnPlayer(target);
	Admin_Log(playerid, "spawn", target);
	SendClientMessage(target, COLOR_ADMIN, "Администратор %s заспавнил тебя.", GetRPName(playerid));
	SendClientMessage(playerid, COLOR_ADMIN, "%s [%d] заспавнен.", GetRPName(target), target);
	return true;
}

// =====================================================================
//  Админ-чат, список админов, назначение
// =====================================================================
stock bool:Cmd_AdminChat(playerid, const params[])
{
	if (!Admin_Check(playerid, 1)) return true;
	new text[100], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /a [текст]");
	format(msg, sizeof msg, "[A] %s %s[%d]%s: %s", Admin_LevelName(Player[playerid][pAdmin]), GetRPName(playerid), playerid, Admin_PlatformTag(playerid), text);
	SendAdminMessage(COLOR_ADMIN, msg);
	return true;
}

stock bool:Cmd_Admins(playerid)
{
	SendClientMessage(playerid, COLOR_ADMIN, "Администрация онлайн:");
	new count = 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pAdmin] > 0)
		{
			SendClientMessage(playerid, COLOR_WHITE, " %s [%d] — %s%s%s", GetRPName(i), i, Admin_LevelName(Player[i][pAdmin]),
				Admin_PlatformTag(i), Admin_IsOnDuty(i) ? (" {FF66FF}(на дежурстве){FFFFFF}") : (""));
			count++;
		}
	}
	if (count == 0) SendClientMessage(playerid, COLOR_GREY, " Сейчас админов нет. Вопрос можно оставить через /report.");
	return true;
}

stock bool:Cmd_MakeAdmin(playerid, const params[])
{
	// Нужен вход RCON-ом или высший уровень админки
	if (!IsPlayerAdmin(playerid) && Player[playerid][pAdmin] < ADMIN_MAX_LEVEL)
		return SendClientMessage(playerid, COLOR_GREY, "Нужен RCON-вход (/rcon login) или уровень %d админки.", ADMIN_MAX_LEVEL);

	new pos = 0, level;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, level) || level < 0 || level > ADMIN_MAX_LEVEL)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /makeadmin [id] [уровень 0-%d]", ADMIN_MAX_LEVEL);
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	if (!IsPlayerAdmin(playerid) && target != playerid && Admin_RealLevel(target) >= Player[playerid][pAdmin])
		return SendClientMessage(playerid, COLOR_GREY, "Менять уровень админа своего уровня можно только через RCON.");

	new old = Admin_RealLevel(target);
	AdminAuth_SetLevel(target, level);      // учитывает, вошёл ли он в админку (admin_auth.inc)
	Account_Save(target);
	Admin_Log(playerid, "makeadmin", target, "", level);

	if (level > 0)
	{
		SendClientMessage(target, COLOR_ADMIN, "%s назначил тебя: %s (уровень %d). Команды: /ahelp", GetRPName(playerid), Admin_LevelName(level), level);
		if (!AdminAuth_HasPassword(target))
			SendClientMessage(target, COLOR_ADMIN, "Придумай админ-пароль: /apass. Без него при следующем входе админка не включится, пока не введёшь /alogin.");
		new msg[144];
		format(msg, sizeof msg, "[A] %s назначил %s: %s (было: %s).", GetRPName(playerid), GetRPName(target), Admin_LevelName(level), Admin_LevelName(old));
		SendAdminMessage(COLOR_ADMIN, msg);
	}
	else
	{
		SendClientMessage(target, COLOR_ADMIN, "%s снял тебя с администрации.", GetRPName(playerid));
		SendClientMessage(playerid, COLOR_ADMIN, "%s снят с администрации.", GetRPName(target));
	}
	return true;
}

// =====================================================================
//  Лог действий: /alog [ник]
// =====================================================================
stock bool:Cmd_ALog(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new pos = 0, name[MAX_PLAYER_NAME];
	new DBResult:r;
	if (NextParam(params, pos, name))
		r = DB_ExecuteQuery(gDB, "SELECT date, admin, action, target, value, reason FROM admin_log WHERE target = '%q' COLLATE NOCASE OR admin = '%q' COLLATE NOCASE ORDER BY id DESC LIMIT 20", name, name);
	else
		r = DB_ExecuteQuery(gDB, "SELECT date, admin, action, target, value, reason FROM admin_log ORDER BY id DESC LIMIT 20");
	if (r == DBResult:0) return SendClientMessage(playerid, COLOR_RED, "Ошибка чтения лога.");

	new text[2600], line[180], admin[MAX_PLAYER_NAME], action[24], target[MAX_PLAYER_NAME], reason[64];
	new rows = DB_GetRowCount(r), now = gettime();
	for (new i = 0; i < rows; i++)
	{
		DB_GetFieldString(r, 1, admin, sizeof admin);
		DB_GetFieldString(r, 2, action, sizeof action);
		DB_GetFieldString(r, 3, target, sizeof target);
		DB_GetFieldString(r, 5, reason, sizeof reason);
		format(line, sizeof line, "{AFAFAF}%s назад  {FF66FF}%s {FFFFFF}%s %s", Admin_FormatTime(now - DB_GetFieldInt(r, 0)), admin, action, target);
		strcat(text, line);
		new value = DB_GetFieldInt(r, 4);
		if (value != 0) { format(line, sizeof line, " (%d)", value); strcat(text, line); }
		if (reason[0] != '\0') { format(line, sizeof line, " {AFAFAF}— %s", reason); strcat(text, line); }
		strcat(text, "\n");
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	if (rows == 0) strcat(text, "Записей нет.");
	Admin_ShowInfo(playerid, "Лог администрации", text);
	return true;
}

// =====================================================================
//  Репорты
// =====================================================================
stock Reports_CountOpen()
{
	new n = 0;
	for (new r = 0; r < MAX_REPORTS; r++) if (gReport[r][rUsed]) n++;
	return n;
}

static Reports_FindByNumber(number)
{
	for (new r = 0; r < MAX_REPORTS; r++)
		if (gReport[r][rUsed] && gReport[r][rDbId] == number) return r;
	return -1;
}

// status: 1 — отвечен, 2 — закрыт без ответа
stock Reports_Close(slot, adminid, status, const answer[])
{
	new admin[MAX_PLAYER_NAME];
	if (adminid != INVALID_PLAYER_ID && adminid != gReport[slot][rFrom]) GetPlayerName(adminid, admin, sizeof admin);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE reports SET status = %d, admin = '%q', answer = '%q', answer_date = %d WHERE id = %d",
		status, admin, answer, gettime(), gReport[slot][rDbId]));
	gReport[slot][rUsed] = false;
}

static Reports_Create(playerid, const text[])
{
	if (gReportLast[playerid] != 0 && gettime() - gReportLast[playerid] < REPORT_COOLDOWN)
		return SendClientMessage(playerid, COLOR_GREY, "Репорт можно писать раз в %d сек. Подожди немного.", REPORT_COOLDOWN);

	// У игрока уже есть открытый репорт — дописываем новый отдельно, но не больше двух
	new mine = 0, slot = -1;
	for (new r = 0; r < MAX_REPORTS; r++)
	{
		if (gReport[r][rUsed]) { if (gReport[r][rFrom] == playerid) mine++; }
		else if (slot == -1) slot = r;
	}
	if (mine >= 2) return SendClientMessage(playerid, COLOR_GREY, "У тебя уже 2 открытых репорта. Дождись ответа.");
	if (slot == -1) return SendClientMessage(playerid, COLOR_GREY, "Очередь репортов переполнена. Попробуй через минуту.");

	new name[MAX_PLAYER_NAME];
	GetPlayerName(playerid, name, sizeof name);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO reports (date, author, author_id, text) VALUES (%d, '%q', %d, '%q')",
		gettime(), name, Player[playerid][pDbId], text));

	gReport[slot][rUsed] = true;
	gReport[slot][rDbId] = DB_LastInsertId();
	gReport[slot][rFrom] = playerid;
	gReport[slot][rFromDb] = Player[playerid][pDbId];
	gReport[slot][rTime] = gettime();
	gReport[slot][rTakenBy] = INVALID_PLAYER_ID;
	gReport[slot][rText][0] = '\0';
	strcat(gReport[slot][rText], text, 100);
	gReportLast[playerid] = gettime();

	SendClientMessage(playerid, COLOR_YELLOW, "Репорт №%d отправлен администрации. Ответ придёт сюда в чат.", gReport[slot][rDbId]);

	new msg[144], count = 0;
	format(msg, sizeof msg, "[Репорт №%d] %s[%d]%s: %s", gReport[slot][rDbId], GetRPName(playerid), playerid, Admin_PlatformTag(playerid), text);
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || Player[i][pAdmin] < 1) continue;
		SendClientMessage(i, COLOR_ORANGE, "%s", msg);
		SendClientMessage(i, COLOR_GREY, "Ответить: /ans %d [текст]  или  /reports", gReport[slot][rDbId]);
		PlayerPlaySound(i, 1085, 0.0, 0.0, 0.0);
		count++;
	}
	if (count == 0) SendClientMessage(playerid, COLOR_GREY, "Сейчас админов нет в сети — репорт увидят, когда кто-то зайдёт.");
	return true;
}

stock bool:Cmd_Report(playerid, const params[])
{
	new text[100];
	if (!RestParam(params, 0, text))
	{
		// Без текста — окно ввода (удобно с телефона)
		ShowPlayerDialog(playerid, DIALOG_REPORT_NEW, DIALOG_STYLE_INPUT, "Репорт администрации",
			"{FFFFFF}Опиши проблему или вопрос.\nЕсли жалуешься на игрока — укажи его id.", "Отправить", "Отмена");
		return true;
	}
	Reports_Create(playerid, text);
	return true;
}

static Reports_Answer(playerid, slot, const answer[])
{
	new author = gReport[slot][rFrom];
	if (IsPlayerConnected(author) && Player[author][pDbId] == gReport[slot][rFromDb])
	{
		SendClientMessage(author, COLOR_YELLOW, "Ответ администратора %s на репорт №%d:", GetRPName(playerid), gReport[slot][rDbId]);
		SendClientMessage(author, COLOR_WHITE, "%s", answer);
		if (IsPlayerMobile[author])
		{
			new text[300];
			format(text, sizeof text, "{FFFFFF}Твой вопрос: {AFAFAF}%s\n\n{FFFFFF}Ответ {FF66FF}%s{FFFFFF}:\n%s", gReport[slot][rText], GetRPName(playerid), answer);
			Admin_ShowInfo(author, "Ответ на репорт", text);
		}
	}
	new msg[144];
	format(msg, sizeof msg, "[A] %s ответил на репорт №%d: %s", GetRPName(playerid), gReport[slot][rDbId], answer);
	SendAdminMessage(COLOR_GREY, msg);
	Admin_Log(playerid, "report", IsPlayerConnected(author) ? author : INVALID_PLAYER_ID, answer, gReport[slot][rDbId]);
	Reports_Close(slot, playerid, 1, answer);
}

stock bool:Cmd_Ans(playerid, const params[])
{
	if (!Admin_Check(playerid, 1)) return true;
	new pos = 0, number, answer[100];
	if (!NextIntParam(params, pos, number) || !RestParam(params, pos, answer))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /ans [номер репорта] [ответ]");
	new slot = Reports_FindByNumber(number);
	if (slot == -1) return SendClientMessage(playerid, COLOR_GREY, "Репорт №%d уже закрыт или не существует.", number);
	Reports_Answer(playerid, slot, answer);
	return true;
}

stock bool:Cmd_Reports(playerid)
{
	if (!Admin_Check(playerid, 1)) return true;
	new text[4000], line[160], n = 0, now = gettime();
	for (new r = 0; r < MAX_REPORTS; r++)
	{
		if (!gReport[r][rUsed]) continue;
		new taken[40], shortText[42];
		strcat(shortText, gReport[r][rText], sizeof shortText);
		if (gReport[r][rTakenBy] != INVALID_PLAYER_ID) format(taken, sizeof taken, " {33AA33}[%s]{FFFFFF}", GetRPName(gReport[r][rTakenBy]));
		format(line, sizeof line, "№%d {AFAFAF}%s назад{FFFFFF}%s %s[%d]: %s\n", gReport[r][rDbId], Admin_FormatTime(now - gReport[r][rTime]),
			taken, GetRPName(gReport[r][rFrom]), gReport[r][rFrom], shortText);
		if (strlen(text) + strlen(line) >= sizeof (text) - 1) break;
		strcat(text, line);
		gReportList[playerid][n++] = r;
	}
	if (n == 0) return SendClientMessage(playerid, COLOR_ADMIN, "Открытых репортов нет.");
	ShowPlayerDialog(playerid, DIALOG_REPORTS, DIALOG_STYLE_LIST, "Репорты", "%s", "Открыть", "Закрыть", text);
	return true;
}

static Reports_ShowActions(playerid, slot)
{
	new title[48], text[300];
	format(title, sizeof title, "Репорт №%d", gReport[slot][rDbId]);
	format(text, sizeof text, "Ответить\nВзять себе (видно другим админам)\nТелепорт к автору\nСледить за автором\nЗакрыть без ответа\n{AFAFAF}%s", gReport[slot][rText]);
	ShowPlayerDialog(playerid, DIALOG_REPORT_ACT, DIALOG_STYLE_LIST, title, "%s", "Выбрать", "Назад", text);
}

// =====================================================================
//  Панель /amenu — всё через окна, удобно с телефона
// =====================================================================
stock bool:Cmd_AMenu(playerid)
{
	if (!Admin_Check(playerid, 1)) return true;
	new text[4000], line[96], n = 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged]) continue;
		format(line, sizeof line, "[%d] %s  ур.%d%s%s%s\n", i, GetRPName(i), Player[i][pLevel], Admin_PlatformTag(i),
			(Player[i][pAdmin] > 0) ? (" {FF66FF}[A]{FFFFFF}") : (""), (gMuteUntil[i] > 0 || gAJail[i] > 0) ? (" {FF6347}[наказан]{FFFFFF}") : (""));
		if (strlen(text) + strlen(line) >= sizeof (text) - 1) break;
		strcat(text, line);
		gAMenuIds[playerid][n++] = i;
	}
	if (n == 0) return SendClientMessage(playerid, COLOR_GREY, "Нет игроков.");
	ShowPlayerDialog(playerid, DIALOG_AMENU, DIALOG_STYLE_LIST, "Игроки онлайн", "%s", "Выбрать", "Закрыть", text);
	return true;
}

static AMenu_ShowActions(playerid)
{
	new target = gAMenuTarget[playerid], title[48];
	format(title, sizeof title, "%s [%d]", GetRPName(target), target);
	ShowPlayerDialog(playerid, DIALOG_AMENU_ACT, DIALOG_STYLE_LIST, title,
		"Телепорт к игроку\nТелепорт к себе\nСледить\nПроверить\nЗаморозить / разморозить\nПодкинуть\nКикнуть\nМут\nПредупреждение\nДеморган\nБан\nВылечить\nБроня 100\nЗабрать оружие\nВыдать оружие",
		"Выбрать", "Назад");
}

static bool:AMenu_TargetOk(playerid)
{
	new t = gAMenuTarget[playerid];
	if (t == INVALID_PLAYER_ID || !IsPlayerConnected(t) || Player[t][pDbId] != gAMenuTargetDb[playerid])
	{
		SendClientMessage(playerid, COLOR_GREY, "Игрок уже вышел.");
		return false;
	}
	return true;
}

stock Admin_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (AdminTools_OnDialog(playerid, dialogid, response, listitem)) return 1;
	if (AdminAuth_OnDialog(playerid, dialogid, response, inputtext)) return 1;
	if (Admin_VehDialog(playerid, dialogid, response, listitem)) return 1;
	if (dialogid < DIALOG_ADMIN_INFO || dialogid > DIALOG_REPORT_NEW) return 0;

	switch (dialogid)
	{
		case DIALOG_ADMIN_INFO: return 1;

		case DIALOG_REPORT_NEW:
		{
			if (response && inputtext[0] != '\0')
			{
				new text[100];
				strcat(text, inputtext);
				Reports_Create(playerid, text);
			}
			return 1;
		}

		case DIALOG_REPORTS:
		{
			if (!response || Player[playerid][pAdmin] < 1) return 1;
			if (listitem < 0 || listitem >= MAX_REPORTS) return 1;
			new slot = gReportList[playerid][listitem];
			if (!gReport[slot][rUsed]) { SendClientMessage(playerid, COLOR_GREY, "Этот репорт уже закрыт."); Cmd_Reports(playerid); return 1; }
			gReportSel[playerid] = slot;
			Reports_ShowActions(playerid, slot);
			return 1;
		}

		case DIALOG_REPORT_ACT:
		{
			if (Player[playerid][pAdmin] < 1) return 1;
			if (!response) { Cmd_Reports(playerid); return 1; }
			new slot = gReportSel[playerid];
			if (!gReport[slot][rUsed]) { SendClientMessage(playerid, COLOR_GREY, "Этот репорт уже закрыт."); return 1; }
			new author = gReport[slot][rFrom], params[8];
			valstr(params, author);
			switch (listitem)
			{
				case 0:
				{
					new text[200];
					format(text, sizeof text, "{FFFFFF}Репорт от %s:\n{AFAFAF}%s\n\n{FFFFFF}Твой ответ:", GetRPName(author), gReport[slot][rText]);
					ShowPlayerDialog(playerid, DIALOG_REPORT_ANSWER, DIALOG_STYLE_INPUT, "Ответ на репорт", "%s", "Ответить", "Назад", text);
				}
				case 1:
				{
					gReport[slot][rTakenBy] = playerid;
					new msg[144];
					format(msg, sizeof msg, "[A] %s взял репорт №%d.", GetRPName(playerid), gReport[slot][rDbId]);
					SendAdminMessage(COLOR_GREY, msg);
					SendClientMessage(author, COLOR_YELLOW, "Твой репорт №%d взял администратор %s.", gReport[slot][rDbId], GetRPName(playerid));
					Reports_ShowActions(playerid, slot);
				}
				case 2: Cmd_Goto(playerid, params);
				case 3: Cmd_Spec(playerid, params);
				case 4:
				{
					Reports_Close(slot, playerid, 2, "");
					SendClientMessage(playerid, COLOR_ADMIN, "Репорт закрыт.");
					if (IsPlayerConnected(author)) SendClientMessage(author, COLOR_GREY, "Твой репорт закрыт администрацией.");
				}
				default: Reports_ShowActions(playerid, slot);
			}
			return 1;
		}

		case DIALOG_REPORT_ANSWER:
		{
			if (Player[playerid][pAdmin] < 1) return 1;
			new slot = gReportSel[playerid];
			if (!gReport[slot][rUsed]) { SendClientMessage(playerid, COLOR_GREY, "Этот репорт уже закрыт."); return 1; }
			if (!response) { Reports_ShowActions(playerid, slot); return 1; }
			if (inputtext[0] == '\0') { Reports_ShowActions(playerid, slot); return 1; }
			new answer[100];
			strcat(answer, inputtext);
			Reports_Answer(playerid, slot, answer);
			return 1;
		}

		case DIALOG_AMENU:
		{
			if (!response || Player[playerid][pAdmin] < 1) return 1;
			if (listitem < 0 || listitem >= MAX_PLAYERS) return 1;
			new t = gAMenuIds[playerid][listitem];
			if (!IsPlayerConnected(t)) { SendClientMessage(playerid, COLOR_GREY, "Игрок уже вышел."); Cmd_AMenu(playerid); return 1; }
			gAMenuTarget[playerid] = t;
			gAMenuTargetDb[playerid] = Player[t][pDbId];
			AMenu_ShowActions(playerid);
			return 1;
		}

		case DIALOG_AMENU_ACT:
		{
			if (Player[playerid][pAdmin] < 1) return 1;
			if (!response) { Cmd_AMenu(playerid); return 1; }
			if (!AMenu_TargetOk(playerid)) return 1;
			new t = gAMenuTarget[playerid], params[8];
			valstr(params, t);
			gAMenuAction[playerid] = listitem;
			switch (listitem)
			{
				case AMENU_GOTO:    Cmd_Goto(playerid, params);
				case AMENU_GETHERE: Cmd_GetHere(playerid, params);
				case AMENU_SPEC:    Cmd_Spec(playerid, params);
				case AMENU_CHECK:   Cmd_Check(playerid, params);
				case AMENU_FREEZE:  Cmd_Freeze(playerid, params, !gFrozen[t]);
				case AMENU_SLAP:    Cmd_Slap(playerid, params);
				case AMENU_HEAL:    Cmd_AHeal(playerid, params);
				case AMENU_DISARM:  Cmd_Disarm(playerid, params);
				case AMENU_ARMOUR:
				{
					new p[16];
					format(p, sizeof p, "%d 100", t);
					Cmd_SetArmour(playerid, p);
				}
				default:
				{
					new hint[160];
					switch (listitem)
					{
						case AMENU_KICK:  strcat(hint, "Причина кика:");
						case AMENU_MUTE:  format(hint, sizeof hint, "Минуты (1-%d) и причина.\nНапример: 30 оскорбления", ADMIN_MAX_MUTE);
						case AMENU_WARN:  strcat(hint, "Причина предупреждения:");
						case AMENU_AJAIL: format(hint, sizeof hint, "Минуты (1-%d) и причина.\nНапример: 60 DM", ADMIN_MAX_AJAIL);
						case AMENU_BAN:   strcat(hint, "Дни (0 — навсегда) и причина.\nНапример: 7 читы");
						case AMENU_GIVEGUN: strcat(hint, "Оружие и патроны.\nНапример: deagle 200, m4 500, 31 300\nСписок: /givegun");
						default: return 1;
					}
					new body[220];
					format(body, sizeof body, "{FFFFFF}%s [%d]\n\n%s", GetRPName(t), t, hint);
					ShowPlayerDialog(playerid, DIALOG_AMENU_INPUT, DIALOG_STYLE_INPUT, (listitem == AMENU_GIVEGUN) ? ("Выдать оружие") : ("Наказание"), "%s", "Готово", "Назад", body);
				}
			}
			return 1;
		}

		case DIALOG_AMENU_INPUT:
		{
			if (Player[playerid][pAdmin] < 1) return 1;
			if (!AMenu_TargetOk(playerid)) return 1;
			if (!response || inputtext[0] == '\0') { AMenu_ShowActions(playerid); return 1; }
			new params[128];
			format(params, sizeof params, "%d %s", gAMenuTarget[playerid], inputtext);
			switch (gAMenuAction[playerid])
			{
				case AMENU_KICK:  Cmd_Kick(playerid, params);
				case AMENU_MUTE:  Cmd_Mute(playerid, params);
				case AMENU_WARN:  Cmd_Warn(playerid, params);
				case AMENU_AJAIL: Cmd_AJail(playerid, params);
				case AMENU_BAN:   Cmd_Ban(playerid, params);
				case AMENU_GIVEGUN: Cmd_GiveGun(playerid, params);
			}
			return 1;
		}
	}
	return 0;
}

// =====================================================================
//  Справка по командам
// =====================================================================
stock bool:Cmd_AHelp(playerid)
{
	if (!Admin_Check(playerid, 1)) return true;
	new text[3800];
	strcat(text, "{FF66FF}Вход\n{FFFFFF}/alogin — включить админку (админ-пароль)   /alogout — выйти из админки   /apass — сменить пароль\n/astats — твоя статистика (часы, репорты, наказания)\n\n");
	strcat(text, "{FF66FF}1 — Хелпер\n{FFFFFF}/a — админ-чат   /amenu — панель (удобно с телефона)   /reports /ans — репорты\n");
	strcat(text, "/check /spec /specoff /specnext /specprev /goto /kick /mute /unmute /admins /aduty\n");
	strcat(text, "Телепорт: метка на карте (выкл. /maptp), /tp — список точек, /back — назад\n\n");
	strcat(text, "{FF66FF}2 — Модератор\n{FFFFFF}/warn /ajail /unajail /freeze /unfreeze /slap /gethere /spawn /sethp /setskin /jail /unjail\n/sendto /gotocar /fixveh /flip   /aheal — вылечить   /setarmour — броня   /disarm — забрать оружие\n\n");
	strcat(text, "{FF66FF}3 — Администратор\n{FFFFFF}/ban [id] [дни] [причина]   /offban [ник] [дни] [причина]   /unwarn /alog /moneylog /dveh\n/veh — меню машин, /veh infernus — по названию, /veh 411 — по номеру\n/ao /gotocoord /respawncars /setweather\n/givelic — выдать лицензию   /setfuel — заправить машину\n/givegun [id] [оружие] [патроны] — выдать оружие (тяжёлое с 6 ур.)\n\n");
	strcat(text, "{FF66FF}4 — Ст. администратор\n{FFFFFF}/unban, бан навсегда (0 дней), /addtp /deltp, фракции, дома, бизнесы, АЗС, банкоматы, районы\n/givefood /giveitem — еда и вещи   /addexampoint /clearexam — маршрут экзамена\n/addgarage [1-2] /delgarage — СТО и штрафстоянки\n/pos [название] /points /dev — координаты   /mo — маппинг (объекты, удаление зданий, выгрузка)\n");
	strcat(text, "Дома: /createhouse /delhouse /sethouseint /gotohouse, шкаф: /makestorage   Входы: /adddoor /setdoor /deldoor /doors\n");
	strcat(text, "Бизнесы: /createbiz /delbiz /setbizint   /addatm /delatm /addfuel /delfuel /addgate /delgate\n");
	strcat(text, "Спавн: /addspawn /delspawn /spawns   Прокат мопедов: /addmoped /delmoped /mopeds /mopedprice\n");
	strcat(text, "Фракции: /createfaction /delfaction /flist /fcolor /makeleader /setfaction /setrankname\n/setfspawn /setfduty /setfint /delfint /fintlock /addfcar /delfcar /setfstore /setarmystore\n");
	strcat(text, "Заводы: /addfactory /setfpoint /setfowner /delfactory /factories   Чёрный рынок: /addbm /delbm /bmprice\n");
	strcat(text, "Районы: /addturf /setturf /delturf\n\n");
	strcat(text, "{FF66FF}5 — Куратор\n{FFFFFF}/astats [ник] — статистика любого админа   /alist — весь состав (часы, последний вход)   /healall — вылечить всех\n\n");
	strcat(text, "{FF66FF}6 — Зам. гл. администратора\n{FFFFFF}/banip [id или IP] [дни] [причина]   /unbanip [IP]\n\n");
	strcat(text, "{FF66FF}7 — Гл. администратор\n{FFFFFF}/resetapass [id или ник] — сбросить админ-пароль   /makeadmin /givemoney /takemoney /settax — налоги   /backup — копия базы\n\n");
	strcat(text, "{AFAFAF}Все наказания и выдачи пишутся в лог (/alog). Наказать админа своего уровня или выше нельзя.");
	Admin_ShowInfo(playerid, "Команды администрации", text);
	return true;
}

// =====================================================================
//  Прочие команды (транспорт, деньги, здоровье, скин)
// =====================================================================
// ---------------------------------------------------------------------
//  /veh — машина по номеру модели, по названию или из меню
//  /veh 411   /veh infernus   /veh (без параметров) — меню по категориям
// ---------------------------------------------------------------------
#define VEH_CHOICES         24

static const gVehCatNames[] = "Спорт\nСеданы\nВнедорожники и пикапы\nМаслкары и лоурайдеры\nМото и велосипеды\nГрузовые, фургоны, автобусы\nСлужебные (полиция, скорая, армия)\nВертолёты и самолёты\nЛодки";
static const gVehCat[][VEH_CHOICES] =
{
	{ 411, 541, 451, 429, 415, 506, 494, 502, 503, 477, 480, 402, 603, 562, 559, 560, 565, 558, 587, 589 },
	{ 405, 410, 421, 426, 445, 492, 507, 516, 529, 540, 546, 547, 550, 551, 580, 585, 409, 466, 467, 549 },
	{ 400, 489, 495, 500, 505, 554, 579, 422, 478, 543, 600, 444, 556, 557, 418, 483 },
	{ 412, 534, 535, 536, 566, 567, 575, 576, 474, 475, 542, 491, 517, 518, 526, 527 },
	{ 522, 521, 461, 463, 468, 471, 581, 586, 462, 448, 509, 481, 510 },
	{ 403, 514, 515, 455, 456, 499, 414, 413, 482, 440, 459, 498, 578, 443, 431, 437, 420, 438, 524, 573 },
	{ 596, 597, 598, 599, 523, 427, 490, 528, 601, 416, 407, 544, 525, 552, 574, 408, 432, 470, 433 },
	{ 487, 497, 469, 447, 563, 425, 548, 417, 488, 519, 593, 513, 511, 553, 577, 592, 520, 476, 512 },
	{ 446, 452, 493, 454, 484, 453, 473, 595, 430, 472, 539 }
};
static gVehChoice[MAX_PLAYERS][VEH_CHOICES];

static Admin_SpawnVeh(playerid, model, c1, c2)
{
	new Float:x, Float:y, Float:z, Float:a;
	GetPlayerPos(playerid, x, y, z);
	GetPlayerFacingAngle(playerid, a);
	new v = CreateVehicle(model, x, y, z + 0.5, a, c1, c2, -1);
	if (v == INVALID_VEHICLE_ID) return SendClientMessage(playerid, COLOR_RED, "Не удалось создать машину (лимит машин на сервере?).");

	Vehicle_Clear(v);
	gVehAdmin[v] = true;
	LinkVehicleToInterior(v, GetPlayerInterior(playerid));
	SetVehicleVirtualWorld(v, GetPlayerVirtualWorld(playerid));
	PutPlayerInVehicle(playerid, v, 0);
	Admin_Log(playerid, "veh", INVALID_PLAYER_ID, "", model);

	new name[32];
	Hud_GetVehicleName(model, name);
	SendClientMessage(playerid, COLOR_ADMIN, "%s (модель %d) создана, id %d. Удалить: /dveh   убрать все пустые: /respawncars", name, model, v);
	return 1;
}

// Окно со списком моделей из gVehChoice[playerid]
static Admin_ShowVehList(playerid, const title[], count)
{
	new text[VEH_CHOICES * 32], line[40], name[32];
	for (new i = 0; i < count; i++)
	{
		name[0] = '\0';
		Hud_GetVehicleName(gVehChoice[playerid][i], name);
		format(line, sizeof line, "%s {AFAFAF}(%d)\n", name, gVehChoice[playerid][i]);
		strcat(text, line);
	}
	ShowPlayerDialog(playerid, DIALOG_VEH_LIST, DIALOG_STYLE_LIST, title, "%s", "Создать", "Назад", text);
}

stock bool:Cmd_Veh(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new pos = 0, what[32], c1 = -1, c2 = -1;
	if (!NextParam(params, pos, what))
	{
		ShowPlayerDialog(playerid, DIALOG_VEH_CAT, DIALOG_STYLE_LIST, "Создать машину", "%s", "Дальше", "Закрыть", gVehCatNames);
		return true;
	}
	NextIntParam(params, pos, c1);
	NextIntParam(params, pos, c2);

	if (IsNumeric(what))
	{
		new model = strval(what);
		if (model < 400 || model > 611) return SendClientMessage(playerid, COLOR_GREY, "Модель от 400 до 611. Или название: /veh infernus. Или меню: /veh");
		Admin_SpawnVeh(playerid, model, c1, c2);
		return true;
	}

	// Поиск по названию: точное совпадение сразу, иначе — все, где есть эти буквы
	new name[32], count = 0;
	for (new model = 400; model <= 611; model++)
	{
		name[0] = '\0';
		Hud_GetVehicleName(model, name);
		if (name[0] == '\0') continue;
		if (!strcmp(name, what, true)) { Admin_SpawnVeh(playerid, model, c1, c2); return true; }
		if (strfind(name, what, true) != -1 && count < VEH_CHOICES) gVehChoice[playerid][count++] = model;
	}
	if (count == 0) return SendClientMessage(playerid, COLOR_GREY, "Машина \"%s\" не найдена. Открой меню: /veh", what);
	if (count == 1) { Admin_SpawnVeh(playerid, gVehChoice[playerid][0], c1, c2); return true; }
	Admin_ShowVehList(playerid, "Найдено несколько", count);
	return true;
}

stock Admin_VehDialog(playerid, dialogid, response, listitem)
{
	if (dialogid != DIALOG_VEH_CAT && dialogid != DIALOG_VEH_LIST) return 0;
	if (Player[playerid][pAdmin] < 3) return 1;
	if (dialogid == DIALOG_VEH_CAT)
	{
		if (!response || listitem < 0 || listitem >= sizeof (gVehCat)) return 1;
		new count = 0;
		for (new i = 0; i < VEH_CHOICES && gVehCat[listitem][i] != 0; i++) gVehChoice[playerid][count++] = gVehCat[listitem][i];
		Admin_ShowVehList(playerid, "Выбери машину", count);
		return 1;
	}
	if (!response)
	{
		ShowPlayerDialog(playerid, DIALOG_VEH_CAT, DIALOG_STYLE_LIST, "Создать машину", "%s", "Дальше", "Закрыть", gVehCatNames);
		return 1;
	}
	if (listitem < 0 || listitem >= VEH_CHOICES) return 1;
	new model = gVehChoice[playerid][listitem];
	if (model >= 400 && model <= 611) Admin_SpawnVeh(playerid, model, -1, -1);
	return 1;
}

stock bool:Cmd_DVeh(playerid)
{
	if (!Admin_Check(playerid, 3)) return true;
	new v = GetPlayerVehicleID(playerid);
	if (v == 0 || !gVehAdmin[v]) return SendClientMessage(playerid, COLOR_GREY, "Сядь в машину, созданную через /veh.");
	Vehicle_Clear(v);
	DestroyVehicle(v);
	SendClientMessage(playerid, COLOR_ADMIN, "Машина удалена.");
	return true;
}

stock bool:Cmd_GiveMoney(playerid, const params[])
{
	if (!Admin_Check(playerid, ADMIN_MAX_LEVEL)) return true;
	new pos = 0, amount;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, amount))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /givemoney [id] [сумма]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");

	new reason[64];
	format(reason, sizeof reason, "админ %s (/givemoney)", GetRPName(playerid));
	GiveMoney(target, amount, reason);
	Account_Save(target);
	Admin_Log(playerid, "givemoney", target, "", amount);
	SendClientMessage(target, COLOR_ADMIN, "Администратор %s выдал тебе $%d.", GetRPName(playerid), amount);
	SendClientMessage(playerid, COLOR_ADMIN, "Ты выдал %s $%d.", GetRPName(target), amount);
	return true;
}

stock bool:Cmd_SetHP(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0, hp;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, hp) || hp < 0 || hp > 100)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /sethp [id] [0-100]");
	SetPlayerHealth(target, float(hp));
	Admin_Log(playerid, "sethp", target, "", hp);
	SendClientMessage(playerid, COLOR_ADMIN, "Здоровье %s: %d.", GetRPName(target), hp);
	return true;
}

stock bool:Cmd_SetSkin(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0, skin;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, skin) || skin < 0 || skin > 311)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setskin [id] [скин 0-311]");
	Player[target][pSkin] = skin;
	SetPlayerSkin(target, skin);
	Account_Save(target);
	Account_SetNormalSpawn(target);
	Admin_Log(playerid, "setskin", target, "", skin);
	SendClientMessage(playerid, COLOR_ADMIN, "Скин %s изменён на %d.", GetRPName(target), skin);
	return true;
}

// =====================================================================
//  Разбор команд администрации и мута. Вызывается в начале Commands_Process.
//  Возвращает 1 — команда обработана здесь.
// =====================================================================
stock Admin_OnCommand(playerid, const cmd[], const params[])
{
	// Мут закрывает все чаты, кроме /report
	if (!strcmp(cmd, "/me", true) || !strcmp(cmd, "/do", true) || !strcmp(cmd, "/try", true)
		|| !strcmp(cmd, "/s", true) || !strcmp(cmd, "/b", true) || !strcmp(cmd, "/pm", true)
		|| !strcmp(cmd, "/f", true) || !strcmp(cmd, "/m", true) || !strcmp(cmd, "/sms", true))
	{
		return Admin_IsMuted(playerid) ? 1 : 0;
	}

	if (!strcmp(cmd, "/report", true))      { Cmd_Report(playerid, params); return 1; }
	if (!strcmp(cmd, "/admins", true))      { Cmd_Admins(playerid); return 1; }
	if (!strcmp(cmd, "/makeadmin", true))   { Cmd_MakeAdmin(playerid, params); return 1; }
	if (AdminAuth_OnCommand(playerid, cmd, params)) return 1;   // /alogin /alogout /apass /astats /alist

	// Остальное — только админам; игроку — "Неизвестная команда"
	if (Player[playerid][pAdmin] < 1) return 0;

	if (!strcmp(cmd, "/a", true))           { Cmd_AdminChat(playerid, params); return 1; }
	if (!strcmp(cmd, "/ahelp", true))       { Cmd_AHelp(playerid); return 1; }
	if (!strcmp(cmd, "/amenu", true))       { Cmd_AMenu(playerid); return 1; }
	if (!strcmp(cmd, "/reports", true))     { Cmd_Reports(playerid); return 1; }
	if (!strcmp(cmd, "/ans", true))         { Cmd_Ans(playerid, params); return 1; }
	if (!strcmp(cmd, "/check", true))       { Cmd_Check(playerid, params); return 1; }
	if (!strcmp(cmd, "/spec", true))        { Cmd_Spec(playerid, params); return 1; }
	if (!strcmp(cmd, "/specoff", true))     { Cmd_SpecOff(playerid); return 1; }
	if (!strcmp(cmd, "/specnext", true))    { Admin_SpecCycle(playerid, 1); return 1; }
	if (!strcmp(cmd, "/specprev", true))    { Admin_SpecCycle(playerid, -1); return 1; }
	if (!strcmp(cmd, "/goto", true))        { Cmd_Goto(playerid, params); return 1; }
	if (!strcmp(cmd, "/gethere", true))     { Cmd_GetHere(playerid, params); return 1; }
	if (!strcmp(cmd, "/spawn", true) || !strcmp(cmd, "/aspawn", true)) { Cmd_ASpawn(playerid, params); return 1; }
	if (!strcmp(cmd, "/kick", true))        { Cmd_Kick(playerid, params); return 1; }
	if (!strcmp(cmd, "/mute", true))        { Cmd_Mute(playerid, params); return 1; }
	if (!strcmp(cmd, "/unmute", true))      { Cmd_Unmute(playerid, params); return 1; }
	if (!strcmp(cmd, "/warn", true))        { Cmd_Warn(playerid, params); return 1; }
	if (!strcmp(cmd, "/unwarn", true))      { Cmd_Unwarn(playerid, params); return 1; }
	if (!strcmp(cmd, "/ajail", true))       { Cmd_AJail(playerid, params); return 1; }
	if (!strcmp(cmd, "/unajail", true))     { Cmd_UnAJail(playerid, params); return 1; }
	if (!strcmp(cmd, "/freeze", true))      { Cmd_Freeze(playerid, params, true); return 1; }
	if (!strcmp(cmd, "/unfreeze", true))    { Cmd_Freeze(playerid, params, false); return 1; }
	if (!strcmp(cmd, "/slap", true))        { Cmd_Slap(playerid, params); return 1; }
	if (!strcmp(cmd, "/ban", true))         { Cmd_Ban(playerid, params); return 1; }
	if (!strcmp(cmd, "/offban", true))      { Cmd_OffBan(playerid, params); return 1; }
	if (!strcmp(cmd, "/unban", true))       { Cmd_Unban(playerid, params); return 1; }
	if (!strcmp(cmd, "/banip", true))       { Cmd_BanIp(playerid, params); return 1; }
	if (!strcmp(cmd, "/unbanip", true))     { Cmd_UnbanIp(playerid, params); return 1; }
	if (!strcmp(cmd, "/backup", true))      { Cmd_Backup(playerid); return 1; }
	if (!strcmp(cmd, "/alog", true))        { Cmd_ALog(playerid, params); return 1; }
	if (!strcmp(cmd, "/logs", true))        { Cmd_Logs(playerid, params); return 1; }
	if (!strcmp(cmd, "/veh", true))         { Cmd_Veh(playerid, params); return 1; }
	if (!strcmp(cmd, "/dveh", true))        { Cmd_DVeh(playerid); return 1; }
	if (!strcmp(cmd, "/givemoney", true))   { Cmd_GiveMoney(playerid, params); return 1; }
	if (!strcmp(cmd, "/sethp", true))       { Cmd_SetHP(playerid, params); return 1; }
	if (!strcmp(cmd, "/setskin", true))     { Cmd_SetSkin(playerid, params); return 1; }
	if (AdminGive_OnCommand(playerid, cmd, params)) return 1;   // оружие, броня, лечение
	return AdminTools_OnCommand(playerid, cmd, params);   // телепорты и инструменты
}
