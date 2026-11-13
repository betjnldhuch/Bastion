// =====================================================================
//  worklife.inc — новые работы: рыбак, фермер, инкассатор, мусорщик.
//
//  Устроиться — как везде: значок работы (оранжевая футболка), ALT или /getjob.
//  Значки работ двигаются в /markers, точки заданий и машины админ ставит сам (places.inc):
//    рыбак      — /place fish (места на пирсе). Смена: /work у значка, у точки /fish.
//                 Улов в сумке, сдать — ALT у значка работы.
//    фермер     — /place field (грядки), /place barn (амбар). Смена: /work у значка,
//                 собрать урожай на грядке, отнести ящик в амбар.
//    инкассатор — банкоматы (/addatm) и /place vault (хранилище). Машина: /place car collector
//                 (сесть в Securicar). Смена: /work в машине, объехать банкоматы (у каждого
//                 выйти и забрать кассету), отвезти деньги в хранилище.
//                 Пока инкассатор вне машины, её можно ограбить: /robvan у задних дверей.
//    мусорщик   — /place bin (баки), /place dump (свалка), машина: /place car garbage (Trashmaster).
//                 Смена: /work в мусоровозе, объехать баки, разгрузиться на свалке.
// =====================================================================

#define FISH_PRICE              45      // за одну рыбу (с надбавкой за стаж)
#define FISH_TIME               9       // секунд на заброс
#define FISH_SPOT_RANGE         4.0
#define FARM_PAY                90      // за ящик в амбар
#define FARM_HARVEST_SEC        5
#define COLLECT_ATMS            5       // банкоматов за рейс
#define COLLECT_CASH_ATM        2500    // денег в машине с одного банкомата (это добыча грабителя)
#define COLLECT_PAY_ATM         140     // инкассатору за банкомат, получает в хранилище
#define COLLECT_PAY_BONUS       200
#define ROBVAN_TIME             25      // секунд вскрывать машину
#define ROBVAN_SHARE            60      // % денег из машины достаётся грабителю
#define ROBVAN_WANTED           4
#define GARBAGE_BINS            6
#define GARBAGE_PAY_BIN         70
#define GARBAGE_PAY_DUMP        250
#define WORKLIFE_HQ_RANGE       40.0    // как далеко от значка можно начать пешую смену

// Шаги смены (pWorkStep)
enum
{
	WL_NONE = 100,
	WL_FISH_GO,         // идёт к месту рыбалки
	WL_FARM_FIELD,      // идёт к грядке
	WL_FARM_BARN,       // несёт ящик в амбар
	WL_COL_ATM,         // едет к банкомату (забрать кассету — пешком)
	WL_COL_VAULT,       // везёт деньги в хранилище
	WL_GARB_BIN,        // едет к баку
	WL_GARB_DUMP        // едет на свалку
}

static const Float:gWlHQ[4][3] =
{
	{ 836.0, -1999.0, 12.87 },          // рыбак: начало пирса Санта-Марии
	{ -1065.0, -1190.0, 129.2 },        // фермер: ферма EasterBoard
	{ 1457.0, -1010.0, 26.84 },         // инкассатор: банк в Market
	{ 2195.0, -1975.0, 13.55 }          // мусорщик: Willowfield
};
static gWlMk[4] = { -1, ... };
static gPlaceVeh[MAX_PLACES] = { INVALID_VEHICLE_ID, ... };
static gVanCash[MAX_VEHICLES];

static gWlTarget[MAX_PLAYERS];          // точка задания (places / банкомат)
static gWlCount[MAX_PLAYERS];           // сколько сделано в рейсе
static gWlBusy[MAX_PLAYERS];            // секунд до конца действия (рыбалка, сбор)
static gRobVan[MAX_PLAYERS] = { INVALID_VEHICLE_ID, ... };
static gRobVanLeft[MAX_PLAYERS];

// ---------------------------------------------------------------------
stock WorkLife_JobByKey(const key[])
{
	if (!strcmp(key, "fisher", true)) return JOB_FISHER;
	if (!strcmp(key, "farmer", true)) return JOB_FARMER;
	if (!strcmp(key, "collector", true)) return JOB_COLLECTOR;
	if (!strcmp(key, "garbage", true)) return JOB_GARBAGE;
	return JOB_NONE;
}

stock WorkLife_JobKey(job)
{
	new s[12];
	switch (job)
	{
		case JOB_FISHER: strcat(s, "fisher");
		case JOB_FARMER: strcat(s, "farmer");
		case JOB_COLLECTOR: strcat(s, "collector");
		case JOB_GARBAGE: strcat(s, "garbage");
		default: strcat(s, "?");
	}
	return s;
}

stock WorkLife_JobName(job)
{
	new s[32];
	switch (job)
	{
		case JOB_FISHER: strcat(s, "Рыбак");
		case JOB_FARMER: strcat(s, "Фермер");
		case JOB_COLLECTOR: strcat(s, "Инкассатор");
		case JOB_GARBAGE: strcat(s, "Мусорщик");
		default: strcat(s, "Безработный");
	}
	return s;
}

// Нужны ли права на авто
stock bool:WorkLife_NeedsLicense(job)
{
	return (job == JOB_COLLECTOR || job == JOB_GARBAGE);
}

static WorkLife_AddHQ(job, const hint[])
{
	new k = job - JOB_FISHER;
	new Float:x = gWlHQ[k][0], Float:y = gWlHQ[k][1], Float:z = gWlHQ[k][2];
	new text[MK_TEXT_LEN], name[32];
	format(name, sizeof name, "Работа: %s", WorkLife_JobName(job));
	format(text, sizeof text, "{FF9900}Работа: %s\n{FFFFFF}ALT — устроиться\n%s", WorkLife_JobName(job), hint);
	gWlMk[k] = Marker_Create(MK_JOB, job, name, 1275, x, y, z, 0, 0, GPS_JOBS);
	Marker_SetLabel(gWlMk[k], text);
}

stock WorkLife_Init()
{
	WorkLife_AddHQ(JOB_FISHER, "/work — смена, /fish — закинуть удочку\nALT — сдать улов");
	WorkLife_AddHQ(JOB_FARMER, "/work — начать смену");
	WorkLife_AddHQ(JOB_COLLECTOR, "Сядь в инкассаторскую машину и введи /work");
	WorkLife_AddHQ(JOB_GARBAGE, "Сядь в мусоровоз и введи /work");
	for (new i = 0; i < MAX_PLACES; i++)
		if (gPlace[i][plUsed] && gPlace[i][plKind] == PL_CAR) WorkLife_SpawnCar(i);
	SetTimer("WorkLife_OnSecond", 1000, true);
}

stock WorkLife_GetNearbyHQ(playerid)
{
	for (new k = 0; k < 4; k++)
		if (Marker_InRange(playerid, gWlMk[k], 3.0)) return JOB_FISHER + k;
	return JOB_NONE;
}

// ---------------------------------------------------------------------
//  Машины работ (точки PL_CAR: arg = работа * 1000 + модель)
// ---------------------------------------------------------------------
stock WorkLife_SpawnCar(i)
{
	WorkLife_RemoveCar(i);
	new job = gPlace[i][plArg] / 1000, model = gPlace[i][plArg] % 1000;
	if (model < 400 || model > 611) return;
	new v = AddStaticVehicleEx(model, gPlace[i][plX], gPlace[i][plY], gPlace[i][plZ], gPlace[i][plA], 1, 1, 300);
	if (v == INVALID_VEHICLE_ID || v >= MAX_VEHICLES) return;
	Jobs_SetVehicleJob(v, job);
	gPlaceVeh[i] = v;
	gVanCash[v] = 0;
}

stock WorkLife_RemoveCar(i)
{
	if (gPlaceVeh[i] == INVALID_VEHICLE_ID) return;
	Jobs_SetVehicleJob(gPlaceVeh[i], JOB_NONE);
	DestroyVehicle(gPlaceVeh[i]);
	gPlaceVeh[i] = INVALID_VEHICLE_ID;
}

stock WorkLife_PlaceOfCar(v)
{
	for (new i = 0; i < MAX_PLACES; i++) if (gPlaceVeh[i] == v && gPlace[i][plUsed]) return i;
	return -1;
}

// Машина вернулась на место — денег в ней больше нет
stock WorkLife_OnVehicleSpawn(v)
{
	if (v > 0 && v < MAX_VEHICLES) gVanCash[v] = 0;
}

// ---------------------------------------------------------------------
//  Начало смены (из Cmd_Work, jobs.inc). true — работа наша
// ---------------------------------------------------------------------
static WorkLife_Target(playerid, i, Float:size)
{
	gWlTarget[playerid] = i;
	DisablePlayerCheckpoint(playerid);
	Jobs_Checkpoint(playerid, gPlace[i][plX], gPlace[i][plY], gPlace[i][plZ], size);
}

static WorkLife_NextAtm(playerid)
{
	if (gAtmCount <= 0) return 0;
	new a, tries = 0;
	do { a = random(gAtmCount); tries++; } while (a == gWlTarget[playerid] && gAtmCount > 1 && tries < 10);
	gWlTarget[playerid] = a;
	DisablePlayerCheckpoint(playerid);
	Jobs_Checkpoint(playerid, Atm[a][aX], Atm[a][aY], Atm[a][aZ], 3.0);
	return 1;
}

stock bool:WorkLife_StartWork(playerid)
{
	new job = Player[playerid][pJob];
	if (job < JOB_FISHER || job > JOB_GARBAGE) return false;
	new k = job - JOB_FISHER;
	switch (job)
	{
		case JOB_FISHER, JOB_FARMER:
		{
			if (!Marker_InRange(playerid, gWlMk[k], WORKLIFE_HQ_RANGE)) { SendClientMessage(playerid, COLOR_GREY, "Смену начинают у значка работы."); return true; }
			if (IsPlayerInAnyVehicle(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Выйди из транспорта."); return true; }
			new kind = (job == JOB_FISHER) ? PL_FISH : PL_FIELD;
			new i = Place_Random(kind);
			if (i == -1 || (job == JOB_FARMER && Place_First(PL_BARN) == -1))
			{
				SendClientMessage(playerid, COLOR_GREY, "Точки работы не расставлены. Попроси админа: /place %s%s", (job == JOB_FISHER) ? "fish" : "field", (job == JOB_FARMER) ? " и /place barn" : "");
				return true;
			}
			Player[playerid][pWorking] = 1;
			gWlCount[playerid] = 0;
			gWlBusy[playerid] = 0;
			if (job == JOB_FISHER)
			{
				Player[playerid][pWorkStep] = WL_FISH_GO;
				SetPlayerAttachedObject(playerid, JOB_OBJECT_SLOT, 18632, 6, 0.08, 0.02, 0.0, 180.0, 0.0, 0.0);
				WorkLife_Target(playerid, i, 2.0);
				SendClientMessage(playerid, COLOR_GREEN, "Смена рыбака началась. Иди к месту (красный маркер) и введи /fish. Улов сдают у значка работы (ALT).");
			}
			else
			{
				Player[playerid][pWorkStep] = WL_FARM_FIELD;
				WorkLife_Target(playerid, i, 2.0);
				SendClientMessage(playerid, COLOR_GREEN, "Смена фермера началась. Иди к грядке (красный маркер), собери урожай и отнеси в амбар. Закончить: /work");
			}
		}
		case JOB_COLLECTOR, JOB_GARBAGE:
		{
			if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || Jobs_GetVehicleJob(GetPlayerVehicleID(playerid)) != job)
			{
				SendClientMessage(playerid, COLOR_GREY, "Сядь за руль рабочей машины. Если машин нет — попроси админа: /place car %s", WorkLife_JobKey(job));
				return true;
			}
			if (!Licenses_Has(playerid, LIC_CAR)) { SendClientMessage(playerid, COLOR_GREY, "Нужны права на авто."); return true; }
			if (job == JOB_COLLECTOR && (gAtmCount <= 0 || Place_First(PL_VAULT) == -1))
			{
				SendClientMessage(playerid, COLOR_GREY, "Нет банкоматов или хранилища. Попроси админа: /addatm и /place vault");
				return true;
			}
			if (job == JOB_GARBAGE && (Place_First(PL_BIN) == -1 || Place_First(PL_DUMP) == -1))
			{
				SendClientMessage(playerid, COLOR_GREY, "Баки или свалка не расставлены. Попроси админа: /place bin и /place dump");
				return true;
			}
			Player[playerid][pWorking] = 1;
			Player[playerid][pWorkVehicle] = GetPlayerVehicleID(playerid);
			gWlCount[playerid] = 0;
			gWlTarget[playerid] = -1;
			if (job == JOB_COLLECTOR)
			{
				gVanCash[Player[playerid][pWorkVehicle]] = 0;
				Player[playerid][pWorkStep] = WL_COL_ATM;
				WorkLife_NextAtm(playerid);
				SendClientMessage(playerid, COLOR_GREEN, "Рейс инкассатора: %d банкоматов. У каждого выйди и подойди к банкомату за кассетой. Не оставляй машину надолго: её могут вскрыть.", COLLECT_ATMS);
			}
			else
			{
				Player[playerid][pWorkStep] = WL_GARB_BIN;
				WorkLife_Target(playerid, Place_Random(PL_BIN), 5.0);
				SendClientMessage(playerid, COLOR_GREEN, "Рейс мусорщика: %d баков, потом свалка. $%d за бак. Закончить: /work", GARBAGE_BINS, GARBAGE_PAY_BIN);
			}
		}
	}
	return true;
}

// ---------------------------------------------------------------------
//  Точки заданий (из Jobs_OnEnterCheckpoint)
// ---------------------------------------------------------------------
stock WorkLife_OnCheckpoint(playerid)
{
	new step = Player[playerid][pWorkStep];
	switch (step)
	{
		case WL_FISH_GO:
		{
			DisablePlayerCheckpoint(playerid);
			SendClientMessage(playerid, COLOR_WHITE, "Ты на месте. Закинуть удочку: /fish");
		}
		case WL_FARM_FIELD:
		{
			if (IsPlayerInAnyVehicle(playerid) || gWlBusy[playerid] > 0) return;
			DisablePlayerCheckpoint(playerid);
			gWlBusy[playerid] = FARM_HARVEST_SEC;
			TogglePlayerControllable(playerid, false);
			ApplyAnimation(playerid, "BOMBER", "BOM_Plant_Loop", 4.1, true, false, false, false, 0);
			GameTextForPlayer(playerid, "~g~harvest...", FARM_HARVEST_SEC * 1000, 4);
		}
		case WL_FARM_BARN:
		{
			if (IsPlayerInAnyVehicle(playerid)) return;
			SetPlayerSpecialAction(playerid, SPECIAL_ACTION_NONE);
			RemovePlayerAttachedObject(playerid, JOB_OBJECT_SLOT);
			new pay = Job_Pay(playerid, FARM_PAY);
			gWlCount[playerid]++;
			Player[playerid][pWorkStep] = WL_FARM_FIELD;
			WorkLife_Target(playerid, Place_Random(PL_FIELD, gWlTarget[playerid]), 2.0);
			SendClientMessage(playerid, COLOR_GREEN, "Урожай сдан: +$%d. Следующая грядка отмечена.", pay);
		}
		case WL_COL_ATM:
		{
			if (IsPlayerInAnyVehicle(playerid))
			{
				SendClientMessage(playerid, COLOR_GREY, "Выйди из машины и подойди к банкомату за кассетой.");
				return;
			}
			new v = Player[playerid][pWorkVehicle];
			new Float:vx, Float:vy, Float:vz;
			GetVehiclePos(v, vx, vy, vz);
			if (!IsPlayerInRangeOfPoint(playerid, 40.0, vx, vy, vz)) { SendClientMessage(playerid, COLOR_GREY, "Твоя машина слишком далеко."); return; }
			ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, false, false, false, false, 0);
			gVanCash[v] += COLLECT_CASH_ATM;
			gWlCount[playerid]++;
			if (gWlCount[playerid] >= COLLECT_ATMS)
			{
				Player[playerid][pWorkStep] = WL_COL_VAULT;
				WorkLife_Target(playerid, Place_First(PL_VAULT), 6.0);
				SendClientMessage(playerid, COLOR_GREEN, "Кассета загружена (%d/%d). Вези деньги в хранилище банка (маркер).", gWlCount[playerid], COLLECT_ATMS);
			}
			else
			{
				WorkLife_NextAtm(playerid);
				SendClientMessage(playerid, COLOR_GREEN, "Кассета загружена (%d/%d). В машине $%d. Следующий банкомат отмечен.", gWlCount[playerid], COLLECT_ATMS, gVanCash[v]);
			}
		}
		case WL_COL_VAULT:
		{
			if (GetPlayerVehicleID(playerid) != Player[playerid][pWorkVehicle]) { SendClientMessage(playerid, COLOR_GREY, "Приезжай на своей инкассаторской машине."); return; }
			new v = Player[playerid][pWorkVehicle];
			if (gVanCash[v] <= 0)
			{
				SendClientMessage(playerid, COLOR_RED, "Машина пустая: деньги украли. Рейс не оплачен.");
			}
			else
			{
				new pay = Job_Pay(playerid, COLLECT_PAY_ATM * gWlCount[playerid] + COLLECT_PAY_BONUS);
				SendClientMessage(playerid, COLOR_GREEN, "Деньги сданы в хранилище ($%d). Оплата рейса: +$%d.", gVanCash[v], pay);
			}
			gVanCash[v] = 0;
			gWlCount[playerid] = 0;
			Player[playerid][pWorkStep] = WL_COL_ATM;
			WorkLife_NextAtm(playerid);
			SendClientMessage(playerid, COLOR_WHITE, "Новый рейс: первый банкомат отмечен. Закончить: /work");
		}
		case WL_GARB_BIN:
		{
			if (GetPlayerVehicleID(playerid) != Player[playerid][pWorkVehicle]) { SendClientMessage(playerid, COLOR_GREY, "Приезжай на мусоровозе."); return; }
			new pay = Job_Pay(playerid, GARBAGE_PAY_BIN);
			gWlCount[playerid]++;
			if (gWlCount[playerid] >= GARBAGE_BINS)
			{
				Player[playerid][pWorkStep] = WL_GARB_DUMP;
				WorkLife_Target(playerid, Place_Random(PL_DUMP), 8.0);
				SendClientMessage(playerid, COLOR_GREEN, "Бак пуст: +$%d. Кузов полный, вези на свалку.", pay);
			}
			else
			{
				WorkLife_Target(playerid, Place_Random(PL_BIN, gWlTarget[playerid]), 5.0);
				SendClientMessage(playerid, COLOR_GREEN, "Бак пуст: +$%d (%d/%d). Следующий отмечен.", pay, gWlCount[playerid], GARBAGE_BINS);
			}
		}
		case WL_GARB_DUMP:
		{
			if (GetPlayerVehicleID(playerid) != Player[playerid][pWorkVehicle]) { SendClientMessage(playerid, COLOR_GREY, "Приезжай на мусоровозе."); return; }
			new pay = Job_Pay(playerid, GARBAGE_PAY_DUMP);
			gWlCount[playerid] = 0;
			Player[playerid][pWorkStep] = WL_GARB_BIN;
			WorkLife_Target(playerid, Place_Random(PL_BIN), 5.0);
			SendClientMessage(playerid, COLOR_GREEN, "Мусор выгружен: +$%d. Новый рейс, первый бак отмечен.", pay);
		}
	}
}

// ---------------------------------------------------------------------
//  Рыбалка: /fish
// ---------------------------------------------------------------------
stock bool:Cmd_Fish(playerid)
{
	if (Player[playerid][pJob] != JOB_FISHER || !Player[playerid][pWorking]) return SendClientMessage(playerid, COLOR_GREY, "Рыбачат на смене рыбака (/getjob у значка, потом /work).");
	if (gWlBusy[playerid] > 0) return SendClientMessage(playerid, COLOR_GREY, "Удочка уже заброшена.");
	if (IsPlayerInAnyVehicle(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Выйди из транспорта.");
	if (Place_Nearby(playerid, PL_FISH, FISH_SPOT_RANGE) == -1) return SendClientMessage(playerid, COLOR_GREY, "Здесь не клюёт. Иди к месту рыбалки (маркер на карте).");
	if (Inv_Count(playerid, INV_FISH) >= Inv_GetMax(INV_FISH)) return SendClientMessage(playerid, COLOR_GREY, "Сумка полная: сдай улов у значка работы.");
	DisablePlayerCheckpoint(playerid);
	gWlBusy[playerid] = FISH_TIME + random(5);
	TogglePlayerControllable(playerid, false);
	ApplyAnimation(playerid, "SWORD", "sword_block", 4.1, false, false, false, true, 0);
	GameTextForPlayer(playerid, "~b~fishing...", gWlBusy[playerid] * 1000, 4);
	new msg[96];
	format(msg, sizeof msg, "%s закидывает удочку.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

static WorkLife_FishDone(playerid)
{
	TogglePlayerControllable(playerid, true);
	ClearAnimations(playerid);
	new roll = random(100);
	if (roll < 25) { SendClientMessage(playerid, COLOR_GREY, "Сорвалась... Попробуй ещё: /fish"); return; }
	if (roll < 35) { SendClientMessage(playerid, COLOR_GREY, "Вытащил старый ботинок. Выбросил. /fish"); return; }
	new n = (roll >= 90) ? 2 : 1;
	if (Inv_GetMax(INV_FISH) - Inv_Count(playerid, INV_FISH) < n) n = 1;
	Inv_Add(playerid, INV_FISH, n);
	SendClientMessage(playerid, COLOR_GREEN, "Поймал%s! Рыбы в сумке: %d. Ещё: /fish, сдать — ALT у значка работы.", (n == 2) ? " сразу две" : "", Inv_Count(playerid, INV_FISH));
	// каждые 5 рыб место меняется — рыба уходит
	if (++gWlCount[playerid] % 5 == 0)
	{
		Player[playerid][pWorkStep] = WL_FISH_GO;
		WorkLife_Target(playerid, Place_Random(PL_FISH, Place_Nearby(playerid, PL_FISH, FISH_SPOT_RANGE)), 2.0);
		SendClientMessage(playerid, COLOR_WHITE, "Здесь клевать перестало. Новое место отмечено на карте.");
	}
}

// ALT у значка своей работы (из Cmd_GetJob). true — обработано
stock bool:WorkLife_OnHQAlt(playerid, job)
{
	if (job != JOB_FISHER) return false;
	new n = Inv_Count(playerid, INV_FISH);
	if (n <= 0) { SendClientMessage(playerid, COLOR_GREY, "Рыбы в сумке нет. Смена: /work, у места рыбалки — /fish."); return true; }
	Inv_Remove(playerid, INV_FISH, n);
	new pay = Job_Pay(playerid, FISH_PRICE * n);
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Улов сдан: %d рыб, +$%d.", n, pay);
	return true;
}

// ---------------------------------------------------------------------
//  Ограбление инкассаторской машины: /robvan
// ---------------------------------------------------------------------
static WorkLife_VanDriver(v)
{
	for (new i = 0; i < MAX_PLAYERS; i++)
		if (IsPlayerConnected(i) && Player[i][pWorking] && Player[i][pJob] == JOB_COLLECTOR && Player[i][pWorkVehicle] == v) return i;
	return INVALID_PLAYER_ID;
}

stock bool:Cmd_RobVan(playerid)
{
	if (gRobVan[playerid] != INVALID_VEHICLE_ID) return SendClientMessage(playerid, COLOR_GREY, "Ты уже вскрываешь машину.");
	if (IsPlayerInAnyVehicle(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Выйди из машины.");
	if (Player[playerid][pLevel] < ROB_MIN_LEVEL) return SendClientMessage(playerid, COLOR_GREY, "Грабить можно с %d уровня.", ROB_MIN_LEVEL);
	if (Faction_IsLaw(Player[playerid][pFaction]) && Player[playerid][pDuty]) return SendClientMessage(playerid, COLOR_GREY, "Не на службе.");
	new Float:px, Float:py, Float:pz, v = INVALID_VEHICLE_ID;
	GetPlayerPos(playerid, px, py, pz);
	for (new i = 1; i < MAX_VEHICLES; i++)
	{
		if (!IsValidVehicle(i) || gVanCash[i] <= 0) continue;
		if (GetVehicleDistanceFromPoint(i, px, py, pz) < 5.0) { v = i; break; }
	}
	if (v == INVALID_VEHICLE_ID) return SendClientMessage(playerid, COLOR_GREY, "Рядом нет инкассаторской машины с деньгами.");
	for (new i = 0; i < MAX_PLAYERS; i++)
		if (IsPlayerConnected(i) && GetPlayerVehicleID(i) == v) return SendClientMessage(playerid, COLOR_GREY, "В машине кто-то сидит.");
	if (GetPlayerWeapon(playerid) < WEAPON_COLT45 || GetPlayerWeapon(playerid) > WEAPON_SNIPER) return SendClientMessage(playerid, COLOR_GREY, "Нужно оружие в руках.");

	gRobVan[playerid] = v;
	gRobVanLeft[playerid] = ROBVAN_TIME;
	new stars = Player[playerid][pWanted] + ROBVAN_WANTED;
	if (stars > 6) stars = 6;
	Player[playerid][pWanted] = stars;
	SetPlayerWantedLevel(playerid, stars);
	Account_Save(playerid);
	ApplyAnimation(playerid, "BOMBER", "BOM_Plant_Loop", 4.1, true, false, false, false, 0);
	SendClientMessage(playerid, COLOR_RED, "Ты вскрываешь инкассаторскую машину! Держись рядом %d сек. Розыск: %d зв.", ROBVAN_TIME, stars);
	new msg[144];
	format(msg, sizeof msg, "[Диспетчер] Нападение на инкассаторскую машину! Подозреваемый: %s [%d].", GetRPName(playerid), playerid);
	Faction_MessageLaw(msg);
	new d = WorkLife_VanDriver(v);
	if (d != INVALID_PLAYER_ID) SendClientMessage(d, COLOR_RED, "Твою машину вскрывают! Возвращайся.");
	return true;
}

static WorkLife_RobVanTick(playerid)
{
	new v = gRobVan[playerid];
	new Float:x, Float:y, Float:z;
	GetVehiclePos(v, x, y, z);
	if (!IsValidVehicle(v) || gVanCash[v] <= 0 || !IsPlayerInRangeOfPoint(playerid, 7.0, x, y, z) || Wound_IsWounded(playerid) || Player[playerid][pCuffed])
	{
		gRobVan[playerid] = INVALID_VEHICLE_ID;
		ClearAnimations(playerid);
		SendClientMessage(playerid, COLOR_GREY, "Ограбление сорвано.");
		return;
	}
	if (--gRobVanLeft[playerid] > 0) return;
	gRobVan[playerid] = INVALID_VEHICLE_ID;
	ClearAnimations(playerid);
	new loot = gVanCash[v] * ROBVAN_SHARE / 100;
	gVanCash[v] = 0;
	GiveMoney(playerid, loot, "ограбление инкассаторов");
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Ты вскрыл машину и забрал $%d. Уходи, тебя ищут!", loot);
	new msg[144];
	format(msg, sizeof msg, "[Диспетчер] Инкассаторская машина ограблена. Подозреваемый: %s [%d].", GetRPName(playerid), playerid);
	Faction_MessageLaw(msg);
	new d = WorkLife_VanDriver(v);
	if (d != INVALID_PLAYER_ID) SendClientMessage(d, COLOR_RED, "Машину ограбили. Рейс сорван: доедь до хранилища, чтобы начать новый.");
}

// ---------------------------------------------------------------------
forward WorkLife_OnSecond();
public WorkLife_OnSecond()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i)) continue;
		if (gRobVan[i] != INVALID_VEHICLE_ID) WorkLife_RobVanTick(i);
		if (gWlBusy[i] <= 0) continue;
		if (--gWlBusy[i] > 0) continue;
		if (!Player[i][pWorking]) { TogglePlayerControllable(i, true); continue; }
		if (Player[i][pJob] == JOB_FISHER) WorkLife_FishDone(i);
		else if (Player[i][pJob] == JOB_FARMER && Player[i][pWorkStep] == WL_FARM_FIELD)
		{
			TogglePlayerControllable(i, true);
			ClearAnimations(i);
			SetPlayerSpecialAction(i, SPECIAL_ACTION_CARRY);
			SetPlayerAttachedObject(i, JOB_OBJECT_SLOT, 2969, 1, 0.11, 0.36, 0.0, 0.0, 90.0, 0.0, 0.8, 0.8, 0.8);
			Player[i][pWorkStep] = WL_FARM_BARN;
			WorkLife_Target(i, Place_Random(PL_BARN), 2.5);
			SendClientMessage(i, COLOR_WHITE, "Ящик с урожаем в руках. Неси в амбар (маркер).");
		}
	}
}

// Из Job_Stop (jobs.inc)
stock WorkLife_OnStop(playerid)
{
	if (gWlBusy[playerid] > 0 && IsPlayerConnected(playerid)) TogglePlayerControllable(playerid, true);
	gWlBusy[playerid] = 0;
	gWlCount[playerid] = 0;
}

stock WorkLife_OnDisconnect(playerid)
{
	gRobVan[playerid] = INVALID_VEHICLE_ID;
	gWlBusy[playerid] = 0;
}

stock bool:WorkLife_OnCommand(playerid, const cmd[])
{
	if (!strcmp(cmd, "/fish", true))   { Cmd_Fish(playerid); return true; }
	if (!strcmp(cmd, "/robvan", true)) { Cmd_RobVan(playerid); return true; }
	return false;
}
