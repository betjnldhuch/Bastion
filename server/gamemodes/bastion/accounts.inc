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

// Показать окно входа или регистрации
stock Account_ShowAuth(playerid)
{
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

// Красивый вид города, пока игрок вводит пароль
stock Account_ShowLoginCamera(playerid)
{
	SetPlayerCameraPos(playerid, 1300.0, -1500.0, 150.0);
	SetPlayerCameraLookAt(playerid, 1550.0, -1350.0, 60.0);
}

// Обработка окон аккаунта. Возвращает 1, если окно было наше.
stock Account_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid != DIALOG_REGISTER && dialogid != DIALOG_LOGIN && dialogid != DIALOG_GENDER) return 0;
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
				Account_ShowAuth(playerid);
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

			ShowPlayerDialog(playerid, DIALOG_GENDER, DIALOG_STYLE_LIST, "Выбери пол персонажа",
				"Мужской\nЖенский", "Выбрать", "");
			return 1;
		}
		case DIALOG_GENDER:
		{
			if (Player[playerid][pDbId] == 0) return 1;
			Player[playerid][pSkin] = (response && listitem == 1) ? SKIN_FEMALE : SKIN_MALE;
			SendClientMessage(playerid, COLOR_GREEN, "Регистрация завершена! Тебе выдано $%d на старт.", START_MONEY);
			SendClientMessage(playerid, COLOR_WHITE, "Список команд: /help");
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
				if (Player[playerid][pLoginTries] >= MAX_LOGIN_TRIES)
				{
					SendClientMessage(playerid, COLOR_RED, "Слишком много неверных попыток. Вход с твоего адреса закрыт на %d мин.", LOGIN_LOCK_SECONDS / 60);
					Login_Lock(playerid);
					KickWithDelay(playerid);
					return 1;
				}
				SendClientMessage(playerid, COLOR_RED, "Неверный пароль! Осталось попыток: %d",
					MAX_LOGIN_TRIES - Player[playerid][pLoginTries]);
				Account_ShowAuth(playerid);
				return 1;
			}

			Account_Load(playerid);
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
		Player[playerid][pSkin], Player[playerid][pAdmin], Player[playerid][pJob],
		Player[playerid][pFaction], Player[playerid][pRank], Player[playerid][pWanted], Player[playerid][pJail],
		Player[playerid][pBank], Player[playerid][pPhone],
		Player[playerid][pDbId]));
	return 1;
}

// Всё готово — выпускаем игрока в мир
stock Account_FinishLogin(playerid)
{
	Player[playerid][pLogged] = true;

	ResetPlayerMoney(playerid);
	GivePlayerMoney(playerid, Player[playerid][pMoney]);
	SetPlayerScore(playerid, Player[playerid][pLevel]);
	Account_Save(playerid);

	Vehicles_LoadForPlayer(playerid);   // личная машина, если есть (vehicles.inc)
	Admin_OnLogin(playerid);            // мут, варны, деморган (admin.inc)
	Families_OnLogin(playerid);         // семья игрока (families.inc)
	Debts_OnLogin(playerid);            // долги по налогам и штрафам (debts.inc)
	Spawns_LoadPlayer(playerid);        // выбор из /setspawn (spawns.inc)
	Squad_Load(playerid);               // взвод во фракции (factions.inc)

	Account_SetNormalSpawn(playerid);
	TogglePlayerSpectating(playerid, false);  // выход из режима наблюдения = спавн
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
