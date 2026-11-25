// =====================================================================
//  licenses.inc — лицензии и автошкола.
//
//  Лицензии: авто, мото, лодки, оружие. Хранятся в accounts
//  (колонки lic_car, lic_moto, lic_boat, lic_gun).
//
//  Автошкола (значок у мэрии Лос-Сантоса, ALT или /exam):
//    1. Теория — LIC_THEORY_QUESTIONS вопросов о правилах, нужно ответить
//       без ошибок хотя бы на LIC_THEORY_PASS.
//    2. Практика (авто и мото) — проехать маршрут по чекпоинтам на
//       учебной машине, не разбив её (здоровье машины не ниже LIC_EXAM_MIN_HP).
//       Вышел из машины или прошло LIC_EXAM_TIME секунд — экзамен провален.
//    Лодки и оружие — только теория (оружие с LIC_GUN_MIN_LEVEL уровня).
//
//  Без прав ездить можно, но полиция проверяет их (/checklic) и может
//  забрать (/takelic). Оружие в оружейном магазине продают только с лицензией.
//
//  /licenses — мои лицензии. /showlic [id] — показать игроку рядом.
//  Админы: /givelic [id] [1-4] (3+), /addexampoint, /clearexam (4+) —
//  записать свой маршрут экзамена: проедь его и ставь точки по порядку.
// =====================================================================

enum
{
	LIC_CAR = 1,
	LIC_MOTO,
	LIC_BOAT,
	LIC_GUN
}
#define LIC_COUNT           5           // 0 не используется

#define LIC_THEORY_QUESTIONS 5
#define LIC_THEORY_PASS      4
#define LIC_EXAM_TIME        300
#define LIC_EXAM_MIN_HP      700.0
#define LIC_MAX_ROUTE        30


static const gLicNames[LIC_COUNT][] = { "", "Авто", "Мото", "Лодки", "Оружие" };
static const gLicColumns[LIC_COUNT][] = { "", "lic_car", "lic_moto", "lic_boat", "lic_gun" };
// Цена экзамена (меняется в /srvset)
static Lic_Price(l)
{
	switch (l)
	{
		case 1: return LIC_PRICE_CAR;
		case 2: return LIC_PRICE_MOTO;
		case 3: return LIC_PRICE_BOAT;
		case 4: return LIC_PRICE_GUN;
	}
	return 0;
}

static bool:gLic[MAX_PLAYERS][LIC_COUNT];
static gSchoolMk = -1;                  // маркер автошколы (двигается в игре: /markers)
static gExamType[MAX_PLAYERS];          // какую лицензию сдаёт (0 = не сдаёт)
static gExamStep[MAX_PLAYERS];          // номер вопроса / чекпоинта
static gExamCorrect[MAX_PLAYERS];
static gExamQuestion[MAX_PLAYERS][LIC_THEORY_QUESTIONS];
static gExamVehicle[MAX_PLAYERS] = { INVALID_VEHICLE_ID, ... };
static gExamTime[MAX_PLAYERS];

static Float:gRoute[LIC_MAX_ROUTE][3];
static gRouteCount;

// Вопросы теории: текст, варианты через \n, номер правильного (с 0)
static const gTheoryText[][] =
{
	"На светофоре горит красный. Что делать?",
	"С какой стороны дороги ехать?",
	"Тебя останавливает полиция. Что делать?",
	"Скорость в городе?",
	"Ты задел другую машину. Что делать?",
	"Можно ли садиться за руль пьяным?",
	"Где можно парковаться?",
	"Сзади едет скорая с сиреной. Что делать?"
};
static const gTheoryAnswers[][] =
{
	"Проехать, если никого нет\nОстановиться\nПосигналить и ехать",
	"По левой\nПо центру\nПо правой",
	"Остановиться и выполнять требования\nУехать\nВыйти и убежать",
	"Сколько выжмет машина\nНе больше 60 км/ч\nНе меньше 100 км/ч",
	"Остановиться и решить вопрос\nУехать как можно быстрее\nНичего, так бывает",
	"Да, если недалеко\nДа, ночью\nНет",
	"У обочины, не мешая движению\nНа перекрёстке\nПосреди дороги",
	"Ускориться, чтобы не мешать\nПрижаться вправо и пропустить\nЕхать как ехал"
};
static const gTheoryRight[] = { 1, 2, 0, 1, 0, 2, 0, 1 };

// Маршрут по умолчанию вокруг Pershing Square (если админ не записал свой)
static const Float:gDefaultRoute[][3] =
{
	{ 1530.0, -1735.0, 13.4 },
	{ 1570.0, -1700.0, 13.4 },
	{ 1570.0, -1600.0, 13.4 },
	{ 1480.0, -1590.0, 13.4 },
	{ 1390.0, -1600.0, 13.4 },
	{ 1390.0, -1700.0, 13.4 },
	{ 1430.0, -1735.0, 13.4 },
	{ LIC_CAR_SPAWN_X, LIC_CAR_SPAWN_Y, LIC_CAR_SPAWN_Z }
};

// ---------------------------------------------------------------------
//  Загрузка
// ---------------------------------------------------------------------
forward Licenses_OnSecond();

stock Licenses_Init()
{
	for (new l = 1; l < LIC_COUNT; l++) DB_AddColumn("accounts", gLicColumns[l], "INTEGER DEFAULT 0");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS exam_route (id INTEGER PRIMARY KEY AUTOINCREMENT, x REAL, y REAL, z REAL)"));
	Licenses_LoadRoute();

	new Float:sx = LIC_SCHOOL_X, Float:sy = LIC_SCHOOL_Y, Float:sz = LIC_SCHOOL_Z;
	gSchoolMk = Marker_Create(MK_SCHOOL, 0, "Автошкола", 1254, sx, sy, sz, 0, 0, GPS_GOV);
	Marker_SetLabel(gSchoolMk, "{33CCFF}Автошкола\n{FFFFFF}ALT — сдать на права и лицензии");
	SetTimer("Licenses_OnSecond", 1000, true);
}

static Licenses_LoadRoute()
{
	gRouteCount = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT x, y, z FROM exam_route ORDER BY id");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows && gRouteCount < LIC_MAX_ROUTE; i++)
		{
			gRoute[gRouteCount][0] = DB_GetFieldFloat(r, 0);
			gRoute[gRouteCount][1] = DB_GetFieldFloat(r, 1);
			gRoute[gRouteCount][2] = DB_GetFieldFloat(r, 2);
			gRouteCount++;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	if (gRouteCount < 2)
	{
		gRouteCount = 0;
		for (new i = 0; i < sizeof (gDefaultRoute); i++)
		{
			gRoute[i][0] = gDefaultRoute[i][0];
			gRoute[i][1] = gDefaultRoute[i][1];
			gRoute[i][2] = gDefaultRoute[i][2];
			gRouteCount++;
		}
	}
}

// После входа в аккаунт (из Vehicles_LoadForPlayer)
stock Licenses_LoadPlayer(playerid)
{
	for (new l = 0; l < LIC_COUNT; l++) gLic[playerid][l] = false;
	gExamType[playerid] = 0;
	gExamVehicle[playerid] = INVALID_VEHICLE_ID;

	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT lic_car, lic_moto, lic_boat, lic_gun FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		for (new l = 1; l < LIC_COUNT; l++) gLic[playerid][l] = DB_GetFieldInt(r, l - 1) != 0;
	}
	DB_FreeResultSet(r);
}

stock bool:Licenses_Has(playerid, lic)
{
	if (lic < 1 || lic >= LIC_COUNT) return false;
	return gLic[playerid][lic];
}

static Licenses_Set(playerid, lic, bool:value)
{
	gLic[playerid][lic] = value;
	if (Player[playerid][pDbId] != 0)
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET %s = %d WHERE id = %d", gLicColumns[lic], value ? 1 : 0, Player[playerid][pDbId]));
}

// Выдать лицензию без экзамена (донат-магазин, donate.inc)
stock Licenses_Give(playerid, lic)
{
	if (lic >= 1 && lic < LIC_COUNT) Licenses_Set(playerid, lic, true);
}

// Какая лицензия нужна для модели: 0 — никакой
stock Licenses_ForModel(model)
{
	switch (model)
	{
		case 0, 481, 509, 510: return 0;                                         // велосипеды
		case 448, 461, 462, 463, 468, 471, 521, 522, 523, 581, 586: return LIC_MOTO;
		case 430, 446, 452, 453, 454, 472, 473, 484, 493, 595: return LIC_BOAT;
		case 417, 425, 447, 460, 469, 476, 487, 488, 497, 511, 512, 513, 519, 520, 548, 553, 563, 577, 592, 593: return 0;   // авиация — позже
		case 537, 538, 569, 570, 590: return 0;                                 // поезда
	}
	return LIC_CAR;
}

// Из OnPlayerStateChange, когда игрок сел за руль
stock Licenses_OnBecomeDriver(playerid)
{
	new v = GetPlayerVehicleID(playerid);

	// В учебную машину садится только экзаменуемый
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (i != playerid && gExamVehicle[i] == v)
		{
			RemovePlayerFromVehicle(playerid);
			SendClientMessage(playerid, COLOR_GREY, "Это учебная машина автошколы.");
			return;
		}
	}
	if (v == gExamVehicle[playerid] || gVehFaction[v] != 0 || gVehAdmin[v] || v == Player[playerid][pWorkVehicle] || Jobs_GetVehicleJob(v) != JOB_NONE) return;

	new lic = Licenses_ForModel(GetVehicleModel(v));
	if (lic != 0 && !gLic[playerid][lic])
		SendClientMessage(playerid, COLOR_ORANGE, "У тебя нет лицензии (%s). Полиция может оштрафовать. Автошкола — у мэрии.", gLicNames[lic]);
}

// ---------------------------------------------------------------------
//  Автошкола
// ---------------------------------------------------------------------
stock bool:Licenses_IsNearSchool(playerid)
{
	return Marker_InRange(playerid, gSchoolMk, 3.0) != 0;
}

stock bool:Cmd_Exam(playerid)
{
	if (!Licenses_IsNearSchool(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Ты должен быть в автошколе (у мэрии).");
	if (gExamType[playerid] != 0) return SendClientMessage(playerid, COLOR_GREY, "Ты уже сдаёшь экзамен.");

	new list[256] = "Лицензия\tЦена\tСтатус\n", line[64];
	for (new l = 1; l < LIC_COUNT; l++)
	{
		if (gLic[playerid][l]) format(line, sizeof line, "%s\t$%d\t{33AA33}есть\n", gLicNames[l], Lic_Price(l));
		else format(line, sizeof line, "%s\t$%d\t{AFAFAF}нет\n", gLicNames[l], Lic_Price(l));
		strcat(list, line);
	}
	ShowPlayerDialog(playerid, DIALOG_LIC_MENU, DIALOG_STYLE_TABLIST_HEADERS, "Автошкола", "%s", "Сдавать", "Закрыть", list);
	return true;
}

static Licenses_AskQuestion(playerid)
{
	new q = gExamQuestion[playerid][gExamStep[playerid]];
	new title[48];
	format(title, sizeof title, "Теория: вопрос %d из %d", gExamStep[playerid] + 1, LIC_THEORY_QUESTIONS);
	ShowPlayerDialog(playerid, DIALOG_LIC_THEORY, DIALOG_STYLE_LIST, gTheoryText[q], "%s", "Ответить", "Сдаться", gTheoryAnswers[q]);
	SendClientMessage(playerid, COLOR_BLUE, "%s: %s", title, gTheoryText[q]);
}

static Licenses_StartTheory(playerid, lic)
{
	// Случайные разные вопросы
	new pool[sizeof (gTheoryText)];
	for (new i = 0; i < sizeof pool; i++) pool[i] = i;
	for (new i = sizeof (pool) - 1; i > 0; i--)
	{
		new j = random(i + 1), t = pool[i];
		pool[i] = pool[j];
		pool[j] = t;
	}
	for (new i = 0; i < LIC_THEORY_QUESTIONS; i++) gExamQuestion[playerid][i] = pool[i];

	gExamType[playerid] = lic;
	gExamStep[playerid] = 0;
	gExamCorrect[playerid] = 0;
	Licenses_AskQuestion(playerid);
}

static Licenses_StartPractice(playerid)
{
	new model = (gExamType[playerid] == LIC_MOTO) ? LIC_MOTO_MODEL : LIC_CAR_MODEL;
	new v = CreateVehicle(model, LIC_CAR_SPAWN_X, LIC_CAR_SPAWN_Y, LIC_CAR_SPAWN_Z, LIC_CAR_SPAWN_A, 1, 1, -1);
	if (v == INVALID_VEHICLE_ID)
	{
		SendClientMessage(playerid, COLOR_RED, "Нет свободной учебной машины, попробуй позже. Деньги возвращены.");
		GiveMoney(playerid, Lic_Price(gExamType[playerid]), "возврат за экзамен");
		gExamType[playerid] = 0;
		return;
	}
	Vehicle_Clear(v);
	gExamVehicle[playerid] = v;
	gExamStep[playerid] = 0;
	gExamTime[playerid] = LIC_EXAM_TIME;
	PutPlayerInVehicle(playerid, v, 0);
	Licenses_ShowCheckpoint(playerid);
	SendClientMessage(playerid, COLOR_BLUE, "Практика: проедь все точки за %d минут и не разбей машину. Заведи двигатель: 2.", LIC_EXAM_TIME / 60);
}

static Licenses_ShowCheckpoint(playerid)
{
	new s = gExamStep[playerid];
	new CP_TYPE:type = (s == gRouteCount - 1) ? CP_TYPE_GROUND_FINISH : CP_TYPE_GROUND_NORMAL;
	new n = (s + 1 < gRouteCount) ? s + 1 : s;
	SetPlayerRaceCheckpoint(playerid, type, gRoute[s][0], gRoute[s][1], gRoute[s][2],
		gRoute[n][0], gRoute[n][1], gRoute[n][2], 5.0);
}

static Licenses_EndExam(playerid, bool:passed, const reason[] = "")
{
	new lic = gExamType[playerid];
	if (lic == 0) return;
	gExamType[playerid] = 0;
	DisablePlayerRaceCheckpoint(playerid);
	new v = gExamVehicle[playerid];
	gExamVehicle[playerid] = INVALID_VEHICLE_ID;
	if (v != INVALID_VEHICLE_ID)
	{
		if (IsPlayerConnected(playerid) && GetPlayerVehicleID(playerid) == v) RemovePlayerFromVehicle(playerid);
		Vehicle_Clear(v);
		DestroyVehicle(v);
	}
	if (!IsPlayerConnected(playerid)) return;

	if (passed)
	{
		Licenses_Set(playerid, lic, true);
		SendClientMessage(playerid, COLOR_GREEN, "Экзамен сдан! Лицензия «%s» получена. Посмотреть: /licenses", gLicNames[lic]);
		if (v != INVALID_VEHICLE_ID) SetPlayerPos(playerid, LIC_SCHOOL_X, LIC_SCHOOL_Y, LIC_SCHOOL_Z);
		Quest_OnLicense(playerid, lic);     // квест новичка (quest.inc)
	}
	else
	{
		if (reason[0] != '\0') SendClientMessage(playerid, COLOR_RED, "Экзамен не сдан: %s. Можно попробовать снова (оплата заново).", reason);
		else SendClientMessage(playerid, COLOR_RED, "Экзамен не сдан. Можно попробовать снова (оплата заново).");
	}
}

stock Licenses_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid == DIALOG_LIC_MENU)
	{
		if (!response || listitem < 0 || listitem >= LIC_COUNT - 1) return 1;
		new lic = listitem + 1;
		if (gExamType[playerid] != 0 || !Licenses_IsNearSchool(playerid)) return 1;
		if (gLic[playerid][lic])
		{
			SendClientMessage(playerid, COLOR_GREY, "У тебя уже есть эта лицензия.");
			return 1;
		}
		if (lic == LIC_GUN && Player[playerid][pLevel] < LIC_GUN_MIN_LEVEL)
		{
			SendClientMessage(playerid, COLOR_GREY, "Лицензию на оружие выдают с %d уровня.", LIC_GUN_MIN_LEVEL);
			return 1;
		}
		if (lic == LIC_GUN && !Hospital_HasMedcard(playerid))
		{
			SendClientMessage(playerid, COLOR_GREY, "Для лицензии на оружие нужна медкарта: регистратура больницы (/gps) или медик.");
			return 1;
		}
		if (GetMoney(playerid) < Lic_Price(lic))
		{
			SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", Lic_Price(lic));
			return 1;
		}
		GiveMoney(playerid, -Lic_Price(lic), "экзамен в автошколе");
		Econ_ToTreasury(Lic_Price(lic));
		Account_Save(playerid);
		Licenses_StartTheory(playerid, lic);
		return 1;
	}
	if (dialogid == DIALOG_LIC_THEORY)
	{
		if (gExamType[playerid] == 0 || gExamVehicle[playerid] != INVALID_VEHICLE_ID) return 1;
		if (!response)
		{
			Licenses_EndExam(playerid, false, "ты отказался");
			return 1;
		}
		if (listitem == gTheoryRight[gExamQuestion[playerid][gExamStep[playerid]]]) gExamCorrect[playerid]++;
		gExamStep[playerid]++;
		if (gExamStep[playerid] < LIC_THEORY_QUESTIONS)
		{
			Licenses_AskQuestion(playerid);
			return 1;
		}
		SendClientMessage(playerid, COLOR_WHITE, "Теория: правильных ответов %d из %d.", gExamCorrect[playerid], LIC_THEORY_QUESTIONS);
		if (gExamCorrect[playerid] < LIC_THEORY_PASS)
		{
			Licenses_EndExam(playerid, false, "много ошибок в теории");
			return 1;
		}
		if (gExamType[playerid] == LIC_CAR || gExamType[playerid] == LIC_MOTO) Licenses_StartPractice(playerid);
		else Licenses_EndExam(playerid, true);
		return 1;
	}
	return 0;
}

// Из OnPlayerEnterRaceCheckpoint
stock Licenses_OnEnterRaceCheckpoint(playerid)
{
	new v = gExamVehicle[playerid];
	if (gExamType[playerid] == 0 || v == INVALID_VEHICLE_ID) return 0;
	if (GetPlayerVehicleID(playerid) != v) return 1;

	gExamStep[playerid]++;
	if (gExamStep[playerid] < gRouteCount)
	{
		PlayerPlaySound(playerid, 1139, 0.0, 0.0, 0.0);
		Licenses_ShowCheckpoint(playerid);
		return 1;
	}
	new Float:hp;
	GetVehicleHealth(v, hp);
	if (hp < LIC_EXAM_MIN_HP) Licenses_EndExam(playerid, false, "машина сильно повреждена");
	else Licenses_EndExam(playerid, true);
	return 1;
}

// Раз в секунду: время экзамена и проверка, что игрок в учебной машине
public Licenses_OnSecond()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gExamVehicle[i] == INVALID_VEHICLE_ID || !IsPlayerConnected(i)) continue;
		// первые секунды клиент ещё садится в машину
		if (gExamTime[i] < LIC_EXAM_TIME - 3 && GetPlayerVehicleID(i) != gExamVehicle[i])
		{
			Licenses_EndExam(i, false, "ты вышел из учебной машины");
			continue;
		}
		if (--gExamTime[i] <= 0) Licenses_EndExam(i, false, "время вышло");
	}
	return 1;
}

// Из OnPlayerDisconnect и OnPlayerDeath
stock Licenses_Abort(playerid)
{
	if (gExamType[playerid] != 0) Licenses_EndExam(playerid, false, "экзамен прерван");
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
static Licenses_Format(playerid, dest[], size = sizeof (dest))
{
	dest[0] = '\0';
	for (new l = 1; l < LIC_COUNT; l++)
	{
		if (!gLic[playerid][l]) continue;
		if (dest[0] != '\0') strcat(dest, ", ", size);
		strcat(dest, gLicNames[l], size);
	}
	if (dest[0] == '\0') strcat(dest, "нет", size);
}

stock bool:Cmd_Licenses(playerid)
{
	new text[64];
	Licenses_Format(playerid, text);
	SendClientMessage(playerid, COLOR_WHITE, "Твои лицензии: %s.", text);
	return true;
}

stock bool:Cmd_ShowLic(playerid, const params[])
{
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /showlic [id]");
	if (!IsPlayerNearPlayer(playerid, target, 5.0)) return SendClientMessage(playerid, COLOR_GREY, "Игрок слишком далеко.");
	new text[64], msg[96];
	Licenses_Format(playerid, text);
	SendClientMessage(target, COLOR_WHITE, "%s показывает тебе лицензии: %s.", GetRPName(playerid), text);
	format(msg, sizeof msg, "%s показывает лицензии %s.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

// Полиция на смене
static bool:Licenses_IsCop(playerid)
{
	return Faction_IsLaw(Player[playerid][pFaction]) && Player[playerid][pDuty];
}

stock bool:Cmd_CheckLic(playerid, const params[])
{
	if (!Licenses_IsCop(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Только для полиции на смене.");
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /checklic [id]");
	if (!IsPlayerNearPlayer(playerid, target, 6.0)) return SendClientMessage(playerid, COLOR_GREY, "Игрок слишком далеко.");
	new text[64];
	Licenses_Format(target, text);
	SendClientMessage(playerid, COLOR_BLUE, "Лицензии %s: %s.", GetRPName(target), text);
	SendClientMessage(target, COLOR_BLUE, "Офицер %s проверил твои лицензии.", GetRPName(playerid));
	return true;
}

stock bool:Cmd_TakeLic(playerid, const params[])
{
	if (!Licenses_IsCop(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Только для полиции на смене.");
	if (Player[playerid][pRank] < LIC_TAKE_MIN_RANK) return SendClientMessage(playerid, COLOR_GREY, "Забирать лицензии можно с %d ранга.", LIC_TAKE_MIN_RANK);
	new pos = 0, lic;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, lic) || lic < 1 || lic >= LIC_COUNT)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /takelic [id] [1 авто, 2 мото, 3 лодки, 4 оружие]");
	if (!IsPlayerNearPlayer(playerid, target, 6.0)) return SendClientMessage(playerid, COLOR_GREY, "Игрок слишком далеко.");
	if (!gLic[target][lic]) return SendClientMessage(playerid, COLOR_GREY, "У игрока нет этой лицензии.");
	Licenses_Set(target, lic, false);
	SendClientMessage(target, COLOR_RED, "Офицер %s забрал у тебя лицензию «%s».", GetRPName(playerid), gLicNames[lic]);
	SendClientMessage(playerid, COLOR_BLUE, "Ты забрал у %s лицензию «%s».", GetRPName(target), gLicNames[lic]);
	return true;
}

stock bool:Cmd_GiveLic(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new pos = 0, lic;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, lic) || lic < 1 || lic >= LIC_COUNT)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /givelic [id] [1 авто, 2 мото, 3 лодки, 4 оружие]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	Licenses_Set(target, lic, true);
	Admin_Log(playerid, "givelic", target, gLicNames[lic], lic);
	SendClientMessage(target, COLOR_ADMIN, "Администратор выдал тебе лицензию «%s».", gLicNames[lic]);
	SendClientMessage(playerid, COLOR_ADMIN, "Лицензия «%s» выдана %s.", gLicNames[lic], GetRPName(target));
	return true;
}

stock bool:Cmd_AddExamPoint(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO exam_route (x, y, z) VALUES (%f, %f, %f)", x, y, z));
	Licenses_LoadRoute();
	new count = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM exam_route");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (count < 2) SendClientMessage(playerid, COLOR_ADMIN, "Точка %d записана. Свой маршрут включится со 2-й точки (пока работает маршрут по умолчанию).", count);
	else SendClientMessage(playerid, COLOR_ADMIN, "Точка %d записана. Маршрут экзамена: %d точек.", count, gRouteCount);
	return true;
}

stock bool:Cmd_ClearExam(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM exam_route"));
	Licenses_LoadRoute();
	SendClientMessage(playerid, COLOR_ADMIN, "Свой маршрут удалён, работает маршрут по умолчанию. Новые точки: /addexampoint");
	return true;
}
