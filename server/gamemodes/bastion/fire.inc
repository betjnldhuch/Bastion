// =====================================================================
//  fire.inc — пожары и пожарные.
//
//  Пожарные — фракция типа 9 (/createfaction 9 Пожарные). На смене (/duty)
//  выдаётся огнетушитель. Машины ставит админ: /addfcar (пожарная 407, лестница 544).
//
//  Пожар — несколько очагов огня. Тушат огнетушителем (зажать огонь, смотреть
//  на очаг, до 7 м) или из пожарной машины (за рулём, огонь, до 25 м).
//  За каждый потушенный очаг пожарному на смене — fire_pay, за весь пожар
//  всем пожарным на смене рядом — fire_bonus. Тушить может любой, платят только пожарным.
//  Не потушили за fire_burn минут — пожар догорает сам, и это дорого:
//    склад завода сгорает целиком, завод стоит fac_sab_down минут;
//    у бизнеса сгорает половина товара на складе.
//  Потушили — завод стоит только fac_repair минут, остальное цело.
//  Пока горит, огонь растёт (новый очаг раз в fire_spread секунд),
//  а со склада горящего завода раз в fire_loss секунд пропадает ящик.
//
//  Откуда пожары: поджог склада завода (саботаж, factions.inc), случайные
//  пожары в бизнесах раз в fire_random минут (если на смене хватает пожарных),
//  админ: /startfire, /stopfire. Список пожаров и GPS: /fires.
//  Все цифры — /srvset, раздел «Пожары».
// =====================================================================

#define MAX_FIRES           8
#define FIRE_FLAMES         8           // сколько очагов может быть у одного пожара
#define FIRE_FLAME_HP       100
#define FIRE_START_FLAMES   3
#define FIRE_OBJECT         18691       // большой огонь
#define FIRE_RADIUS         4.0         // разброс очагов от центра
#define FIRE_HOSE_RANGE     7.0         // огнетушитель
#define FIRE_TRUCK_RANGE    25.0        // пожарная машина
#define FIRE_TRUCK_MODEL    407
#define FIRE_BONUS_RANGE    60.0
#define FIRE_ROWS           MAX_FIRES


enum E_FIRE
{
	bool:frUsed,
	frSrc,
	frRef,
	Float:frX,
	Float:frY,
	Float:frZ,
	frStart,
	frLastSpread,
	frLastLoss,
	frName[48]
}
static gFire[MAX_FIRES][E_FIRE];
static gFlameObj[MAX_FIRES][FIRE_FLAMES];
static gFlameHp[MAX_FIRES][FIRE_FLAMES];
static Float:gFlameX[MAX_FIRES][FIRE_FLAMES];
static Float:gFlameY[MAX_FIRES][FIRE_FLAMES];
static Float:gFlameZ[MAX_FIRES][FIRE_FLAMES];
static gFireMap[MAX_PLAYERS][FIRE_ROWS];
static gFireNextRandom = 0;

forward Fire_OnTick();

stock bool:Faction_IsFire(f)
{
	return (f > FACTION_NONE && f < MAX_FACTIONS && Faction[f][fType] == FTYPE_FIRE);
}

static bool:Fire_IsFireman(playerid)
{
	return (Faction_IsFire(Player[playerid][pFaction]) && Player[playerid][pDuty]);
}

stock Fire_Init()
{
	for (new i = 0; i < MAX_FIRES; i++)
	{
		gFire[i][frUsed] = false;
		for (new k = 0; k < FIRE_FLAMES; k++) { gFlameObj[i][k] = INVALID_OBJECT_ID; gFlameHp[i][k] = 0; }
	}
	gFireNextRandom = gettime() + 600;
	SetTimer("Fire_OnTick", 500, true);
}

// Пожарным: тревога
stock Fire_Alarm(const text[])
{
	for (new p = 0; p < MAX_PLAYERS; p++)
	{
		if (!IsPlayerConnected(p) || !Player[p][pLogged]) continue;
		if (Faction_IsFire(Player[p][pFaction])) SendClientMessage(p, COLOR_RED, "%s", text);
	}
}

stock Fire_FiremenOnDuty()
{
	new n = 0;
	for (new p = 0; p < MAX_PLAYERS; p++)
	{
		if (IsPlayerConnected(p) && Player[p][pLogged] && Fire_IsFireman(p)) n++;
	}
	return n;
}

static Fire_Flames(i)
{
	new n = 0;
	for (new k = 0; k < FIRE_FLAMES; k++) if (gFlameHp[i][k] > 0) n++;
	return n;
}

static Fire_AddFlame(i)
{
	for (new k = 0; k < FIRE_FLAMES; k++)
	{
		if (gFlameHp[i][k] > 0) continue;
		new Float:a = float(random(360)), Float:d = (k == 0) ? 0.0 : (1.0 + float(random(30)) / 10.0);
		if (d > FIRE_RADIUS) d = FIRE_RADIUS;
		gFlameX[i][k] = gFire[i][frX] + d * floatsin(a, degrees);
		gFlameY[i][k] = gFire[i][frY] + d * floatcos(a, degrees);
		gFlameZ[i][k] = gFire[i][frZ] - 1.8;
		gFlameHp[i][k] = FIRE_FLAME_HP;
		gFlameObj[i][k] = CreateObject(FIRE_OBJECT, gFlameX[i][k], gFlameY[i][k], gFlameZ[i][k], 0.0, 0.0, 0.0, 200.0);
		return 1;
	}
	return 0;
}

static Fire_KillFlame(i, k)
{
	gFlameHp[i][k] = 0;
	if (gFlameObj[i][k] != INVALID_OBJECT_ID) { DestroyObject(gFlameObj[i][k]); gFlameObj[i][k] = INVALID_OBJECT_ID; }
}

// Горит ли уже что-то от этого источника (номер пожара или -1)
stock Fire_Find(src, ref)
{
	for (new i = 0; i < MAX_FIRES; i++)
	{
		if (gFire[i][frUsed] && gFire[i][frSrc] == src && gFire[i][frRef] == ref) return i;
	}
	return -1;
}

// Начать пожар. Возвращает номер или -1, если все места заняты.
stock Fire_Start(src, ref, Float:x, Float:y, Float:z, const name[])
{
	new i = -1;
	for (new k = 0; k < MAX_FIRES; k++) if (!gFire[k][frUsed]) { i = k; break; }
	if (i == -1) return -1;
	new now = gettime();
	gFire[i][frUsed] = true;
	gFire[i][frSrc] = src;
	gFire[i][frRef] = ref;
	gFire[i][frX] = x;
	gFire[i][frY] = y;
	gFire[i][frZ] = z;
	gFire[i][frStart] = now;
	gFire[i][frLastSpread] = now;
	gFire[i][frLastLoss] = now;
	gFire[i][frName][0] = '\0';
	strcat(gFire[i][frName], name, 48);
	for (new k = 0; k < FIRE_START_FLAMES; k++) Fire_AddFlame(i);
	new msg[144];
	format(msg, sizeof msg, "[Пожар] Горит: %s! Все на выезд. Список и GPS: /fires", name);
	Fire_Alarm(msg);
	return i;
}

// Пожар закончился: потушили (putOut) или догорел сам
stock Fire_End(i, bool:putOut)
{
	if (i < 0 || i >= MAX_FIRES || !gFire[i][frUsed]) return;
	for (new k = 0; k < FIRE_FLAMES; k++) Fire_KillFlame(i, k);
	gFire[i][frUsed] = false;
	new msg[144], src = gFire[i][frSrc], ref = gFire[i][frRef];
	if (putOut)
	{
		new bonus = Cfg(CFG_FIRE_BONUS);
		for (new p = 0; p < MAX_PLAYERS; p++)
		{
			if (!IsPlayerConnected(p) || !Player[p][pLogged] || !Fire_IsFireman(p)) continue;
			if (!IsPlayerInRangeOfPoint(p, FIRE_BONUS_RANGE, gFire[i][frX], gFire[i][frY], gFire[i][frZ])) continue;
			if (bonus > 0) GiveMoney(p, bonus, "пожар потушен");
			SendClientMessage(p, COLOR_GREEN, "Пожар потушен: %s. Премия: $%d.", gFire[i][frName], bonus);
		}
		format(msg, sizeof msg, "[Пожар] Потушено: %s.", gFire[i][frName]);
	}
	else format(msg, sizeof msg, "[Пожар] Догорело: %s. Пожар никто не потушил.", gFire[i][frName]);
	Fire_Alarm(msg);
	if (src == FIRE_SRC_FACTORY) Factory_OnFireEnd(ref, putOut);
	else if (src == FIRE_SRC_BIZ && ref >= 0 && ref < gBizCount && !putOut)
	{
		Biz[ref][bStock] /= 2;
		BizX_Save(ref);
		for (new p = 0; p < MAX_PLAYERS; p++)
		{
			if (IsPlayerConnected(p) && Player[p][pLogged] && Player[p][pDbId] == Biz[ref][bOwner])
				SendClientMessage(p, COLOR_RED, "Пожар в «%s» никто не потушил: сгорела половина товара на складе.", Biz[ref][bName]);
		}
	}
}

// Тушит ли игрок: 1 — огнетушитель, 2 — пожарная машина, 0 — нет
static Fire_Hose(playerid, &Float:range)
{
	new KEY:keys, ud, lr;
	GetPlayerKeys(playerid, keys, ud, lr);
	new PLAYER_STATE:st = GetPlayerState(playerid);
	if (st == PLAYER_STATE_ONFOOT)
	{
		if (!(keys & KEY_FIRE) || GetPlayerWeapon(playerid) != WEAPON:42) return 0;
		range = FIRE_HOSE_RANGE;
		return 1;
	}
	if (st == PLAYER_STATE_DRIVER)
	{
		if (!(keys & KEY_FIRE) && !(keys & KEY_ACTION)) return 0;
		if (GetVehicleModel(GetPlayerVehicleID(playerid)) != FIRE_TRUCK_MODEL) return 0;
		range = FIRE_TRUCK_RANGE;
		return 2;
	}
	return 0;
}

public Fire_OnTick()
{
	static tick = 0;
	tick++;
	new now = gettime();
	for (new i = 0; i < MAX_FIRES; i++)
	{
		if (!gFire[i][frUsed]) continue;
		// тушат
		for (new p = 0; p < MAX_PLAYERS; p++)
		{
			if (!IsPlayerConnected(p) || !Player[p][pLogged]) continue;
			if (!IsPlayerInRangeOfPoint(p, FIRE_TRUCK_RANGE + FIRE_RADIUS, gFire[i][frX], gFire[i][frY], gFire[i][frZ])) continue;
			new Float:range, kind = Fire_Hose(p, range);
			if (kind == 0) continue;
			new Float:px, Float:py, Float:pz, Float:face;
			GetPlayerPos(p, px, py, pz);
			if (kind == 1) GetPlayerFacingAngle(p, face);
			for (new k = 0; k < FIRE_FLAMES; k++)
			{
				if (gFlameHp[i][k] <= 0) continue;
				new Float:dx = gFlameX[i][k] - px, Float:dy = gFlameY[i][k] - py;
				if (floatsqroot(dx * dx + dy * dy) > range) continue;
				if (kind == 1)
				{
					// смотрит ли на очаг: угол в SA считается от севера против часовой
					new Float:want = atan2(-dx, dy), Float:diff = want - face;
					while (diff > 180.0) diff -= 360.0;
					while (diff < -180.0) diff += 360.0;
					if (diff > 40.0 || diff < -40.0) continue;
				}
				gFlameHp[i][k] -= (kind == 2) ? 10 : 14;
				if (gFlameHp[i][k] > 0) continue;
				Fire_KillFlame(i, k);
				new pay = Cfg(CFG_FIRE_PAY);
				if (Fire_IsFireman(p) && pay > 0)
				{
					GiveMoney(p, pay, "тушение пожара");
					GameTextForPlayer(p, "~g~+$", 1000, 3);
				}
			}
		}
		if (Fire_Flames(i) == 0) { Fire_End(i, true); continue; }
		if (tick % 2 != 0) continue;   // дальше раз в секунду
		if (now - gFire[i][frStart] >= Cfg(CFG_FIRE_BURN) * 60) { Fire_End(i, false); continue; }
		if (now - gFire[i][frLastSpread] >= Cfg(CFG_FIRE_SPREAD))
		{
			gFire[i][frLastSpread] = now;
			Fire_AddFlame(i);
		}
		if (gFire[i][frSrc] == FIRE_SRC_FACTORY && now - gFire[i][frLastLoss] >= Cfg(CFG_FIRE_LOSS))
		{
			gFire[i][frLastLoss] = now;
			Factory_FireLoss(gFire[i][frRef]);
		}
	}
	if (tick % 120 == 0) Fire_RandomTick();
	return 1;
}

// Раз в минуту: случайный пожар в бизнесе
static Fire_RandomTick()
{
	new every = Cfg(CFG_FIRE_RANDOM), now = gettime();
	if (every <= 0 || now < gFireNextRandom) return;
	gFireNextRandom = now + every * 60 * (70 + random(61)) / 100;
	if (Fire_FiremenOnDuty() < Cfg(CFG_FIRE_MIN_FF) || gBizCount <= 0) return;
	for (new tries = 0; tries < 10; tries++)
	{
		new b = random(gBizCount);
		if (Biz[b][bOwner] == 0 || Biz[b][bOInt] != 0 || Biz[b][bOWorld] != 0 || Fire_Find(FIRE_SRC_BIZ, b) != -1) continue;
		new name[48];
		format(name, sizeof name, "бизнес «%s»", Biz[b][bName]);
		Fire_Start(FIRE_SRC_BIZ, b, Biz[b][bX] + 2.0, Biz[b][bY] + 2.0, Biz[b][bZ], name);
		return;
	}
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Cmd_Fires(playerid)
{
	new body[1200], line[128], n = 0, now = gettime();
	strcat(body, "Где\tОчагов\tГорит\n");
	for (new i = 0; i < MAX_FIRES; i++)
	{
		if (!gFire[i][frUsed]) continue;
		format(line, sizeof line, "{FF6347}%s\t{FFFFFF}%d\t%d мин из %d\n", gFire[i][frName], Fire_Flames(i), (now - gFire[i][frStart]) / 60, Cfg(CFG_FIRE_BURN));
		strcat(body, line);
		gFireMap[playerid][n++] = i;
	}
	for (new k = n; k < FIRE_ROWS; k++) gFireMap[playerid][k] = -1;
	if (n == 0) return SendClientMessage(playerid, COLOR_GREY, "Сейчас нигде не горит.");
	ShowPlayerDialog(playerid, DIALOG_FIRES, DIALOG_STYLE_TABLIST_HEADERS, "Пожары: выбери для GPS", "%s", "GPS", "Закрыть", body);
	return true;
}

stock Fire_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid != DIALOG_FIRES) return 0;
	if (!response || listitem < 0 || listitem >= FIRE_ROWS) return 1;
	new i = gFireMap[playerid][listitem];
	if (i < 0 || !gFire[i][frUsed]) return 1;
	Gps_SetPoint(playerid, gFire[i][frX], gFire[i][frY], gFire[i][frZ], "Пожар");
	SendClientMessage(playerid, COLOR_WHITE, "GPS: пожар, %s. Огнетушитель: зажми огонь и смотри на очаг. Пожарная машина: огонь за рулём.", gFire[i][frName]);
	return 1;
}

stock bool:Cmd_StartFire(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	new a = gettime() % 100000, name[48];
	format(name, sizeof name, "вызов №%d", a);
	if (Fire_Start(FIRE_SRC_ADMIN, a, x + 3.0, y, z, name) == -1) return SendClientMessage(playerid, COLOR_GREY, "Горит уже слишком много пожаров (%d).", MAX_FIRES);
	return SendClientMessage(playerid, COLOR_ADMIN, "Пожар начат рядом с тобой. Пожарным на смене: %d.", Fire_FiremenOnDuty());
}

stock bool:Cmd_StopFire(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new n = 0;
	for (new i = 0; i < MAX_FIRES; i++) if (gFire[i][frUsed]) { Fire_End(i, true); n++; }
	return SendClientMessage(playerid, COLOR_ADMIN, "Потушено пожаров: %d.", n);
}
