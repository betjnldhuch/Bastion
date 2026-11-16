// =====================================================================
//  database.inc Ч база данных SQLite.
//  ‘айл scriptfiles/rp.db создаЄтс€ сам при первом запуске.
//  ќткрыть его можно программой "DB Browser for SQLite".
//
//   ак писать запросы:
//    DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET money = %d WHERE id = %d", 100, 5));
//  %d Ч целое число, %f Ч дробное, %q Ч текст (экранируетс€, защита от SQL-инъекций).
//  DB_FreeResultSet об€зателен Ч иначе утечка пам€ти.
// =====================================================================

new DB:gDB;

stock DB_Init()
{
	gDB = DB_Open(DATABASE_FILE);
	if (gDB == DB:0)
	{
		printf("[ERROR] Cannot open database %s - check that the scriptfiles folder exists", DATABASE_FILE);
		return 0;
	}

	// Ѕазу читает ещЄ и форум. ≈сли она на мгновение зан€та Ч ждЄм до 3 секунд, а не тер€ем запись
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "PRAGMA busy_timeout = 3000"));
	// WAL: запись не ждЄт fsync на каждый запрос и не блокирует чтение форумом.
	// –€дом с rp.db по€в€тс€ rp.db-wal и rp.db-shm Ч это нормально, при остановке сервера они сливаютс€ в базу.
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "PRAGMA journal_mode = WAL"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "PRAGMA synchronous = NORMAL"));

	// јккаунты игроков
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS accounts (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE COLLATE NOCASE, password TEXT NOT NULL, salt TEXT NOT NULL, money INTEGER DEFAULT 0, level INTEGER DEFAULT 1, exp INTEGER DEFAULT 0, skin INTEGER DEFAULT 60, admin INTEGER DEFAULT 0, job INTEGER DEFAULT 0, reg_date INTEGER DEFAULT 0)"));

	// ƒома
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS houses (id INTEGER PRIMARY KEY AUTOINCREMENT, owner INTEGER DEFAULT 0, price INTEGER NOT NULL, x REAL, y REAL, z REAL, ix REAL, iy REAL, iz REAL, interior INTEGER, locked INTEGER DEFAULT 0)"));

	// Ћичные машины (одна на игрока)
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS vehicles (id INTEGER PRIMARY KEY AUTOINCREMENT, owner INTEGER NOT NULL UNIQUE, model INTEGER, x REAL, y REAL, z REAL, a REAL, c1 INTEGER, c2 INTEGER, locked INTEGER DEFAULT 0)"));

	// Ќовые пол€ аккаунта дл€ фракций (стара€ база дополн€етс€ сама)
	DB_AddColumn("accounts", "faction", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "frank", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "wanted", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "jail", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "bank", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "phone", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "referrer", "INTEGER DEFAULT 0");   // кто пригласил (id аккаунта), referral.inc
	DB_AddColumn("accounts", "ref_paid", "INTEGER DEFAULT 0");   // награда пригласившему уже выдана

	// Ѕаны, районы банд, банкоматы, бизнесы
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS bans (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE COLLATE NOCASE, reason TEXT, admin TEXT, date INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS turfs (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, minx REAL, miny REAL, maxx REAL, maxy REAL, owner INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS gates (id INTEGER PRIMARY KEY AUTOINCREMENT, faction INTEGER DEFAULT 0, type INTEGER, x REAL, y REAL, z REAL, a REAL)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS atms (id INTEGER PRIMARY KEY AUTOINCREMENT, x REAL, y REAL, z REAL)"));
	DB_AddColumn("atms", "a", "REAL DEFAULT -1.0");   // угол аппарата; -1 Ч только значок
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS bank_log (id INTEGER PRIMARY KEY AUTOINCREMENT, account INTEGER NOT NULL, type INTEGER, amount INTEGER, other INTEGER DEFAULT 0, date INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS bank_log_account ON bank_log (account, id)"));
	// ƒополнительные счета игроков (bank.inc): владелец Ч accounts.id
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS bank_accounts (id INTEGER PRIMARY KEY AUTOINCREMENT, owner INTEGER NOT NULL, balance INTEGER DEFAULT 0, created INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS bank_accounts_owner ON bank_accounts (owner)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS businesses (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, type INTEGER, owner INTEGER DEFAULT 0, price INTEGER, x REAL, y REAL, z REAL, till INTEGER DEFAULT 0)"));

	// Ѕазы фракций (спавн и раздевалка) и машины фракций
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS factions (id INTEGER PRIMARY KEY, sx REAL, sy REAL, sz REAL, sa REAL, dx REAL, dy REAL, dz REAL)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS fvehicles (id INTEGER PRIMARY KEY AUTOINCREMENT, faction INTEGER NOT NULL, model INTEGER, x REAL, y REAL, z REAL, a REAL, c1 INTEGER, c2 INTEGER)"));

	print("[DB] Database opened.");
	return 1;
}

// ƒобавить колонку в таблицу, если еЄ ещЄ нет
stock DB_AddColumn(const table[], const column[], const definition[])
{
	new exists = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM pragma_table_info('%q') WHERE name = '%q'", table, column);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) exists = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (exists == 0)
	{
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "ALTER TABLE %s ADD COLUMN %s %s", table, column, definition));
		printf("[DB] Added column %s.%s", table, column);
	}
}

stock DB_Shutdown()
{
	if (gDB != DB:0)
	{
		DB_Close(gDB);
		gDB = DB:0;
	}
}

// ¬ерси€ базы (PRAGMA user_version): по ней разовые переделки данных выполн€ютс€ один раз.
// 1 Ч админка 7 уровней (admin.inc), 2 Ч 10 рангов во фракци€х (factions.inc)
stock DB_GetVersion()
{
	new version = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "PRAGMA user_version");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) version = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return version;
}

// Ќомер последней вставленной строки (после INSERT)
stock DB_LastInsertId()
{
	new id = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT last_insert_rowid()");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) id = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return id;
}
