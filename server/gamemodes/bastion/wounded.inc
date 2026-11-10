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
//
//  Обычно игрок падает раненым ещё до смерти: когда здоровье опускается
//  ниже WOUND_DOWN_HP. Если его всё же убили одним выстрелом — он
//  появляется раненым на том же месте.
//
//  /drag [id] — полиция на смене ведёт за собой задержанного в наручниках
//  или раненого. /drag без id — отпустить.
//  /carry [id] — понести раненого на руках (может любой, но раненый должен
//  согласиться: Y). /carry без id — положить.
// =====================================================================

#define WOUND_TIME          180     // сколько секунд лежать раненым
#define WOUND_GIVEUP        30      // через сколько секунд можно /dead
#define WOUND_HEALTH        50.0    // здоровье раненого (пули его почти не берут)
#define WOUND_DOWN_HP       20.0    // ниже этого здоровья игрок падает раненым, а не умирает
#define WOUND_REVIVE_HP     40.0    // здоровье после /revive
#define WOUND_AID_HP        25.0    // здоровье после /aid
#define WOUND_REWARD        250     // медику за /revive
#define WOUND_RANGE         3.0

forward Wound_OnSecond();
forward Wound_ApplyAnim(playerid);
forward Drag_Tick();

static gDragBy[MAX_PLAYERS] = { INVALID_PLAYER_ID, ... };   // кто ведёт / несёт этого игрока
static bool:gCarried[MAX_PLAYERS];                          // несут на руках (а не ведут)
static gCarryAnimTick[MAX_PLAYERS];
static gDragWalk[MAX_PLAYERS];                              // 0 стоит, 1 идёт, 2 бежит

static bool:gWoundPending[MAX_PLAYERS];     // умер, следующий спавн — на месте раненым
static bool:gWounded[MAX_PLAYERS];          // лежит раненым
static gWoundLeft[MAX_PLAYERS];             // секунд до больницы
static Float:gWoundPos[MAX_PLAYERS][4];
static gWoundInt[MAX_PLAYERS];
static gWoundWorld[MAX_PLAYERS];
static gWoundList[MAX_PLAYERS][MAX_PLAYERS]; // строка окна /wounds -> id


stock Wound_Init()
{
	SetTimer("Wound_OnSecond", 1000, true);
	SetTimer("Drag_Tick", 300, true);
}

stock bool:Wound_IsWounded(playerid) { return gWounded[playerid]; }

stock Wound_Reset(playerid)
{
	Drag_StopAll(playerid);
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
	Wound_Start(playerid);
}

// Уложить раненым прямо сейчас (без смерти)
static Wound_Start(playerid)
{
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

// Из OnPlayerTakeDamage: при малом здоровье — упасть раненым, а не умереть
stock Wound_OnDamage(playerid, Float:amount)
{
	if (!Player[playerid][pLogged]) return;
	if (gWounded[playerid])
	{
		SetPlayerHealth(playerid, WOUND_HEALTH);    // лежачего не добивают обычным уроном
		return;
	}
	if (Player[playerid][pJail] > 0 || Admin_IsInAJail(playerid)) return;
	if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return;  // в машине — как раньше
	new Float:hp;
	GetPlayerHealth(playerid, hp);
	if (hp <= 0.0 || hp - amount > WOUND_DOWN_HP) return;
	// смертельный выстрел: клиент всё равно умрёт — пусть сработает OnPlayerDeath,
	// он поставит раненым на том же месте (иначе смерть «раненого» = больница
	// и двойное убийство в войне за район)
	if (hp - amount <= 0.0) return;

	Job_Stop(playerid);
	Licenses_Abort(playerid);
	Phone_HangUp(playerid);
	Wound_Start(playerid);
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
	Drag_Stop(playerid);
	gWounded[playerid] = false;
	gWoundLeft[playerid] = 0;
	TogglePlayerControllable(playerid, true);
	ClearAnimations(playerid, SYNC_ALL);
	ApplyAnimation(playerid, "PED", "getup_front", 4.1, false, false, false, false, 0, SYNC_ALL);
	SetCameraBehindPlayer(playerid);
	if (hp > 0.0) SetPlayerHealth(playerid, hp);
	GameTextForPlayer(playerid, "~g~Revived", 2000, 4);
}

// В больницу (время вышло или /dead) — как обычная смерть
static Wound_ToHospital(playerid)
{
	Drag_Stop(playerid);
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
		// вылечили другим способом (/heal медика, админ) — встаёт сам
		new Float:hp;
		GetPlayerHealth(i, hp);
		if (hp > WOUND_HEALTH + 5.0)
		{
			Wound_StandUp(i, 0.0);
			SendClientMessage(i, COLOR_GREEN, "Тебя вылечили, ты снова на ногах.");
			continue;
		}
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

// За одного и того же раненого премию платят не чаще раза в WOUND_REWARD_COOLDOWN секунд:
// иначе медик с приятелем «роняют и поднимают» друг друга и печатают деньги
#define WOUND_REWARD_COOLDOWN   600
static gReviveRewardAt[MAX_PLAYERS];

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
		if (gettime() - gReviveRewardAt[target] >= WOUND_REWARD_COOLDOWN)
		{
			gReviveRewardAt[target] = gettime();
			GiveMoney(playerid, WOUND_REWARD, "реанимация");
			SendClientMessage(playerid, COLOR_GREEN, "Ты поднял %s. Премия: $%d.", GetRPName(target), WOUND_REWARD);
		}
		else SendClientMessage(playerid, COLOR_GREEN, "Ты поднял %s. Премию за этого пациента недавно уже платили.", GetRPName(target));
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

// ---------------------------------------------------------------------
//  /drag — полиция ведёт задержанного, /carry — нести раненого на руках
// ---------------------------------------------------------------------
static Drag_Stop(target)
{
	new by = gDragBy[target];
	if (by == INVALID_PLAYER_ID) return;
	new bool:carried = gCarried[target];
	gDragBy[target] = INVALID_PLAYER_ID;
	gCarried[target] = false;
	Drag_StopWalk(target);
	if (!IsPlayerConnected(by)) return;
	if (carried)
	{
		ClearAnimations(by, SYNC_ALL);
		SendClientMessage(by, COLOR_GREY, "Ты положил %s.", GetRPName(target));
	}
	else SendClientMessage(by, COLOR_GREY, "Ты отпустил %s.", GetRPName(target));
}

// Игрок вышел: отпустить того, кого он вёл, и освободиться самому
// Остановить шаг задержанного, которого вели
static Drag_StopWalk(target)
{
	if (gDragWalk[target] == 0) return;
	gDragWalk[target] = 0;
	if (IsPlayerConnected(target)) ClearAnimations(target, SYNC_ALL);
}

static Drag_StopAll(playerid)
{
	gDragBy[playerid] = INVALID_PLAYER_ID;
	gCarried[playerid] = false;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gDragBy[i] == playerid)
		{
			gDragBy[i] = INVALID_PLAYER_ID;
			gCarried[i] = false;
			Drag_StopWalk(i);
		}
	}
}

static Drag_Find(playerid)
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gDragBy[i] == playerid) return i;
	}
	return INVALID_PLAYER_ID;
}

static bool:Drag_CanLead(playerid)
{
	if (gWounded[playerid] || Player[playerid][pCuffed])
	{
		SendClientMessage(playerid, COLOR_GREY, "Сейчас ты не можешь никого вести.");
		return false;
	}
	if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT)
	{
		SendClientMessage(playerid, COLOR_GREY, "Выйди из машины.");
		return false;
	}
	return true;
}

static Drag_Begin(target, by, bool:carry)
{
	new cur = Drag_Find(by);
	if (cur != INVALID_PLAYER_ID) Drag_Stop(cur);
	gDragBy[target] = by;
	gCarried[target] = carry;
	gCarryAnimTick[target] = 0;
	gDragWalk[target] = 0;
	if (carry) ApplyAnimation(by, "CARRY", "crry_prtial", 4.1, true, false, false, true, 1, SYNC_ALL);
}

// /drag [id] — только полиция на смене
stock bool:Cmd_Drag(playerid, const params[])
{
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID)
	{
		new cur = Drag_Find(playerid);
		if (cur != INVALID_PLAYER_ID && !gCarried[cur]) { Drag_Stop(cur); return true; }
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /drag [id] — вести задержанного, /drag — отпустить");
	}
	if (gDragBy[target] == playerid && !gCarried[target]) { Drag_Stop(target); return true; }
	if (!Faction_Need(playerid, FACTION_POLICE, true)) return true;
	if (target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Себя вести нельзя.");
	if (!Drag_CanLead(playerid)) return true;
	if (!IsPlayerNearPlayer(playerid, target, WOUND_RANGE)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе.");
	if (!gWounded[target] && !Player[target][pCuffed]) return SendClientMessage(playerid, COLOR_GREY, "Сначала надень наручники (/cuff).");
	if (gDragBy[target] != INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Его уже ведут.");

	Drag_Begin(target, playerid, false);
	SendClientMessage(playerid, COLOR_WHITE, "Ты ведёшь %s. Отпустить: /drag", GetRPName(target));
	SendClientMessage(target, COLOR_WHITE, "Офицер %s ведёт тебя за собой.", GetRPName(playerid));
	return true;
}

// /carry [id] — предложить раненому понести его
stock bool:Cmd_Carry(playerid, const params[])
{
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID)
	{
		new cur = Drag_Find(playerid);
		if (cur != INVALID_PLAYER_ID && gCarried[cur]) { Drag_Stop(cur); return true; }
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /carry [id] — понести раненого, /carry — положить");
	}
	if (gDragBy[target] == playerid && gCarried[target]) { Drag_Stop(target); return true; }
	if (target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Себя нести нельзя.");
	if (!Drag_CanLead(playerid)) return true;
	if (!gWounded[target]) return SendClientMessage(playerid, COLOR_GREY, "Нести можно только раненого.");
	if (!IsPlayerNearPlayer(playerid, target, WOUND_RANGE)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе.");
	if (gDragBy[target] != INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Его уже несут.");

	Player[target][pOfferType] = OFFER_CARRY;
	Player[target][pOfferFrom] = playerid;
	Player[target][pOfferFromDb] = Player[playerid][pDbId];
	Player[target][pOfferValue] = 0;
	SendClientMessage(target, COLOR_BLUE, "%s хочет понести тебя на руках. Согласиться: Y (или /accept), отказаться: N", GetRPName(playerid));
	SendClientMessage(playerid, COLOR_WHITE, "Ты предложил %s понести его. Ждём согласия.", GetRPName(target));
	return true;
}

// Из Cmd_Accept (factions.inc): раненый согласился
stock Wound_AcceptCarry(playerid, from)
{
	if (!gWounded[playerid])
	{
		SendClientMessage(playerid, COLOR_GREY, "Ты уже не ранен.");
		return;
	}
	if (gDragBy[playerid] != INVALID_PLAYER_ID)
	{
		SendClientMessage(playerid, COLOR_GREY, "Тебя уже несут.");
		return;
	}
	if (gWounded[from] || Player[from][pCuffed] || GetPlayerState(from) != PLAYER_STATE_ONFOOT || !IsPlayerNearPlayer(playerid, from, WOUND_RANGE + 2.0))
	{
		SendClientMessage(playerid, COLOR_GREY, "Человек отошёл.");
		return;
	}
	Drag_Begin(playerid, from, true);
	SendClientMessage(from, COLOR_WHITE, "Ты несёшь %s. Положить: /carry", GetRPName(playerid));
	SendClientMessage(playerid, COLOR_WHITE, "%s несёт тебя на руках.", GetRPName(from));
	new msg[144];
	format(msg, sizeof msg, "%s поднимает %s на руки.", GetRPName(from), GetRPName(playerid));
	SendNearbyMessage(from, CHAT_RADIUS, COLOR_PURPLE, msg);
}

// Задержанный идёт за офицером своими ногами (анимация шага двигает его сама)
static Drag_Walk(t, by)
{
	new Float:tx, Float:ty, Float:tz, Float:bx, Float:by_, Float:bz;
	GetPlayerPos(t, tx, ty, tz);
	GetPlayerPos(by, bx, by_, bz);
	new Float:dist = VectorSize(bx - tx, by_ - ty, 0.0);
	if (dist > 10.0 || floatabs(bz - tz) > 3.0)
	{
		// отстал (дверь, лестница, телепорт) — ставим рядом
		new Float:a;
		GetPlayerFacingAngle(by, a);
		SetPlayerPos(t, bx - 1.2 * floatsin(-a, degrees), by_ - 1.2 * floatcos(-a, degrees), bz);
		SetPlayerFacingAngle(t, a);
		gDragWalk[t] = 0;
		ClearAnimations(t, SYNC_ALL);
		return;
	}
	new Float:ang = atan2(by_ - ty, bx - tx) - 90.0;    // atan2 в SA-MP/open.mp — в градусах
	SetPlayerFacingAngle(t, ang);
	new mode = 0;                       // 0 стоит, 1 идёт, 2 бежит
	if (dist > 4.0) mode = 2;
	else if (dist > 1.6) mode = 1;
	if (mode == gDragWalk[t]) return;
	gDragWalk[t] = mode;
	if (mode == 2) ApplyAnimation(t, "PED", "run_civi", 4.1, true, true, true, false, 0, SYNC_ALL);
	else if (mode == 1) ApplyAnimation(t, "PED", "WALK_civi", 4.1, true, true, true, false, 0, SYNC_ALL);
	else ClearAnimations(t, SYNC_ALL);
}

public Drag_Tick()
{
	new Float:x, Float:y, Float:z, Float:a;
	for (new t = 0; t < MAX_PLAYERS; t++)
	{
		new by = gDragBy[t];
		if (by == INVALID_PLAYER_ID) continue;
		new bool:ok = IsPlayerConnected(t) && IsPlayerConnected(by)
			&& GetPlayerState(by) == PLAYER_STATE_ONFOOT && GetPlayerState(t) == PLAYER_STATE_ONFOOT;
		if (ok)
		{
			if (gCarried[t]) ok = gWounded[t] && !gWounded[by];
			else ok = (gWounded[t] || Player[t][pCuffed]) && Player[by][pFaction] == FACTION_POLICE && Player[by][pDuty];
		}
		if (!ok)
		{
			Drag_Stop(t);
			continue;
		}
		if (GetPlayerInterior(t) != GetPlayerInterior(by)) SetPlayerInterior(t, GetPlayerInterior(by));
		if (GetPlayerVirtualWorld(t) != GetPlayerVirtualWorld(by)) SetPlayerVirtualWorld(t, GetPlayerVirtualWorld(by));

		// задержанный на ногах идёт сам: поворачиваем к офицеру и включаем шаг
		if (!gCarried[t] && !gWounded[t])
		{
			Drag_Walk(t, by);
			continue;
		}

		GetPlayerPos(by, x, y, z);
		GetPlayerFacingAngle(by, a);
		// раненого тащат сзади, на руках — перед грудью
		new Float:d = gCarried[t] ? 0.55 : -1.0;
		x += d * floatsin(-a, degrees);
		y += d * floatcos(-a, degrees);
		if (!IsPlayerInRangeOfPoint(t, 0.4, x, y, z))
		{
			SetPlayerPos(t, x, y, z);
			SetPlayerFacingAngle(t, a + 90.0);
		}
		// руки «с ношей» сбиваются от бега — обновляем раз в ~3 сек.
		if (gCarried[t] && ++gCarryAnimTick[t] >= 10)
		{
			gCarryAnimTick[t] = 0;
			ApplyAnimation(by, "CARRY", "crry_prtial", 4.1, true, false, false, true, 1, SYNC_ALL);
		}
	}
	return 1;
}
