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

// /celldoor — полицейский открывает (или закрывает) дверь участка для заключённых
stock bool:Cmd_CellDoor(playerid)
{
	if (!Faction_Need(playerid, FACTION_POLICE, true)) return true;
	if (GetPlayerInterior(playerid) != JAIL_INTERIOR || !IsPlayerInRangeOfPoint(playerid, 60.0, JAIL_X, JAIL_Y, JAIL_Z))
		return SendClientMessage(playerid, COLOR_GREY, "Дверь открывается изнутри участка LSPD.");

	new msg[144];
	if (gCellOpenUntil > gettime())
	{
		gCellOpenUntil = 0;
		format(msg, sizeof msg, "%s запирает дверь участка.", GetRPName(playerid));
		SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
		return true;
	}
	gCellOpenUntil = gettime() + CELLDOOR_SECONDS;
	format(msg, sizeof msg, "%s отпирает дверь участка.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(playerid, COLOR_ORANGE, "Дверь открыта для заключённых на %d сек. Закрыть раньше: /celldoor. Запись ушла в журнал фракции.", CELLDOOR_SECONDS);
	format(msg, sizeof msg, "%s открыл дверь участка для заключённых.", GetRPName(playerid));
	Faction_Log(FACTION_POLICE, msg);
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
