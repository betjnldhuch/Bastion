// =====================================================================
//  backup.inc — резервные копии базы.
//
//  Раз в сутки (и при запуске, если сегодняшней копии ещё нет) сервер
//  сохраняет полную копию rp.db в ту же папку scriptfiles под именем
//  rp_backup_ГГГГ-ММ-ДД.db и держит BACKUP_KEEP последних, старые удаляет.
//
//  Копии лежат на том же диске, что и база. Чтобы пережить поломку диска,
//  запускай по расписанию backup_copy.bat (лежит в server/tools): он
//  складывает копии на другой диск или в папку облака.
//
//  Вернуть базу из копии: останови сервер, переименуй нужный файл
//  rp_backup_....db в rp.db, запусти сервер.
//
//  /backup — сделать копию прямо сейчас (главный администратор).
// =====================================================================

#define BACKUP_KEEP         14          // сколько копий хранить

static gBackupDay = -1;                 // число месяца, за которое копия уже проверена

stock Backup_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS backups (id INTEGER PRIMARY KEY AUTOINCREMENT, file TEXT NOT NULL UNIQUE, date INTEGER DEFAULT 0)"));
	Backup_Daily();
}

// Сделать копию. Возвращает true, если файл появился.
stock bool:Backup_Run()
{
	new y, m, d, file[40];
	getdate(y, m, d);
	format(file, sizeof file, "rp_backup_%04d-%02d-%02d.db", y, m, d);

	if (fexist(file)) fremove(file);    // сегодняшнюю копию заменяем свежей
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO backups (file, date) VALUES ('%q', %d)", file, gettime()));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "VACUUM INTO 'scriptfiles/%q'", file));

	if (!fexist(file))
	{
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM backups WHERE file = '%q'", file));
		printf("[backup] ERROR: copy %s was not created", file);
		SendAdminMessage(COLOR_RED, "[Копия базы] Не удалось сохранить копию базы. Сообщи разработчику.");
		return false;
	}
	printf("[backup] saved scriptfiles/%s", file);

	// Удалить копии старше BACKUP_KEEP последних
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT file FROM backups ORDER BY date DESC, id DESC LIMIT -1 OFFSET %d", BACKUP_KEEP);
	if (r != DBResult:0)
	{
		new old[40];
		for (new i = 0, n = DB_GetRowCount(r); i < n; i++)
		{
			DB_GetFieldString(r, 0, old, sizeof old);
			if (old[0] != '\0' && fexist(old)) fremove(old);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM backups WHERE id NOT IN (SELECT id FROM backups ORDER BY date DESC, id DESC LIMIT %d)", BACKUP_KEEP));
	return true;
}

// Раз в минуту из rp.pwn (вне транзакции): наступил новый день — делаем копию
stock Backup_Daily()
{
	new y, m, d, file[40];
	getdate(y, m, d);
	if (d == gBackupDay) return;
	gBackupDay = d;
	format(file, sizeof file, "rp_backup_%04d-%02d-%02d.db", y, m, d);
	if (!fexist(file)) Backup_Run();
}

stock bool:Cmd_Backup(playerid)
{
	if (!Admin_Check(playerid, ADMIN_MAX_LEVEL)) return true;
	if (Backup_Run()) SendClientMessage(playerid, COLOR_ADMIN, "Копия базы сохранена в scriptfiles (rp_backup_сегодняшняя дата.db).");
	else SendClientMessage(playerid, COLOR_RED, "Копию сохранить не удалось, подробности в консоли сервера.");
	Admin_Log(playerid, "backup", INVALID_PLAYER_ID);
	return true;
}
