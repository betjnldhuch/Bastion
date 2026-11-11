// =====================================================================
//  law.inc — закон: метка задержания, дверь участка, взятка, побег.
//
//  Метка задержания: /mark [id] ставит полицейский на смене. Метка висит
//  MARK_SECONDS. Если игрок с меткой вышел из игры (сам или «вылетел») —
//  при следующем входе он попадает в деморган: MARK_AJAIL_PER_STAR минут
//  за каждую звезду розыска, но не меньше MARK_AJAIL_MIN.
//  Наручники меткой не считаются. Повторный /mark снимает метку.
//
//  Дверь участка LSPD для заключённых заперта. Открыть её может
//  полицейский на смене: /celldoor (на CELLDOOR_SECONDS секунд).
//  Кто из заключённых успел выйти — сбежал: срок снят, но розыск
//  +ESCAPE_STARS звёзд и сообщение всей полиции. Кто открыл — пишется
//  в журнал фракции (/lmenu > Журнал).
//
//  Взятка: заключённый пишет /bribe [id] [сумма] полицейскому рядом.
//  Если тот принял (Y или /accept) — деньги переходят ему, а заключённый
//  в течение BRIBE_PASS_SECONDS может тихо выйти через дверь: срок снят,
//  розыска нет. Передача денег видна админам в журнале денег.
// =====================================================================

#define MARK_SECONDS            600     // сколько висит метка задержания
#define MARK_RANGE              30.0    // с какого расстояния её можно поставить
#define MARK_AJAIL_PER_STAR     10      // минут деморгана за звезду розыска
#define MARK_AJAIL_MIN          10      // и не меньше этого
#define CELLDOOR_SECONDS        30      // на сколько /celldoor открывает дверь
#define ESCAPE_STARS            3       // розыск за побег
#define BRIBE_MIN               500
#define BRIBE_MAX               1000000
#define BRIBE_PASS_SECONDS      120     // сколько времени на выход после взятки

static gMarkUntil[MAX_PLAYERS];         // до какого времени висит метка (0 — нет)
static gJailPass[MAX_PLAYERS];          // до какого времени можно тихо выйти (взятка)
static gCellOpenUntil;                  // до какого времени открыта дверь участка

stock Law_OnConnect(playerid)
{
	gMarkUntil[playerid] = 0;
	gJailPass[playerid] = 0;
}

stock Law_ClearMark(playerid)
{
	gMarkUntil[playerid] = 0;
}

// Из OnPlayerDisconnect — первой строкой, пока аккаунт ещё загружен.
// reason: 0 — вылет, 1 — вышел сам, 2 — кик или бан.
stock Law_OnDisconnect(playerid, reason)
{
	new bool:marked = (gMarkUntil[playerid] > gettime());
	gMarkUntil[playerid] = 0;
	gJailPass[playerid] = 0;
	if (!marked || reason == 2 || !Player[playerid][pLogged] || Player[playerid][pJail] > 0) return;

	new minutes = Player[playerid][pWanted] * MARK_AJAIL_PER_STAR;
	if (minutes < MARK_AJAIL_MIN) minutes = MARK_AJAIL_MIN;
	Admin_AddAJail(playerid, minutes);

	new name[MAX_PLAYER_NAME], msg[144];
	GetPlayerName(playerid, name, sizeof name);
	Admin_LogName("Сервер", 0, "ajail", name, Player[playerid][pDbId], "выход из игры с меткой задержания", minutes);
	format(msg, sizeof msg, "[Диспетчер] %s вышел из игры с меткой задержания. Деморган: %d мин.", GetRPName(playerid), minutes);
	Faction_Message(FACTION_POLICE, Faction_GetColor(FACTION_POLICE), msg);
}

// /mark [id] — поставить или снять метку задержания
stock bool:Cmd_Mark(playerid, const params[])
{
	if (!Faction_Need(playerid, FACTION_POLICE, true)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /mark [id] — метка задержания (ещё раз — снять)");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");

	new msg[144];
	if (gMarkUntil[target] > gettime())
	{
		gMarkUntil[target] = 0;
		SendClientMessage(target, COLOR_GREEN, "Метка задержания снята.");
		format(msg, sizeof msg, "[Диспетчер] %s снял метку задержания с %s.", GetRPName(playerid), GetRPName(target));
		Faction_Message(FACTION_POLICE, Faction_GetColor(FACTION_POLICE), msg);
		return true;
	}
	if (Player[target][pJail] > 0) return SendClientMessage(playerid, COLOR_GREY, "Этот человек уже в тюрьме.");
	if (Player[target][pFaction] == FACTION_POLICE && Player[target][pDuty])
		return SendClientMessage(playerid, COLOR_GREY, "Нельзя поставить метку полицейскому на смене.");
	if (!IsPlayerNearPlayer(playerid, target, MARK_RANGE)) return SendClientMessage(playerid, COLOR_GREY, "Игрок слишком далеко.");

	gMarkUntil[target] = gettime() + MARK_SECONDS;
	SendClientMessage(target, COLOR_RED, "Полиция поставила на тебя метку задержания на %d мин. Выйдешь из игры — попадёшь в деморган.", MARK_SECONDS / 60);
	format(msg, sizeof msg, "[Диспетчер] %s поставил метку задержания на %s [%d] (%d мин).", GetRPName(playerid), GetRPName(target), target, MARK_SECONDS / 60);
	Faction_Message(FACTION_POLICE, Faction_GetColor(FACTION_POLICE), msg);
	return true;
}

// Три двери камер: каждая отъезжает вбок вдоль решётки и возвращается.
// Места — из игры (/pos): где стоит закрытая дверь и куда она уезжает.
// Модель 19302 — решётчатая дверь камеры (не ворота!). Дверь стоит вдоль оси Y.
#define CELL_BARS           3
#define CELL_SPEED          1.2     // скорость движения двери
#define CELL_CENTER_X       266.4   // середина ряда камер: отсюда проверяется, стоит ли полицейский рядом
#define CELL_CENTER_Y       82.6
#define CELL_CENTER_Z       1001.03
#define CELL_USE_RANGE      6.0     // ALT у дверей
#define CELL_GATE_MODEL     19302
#define CELL_GATE_X         266.30  // линия решёток
#define CELL_GATE_Z         1001.28 // середина двери по высоте (игрок стоит на 1001.03)
#define CELL_GATE_ROT       90.0

// y закрытой и открытой двери; x и z у всех одни (с /pos снят игрок, а не объект)
static const Float:gCellClosed[CELL_BARS][4] =
{
	{ CELL_GATE_X, 87.89, CELL_GATE_Z, CELL_GATE_ROT },   // у входа
	{ CELL_GATE_X, 83.20, CELL_GATE_Z, CELL_GATE_ROT },   // в центре
	{ CELL_GATE_X, 76.82, CELL_GATE_Z, CELL_GATE_ROT }    // у стены
};
static const Float:gCellOpen[CELL_BARS][3] =
{
	{ CELL_GATE_X, 86.44, CELL_GATE_Z },
	{ CELL_GATE_X, 81.55, CELL_GATE_Z },
	{ CELL_GATE_X, 78.65, CELL_GATE_Z }
};

static gCellObj[CELL_BARS] = { -1, -1, -1 };
static gCellSerial = 0;                  // номер открытия: старый таймер не закроет новое

stock Law_Init()
{
	for (new i = 0; i < CELL_BARS; i++)
	{
		gCellObj[i] = CreateObject(CELL_GATE_MODEL, gCellClosed[i][0], gCellClosed[i][1], gCellClosed[i][2], 0.0, 0.0, gCellClosed[i][3], 150.0);
	}
}

stock bool:Law_NearCellGate(playerid, Float:range)
{
	return (GetPlayerInterior(playerid) == JAIL_INTERIOR)
		&& IsPlayerInRangeOfPoint(playerid, range, CELL_CENTER_X, CELL_CENTER_Y, CELL_CENTER_Z);
}

// Открыть все три или закрыть все три (объекты едут в свои места)
static Law_SetCellGate(bool:open)
{
	for (new i = 0; i < CELL_BARS; i++)
	{
		if (gCellObj[i] == -1) continue;
		if (open) MoveObject(gCellObj[i], gCellOpen[i][0], gCellOpen[i][1], gCellOpen[i][2], CELL_SPEED);
		else MoveObject(gCellObj[i], gCellClosed[i][0], gCellClosed[i][1], gCellClosed[i][2], CELL_SPEED);
	}
	if (open)
	{
		gCellSerial++;
		gCellOpenUntil = gettime() + CELLDOOR_SECONDS;
		SetTimerEx("OnCellGateClose", CELLDOOR_SECONDS * 1000, false, "i", gCellSerial);
	}
	else gCellOpenUntil = 0;
}

forward OnCellGateClose(serial);
public OnCellGateClose(serial)
{
	if (serial == gCellSerial && gCellOpenUntil > 0) Law_SetCellGate(false);
	return 1;
}

// Полицейский на смене у решётки: жмёт ALT или вводит /celldoor — решётка открывается или закрывается
static Law_ToggleCellGate(playerid)
{
	new msg[144];
	new bool:opening = (gCellOpenUntil <= gettime());
	Law_SetCellGate(opening);
	ApplyAnimation(playerid, "BOMBER", "BOM_Plant_In", 4.1, false, false, false, false, 0, SYNC_ALL);
	if (opening)
	{
		format(msg, sizeof msg, "%s поворачивает ключ и открывает решётку камеры.", GetRPName(playerid));
		SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
		format(msg, sizeof msg, "%s открыл решётку камеры для заключённых на %d сек.", GetRPName(playerid), CELLDOOR_SECONDS);
		Faction_Log(FACTION_POLICE, msg);
	}
	else
	{
		format(msg, sizeof msg, "%s закрывает решётку камеры.", GetRPName(playerid));
		SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	}
	return opening;
}

// Из Doors_Try (doors.inc): ALT у двери участка рядом с решёткой. true — обработано.
stock bool:Law_CellGateUse(playerid)
{
	if (Player[playerid][pFaction] != FACTION_POLICE || !Player[playerid][pDuty]) return false;
	if (!Law_NearCellGate(playerid, CELL_USE_RANGE)) return false;
	Law_ToggleCellGate(playerid);
	return true;
}

// /celldoor — полицейский на смене открывает или закрывает решётку камеры
stock bool:Cmd_CellDoor(playerid)
{
	if (!Faction_Need(playerid, FACTION_POLICE, true)) return true;
	if (!Law_NearCellGate(playerid, 12.0))
		return SendClientMessage(playerid, COLOR_GREY, "Подойди к решётке камеры в участке LSPD.");
	Law_ToggleCellGate(playerid);
	SendClientMessage(playerid, COLOR_ORANGE, "Решётка открыта на %d сек. Закрыть раньше: /celldoor или ALT у неё.", CELLDOOR_SECONDS);
	return true;
}

// Заключённый не на месте, но это законно: дверь открыта или он заплатил
stock Law_MayLeaveCell(playerid)
{
	new now = gettime();
	if (gJailPass[playerid] > now || gCellOpenUntil > now) return 1;
	return 0;
}

// Из doors.inc: игрок у двери. 1 — можно пройти, 0 — дверь заперта (игроку уже написано).
stock Law_TryLeave(playerid)
{
	if (Player[playerid][pJail] <= 0) return 1;
	new now = gettime();
	if (gJailPass[playerid] > now)
	{
		gJailPass[playerid] = 0;
		Player[playerid][pJail] = 0;
		Account_Save(playerid);
		SendClientMessage(playerid, COLOR_GREEN, "Тебя тихо выпустили. Срок снят.");
		return 1;
	}
	if (gCellOpenUntil > now)
	{
		Player[playerid][pJail] = 0;
		Player[playerid][pWanted] += ESCAPE_STARS;
		if (Player[playerid][pWanted] > 6) Player[playerid][pWanted] = 6;
		SetPlayerWantedLevel(playerid, Player[playerid][pWanted]);
		Account_Save(playerid);
		SendClientMessage(playerid, COLOR_RED, "Ты сбежал из участка! Розыск: %d зв.", Player[playerid][pWanted]);
		new msg[144];
		format(msg, sizeof msg, "[Диспетчер] Побег из участка LSPD: %s [%d]. Розыск %d зв.", GetRPName(playerid), playerid, Player[playerid][pWanted]);
		Faction_Message(FACTION_POLICE, Faction_GetColor(FACTION_POLICE), msg);
		return 1;
	}
	SendClientMessage(playerid, COLOR_GREY, "Дверь заперта. Открыть её может только полицейский. Договориться: /bribe [id] [сумма]");
	return 0;
}

// /bribe [id] [сумма] — заключённый предлагает взятку полицейскому рядом
stock bool:Cmd_Bribe(playerid, const params[])
{
	if (Player[playerid][pJail] <= 0) return SendClientMessage(playerid, COLOR_GREY, "Взятку предлагают из камеры: ты не в тюрьме.");
	new pos = 0, amount;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid || !NextIntParam(params, pos, amount))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /bribe [id полицейского] [сумма от $%d]", BRIBE_MIN);
	if (amount < BRIBE_MIN || amount > BRIBE_MAX) return SendClientMessage(playerid, COLOR_GREY, "Сумма от $%d до $%d.", BRIBE_MIN, BRIBE_MAX);
	if (!Player[target][pLogged] || Player[target][pFaction] != FACTION_POLICE || !Player[target][pDuty])
		return SendClientMessage(playerid, COLOR_GREY, "Это не полицейский на смене.");
	if (!IsPlayerNearPlayer(playerid, target, 5.0)) return SendClientMessage(playerid, COLOR_GREY, "Полицейский слишком далеко.");
	if (GetMoney(playerid) < amount) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет столько наличных.");

	Player[target][pOfferType] = OFFER_BRIBE;
	Player[target][pOfferFrom] = playerid;
	Player[target][pOfferFromDb] = Player[playerid][pDbId];
	Player[target][pOfferValue] = amount;
	SendClientMessage(target, COLOR_BLUE, "%s предлагает тебе $%d, чтобы ты его выпустил. Взять: Y (или /accept), отказаться: N", GetRPName(playerid), amount);
	SendClientMessage(playerid, COLOR_WHITE, "Ты предложил $%d. Ждём ответа.", amount);
	return true;
}

// Из Cmd_Accept (factions.inc): полицейский playerid берёт взятку у заключённого from
stock Law_AcceptBribe(playerid, from, amount)
{
	if (Player[playerid][pFaction] != FACTION_POLICE || !Player[playerid][pDuty] || Player[from][pJail] <= 0
		|| amount < BRIBE_MIN || !IsPlayerNearPlayer(playerid, from, 5.0))
	{
		SendClientMessage(playerid, COLOR_GREY, "Предложение больше не действует.");
		return 0;
	}
	if (GetMoney(from) < amount)
	{
		SendClientMessage(from, COLOR_GREY, "У тебя уже нет этих денег.");
		SendClientMessage(playerid, COLOR_GREY, "У него нет таких денег.");
		return 0;
	}
	GiveMoney(from, -amount, "взятка полицейскому");
	GiveMoney(playerid, amount, "взятка от заключённого");
	Account_Save(from);
	Account_Save(playerid);
	gJailPass[from] = gettime() + BRIBE_PASS_SECONDS;
	SendClientMessage(playerid, COLOR_GREEN, "Ты взял $%d. У него %d сек., чтобы выйти через дверь участка.", amount, BRIBE_PASS_SECONDS);
	SendClientMessage(from, COLOR_GREEN, "Взятку взяли. У тебя %d сек.: иди к двери участка и выходи (ALT).", BRIBE_PASS_SECONDS);
	return 1;
}
