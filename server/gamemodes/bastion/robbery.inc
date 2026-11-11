// =====================================================================
//  robbery.inc — ограбление магазинов 24/7 и баров.
//
//  Встань у значка магазина с оружием в руках и введи /rob. Полиции
//  сразу приходит вызов, тебе — розыск ROB_WANTED звёзд. Нужно
//  продержаться ROB_TIME секунд рядом с магазином — тогда касса твоя.
//  Ушёл дальше ROB_RANGE, ранен или вышел — ограбление сорвано.
//  Один магазин можно грабить раз в ROB_BIZ_COOLDOWN, игроку — раз в
//  ROB_PLAYER_COOLDOWN. Нужен хотя бы ROB_MIN_POLICE полицейский онлайн
//  (админов 4+ это не касается — для проверки).
// =====================================================================

#define ROB_TIME            40          // секунд держаться у магазина
#define ROB_RANGE           10.0
#define ROB_MIN_LEVEL       2
#define ROB_WANTED          3           // звёзд за ограбление
#define ROB_MIN_MONEY       1500
#define ROB_MAX_MONEY       3500
#define ROB_BIZ_COOLDOWN    1800        // секунд между ограблениями одного магазина
#define ROB_PLAYER_COOLDOWN 900         // секунд между ограблениями одного игрока
#define ROB_MIN_POLICE      1

forward Rob_OnSecond();

static gRobBiz[MAX_PLAYERS] = { -1, ... };      // какой бизнес грабит (-1 — никакой)
static gRobLeft[MAX_PLAYERS];
static gRobPlayerLast[MAX_PLAYERS];
static gRobBizLast[MAX_BUSINESSES];

stock Robbery_Init()
{
	SetTimer("Rob_OnSecond", 1000, true);
}

stock Rob_OnDisconnect(playerid)
{
	if (gRobBiz[playerid] != -1) Rob_Fail(playerid, "грабитель скрылся из города");
	gRobPlayerLast[playerid] = 0;
}

static Rob_PoliceOnline()
{
	new n = 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pFaction] == FACTION_POLICE) n++;
	}
	return n;
}

static Rob_Fail(playerid, const why[])
{
	new b = gRobBiz[playerid];
	gRobBiz[playerid] = -1;
	if (b < 0) return;
	if (IsPlayerConnected(playerid)) SendClientMessage(playerid, COLOR_RED, "Ограбление сорвано: %s.", why);
	new msg[144];
	format(msg, sizeof msg, "[Диспетчер] Ограбление «%s» сорвано: %s.", Biz[b][bName], why);
	Faction_Message(FACTION_POLICE, Faction_GetColor(FACTION_POLICE), msg);
}

stock bool:Cmd_Rob(playerid)
{
	if (gRobBiz[playerid] != -1) return SendClientMessage(playerid, COLOR_GREY, "Ты уже грабишь. Держись рядом с магазином.");
	new b = Biz_GetNearby(playerid);
	if (b == -1 || (Biz[b][bType] != BIZ_SHOP && Biz[b][bType] != BIZ_BAR))
		return SendClientMessage(playerid, COLOR_GREY, "Грабить можно магазин 24/7 или бар: встань у его значка.");
	if (Player[playerid][pLevel] < ROB_MIN_LEVEL) return SendClientMessage(playerid, COLOR_GREY, "Грабить можно с %d уровня.", ROB_MIN_LEVEL);
	if (Player[playerid][pDuty] && (Player[playerid][pFaction] == FACTION_POLICE || Player[playerid][pFaction] == FACTION_EMS))
		return SendClientMessage(playerid, COLOR_GREY, "На смене грабить нельзя.");
	new w = _:GetPlayerWeapon(playerid);
	if (w < 22 || w > 34) return SendClientMessage(playerid, COLOR_GREY, "Нужно огнестрельное оружие в руках.");

	new bool:admin = Player[playerid][pAdmin] >= 4;
	if (!admin && Rob_PoliceOnline() < ROB_MIN_POLICE) return SendClientMessage(playerid, COLOR_GREY, "Сейчас в городе нет полиции — грабить неинтересно.");
	new now = gettime();
	if (now - gRobPlayerLast[playerid] < ROB_PLAYER_COOLDOWN)
		return SendClientMessage(playerid, COLOR_GREY, "Ты недавно грабил. Подожди ещё %d мин.", (ROB_PLAYER_COOLDOWN - (now - gRobPlayerLast[playerid])) / 60 + 1);
	if (now - gRobBizLast[b] < ROB_BIZ_COOLDOWN)
		return SendClientMessage(playerid, COLOR_GREY, "Этот магазин недавно грабили, касса пустая. Подожди %d мин.", (ROB_BIZ_COOLDOWN - (now - gRobBizLast[b])) / 60 + 1);

	gRobBiz[playerid] = b;
	gRobLeft[playerid] = ROB_TIME;
	gRobPlayerLast[playerid] = now;
	gRobBizLast[b] = now;

	new stars = Player[playerid][pWanted] + ROB_WANTED;
	if (stars > 6) stars = 6;
	Player[playerid][pWanted] = stars;
	SetPlayerWantedLevel(playerid, stars);
	Account_Save(playerid);

	SendClientMessage(playerid, COLOR_RED, "Ты начал ограбление «%s»! Продержись %d сек. рядом с магазином. Розыск: %d зв.", Biz[b][bName], ROB_TIME, stars);
	new msg[144];
	format(msg, sizeof msg, "[Диспетчер] Ограбление «%s»! Подозреваемый: %s [%d], %d зв. Метка на карте у всех на смене.", Biz[b][bName], GetRPName(playerid), playerid, stars);
	Faction_Message(FACTION_POLICE, Faction_GetColor(FACTION_POLICE), msg);
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || Player[i][pFaction] != FACTION_POLICE || !Player[i][pDuty]) continue;
		Gps_SetPoint(i, Biz[b][bX], Biz[b][bY], Biz[b][bZ], "ограбление");
	}
	return true;
}

public Rob_OnSecond()
{
	new buf[48];
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		new b = gRobBiz[i];
		if (b == -1) continue;
		if (!IsPlayerConnected(i)) { gRobBiz[i] = -1; continue; }
		if (Wound_IsWounded(i) || GetPlayerState(i) == PLAYER_STATE_WASTED) { Rob_Fail(i, "грабитель ранен"); continue; }
		if (Player[i][pCuffed]) { Rob_Fail(i, "грабитель задержан"); continue; }
		if (!Biz_IsInside(i, b) && (GetPlayerInterior(i) != 0 || !IsPlayerInRangeOfPoint(i, ROB_RANGE, Biz[b][bX], Biz[b][bY], Biz[b][bZ])))
		{
			Rob_Fail(i, "грабитель ушёл без денег");
			continue;
		}
		gRobLeft[i]--;
		if (gRobLeft[i] > 0)
		{
			format(buf, sizeof buf, "~r~Robbery~n~~w~%d s", gRobLeft[i]);
			GameTextForPlayer(i, buf, 1100, 3);
			continue;
		}
		gRobBiz[i] = -1;
		new money = ROB_MIN_MONEY + random(ROB_MAX_MONEY - ROB_MIN_MONEY + 1);
		GiveMoney(i, money, "ограбление");
		GameTextForPlayer(i, "~g~Robbery done", 2500, 3);
		SendClientMessage(i, COLOR_GREEN, "Касса «%s» твоя: $%d. Уходи, полиция уже едет!", Biz[b][bName], money);
		new msg[144];
		format(msg, sizeof msg, "[Диспетчер] Из «%s» унесли $%d. Грабитель: %s [%d].", Biz[b][bName], money, GetRPName(i), i);
		Faction_Message(FACTION_POLICE, Faction_GetColor(FACTION_POLICE), msg);
	}
	return 1;
}
