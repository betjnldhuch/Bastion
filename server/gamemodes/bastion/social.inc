// =====================================================================
//  social.inc — связь с ботами ВКонтакте и Telegram (папка bots/).
//
//  Бот работает на том же компьютере и ходит в ту же базу rp.db.
//  Мод и бот общаются через таблицы:
//    social_codes  — код привязки, который игрок получил в игре (/bot)
//    social_links  — какой аккаунт привязан к какому ВК/ТГ (пишет бот)
//    bot_outbox    — уведомления игроку: мод пишет, бот отправляет и удаляет
//    bot_actions   — просьбы бота к моду: кикнуть, сменить пароль (мод выполняет и удаляет)
//
//  Отправить игроку сообщение в ВК/ТГ из любого места мода:
//    Social_Notify(Player[playerid][pDbId], "Текст");
// =====================================================================

#define SOCIAL_CODE_MINUTES     10       // сколько минут живёт код привязки
#define SOCIAL_ACTIONS_MS       3000     // как часто мод проверяет просьбы бота

stock Social_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS social_links (id INTEGER PRIMARY KEY AUTOINCREMENT, account INTEGER NOT NULL, platform TEXT NOT NULL, user_id INTEGER NOT NULL, notify INTEGER DEFAULT 1, linked INTEGER DEFAULT 0, UNIQUE (account, platform))"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS social_links_user ON social_links (platform, user_id)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS social_codes (code TEXT PRIMARY KEY, account INTEGER NOT NULL, until INTEGER NOT NULL)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS bot_outbox (id INTEGER PRIMARY KEY AUTOINCREMENT, account INTEGER NOT NULL, text TEXT NOT NULL, created INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS bot_actions (id INTEGER PRIMARY KEY AUTOINCREMENT, account INTEGER NOT NULL, action TEXT NOT NULL, data TEXT DEFAULT '', created INTEGER DEFAULT 0)"));
	SetTimer("Social_Timer", SOCIAL_ACTIONS_MS, true);
}

// Уведомление в ВК/ТГ. Пишется, только если аккаунт привязан и уведомления включены.
stock Social_Notify(account, const text[])
{
	if (account <= 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"INSERT INTO bot_outbox (account, text, created) SELECT %d, '%q', %d WHERE EXISTS (SELECT 1 FROM social_links WHERE account = %d AND notify = 1)",
		account, text, gettime(), account));
}

// То же по нику (для офлайн-наказаний)
stock Social_NotifyName(const name[], const text[])
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"INSERT INTO bot_outbox (account, text, created) SELECT a.id, '%q', %d FROM accounts a WHERE a.name = '%q' AND EXISTS (SELECT 1 FROM social_links s WHERE s.account = a.id AND s.notify = 1)",
		text, gettime(), name));
}

// IP без последних цифр: 95.24.*.*
static Social_MaskIp(playerid, out[], size = sizeof out)
{
	new ip[16], dots = 0;
	GetPlayerIp(playerid, ip, sizeof ip);
	out[0] = '\0';
	for (new i = 0; ip[i] != '\0'; i++)
	{
		if (ip[i] == '.' && ++dots == 2) break;
		if (i < size - 1) { out[i] = ip[i]; out[i + 1] = '\0'; }
	}
	strcat(out, ".*.*", size);
}

// Успешный вход (accounts.inc)
stock Social_OnLogin(playerid)
{
	new ip[24], text[160], name[MAX_PLAYER_NAME];
	GetPlayerName(playerid, name, sizeof name);
	Social_MaskIp(playerid, ip);
	format(text, sizeof text, "Вход в аккаунт %s на сервере %s. IP: %s.\nЕсли это не ты — нажми «Это не я» или /notme.", name, SERVER_NAME, ip);
	Social_Notify(Player[playerid][pDbId], text);
}

// Вход закрыт после неверных паролей (accounts.inc)
stock Social_OnBadPassword(playerid)
{
	new ip[24], text[160], name[MAX_PLAYER_NAME];
	GetPlayerName(playerid, name, sizeof name);
	Social_MaskIp(playerid, ip);
	format(text, sizeof text, "Кто-то %d раз ввёл неверный пароль от аккаунта %s. IP: %s. Вход с этого адреса закрыт на время.", MAX_LOGIN_TRIES, name, ip);
	Social_Notify(Player[playerid][pDbId], text);
}

// /bot — код привязки и как им пользоваться
stock bool:Cmd_Bot(playerid)
{
	new account = Player[playerid][pDbId];
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM social_codes WHERE account = %d OR until < %d", account, gettime()));

	new code[8];
	for (new tries = 0; tries < 10; tries++)
	{
		format(code, sizeof code, "%06d", 100000 + random(900000));
		new busy = 0;
		new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM social_codes WHERE code = '%q'", code);
		if (r != DBResult:0)
		{
			if (DB_GetRowCount(r) > 0) busy = DB_GetFieldInt(r, 0);
			DB_FreeResultSet(r);
		}
		if (!busy) break;
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO social_codes (code, account, until) VALUES ('%q', %d, %d)",
		code, account, gettime() + SOCIAL_CODE_MINUTES * 60));

	new vk[24] = "не привязан", tg[24] = "не привязан", platform[8];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT platform FROM social_links WHERE account = %d", account);
	if (r != DBResult:0)
	{
		for (new i = 0, n = DB_GetRowCount(r); i < n; i++)
		{
			DB_GetFieldString(r, 0, platform, sizeof platform);
			if (!strcmp(platform, "vk")) vk = "{33AA33}привязан";
			else if (!strcmp(platform, "tg")) tg = "{33AA33}привязан";
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}

	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "Боты ВКонтакте и Telegram",
		"{FFFFFF}Привяжи аккаунт к боту, чтобы получать уведомления о входе,\nсмотреть статистику и восстановить пароль, если забудешь.\n\n\
ВКонтакте: %s{FFFFFF}\nTelegram: %s{FFFFFF}\n\n\
Твой код: {FFFF00}%s{FFFFFF} (действует %d мин.)\n\
Напиши боту: {33CCFF}/link %s",
		"Закрыть", "", vk, tg, code, SOCIAL_CODE_MINUTES, code);
	return true;
}

// Найти игрока на сервере по id аккаунта (и тех, кто ещё вводит пароль)
static Social_FindOnline(account)
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pDbId] == account) return i;
	}
	return INVALID_PLAYER_ID;
}

// Просьбы бота: kick — выкинуть с сервера, setpass — новый пароль (data = пароль)
forward Social_Timer();
public Social_Timer()
{
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, account, action, data FROM bot_actions ORDER BY id LIMIT 20");
	if (r == DBResult:0) return;
	new n = DB_GetRowCount(r);
	if (n == 0) { DB_FreeResultSet(r); return; }

	new ids[20], accounts[20], actions[20][12], datas[20][MAX_PASSWORD_LEN + 1];
	if (n > 20) n = 20;
	for (new i = 0; i < n; i++)
	{
		ids[i] = DB_GetFieldInt(r, 0);
		accounts[i] = DB_GetFieldInt(r, 1);
		DB_GetFieldString(r, 2, actions[i], sizeof actions[]);
		DB_GetFieldString(r, 3, datas[i], sizeof datas[]);
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);

	for (new i = 0; i < n; i++)
	{
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM bot_actions WHERE id = %d", ids[i]));
		new target = Social_FindOnline(accounts[i]);

		if (!strcmp(actions[i], "setpass"))
		{
			new len = strlen(datas[i]);
			if (len < MIN_PASSWORD_LEN || len > MAX_PASSWORD_LEN) continue;
			new salt[17], hash[65];
			RandomString(salt, 16);
			SHA256_Hash(datas[i], salt, hash, sizeof hash);
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET password = '%q', salt = '%q' WHERE id = %d", hash, salt, accounts[i]));
			Social_Notify(accounts[i], "Пароль от аккаунта изменён через бота.");
			if (target != INVALID_PLAYER_ID)
			{
				SendClientMessage(target, COLOR_RED, "Пароль от аккаунта изменён через бота ВК/Telegram. Зайди заново с новым паролем.");
				KickWithDelay(target);
			}
		}
		else if (!strcmp(actions[i], "kick"))
		{
			if (target == INVALID_PLAYER_ID) continue;
			SendClientMessage(target, COLOR_RED, "Владелец аккаунта выкинул тебя с сервера через бота ВК/Telegram.");
			KickWithDelay(target);
			Social_Notify(accounts[i], "Готово: аккаунт выкинут с сервера. Смени пароль: /restore.");
		}
	}
}
