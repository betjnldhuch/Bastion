// =====================================================================
//  quest.inc — «Первые шаги»: цепочка заданий для новичка с наградами.
//
//  Новичок получает квест сразу после регистрации, игроки 1-2 уровня
//  могут начать его сами: /quest. На каждом шаге GPS ставит метку
//  к нужному месту. Шаг можно пропустить без награды (/quest).
//
//  Цепочка (gQuestOrder): паспорт в мэрии > мопед в прокате > телефон >
//  еда > права > работа > задания на работе > банк > зарплата > свой транспорт.
//
//  Прогресс в accounts.quest_step (id шага из enum, 0 — не начат,
//  QUEST_DONE — пройден) и accounts.quest_cnt (счётчик заданий на работе).
//  Id шагов не меняются (они в базе), порядок задаёт только gQuestOrder —
//  новый шаг = новый id в конце enum + место в gQuestOrder.
//  События приходят из модулей: Quest_OnRental, Quest_OnPhone и т.д.
//
//  Окно: HTML «quest» в клиенте Bastion (index.html, ПК и APK), иначе диалог.
// =====================================================================

#define QUEST_DONE          99
#define QUEST_JOB_TASKS     5       // сколько заданий на работе нужно выполнить
#define QUEST_MAX_LEVEL     2       // до какого уровня можно начать квест через /quest
#define QUEST_FINAL_EXP     1       // опыт за последний шаг

// Паспортный стол в мэрии (docs.inc, по умолчанию у автошколы). docs.inc подключается
// позже, поэтому метка GPS — по точке стола по умолчанию.
#define QUEST_HALL_X        1477.6
#define QUEST_HALL_Y        -1771.3
#define QUEST_HALL_Z        18.8
// Вход в автосалон «Эконом» (Coutt & Schutz)
#define QUEST_SALON_X       2131.7
#define QUEST_SALON_Y       -1150.4
#define QUEST_SALON_Z       24.1

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
	QS_PASSPORT,    // сфотографироваться в мэрии и получить паспорт (docs.inc зовёт Quest_OnPassport)
	QS_VEHICLE,     // купить свой транспорт в автосалоне
	QS_COUNT
}

// Порядок шагов в квесте
static const gQuestOrder[] = { QS_PASSPORT, QS_RENTAL, QS_PHONE, QS_FOOD, QS_LICENSE, QS_HIRE, QS_WORK, QS_BANK, QS_PAYDAY, QS_VEHICLE };

enum E_QUEST_STEP
{
	qsKey[12],      // картинка шага в окне: ui/quest/<key>.png
	qsTitle[40],
	qsHint[200],
	qsReward
}

static const gQuestSteps[QS_COUNT][E_QUEST_STEP] =
{
	{ "", "-", "-", 0 },
	{ "rental",   "Возьми мопед в прокате",  "Подойди к точке проката (метка GPS) и нажми ALT. Мопед поможет быстро добраться до следующих целей.", 300 },
	{ "phone",    "Купи телефон",            "Зайди в магазин 24/7 (метка GPS), нажми ALT у кассы и купи телефон. В нём карта, банк, такси и объявления.", 300 },
	{ "food",     "Поешь",                   "Голод отнимает здоровье. Купи еду в 24/7 или закусочной и съешь её (еда в сумке: /inv). Сытость: /hunger.", 200 },
	{ "license",  "Сдай на права",           "Приезжай в автошколу у мэрии (метка GPS), нажми ALT и сдай экзамен на права категории «авто».", 1000 },
	{ "hire",     "Устройся на работу",      "Подойди к значку работы (оранжевая футболка, метка GPS) и нажми ALT. Все работы: /gps > Работы.", 300 },
	{ "work",     "Поработай",               "Начни смену командой /work и выполни задания. Каждые несколько заданий растёт стаж и надбавка к оплате.", 1500 },
	{ "bank",     "Положи деньги в банк",    "Подойди к банкомату (метка GPS) или в зал банка, нажми ALT и положи на счёт любую сумму. Зарплата приходит на счёт.", 500 },
	{ "payday",   "Получи первую зарплату",  "Зарплата (PayDay) приходит в начале каждого часа, если ты в игре. Просто играй и дождись её.", 3000 },
	{ "passport", "Получи паспорт",          "Приди в мэрию (метка GPS), подойди к паспортному столу, сфотографируйся и получи паспорт. Показать его: /showpass.", 300 },
	{ "vehicle",  "Купи свой транспорт",     "Приезжай в автосалон «Эконом» (метка GPS) и купи машину или мопед: /buycar. Faggio стоит $3000. Свои машины: /cars.", 1500 }
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

// Номер шага в цепочке (0..), -1 — шага нет в цепочке
static Quest_Pos(step)
{
	for (new i = 0; i < sizeof gQuestOrder; i++) if (gQuestOrder[i] == step) return i;
	return -1;
}

// Сумма наград за все шаги
static Quest_TotalReward()
{
	new sum = 0;
	for (new i = 0; i < sizeof gQuestOrder; i++) sum += gQuestSteps[gQuestOrder[i]][qsReward];
	return sum;
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
		case QS_PASSPORT:
		{
			Gps_SetPoint(playerid, QUEST_HALL_X, QUEST_HALL_Y, QUEST_HALL_Z, "Мэрия");
			return 1;
		}
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
		case QS_VEHICLE:
		{
			Gps_SetPoint(playerid, QUEST_SALON_X, QUEST_SALON_Y, QUEST_SALON_Z, "Автосалон «Эконом»");
			return 1;
		}
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
		case QS_PASSPORT: return Docs_Has(playerid, INV_PASSPORT) != 0;
		case QS_PHONE:    return Player[playerid][pPhone] != 0;
		case QS_LICENSE:  return Licenses_Has(playerid, LIC_CAR);
		case QS_HIRE:     return Player[playerid][pJob] != JOB_NONE;
		case QS_VEHICLE:  return Vehicles_CountOwned(playerid) > 0;
	}
	return false;
}

static Quest_ShowTask(playerid)
{
	new s = gQuestStep[playerid], p = Quest_Pos(s);
	if (p == -1) return;
	SendClientMessage(playerid, COLOR_YELLOW, "[Квест %d/%d] %s. Награда: $%d.", p + 1, sizeof gQuestOrder, gQuestSteps[s][qsTitle], gQuestSteps[s][qsReward]);
	SendClientMessage(playerid, COLOR_WHITE, "%s", gQuestSteps[s][qsHint]);
	if (s == QS_WORK) SendClientMessage(playerid, COLOR_WHITE, "Выполнено заданий: %d/%d.", gQuestCnt[playerid], QUEST_JOB_TASKS);
}

// Перейти к шагу номер pos в цепочке (пропуская уже выполненные без награды)
static Quest_GoTo(playerid, pos)
{
	while (pos < sizeof gQuestOrder && Quest_AlreadyDone(playerid, gQuestOrder[pos]))
	{
		SendClientMessage(playerid, COLOR_GREY, "[Квест] «%s» у тебя уже есть, шаг засчитан.", gQuestSteps[gQuestOrder[pos]][qsTitle]);
		pos++;
	}
	gQuestCnt[playerid] = 0;
	if (pos >= sizeof gQuestOrder)
	{
		gQuestStep[playerid] = QUEST_DONE;
		Quest_Save(playerid);
		Gps_Clear(playerid);
		SendClientMessage(playerid, COLOR_GREEN, "[Квест] «Первые шаги» пройден! Дальше всё в твоих руках: дома, бизнесы, фракции (/help).");
		return;
	}
	gQuestStep[playerid] = gQuestOrder[pos];
	Quest_Save(playerid);
	Quest_ShowTask(playerid);
	Quest_SetGps(playerid, true);
}

// Следующий шаг после step
static Quest_Next(playerid, step)
{
	new p = Quest_Pos(step);
	Quest_GoTo(playerid, (p == -1) ? sizeof gQuestOrder : p + 1);
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
	if (step == gQuestOrder[sizeof gQuestOrder - 1])
	{
		Player[playerid][pExp] += QUEST_FINAL_EXP;
		SendClientMessage(playerid, COLOR_GREEN, "[Квест] За прохождение всей цепочки: +%d опыта.", QUEST_FINAL_EXP);
	}
	Account_Save(playerid);
	Quest_Next(playerid, step);
}

// ---------------------------------------------------------------------
//  Окно (HTML «quest» или диалог)
// ---------------------------------------------------------------------
// intro — окно приветствия после регистрации (одна кнопка)
static Quest_ShowWindow(playerid, bool:intro)
{
	new s = gQuestStep[playerid], p = Quest_Pos(s);
	if (p == -1) return;
	if (UI_Has(playerid))
	{
		new json[3200], item[300], hint[220];
		format(json, sizeof json, "{\"intro\":%d,\"cur\":%d,\"cnt\":%d,\"need\":%d,\"sum\":%d,\"steps\":[",
			intro ? 1 : 0, p, gQuestCnt[playerid], (s == QS_WORK) ? QUEST_JOB_TASKS : 0, Quest_TotalReward());
		for (new i = 0; i < sizeof gQuestOrder; i++)
		{
			new q = gQuestOrder[i];
			UI_JsonEscape(hint, gQuestSteps[q][qsHint]);
			format(item, sizeof item, "%s[\"%s\",\"%s\",\"%s\",%d]", i ? "," : "", gQuestSteps[q][qsKey], gQuestSteps[q][qsTitle], hint, gQuestSteps[q][qsReward]);
			strcat(json, item);
		}
		strcat(json, "]}");
		// Ответ (стиль LIST): 1/0 — метка GPS, 1/1 — пропустить шаг, 1/2 — отказаться; 0 — закрыть
		UI_Show(playerid, DIALOG_QUEST_UI, DIALOG_STYLE_LIST, "quest", json, intro ? "Понятно" : "Метка GPS", "Закрыть");
		return;
	}
	if (intro)
	{
		new text[900];
		format(text, sizeof text, "{FFFFFF}Добро пожаловать в Bastion!\n\nПройди {FFFF00}%d коротких заданий{FFFFFF}: они покажут город, работу и банк.\nЗа каждое — деньги, всего до {33AA33}$%d{FFFFFF}.\n\n{FFFF00}Первое задание: %s.{FFFFFF}\n%s\n\n{AAAAAA}Метка стоит на карте. Список заданий и пропуск шага: /quest",
			sizeof gQuestOrder, Quest_TotalReward(), gQuestSteps[s][qsTitle], gQuestSteps[s][qsHint]);
		ShowPlayerDialog(playerid, DIALOG_QUEST_INTRO, DIALOG_STYLE_MSGBOX, "Первые шаги", "%s", "Понятно", "", text);
		return;
	}
	new text[1400], line[160];
	for (new i = 0; i < sizeof gQuestOrder; i++)
	{
		new q = gQuestOrder[i];
		if (i < p) format(line, sizeof line, "{33AA33}+ %s{AAAAAA} ($%d)\n", gQuestSteps[q][qsTitle], gQuestSteps[q][qsReward]);
		else if (i == p) format(line, sizeof line, "{FFFF00}> %s{FFFFFF} ($%d)\n", gQuestSteps[q][qsTitle], gQuestSteps[q][qsReward]);
		else format(line, sizeof line, "{AAAAAA}  %s ($%d)\n", gQuestSteps[q][qsTitle], gQuestSteps[q][qsReward]);
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
}

static Quest_Skip(playerid)
{
	if (!Quest_Active(playerid)) return;
	new s = gQuestStep[playerid];
	SendClientMessage(playerid, COLOR_GREY, "[Квест] Задание «%s» пропущено.", gQuestSteps[s][qsTitle]);
	Quest_Next(playerid, s);
}

static Quest_Quit(playerid)
{
	if (!Quest_Active(playerid)) return;
	gQuestStep[playerid] = QUEST_DONE;
	gQuestCnt[playerid] = 0;
	Quest_Save(playerid);
	Gps_Clear(playerid);
	SendClientMessage(playerid, COLOR_GREY, "[Квест] Ты отказался от заданий «Первые шаги».");
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
		// Новичок: квест стартует сам (с первого шага цепочки), окно с заданием — после спавна
		gQuestFresh[playerid] = false;
		gQuestStep[playerid] = gQuestOrder[0];
		gQuestCnt[playerid] = 0;
		Quest_Save(playerid);
		SetTimerEx("Quest_Intro", 3000, false, "i", playerid);
		return;
	}
	if (Quest_Active(playerid))
	{
		new p = Quest_Pos(gQuestStep[playerid]);
		if (p == -1) { gQuestStep[playerid] = QUEST_DONE; Quest_Save(playerid); return; }
		SendClientMessage(playerid, COLOR_YELLOW, "[Квест %d/%d] %s. Подробнее и метка GPS: /quest", p + 1, sizeof gQuestOrder, gQuestSteps[gQuestStep[playerid]][qsTitle]);
		SetTimerEx("Quest_Resume", 4000, false, "i", playerid);
	}
	else if (gQuestStep[playerid] == QS_NONE && Player[playerid][pLevel] <= QUEST_MAX_LEVEL)
		SendClientMessage(playerid, COLOR_YELLOW, "Новичок? Пройди задания «Первые шаги» и получи до $%d: /quest", Quest_TotalReward());
}

forward Quest_Intro(playerid);
public Quest_Intro(playerid)
{
	if (!IsPlayerConnected(playerid) || !Quest_Active(playerid)) return 0;
	// первый шаг мог быть выполнен заранее (например, паспорт уже выдан)
	if (Quest_AlreadyDone(playerid, gQuestStep[playerid])) Quest_GoTo(playerid, Quest_Pos(gQuestStep[playerid]));
	if (!Quest_Active(playerid)) return 0;
	Quest_ShowWindow(playerid, true);
	Quest_SetGps(playerid, true);
	return 1;
}

// После входа: метка GPS к текущему заданию; шаг, выполненный не по квесту, засчитывается
forward Quest_Resume(playerid);
public Quest_Resume(playerid)
{
	if (!IsPlayerConnected(playerid) || !Quest_Active(playerid)) return 0;
	if (Quest_AlreadyDone(playerid, gQuestStep[playerid])) Quest_GoTo(playerid, Quest_Pos(gQuestStep[playerid]));
	else Quest_SetGps(playerid, true);
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
stock Quest_OnPassport(playerid) { Quest_Complete(playerid, QS_PASSPORT); }
stock Quest_OnRental(playerid)  { Quest_Complete(playerid, QS_RENTAL); }
stock Quest_OnPhone(playerid)   { Quest_Complete(playerid, QS_PHONE); }
stock Quest_OnEat(playerid)     { Quest_Complete(playerid, QS_FOOD); }
stock Quest_OnHire(playerid)    { Quest_Complete(playerid, QS_HIRE); }
stock Quest_OnDeposit(playerid) { Quest_Complete(playerid, QS_BANK); }
stock Quest_OnPayDay(playerid)  { Quest_Complete(playerid, QS_PAYDAY); }
stock Quest_OnVehicle(playerid) { Quest_Complete(playerid, QS_VEHICLE); }

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
	if (!Quest_Active(playerid))
	{
		if (Player[playerid][pLevel] > QUEST_MAX_LEVEL)
			return SendClientMessage(playerid, COLOR_GREY, "Задания «Первые шаги» доступны до %d уровня.", QUEST_MAX_LEVEL);
		SendClientMessage(playerid, COLOR_GREEN, "[Квест] Ты начал задания «Первые шаги».");
		Quest_GoTo(playerid, 0);
		if (Quest_Active(playerid)) Quest_ShowWindow(playerid, false);
		return true;
	}
	// шаг выполнен не по квесту (например, паспорт выдали раньше) — засчитать и показать следующий
	if (Quest_AlreadyDone(playerid, gQuestStep[playerid]))
	{
		Quest_GoTo(playerid, Quest_Pos(gQuestStep[playerid]));
		if (!Quest_Active(playerid)) return true;
	}
	Quest_ShowWindow(playerid, false);
	return true;
}

stock Quest_OnDialog(playerid, dialogid, response, listitem)
{
	switch (dialogid)
	{
		case DIALOG_QUEST_INTRO: return 1;
		case DIALOG_QUEST_UI:
		{
			if (!response) return 1;
			switch (listitem)
			{
				case 1: Quest_Skip(playerid);
				case 2: Quest_Quit(playerid);
				default: Quest_SetGps(playerid);
			}
			return 1;
		}
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
			if (listitem == 0) Quest_Skip(playerid);
			else if (listitem == 1) Quest_Quit(playerid);
			return 1;
		}
	}
	return 0;
}
