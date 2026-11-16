// =====================================================================
//  dealers.inc — автосалоны, авиасалон и личный транспорт игрока.
//
//  * Салонов несколько, у каждого свой набор моделей и цен (таблицы ниже).
//    ALT у значка салона или /buycar — купить, /sellcar — продать текущую
//    машину за DEALER_REFUND_PCT % цены (в любом салоне).
//  * У игрока может быть несколько машин и вертолётов. Лимит —
//    Props_MaxVehicles (property.inc). На улице стоит одна, остальные в
//    гараже: /cars — список, вызвать другую или найти текущую по GPS.
//  * Значки салонов двигаются в игре (/markers), место выдачи купленного
//    транспорта — /dealerspawn (главный админ, стоя на нужном месте).
//
//  ЦЕНЫ И СОСТАВ САЛОНОВ — в gDealerCars. Строка: { салон, модель, цена }.
//  Новый салон — строка в gDealer, новая машина — строка в gDealerCars,
//  больше ничего менять не надо.
// =====================================================================

#define DEALER_RANGE        3.0     // как близко к значку салона
#define DEALER_REFUND_PCT   50      // сколько процентов цены возвращают при продаже
#define DEALER_NO_PRICE     10000   // "цена" модели, которой нет ни в одном салоне (для налога)
#define DEALER_SELL_MIN     1000    // за модель не из салона дают столько
#define CARS_MENU_MAX       12      // строк в /cars
#define CARS_SUMMON_DELAY   60      // раз в сколько секунд можно вызывать другую машину

#define DEALER_ECONOMY      0
#define DEALER_PREMIUM      1
#define DEALER_AIR          2

enum E_DEALER
{
	dlName[32],
	dlWhat[24],                 // что продаёт, для надписи
	Float:dlX, Float:dlY, Float:dlZ,                    // значок (точка покупки)
	dlWorld, dlInt,                                     // мир и интерьер значка
	Float:dlSX, Float:dlSY, Float:dlSZ, Float:dlSA      // где появляется купленный транспорт
}

// Места — стандартные салоны игры. Значки правятся в игре: /markers, выдача: /dealerspawn
static gDealer[][E_DEALER] =
{
	// Bastion Motors у Coutt & Schutz: покупка у стойки менеджера в зале (interiors.inc), машину выдают на улице
	{ "Автосалон «Эконом»",  "машину или мотоцикл", 2850.0, -2839.8, 1501.0, SALON_WORLD, SALON_INTERIOR,   2124.0, -1135.0, 25.3, 0.0 },
	{ "Автосалон «Премиум»", "машину",              542.3, -1291.6, 17.2, 0, 0,    552.0, -1270.0, 17.3, 0.0 },     // Grotti, Родео
	{ "Авиасалон",           "вертолёт",            1685.8, -2238.9, 13.5, 0, 0,   1800.0, -2450.0, 13.8, 180.0 }   // аэропорт Лос-Сантоса
};
static gDealerMk[sizeof (gDealer)] = { -1, ... };

// Что стоит на подиумах зала Bastion Motors (SALON_SLOTS мест, interiors.inc)
static const gShowModels[SALON_SLOTS] = { 401, 516, 405, 426, 489, 521 };
static bool:gVehShow[MAX_VEHICLES];     // выставочная машина: закрыта, уехать нельзя

// { салон, модель, цена }. Итоговые цены — из таблицы экономики сервера
static const gDealerCars[][3] =
{
	// --- Эконом: мототехника
	{ DEALER_ECONOMY, 462, 3000 },      // Faggio
	{ DEALER_ECONOMY, 468, 15000 },     // Sanchez
	{ DEALER_ECONOMY, 586, 18000 },     // Wayfarer
	{ DEALER_ECONOMY, 463, 22000 },     // Freeway
	{ DEALER_ECONOMY, 581, 28000 },     // BF-400
	{ DEALER_ECONOMY, 461, 32000 },     // PCJ-600
	{ DEALER_ECONOMY, 521, 38000 },     // FCR-900
	// --- Эконом: машины
	{ DEALER_ECONOMY, 410, 9000 },      // Manana
	{ DEALER_ECONOMY, 404, 9500 },      // Perennial
	{ DEALER_ECONOMY, 542, 10000 },     // Clover
	{ DEALER_ECONOMY, 436, 11000 },     // Previon
	{ DEALER_ECONOMY, 401, 12000 },     // Bravura
	{ DEALER_ECONOMY, 492, 14000 },     // Greenwood
	{ DEALER_ECONOMY, 496, 15000 },     // Blista Compact
	{ DEALER_ECONOMY, 516, 18000 },     // Nebula
	{ DEALER_ECONOMY, 589, 20000 },     // Club
	{ DEALER_ECONOMY, 426, 22000 },     // Premier
	{ DEALER_ECONOMY, 405, 25000 },     // Sentinel
	{ DEALER_ECONOMY, 500, 28000 },     // Mesa
	{ DEALER_ECONOMY, 400, 30000 },     // Landstalker
	{ DEALER_ECONOMY, 489, 35000 },     // Rancher
	// --- Премиум
	{ DEALER_PREMIUM, 445, 40000 },     // Admiral
	{ DEALER_PREMIUM, 421, 45000 },     // Washington
	{ DEALER_PREMIUM, 565, 48000 },     // Flash
	{ DEALER_PREMIUM, 558, 52000 },     // Uranus
	{ DEALER_PREMIUM, 533, 55000 },     // Feltzer
	{ DEALER_PREMIUM, 560, 60000 },     // Sultan
	{ DEALER_PREMIUM, 579, 65000 },     // Huntley
	{ DEALER_PREMIUM, 562, 70000 },     // Elegy
	{ DEALER_PREMIUM, 559, 75000 },     // Jester
	{ DEALER_PREMIUM, 580, 90000 },     // Stafford
	{ DEALER_PREMIUM, 480, 120000 },    // Comet
	{ DEALER_PREMIUM, 477, 130000 },    // ZR-350
	{ DEALER_PREMIUM, 429, 140000 },    // Banshee
	{ DEALER_PREMIUM, 506, 150000 },    // Super GT
	{ DEALER_PREMIUM, 522, 160000 },    // NRG-500
	{ DEALER_PREMIUM, 415, 180000 },    // Cheetah
	{ DEALER_PREMIUM, 541, 220000 },    // Bullet
	{ DEALER_PREMIUM, 451, 230000 },    // Turismo
	{ DEALER_PREMIUM, 411, 250000 },    // Infernus
	// --- Авиасалон: вертолёты (без вооружённых)
	{ DEALER_AIR, 469, 500000 },        // Sparrow
	{ DEALER_AIR, 487, 900000 },        // Maverick
	{ DEALER_AIR, 563, 1300000 },       // Raindance
	{ DEALER_AIR, 417, 1500000 }        // Leviathan
};

static gDealerAt[MAX_PLAYERS];                      // в каком салоне игрок открыл список
static gDealerPick[MAX_PLAYERS] = { -1, ... };      // какую строку gDealerCars выбрал
static gCarsMenu[MAX_PLAYERS][CARS_MENU_MAX];       // id машин (в базе) по строкам /cars
static gCarsMenuCount[MAX_PLAYERS];
static gCarsSummonAt[MAX_PLAYERS];                  // когда последний раз вызывал машину

// ---------------------------------------------------------------------
stock Dealers_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS dealer_spawns (id INTEGER PRIMARY KEY, x REAL, y REAL, z REAL, a REAL)"));
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, x, y, z, a FROM dealer_spawns");
	if (r != DBResult:0)
	{
		for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
		{
			new d = DB_GetFieldInt(r, 0);
			if (d >= 0 && d < sizeof (gDealer))
			{
				gDealer[d][dlSX] = DB_GetFieldFloat(r, 1);
				gDealer[d][dlSY] = DB_GetFieldFloat(r, 2);
				gDealer[d][dlSZ] = DB_GetFieldFloat(r, 3);
				gDealer[d][dlSA] = DB_GetFieldFloat(r, 4);
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}

	new text[96];
	for (new d = 0; d < sizeof (gDealer); d++)
	{
		gDealerMk[d] = Marker_Create(MK_DEALER, d, gDealer[d][dlName], 1274, gDealer[d][dlX], gDealer[d][dlY], gDealer[d][dlZ],
			gDealer[d][dlWorld], gDealer[d][dlInt], GPS_TRANSPORT);
		format(text, sizeof text, "{FFFF00}%s\n{FFFFFF}ALT — купить %s", gDealer[d][dlName], gDealer[d][dlWhat]);
		Marker_SetLabel(gDealerMk[d], text);
	}

	// Выставочные машины на подиумах зала
	new Float:x, Float:y, Float:z, Float:a;
	for (new i = 0; i < SALON_SLOTS; i++)
	{
		Salon_GetSlot(i, x, y, z, a);
		new v = CreateVehicle(gShowModels[i], x, y, z, a, 1, 1, 60);
		if (v == INVALID_VEHICLE_ID) continue;
		LinkVehicleToInterior(v, SALON_INTERIOR);
		SetVehicleVirtualWorld(v, SALON_WORLD);
		gVehShow[v] = true;
		Dealers_OnVehicleSpawn(v);
	}
}

// Из Vehicles_OnSpawn: выставочная машина всегда закрыта
stock Dealers_OnVehicleSpawn(vehicleid)
{
	if (!gVehShow[vehicleid]) return;
	LinkVehicleToInterior(vehicleid, SALON_INTERIOR);
	SetVehicleParamsEx(vehicleid, 0, 0, 0, 1, 0, 0, 0);
}

// У какого салона стоит игрок (-1 — ни у какого)
static Dealer_Nearby(playerid, Float:range = DEALER_RANGE)
{
	for (new d = 0; d < sizeof (gDealer); d++)
	{
		if (Marker_InRange(playerid, gDealerMk[d], range)) return d;
	}
	return -1;
}

stock Vehicles_IsNearDealer(playerid)
{
	return Dealer_Nearby(playerid) != -1;
}

// Цена модели в салоне (для налога и продажи; DEALER_NO_PRICE, если модели нет ни в одном)
stock Vehicles_GetModelPrice(model)
{
	for (new i = 0; i < sizeof (gDealerCars); i++)
	{
		if (gDealerCars[i][1] == model) return gDealerCars[i][2];
	}
	return DEALER_NO_PRICE;
}

static bool:Dealer_Sells(model)
{
	for (new i = 0; i < sizeof (gDealerCars); i++)
	{
		if (gDealerCars[i][1] == model) return true;
	}
	return false;
}

// Лимит транспорта исчерпан: сообщение и true
static bool:Dealer_LimitHit(playerid)
{
	new have = Vehicles_CountOwned(playerid), lim = Props_MaxVehicles(playerid);
	if (have < lim) return false;
	SendClientMessage(playerid, COLOR_GREY, "У тебя уже %d ед. транспорта — это твой лимит. Продай что-нибудь (/sellcar) или повысь VIP. Список: /cars", have);
	return true;
}

// ---------------------------------------------------------------------
//  Покупка
// ---------------------------------------------------------------------
stock bool:Cmd_BuyCar(playerid)
{
	if (Debts_BlockBuy(playerid)) return true;      // большой долг штату (debts.inc)
	new d = Dealer_Nearby(playerid);
	if (d == -1) return SendClientMessage(playerid, COLOR_GREY, "Подойди к значку доллара в автосалоне (/gps > Транспорт). В «Экономе» он в зале у стойки менеджера.");
	if (Dealer_LimitHit(playerid)) return true;

	new list[1600] = "Модель\tЦена\n";
	new line[64], name[32];
	for (new i = 0; i < sizeof (gDealerCars); i++)
	{
		if (gDealerCars[i][0] != d) continue;
		Hud_GetVehicleName(gDealerCars[i][1], name);
		format(line, sizeof line, "%s\t{33AA33}$%d\n", name, gDealerCars[i][2]);
		strcat(list, line);
	}
	gDealerAt[playerid] = d;
	ShowPlayerDialog(playerid, DIALOG_BUYCAR, DIALOG_STYLE_TABLIST_HEADERS, gDealer[d][dlName], "%s", "Выбрать", "Отмена", list);
	return true;
}

// Строка gDealerCars по номеру в списке салона d (-1 — нет такой)
static Dealer_ItemByList(d, listitem)
{
	new n = 0;
	for (new i = 0; i < sizeof (gDealerCars); i++)
	{
		if (gDealerCars[i][0] != d) continue;
		if (n == listitem) return i;
		n++;
	}
	return -1;
}

static Dealer_Buy(playerid)
{
	new d = Dealer_Nearby(playerid), i = gDealerPick[playerid];
	gDealerPick[playerid] = -1;
	if (d == -1 || i < 0 || i >= sizeof (gDealerCars) || gDealerCars[i][0] != d) return;
	if (Debts_BlockBuy(playerid) || Dealer_LimitHit(playerid)) return;

	new model = gDealerCars[i][1], price = gDealerCars[i][2];
	if (GetMoney(playerid) < price)
	{
		SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", price);
		return;
	}

	// прежняя машина уходит в гараж, новая становится текущей
	new prev = gPlayerVehDb[playerid];
	Vehicles_UnloadForPlayer(playerid);
	new c1 = random(126), c2 = random(126);
	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"INSERT INTO vehicles (owner, model, x, y, z, a, c1, c2) VALUES (%d, %d, %f, %f, %f, %f, %d, %d)",
		Player[playerid][pDbId], model, gDealer[d][dlSX], gDealer[d][dlSY], gDealer[d][dlSZ], gDealer[d][dlSA], c1, c2));
	new dbid = DB_LastInsertId();
	new v = (dbid > 0) ? Vehicles_SpawnDb(playerid, dbid) : INVALID_VEHICLE_ID;
	if (v == INVALID_VEHICLE_ID)
	{
		if (dbid > 0) DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM vehicles WHERE id = %d AND owner = %d", dbid, Player[playerid][pDbId]));
		if (prev != 0) Vehicles_SpawnDb(playerid, prev);
		SendClientMessage(playerid, COLOR_RED, "Не удалось выдать транспорт (на сервере слишком много машин). Деньги не списаны.");
		return;
	}
	Vehicles_SetActive(playerid, dbid);
	GiveMoney(playerid, -price, "покупка транспорта");
	Account_Save(playerid);

	new name[32];
	Hud_GetVehicleName(model, name);
	PutPlayerInVehicle(playerid, v, 0);
	SendClientMessage(playerid, COLOR_GREEN, "Ты купил %s за $%d! Припаркуй в удобном месте: /park", name, price);
	SendClientMessage(playerid, COLOR_WHITE, "Транспорта у тебя %d из %d. Весь список и смена машины: /cars", Vehicles_CountOwned(playerid), Props_MaxVehicles(playerid));
}

// ---------------------------------------------------------------------
//  Продажа текущей машины
// ---------------------------------------------------------------------
stock bool:Cmd_SellCar(playerid)
{
	new v = gPlayerVeh[playerid], dbid = gPlayerVehDb[playerid];
	if (v == INVALID_VEHICLE_ID || dbid == 0)
		return SendClientMessage(playerid, COLOR_GREY, "Продаётся машина, которая сейчас на улице. Вызвать нужную: /cars");
	if (!Vehicles_IsNearDealer(playerid))
		return SendClientMessage(playerid, COLOR_GREY, "Продать транспорт можно в любом автосалоне.");

	new model = GetVehicleModel(v), refund = DEALER_SELL_MIN, name[32];
	if (Dealer_Sells(model)) refund = Vehicles_GetModelPrice(model) / 100 * DEALER_REFUND_PCT;
	Hud_GetVehicleName(model, name);
	Vehicles_UnloadForPlayer(playerid);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM vehicles WHERE id = %d AND owner = %d", dbid, Player[playerid][pDbId]));
	GiveMoney(playerid, refund, "продажа транспорта");
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "%s продан за $%d.", name, refund);
	if (Vehicles_CountOwned(playerid) > 0) SendClientMessage(playerid, COLOR_WHITE, "Вызвать другую машину из гаража: /cars");
	return true;
}

// ---------------------------------------------------------------------
//  /cars — мой транспорт
// ---------------------------------------------------------------------
stock bool:Cmd_Cars(playerid)
{
	new list[1024] = "Транспорт\tГде\n";
	new line[80], name[32], n = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, model, impound FROM vehicles WHERE owner = %d ORDER BY id LIMIT %d", Player[playerid][pDbId], CARS_MENU_MAX);
	if (r != DBResult:0)
	{
		for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
		{
			new dbid = DB_GetFieldInt(r, 0);
			Hud_GetVehicleName(DB_GetFieldInt(r, 1), name);
			if (DB_GetFieldInt(r, 2) > 0) format(line, sizeof line, "%s\t{FF6347}штрафстоянка\n", name);
			else if (dbid == gPlayerVehDb[playerid]) format(line, sizeof line, "%s\t{33AA33}на улице (GPS)\n", name);
			else format(line, sizeof line, "%s\t{AFAFAF}в гараже (вызвать)\n", name);
			strcat(list, line);
			gCarsMenu[playerid][n++] = dbid;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	gCarsMenuCount[playerid] = n;
	if (n == 0) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет транспорта. Купить: автосалон (/gps > Транспорт). Твой лимит: %d.", Props_MaxVehicles(playerid));

	new title[48];
	format(title, sizeof title, "Мой транспорт (%d из %d)", n, Props_MaxVehicles(playerid));
	ShowPlayerDialog(playerid, DIALOG_CARS, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Выбрать", "Закрыть", list);
	return true;
}

static Cars_Pick(playerid, listitem)
{
	if (listitem < 0 || listitem >= gCarsMenuCount[playerid]) return;
	new dbid = gCarsMenu[playerid][listitem];
	new Float:x, Float:y, Float:z;

	// текущая — показать на карте
	if (dbid == gPlayerVehDb[playerid] && gPlayerVeh[playerid] != INVALID_VEHICLE_ID)
	{
		GetVehiclePos(gPlayerVeh[playerid], x, y, z);
		Gps_SetPoint(playerid, x, y, z, "Мой транспорт");
		return;
	}

	new impound = -1;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT impound, x, y, z FROM vehicles WHERE id = %d AND owner = %d", dbid, Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			impound = DB_GetFieldInt(r, 0);
			x = DB_GetFieldFloat(r, 1);
			y = DB_GetFieldFloat(r, 2);
			z = DB_GetFieldFloat(r, 3);
		}
		DB_FreeResultSet(r);
	}
	if (impound == -1) return;      // уже продана
	if (impound > 0)
	{
		SendClientMessage(playerid, COLOR_ORANGE, "Эта машина на штрафстоянке у LSPD (штраф $%d). Забрать: ALT на штрафстоянке.", impound);
		return;
	}
	if (IsPlayerInAnyVehicle(playerid))
	{
		SendClientMessage(playerid, COLOR_GREY, "Сначала выйди из машины.");
		return;
	}
	new wait = gCarsSummonAt[playerid] + CARS_SUMMON_DELAY - gettime();
	if (wait > 0)
	{
		SendClientMessage(playerid, COLOR_GREY, "Менять машину можно раз в минуту. Подожди %d сек.", wait);
		return;
	}
	new cur = gPlayerVeh[playerid];
	if (cur != INVALID_VEHICLE_ID)
	{
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (IsPlayerConnected(i) && GetPlayerVehicleID(i) == cur)
			{
				SendClientMessage(playerid, COLOR_GREY, "В твоей текущей машине кто-то сидит — в гараж её сейчас не убрать.");
				return;
			}
		}
	}

	new prev = gPlayerVehDb[playerid];
	if (Vehicles_Summon(playerid, dbid) == INVALID_VEHICLE_ID)
	{
		if (prev != 0) Vehicles_SpawnDb(playerid, prev);
		SendClientMessage(playerid, COLOR_RED, "Не удалось выдать транспорт (на сервере слишком много машин).");
		return;
	}
	gCarsSummonAt[playerid] = gettime();
	SendClientMessage(playerid, COLOR_GREEN, "Транспорт стоит на своей парковке, прежний убран в гараж.");
	Gps_SetPoint(playerid, x, y, z, "Мой транспорт");
}

// ---------------------------------------------------------------------
//  Окна (из OnDialogResponse)
// ---------------------------------------------------------------------
stock Vehicles_OnDialog(playerid, dialogid, response, listitem)
{
	switch (dialogid)
	{
		case DIALOG_BUYCAR:
		{
			if (!response) return 1;
			new d = Dealer_Nearby(playerid);
			if (d == -1 || d != gDealerAt[playerid]) return 1;
			new i = Dealer_ItemByList(d, listitem);
			if (i == -1) return 1;
			gDealerPick[playerid] = i;
			new name[32];
			Hud_GetVehicleName(gDealerCars[i][1], name);
			ShowPlayerDialog(playerid, DIALOG_BUYCAR_OK, DIALOG_STYLE_MSGBOX, gDealer[d][dlName],
				"{FFFFFF}Купить {FFFF00}%s{FFFFFF} за {33AA33}$%d{FFFFFF}?\n\nУ тебя $%d. При продаже салон вернёт %d%% цены.\nТранспорта у тебя сейчас: %d из %d.",
				"Купить", "Назад", name, gDealerCars[i][2], GetMoney(playerid), DEALER_REFUND_PCT,
				Vehicles_CountOwned(playerid), Props_MaxVehicles(playerid));
			return 1;
		}
		case DIALOG_BUYCAR_OK:
		{
			if (!response) { gDealerPick[playerid] = -1; Cmd_BuyCar(playerid); return 1; }
			Dealer_Buy(playerid);
			return 1;
		}
		case DIALOG_CARS:
		{
			if (response) Cars_Pick(playerid, listitem);
			return 1;
		}
	}
	return 0;
}

// ---------------------------------------------------------------------
//  /dealerspawn — где салон выдаёт купленный транспорт (главный админ)
// ---------------------------------------------------------------------
stock bool:Cmd_DealerSpawn(playerid)
{
	if (!Admin_Check(playerid, ADMIN_MAX_LEVEL)) return true;
	// салон ищем по его нынешнему месту выдачи: значок может быть внутри здания
	new d = -1;
	if (GetPlayerVirtualWorld(playerid) == 0 && GetPlayerInterior(playerid) == 0)
	{
		for (new i = 0; i < sizeof (gDealer); i++)
		{
			if (IsPlayerInRangeOfPoint(playerid, 150.0, gDealer[i][dlSX], gDealer[i][dlSY], gDealer[i][dlSZ])) { d = i; break; }
		}
	}
	if (d == -1) return SendClientMessage(playerid, COLOR_GREY, "Встань на улице там, где салон должен выдавать транспорт (не дальше 150 м от нынешнего места выдачи).");

	new Float:x, Float:y, Float:z, Float:a, v = GetPlayerVehicleID(playerid);
	if (v != 0)
	{
		GetVehiclePos(v, x, y, z);
		GetVehicleZAngle(v, a);
	}
	else
	{
		GetPlayerPos(playerid, x, y, z);
		GetPlayerFacingAngle(playerid, a);
		z += 0.5;
	}
	gDealer[d][dlSX] = x;
	gDealer[d][dlSY] = y;
	gDealer[d][dlSZ] = z;
	gDealer[d][dlSA] = a;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO dealer_spawns (id, x, y, z, a) VALUES (%d, %f, %f, %f, %f)", d, x, y, z, a));
	SendClientMessage(playerid, COLOR_GREEN, "%s: купленный транспорт теперь появляется здесь.", gDealer[d][dlName]);
	return true;
}
