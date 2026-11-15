// =====================================================================
//  admin_give.inc Ч админские выдачи: оружие, брон€, лечение, деньги.
//
//  2 Ч ћодератор:   /aheal [id] Ч вылечить (100 HP, подн€ть раненого, накормить)
//                   /setarmour [id] [0-100] (/setarm)   /disarm [id] Ч забрать оружие
//  3 Ч јдминистратор: /givegun [id] [оружие] [патроны] Ч оружие по номеру или названию
//                   (deagle, m4, ak, mp5, shotgun...). /givegun без параметров Ч список.
//  5 Ч  уратор:     /healall Ч вылечить всех онлайн
//  6 Ч «ам. гл.:    т€жЄлое оружие (миниган, –ѕ√, огнемЄт, взрывчатка)
//  7 Ч √л. админ:   /takemoney [id] [сумма]
//
//  ¬сЄ пишетс€ в лог (/alog). ќружие выдаЄтс€ через античит (anticheat.inc),
//  так что за выданное админом не кикает.
// =====================================================================

#define AGIVE_GUN_LEVEL         3       // /givegun
#define AGIVE_HEAVY_LEVEL       6       // т€жЄлое оружие
#define AGIVE_MAX_AMMO          9999
#define AGIVE_DEF_AMMO          500

//  ороткие названи€ оружи€ дл€ /givegun
enum E_GUN_ALIAS { gaName[12], gaId }
static const gGunAlias[][E_GUN_ALIAS] =
{
	{ "brass", 1 }, { "golf", 2 }, { "nitestick", 3 }, { "baton", 3 }, { "knife", 4 }, { "bat", 5 },
	{ "shovel", 6 }, { "pool", 7 }, { "katana", 8 }, { "chainsaw", 9 }, { "dildo", 10 },
	{ "flowers", 14 }, { "cane", 15 }, { "grenade", 16 }, { "teargas", 17 }, { "molotov", 18 },
	{ "colt", 22 }, { "pistol", 22 }, { "silenced", 23 }, { "deagle", 24 }, { "shotgun", 25 },
	{ "sawnoff", 26 }, { "spas", 27 }, { "uzi", 28 }, { "mp5", 29 }, { "ak", 30 }, { "ak47", 30 },
	{ "m4", 31 }, { "tec9", 32 }, { "rifle", 33 }, { "sniper", 34 }, { "rpg", 35 }, { "hs", 36 },
	{ "flame", 37 }, { "minigun", 38 }, { "satchel", 39 }, { "spray", 41 }, { "fire", 42 },
	{ "camera", 43 }, { "nv", 44 }, { "thermal", 45 }, { "parachute", 46 }
};

static bool:AGive_ValidGun(id)
{
	return (id >= 1 && id <= 18) || (id >= 22 && id <= 46 && id != 40);
}

static bool:AGive_HeavyGun(id)
{
	return (id >= 35 && id <= 39);
}

// Ќомер оружи€ по тексту: номер, короткое название или часть игрового названи€. 0 Ч не нашли.
static AGive_ParseGun(const text[])
{
	if (IsNumeric(text))
	{
		new id = strval(text);
		return AGive_ValidGun(id) ? id : 0;
	}
	for (new i = 0; i < sizeof gGunAlias; i++)
	{
		if (!strcmp(text, gGunAlias[i][gaName], true)) return gGunAlias[i][gaId];
	}
	new name[32];
	for (new id = 1; id <= 46; id++)
	{
		if (!AGive_ValidGun(id)) continue;
		name[0] = '\0';
		GetWeaponName(WEAPON:id, name, sizeof name);
		if (name[0] != '\0' && strfind(name, text, true) != -1) return id;
	}
	return 0;
}

static AGive_GunList(playerid)
{
	SendClientMessage(playerid, COLOR_GREY, "»спользование: /givegun [id] [оружие] [патроны 1-%d, по умолчанию %d]", AGIVE_MAX_AMMO, AGIVE_DEF_AMMO);
	SendClientMessage(playerid, COLOR_WHITE, "ѕистолеты: colt 22, silenced 23, deagle 24   ƒробовики: shotgun 25, sawnoff 26, spas 27");
	SendClientMessage(playerid, COLOR_WHITE, "јвтоматы: uzi 28, mp5 29, tec9 32, ak 30, m4 31   ¬интовки: rifle 33, sniper 34");
	SendClientMessage(playerid, COLOR_WHITE, "Ѕлижний бой: knife 4, bat 5, baton 3, katana 8   ѕрочее: grenade 16, teargas 17, molotov 18, spray 41, camera 43, parachute 46");
	SendClientMessage(playerid, COLOR_WHITE, "“€жЄлое (с %d ур.): rpg 35, hs 36, flame 37, minigun 38, satchel 39", AGIVE_HEAVY_LEVEL);
}

// ---------------------------------------------------------------------
stock bool:Cmd_GiveGun(playerid, const params[])
{
	if (!Admin_Check(playerid, AGIVE_GUN_LEVEL)) return true;
	new pos = 0, what[24], ammo = AGIVE_DEF_AMMO;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextParam(params, pos, what)) { AGive_GunList(playerid); return true; }
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "»грок ещЄ не вошЄл в аккаунт.");
	new gun = AGive_ParseGun(what);
	if (gun == 0) { SendClientMessage(playerid, COLOR_GREY, "Ќет такого оружи€: %s.", what); AGive_GunList(playerid); return true; }
	if (AGive_HeavyGun(gun) && !Admin_Check(playerid, AGIVE_HEAVY_LEVEL)) return true;
	NextIntParam(params, pos, ammo);
	if (ammo < 1) ammo = 1;
	if (ammo > AGIVE_MAX_AMMO) ammo = AGIVE_MAX_AMMO;

	GivePlayerWeapon(target, WEAPON:gun, ammo);
	new name[32];
	GetWeaponName(WEAPON:gun, name, sizeof name);
	Admin_Log(playerid, "givegun", target, name, ammo);
	if (target != playerid) SendClientMessage(target, COLOR_ADMIN, "јдминистратор %s выдал тебе %s (%d патр.).", GetRPName(playerid), name, ammo);
	SendClientMessage(playerid, COLOR_ADMIN, "¬ыдано %s: %s (%d патр.).", GetRPName(target), name, ammo);
	return true;
}

stock bool:Cmd_Disarm(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "»спользование: /disarm [id] Ч забрать всЄ оружие");
	ResetPlayerWeapons(target);
	Admin_Log(playerid, "disarm", target);
	if (target != playerid) SendClientMessage(target, COLOR_ADMIN, "јдминистратор %s забрал у теб€ оружие.", GetRPName(playerid));
	SendClientMessage(playerid, COLOR_ADMIN, "ќружие у %s изъ€то.", GetRPName(target));
	return true;
}

stock bool:Cmd_SetArmour(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0, ar;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, ar) || ar < 0 || ar > 100)
		return SendClientMessage(playerid, COLOR_GREY, "»спользование: /setarmour [id] [0-100]");
	SetPlayerArmour(target, float(ar));
	Admin_Log(playerid, "setarmour", target, "", ar);
	SendClientMessage(playerid, COLOR_ADMIN, "Ѕрон€ %s: %d.", GetRPName(target), ar);
	return true;
}

static AGive_Heal(target)
{
	if (Wound_IsWounded(target)) Wound_AdminRevive(target);
	SetPlayerHealth(target, 100.0);
	if (Player[target][pLogged]) Needs_Feed(target, 100);
}

stock bool:Cmd_AHeal(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0, target = playerid;
	if (params[0] != '\0')
	{
		target = NextPlayerParam(params, pos);
		if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "»спользование: /aheal [id] Ч 100 HP, подн€ть раненого, накормить");
	}
	AGive_Heal(target);
	Admin_Log(playerid, "aheal", target);
	if (target != playerid) SendClientMessage(target, COLOR_ADMIN, "јдминистратор %s теб€ вылечил.", GetRPName(playerid));
	SendClientMessage(playerid, COLOR_ADMIN, "%s вылечен.", GetRPName(target));
	return true;
}

stock bool:Cmd_HealAll(playerid)
{
	if (!Admin_Check(playerid, 5)) return true;
	new n = 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || Admin_IsInAJail(i)) continue;
		AGive_Heal(i);
		n++;
	}
	Admin_Log(playerid, "healall", INVALID_PLAYER_ID, "", n);
	SendClientMessageToAll(COLOR_ADMIN, "јдминистратор %s вылечил всех игроков.", GetRPName(playerid));
	return true;
}

stock bool:Cmd_TakeMoney(playerid, const params[])
{
	if (!Admin_Check(playerid, ADMIN_MAX_LEVEL)) return true;
	new pos = 0, amount;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, amount) || amount < 1)
		return SendClientMessage(playerid, COLOR_GREY, "»спользование: /takemoney [id] [сумма]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "»грок ещЄ не вошЄл в аккаунт.");
	new have = GetMoney(target);
	if (amount > have) amount = have;
	if (amount <= 0) return SendClientMessage(playerid, COLOR_GREY, "” %s нет наличных.", GetRPName(target));

	new reason[64];
	format(reason, sizeof reason, "админ %s (/takemoney)", GetRPName(playerid));
	GiveMoney(target, -amount, reason);
	Account_Save(target);
	Admin_Log(playerid, "takemoney", target, "", amount);
	SendClientMessage(target, COLOR_ADMIN, "јдминистратор %s изъ€л у теб€ $%d.", GetRPName(playerid), amount);
	SendClientMessage(playerid, COLOR_ADMIN, "»зъ€то у %s: $%d.", GetRPName(target), amount);
	return true;
}

// ¬ызываетс€ из Admin_OnCommand (админка уже включена). 1 Ч команда обработана.
stock AdminGive_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/givegun", true) || !strcmp(cmd, "/gun", true)) { Cmd_GiveGun(playerid, params); return 1; }
	if (!strcmp(cmd, "/disarm", true))      { Cmd_Disarm(playerid, params); return 1; }
	if (!strcmp(cmd, "/setarmour", true) || !strcmp(cmd, "/setarm", true) || !strcmp(cmd, "/setarmor", true)) { Cmd_SetArmour(playerid, params); return 1; }
	if (!strcmp(cmd, "/aheal", true))       { Cmd_AHeal(playerid, params); return 1; }
	if (!strcmp(cmd, "/healall", true))     { Cmd_HealAll(playerid); return 1; }
	if (!strcmp(cmd, "/takemoney", true))   { Cmd_TakeMoney(playerid, params); return 1; }
	return 0;
}
