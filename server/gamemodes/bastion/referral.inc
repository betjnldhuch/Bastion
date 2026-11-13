// =====================================================================
//  referral.inc — первые шаги новичка: правила сервера и «кто пригласил».
//
//  Регистрация: правила (Принимаю) -> пароль -> кто пригласил -> пол.
//  Кто пригласил пишется в accounts.referrer (id аккаунта). Когда новичок
//  дойдёт до REFERRAL_LEVEL, пригласивший получает REFERRAL_REWARD (config.inc).
//  Окна: HTML на клиенте Bastion (ui.inc), иначе обычные диалоги.
// =====================================================================

// Правила: заголовок и текст. Новое правило — новая пара строк.
static const RULES[][][] =
{
	{"Ролевая игра",     "Веди себя как житель города. Не выходи из роли без причины (NonRP)."},
	{"DM и DB",          "Нельзя убивать без RP-причины (DM) и давить игроков машиной (DB)."},
	{"PG и MG",          "Не делай невозможного в жизни (PG), не используй в игре то, что узнал вне её (MG)."},
	{"SK и TK",          "Нельзя убивать на спавне (SK) и своих по фракции или банде (TK)."},
	{"Читы и баги",      "Запрещены читы, сторонние программы и использование ошибок сервера."},
	{"Общение",          "Без оскорблений игроков, родных и администрации, без рекламы других серверов."},
	{"Администрация",    "Решение администратора обязательно. Не согласен — пиши жалобу на форуме."}
};

stock Rules_Show(playerid)
{
	if (UI_Has(playerid))
	{
		new json[2048], part[256];
		json = "{\"rules\":[";
		for (new i = 0; i < sizeof RULES; i++)
		{
			format(part, sizeof part, "%s[\"%s\",\"%s\"]", i ? "," : "", RULES[i][0], RULES[i][1]);
			strcat(json, part);
		}
		strcat(json, "]}");
		UI_Show(playerid, DIALOG_RULES, DIALOG_STYLE_MSGBOX, "rules", json, "Принимаю", "Выход");
		return;
	}
	new text[2048], part[256];
	text = "{FFFFFF}Перед регистрацией прочитай правила сервера:\n\n";
	for (new i = 0; i < sizeof RULES; i++)
	{
		format(part, sizeof part, "{33CCFF}%d. %s.{FFFFFF} %s\n", i + 1, RULES[i][0], RULES[i][1]);
		strcat(text, part);
	}
	strcat(text, "\nНажимая «Принимаю», ты соглашаешься с правилами.");
	ShowPlayerDialog(playerid, DIALOG_RULES, DIALOG_STYLE_MSGBOX, "Правила сервера", "%s", "Принимаю", "Выход", text);
}

stock Referral_Show(playerid, const error[] = "")
{
	if (UI_Has(playerid))
	{
		new json[192], err[128];
		UI_JsonEscape(err, error);
		format(json, sizeof json, "{\"error\":\"%s\"}", err);
		UI_Show(playerid, DIALOG_REFERRAL, DIALOG_STYLE_INPUT, "referral", json, "Далее", "Пропустить");
		return;
	}
	ShowPlayerDialog(playerid, DIALOG_REFERRAL, DIALOG_STYLE_INPUT, "Кто тебя пригласил?",
		"{FFFFFF}Если на сервер тебя позвал друг, впиши его ник (Имя_Фамилия).\n%s%s\nНикто не звал — нажми «Пропустить».",
		"Далее", "Пропустить", error[0] ? "\n{FF6347}" : "", error);
}

// Записать пригласившего. false — ошибка, текст в error.
stock bool:Referral_Set(playerid, const nick[], error[], size = sizeof error)
{
	new name[MAX_PLAYER_NAME];
	GetPlayerName(playerid, name, sizeof name);
	if (strlen(nick) >= MAX_PLAYER_NAME || !IsRPName(nick))
	{
		format(error, size, "Ник пишется как Имя_Фамилия.");
		return false;
	}
	if (!strcmp(nick, name, true))
	{
		format(error, size, "Себя указать нельзя.");
		return false;
	}
	new id = 0, real[MAX_PLAYER_NAME];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, name FROM accounts WHERE name = '%q' LIMIT 1", nick);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			id = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 1, real, sizeof real);
		}
		DB_FreeResultSet(r);
	}
	if (id == 0)
	{
		format(error, size, "Игрока %s нет на сервере. Проверь ник.", nick);
		return false;
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET referrer = %d WHERE id = %d", id, Player[playerid][pDbId]));
	SendClientMessage(playerid, COLOR_GREEN, "Записали: тебя пригласил %s.", real);

	// Сообщить пригласившему, если он в игре
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pDbId] == id)
		{
			SendClientMessage(i, COLOR_GREEN, "Игрок %s зарегистрировался по твоему приглашению.", name);
			break;
		}
	}
	return true;
}

// Вызывается при повышении уровня (rp.pwn, PayDay)
stock Referral_OnLevelUp(playerid)
{
	if (Player[playerid][pLevel] < REFERRAL_LEVEL) return;
	new referrer = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT referrer FROM accounts WHERE id = %d AND ref_paid = 0", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) referrer = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (referrer == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET ref_paid = 1 WHERE id = %d", Player[playerid][pDbId]));

	new name[MAX_PLAYER_NAME];
	GetPlayerName(playerid, name, sizeof name);
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pDbId] == referrer)
		{
			GiveMoney(i, REFERRAL_REWARD, "приглашение");
			SendClientMessage(i, COLOR_GREEN, "Приглашённый тобой %s достиг %d уровня. Награда: $%d.", name, REFERRAL_LEVEL, REFERRAL_REWARD);
			return;
		}
	}
	// Пригласивший не в игре: деньги на руки в базе, увидит при входе
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET money = money + %d WHERE id = %d", REFERRAL_REWARD, referrer));
}
