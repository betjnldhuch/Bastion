// =====================================================================
//  jobs.inc — работы.
//
//  1. Грузчик (порт): бери ящик в одной точке, неси в другую.
//  2. Развозчик пиццы: садись на мопед Pizzaboy, вези заказы по городу.
//  3. Таксист (Unity Station): вызовы по городу + живые пассажиры (/taxi).
//  4. Водитель автобуса (Unity Station): маршрут по остановкам Лос-Сантоса.
//  5. Дальнобойщик (порт): везёт бензин на АЗС по всему штату.
//  6. Механик (Idlewood): чинит и заправляет машины игроков, ездит на вызовы.
//
//  /getjob — устроиться (стоя на значке работы, или ALT)
//  /work   — начать / закончить смену (для 3-6 — сидя за рулём рабочей машины)
//  /quitjob — уволиться
//  /taxi   — вызвать такси (любой игрок)
//  Механику: /repair [id], /refill [id] — предложить ремонт / бензин игроку в машине.
//
//  Стаж: каждое выполненное задание +1. За каждые JOB_SKILL_STEP заданий
//  оплата растёт на 5%, максимум на JOB_SKILL_MAX_BONUS%. При смене работы
//  стаж обнуляется. Хранится в accounts.job_exp.
//
//  Для такси, автобуса, грузовика и эвакуатора нужны права (licenses.inc).
//  Рабочие машины водят только работники этой профессии; бак у них всегда
//  полный при появлении.
// =====================================================================

// --- Грузчик (Ocean Docks) ---
#define LOADER_JOB_X        2756.2
#define LOADER_JOB_Y        -2414.6
#define LOADER_JOB_Z        13.6
#define LOADER_TAKE_X       2786.1      // где брать ящик
#define LOADER_TAKE_Y       -2417.4
#define LOADER_TAKE_Z       13.6
#define LOADER_DROP_X       2766.4      // куда нести
#define LOADER_DROP_Y       -2444.9
#define LOADER_DROP_Z       13.6
#define LOADER_PAY          120

// --- Пиццерия (Idlewood Pizza Stack) ---
#define PIZZA_JOB_X         2105.0
#define PIZZA_JOB_Y         -1806.4
#define PIZZA_JOB_Z         13.55
#define PIZZA_BIKE_MODEL    448         // Pizzaboy
#define PIZZA_PAY_MIN       80
#define PIZZA_PAY_RANDOM    70          // + случайно от 0 до 69

// --- Новые работы. Координаты можно поправить здесь, если точка в игре неудобная ---
// Таксист (Unity Station)
#define TAXI_JOB_X          1742.9
#define TAXI_JOB_Y          -1861.9
#define TAXI_JOB_Z          13.58
#define TAXI_CARS_X         1775.0      // первая машина ряда, следующие через 4 м по X
#define TAXI_CARS_Y         -1860.0
#define TAXI_CARS_Z         13.4
#define TAXI_CARS_A         270.0
#define TAXI_MODEL          420         // Taxi
#define TAXI_FARE           50          // платит живой пассажир при посадке
#define TAXI_CALL_BASE      60          // вызов диспетчера: база...
#define TAXI_PAY_PER_100M   8           // ...плюс за каждые 100 м

// Водитель автобуса (Unity Station)
#define BUS_JOB_X           1752.9
#define BUS_JOB_Y           -1894.2
#define BUS_JOB_Z           13.56
#define BUS_CARS_X          1790.0
#define BUS_CARS_Y          -1912.0
#define BUS_CARS_Z          13.4
#define BUS_CARS_A          0.0
#define BUS_MODEL           431         // Bus
#define BUS_PAY_STOP        60
#define BUS_PAY_ROUTE       200         // премия за полный круг

// Дальнобойщик (Ocean Docks, рядом с грузчиками)
#define TRUCK_JOB_X         2745.0
#define TRUCK_JOB_Y         -2414.6
#define TRUCK_JOB_Z         13.6
#define TRUCK_CARS_X        2700.0
#define TRUCK_CARS_Y        -2400.0
#define TRUCK_CARS_Z        13.6
#define TRUCK_CARS_A        90.0
#define TRUCK_MODEL         456         // Yankee
#define TRUCK_PAY_BASE      150
#define TRUCK_PAY_PER_100M  12          // Лос-Сантос -> Лас-Вентурас примерно $450

// Механик (заправка Idlewood)
#define MECH_JOB_X          1925.0
#define MECH_JOB_Y          -1790.0
#define MECH_JOB_Z          13.4
#define MECH_CARS_X         1915.0
#define MECH_CARS_Y         -1775.0
#define MECH_CARS_Z         13.4
#define MECH_CARS_A         0.0
#define MECH_MODEL          525         // Towtruck
#define MECH_REPAIR_PRICE   300         // платит игрок, получает механик
#define MECH_FUEL_PRICE     150
#define MECH_CALL_PAY       100         // вызов диспетчера

// Стаж
#define JOB_SKILL_STEP      20          // каждые 20 заданий...
#define JOB_MAX_SPEED       70.0        // м/с по прямой до точки задания; быстрее не доехать — значит телепорт
#define JOB_SKILL_MAX_BONUS 50          // ...+5% к оплате, максимум +50%

#define JOB_OBJECT_SLOT     9           // слот прикреплённого объекта (ящик)


// Шаги смены у новых работ (pWorkStep)
enum
{
	WS_NONE,
	WS_TAXI_PICKUP,     // едет за клиентом
	WS_TAXI_DROPOFF,    // везёт клиента
	WS_TAXI_CALLER,     // едет к игроку, вызвавшему такси
	WS_BUS,             // едет к остановке
	WS_TRUCK,           // везёт груз
	WS_MECH_CALL        // едет на вызов
}

// Адреса доставки пиццы
static const Float:gPizzaPoints[][3] =
{
	{ 2495.3, -1688.2, 13.6 },      // Grove Street
	{ 1940.6, -1772.7, 13.4 },      // заправка Idlewood
	{ 2229.9, -1721.5, 13.6 },      // спортзал Ganton
	{ 2400.0, -1980.0, 13.5 },      // Willowfield
	{ 1833.8, -1842.5, 13.6 },
	{ 2163.3, -1671.9, 15.1 },
	{ 1975.0, -2040.0, 13.5 }       // El Corona
};

// Точки города для вызовов такси и механика
static const Float:gCityPoints[][3] =
{
	{ 2495.3, -1688.2, 13.6 },      // Grove Street
	{ 1940.6, -1772.7, 13.4 },      // заправка Idlewood
	{ 2229.9, -1721.5, 13.6 },      // спортзал Ganton
	{ 2400.0, -1980.0, 13.5 },      // Willowfield
	{ 1833.8, -1842.5, 13.6 },      // Idlewood
	{ 1975.0, -2040.0, 13.5 },      // El Corona
	{ 1544.0, -1675.5, 13.6 },      // полиция, Pershing Square
	{ 1178.0, -1323.5, 14.1 },      // больница All Saints
	{ 2220.0, -1160.0, 25.8 },      // мотель Jefferson
	{ 1004.0, -939.3, 42.2 },       // заправка Mulholland
	{ 2131.8, -1150.6, 24.1 },      // автосалон Coutt & Schutz
	{ 2105.0, -1806.4, 13.55 }      // пиццерия Idlewood
};

// Остановки автобуса (по кругу)
static const Float:gBusStops[][3] =
{
	{ 1940.6, -1772.7, 13.4 },      // Idlewood
	{ 2229.9, -1721.5, 13.6 },      // Ganton
	{ 2495.3, -1688.2, 13.6 },      // Grove Street
	{ 2400.0, -1980.0, 13.5 },      // Willowfield
	{ 1975.0, -2040.0, 13.5 },      // El Corona
	{ BUS_JOB_X, BUS_JOB_Y, BUS_JOB_Z }   // Unity Station
};

static gPizzaTarget[MAX_PLAYERS];
static gJobTarget[MAX_PLAYERS];                 // номер точки задания
static Float:gJobFromX[MAX_PLAYERS];            // откуда начался рейс (для оплаты по расстоянию)
static Float:gJobFromY[MAX_PLAYERS];
static gJobExp[MAX_PLAYERS];                    // стаж (accounts.job_exp)
static gJobCpTick[MAX_PLAYERS];                 // когда поставлена текущая точка задания (GetTickCount)
static Float:gJobCpDist[MAX_PLAYERS];           // сколько метров до неё было в тот момент
static gVehJob[MAX_VEHICLES];                   // рабочая машина какой профессии (JOB_*)
static gTaxiPaidBy[MAX_PLAYERS] = { INVALID_PLAYER_ID, ... };   // пассажир уже заплатил этому таксисту
static gTaxiCaller[MAX_PLAYERS] = { INVALID_PLAYER_ID, ... };   // к кому едет таксист

// Предложение механика (окно у игрока)
static gMechOfferFrom[MAX_PLAYERS] = { INVALID_PLAYER_ID, ... };
static gMechOfferFromDb[MAX_PLAYERS];
static gMechOfferType[MAX_PLAYERS];             // 1 — ремонт, 2 — бензин

// ---------------------------------------------------------------------
//  Значки работ и рабочие машины
// ---------------------------------------------------------------------
static Jobs_AddVehicle(job, model, Float:x, Float:y, Float:z, Float:a, c1, c2)
{
	new v = AddStaticVehicleEx(model, x, y, z, a, c1, c2, 300);
	if (v != INVALID_VEHICLE_ID && v < MAX_VEHICLES) gVehJob[v] = job;
}

static gJobMk[JOB_MECHANIC + 1] = { -1, ... };

// Записать машину за работой (worklife.inc ставит машины новых работ)
stock Jobs_SetVehicleJob(v, job)
{
	if (v > 0 && v < MAX_VEHICLES) gVehJob[v] = job;
}     // маркеры работ (двигаются в игре: /markers)

static Jobs_AddHQ(job, Float:x, Float:y, Float:z, const title[], const hint[])
{
	new text[MK_TEXT_LEN], name[32];
	format(text, sizeof text, "{FF9900}Работа: %s\n{FFFFFF}ALT — устроиться\n%s", title, hint);
	format(name, sizeof name, "Работа: %s", title);
	gJobMk[job] = Marker_Create(MK_JOB, job, name, 1275, x, y, z, 0, 0, GPS_JOBS);
	Marker_SetLabel(gJobMk[job], text);
}

stock Jobs_Init()
{
	DB_AddColumn("accounts", "job_exp", "INTEGER DEFAULT 0");

	Jobs_AddHQ(JOB_LOADER, LOADER_JOB_X, LOADER_JOB_Y, LOADER_JOB_Z, "Грузчик", "/work — начать смену");
	Jobs_AddHQ(JOB_PIZZA, PIZZA_JOB_X, PIZZA_JOB_Y, PIZZA_JOB_Z, "Развозчик пиццы", "Сядь на мопед и введи /work");
	Jobs_AddHQ(JOB_TAXI, TAXI_JOB_X, TAXI_JOB_Y, TAXI_JOB_Z, "Таксист", "Сядь в такси и введи /work");
	Jobs_AddHQ(JOB_BUS, BUS_JOB_X, BUS_JOB_Y, BUS_JOB_Z, "Водитель автобуса", "Сядь в автобус и введи /work");
	Jobs_AddHQ(JOB_TRUCKER, TRUCK_JOB_X, TRUCK_JOB_Y, TRUCK_JOB_Z, "Дальнобойщик", "Сядь в грузовик и введи /work");
	Jobs_AddHQ(JOB_MECHANIC, MECH_JOB_X, MECH_JOB_Y, MECH_JOB_Z, "Механик", "Сядь в эвакуатор и введи /work");

	// Мопеды пиццерии
	for (new i = 0; i < 4; i++) Jobs_AddVehicle(JOB_PIZZA, PIZZA_BIKE_MODEL, 2097.0 + float(i) * 1.5, -1797.5, 13.0, 0.0, 3, 6);
	// Такси, автобусы, грузовики, эвакуаторы (ряд машин от точки *_CARS_*)
	for (new i = 0; i < 4; i++) Jobs_AddVehicle(JOB_TAXI, TAXI_MODEL, TAXI_CARS_X + float(i) * 4.0, TAXI_CARS_Y, TAXI_CARS_Z, TAXI_CARS_A, 6, 1);
	for (new i = 0; i < 3; i++) Jobs_AddVehicle(JOB_BUS, BUS_MODEL, BUS_CARS_X + float(i) * 6.0, BUS_CARS_Y, BUS_CARS_Z, BUS_CARS_A, 1, 1);
	for (new i = 0; i < 3; i++) Jobs_AddVehicle(JOB_TRUCKER, TRUCK_MODEL, TRUCK_CARS_X + float(i) * 6.0, TRUCK_CARS_Y, TRUCK_CARS_Z, TRUCK_CARS_A, 1, 1);
	for (new i = 0; i < 2; i++) Jobs_AddVehicle(JOB_MECHANIC, MECH_MODEL, MECH_CARS_X + float(i) * 5.0, MECH_CARS_Y, MECH_CARS_Z, MECH_CARS_A, 1, 1);
}

// Рабочая машина какой профессии (0 — не рабочая). Нужна fuel.inc и licenses.inc.
stock Jobs_GetVehicleJob(vehicleid)
{
	if (vehicleid < 1 || vehicleid >= MAX_VEHICLES) return JOB_NONE;
	return gVehJob[vehicleid];
}

stock Job_GetName(job)
{
	new name[32];
	switch (job)
	{
		case JOB_LOADER:   strcat(name, "Грузчик");
		case JOB_PIZZA:    strcat(name, "Развозчик пиццы");
		case JOB_TAXI:     strcat(name, "Таксист");
		case JOB_BUS:      strcat(name, "Водитель автобуса");
		case JOB_TRUCKER:  strcat(name, "Дальнобойщик");
		case JOB_MECHANIC: strcat(name, "Механик");
		default:           strcat(name, WorkLife_JobName(job));    // новые работы (worklife.inc)
	}
	return name;
}

// Значок работы рядом (для ALT и /getjob)
stock Jobs_GetNearbyHQ(playerid)
{
	for (new j = JOB_LOADER; j <= JOB_MECHANIC; j++)
	{
		if (Marker_InRange(playerid, gJobMk[j], 3.0)) return j;
	}
	return WorkLife_GetNearbyHQ(playerid);
}

// ---------------------------------------------------------------------
//  Стаж и оплата
// ---------------------------------------------------------------------
// После входа в аккаунт (из Vehicles_LoadForPlayer)
stock Jobs_LoadPlayer(playerid)
{
	gJobExp[playerid] = 0;
	gTaxiPaidBy[playerid] = INVALID_PLAYER_ID;
	gTaxiCaller[playerid] = INVALID_PLAYER_ID;
	gMechOfferFrom[playerid] = INVALID_PLAYER_ID;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT job_exp FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0) gJobExp[playerid] = DB_GetFieldInt(r, 0);
	DB_FreeResultSet(r);
}

static Jobs_SaveExp(playerid)
{
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET job_exp = %d WHERE id = %d", gJobExp[playerid], Player[playerid][pDbId]));
}

stock Job_GetBonus(playerid)
{
	new bonus = (gJobExp[playerid] / JOB_SKILL_STEP) * 5;
	if (bonus > JOB_SKILL_MAX_BONUS) bonus = JOB_SKILL_MAX_BONUS;
	return bonus;
}

// Заплатить за выполненное задание с учётом стажа. Возвращает сумму.
stock Job_Pay(playerid, base)
{
	new pay = base + base * Job_GetBonus(playerid) / 100;
	GiveMoney(playerid, pay);
	gJobExp[playerid]++;
	Jobs_SaveExp(playerid);
	if (gJobExp[playerid] % JOB_SKILL_STEP == 0 && Job_GetBonus(playerid) <= JOB_SKILL_MAX_BONUS)
		SendClientMessage(playerid, COLOR_GREEN, "Стаж растёт: %d заданий. Надбавка к оплате теперь %d%%.", gJobExp[playerid], Job_GetBonus(playerid));
	return pay;
}

static Float:Jobs_Distance(Float:x1, Float:y1, Float:x2, Float:y2)
{
	return floatsqroot((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

// ---------------------------------------------------------------------
//  Начало и конец смены
// ---------------------------------------------------------------------
// Поставить точку задания и запомнить, когда и как далеко она была (проверка на телепорт)
stock Jobs_Checkpoint(playerid, Float:x, Float:y, Float:z, Float:size)
{
	gJobCpTick[playerid] = GetTickCount();
	gJobCpDist[playerid] = GetPlayerDistanceFromPoint(playerid, x, y, z);
	if (GetPlayerInterior(playerid) != 0) gJobCpDist[playerid] = 0.0;    // из интерьера расстояние до улицы ненастоящее — не проверяем
	SetPlayerCheckpoint(playerid, x, y, z, size);
}

// Закончить смену (по команде, при смерти, при выходе)
stock Job_Stop(playerid)
{
	if (!Player[playerid][pWorking]) return;
	Player[playerid][pWorking] = 0;
	Player[playerid][pWorkStep] = 0;
	Player[playerid][pWorkVehicle] = INVALID_VEHICLE_ID;
	gTaxiCaller[playerid] = INVALID_PLAYER_ID;
	WorkLife_OnStop(playerid);
	DisablePlayerCheckpoint(playerid);
	if (IsPlayerConnected(playerid))
	{
		SetPlayerSpecialAction(playerid, SPECIAL_ACTION_NONE);
		RemovePlayerAttachedObject(playerid, JOB_OBJECT_SLOT);
	}
}

stock Job_NextPizzaPoint(playerid)
{
	new p;
	do { p = random(sizeof (gPizzaPoints)); } while (p == gPizzaTarget[playerid]);
	gPizzaTarget[playerid] = p;
	Jobs_Checkpoint(playerid, gPizzaPoints[p][0], gPizzaPoints[p][1], gPizzaPoints[p][2], 4.0);
}

// Случайная точка города, не ближе 300 м к игроку
static Jobs_RandomCityPoint(playerid)
{
	new Float:x, Float:y, Float:z, p;
	GetPlayerPos(playerid, x, y, z);
	for (new tries = 0; tries < 20; tries++)
	{
		p = random(sizeof (gCityPoints));
		if (Jobs_Distance(x, y, gCityPoints[p][0], gCityPoints[p][1]) > 300.0) break;
	}
	return p;
}

static Jobs_SetTarget(playerid, Float:x, Float:y, Float:z, Float:size)
{
	DisablePlayerCheckpoint(playerid);
	Jobs_Checkpoint(playerid, x, y, z, size);
}

static Job_TaxiNextCall(playerid)
{
	new p = Jobs_RandomCityPoint(playerid);
	gJobTarget[playerid] = p;
	Player[playerid][pWorkStep] = WS_TAXI_PICKUP;
	Jobs_SetTarget(playerid, gCityPoints[p][0], gCityPoints[p][1], gCityPoints[p][2], 5.0);
	SendClientMessage(playerid, COLOR_YELLOW, "Диспетчер: новый вызов, клиент ждёт (красный маркер на карте).");
}

static Job_TruckNextOrder(playerid)
{
	// Любая АЗС не ближе 500 м
	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	new list[MAX_FUEL_STATIONS], n = 0;
	for (new s = 0; s < MAX_FUEL_STATIONS; s++)
	{
		if (FuelStation[s][fsUsed] && Jobs_Distance(x, y, FuelStation[s][fsX], FuelStation[s][fsY]) > 500.0) list[n++] = s;
	}
	if (n == 0)
	{
		SendClientMessage(playerid, COLOR_GREY, "Заказов нет: на карте нет АЗС. Попроси админа добавить (/addfuel).");
		Job_Stop(playerid);
		return;
	}
	new s = list[random(n)];
	gJobTarget[playerid] = s;
	gJobFromX[playerid] = x;
	gJobFromY[playerid] = y;
	Player[playerid][pWorkStep] = WS_TRUCK;
	Jobs_SetTarget(playerid, FuelStation[s][fsX], FuelStation[s][fsY], FuelStation[s][fsZ], 8.0);
	SendClientMessage(playerid, COLOR_YELLOW, "Груз: бензин для АЗС. Расстояние %d м, оплата по прибытии.",
		floatround(Jobs_Distance(x, y, FuelStation[s][fsX], FuelStation[s][fsY])));
}

static Job_MechNextCall(playerid)
{
	new p = Jobs_RandomCityPoint(playerid);
	gJobTarget[playerid] = p;
	Player[playerid][pWorkStep] = WS_MECH_CALL;
	Jobs_SetTarget(playerid, gCityPoints[p][0], gCityPoints[p][1], gCityPoints[p][2], 6.0);
	SendClientMessage(playerid, COLOR_YELLOW, "Диспетчер: у клиента сломалась машина (красный маркер). Игрокам рядом — /repair [id].");
}

// Сидит ли игрок за рулём рабочей машины своей профессии
static bool:Jobs_InOwnVehicle(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER) return false;
	return Jobs_GetVehicleJob(GetPlayerVehicleID(playerid)) == Player[playerid][pJob];
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Cmd_GetJob(playerid)
{
	new job = Jobs_GetNearbyHQ(playerid);
	if (job == JOB_NONE) return SendClientMessage(playerid, COLOR_GREY, "Подойди к значку работы (оранжевая футболка).");
	if (Player[playerid][pJob] == job)
	{
		if (WorkLife_OnHQAlt(playerid, job)) return true;     // рыбак сдаёт улов (worklife.inc)
		return SendClientMessage(playerid, COLOR_GREY, "Ты уже работаешь здесь.");
	}
	if (job != JOB_LOADER && job != JOB_PIZZA && (job < JOB_FISHER || WorkLife_NeedsLicense(job)) && !Licenses_Has(playerid, LIC_CAR))
		return SendClientMessage(playerid, COLOR_GREY, "Сюда берут только с правами на авто. Автошкола — у мэрии (/exam).");

	Job_Stop(playerid);
	Player[playerid][pJob] = job;
	gJobExp[playerid] = 0;
	Jobs_SaveExp(playerid);
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Ты устроился на работу: %s. Начать смену: /work", Job_GetName(job));
	return true;
}

stock bool:Cmd_QuitJob(playerid)
{
	if (Player[playerid][pJob] == JOB_NONE) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет работы.");
	Job_Stop(playerid);
	Player[playerid][pJob] = JOB_NONE;
	gJobExp[playerid] = 0;
	Jobs_SaveExp(playerid);
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_WHITE, "Ты уволился с работы.");
	return true;
}

stock bool:Cmd_Work(playerid)
{
	if (Player[playerid][pWorking])
	{
		Job_Stop(playerid);
		return SendClientMessage(playerid, COLOR_WHITE, "Смена окончена.");
	}

	new job = Player[playerid][pJob];
	switch (job)
	{
		case JOB_LOADER:
		{
			if (!Marker_InRange(playerid, gJobMk[JOB_LOADER], 30.0))
				return SendClientMessage(playerid, COLOR_GREY, "Смену грузчика можно начать только в порту.");
			if (IsPlayerInAnyVehicle(playerid))
				return SendClientMessage(playerid, COLOR_GREY, "Выйди из транспорта.");

			Player[playerid][pWorking] = 1;
			Player[playerid][pWorkStep] = 1;
			Jobs_Checkpoint(playerid, LOADER_TAKE_X, LOADER_TAKE_Y, LOADER_TAKE_Z, 1.5);
			SendClientMessage(playerid, COLOR_GREEN, "Смена началась. Иди к красному маркеру и возьми ящик. Закончить: /work");
			return true;
		}
		case JOB_PIZZA:
		{
			if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || GetVehicleModel(GetPlayerVehicleID(playerid)) != PIZZA_BIKE_MODEL)
				return SendClientMessage(playerid, COLOR_GREY, "Сядь за руль мопеда Pizzaboy у пиццерии.");

			Player[playerid][pWorking] = 1;
			Player[playerid][pWorkVehicle] = GetPlayerVehicleID(playerid);
			gPizzaTarget[playerid] = -1;
			Job_NextPizzaPoint(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Смена началась. Отвези пиццу по адресу (красный маркер на карте). Закончить: /work");
			return true;
		}
		case JOB_TAXI, JOB_BUS, JOB_TRUCKER, JOB_MECHANIC:
		{
			if (!Jobs_InOwnVehicle(playerid))
				return SendClientMessage(playerid, COLOR_GREY, "Сядь за руль рабочей машины (стоят у значка работы).");
			if (!Licenses_Has(playerid, LIC_CAR))
				return SendClientMessage(playerid, COLOR_GREY, "Нужны права на авто. Автошкола — у мэрии (/exam).");
		}
		default:
		{
			if (WorkLife_StartWork(playerid)) return true;     // рыбак, фермер, инкассатор, мусорщик
			return SendClientMessage(playerid, COLOR_GREY, "У тебя нет работы. Найди значок работы и введи /getjob");
		}
	}

	Player[playerid][pWorking] = 1;
	Player[playerid][pWorkVehicle] = GetPlayerVehicleID(playerid);
	switch (job)
	{
		case JOB_TAXI:
		{
			gTaxiPaidBy[playerid] = INVALID_PLAYER_ID;
			SendClientMessage(playerid, COLOR_GREEN, "Смена таксиста началась. Пассажир платит $%d при посадке. Вызовы игроков: /taxi. Закончить: /work", TAXI_FARE);
			Job_TaxiNextCall(playerid);
		}
		case JOB_BUS:
		{
			gJobTarget[playerid] = 0;
			Player[playerid][pWorkStep] = WS_BUS;
			Jobs_SetTarget(playerid, gBusStops[0][0], gBusStops[0][1], gBusStops[0][2], 6.0);
			SendClientMessage(playerid, COLOR_GREEN, "Рейс начался. Объезжай остановки по маркерам, $%d за каждую. Закончить: /work", BUS_PAY_STOP);
		}
		case JOB_TRUCKER:
		{
			SendClientMessage(playerid, COLOR_GREEN, "Смена дальнобойщика началась. Закончить: /work");
			Job_TruckNextOrder(playerid);
		}
		case JOB_MECHANIC:
		{
			SendClientMessage(playerid, COLOR_GREEN, "Смена механика началась. /repair [id] — ремонт за $%d, /refill [id] — %d л бензина за $%d. Закончить: /work",
				MECH_REPAIR_PRICE, FUEL_CANISTER, MECH_FUEL_PRICE);
			Job_MechNextCall(playerid);
		}
	}
	return true;
}

// Вызвать такси (любой игрок)
stock bool:Cmd_Taxi(playerid)
{
	if (IsPlayerInAnyVehicle(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Выйди из машины.");
	new count = 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (i == playerid || !IsPlayerConnected(i) || !Player[i][pWorking] || Player[i][pJob] != JOB_TAXI) continue;
		SendClientMessage(i, COLOR_YELLOW, "Диспетчер: %s [%d] вызывает такси. Принять: /gotaxi %d", GetRPName(playerid), playerid, playerid);
		count++;
	}
	if (count == 0) return SendClientMessage(playerid, COLOR_GREY, "Сейчас нет таксистов на смене.");
	SendClientMessage(playerid, COLOR_WHITE, "Вызов принят диспетчером (таксистов на смене: %d). Жди на месте, проезд $%d.", count, TAXI_FARE);
	return true;
}

// Таксист принимает вызов игрока
stock bool:Cmd_GoTaxi(playerid, const params[])
{
	if (!Player[playerid][pWorking] || Player[playerid][pJob] != JOB_TAXI) return SendClientMessage(playerid, COLOR_GREY, "Ты не на смене таксиста.");
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /gotaxi [id]");
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (i != playerid && IsPlayerConnected(i) && gTaxiCaller[i] == target)
			return SendClientMessage(playerid, COLOR_GREY, "Этот вызов уже принял другой таксист.");
	}
	new Float:x, Float:y, Float:z;
	GetPlayerPos(target, x, y, z);
	gTaxiCaller[playerid] = target;
	Player[playerid][pWorkStep] = WS_TAXI_CALLER;
	Jobs_SetTarget(playerid, x, y, z, 6.0);
	SendClientMessage(playerid, COLOR_GREEN, "Ты принял вызов %s. Место отмечено на карте.", GetRPName(target));
	SendClientMessage(target, COLOR_GREEN, "Таксист %s принял вызов и едет к тебе.", GetRPName(playerid));
	return true;
}

// Механик: /repair [id] и /refill [id]
static bool:Jobs_MechOffer(playerid, const params[], type)
{
	if (!Player[playerid][pWorking] || Player[playerid][pJob] != JOB_MECHANIC) return SendClientMessage(playerid, COLOR_GREY, "Ты не на смене механика.");
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid)
	{
		if (type == 1) return SendClientMessage(playerid, COLOR_GREY, "Использование: /repair [id]");
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /refill [id]");
	}
	if (!IsPlayerInAnyVehicle(target) || !IsPlayerNearPlayer(playerid, target, 8.0))
		return SendClientMessage(playerid, COLOR_GREY, "Игрок должен сидеть в машине рядом с тобой.");
	new Float:x, Float:y, Float:z;
	GetVehiclePos(Player[playerid][pWorkVehicle], x, y, z);
	if (!IsPlayerInRangeOfPoint(playerid, 25.0, x, y, z))
		return SendClientMessage(playerid, COLOR_GREY, "Твой эвакуатор с инструментами слишком далеко.");
	if (gMechOfferFrom[target] != INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Игроку уже что-то предложили.");

	gMechOfferFrom[target] = playerid;
	gMechOfferFromDb[target] = Player[playerid][pDbId];
	gMechOfferType[target] = type;
	if (type == 1)
		ShowPlayerDialog(target, DIALOG_JOB_OFFER, DIALOG_STYLE_MSGBOX, "Механик",
			"{FFFFFF}Механик %s предлагает починить машину.\nЦена: {FFFF00}$%d\n\n{FFFFFF}Согласен?", "Да", "Нет", GetRPName(playerid), MECH_REPAIR_PRICE);
	else
		ShowPlayerDialog(target, DIALOG_JOB_OFFER, DIALOG_STYLE_MSGBOX, "Механик",
			"{FFFFFF}Механик %s предлагает залить %d л бензина.\nЦена: {FFFF00}$%d\n\n{FFFFFF}Согласен?", "Да", "Нет", GetRPName(playerid), FUEL_CANISTER, MECH_FUEL_PRICE);
	SendClientMessage(playerid, COLOR_WHITE, "Предложение отправлено %s.", GetRPName(target));
	return true;
}

stock bool:Cmd_Repair(playerid, const params[]) { return Jobs_MechOffer(playerid, params, 1); }
stock bool:Cmd_Refill(playerid, const params[]) { return Jobs_MechOffer(playerid, params, 2); }

stock Jobs_OnDialog(playerid, dialogid, response)
{
	if (dialogid != DIALOG_JOB_OFFER) return 0;
	new mech = gMechOfferFrom[playerid], type = gMechOfferType[playerid];
	gMechOfferFrom[playerid] = INVALID_PLAYER_ID;
	if (mech == INVALID_PLAYER_ID || !IsPlayerConnected(mech) || Player[mech][pDbId] != gMechOfferFromDb[playerid]) return 1;
	if (!response)
	{
		SendClientMessage(mech, COLOR_GREY, "%s отказался.", GetRPName(playerid));
		return 1;
	}
	new v = GetPlayerVehicleID(playerid);
	if (v == 0 || !IsPlayerNearPlayer(playerid, mech, 10.0))
	{
		SendClientMessage(playerid, COLOR_GREY, "Механик ушёл или ты вышел из машины.");
		return 1;
	}
	new price = (type == 1) ? MECH_REPAIR_PRICE : MECH_FUEL_PRICE;
	if (GetMoney(playerid) < price)
	{
		SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", price);
		SendClientMessage(mech, COLOR_GREY, "У клиента не хватает денег.");
		return 1;
	}
	GiveMoney(playerid, -price);
	Account_Save(playerid);

	new msg[96];
	if (type == 1)
	{
		RepairVehicle(v);
		format(msg, sizeof msg, "%s чинит машину %s.", GetRPName(mech), GetRPName(playerid));
	}
	else
	{
		Fuel_Set(v, Fuel_Get(v) + float(FUEL_CANISTER));
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (IsPlayerConnected(i) && gPlayerVeh[i] == v) Fuel_SavePersonal(i);
		}
		format(msg, sizeof msg, "%s заливает бензин в машину %s.", GetRPName(mech), GetRPName(playerid));
	}
	SendNearbyMessage(mech, CHAT_RADIUS, COLOR_PURPLE, msg);
	new pay = Job_Pay(mech, price);
	Account_Save(mech);
	SendClientMessage(mech, COLOR_GREEN, "Работа выполнена, +$%d.", pay);
	return 1;
}

// ---------------------------------------------------------------------
//  События
// ---------------------------------------------------------------------
// Из OnPlayerStateChange: в рабочую машину садятся только работники
stock Jobs_OnBecomeDriver(playerid)
{
	new v = GetPlayerVehicleID(playerid);
	new job = Jobs_GetVehicleJob(v);
	if (job == JOB_NONE) return 0;
	if (Player[playerid][pJob] != job && Player[playerid][pAdmin] < 3)
	{
		RemovePlayerFromVehicle(playerid);
		SendClientMessage(playerid, COLOR_GREY, "Это рабочая машина (%s). Устроиться можно у значка работы рядом.", Job_GetName(job));
		return 1;
	}
	if (!Player[playerid][pWorking]) SendClientMessage(playerid, COLOR_BLUE, "Рабочая машина. Начать смену: /work");
	return 1;
}

// Из OnPlayerStateChange: пассажир сел в такси на смене — платит за проезд
stock Jobs_OnBecomePassenger(playerid)
{
	new v = GetPlayerVehicleID(playerid);
	if (Jobs_GetVehicleJob(v) != JOB_TAXI) return;
	new driver = INVALID_PLAYER_ID;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && GetPlayerVehicleID(i) == v && GetPlayerState(i) == PLAYER_STATE_DRIVER) { driver = i; break; }
	}
	if (driver == INVALID_PLAYER_ID || !Player[driver][pWorking] || Player[driver][pJob] != JOB_TAXI) return;
	if (gTaxiPaidBy[driver] == playerid) return;    // уже заплатил (пересел)
	if (GetMoney(playerid) < TAXI_FARE)
	{
		RemovePlayerFromVehicle(playerid);
		SendClientMessage(playerid, COLOR_GREY, "Проезд стоит $%d, у тебя не хватает денег.", TAXI_FARE);
		return;
	}
	GiveMoney(playerid, -TAXI_FARE);
	Account_Save(playerid);
	gTaxiPaidBy[driver] = playerid;
	new pay = Job_Pay(driver, TAXI_FARE);
	SendClientMessage(playerid, COLOR_WHITE, "Ты заплатил за такси $%d.", TAXI_FARE);
	SendClientMessage(driver, COLOR_GREEN, "Пассажир %s оплатил проезд: +$%d.", GetRPName(playerid), pay);
	if (gTaxiCaller[driver] == playerid)
	{
		gTaxiCaller[driver] = INVALID_PLAYER_ID;
		DisablePlayerCheckpoint(driver);
		SendClientMessage(driver, COLOR_WHITE, "Отвези пассажира, куда он скажет. Новый вызов диспетчера: /work дважды.");
	}
}

// Вызывается из OnPlayerEnterCheckpoint
stock Jobs_OnEnterCheckpoint(playerid)
{
	if (!Player[playerid][pWorking]) return 0;

	// Античит: до точки нельзя добраться быстрее JOB_MAX_SPEED по прямой. Админов не проверяем — они телепортируются.
	new elapsed = GetTickCount() - gJobCpTick[playerid];
	if (Player[playerid][pAdmin] < 1 && gJobCpDist[playerid] > 50.0 && float(elapsed) * JOB_MAX_SPEED / 1000.0 < gJobCpDist[playerid])
	{
		new warn[144];
		format(warn, sizeof warn, "[Античит] %s [%d] оказался на точке работы слишком быстро: %d м за %d сек. Смена остановлена.",
			GetRPName(playerid), playerid, floatround(gJobCpDist[playerid]), elapsed / 1000);
		SendAdminMessage(COLOR_RED, warn);
		printf("%s", warn);
		Job_Stop(playerid);
		SendClientMessage(playerid, COLOR_RED, "Смена остановлена: ты оказался на точке слишком быстро.");
		return 1;
	}

	switch (Player[playerid][pJob])
	{
		case JOB_LOADER:
		{
			if (IsPlayerInAnyVehicle(playerid)) return 1;
			if (Player[playerid][pWorkStep] == 1)
			{
				// Взял ящик
				SetPlayerSpecialAction(playerid, SPECIAL_ACTION_CARRY);
				SetPlayerAttachedObject(playerid, JOB_OBJECT_SLOT, 1220, 1, 0.11, 0.36, 0.0, 0.0, 90.0, 0.0, 0.6, 0.6, 0.6);
				Player[playerid][pWorkStep] = 2;
				Jobs_Checkpoint(playerid, LOADER_DROP_X, LOADER_DROP_Y, LOADER_DROP_Z, 1.5);
				SendClientMessage(playerid, COLOR_WHITE, "Отнеси ящик на склад (красный маркер).");
			}
			else
			{
				// Принёс ящик
				SetPlayerSpecialAction(playerid, SPECIAL_ACTION_NONE);
				RemovePlayerAttachedObject(playerid, JOB_OBJECT_SLOT);
				new pay = Job_Pay(playerid, LOADER_PAY);
				Player[playerid][pWorkStep] = 1;
				Jobs_Checkpoint(playerid, LOADER_TAKE_X, LOADER_TAKE_Y, LOADER_TAKE_Z, 1.5);
				SendClientMessage(playerid, COLOR_GREEN, "+$%d. Иди за следующим ящиком.", pay);
			}
		}
		case JOB_PIZZA:
		{
			if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || GetVehicleModel(GetPlayerVehicleID(playerid)) != PIZZA_BIKE_MODEL)
			{
				SendClientMessage(playerid, COLOR_GREY, "Нужно приехать на мопеде Pizzaboy.");
				return 1;
			}
			new pay = Job_Pay(playerid, PIZZA_PAY_MIN + random(PIZZA_PAY_RANDOM));
			SendClientMessage(playerid, COLOR_GREEN, "Пицца доставлена! +$%d. Новый адрес отмечен на карте.", pay);
			Job_NextPizzaPoint(playerid);
		}
		case JOB_TAXI, JOB_BUS, JOB_TRUCKER, JOB_MECHANIC:
		{
			if (!Jobs_InOwnVehicle(playerid))
			{
				SendClientMessage(playerid, COLOR_GREY, "Нужно приехать на рабочей машине.");
				return 1;
			}
			Jobs_OnVehicleCheckpoint(playerid);
		}
		default: WorkLife_OnCheckpoint(playerid);   // новые работы (worklife.inc)
	}
	return 1;
}

static Jobs_OnVehicleCheckpoint(playerid)
{
	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	switch (Player[playerid][pWorkStep])
	{
		case WS_TAXI_PICKUP:
		{
			// «Клиент» сел — везём в другую точку
			new from = gJobTarget[playerid], p;
			do { p = random(sizeof (gCityPoints)); } while (p == from);
			gJobTarget[playerid] = p;
			gJobFromX[playerid] = x;
			gJobFromY[playerid] = y;
			Player[playerid][pWorkStep] = WS_TAXI_DROPOFF;
			Jobs_SetTarget(playerid, gCityPoints[p][0], gCityPoints[p][1], gCityPoints[p][2], 5.0);
			SendClientMessage(playerid, COLOR_WHITE, "Клиент сел. Отвези его по адресу (маркер на карте).");
		}
		case WS_TAXI_DROPOFF:
		{
			new dist = floatround(Jobs_Distance(gJobFromX[playerid], gJobFromY[playerid], x, y));
			new pay = Job_Pay(playerid, TAXI_CALL_BASE + dist * TAXI_PAY_PER_100M / 100);
			SendClientMessage(playerid, COLOR_GREEN, "Клиент доставлен (%d м): +$%d.", dist, pay);
			Job_TaxiNextCall(playerid);
		}
		case WS_TAXI_CALLER:
		{
			DisablePlayerCheckpoint(playerid);
			SendClientMessage(playerid, COLOR_WHITE, "Ты на месте. Жди, пока пассажир сядет.");
		}
		case WS_BUS:
		{
			new pay = Job_Pay(playerid, BUS_PAY_STOP);
			new next = gJobTarget[playerid] + 1;
			if (next >= sizeof (gBusStops))
			{
				next = 0;
				GiveMoney(playerid, BUS_PAY_ROUTE);
				SendClientMessage(playerid, COLOR_GREEN, "Круг пройден! Премия $%d.", BUS_PAY_ROUTE);
			}
			gJobTarget[playerid] = next;
			Jobs_SetTarget(playerid, gBusStops[next][0], gBusStops[next][1], gBusStops[next][2], 6.0);
			SendClientMessage(playerid, COLOR_GREEN, "Остановка: +$%d. Следующая отмечена на карте.", pay);
		}
		case WS_TRUCK:
		{
			new dist = floatround(Jobs_Distance(gJobFromX[playerid], gJobFromY[playerid], x, y));
			new Float:hp;
			GetVehicleHealth(GetPlayerVehicleID(playerid), hp);
			new base = TRUCK_PAY_BASE + dist * TRUCK_PAY_PER_100M / 100;
			if (hp < 600.0)
			{
				base /= 2;
				SendClientMessage(playerid, COLOR_ORANGE, "Грузовик сильно побит, часть груза испорчена: оплата вдвое меньше.");
			}
			new pay = Job_Pay(playerid, base);
			SendClientMessage(playerid, COLOR_GREEN, "Бензин доставлен (%d м): +$%d.", dist, pay);
			Job_TruckNextOrder(playerid);
		}
		case WS_MECH_CALL:
		{
			new pay = Job_Pay(playerid, MECH_CALL_PAY);
			SendClientMessage(playerid, COLOR_GREEN, "Машина клиента починена: +$%d.", pay);
			Job_MechNextCall(playerid);
		}
	}
}

// Из OnPlayerDisconnect: сбросить ссылки других игроков на этого
stock Jobs_OnDisconnect(playerid)
{
	gMechOfferFrom[playerid] = INVALID_PLAYER_ID;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gTaxiCaller[i] == playerid)
		{
			gTaxiCaller[i] = INVALID_PLAYER_ID;
			if (IsPlayerConnected(i) && Player[i][pWorking])
			{
				SendClientMessage(i, COLOR_GREY, "Клиент ушёл. Новый вызов диспетчера.");
				Job_TaxiNextCall(i);
			}
		}
		if (gTaxiPaidBy[i] == playerid) gTaxiPaidBy[i] = INVALID_PLAYER_ID;
		if (gMechOfferFrom[i] == playerid) gMechOfferFrom[i] = INVALID_PLAYER_ID;
	}
}
