// =====================================================================
//  economy.inc — казна штата, налоги и журнал денег.
//
//  Журнал денег (таблица money_log): каждое изменение наличных через
//  GiveMoney с причиной или на сумму от ECON_LOG_MIN, а также банковские
//  переводы. Хранится ECON_LOG_DAYS дней. Админ (3+): /moneylog [id] [N].
//
//  Казна (таблица econ, ключ treasury): сюда идут налоги и штрафы.
//    * подоходный налог — tax_income% с зарплаты на PayDay;
//    * налог на машину — tax_car промилле (‰) от цены модели каждый PayDay,
//      пока владелец в игре (сначала со счёта в банке, потом наличными);
//    * налог на дома (houses.inc), штрафы штрафстоянки (garage.inc),
//      счёт больницы, сборы за экзамены (licenses.inc).
//  /taxes — ставки и казна для всех. Гл. админ (5): /settax [income|car] [значение].
// =====================================================================

#define ECON_LOG_MIN        500     // писать в журнал суммы от $500 (с причиной — любые)
#define ECON_LOG_DAYS       30
#define TAX_INCOME_DEFAULT  10      // %
#define TAX_CAR_DEFAULT     2       // ‰ от цены машины в час
#define TAX_INCOME_MAX      50
#define TAX_CAR_MAX         20
#define TAX_CAR_MIN_SUM     10      // налог на машину не меньше $10 в час

static gTreasury = 0;
static gTaxIncome = TAX_INCOME_DEFAULT;
static gTaxCar = TAX_CAR_DEFAULT;

// ---------------------------------------------------------------------
//  Запуск
// ---------------------------------------------------------------------
stock Econ_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS money_log (id INTEGER PRIMARY KEY AUTOINCREMENT, time INTEGER, account INTEGER, amount INTEGER, cash INTEGER, reason TEXT)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS money_log_account ON money_log (account, id)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS econ (key TEXT PRIMARY KEY, value INTEGER)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM money_log WHERE time < %d", gettime() - ECON_LOG_DAYS * 86400));

	gTreasury = Econ_LoadValue("treasury", 0);
	gTaxIncome = Econ_LoadValue("tax_income", TAX_INCOME_DEFAULT);
	gTaxCar = Econ_LoadValue("tax_car", TAX_CAR_DEFAULT);
	printf("[Economy] Treasury $%d, income tax %d%%, car tax %d permille.", gTreasury, gTaxIncome, gTaxCar);
}

static Econ_LoadValue(const key[], def)
{
	new value = def;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT value FROM econ WHERE key = '%q'", key);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) value = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return value;
}

static Econ_SaveValue(const key[], value)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO econ (key, value) VALUES ('%q', %d)", key, value));
}

// ---------------------------------------------------------------------
//  Журнал денег
// ---------------------------------------------------------------------
// Из GiveMoney (base.inc): записать изменение наличных
stock Econ_LogMoney(playerid, amount, const reason[])
{
	if (Player[playerid][pDbId] == 0 || amount == 0) return;
	if (reason[0] == '\0' && amount < ECON_LOG_MIN && amount > -ECON_LOG_MIN) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO money_log (time, account, amount, cash, reason) VALUES (%d, %d, %d, %d, '%q')",
		gettime(), Player[playerid][pDbId], amount, Player[playerid][pMoney], reason));
}

// ---------------------------------------------------------------------
//  Казна
// ---------------------------------------------------------------------
// Деньги в казну (налоги, штрафы, сборы)
stock Econ_ToTreasury(amount)
{
	if (amount <= 0) return;
	gTreasury += amount;
	Econ_SaveValue("treasury", gTreasury);
}

stock Econ_GetTreasury() { return gTreasury; }

// Взять из казны (премии, выплаты). false — в казне не хватает
stock bool:Econ_FromTreasury(amount)
{
	if (amount <= 0 || gTreasury < amount) return false;
	gTreasury -= amount;
	Econ_SaveValue("treasury", gTreasury);
	return true;
}

// Взять с игрока сумму: сначала со счёта в банке, потом наличными. true — получилось
static bool:Econ_Charge(playerid, sum, const reason[])
{
	if (Player[playerid][pBank] >= sum)
	{
		Player[playerid][pBank] -= sum;
		new DBResult:r = DB_ExecuteQuery(gDB, "INSERT INTO money_log (time, account, amount, cash, reason) VALUES (%d, %d, %d, %d, 'банк: %q')",
			gettime(), Player[playerid][pDbId], -sum, Player[playerid][pMoney], reason);
		DB_FreeResultSet(r);
		return true;
	}
	if (GetMoney(playerid) >= sum)
	{
		GiveMoney(playerid, -sum, reason);
		return true;
	}
	return false;
}

// Налог на машину в час (0 — нет машины)
stock Econ_GetCarTax(playerid)
{
	new v = gPlayerVeh[playerid];
	if (v == INVALID_VEHICLE_ID) return 0;
	new tax = Vehicles_GetModelPrice(GetVehicleModel(v)) * gTaxCar / 1000;
	if (tax < TAX_CAR_MIN_SUM) tax = TAX_CAR_MIN_SUM;
	return tax;
}

// На PayDay (из rp.pwn) после зарплаты: подоходный налог и налог на машину
stock Econ_PayDay(playerid, salary)
{
	new itax = salary * gTaxIncome / 100;
	if (itax > 0)
	{
#if PAYDAY_TO_BANK
		// зарплата пришла на счёт (bank.inc) — налог тоже со счёта
		if (!Econ_Charge(playerid, itax, "подоходный налог")) GiveMoney(playerid, -itax, "подоходный налог");
#else
		GiveMoney(playerid, -itax, "подоходный налог");
#endif
		Econ_ToTreasury(itax);
		SendClientMessage(playerid, COLOR_WHITE, "Подоходный налог %d%%: -$%d.", gTaxIncome, itax);
	}

	new ctax = Econ_GetCarTax(playerid);
	if (ctax > 0)
	{
		// не списывается сам: копится на налоговом счёте, игрок платит в банкомате (debts.inc)
		Debts_Add(playerid, DEBT_TAX, ctax);
		SendClientMessage(playerid, COLOR_WHITE, "Налог на машину: $%d начислен на налоговый счёт.", ctax);
	}
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Cmd_Taxes(playerid)
{
	new text[700];
	format(text, sizeof text,
		"{FFFFFF}Казна штата: {33AA33}$%d\n\n{FFFF00}Налоги\n{FFFFFF}С зарплаты: %d%%\nНа машину: %d‰ от цены в час (не меньше $%d)\nНа дом: 0.1%% цены в час (не меньше $%d), со счёта дома\nНалог на машину копится на налоговом счёте: /debts\n\nТвой налог на машину: $%d в час",
		gTreasury, gTaxIncome, gTaxCar, TAX_CAR_MIN_SUM, HOUSE_TAX_MIN, Econ_GetCarTax(playerid));
	new h = House_GetOwned(playerid);
	if (h != -1)
	{
		new line[96];
		format(line, sizeof line, "\nТвой налог на дом: $%d в час, на счёте дома $%d", House_GetTax(h), House[h][hBalance]);
		strcat(text, line);
	}
	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "Налоги", "%s", "Закрыть", "", text);
	return true;
}

stock bool:Cmd_SetTax(playerid, const params[])
{
	if (!Admin_Check(playerid, ADMIN_MAX_LEVEL)) return true;
	new pos = 0, kind[16], value;
	if (!NextParam(params, pos, kind) || !NextIntParam(params, pos, value))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /settax income [0-%d %%] или /settax car [0-%d ‰]", TAX_INCOME_MAX, TAX_CAR_MAX);

	if (!strcmp(kind, "income", true))
	{
		if (value < 0 || value > TAX_INCOME_MAX) return SendClientMessage(playerid, COLOR_GREY, "Подоходный налог от 0 до %d%%.", TAX_INCOME_MAX);
		gTaxIncome = value;
		Econ_SaveValue("tax_income", value);
		Admin_Log(playerid, "settax income", INVALID_PLAYER_ID, "", value);
		SendClientMessageToAll(COLOR_YELLOW, "[Правительство] Подоходный налог теперь %d%%.", value);
	}
	else if (!strcmp(kind, "car", true))
	{
		if (value < 0 || value > TAX_CAR_MAX) return SendClientMessage(playerid, COLOR_GREY, "Налог на машину от 0 до %d‰.", TAX_CAR_MAX);
		gTaxCar = value;
		Econ_SaveValue("tax_car", value);
		Admin_Log(playerid, "settax car", INVALID_PLAYER_ID, "", value);
		SendClientMessageToAll(COLOR_YELLOW, "[Правительство] Налог на машину теперь %d‰ от цены в час.", value);
	}
	else return SendClientMessage(playerid, COLOR_GREY, "Вид налога: income (с зарплаты) или car (на машину).");
	return true;
}

stock bool:Cmd_MoneyLog(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new pos = 0, count = 15;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /moneylog [id] [сколько записей, до 40]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	NextIntParam(params, pos, count);
	if (count < 1) count = 1;
	if (count > 40) count = 40;

	new DBResult:r = DB_ExecuteQuery(gDB,
		"SELECT strftime('%%d.%%m %%H:%%M', time, 'unixepoch', 'localtime'), amount, cash, reason FROM money_log WHERE account = %d ORDER BY id DESC LIMIT %d",
		Player[target][pDbId], count);
	if (r == DBResult:0) return true;
	new rows = DB_GetRowCount(r);
	new text[3600], line[160], when[16], reason[64];
	format(text, sizeof text, "{FFFFFF}Наличные сейчас: $%d, в банке $%d\n\n", GetMoney(target), Player[target][pBank]);
	if (rows == 0) strcat(text, "Записей нет.");
	for (new i = 0; i < rows; i++)
	{
		DB_GetFieldString(r, 0, when, sizeof when);
		DB_GetFieldString(r, 3, reason, sizeof reason);
		new amount = DB_GetFieldInt(r, 1);
		if (reason[0] == '\0') strcat(reason, "-");
		if (amount >= 0)
			format(line, sizeof line, "%s  {33AA33}+$%d{FFFFFF}  (стало $%d)  %s\n", when, amount, DB_GetFieldInt(r, 2), reason);
		else
			format(line, sizeof line, "%s  {FF6347}-$%d{FFFFFF}  (стало $%d)  %s\n", when, -amount, DB_GetFieldInt(r, 2), reason);
		strcat(text, line);
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	new title[64];
	format(title, sizeof title, "Деньги: %s", GetRPName(target));
	Admin_ShowInfo(playerid, title, text);
	return true;
}
