// =====================================================================
//  jobs.inc — работы.
//
//  1. Грузчик (порт): бери ящик в одной точке, неси в другую.
//  2. Развозчик пиццы: садись на мопед Pizzaboy, вези заказы по городу.
//
//  /getjob — устроиться (стоя на значке работы)
//  /work   — начать / закончить смену
//  /quitjob — уволиться
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

#define JOB_OBJECT_SLOT     9           // слот прикреплённого объекта (ящик)

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

static gPizzaTarget[MAX_PLAYERS];

stock Jobs_Init()
{
	CreatePickup(1275, 1, LOADER_JOB_X, LOADER_JOB_Y, LOADER_JOB_Z, 0);
	Create3DTextLabel("{FF9900}Работа: Грузчик\n{FFFFFF}ALT — устроиться\n/work — начать смену",
		COLOR_WHITE, LOADER_JOB_X, LOADER_JOB_Y, LOADER_JOB_Z + 0.6, 15.0, 0, true);

	CreatePickup(1275, 1, PIZZA_JOB_X, PIZZA_JOB_Y, PIZZA_JOB_Z, 0);
	Create3DTextLabel("{FF9900}Работа: Развозчик пиццы\n{FFFFFF}ALT — устроиться\nСядь на мопед и введи /work",
		COLOR_WHITE, PIZZA_JOB_X, PIZZA_JOB_Y, PIZZA_JOB_Z + 0.6, 15.0, 0, true);

	// Мопеды пиццерии
	for (new i = 0; i < 4; i++)
	{
		AddStaticVehicleEx(PIZZA_BIKE_MODEL, 2097.0 + float(i) * 1.5, -1797.5, 13.0, 0.0, 3, 6, 120);
	}
}

stock Job_GetName(job)
{
	new name[32];
	switch (job)
	{
		case JOB_LOADER: strcat(name, "Грузчик");
		case JOB_PIZZA:  strcat(name, "Развозчик пиццы");
		default:         strcat(name, "Безработный");
	}
	return name;
}

// Закончить смену (по команде, при смерти, при выходе)
stock Job_Stop(playerid)
{
	if (!Player[playerid][pWorking]) return;
	Player[playerid][pWorking] = 0;
	Player[playerid][pWorkStep] = 0;
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
	SetPlayerCheckpoint(playerid, gPizzaPoints[p][0], gPizzaPoints[p][1], gPizzaPoints[p][2], 4.0);
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Cmd_GetJob(playerid)
{
	new job = JOB_NONE;
	if (IsPlayerInRangeOfPoint(playerid, 3.0, LOADER_JOB_X, LOADER_JOB_Y, LOADER_JOB_Z)) job = JOB_LOADER;
	else if (IsPlayerInRangeOfPoint(playerid, 3.0, PIZZA_JOB_X, PIZZA_JOB_Y, PIZZA_JOB_Z)) job = JOB_PIZZA;

	if (job == JOB_NONE) return SendClientMessage(playerid, COLOR_GREY, "Подойди к значку работы (оранжевая футболка).");
	if (Player[playerid][pJob] == job) return SendClientMessage(playerid, COLOR_GREY, "Ты уже работаешь здесь.");

	Job_Stop(playerid);
	Player[playerid][pJob] = job;
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Ты устроился на работу: %s. Начать смену: /work", Job_GetName(job));
	return true;
}

stock bool:Cmd_QuitJob(playerid)
{
	if (Player[playerid][pJob] == JOB_NONE) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет работы.");
	Job_Stop(playerid);
	Player[playerid][pJob] = JOB_NONE;
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

	switch (Player[playerid][pJob])
	{
		case JOB_LOADER:
		{
			if (!IsPlayerInRangeOfPoint(playerid, 30.0, LOADER_JOB_X, LOADER_JOB_Y, LOADER_JOB_Z))
				return SendClientMessage(playerid, COLOR_GREY, "Смену грузчика можно начать только в порту.");
			if (IsPlayerInAnyVehicle(playerid))
				return SendClientMessage(playerid, COLOR_GREY, "Выйди из транспорта.");

			Player[playerid][pWorking] = 1;
			Player[playerid][pWorkStep] = 1;
			SetPlayerCheckpoint(playerid, LOADER_TAKE_X, LOADER_TAKE_Y, LOADER_TAKE_Z, 1.5);
			SendClientMessage(playerid, COLOR_GREEN, "Смена началась. Иди к красному маркеру и возьми ящик. Закончить: /work");
		}
		case JOB_PIZZA:
		{
			if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || GetVehicleModel(GetPlayerVehicleID(playerid)) != PIZZA_BIKE_MODEL)
				return SendClientMessage(playerid, COLOR_GREY, "Сядь за руль мопеда Pizzaboy у пиццерии.");

			Player[playerid][pWorking] = 1;
			gPizzaTarget[playerid] = -1;
			Job_NextPizzaPoint(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Смена началась. Отвези пиццу по адресу (красный маркер на карте). Закончить: /work");
		}
		default:
		{
			return SendClientMessage(playerid, COLOR_GREY, "У тебя нет работы. Найди значок работы и введи /getjob");
		}
	}
	return true;
}

// Вызывается из OnPlayerEnterCheckpoint
stock Jobs_OnEnterCheckpoint(playerid)
{
	if (!Player[playerid][pWorking]) return 0;

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
				SetPlayerCheckpoint(playerid, LOADER_DROP_X, LOADER_DROP_Y, LOADER_DROP_Z, 1.5);
				SendClientMessage(playerid, COLOR_WHITE, "Отнеси ящик на склад (красный маркер).");
			}
			else
			{
				// Принёс ящик
				SetPlayerSpecialAction(playerid, SPECIAL_ACTION_NONE);
				RemovePlayerAttachedObject(playerid, JOB_OBJECT_SLOT);
				GiveMoney(playerid, LOADER_PAY);
				Player[playerid][pWorkStep] = 1;
				SetPlayerCheckpoint(playerid, LOADER_TAKE_X, LOADER_TAKE_Y, LOADER_TAKE_Z, 1.5);
				SendClientMessage(playerid, COLOR_GREEN, "+$%d. Иди за следующим ящиком.", LOADER_PAY);
			}
		}
		case JOB_PIZZA:
		{
			if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || GetVehicleModel(GetPlayerVehicleID(playerid)) != PIZZA_BIKE_MODEL)
			{
				SendClientMessage(playerid, COLOR_GREY, "Нужно приехать на мопеде Pizzaboy.");
				return 1;
			}
			new pay = PIZZA_PAY_MIN + random(PIZZA_PAY_RANDOM);
			GiveMoney(playerid, pay);
			SendClientMessage(playerid, COLOR_GREEN, "Пицца доставлена! +$%d. Новый адрес отмечен на карте.", pay);
			Job_NextPizzaPoint(playerid);
		}
	}
	return 1;
}
