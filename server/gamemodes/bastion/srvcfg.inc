// =====================================================================
//  srvcfg.inc — настройки сервера из игры: /srvset.
//
//  Главный админ (ADMIN_MAX_LEVEL) открывает /srvset: разделы > настройка >
//  новое значение. Меняется сразу, без перезапуска, и хранится в базе
//  (таблица server_settings). Пусто или «default» — вернуть как было.
//
//  Новая настройка: строчка в enum CFG_* и в gCfgDef (тот же порядок).
//  В коде значение берётся так: Cfg(CFG_...).
//  Налоги хранятся в economy.inc (/settax), здесь только показаны и меняются.
// =====================================================================

#define SRVCFG_ADMIN_LEVEL  7           // главный админ (ADMIN_MAX_LEVEL в admin.inc)

forward bool:Admin_Check(playerid, level);

enum
{
	CFGC_FACTORY,
	CFGC_MARKET,
	CFGC_ECONOMY,
	CFGC_COUNT
}

static const gCfgCatName[CFGC_COUNT][32] = { "Заводы", "Поставки и рынок", "Экономика" };

enum
{
	// Заводы
	CFG_FAC_PAY,
	CFG_FAC_BENCH,
	CFG_FAC_CAP_TIME,
	CFG_FAC_CAP_PAUSE,
	CFG_FAC_CLEAR,
	CFG_FAC_SAB_TIME,
	CFG_FAC_SAB_DOWN,
	CFG_FAC_SAB_PAUSE,
	// Поставки и рынок
	CFG_GOODS_PRICE,
	CFG_GOODS_PAY,
	CFG_GOODS_VALUE,
	CFG_BIZ_AIR,
	CFG_MKT_LOW,
	CFG_MKT_UP,
	CFG_MKT_HIGH,
	CFG_MKT_DOWN,
	// Экономика
	CFG_PAYDAY,
	CFG_START_MONEY,
	CFG_BIZ_TAX,
	CFG_TAX_INCOME,         // хранится в economy.inc
	CFG_TAX_CAR,            // хранится в economy.inc
	CFG_COUNT
}

enum E_CFG_DEF
{
	cdCat,
	cdKey[20],
	cdName[64],
	cdDef,
	cdMin,
	cdMax
}

static const gCfgDef[CFG_COUNT][E_CFG_DEF] =
{
	{ CFGC_FACTORY, "fac_pay",       "Рабочему за ящик, $",                        15,   0, 5000 },
	{ CFGC_FACTORY, "fac_bench",     "Время у станка, сек",                        8,    2, 60 },
	{ CFGC_FACTORY, "fac_cap_time",  "Захват: стоять на маркере, сек",             60,   10, 600 },
	{ CFGC_FACTORY, "fac_cap_pause", "Захват: пауза для одного завода, мин",       30,   0, 1440 },
	{ CFGC_FACTORY, "fac_clear",     "Захват: радиус защитников, м",               60,   10, 300 },
	{ CFGC_FACTORY, "fac_sab_time",  "Саботаж: закладывать у станка, сек",         20,   5, 300 },
	{ CFGC_FACTORY, "fac_sab_down",  "Саботаж: завод стоит, мин",                  30,   1, 600 },
	{ CFGC_FACTORY, "fac_sab_pause", "Саботаж: пауза для одного завода, мин",      60,   0, 1440 },

	{ CFGC_MARKET,  "goods_price",   "Ящик товара на заводе, $ (в общак)",         300,  0, 100000 },
	{ CFGC_MARKET,  "goods_pay",     "Бизнес платит водителю за ящик, $",          450,  0, 100000 },
	{ CFGC_MARKET,  "goods_value",   "Ящик пополняет склад бизнеса на, $",         500,  1, 100000 },
	{ CFGC_MARKET,  "biz_air",       "Закупка «из воздуха» дороже на, %",          50,   0, 500 },
	{ CFGC_MARKET,  "mkt_low",       "Дефицит: на складах меньше, %",              25,   0, 100 },
	{ CFGC_MARKET,  "mkt_up",        "При дефиците цены выше на, %",               20,   0, 200 },
	{ CFGC_MARKET,  "mkt_high",      "Избыток: на складах больше, %",              80,   0, 101 },
	{ CFGC_MARKET,  "mkt_down",      "При избытке цены ниже на, %",                10,   0, 90 },

	{ CFGC_ECONOMY, "payday",        "Зарплата на PayDay, $",                      PAYDAY_SALARY_DEF, 0, 100000 },
	{ CFGC_ECONOMY, "start_money",   "Деньги новичка, $",                          START_MONEY_DEF,   0, 10000000 },
	{ CFGC_ECONOMY, "biz_tax",       "Налог с продажи в бизнесе, %",               10,   0, 90 },
	{ CFGC_ECONOMY, "tax_income",    "Подоходный налог, %",                        10,   0, 50 },
	{ CFGC_ECONOMY, "tax_car",       "Налог на машину, ‰ от цены в час",           1,    0, 20 }
};

static gCfgVal[CFG_COUNT];
static gCfgCat[MAX_PLAYERS];
static gCfgSel[MAX_PLAYERS];
static gCfgMap[MAX_PLAYERS][CFG_COUNT];

forward Econ_GetTaxValue(car);
forward Econ_SetTaxValue(car, value);

stock Cfg(id)
{
	if (id == CFG_TAX_INCOME) return Econ_GetTaxValue(0);
	if (id == CFG_TAX_CAR) return Econ_GetTaxValue(1);
	return gCfgVal[id];
}

stock Cfg_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS server_settings (key TEXT PRIMARY KEY, value INTEGER)"));
	for (new i = 0; i < CFG_COUNT; i++) gCfgVal[i] = gCfgDef[i][cdDef];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT key, value FROM server_settings");
	if (r == DBResult:0) return;
	new key[24];
	for (new k = 0, rows = DB_GetRowCount(r); k < rows; k++)
	{
		DB_GetFieldString(r, 0, key, sizeof key);
		new v = DB_GetFieldInt(r, 1);
		for (new i = 0; i < CFG_COUNT; i++)
		{
			if (strcmp(key, gCfgDef[i][cdKey], false) != 0) continue;
			if (v >= gCfgDef[i][cdMin] && v <= gCfgDef[i][cdMax]) gCfgVal[i] = v;
			break;
		}
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
}

static Cfg_Set(id, value)
{
	if (id == CFG_TAX_INCOME) { Econ_SetTaxValue(0, value); return; }
	if (id == CFG_TAX_CAR) { Econ_SetTaxValue(1, value); return; }
	gCfgVal[id] = value;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO server_settings (key, value) VALUES ('%q', %d)", gCfgDef[id][cdKey], value));
}

stock bool:Cmd_SrvSet(playerid)
{
	if (!Admin_Check(playerid, SRVCFG_ADMIN_LEVEL)) return true;
	new body[512], line[64];
	for (new c = 0; c < CFGC_COUNT; c++)
	{
		format(line, sizeof line, "%s\n", gCfgCatName[c]);
		strcat(body, line);
	}
	ShowPlayerDialog(playerid, DIALOG_SRVCFG, DIALOG_STYLE_LIST, "Настройки сервера", "%s", "Открыть", "Закрыть", body);
	return true;
}

static Cfg_ShowCat(playerid)
{
	new c = gCfgCat[playerid], body[2400], line[128], n = 0;
	strcat(body, "Настройка\tЗначение\n");
	for (new i = 0; i < CFG_COUNT; i++)
	{
		if (gCfgDef[i][cdCat] != c) continue;
		new v = Cfg(i);
		format(line, sizeof line, "%s\t%s%d\n", gCfgDef[i][cdName], (v == gCfgDef[i][cdDef]) ? ("{FFFFFF}") : ("{FFFF00}"), v);
		strcat(body, line);
		gCfgMap[playerid][n++] = i;
	}
	for (new k = n; k < CFG_COUNT; k++) gCfgMap[playerid][k] = -1;
	ShowPlayerDialog(playerid, DIALOG_SRVCFG_CAT, DIALOG_STYLE_TABLIST_HEADERS, gCfgCatName[c], "%s", "Изменить", "Назад", body);
}

stock SrvCfg_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid != DIALOG_SRVCFG && dialogid != DIALOG_SRVCFG_CAT && dialogid != DIALOG_SRVCFG_EDIT) return 0;
	if (Player[playerid][pAdmin] < SRVCFG_ADMIN_LEVEL) return 1;
	switch (dialogid)
	{
		case DIALOG_SRVCFG:
		{
			if (!response || listitem < 0 || listitem >= CFGC_COUNT) return 1;
			gCfgCat[playerid] = listitem;
			Cfg_ShowCat(playerid);
		}
		case DIALOG_SRVCFG_CAT:
		{
			if (!response) { Cmd_SrvSet(playerid); return 1; }
			if (listitem < 0 || listitem >= CFG_COUNT || gCfgMap[playerid][listitem] == -1) return 1;
			new i = gCfgMap[playerid][listitem];
			gCfgSel[playerid] = i;
			ShowPlayerDialog(playerid, DIALOG_SRVCFG_EDIT, DIALOG_STYLE_INPUT, "Изменить настройку",
				"{FFFFFF}%s\n\nСейчас: {FFFF00}%d{FFFFFF}, по умолчанию: %d\nМожно от %d до %d. Пусто или default — вернуть по умолчанию.",
				"Сохранить", "Назад", gCfgDef[i][cdName], Cfg(i), gCfgDef[i][cdDef], gCfgDef[i][cdMin], gCfgDef[i][cdMax]);
		}
		case DIALOG_SRVCFG_EDIT:
		{
			new i = gCfgSel[playerid];
			if (response && i >= 0 && i < CFG_COUNT)
			{
				new v;
				if (inputtext[0] == '\0' || !strcmp(inputtext, "default", true)) v = gCfgDef[i][cdDef];
				else if (!IsNumeric(inputtext) || strlen(inputtext) > 9) { SendClientMessage(playerid, COLOR_GREY, "Нужно число."); Cfg_ShowCat(playerid); return 1; }
				else v = strval(inputtext);
				if (v < gCfgDef[i][cdMin] || v > gCfgDef[i][cdMax])
					SendClientMessage(playerid, COLOR_GREY, "Можно от %d до %d.", gCfgDef[i][cdMin], gCfgDef[i][cdMax]);
				else
				{
					Cfg_Set(i, v);
					Admin_Log(playerid, "srvset", INVALID_PLAYER_ID, gCfgDef[i][cdKey], v);
					SendClientMessage(playerid, COLOR_ADMIN, "%s: теперь %d.", gCfgDef[i][cdName], v);
				}
			}
			Cfg_ShowCat(playerid);
		}
	}
	return 1;
}
