// =====================================================================
//  wounded.inc — ранение вместо мгновенной смерти.
//
//  Игрока убили — он не уезжает сразу в больницу, а лежит раненым на
//  месте WOUND_TIME секунд. За это время его могут поднять:
//    медик EMS на смене — /revive [id] (медик получает WOUND_REWARD);
//    любой игрок с аптечкой в сумке — /aid [id] (тратится аптечка).
//  Медикам на смене приходит вызов, /wounds — список раненых с GPS.
//  Сдаться и уехать в больницу: /dead (через WOUND_GIVEUP секунд).
//  Если раненого добили или время вышло — больница, как раньше.
// =====================================================================

#define WOUND_TIME          180     // сколько секунд лежать раненым
#define WOUND_GIVEUP        30      // через сколько секунд можно /dead
#define WOUND_HEALTH        15.0    // здоровье раненого (можно добить)
#define WOUND_REVIVE_HP     40.0    // здоровье после /revive
#define WOUND_AID_HP        25.0    // здоровье после /aid
#define WOUND_REWARD        250     // медику за /revive
#define WOUND_RANGE         3.0

forward Wound_OnSecond();
forward Wound_ApplyAnim(playerid);

static bool:gWoundPending[MAX_PLAYERS];     // умер, следующий спавн — на месте раненым
static bool:gWounded[MAX_PLAYERS];          // лежит раненым
static gWoundLeft[MAX_PLAYERS];             // секунд до больницы
static Float:gWoundPos[MAX_PLAYERS][4];
static gWoundInt[MAX_PLAYERS];
static gWoundWorld[MAX_PLAYERS];
static gWoundList[MAX_PLAYERS][MAX_PLAYERS]; // строка окна /wounds -> id

#define DIALOG_WOUNDS       1240

stock Wound_Init()
{
	SetTimer("Wound_OnSecond", 1000, true);
}

stock bool:Wound_IsWounded(playerid) { return gWounded[playerid]; }

stock Wound_Reset(playerid)
{
	gWoundPending[playerid] = false;
	gWounded[playerid] = false;
	gWoundLeft[playerid] = 0;
}

// Из OnPlayerDeath. true — игрок появится на месте раненым (больницы нет).
stock bool:Wound_OnDeath(playerid)
{
	if (gWounded[playerid])
	{
		// добили раненого — в больницу
		gWounded[playerid] = false;
		return false;
	}
	if (Player[playerid][pJail] > 0 || Admin_IsInAJail(playerid)) return false;

	GetPlayerPos(playerid, gWoundPos[playerid][0], gWoundPos[playerid][1], gWoundPos[playerid][2]);
	GetPlayerFacingAngle(playerid, gWoundPos[playerid][3]);
	gWoundInt[playerid] = GetPlayerInterior(playerid);
	gWoundWorld[playerid] = GetPlayerVirtualWorld(playerid);
	gWoundPending[playerid] = true;
	SetSpawnInfo(playerid, NO_TEAM, Player[playerid][pSkin], gWoundPos[playerid][0], gWoundPos[playerid][1], gWoundPos[playerid][2], gWoundPos[playerid][3]);
	return true;
}

// Из OnPlayerSpawn (в самом конце)
stock Wound_OnSpawn(playerid)
{
	if (!gWoundPending[playerid]) return;
	gWoundPending[playerid] = false;
	if (Player[playerid][pJail] > 0 || Admin_IsInAJail(playerid)) return;

	SetPlayerInterior(playerid, gWoundInt[playerid]);
	SetPlayerVirtualWorld(playerid, gWoundWorld[playerid]);
	SetPlayerPos(playerid, gWoundPos[playerid][0], gWoundPos[playerid][1], gWoundPos[playerid][2]);
	SetPlayerHealth(playerid, WOUND_HEALTH);
	gWounded[playerid] = true;
	gWoundLeft[playerid] = WOUND_TIME;
	TogglePlayerControllable(playerid, false);
	// анимацию ставим чуть позже: сразу после спавна клиент её сбрасывает
	ApplyAnimation(playerid, "CRACK", "null", 4.1, false, false, false, false, 0);
	SetTimerEx("Wound_ApplyAnim", 500, false, "i", playerid);

	SendClientMessage(playerid, COLOR_RED, "Ты тяжело ранен. Жди медика (вызов уже отправлен) или того, у кого есть аптечка.");
	SendClientMessage(playerid, COLOR_GREY, "Через %d сек. можно сдаться и уехать в больницу: /dead. Через %d сек. увезут сами.", WOUND_GIVEUP, WOUND_TIME);

	new msg[144];
	format(msg, sizeof msg, "[Вызов] Ранен %s [%d]. Метка на карте: /wounds, поднять: /revive %d", GetRPName(playerid), playerid, playerid);
	Faction_Message(FACTION_EMS, Faction_GetColor(FACTION_EMS), msg);
}

public Wound_ApplyAnim(playerid)
{
	if (!IsPlayerConnected(playerid) || !gWounded[playerid]) return 1;
	ApplyAnimation(playerid, "CRACK", "crckdeth2", 4.1, true, false, false, true, 0, SYNC_ALL);
	return 1;
}

// Поставить на ноги
static Wound_StandUp(playerid, Float:hp)
{
	gWounded[playerid] = false;
	gWoundLeft[playerid] = 0;
	TogglePlayerControllable(playerid, true);
	ClearAnimations(playerid, SYNC_ALL);
	SetPlayerHealth(playerid, hp);
	GameTextForPlayer(playerid, "~g~Revived", 2000, 4);
}

// В больницу (время вышло или /dead) — как обычная смерть
static Wound_ToHospital(playerid)
{
	gWounded[playerid] = false;
	gWoundLeft[playerid] = 0;
	TogglePlayerControllable(playerid, true);
	ClearAnimations(playerid, SYNC_ALL);
	SetPlayerInterior(playerid, 0);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerPos(playerid, HOSPITAL_X, HOSPITAL_Y, HOSPITAL_Z);
	SetPlayerHealth(playerid, 100.0);
	SetCameraBehindPlayer(playerid);
	Needs_OnHospital(playerid);
	GiveMoney(playerid, -HOSPITAL_FEE, "счёт больницы");
	Econ_ToTreasury(HOSPITAL_FEE);
	SendClientMessage(playerid, COLOR_WHITE, "Тебя довезли до больницы и вылечили. Счёт за лечение: $%d.", HOSPITAL_FEE);
}

public Wound_OnSecond()
{
	new buf[48];
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!gWounded[i] || !IsPlayerConnected(i)) continue;
		gWoundLeft[i]--;
		if (gWoundLeft[i] <= 0)
		{
			Wound_ToHospital(i);
			continue;
		}
		format(buf, sizeof buf, "~r~WOUNDED~n~~w~%d s", gWoundLeft[i]);
		GameTextForPlayer(i, buf, 1100, 3);
		if (gWoundLeft[i] % 10 == 0) Wound_ApplyAnim(i);   // встать сам не должен
	}
	return 1;
}

// /dead — сдаться
stock bool:Cmd_Dead(playerid)
{
	if (!gWounded[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Ты не ранен.");
	new waited = WOUND_TIME - gWoundLeft[playerid];
	if (waited < WOUND_GIVEUP) return SendClientMessage(playerid, COLOR_GREY, "Подожди ещё %d сек. — вдруг успеют помочь.", WOUND_GIVEUP - waited);
	Wound_ToHospital(playerid);
	return true;
}

// /revive [id] — медик EMS на смене (или админ 2+ где угодно)
stock bool:Cmd_Revive(playerid, const params[])
{
	new bool:admin = Player[playerid][pAdmin] >= 2;
	if (!admin && !Faction_Need(playerid, FACTION_EMS, true)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /revive [id]");
	if (!gWounded[target]) return SendClientMessage(playerid, COLOR_GREY, "Этот игрок не ранен.");
	new bool:near = IsPlayerNearPlayer(playerid, target, WOUND_RANGE);
	if (!near && !admin) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе к раненому.");

	Wound_StandUp(target, WOUND_REVIVE_HP);
	if (near) ApplyAnimation(playerid, "MEDIC", "CPR", 4.1, false, false, false, false, 0, SYNC_ALL);
	SendClientMessage(target, COLOR_GREEN, "%s поставил тебя на ноги. Долечиться: медик (/heal) или аптечка.", GetRPName(playerid));
	if (Player[playerid][pFaction] == FACTION_EMS && near)
	{
		GiveMoney(playerid, WOUND_REWARD, "реанимация");
		SendClientMessage(playerid, COLOR_GREEN, "Ты поднял %s. Премия: $%d.", GetRPName(target), WOUND_REWARD);
	}
	else SendClientMessage(playerid, COLOR_ADMIN, "%s поднят.", GetRPName(target));
	return true;
}

// /aid [id] — первая помощь аптечкой из сумки, может любой
stock bool:Cmd_Aid(playerid, const params[])
{
	if (gWounded[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Ты сам ранен.");
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /aid [id] — нужна аптечка в сумке");
	if (!gWounded[target]) return SendClientMessage(playerid, COLOR_GREY, "Этот игрок не ранен.");
	if (!IsPlayerNearPlayer(playerid, target, WOUND_RANGE)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе к раненому.");
	if (!Inv_TakeOne(playerid, INV_MEDKIT)) return SendClientMessage(playerid, COLOR_GREY, "Нужна аптечка в сумке (продают в 24/7).");

	Wound_StandUp(target, WOUND_AID_HP);
	ApplyAnimation(playerid, "MEDIC", "CPR", 4.1, false, false, false, false, 0, SYNC_ALL);
	SendClientMessage(target, COLOR_GREEN, "%s перевязал тебя и поставил на ноги.", GetRPName(playerid));
	SendClientMessage(playerid, COLOR_GREEN, "Ты помог %s. Потрачена аптечка.", GetRPName(target));
	return true;
}

// /wounds — список раненых (медикам и админам), выбор — метка GPS
stock bool:Cmd_Wounds(playerid)
{
	if (Player[playerid][pFaction] != FACTION_EMS && Player[playerid][pAdmin] < 2)
		return SendClientMessage(playerid, COLOR_GREY, "Только для медиков EMS.");
	new list[1024], line[96], n = 0, Float:x, Float:y, Float:z;
	strcat(list, "Раненый\tДистанция\tОсталось\n");
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!gWounded[i] || !IsPlayerConnected(i)) continue;
		GetPlayerPos(i, x, y, z);
		format(line, sizeof line, "%s [%d]\t%.0f м\t%d сек.\n", GetRPName(i), i, GetPlayerDistanceFromPoint(playerid, x, y, z), gWoundLeft[i]);
		if (strlen(list) + strlen(line) >= sizeof list) break;
		strcat(list, line);
		gWoundList[playerid][n++] = i;
	}
	if (n == 0) return SendClientMessage(playerid, COLOR_GREY, "Раненых сейчас нет.");
	ShowPlayerDialog(playerid, DIALOG_WOUNDS, DIALOG_STYLE_TABLIST_HEADERS, "Раненые", "%s", "GPS", "Закрыть", list);
	return true;
}

// Из OnDialogResponse
stock Wound_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid != DIALOG_WOUNDS) return 0;
	if (!response || listitem < 0 || listitem >= MAX_PLAYERS) return 1;
	new target = gWoundList[playerid][listitem];
	if (!IsPlayerConnected(target) || !gWounded[target])
	{
		SendClientMessage(playerid, COLOR_GREY, "Ему уже помогли.");
		return 1;
	}
	new Float:x, Float:y, Float:z, name[48];
	GetPlayerPos(target, x, y, z);
	format(name, sizeof name, "раненый %s", GetRPName(target));
	Gps_SetPoint(playerid, x, y, z, name);
	return 1;
}
