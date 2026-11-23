// =====================================================================
//  skills.inc — навыки оружия с прокачкой.
//
//  7 групп оружия, навык 0..SKILL_MAX (как в GTA: чем выше, тем точнее и
//  быстрее стрельба; на 999 пистолет, обрез и Узи/Tec-9 — в обе руки).
//  Растёт за попадания (OnPlayerWeaponShot): по игроку +SKILL_HIT_PLAYER,
//  по машине или объекту +SKILL_HIT_OTHER, не больше SKILL_MINUTE_CAP
//  очков в минуту на группу (от накрутки).
//  Новые аккаунты начинают с 0, старые (до этого обновления) — с SKILL_OLD_START.
//  Хранится в accounts.wskills: "a,b,c,d,e,f,g".
//
//  Игроку навык показывается в процентах (0..999 -> 0..100%).
//  /skills — выписка из тира (docs.inc), /skills [id] — показать игроку рядом.
//  Админы 4+: /setskill [id] [группа 1-7|all] [0-100%].
// =====================================================================

#define SKILL_GROUPS        7
#define SKILL_MAX           999
#define SKILL_OLD_START     300
#define SKILL_HIT_PLAYER    3
#define SKILL_HIT_OTHER     1
#define SKILL_MINUTE_CAP    40

static const gSkillNames[SKILL_GROUPS][] = { "Пистолеты", "Desert Eagle", "Дробовики", "Обрез", "Пистолеты-пулемёты", "Автоматы", "Винтовки" };
static gSkill[MAX_PLAYERS][SKILL_GROUPS];
static gSkillGain[MAX_PLAYERS][SKILL_GROUPS];   // сколько набрано в эту минуту
static bool:gSkillDirty[MAX_PLAYERS];
static bool:gSkillLoaded[MAX_PLAYERS];

stock Skills_Init()
{
	DB_AddColumn("accounts", "wskills", "TEXT DEFAULT ''");
}

// Сразу после создания аккаунта (accounts.inc): новичок начинает с нуля
stock Skills_OnRegister(playerid)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET wskills = '0,0,0,0,0,0,0' WHERE id = %d", Player[playerid][pDbId]));
}

static Skills_Apply(playerid)
{
	// группа -> навыки GTA
	SetPlayerSkillLevel(playerid, WEAPONSKILL_PISTOL, gSkill[playerid][0]);
	SetPlayerSkillLevel(playerid, WEAPONSKILL_PISTOL_SILENCED, gSkill[playerid][0]);
	SetPlayerSkillLevel(playerid, WEAPONSKILL_DESERT_EAGLE, gSkill[playerid][1]);
	SetPlayerSkillLevel(playerid, WEAPONSKILL_SHOTGUN, gSkill[playerid][2]);
	SetPlayerSkillLevel(playerid, WEAPONSKILL_SPAS12_SHOTGUN, gSkill[playerid][2]);
	SetPlayerSkillLevel(playerid, WEAPONSKILL_SAWNOFF_SHOTGUN, gSkill[playerid][3]);
	SetPlayerSkillLevel(playerid, WEAPONSKILL_MICRO_UZI, gSkill[playerid][4]);
	SetPlayerSkillLevel(playerid, WEAPONSKILL_MP5, gSkill[playerid][4]);
	SetPlayerSkillLevel(playerid, WEAPONSKILL_AK47, gSkill[playerid][5]);
	SetPlayerSkillLevel(playerid, WEAPONSKILL_M4, gSkill[playerid][5]);
	SetPlayerSkillLevel(playerid, WEAPONSKILL_SNIPERRIFLE, gSkill[playerid][6]);
}

// После входа (Account_FinishLogin)
stock Skills_LoadPlayer(playerid)
{
	new text[64];
	gSkillLoaded[playerid] = false;
	gSkillDirty[playerid] = false;
	for (new g = 0; g < SKILL_GROUPS; g++) { gSkill[playerid][g] = SKILL_OLD_START; gSkillGain[playerid][g] = 0; }
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT wskills FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) DB_GetFieldString(r, 0, text, sizeof text);
		DB_FreeResultSet(r);
	}
	if (text[0])
	{
		new pos = 0;
		for (new g = 0; g < SKILL_GROUPS; g++)
		{
			new v = strval(text[pos]);
			if (v < 0) v = 0;
			if (v > SKILL_MAX) v = SKILL_MAX;
			gSkill[playerid][g] = v;
			new c = strfind(text, ",", false, pos);
			if (c == -1) break;
			pos = c + 1;
		}
	}
	else gSkillDirty[playerid] = true;      // старый аккаунт: записать стартовые навыки
	gSkillLoaded[playerid] = true;
	Skills_Apply(playerid);
}

stock Skills_Save(playerid)
{
	if (!gSkillLoaded[playerid] || !gSkillDirty[playerid] || Player[playerid][pDbId] == 0) return;
	gSkillDirty[playerid] = false;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET wskills = '%d,%d,%d,%d,%d,%d,%d' WHERE id = %d",
		gSkill[playerid][0], gSkill[playerid][1], gSkill[playerid][2], gSkill[playerid][3],
		gSkill[playerid][4], gSkill[playerid][5], gSkill[playerid][6], Player[playerid][pDbId]));
}

stock Skills_OnDisconnect(playerid)
{
	Skills_Save(playerid);
	gSkillLoaded[playerid] = false;
}

// GTA сбрасывает навыки не всегда, но после спавна ставим заново
stock Skills_OnSpawn(playerid)
{
	if (gSkillLoaded[playerid]) Skills_Apply(playerid);
}

// Раз в минуту: снять лимит и сохранить
stock Skills_OnMinute()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!gSkillLoaded[i]) continue;
		for (new g = 0; g < SKILL_GROUPS; g++) gSkillGain[i][g] = 0;
		Skills_Save(i);
	}
}

static Skills_Group(WEAPON:weaponid)
{
	switch (weaponid)
	{
		case WEAPON_COLT45, WEAPON_SILENCED: return 0;
		case WEAPON_DEAGLE: return 1;
		case WEAPON_SHOTGUN, WEAPON_SHOTGSPA: return 2;
		case WEAPON_SAWEDOFF: return 3;
		case WEAPON_UZI, WEAPON_MP5, WEAPON_TEC9: return 4;
		case WEAPON_AK47, WEAPON_M4: return 5;
		case WEAPON_RIFLE, WEAPON_SNIPER: return 6;
	}
	return -1;
}

// Какой ранг у навыка (для окна и сообщений)
static Skills_Rank(value, dest[], size)
{
	if (value >= SKILL_MAX) format(dest, size, "мастер");
	else if (value >= 700) format(dest, size, "профи");
	else if (value >= 400) format(dest, size, "опытный");
	else if (value >= 150) format(dest, size, "любитель");
	else format(dest, size, "новичок");
}

static Skills_Add(playerid, g, amount)
{
	if (gSkill[playerid][g] >= SKILL_MAX) return;
	new room = SKILL_MINUTE_CAP - gSkillGain[playerid][g];
	if (room <= 0) return;
	if (amount > room) amount = room;
	new before = gSkill[playerid][g];
	gSkill[playerid][g] += amount;
	if (gSkill[playerid][g] > SKILL_MAX) gSkill[playerid][g] = SKILL_MAX;
	gSkillGain[playerid][g] += amount;
	gSkillDirty[playerid] = true;
	Skills_Apply(playerid);

	// новый ранг — сообщить
	new r1[16], r2[16];
	Skills_Rank(before, r1, sizeof r1);
	Skills_Rank(gSkill[playerid][g], r2, sizeof r2);
	if (strcmp(r1, r2) != 0)
	{
		SendClientMessage(playerid, 0xFF7A1AFF, "Навык «%s» вырос: %s (%d/%d).", gSkillNames[g], r2, gSkill[playerid][g], SKILL_MAX);
		if (gSkill[playerid][g] >= SKILL_MAX && (g == 0 || g == 3 || g == 4))
			SendClientMessage(playerid, COLOR_WHITE, "Теперь это оружие можно держать в обеих руках.");
	}
}

// Из OnPlayerWeaponShot (rp.pwn)
stock Skills_OnShot(playerid, WEAPON:weaponid, BULLET_HIT_TYPE:hittype, hitid)
{
	if (!gSkillLoaded[playerid]) return;
	new g = Skills_Group(weaponid);
	if (g == -1) return;
	switch (hittype)
	{
		case BULLET_HIT_TYPE_PLAYER:
		{
			if (hitid == playerid || !IsPlayerConnected(hitid)) return;
			Skills_Add(playerid, g, SKILL_HIT_PLAYER);
		}
		case BULLET_HIT_TYPE_VEHICLE, BULLET_HIT_TYPE_OBJECT, BULLET_HIT_TYPE_PLAYER_OBJECT: Skills_Add(playerid, g, SKILL_HIT_OTHER);
	}
}

stock Skills_Get(playerid, g)
{
	if (g < 0 || g >= SKILL_GROUPS) return 0;
	return gSkill[playerid][g];
}

// Проценты для игрока: 999 -> 100
stock Skills_Pct(value)
{
	if (value >= SKILL_MAX) return 100;
	if (value <= 0) return 0;
	new p = (value * 100 + SKILL_MAX / 2) / SKILL_MAX;
	return p >= 100 ? 99 : p;
}

// Для выписки из тира: ,"skills":[["Пистолеты",30,"любитель"],...]
stock Skills_Json(playerid, dest[], size = sizeof dest)
{
	dest[0] = '\0';
	strcat(dest, ",\"skills\":[", size);
	new part[80], rank[16];
	for (new g = 0; g < SKILL_GROUPS; g++)
	{
		Skills_Rank(gSkill[playerid][g], rank, sizeof rank);
		format(part, sizeof part, "%s[\"%s\",%d,\"%s\"]", g ? "," : "", gSkillNames[g], Skills_Pct(gSkill[playerid][g]), rank);
		strcat(dest, part, size);
	}
	strcat(dest, "]", size);
}

// Для диалога выписки
stock Skills_Text(playerid, dest[], size = sizeof dest)
{
	dest[0] = '\0';
	new line[80], rank[16];
	for (new g = 0; g < SKILL_GROUPS; g++)
	{
		Skills_Rank(gSkill[playerid][g], rank, sizeof rank);
		format(line, sizeof line, "\n{8C9098}%s: {FFFFFF}%d%% {8C9098}(%s)", gSkillNames[g], Skills_Pct(gSkill[playerid][g]), rank);
		strcat(dest, line, size);
	}
}

stock bool:Cmd_Skills(playerid, const params[])
{
	if (!params[0])
	{
		Docs_ShowShooting(playerid, playerid);
		return true;
	}
	new pos = 0, target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid || !Player[target][pLogged])
	{
		SendClientMessage(playerid, COLOR_GREY, "Использование: /skills — выписка из тира, /skills [id] — показать игроку рядом.");
		return true;
	}
	if (!IsPlayerNearPlayer(playerid, target, 3.0))
	{
		SendClientMessage(playerid, COLOR_GREY, "Подойди ближе к игроку.");
		return true;
	}
	Docs_ShowShooting(playerid, target);
	new msg[128];
	format(msg, sizeof msg, "%s показывает выписку из тира %s.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_SetSkill(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, group[8], pct;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !Player[target][pLogged] || !NextParam(params, pos, group) || !NextIntParam(params, pos, pct)
		|| pct < 0 || pct > 100)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setskill [id] [группа 1-%d или all] [0-100%%]. 1 пистолеты, 2 deagle, 3 дробовики, 4 обрез, 5 ПП, 6 автоматы, 7 винтовки", SKILL_GROUPS);
	new value = (pct * SKILL_MAX + 50) / 100;
	if (!gSkillLoaded[target]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл.");
	new from = 0, to = SKILL_GROUPS - 1;
	if (strcmp(group, "all", true) != 0)
	{
		new g = strval(group) - 1;
		if (g < 0 || g >= SKILL_GROUPS) return SendClientMessage(playerid, COLOR_GREY, "Группа: 1-%d или all.", SKILL_GROUPS);
		from = to = g;
	}
	for (new g = from; g <= to; g++) gSkill[target][g] = value;
	gSkillDirty[target] = true;
	Skills_Apply(target);
	Skills_Save(target);
	Admin_Log(playerid, "setskill", target, group, value);
	SendClientMessage(playerid, COLOR_ADMIN, "Навыки %s (%s) = %d%%.", GetRPName(target), group, pct);
	SendClientMessage(target, COLOR_ADMIN, "Администратор изменил твои навыки оружия: /skills.");
	return true;
}

stock bool:Skills_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/skills", true) || !strcmp(cmd, "/skill", true)) { Cmd_Skills(playerid, params); return true; }
	if (!strcmp(cmd, "/setskill", true)) { Cmd_SetSkill(playerid, params); return true; }
	return false;
}
