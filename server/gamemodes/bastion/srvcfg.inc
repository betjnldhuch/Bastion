// =====================================================================
//  srvcfg.inc — настройки сервера из игры: /srvset.
//
//  Главный админ (ADMIN_MAX_LEVEL) открывает /srvset: разделы > настройка >
//  новое значение. Меняется сразу, без перезапуска, и хранится в базе
//  (таблица server_settings). Пусто или «default» — вернуть как было.
//
//  Новая настройка: строчка в enum CFG_* и в gCfgDef (тот же порядок).
//  В коде значение берётся так: Cfg(CFG_...).
//  Разделы: игроки и цены, банк, фракции, банды и районы, бизнесы, заводы,
//  поставки и рынок, пожары, налоги. Налоги с зарплаты и на машину хранятся
//  в economy.inc (/settax), здесь только показаны и меняются.
// =====================================================================

#define SRVCFG_ADMIN_LEVEL  7           // главный админ (ADMIN_MAX_LEVEL в admin.inc)

forward bool:Admin_Check(playerid, level);

enum
{
	CFGC_PLAYERS,
	CFGC_BANK,
	CFGC_FACTIONS,
	CFGC_GANGS,
	CFGC_BIZ,
	CFGC_FACTORY,
	CFGC_MARKET,
	CFGC_FIRE,
	CFGC_TAX,
	CFGC_COUNT
}

static const gCfgCatName[CFGC_COUNT][32] = { "Игроки и цены", "Банк", "Фракции", "Банды и районы", "Бизнесы", "Заводы", "Поставки и рынок", "Пожары", "Налоги" };

enum
{
	CFG_START_MONEY,
	CFG_PAYDAY,
	CFG_REFERRAL_REWARD,
	CFG_REFERRAL_LEVEL,
	CFG_EXP_PER_LEVEL,
	CFG_FUEL_PRICE,
	CFG_FUEL_CANISTER,
	CFG_LIC_CAR,
	CFG_LIC_MOTO,
	CFG_LIC_BOAT,
	CFG_LIC_GUN,
	CFG_LIC_GUN_LEVEL,
	CFG_HEAL_PRICE,
	CFG_HOSPITAL_FEE,
	CFG_SMS_PRICE,
	CFG_BANK_MAX_TRANSFER,
	CFG_BANK_OPEN_COST,
	CFG_BANK_ATM_FEE,
	CFG_BANK_TRANSFER_FEE,
	CFG_BANK_INTEREST,
	CFG_BANK_INTEREST_MAX,
	CFG_FSAL_BASE,
	CFG_FSAL_RANK,
	CFG_FSTATE_BUDGET,
	CFG_FMAX_SALARY,
	CFG_FSTATE_MAX_SALARY,
	CFG_ARREST_REWARD,
	CFG_JAIL_PER_STAR,
	CFG_SUPPLY_HAUL,
	CFG_SUPPLY_DELIVER,
	CFG_STEAL_MIN_ARMY,
	CFG_CAPTURE_FROM,
	CFG_CAPTURE_TO,
	CFG_TURF_TIME,
	CFG_TURF_COOLDOWN,
	CFG_TURF_RANK,
	CFG_TURF_INCOME,
	CFG_TURF_BANK,
	CFG_BIZ_TAX,
	CFG_BIZ_MARKUP_MIN,
	CFG_BIZ_MARKUP_MAX,
	CFG_BIZ_START_STOCK,
	CFG_BIZ_RENAME,
	CFG_FAC_PAY,
	CFG_FAC_BENCH,
	CFG_FAC_CAP_TIME,
	CFG_FAC_CAP_PAUSE,
	CFG_FAC_CLEAR,
	CFG_FAC_SAB_TIME,
	CFG_FAC_SAB_DOWN,
	CFG_FAC_REPAIR,
	CFG_FAC_SAB_PAUSE,
	CFG_FAC_SAB_MIL,
	CFG_GOODS_PRICE,
	CFG_GOODS_PAY,
	CFG_GOODS_VALUE,
	CFG_BIZ_AIR,
	CFG_MKT_LOW,
	CFG_MKT_UP,
	CFG_MKT_HIGH,
	CFG_MKT_DOWN,
	CFG_FIRE_PAY,
	CFG_FIRE_BONUS,
	CFG_FIRE_BURN,
	CFG_FIRE_SPREAD,
	CFG_FIRE_LOSS,
	CFG_FIRE_RANDOM,
	CFG_FIRE_MIN_FF,
	CFG_FIRE_FINE,
	CFG_FIRE_CHECK_PAY,
	CFG_FIRE_CHECK_HOURS,
	CFG_TAX_INCOME,
	CFG_TAX_CAR,
	CFG_TAX_CAR_MIN,
	CFG_TAX_CAR_MAX,
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
	{ CFGC_PLAYERS, "start_money", "Деньги новичка, $", START_MONEY_DEF, 0, 10000000 },
	{ CFGC_PLAYERS, "payday", "Зарплата на PayDay, $", PAYDAY_SALARY_DEF, 0, 100000 },
	{ CFGC_PLAYERS, "referral_reward", "Награда за приглашённого, $", 5000, 0, 10000000 },
	{ CFGC_PLAYERS, "referral_level", "Награда, когда новичок дойдёт до уровня", 3, 1, 100 },
	{ CFGC_PLAYERS, "exp_per_level", "Опыта на уровень (x уровень)", 4, 1, 100 },
	{ CFGC_PLAYERS, "fuel_price", "Цена литра на новых АЗС, $", 5, 0, 1000 },
	{ CFGC_PLAYERS, "fuel_canister", "Литров в канистре", 20, 1, 60 },
	{ CFGC_PLAYERS, "lic_car", "Права на машину, $", 1500, 0, 1000000 },
	{ CFGC_PLAYERS, "lic_moto", "Права на мото, $", 1000, 0, 1000000 },
	{ CFGC_PLAYERS, "lic_boat", "Права на лодку, $", 2000, 0, 1000000 },
	{ CFGC_PLAYERS, "lic_gun", "Лицензия на оружие, $", 5000, 0, 1000000 },
	{ CFGC_PLAYERS, "lic_gun_level", "Лицензия на оружие с уровня", 2, 1, 100 },
	{ CFGC_PLAYERS, "heal_price", "Лечение у медика (/heal), $", 200, 0, 100000 },
	{ CFGC_PLAYERS, "hospital_fee", "Счёт больницы после смерти, $", 100, 0, 100000 },
	{ CFGC_PLAYERS, "sms_price", "Одно SMS, $", 5, 0, 1000 },
	{ CFGC_BANK, "bank_max_transfer", "Максимум за один перевод, $", 1000000, 1, 1000000000 },
	{ CFGC_BANK, "bank_open_cost", "Открыть доп. счёт, $", 10000, 0, 10000000 },
	{ CFGC_BANK, "bank_atm_fee", "Комиссия банкомата, %", 1, 0, 50 },
	{ CFGC_BANK, "bank_transfer_fee", "Комиссия за перевод, %", 1, 0, 50 },
	{ CFGC_BANK, "bank_interest", "Проценты на остаток за PayDay, 0.1%", 1, 0, 100 },
	{ CFGC_BANK, "bank_interest_max", "Максимум процентов за PayDay, $", 100, 0, 10000000 },
	{ CFGC_FACTIONS, "fsal_base", "Зарплата госструктур: база, $", 300, 0, 100000 },
	{ CFGC_FACTIONS, "fsal_rank", "Зарплата госструктур: за ранг, $", 100, 0, 100000 },
	{ CFGC_FACTIONS, "fstate_budget", "Бюджет госструктуры за PayDay, $", 15000, 0, 10000000 },
	{ CFGC_FACTIONS, "fmax_salary", "Потолок зарплаты ранга, $", 20000, 0, 10000000 },
	{ CFGC_FACTIONS, "fstate_max_salary", "Потолок зарплаты в госструктуре, $", 3000, 0, 10000000 },
	{ CFGC_FACTIONS, "arrest_reward", "Полицейскому за звезду при аресте, $", 150, 0, 100000 },
	{ CFGC_FACTIONS, "jail_per_star", "Минут тюрьмы за звезду", 5, 1, 120 },
	{ CFGC_FACTIONS, "supply_haul", "Армии: за 100 материалов на свой склад, $", 30, 0, 100000 },
	{ CFGC_FACTIONS, "supply_deliver", "Армии: за 100 материалов другим, $", 50, 0, 100000 },
	{ CFGC_FACTIONS, "steal_min_army", "Склад армии грабят, если военных в сети", 2, 0, 50 },
	{ CFGC_GANGS, "capture_from", "Захват районов: с часа", 16, 0, 24 },
	{ CFGC_GANGS, "capture_to", "Захват районов: до часа", 24, 0, 24 },
	{ CFGC_GANGS, "turf_time", "Захват района длится, сек", 300, 30, 3600 },
	{ CFGC_GANGS, "turf_cooldown", "Район снова можно атаковать через, сек", 1800, 0, 86400 },
	{ CFGC_GANGS, "turf_rank", "Начать захват района с ранга", 3, 1, 10 },
	{ CFGC_GANGS, "turf_income", "Доход бойцу за район на PayDay, $", 150, 0, 100000 },
	{ CFGC_GANGS, "turf_bank", "В общак банды за район на PayDay, $", 300, 0, 1000000 },
	{ CFGC_BIZ, "biz_tax", "Налог с продажи в бизнесе, %", 10, 0, 90 },
	{ CFGC_BIZ, "biz_markup_min", "Цены владельца: от, %", 70, 10, 100 },
	{ CFGC_BIZ, "biz_markup_max", "Цены владельца: до, %", 150, 100, 1000 },
	{ CFGC_BIZ, "biz_start_stock", "Товар при покупке у штата, $", 2000, 0, 1000000 },
	{ CFGC_BIZ, "biz_rename", "Смена названия бизнеса, $", 1000, 0, 1000000 },
	{ CFGC_FACTORY, "fac_pay", "Рабочему за ящик, $", 15, 0, 5000 },
	{ CFGC_FACTORY, "fac_bench", "Время у станка, сек", 8, 2, 60 },
	{ CFGC_FACTORY, "fac_cap_time", "Захват: стоять на маркере, сек", 60, 10, 600 },
	{ CFGC_FACTORY, "fac_cap_pause", "Захват: пауза для одного завода, мин", 30, 0, 1440 },
	{ CFGC_FACTORY, "fac_clear", "Захват: радиус защитников, м", 60, 10, 300 },
	{ CFGC_FACTORY, "fac_sab_time", "Поджог: стоять на складе, сек", 20, 5, 300 },
	{ CFGC_FACTORY, "fac_sab_down", "Склад сгорел: завод стоит, мин", 30, 1, 600 },
	{ CFGC_FACTORY, "fac_repair", "Пожар потушили: ремонт, мин", 5, 0, 600 },
	{ CFGC_FACTORY, "fac_sab_pause", "Поджог: пауза для одного завода, мин", 60, 0, 1440 },
	{ CFGC_FACTORY, "fac_sab_mil", "Можно поджигать военные заводы (1/0)", 1, 0, 1 },
	{ CFGC_MARKET, "goods_price", "Ящик товара на заводе, $ (в общак)", 300, 0, 100000 },
	{ CFGC_MARKET, "goods_pay", "Бизнес платит водителю за ящик, $", 450, 0, 100000 },
	{ CFGC_MARKET, "goods_value", "Ящик пополняет склад бизнеса на, $", 500, 1, 100000 },
	{ CFGC_MARKET, "biz_air", "Закупка «из воздуха» дороже на, %", 50, 0, 500 },
	{ CFGC_MARKET, "mkt_low", "Дефицит: на складах меньше, %", 25, 0, 100 },
	{ CFGC_MARKET, "mkt_up", "При дефиците цены выше на, %", 20, 0, 200 },
	{ CFGC_MARKET, "mkt_high", "Избыток: на складах больше, %", 80, 0, 101 },
	{ CFGC_MARKET, "mkt_down", "При избытке цены ниже на, %", 10, 0, 90 },
	{ CFGC_FIRE, "fire_pay", "Пожарному за очаг, $", 60, 0, 100000 },
	{ CFGC_FIRE, "fire_bonus", "Пожарным за весь пожар, $", 300, 0, 1000000 },
	{ CFGC_FIRE, "fire_burn", "Пожар догорает сам через, мин", 10, 1, 120 },
	{ CFGC_FIRE, "fire_spread", "Новый очаг раз в, сек", 40, 5, 600 },
	{ CFGC_FIRE, "fire_loss", "Горит склад: ящик пропадает раз в, сек", 30, 5, 600 },
	{ CFGC_FIRE, "fire_random", "Случайный пожар в бизнесе раз в, мин (0 — нет)", 90, 0, 1440 },
	{ CFGC_FIRE, "fire_min_ff", "Случайные пожары, если пожарных на смене", 2, 1, 50 },
	{ CFGC_FIRE, "fire_fine", "Штраф без сигнализации, $", 2000, 0, 1000000 },
	{ CFGC_FIRE, "fire_check_pay", "Пожарному за проверку, $", 100, 0, 100000 },
	{ CFGC_FIRE, "fire_check_hours", "Один бизнес проверяют раз в, часов", 24, 1, 720 },
	{ CFGC_TAX, "tax_income", "Подоходный налог, %", 10, 0, 50 },
	{ CFGC_TAX, "tax_car", "Налог на машину, ‰ от цены в час", 1, 0, 20 },
	{ CFGC_TAX, "tax_car_min", "Налог на машину: не меньше, $ в час", 10, 0, 100000 },
	{ CFGC_TAX, "tax_car_max", "Налог на машину: не больше, $ в час", 500, 0, 1000000 }
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
