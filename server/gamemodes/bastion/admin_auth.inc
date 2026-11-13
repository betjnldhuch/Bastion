// =====================================================================
//  admin_auth.inc — вход в админку по отдельному паролю и учёт работы.
//
//  Права админа включаются не сразу при входе в аккаунт, а после /alogin
//  и ввода админ-пароля. Если аккаунт украдут, без этого пароля вор
//  админку не получит.
//
//  /alogin [пароль] — включить админку. Без пароля — окно ввода
//                     (удобно с телефона). Первый раз — придумать пароль.
//                     3 ошибки подряд — кик и сообщение старшим админам.
//  /alogout         — выключить админку до следующего /alogin.
//  /apass           — сменить свой админ-пароль.
//  /resetapass [id или ник] — сбросить пароль админу (уровень 7).
//  /astats [ник]    — статистика: часы в админке, репорты, наказания.
//                     Чужую — с уровня 5.
//  /alist           — весь состав админов из базы (уровень 5).
//
//  Пока админ не ввёл /alogin, он для мода обычный игрок: нет команд,
//  не видит /a-чат и не виден в /admins. Уровень в базе при этом не
//  меняется (сохранение — через Admin_RealLevel).
//  Вход с нового IP — сообщение админам 6+ уровня.
//
//  Поля в accounts: admin_pass, admin_salt, admin_ip, admin_last, admin_time.
// =====================================================================

#define ADMIN_REQUIRE_ALOGIN    1       // 0 — права админа сразу при входе (как раньше)
#define ALOGIN_MAX_TRIES        3       // ошибок пароля до кика
#define ADMIN_PASS_MIN          6
#define ADMIN_PASS_MAX          32
#define ADMIN_IP_ALERT_LEVEL    6       // кому сообщать о входе с нового IP и ошибках
#define ADMIN_STATS_OTHERS      5       // с какого уровня смотреть чужую статистику и /alist

static gARealLevel[MAX_PLAYERS];        // настоящий уровень, пока админка выключена
static bool:gAHidden[MAX_PLAYERS];      // админ, но /alogin ещё не ввёл
static gALoginTries[MAX_PLAYERS];
static bool:gAPassChecked[MAX_PLAYERS];  // /apass: старый пароль уже проверен

// =====================================================================
//  Запуск и события
// =====================================================================
stock AdminAuth_Init()
{
	DB_AddColumn("accounts", "admin_pass", "TEXT DEFAULT ''");
	DB_AddColumn("accounts", "admin_salt", "TEXT DEFAULT ''");
	DB_AddColumn("accounts", "admin_ip", "TEXT DEFAULT ''");
	DB_AddColumn("accounts", "admin_last", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "admin_time", "INTEGER DEFAULT 0");
}

stock AdminAuth_OnConnect(playerid)
{
	gARealLevel[playerid] = 0;
	gAHidden[playerid] = false;
	gALoginTries[playerid] = 0;
	gAPassChecked[playerid] = false;
}

// Из Admin_OnLogin, когда у аккаунта есть уровень админки
stock AdminAuth_OnLogin(playerid)
{
#if ADMIN_REQUIRE_ALOGIN
	gARealLevel[playerid] = Player[playerid][pAdmin];
	Player[playerid][pAdmin] = 0;
	gAHidden[playerid] = true;
	if (AdminAuth_HasPassword(playerid))
		SendClientMessage(playerid, COLOR_ADMIN, "Ты %s. Чтобы включить админку, введи /alogin.", Admin_LevelName(gARealLevel[playerid]));
	else
		SendClientMessage(playerid, COLOR_ADMIN, "Ты %s. Придумай админ-пароль: /alogin.", Admin_LevelName(gARealLevel[playerid]));
#else
	AdminAuth_Granted(playerid);
#endif
}

// Уровень для сохранения в базу и для сравнения "кто старше"
stock Admin_RealLevel(playerid)
{
	return gAHidden[playerid] ? gARealLevel[playerid] : Player[playerid][pAdmin];
}

// Ждёт /alogin
stock bool:Admin_IsHidden(playerid)
{
	return gAHidden[playerid];
}

// /makeadmin: новый уровень с учётом того, включена ли админка
stock AdminAuth_SetLevel(playerid, level)
{
	if (gAHidden[playerid])
	{
		gARealLevel[playerid] = level;
		if (level == 0) gAHidden[playerid] = false;
		return;
	}
	if (level == 0 && Player[playerid][pAdmin] > 0) AdminAuth_Drop(playerid);
	Player[playerid][pAdmin] = level;
}

// Раз в минуту: время в админке
stock AdminAuth_OnMinute()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || Player[i][pAdmin] < 1) continue;
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET admin_time = admin_time + 1 WHERE id = %d", Player[i][pDbId]));
	}
}

// =====================================================================
//  Пароль
// =====================================================================
stock bool:AdminAuth_HasPassword(playerid)
{
	new bool:has = false;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT admin_pass FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			new hash[65];
			DB_GetFieldString(r, 0, hash, sizeof hash);
			has = (hash[0] != '\0');
		}
		DB_FreeResultSet(r);
	}
	return has;
}

static bool:AdminAuth_CheckPassword(playerid, const pass[])
{
	if (pass[0] == '\0') return false;
	new bool:ok = false;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT admin_pass, admin_salt FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			new hash[65], salt[17], test[65];
			DB_GetFieldString(r, 0, hash, sizeof hash);
			DB_GetFieldString(r, 1, salt, sizeof salt);
			if (hash[0] != '\0')
			{
				SHA256_Hash(pass, salt, test, sizeof test);
				ok = (strcmp(test, hash) == 0);
			}
		}
		DB_FreeResultSet(r);
	}
	return ok;
}

// Подходит ли пароль; если нет — пишет почему
static bool:AdminAuth_PasswordOk(playerid, const pass[])
{
	new len = strlen(pass);
	if (len < ADMIN_PASS_MIN || len > ADMIN_PASS_MAX)
	{
		SendClientMessage(playerid, COLOR_GREY, "Админ-пароль — от %d до %d символов.", ADMIN_PASS_MIN, ADMIN_PASS_MAX);
		return false;
	}
	for (new i = 0; i < len; i++)
	{
		if (pass[i] == ' ' || pass[i] == '%' || pass[i] == '{' || pass[i] == '}')
		{
			SendClientMessage(playerid, COLOR_GREY, "В админ-пароле не должно быть пробелов, фигурных скобок и знака процента.");
			return false;
		}
	}
	// Не такой же, как пароль от аккаунта: иначе смысла во втором пароле нет
	new hash[65];
	SHA256_Hash(pass, Player[playerid][pSalt], hash, sizeof hash);
	if (strcmp(hash, Player[playerid][pPassHash]) == 0)
	{
		SendClientMessage(playerid, COLOR_GREY, "Админ-пароль должен отличаться от пароля аккаунта.");
		return false;
	}
	return true;
}

static AdminAuth_SavePassword(playerid, const pass[])
{
	new salt[17], hash[65];
	RandomString(salt, 16);
	SHA256_Hash(pass, salt, hash, sizeof hash);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET admin_pass = '%q', admin_salt = '%q' WHERE id = %d",
		hash, salt, Player[playerid][pDbId]));
}

// =====================================================================
//  Вход и выход
// =====================================================================
static AdminAuth_ShowLogin(playerid, const error[] = "")
{
	if (AdminAuth_HasPassword(playerid))
	{
		ShowPlayerDialog(playerid, DIALOG_ALOGIN, DIALOG_STYLE_PASSWORD, "Вход в админку",
			"%s{FFFFFF}%s, уровень %d.\n\nВведи админ-пароль:", "Войти", "Отмена",
			error, Admin_LevelName(gARealLevel[playerid]), gARealLevel[playerid]);
	}
	else
	{
		ShowPlayerDialog(playerid, DIALOG_ALOGIN_NEW, DIALOG_STYLE_PASSWORD, "Админ-пароль",
			"%s{FFFFFF}У тебя ещё нет админ-пароля.\n\nПридумай его: %d-%d символов, не такой, как пароль аккаунта.\nЕго будут спрашивать при каждом /alogin.",
			"Сохранить", "Отмена", error, ADMIN_PASS_MIN, ADMIN_PASS_MAX);
	}
}

// Права включены
static AdminAuth_Granted(playerid)
{
	if (gAHidden[playerid])
	{
		Player[playerid][pAdmin] = gARealLevel[playerid];
		gAHidden[playerid] = false;
	}
	gALoginTries[playerid] = 0;

	// IP прошлого входа — для предупреждения о входе с другого места
	new ip[16], oldIp[16];
	GetPlayerIp(playerid, ip, sizeof ip);
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT admin_ip FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) DB_GetFieldString(r, 0, oldIp, sizeof oldIp);
		DB_FreeResultSet(r);
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET admin_ip = '%q', admin_last = %d WHERE id = %d",
		ip, gettime(), Player[playerid][pDbId]));
	Admin_Log(playerid, "alogin", INVALID_PLAYER_ID, ip);

	Admin_Welcome(playerid);     // admin.inc: приветствие и сообщение в /a-чат

	if (oldIp[0] != '\0' && strcmp(oldIp, ip) != 0)
	{
		new msg[144];
		format(msg, sizeof msg, "[A] Внимание: %s[%d] вошёл в админку с нового IP %s (раньше %s).", GetRPName(playerid), playerid, ip, oldIp);
		AdminAuth_Alert(msg);
	}
}

// Права сняты: выключить слежку и дежурство
static AdminAuth_Drop(playerid)
{
	if (Admin_IsSpectating(playerid)) Admin_StopSpec(playerid);
	if (Admin_IsOnDuty(playerid)) Cmd_ADuty(playerid);
}

// Сообщение старшим (ADMIN_IP_ALERT_LEVEL+)
static AdminAuth_Alert(const msg[])
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pAdmin] >= ADMIN_IP_ALERT_LEVEL)
			SendClientMessage(i, COLOR_RED, "%s", msg);
	}
	printf("%s", msg);
}

static AdminAuth_TryLogin(playerid, const pass[])
{
	if (AdminAuth_CheckPassword(playerid, pass))
	{
		AdminAuth_Granted(playerid);
		return;
	}
	gALoginTries[playerid]++;
	new ip[16];
	GetPlayerIp(playerid, ip, sizeof ip);
	Admin_Log(playerid, "alogin_fail", INVALID_PLAYER_ID, ip, gALoginTries[playerid]);

	if (gALoginTries[playerid] >= ALOGIN_MAX_TRIES)
	{
		new msg[144];
		format(msg, sizeof msg, "[A] %s[%d] (IP %s) %d раза ввёл неверный админ-пароль и кикнут.", GetRPName(playerid), playerid, ip, ALOGIN_MAX_TRIES);
		AdminAuth_Alert(msg);
		Admin_ShowInfo(playerid, "Вход в админку", "{FF6347}Неверный админ-пароль.\n\n{FFFFFF}Слишком много попыток — ты отключён от сервера.\nЗабыл пароль — обратись к главному администратору.");
		KickWithDelay(playerid);
		return;
	}
	new err[96];
	format(err, sizeof err, "{FF6347}Неверный пароль. Осталось попыток: %d.\n\n", ALOGIN_MAX_TRIES - gALoginTries[playerid]);
	AdminAuth_ShowLogin(playerid, err);
}

static bool:Cmd_ALogin(playerid, const params[])
{
	if (!gAHidden[playerid])
	{
		if (Player[playerid][pAdmin] > 0) SendClientMessage(playerid, COLOR_GREY, "Админка уже включена. Выключить: /alogout");
		else SendClientMessage(playerid, COLOR_GREY, "Ты не администратор.");
		return true;
	}
	new pass[ADMIN_PASS_MAX + 1];
	if (RestParam(params, 0, pass) && AdminAuth_HasPassword(playerid))
	{
		AdminAuth_TryLogin(playerid, pass);
		return true;
	}
	AdminAuth_ShowLogin(playerid);
	return true;
}

static bool:Cmd_ALogout(playerid)
{
	if (Player[playerid][pAdmin] < 1) return SendClientMessage(playerid, COLOR_GREY, "Админка и так выключена.");
	new msg[144];
	format(msg, sizeof msg, "[A] %s %s[%d] вышел из админки.", Admin_LevelName(Player[playerid][pAdmin]), GetRPName(playerid), playerid);
	SendAdminMessage(COLOR_ADMIN, msg);
	AdminAuth_Drop(playerid);
	gARealLevel[playerid] = Player[playerid][pAdmin];
	Player[playerid][pAdmin] = 0;
	gAHidden[playerid] = true;
	Admin_Log(playerid, "alogout", INVALID_PLAYER_ID);
	SendClientMessage(playerid, COLOR_ADMIN, "Админка выключена. Включить снова: /alogin");
	return true;
}

static bool:Cmd_APass(playerid)
{
	if (Player[playerid][pAdmin] < 1)
		return SendClientMessage(playerid, COLOR_GREY, gAHidden[playerid] ? ("Сначала войди в админку: /alogin") : ("Ты не администратор."));
	gAPassChecked[playerid] = !AdminAuth_HasPassword(playerid);
	if (gAPassChecked[playerid])
		ShowPlayerDialog(playerid, DIALOG_APASS_NEW, DIALOG_STYLE_PASSWORD, "Админ-пароль",
			"{FFFFFF}Придумай админ-пароль: %d-%d символов, не такой, как пароль аккаунта.", "Сохранить", "Отмена", ADMIN_PASS_MIN, ADMIN_PASS_MAX);
	else
		ShowPlayerDialog(playerid, DIALOG_APASS_OLD, DIALOG_STYLE_PASSWORD, "Смена админ-пароля",
			"{FFFFFF}Введи текущий админ-пароль:", "Далее", "Отмена");
	return true;
}

// /resetapass [id или ник] — пароль стирается, при следующем /alogin админ придумает новый
static bool:Cmd_ResetAPass(playerid, const params[])
{
	if (!IsPlayerAdmin(playerid) && !Admin_Check(playerid, ADMIN_MAX_LEVEL)) return true;
	new pos = 0, name[MAX_PLAYER_NAME];
	if (!NextParam(params, pos, name))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /resetapass [id или ник]");

	new target = INVALID_PLAYER_ID;
	if (IsNumeric(name))
	{
		target = strval(name);
		if (target < 0 || target >= MAX_PLAYERS || !IsPlayerConnected(target) || !Player[target][pLogged])
			return SendClientMessage(playerid, COLOR_GREY, "Игрок не найден.");
		GetPlayerName(target, name, sizeof name);
	}

	new dbId = 0, level = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, admin, name FROM accounts WHERE name = '%q' COLLATE NOCASE LIMIT 1", name);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			dbId = DB_GetFieldInt(r, 0);
			level = DB_GetFieldInt(r, 1);
			DB_GetFieldString(r, 2, name, sizeof name);
		}
		DB_FreeResultSet(r);
	}
	if (target != INVALID_PLAYER_ID) level = Admin_RealLevel(target);
	if (dbId == 0) return SendClientMessage(playerid, COLOR_GREY, "Аккаунт %s не найден.", name);
	if (level < 1) return SendClientMessage(playerid, COLOR_GREY, "%s не администратор.", name);
	if (!IsPlayerAdmin(playerid) && dbId != Player[playerid][pDbId] && level >= Player[playerid][pAdmin])
		return SendClientMessage(playerid, COLOR_GREY, "Сбросить пароль админу своего уровня можно только через RCON.");

	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET admin_pass = '', admin_salt = '' WHERE id = %d", dbId));
	Admin_LogName(GetRPName(playerid), Player[playerid][pDbId], "resetapass", name, dbId, "", level);
	SendClientMessage(playerid, COLOR_ADMIN, "Админ-пароль %s сброшен. При следующем /alogin он придумает новый.", name);
	if (target != INVALID_PLAYER_ID && target != playerid)
		SendClientMessage(target, COLOR_ADMIN, "%s сбросил твой админ-пароль. Придумай новый: %s", GetRPName(playerid), gAHidden[target] ? ("/alogin") : ("/apass"));
	return true;
}

// =====================================================================
//  Статистика
// =====================================================================
static AdminAuth_Count(const query[], const name[], since)
{
	new n = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, query, name, since);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) n = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return n;
}

// "12 ч. 30 мин." из минут
static AdminAuth_Hours(minutes)
{
	new text[24];
	format(text, sizeof text, "%d ч. %02d мин.", minutes / 60, minutes % 60);
	return text;
}

// /astats [ник]
static bool:Cmd_AStats(playerid, const params[])
{
	new pos = 0, name[MAX_PLAYER_NAME];
	if (NextParam(params, pos, name))
	{
		if (IsNumeric(name))
		{
			new t = strval(name);
			if (t < 0 || t >= MAX_PLAYERS || !IsPlayerConnected(t)) return SendClientMessage(playerid, COLOR_GREY, "Игрок не найден.");
			GetPlayerName(t, name, sizeof name);
		}
		new mine[MAX_PLAYER_NAME];
		GetPlayerName(playerid, mine, sizeof mine);
		if (strcmp(name, mine, true) != 0 && !Admin_Check(playerid, ADMIN_STATS_OTHERS)) return true;
	}
	else GetPlayerName(playerid, name, sizeof name);

	new level = 0, minutes = 0, last = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT admin, admin_time, admin_last, name FROM accounts WHERE name = '%q' COLLATE NOCASE LIMIT 1", name);
	if (r == DBResult:0) return SendClientMessage(playerid, COLOR_RED, "Ошибка чтения базы.");
	new found = DB_GetRowCount(r);
	if (found > 0)
	{
		level = DB_GetFieldInt(r, 0);
		minutes = DB_GetFieldInt(r, 1);
		last = DB_GetFieldInt(r, 2);
		DB_GetFieldString(r, 3, name, sizeof name);
	}
	DB_FreeResultSet(r);
	if (found == 0) return SendClientMessage(playerid, COLOR_GREY, "Аккаунт %s не найден.", name);

	new week = gettime() - 7 * 86400;
	new text[1400], line[160];
	format(line, sizeof line, "{FF66FF}%s{FFFFFF} — %s (уровень %d)\n\n", name, Admin_LevelName(level), level);
	strcat(text, line);
	format(line, sizeof line, "В админке всего: {FFFF00}%s{FFFFFF}\n", AdminAuth_Hours(minutes));
	strcat(text, line);
	if (last > 0) format(line, sizeof line, "Последний /alogin: %s назад\n\n", Admin_FormatTime(gettime() - last));
	else format(line, sizeof line, "Последний /alogin: не было\n\n");
	strcat(text, line);

	format(line, sizeof line, "{AFAFAF}За 7 дней / всего{FFFFFF}\nОтветов на репорты: %d / %d\n",
		AdminAuth_Count("SELECT COUNT(*) FROM reports WHERE admin = '%q' COLLATE NOCASE AND status = 1 AND answer_date > %d", name, week),
		AdminAuth_Count("SELECT COUNT(*) FROM reports WHERE admin = '%q' COLLATE NOCASE AND status = 1 AND answer_date > %d", name, 0));
	strcat(text, line);

	// Наказания по видам за 7 дней
	static const acts[][] = { "kick", "mute", "warn", "ajail", "ban", "offban", "banip" };
	static const actNames[][] = { "Кики", "Муты", "Варны", "Деморган", "Баны", "Оффбаны", "Баны по IP" };
	for (new i = 0; i < sizeof acts; i++)
	{
		new n7 = 0, nAll = 0;
		r = DB_ExecuteQuery(gDB, "SELECT SUM(date > %d), COUNT(*) FROM admin_log WHERE admin = '%q' COLLATE NOCASE AND action = '%q'", week, name, acts[i]);
		if (r != DBResult:0)
		{
			if (DB_GetRowCount(r) > 0) { n7 = DB_GetFieldInt(r, 0); nAll = DB_GetFieldInt(r, 1); }
			DB_FreeResultSet(r);
		}
		if (nAll == 0) continue;
		format(line, sizeof line, "%s: %d / %d\n", actNames[i], n7, nAll);
		strcat(text, line);
	}
	new fails = AdminAuth_Count("SELECT COUNT(*) FROM admin_log WHERE admin = '%q' COLLATE NOCASE AND action = 'alogin_fail' AND date > %d", name, week);
	if (fails > 0)
	{
		format(line, sizeof line, "\n{FF6347}Неверных админ-паролей за 7 дней: %d", fails);
		strcat(text, line);
	}
	Admin_ShowInfo(playerid, "Статистика администратора", text);
	return true;
}

// /alist — все админы из базы
static bool:Cmd_AList(playerid)
{
	if (!Admin_Check(playerid, ADMIN_STATS_OTHERS)) return true;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT name, admin, admin_time, admin_last FROM accounts WHERE admin > 0 ORDER BY admin DESC, admin_time DESC LIMIT 40");
	if (r == DBResult:0) return SendClientMessage(playerid, COLOR_RED, "Ошибка чтения базы.");

	new text[3600], line[128], name[MAX_PLAYER_NAME], rows = DB_GetRowCount(r), now = gettime();
	strcat(text, "Ник\tДолжность\tЧасов\tПоследний вход\n");
	for (new i = 0; i < rows; i++)
	{
		DB_GetFieldString(r, 0, name, sizeof name);
		new last = DB_GetFieldInt(r, 3);
		new online = INVALID_PLAYER_ID;
		for (new p = 0; p < MAX_PLAYERS; p++)
		{
			if (!IsPlayerConnected(p)) continue;
			new other[MAX_PLAYER_NAME];
			GetPlayerName(p, other, sizeof other);
			if (!strcmp(other, name, true)) { online = p; break; }
		}
		new when[32];
		if (online != INVALID_PLAYER_ID) format(when, sizeof when, gAHidden[online] ? ("{AFAFAF}онлайн, без /alogin") : ("{33CC33}онлайн"));
		else if (last > 0) format(when, sizeof when, "%s назад", Admin_FormatTime(now - last));
		else format(when, sizeof when, "{AFAFAF}не входил");
		format(line, sizeof line, "%s\t%s\t%d\t%s\n", name, Admin_LevelName(DB_GetFieldInt(r, 1)), DB_GetFieldInt(r, 2) / 60, when);
		strcat(text, line);
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	ShowPlayerDialog(playerid, DIALOG_ADMIN_INFO, DIALOG_STYLE_TABLIST_HEADERS, "Состав администрации", "%s", "Закрыть", "", text);
	return true;
}

// =====================================================================
//  Команды и окна (из admin.inc)
// =====================================================================
stock AdminAuth_OnCommand(playerid, const cmd[], const params[])
{
	// Эти команды работают и без /alogin — нужны админам с выключенной админкой
	if (!strcmp(cmd, "/alogin", true))
	{
		if (!gAHidden[playerid] && Player[playerid][pAdmin] < 1) return 0;   // игроку — "Неизвестная команда"
		Cmd_ALogin(playerid, params);
		return 1;
	}
	if (!strcmp(cmd, "/resetapass", true))
	{
		if (!IsPlayerAdmin(playerid) && Player[playerid][pAdmin] < 1) return 0;
		Cmd_ResetAPass(playerid, params);
		return 1;
	}
	if (Player[playerid][pAdmin] < 1) return 0;
	if (!strcmp(cmd, "/alogout", true))  { Cmd_ALogout(playerid); return 1; }
	if (!strcmp(cmd, "/apass", true))    { Cmd_APass(playerid); return 1; }
	if (!strcmp(cmd, "/astats", true))   { Cmd_AStats(playerid, params); return 1; }
	if (!strcmp(cmd, "/alist", true))    { Cmd_AList(playerid); return 1; }
	return 0;
}

stock AdminAuth_OnDialog(playerid, dialogid, response, const inputtext[])
{
	switch (dialogid)
	{
		case DIALOG_ALOGIN:
		{
			if (response && gAHidden[playerid]) AdminAuth_TryLogin(playerid, inputtext);
			return 1;
		}
		case DIALOG_ALOGIN_NEW:
		{
			if (!response || !gAHidden[playerid]) return 1;
			if (AdminAuth_HasPassword(playerid)) { AdminAuth_ShowLogin(playerid); return 1; }
			if (!AdminAuth_PasswordOk(playerid, inputtext)) { AdminAuth_ShowLogin(playerid); return 1; }
			AdminAuth_SavePassword(playerid, inputtext);
			SendClientMessage(playerid, COLOR_ADMIN, "Админ-пароль сохранён. Запомни его: дальше вход в админку — /alogin.");
			AdminAuth_Granted(playerid);
			return 1;
		}
		case DIALOG_APASS_OLD:
		{
			if (!response || Player[playerid][pAdmin] < 1) return 1;
			if (!AdminAuth_CheckPassword(playerid, inputtext))
			{
				Admin_Log(playerid, "apass_fail", INVALID_PLAYER_ID);
				SendClientMessage(playerid, COLOR_RED, "Неверный админ-пароль.");
				return 1;
			}
			gAPassChecked[playerid] = true;
			ShowPlayerDialog(playerid, DIALOG_APASS_NEW, DIALOG_STYLE_PASSWORD, "Смена админ-пароля",
				"{FFFFFF}Новый админ-пароль: %d-%d символов, не такой, как пароль аккаунта.", "Сохранить", "Отмена", ADMIN_PASS_MIN, ADMIN_PASS_MAX);
			return 1;
		}
		case DIALOG_APASS_NEW:
		{
			if (!response || Player[playerid][pAdmin] < 1 || !gAPassChecked[playerid]) return 1;
			if (!AdminAuth_PasswordOk(playerid, inputtext))
			{
				ShowPlayerDialog(playerid, DIALOG_APASS_NEW, DIALOG_STYLE_PASSWORD, "Смена админ-пароля",
					"{FFFFFF}Новый админ-пароль: %d-%d символов, не такой, как пароль аккаунта.", "Сохранить", "Отмена", ADMIN_PASS_MIN, ADMIN_PASS_MAX);
				return 1;
			}
			gAPassChecked[playerid] = false;
			AdminAuth_SavePassword(playerid, inputtext);
			Admin_Log(playerid, "apass", INVALID_PLAYER_ID);
			SendClientMessage(playerid, COLOR_ADMIN, "Админ-пароль изменён.");
			return 1;
		}
	}
	return 0;
}
