// =====================================================================
//  quest.inc — «Первые шаги»: цепочка заданий для новичка с наградами.
//
//  Новичок получает квест сразу после регистрации, игроки 1-2 уровня
//  могут начать его сами: /quest. На каждом шаге GPS ставит метку
//  к нужному месту. Шаг можно пропустить без награды (/quest).
//
//  Прогресс в accounts.quest_step (0 — не начат, 1..QS_COUNT-1 — шаг,
//  QUEST_DONE — пройден) и accounts.quest_cnt (счётчик заданий на работе).
//  События приходят из модулей: Quest_OnRental, Quest_OnPhone и т.д.
// =====================================================================

#define QUEST_DONE          99
#define QUEST_JOB_TASKS     5       // сколько заданий на работе нужно выполнить
#define QUEST_MAX_LEVEL     2       // до какого уровня можно начать квест через /quest
#define QUEST_FINAL_EXP     1       // опыт за последний шаг

enum
{
	QS_NONE,
	QS_RENTAL,      // взять мопед в прокате
	QS_PHONE,       // купить телефон в 24/7
	QS_FOOD,        // поесть
	QS_LICENSE,     // сдать на права
	QS_HIRE,        // устроиться на работу
	QS_WORK,        // выполнить QUEST_JOB_TASKS заданий
	QS_BANK,        // положить деньги на счёт
	QS_PAYDAY,      // получить зарплату
	QS_COUNT
}

enum E_QUEST_STEP
{
	qsTitle[40],
	qsHint[200],
	qsReward
}

static const gQuestSteps[QS_COUNT][E_QUEST_STEP] =
{
	{ "-", "-", 0 },
	{ "Возьми мопед в прокате",  "Подойди к точке проката (метка GPS) и нажми ALT. Мопед поможет быстро добраться до следующих целей.", 300 },
	{ "Купи телефон",            "Зайди в магазин 24/7 (метка GPS), нажми ALT у кассы и купи телефон. С ним можно звонить: /call, /sms, /911.", 300 },
	{ "Поешь",                   "Голод отнимает здоровье. Купи еду в 24/7 или закусочной и съешь её (еда в сумке: /inv). Сытость: /hunger.", 200 },
	{ "Сдай на права",           "Приезжай в автошколу у мэрии (метка GPS), нажми ALT и сдай экзамен на права категории «авто».", 1000 },
	{ "Устройся на работу",      "Подойди к значку работы (оранжевая футболка, метка GPS) и нажми ALT. Все работы: /gps > Работы.", 300 },
	{ "Поработай",               "Начни смену командой /work и выполни задания. Каждые несколько заданий растёт стаж и надбавка к оплате.", 1500 },
	{ "Положи деньги в банк",    "Подойди к банкомату (метка GPS) или в зал банка, нажми ALT и положи на счёт любую сумму. Зарплата приходит на счёт.", 500 },
	{ "Получи первую зарплату",  "Зарплата (PayDay) приходит в начале каждого часа, если ты в игре. Просто играй и дождись её.", 3000 }
};

static gQuestStep[MAX_PLAYERS];
static gQuestCnt[MAX_PLAYERS];
static bool:gQuestFresh[MAX_PLAYERS];     // только что зарегистрировался

stock Quest_Init()
{
	DB_AddColumn("accounts", "quest_step", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "quest_cnt", "INTEGER DEFAULT 0");
}

static Quest_Save(playerid)
{
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET quest_step = %d, quest_cnt = %d WHERE id = %d",
		gQuestStep[playerid], gQuestCnt[playerid], Player[playerid][pDbId]));
}

static bool:Quest_Active(playerid)
{
	return Player[playerid][pLogged] && gQuestStep[playerid] > QS_NONE && gQuestStep[playerid] < QS_COUNT;
}

// ---------------------------------------------------------------------
//  Куда вести GPS
// ---------------------------------------------------------------------
static Float:Quest_Dist(playerid, m)
{
	return GetPlayerDistanceFromPoint(playerid, gMarker[m][mkX], gMarker[m][mkY], gMarker[m][mkZ]);
}

static bool:Quest_MarkerOk(playerid, m)
{
	return Marker_IsValid(m) && gMarker[m][mkWorld] == 0 && gMarker[m][mkInterior] == 0 && Marker_VisibleTo(playerid, m);
}

// Ближайший маркер этого типа на улице, -1 — нет
static Quest_NearestMarker(playerid, type)
{
	new best = -1, Float:bestD = 999999.0;
	for (new m = 0; m < MAX_MARKERS; m++)
	{
		if (!gMarker[m][mkUsed] || gMarker[m][mkType] != type || !Quest_MarkerOk(playerid, m)) continue;
		new Float:d = Quest_Dist(playerid, m);
		if (d < bestD) { bestD = d; best = m; }
	}
	return best;
}

// Маркер ближайшего бизнеса одного из двух типов, -1 — нет
static Quest_NearestBiz(playerid, type1, type2)
{
	new best = -1, Float:bestD = 999999.0;
	for (new b = 0; b < gBizCount; b++)
	{
		if (Biz[b][bDbId] == 0 || (Biz[b][bType] != type1 && Biz[b][bType] != type2)) continue;
		new m = Biz[b][bMarker];
		if (!Quest_MarkerOk(playerid, m)) continue;
		new Float:d = Quest_Dist(playerid, m);
		if (d < bestD) { bestD = d; best = m; }
	}
	return best;
}

static Quest_SetGps(playerid, bool:quiet = false)
{
	new m = -1;
	switch (gQuestStep[playerid])
	{
		case QS_RENTAL:  m = Quest_NearestMarker(playerid, MK_RENTAL);
		case QS_PHONE:   m = Quest_NearestBiz(playerid, BIZ_SHOP, BIZ_SHOP);
		case QS_FOOD:    m = Quest_NearestBiz(playerid, BIZ_SHOP, BIZ_FASTFOOD);
		case QS_LICENSE:
		{
			Gps_SetPoint(playerid, LIC_SCHOOL_X, LIC_SCHOOL_Y, LIC_SCHOOL_Z, "Автошкола");
			return 1;
		}
		case QS_HIRE:    m = Quest_NearestMarker(playerid, MK_JOB);
		case QS_BANK:    m = Quest_NearestMarker(playerid, MK_ATM);
		default:
		{
			if (!quiet) SendClientMessage(playerid, COLOR_GREY, "Для этого задания метка не нужна.");
			return 0;
		}
	}
	if (m == -1)
	{
		if (!quiet) SendClientMessage(playerid, COLOR_GREY, "Подходящего места на карте пока нет. Можно пропустить шаг: /quest.");
		return 0;
	}
	Gps_SetPoint(playerid, gMarker[m][mkX], gMarker[m][mkY], gMarker[m][mkZ], gMarker[m][mkName]);
	return 1;
}

// ---------------------------------------------------------------------
//  Шаги
// ---------------------------------------------------------------------
// Условие шага уже выполнено раньше (старый игрок начал квест)
static bool:Quest_AlreadyDone(playerid, step)
{
	switch (step)
	{
		case QS_PHONE:   return Player[playerid][pPhone] != 0;
		case QS_LICENSE: return Licenses_Has(playerid, LIC_CAR);
		case QS_HIRE:    return Player[playerid][pJob] != JOB_NONE;
	}
	return false;
}

static Quest_ShowTask(playerid)
{
	new s = gQuestStep[playerid];
	if (s <= QS_NONE || s >= QS_COUNT) return;
	SendClientMessage(playerid, COLOR_YELLOW, "[Квест %d/%d] %s. Награда: $%d.", s, QS_COUNT - 1, gQuestSteps[s][qsTitle], gQuestSteps[s][qsReward]);
	SendClientMessage(playerid, COLOR_WHITE, "%s", gQuestSteps[s][qsHint]);
	if (s == QS_WORK) SendClientMessage(playerid, COLOR_WHITE, "Выполнено заданий: %d/%d.", gQuestCnt[playerid], QUEST_JOB_TASKS);
}

// Перейти к шагу step (пропуская уже выполненные без награды)
static Quest_GoTo(playerid, step)
{
	while (step < QS_COUNT && Quest_AlreadyDone(playerid, step))
	{
		SendClientMessage(playerid, COLOR_GREY, "[Квест] «%s» у тебя уже есть, шаг засчитан.", gQuestSteps[step][qsTitle]);
		step++;
	}
	gQuestCnt[playerid] = 0;
	if (step >= QS_COUNT)
	{
		gQuestStep[playerid] = QUEST_DONE;
		Quest_Save(playerid);
		SendClientMessage(playerid, COLOR_GREEN, "[Квест] «Первые шаги» пройден! Дальше всё в твоих руках: дома, бизнесы, фракции (/help).");
		return;
	}
	gQuestStep[playerid] = step;
	Quest_Save(playerid);
	Quest_ShowTask(playerid);
	Quest_SetGps(playerid, true);
}

// Шаг выполнен: награда и следующий
static Quest_Complete(playerid, step)
{
	if (!Quest_Active(playerid) || gQuestStep[playerid] != step) return;
	new reward = gQuestSteps[step][qsReward];
	GiveMoney(playerid, reward, "квест новичка");
	PlayerPlaySound(playerid, 1058, 0.0, 0.0, 0.0);
	GameTextForPlayer(playerid, "~g~Quest +$%d", 3000, 1, reward);
	SendClientMessage(playerid, COLOR_GREEN, "[Квест] Выполнено: %s. Награда: $%d.", gQuestSteps[step][qsTitle], reward);
	if (step == QS_PAYDAY) Player[playerid][pExp] += QUEST_FINAL_EXP;
	Account_Save(playerid);
	Quest_GoTo(playerid, step + 1);
}

// ---------------------------------------------------------------------
//  Вход и регистрация
// ---------------------------------------------------------------------
// Из Account_FinishLogin
stock Quest_OnLogin(playerid)
{
	gQuestStep[playerid] = QS_NONE;
	gQuestCnt[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT quest_step, quest_cnt FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			gQuestStep[playerid] = DB_GetFieldInt(r, 0);
			gQuestCnt[playerid] = DB_GetFieldInt(r, 1);
		}
		DB_FreeResultSet(r);
	}
	if (gQuestFresh[playerid])
	{
		// Новичок: квест стартует сам, окно с первым заданием — после спавна
		gQuestFresh[playerid] = false;
		gQuestStep[playerid] = QS_RENTAL;
		gQuestCnt[playerid] = 0;
		Quest_Save(playerid);
		SetTimerEx("Quest_Intro", 3000, false, "i", playerid);
		return;
	}
	if (Quest_Active(playerid))
		SendClientMessage(playerid, COLOR_YELLOW, "[Квест %d/%d] %s. Подробнее и метка GPS: /quest", gQuestStep[playerid], QS_COUNT - 1, gQuestSteps[gQuestStep[playerid]][qsTitle]);
	else if (gQuestStep[playerid] == QS_NONE && Player[playerid][pLevel] <= QUEST_MAX_LEVEL)
		SendClientMessage(playerid, COLOR_YELLOW, "Новичок? Пройди задания «Первые шаги» и получи до $7100: /quest");
}

forward Quest_Intro(playerid);
public Quest_Intro(playerid)
{
	if (!IsPlayerConnected(playerid) || !Quest_Active(playerid)) return 0;
	new s = gQuestStep[playerid];
	new text[900];
	format(text, sizeof text, "{FFFFFF}Добро пожаловать в Bastion!\n\nПройди {FFFF00}%d коротких заданий{FFFFFF}: они покажут город, работу и банк.\nЗа каждое — деньги, всего до {33AA33}$7100{FFFFFF}.\n\n{FFFF00}Первое задание: %s.{FFFFFF}\n%s\n\n{AAAAAA}Метка стоит на карте. Список заданий и пропуск шага: /quest",
		QS_COUNT - 1, gQuestSteps[s][qsTitle], gQuestSteps[s][qsHint]);
	ShowPlayerDialog(playerid, DIALOG_QUEST_INTRO, DIALOG_STYLE_MSGBOX, "Первые шаги", "%s", "Понятно", "", text);
	Quest_SetGps(playerid, true);
	return 1;
}

// После выбора пола при регистрации (accounts.inc), до Account_FinishLogin
stock Quest_OnRegister(playerid)
{
	gQuestFresh[playerid] = true;
}

// ---------------------------------------------------------------------
//  События из других модулей
// ---------------------------------------------------------------------
stock Quest_OnRental(playerid)  { Quest_Complete(playerid, QS_RENTAL); }
stock Quest_OnPhone(playerid)   { Quest_Complete(playerid, QS_PHONE); }
stock Quest_OnEat(playerid)     { Quest_Complete(playerid, QS_FOOD); }
stock Quest_OnHire(playerid)    { Quest_Complete(playerid, QS_HIRE); }
stock Quest_OnDeposit(playerid) { Quest_Complete(playerid, QS_BANK); }
stock Quest_OnPayDay(playerid)  { Quest_Complete(playerid, QS_PAYDAY); }

stock Quest_OnLicense(playerid, lic)
{
	if (lic == LIC_CAR) Quest_Complete(playerid, QS_LICENSE);
}

// Из Job_Pay: выполнено задание на работе
stock Quest_OnJobTask(playerid)
{
	if (!Quest_Active(playerid) || gQuestStep[playerid] != QS_WORK) return;
	gQuestCnt[playerid]++;
	if (gQuestCnt[playerid] >= QUEST_JOB_TASKS)
	{
		Quest_Complete(playerid, QS_WORK);
		return;
	}
	Quest_Save(playerid);
	SendClientMessage(playerid, COLOR_YELLOW, "[Квест] Заданий на работе: %d/%d.", gQuestCnt[playerid], QUEST_JOB_TASKS);
}

// ---------------------------------------------------------------------
//  /quest
// ---------------------------------------------------------------------
stock bool:Cmd_Quest(playerid)
{
	if (gQuestStep[playerid] == QUEST_DONE)
		return SendClientMessage(playerid, COLOR_GREY, "Ты уже прошёл задания «Первые шаги».");
	if (gQuestStep[playerid] == QS_NONE || gQuestStep[playerid] >= QS_COUNT)
	{
		if (Player[playerid][pLevel] > QUEST_MAX_LEVEL)
			return SendClientMessage(playerid, COLOR_GREY, "Задания «Первые шаги» доступны до %d уровня.", QUEST_MAX_LEVEL);
		SendClientMessage(playerid, COLOR_GREEN, "[Квест] Ты начал задания «Первые шаги».");
		Quest_GoTo(playerid, QS_RENTAL);
		return true;
	}
	new s = gQuestStep[playerid];
	new text[1100], line[160];
	for (new i = 1; i < QS_COUNT; i++)
	{
		if (i < s) format(line, sizeof line, "{33AA33}+ %s{AAAAAA} ($%d)\n", gQuestSteps[i][qsTitle], gQuestSteps[i][qsReward]);
		else if (i == s) format(line, sizeof line, "{FFFF00}> %s{FFFFFF} ($%d)\n", gQuestSteps[i][qsTitle], gQuestSteps[i][qsReward]);
		else format(line, sizeof line, "{AAAAAA}  %s ($%d)\n", gQuestSteps[i][qsTitle], gQuestSteps[i][qsReward]);
		strcat(text, line);
	}
	format(line, sizeof line, "\n{FFFFFF}Сейчас: {FFFF00}%s\n{FFFFFF}", gQuestSteps[s][qsTitle]);
	strcat(text, line);
	strcat(text, gQuestSteps[s][qsHint]);
	if (s == QS_WORK)
	{
		format(line, sizeof line, "\nВыполнено: %d/%d.", gQuestCnt[playerid], QUEST_JOB_TASKS);
		strcat(text, line);
	}
	ShowPlayerDialog(playerid, DIALOG_QUEST, DIALOG_STYLE_MSGBOX, "Первые шаги", "%s", "Метка GPS", "Ещё", text);
	return true;
}

stock Quest_OnDialog(playerid, dialogid, response, listitem)
{
	switch (dialogid)
	{
		case DIALOG_QUEST_INTRO: return 1;
		case DIALOG_QUEST:
		{
			if (response) { Quest_SetGps(playerid); return 1; }
			ShowPlayerDialog(playerid, DIALOG_QUEST_MORE, DIALOG_STYLE_LIST, "Первые шаги",
				"Пропустить это задание (без награды)\nОтказаться от всех заданий", "Выбрать", "Назад");
			return 1;
		}
		case DIALOG_QUEST_MORE:
		{
			if (!response) { Cmd_Quest(playerid); return 1; }
			if (!Quest_Active(playerid)) return 1;
			if (listitem == 0)
			{
				SendClientMessage(playerid, COLOR_GREY, "[Квест] Задание «%s» пропущено.", gQuestSteps[gQuestStep[playerid]][qsTitle]);
				Quest_GoTo(playerid, gQuestStep[playerid] + 1);
			}
			else if (listitem == 1)
			{
				gQuestStep[playerid] = QUEST_DONE;
				gQuestCnt[playerid] = 0;
				Quest_Save(playerid);
				Gps_Clear(playerid);
				SendClientMessage(playerid, COLOR_GREY, "[Квест] Ты отказался от заданий «Первые шаги».");
			}
			return 1;
		}
	}
	return 0;
}
