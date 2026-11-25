// =====================================================================
//  accounts.inc — регистрация, вход, сохранение аккаунта.
//
//  Как это работает:
//  1. Игрок заходит -> ищем его ник в таблице accounts.
//  2. Нашли -> окно "Вход", не нашли -> окно "Регистрация".
//  3. Пароль не хранится открыто: хранится SHA-256(пароль + соль).
//  4. После входа загружаем деньги/уровень/скин и спавним игрока.
// =====================================================================

// Вызывается из OnPlayerConnect
stock Account_OnConnect(playerid)
{
	Player_Reset(playerid);

	new name[MAX_PLAYER_NAME];
	GetPlayerName(playerid, name, sizeof name);

#if REQUIRE_RP_NAME
	if (!IsRPName(name))
	{
		SendClientMessage(playerid, COLOR_RED, "Ник должен быть в формате Имя_Фамилия (например Ivan_Petrov).");
		SendClientMessage(playerid, COLOR_RED, "Смени ник в клиенте SA-MP и зайди снова.");
		KickWithDelay(playerid);
		return 0;
	}
#endif

	if (Bans_IsBanned(playerid, name)) return 0;   // admin.inc
	if (Bans_IsIpBanned(playerid)) return 0;        // бан по IP (/banip)

	new lock = Login_LockLeft(playerid);            // пауза после неверных паролей (admin.inc)
	if (lock > 0)
	{
		SendClientMessage(playerid, COLOR_RED, "С твоего адреса слишком много раз ввели неверный пароль. Вход откроется через %d мин.", (lock + 59) / 60);
		KickWithDelay(playerid);
		return 0;
	}

	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, password, salt FROM accounts WHERE name = '%q' LIMIT 1", name);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			Player[playerid][pDbId] = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 1, Player[playerid][pPassHash], 65);
			DB_GetFieldString(r, 2, Player[playerid][pSalt], 17);
		}
		DB_FreeResultSet(r);
	}
	return 1;
}

// Показать окно входа или регистрации. error — текст ошибки в HTML-окне (ui.inc).
stock Account_ShowAuth(playerid, const error[] = "")
{
	// Новичок сначала принимает правила (referral.inc)
	if (Player[playerid][pDbId] == 0 && !Player[playerid][pRulesOk])
	{
		Rules_Show(playerid);
		return;
	}
	if (UI_Has(playerid))
	{
		new json[256], err[128];
		UI_JsonEscape(err, error);
		if (Player[playerid][pDbId] != 0)
		{
			format(json, sizeof json, "{\"name\":\"%s\",\"error\":\"%s\"}", GetRPName(playerid), err);
			UI_Show(playerid, DIALOG_LOGIN, DIALOG_STYLE_PASSWORD, "login", json, "Войти", "Выход");
		}
		else
		{
			format(json, sizeof json, "{\"name\":\"%s\",\"min\":%d,\"max\":%d,\"error\":\"%s\"}",
				GetRPName(playerid), MIN_PASSWORD_LEN, MAX_PASSWORD_LEN, err);
			UI_Show(playerid, DIALOG_REGISTER, DIALOG_STYLE_PASSWORD, "register", json, "Далее", "Выход");
		}
		return;
	}
	if (Player[playerid][pDbId] != 0)
	{
		ShowPlayerDialog(playerid, DIALOG_LOGIN, DIALOG_STYLE_PASSWORD, "Вход",
			"{FFFFFF}С возвращением, {33CCFF}%s{FFFFFF}!\n\nВведи пароль от аккаунта:",
			"Войти", "Выход", GetRPName(playerid));
	}
	else
	{
		ShowPlayerDialog(playerid, DIALOG_REGISTER, DIALOG_STYLE_PASSWORD, "Регистрация",
			"{FFFFFF}Добро пожаловать на сервер, {33CCFF}%s{FFFFFF}!\n\nПридумай пароль (от %d до %d символов):",
			"Далее", "Выход", GetRPName(playerid), MIN_PASSWORD_LEN, MAX_PASSWORD_LEN);
	}
}

// Выбор пола после регистрации
stock Account_ShowGender(playerid)
{
	if (UI_Has(playerid))
		// По строке на пункт списка, чтобы клиент мог вернуть listitem 0 или 1
		UI_Show(playerid, DIALOG_GENDER, DIALOG_STYLE_LIST, "gender", "{\"items\":[\n\"Мужской\",\n\"Женский\"]}", "Выбрать");
	else
		ShowPlayerDialog(playerid, DIALOG_GENDER, DIALOG_STYLE_LIST, "Выбери пол персонажа",
			"Мужской\nЖенский", "Выбрать", "");
}

// Красивый вид города, пока игрок вводит пароль
stock Account_ShowLoginCamera(playerid)
{
	SetPlayerCameraPos(playerid, 1300.0, -1500.0, 150.0);
	SetPlayerCameraLookAt(playerid, 1550.0, -1350.0, 60.0);
}

// Обработка окон аккаунта. Возвращает 1, если окно было наше.
stock Account_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid != DIALOG_REGISTER && dialogid != DIALOG_LOGIN && dialogid != DIALOG_GENDER
		&& dialogid != DIALOG_RULES && dialogid != DIALOG_REFERRAL) return 0;
	if (Player[playerid][pLogged]) return 1;   // защита: уже вошёл — окна входа игнорируем

	switch (dialogid)
	{
		case DIALOG_REGISTER:
		{
			if (Player[playerid][pDbId] != 0) return 1;   // аккаунт уже есть
			if (!response)
			{
				SendClientMessage(playerid, COLOR_GREY, "Ты отказался от регистрации.");
				KickWithDelay(playerid);
				return 1;
			}
			new len = strlen(inputtext);
			if (len < MIN_PASSWORD_LEN || len > MAX_PASSWORD_LEN)
			{
				SendClientMessage(playerid, COLOR_RED, "Пароль должен быть от %d до %d символов.", MIN_PASSWORD_LEN, MAX_PASSWORD_LEN);
				Account_ShowAuth(playerid, "Пароль слишком короткий или слишком длинный.");
				return 1;
			}

			new name[MAX_PLAYER_NAME], salt[17], hash[65];
			GetPlayerName(playerid, name, sizeof name);
			RandomString(salt, 16);
			SHA256_Hash(inputtext, salt, hash, sizeof hash);

			DB_FreeResultSet(DB_ExecuteQuery(gDB,
				"INSERT INTO accounts (name, password, salt, money, reg_date) VALUES ('%q', '%q', '%q', %d, %d)",
				name, hash, salt, START_MONEY, gettime()));

			Player[playerid][pDbId] = DB_LastInsertId();
			Player[playerid][pMoney] = START_MONEY;
			Player[playerid][pLevel] = 1;
			Skills_OnRegister(playerid);   // новичок: навыки оружия с нуля (skills.inc)

			Referral_Show(playerid);   // кто пригласил, потом выбор пола
			return 1;
		}
		case DIALOG_RULES:
		{
			if (Player[playerid][pDbId] != 0) return 1;
			if (!response)
			{
				SendClientMessage(playerid, COLOR_GREY, "Без согласия с правилами играть на сервере нельзя.");
				KickWithDelay(playerid);
				return 1;
			}
			Player[playerid][pRulesOk] = true;
			Account_ShowAuth(playerid);
			return 1;
		}
		case DIALOG_REFERRAL:
		{
			if (Player[playerid][pDbId] == 0) return 1;
			if (response && inputtext[0] != '\0')
			{
				new error[96];
				if (!Referral_Set(playerid, inputtext, error))
				{
					Referral_Show(playerid, error);
					return 1;
				}
			}
			Account_ShowGender(playerid);
			return 1;
		}
		case DIALOG_GENDER:
		{
			if (Player[playerid][pDbId] == 0) return 1;
			Player[playerid][pSkin] = (response && listitem == 1) ? SKIN_FEMALE : SKIN_MALE;
			SendClientMessage(playerid, COLOR_GREEN, "Регистрация завершена! Тебе выдано $%d на старт.", START_MONEY);
			SendClientMessage(playerid, COLOR_WHITE, "Список команд: /help");
			Quest_OnRegister(playerid);     // квест новичка стартует сам (quest.inc)
			Docs_OnRegister(playerid);      // паспорт — только в мэрии (docs.inc)
			Account_FinishLogin(playerid);
			return 1;
		}
		case DIALOG_LOGIN:
		{
			if (Player[playerid][pDbId] == 0) return 1;
			if (!response)
			{
				KickWithDelay(playerid);
				return 1;
			}
			new hash[65];
			SHA256_Hash(inputtext, Player[playerid][pSalt], hash, sizeof hash);

			// strcmp на пустых строках возвращает 0, поэтому отдельно проверяем длину
			if (inputtext[0] == '\0' || strcmp(hash, Player[playerid][pPassHash]) != 0)
			{
				Player[playerid][pLoginTries]++;
				Log_OnBadPassword(playerid, Player[playerid][pLoginTries]);
				if (Player[playerid][pLoginTries] >= MAX_LOGIN_TRIES)
				{
					SendClientMessage(playerid, COLOR_RED, "Слишком много неверных попыток. Вход с твоего адреса закрыт на %d мин.", LOGIN_LOCK_SECONDS / 60);
					Login_Lock(playerid);
					Social_OnBadPassword(playerid);   // уведомление в ВК/ТГ (social.inc)
					KickWithDelay(playerid);
					return 1;
				}
				SendClientMessage(playerid, COLOR_RED, "Неверный пароль! Осталось попыток: %d",
					MAX_LOGIN_TRIES - Player[playerid][pLoginTries]);
				new err[64];
				format(err, sizeof err, "Неверный пароль. Осталось попыток: %d", MAX_LOGIN_TRIES - Player[playerid][pLoginTries]);
				Account_ShowAuth(playerid, err);
				return 1;
			}

			Account_Load(playerid);
			Social_OnLogin(playerid);   // уведомление о входе в ВК/ТГ (social.inc)
			SendClientMessage(playerid, COLOR_GREEN, "Ты успешно вошёл в аккаунт. Список команд: /help");
			Account_FinishLogin(playerid);
			return 1;
		}
	}
	return 0;
}

// Загрузить данные аккаунта из базы
stock Account_Load(playerid)
{
	new DBResult:r = DB_ExecuteQuery(gDB,
		"SELECT money, level, exp, skin, admin, job, faction, frank, wanted, jail, bank, phone FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			Player[playerid][pMoney] = DB_GetFieldInt(r, 0);
			Player[playerid][pLevel] = DB_GetFieldInt(r, 1);
			Player[playerid][pExp]   = DB_GetFieldInt(r, 2);
			Player[playerid][pSkin]  = DB_GetFieldInt(r, 3);
			Player[playerid][pAdmin] = DB_GetFieldInt(r, 4);
			Player[playerid][pJob]   = DB_GetFieldInt(r, 5);
			Player[playerid][pFaction] = DB_GetFieldInt(r, 6);
			Player[playerid][pRank]    = DB_GetFieldInt(r, 7);
			Player[playerid][pWanted]  = DB_GetFieldInt(r, 8);
			Player[playerid][pJail]    = DB_GetFieldInt(r, 9);
			Player[playerid][pBank]    = DB_GetFieldInt(r, 10);
			Player[playerid][pPhone]   = DB_GetFieldInt(r, 11);
			if (Player[playerid][pFaction] < 0 || Player[playerid][pFaction] >= MAX_FACTIONS) Player[playerid][pFaction] = FACTION_NONE;
		}
		DB_FreeResultSet(r);
	}
}

// Сохранить аккаунт в базу. Вызывается при выходе, на PayDay и при важных событиях.
stock Account_Save(playerid)
{
	if (!Player[playerid][pLogged] || Player[playerid][pDbId] == 0) return 0;

	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"UPDATE accounts SET money = %d, level = %d, exp = %d, skin = %d, admin = %d, job = %d, faction = %d, frank = %d, wanted = %d, jail = %d, bank = %d, phone = %d WHERE id = %d",
		Player[playerid][pMoney], Player[playerid][pLevel], Player[playerid][pExp],
		Player[playerid][pSkin], Admin_RealLevel(playerid), Player[playerid][pJob],
		Player[playerid][pFaction], Player[playerid][pRank], Player[playerid][pWanted], Player[playerid][pJail],
		Player[playerid][pBank], Player[playerid][pPhone],
		Player[playerid][pDbId]));
	return 1;
}

// Всё готово — выпускаем игрока в мир
stock Account_FinishLogin(playerid)
{
	Player[playerid][pLogged] = true;
	Log_OnLogin(playerid);

	ResetPlayerMoney(playerid);
	GivePlayerMoney(playerid, Player[playerid][pMoney]);
	SetPlayerScore(playerid, Player[playerid][pLevel]);
	Account_Save(playerid);

	Vehicles_LoadForPlayer(playerid);   // личная машина, если есть (vehicles.inc)
	Docs_LoadPlayer(playerid);          // паспорт: дата выдачи (docs.inc)
	Skills_LoadPlayer(playerid);        // навыки оружия (skills.inc)
	Admin_OnLogin(playerid);            // мут, варны, деморган (admin.inc)
	Families_OnLogin(playerid);         // семья игрока (families.inc)
	Debts_OnLogin(playerid);            // долги по налогам и штрафам (debts.inc)
	Spawns_LoadPlayer(playerid);        // выбор из /setspawn (spawns.inc)
	Squad_Load(playerid);               // взвод во фракции (factions.inc)
	FPerm_OnLogin(playerid);            // личные права во фракции (fmanage.inc)
	Markers_SyncPlayer(playerid);       // точки, которые видит только своя фракция (markers.inc)
	Quest_OnLogin(playerid);            // квест новичка (quest.inc)
	Daily_OnLogin(playerid);            // ежедневные задания (daily.inc)
	PhoneApp_OnLogin(playerid);         // непрочитанные SMS (phoneapp.inc)

	Account_SetNormalSpawn(playerid);
	TogglePlayerSpectating(playerid, false);  // выход из режима наблюдения = спавн
}

// /rec — перезайти на сервер, не закрывая игру.
// open.mp не умеет «уронить» связь одному игроку: BlockIpAddress сразу кикает, и обычный
// клиент останавливается на «Server closed the connection». Поэтому приложение Bastion
// (версия с «-bui2» и новее) и ПК-плагин bastion_ui.asi v3 (client-mods/bastion-ui) получают скрытое окно «[bastion:rec]», запоминает его и после
// кика само подключается заново (mobile/samp-mobile-patches/apply.py, раздел 15).
#define REC_CLIENT_TAG      "-bui"
#define REC_MIN_UI_VERSION  2
#define REC_MIN_PC_VERSION  3       // ПК-плагин bastion_ui.asi: /__bui 3

static bool:Rec_ClientSupports(playerid)
{
	if (gUiPcVersion[playerid] >= REC_MIN_PC_VERSION) return true;   // ПК: bastion_ui.asi v3+ (ui.inc)
	new version[32];
	GetPlayerVersion(playerid, version, sizeof version);
	new p = strfind(version, REC_CLIENT_TAG);
	if (p == -1) return false;
	return (strval(version[p + strlen(REC_CLIENT_TAG)]) >= REC_MIN_UI_VERSION);
}

stock bool:Cmd_Rec(playerid)
{
	if (!Player[playerid][pLogged]) return true;
	if (Player[playerid][pCuffed] || Wound_IsWounded(playerid))
		return SendClientMessage(playerid, COLOR_GREY, "Сейчас перезайти нельзя.");
	if (!Rec_ClientSupports(playerid))
		return SendClientMessage(playerid, COLOR_GREY, "/rec работает в приложении Bastion и на ПК с окнами Bastion. Обнови: перезайди через BastionLauncher.exe.");
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_YELLOW, "Перезаход: через пару секунд игра подключится заново сама. Ничего не нажимай.");
	ShowPlayerDialog(playerid, DIALOG_NONE, DIALOG_STYLE_MSGBOX, "[bastion:rec]", " ", "OK", "");
	KickWithDelay(playerid);
	return true;
}

// Где появляется игрок: у своего дома, если он есть, иначе на базе фракции, иначе на обычном спавне
stock Account_SetNormalSpawn(playerid)
{
	new Float:x = SPAWN_X, Float:y = SPAWN_Y, Float:z = SPAWN_Z, Float:a = SPAWN_A;
	// Выбор игрока из /setspawn (spawns.inc). Если выбранного места больше нет — обычный порядок.
	new chosen = 0;
	switch (Spawns_GetMode(playerid))
	{
		case SPAWN_STATION: chosen = Spawns_GetChosen(playerid, x, y, z, a);
		case SPAWN_HOUSE:   chosen = Houses_GetSpawn(playerid, x, y, z, a);
		case SPAWN_FACTION: chosen = Factions_GetSpawn(playerid, x, y, z, a);
		case SPAWN_ANYSTATION: chosen = Spawns_Get(x, y, z, a);
	}
	if (!chosen)
	{
		Spawns_Get(x, y, z, a);                  // spawns.inc — вокзалы, расставленные админом (случайный)
		Factions_GetSpawn(playerid, x, y, z, a); // factions.inc — база фракции
		Houses_GetSpawn(playerid, x, y, z, a);   // houses.inc — меняет координаты, если есть дом
	}
	SetSpawnInfo(playerid, NO_TEAM, Player[playerid][pSkin], x, y, z, a);
}
