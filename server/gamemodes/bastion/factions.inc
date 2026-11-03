// =====================================================================
//  factions.inc — фракции: полиция, больница, банды.
//
//  Фракции (номер нужен для админ-команд):
//    1 — Полиция LSPD        (розыск, наручники, арест, тюрьма)
//    2 — Больница LS (EMS)   (лечение игроков за деньги)
//    3 — Grove Street Families
//    4 — The Ballas
//    5 — Varrios Los Aztecas
//
//  В каждой фракции 6 рангов. 6 — лидер, 5 — заместитель.
//  Лидер и зам принимают (/invite), увольняют (/uninvite) и
//  повышают (/giverank) игроков.
//
//  Как назначить первого лидера (админ 4+ уровня):
//    /makeleader [id игрока] [номер фракции]
//
//  Базы и машины фракций хранятся в базе данных. Если точка стоит
//  криво — встань в нужное место и введи /setfspawn или /setfduty.
//  Машину фракции можно добавить так: /veh 596, сесть в неё и /addfcar 1
// =====================================================================

#define FACTION_DUTY_RANGE      3.0     // как близко подойти к раздевалке
#define FACTION_VEH_RESPAWN     300     // через сколько секунд брошенная машина фракции вернётся

// Тюремная камера (в здании LSPD, интерьер 6)
#define JAIL_X                  264.0
#define JAIL_Y                  77.5
#define JAIL_Z                  1001.04
#define JAIL_INTERIOR           6
// Где выпускают из тюрьмы (у входа в LSPD)
#define JAIL_RELEASE_X          1544.0
#define JAIL_RELEASE_Y          -1675.5
#define JAIL_RELEASE_Z          13.6

// Виды предложений, которые принимаются командой /accept
enum
{
	OFFER_NONE,
	OFFER_INVITE,       // приглашение во фракцию
	OFFER_HEAL          // медик предлагает лечение
}

static const gFactionNames[MAX_FACTIONS][32] =
{
	"Нет",
	"Полиция LSPD",
	"Больница LS (EMS)",
	"Grove Street Families",
	"The Ballas",
	"Varrios Los Aztecas"
};

static const gFactionColors[MAX_FACTIONS] =
{
	COLOR_CIVIL,
	0x3A6EFFFF,     // полиция — синий
	0xFF8080FF,     // медики — розовый
	0x10A010FF,     // Grove — зелёный
	0xB040FFFF,     // Ballas — фиолетовый
	0x00C8F0FF      // Aztecas — голубой
};

// Скин для каждого ранга (1..6)
static const gFactionSkins[MAX_FACTIONS][MAX_RANK] =
{
	{ 0,   0,   0,   0,   0,   0   },
	{ 280, 280, 281, 282, 288, 265 },   // полиция
	{ 274, 274, 275, 275, 276, 70  },   // медики
	{ 105, 106, 105, 106, 107, 270 },   // Grove
	{ 102, 103, 102, 104, 103, 104 },   // Ballas
	{ 114, 115, 114, 116, 115, 116 }    // Aztecas
};

// Названия рангов: 0 — полиция, 1 — медики, 2 — банды
static const gRankNames[3][MAX_RANK][24] =
{
	{ "Кадет", "Офицер", "Сержант", "Лейтенант", "Капитан", "Шеф полиции" },
	{ "Интерн", "Фельдшер", "Врач", "Хирург", "Зам. главврача", "Главврач" },
	{ "Новичок", "Боец", "Бывалый", "Старший", "Правая рука", "Лидер" }
};

// Базы по умолчанию: спавн (x, y, z, угол) и раздевалка /duty (x, y, z).
// Координаты примерные — их можно поправить в игре через /setfspawn и /setfduty.
static const Float:gFactionDefBase[MAX_FACTIONS][7] =
{
	{ 0.0, 0.0, 0.0, 0.0,          0.0, 0.0, 0.0 },
	{ 1544.0, -1675.5, 13.6, 90.0,   1553.0, -1675.5, 16.2 },   // LSPD, Pershing Square
	{ HOSPITAL_X, HOSPITAL_Y, HOSPITAL_Z, 90.0,   1172.0, -1323.3, 15.4 },   // All Saints
	{ 2512.0, -1673.0, 13.5, 90.0,   2495.9, -1690.0, 14.8 },   // Grove Street
	{ 2220.0, -1160.0, 25.8, 270.0,  2233.8, -1159.8, 25.9 },   // Jefferson Motel
	{ 1975.0, -2040.0, 13.5, 0.0,    1978.0, -2036.0, 13.5 }    // El Corona
};

// Машины по умолчанию (создаются один раз, при первом запуске)
enum E_FVEH_DEF
{
	fdFaction,
	fdModel,
	Float:fdX,
	Float:fdY,
	Float:fdZ,
	Float:fdA,
	fdC1,
	fdC2
}
static const gFactionDefVeh[][E_FVEH_DEF] =
{
	{ FACTION_POLICE, 596, 1570.3, -1710.8, 5.6, 0.0,   0, 1 },
	{ FACTION_POLICE, 596, 1578.6, -1710.8, 5.6, 0.0,   0, 1 },
	{ FACTION_POLICE, 596, 1587.4, -1710.8, 5.6, 0.0,   0, 1 },
	{ FACTION_POLICE, 596, 1595.5, -1710.8, 5.6, 0.0,   0, 1 },
	{ FACTION_EMS,    416, 1179.0, -1308.5, 14.0, 270.0, 1, 3 },
	{ FACTION_EMS,    416, 1179.0, -1315.0, 14.0, 270.0, 1, 3 },
	{ FACTION_GROVE,  492, 2455.0, -1655.8, 13.3, 90.0,  86, 86 },
	{ FACTION_GROVE,  492, 2462.0, -1655.8, 13.3, 90.0,  86, 86 }
};

// Данные фракций во время работы сервера
enum E_FACTION
{
	Float:fSpawnX,
	Float:fSpawnY,
	Float:fSpawnZ,
	Float:fSpawnA,
	Float:fDutyX,
	Float:fDutyY,
	Float:fDutyZ,
	fPickup,
	Text3D:fLabel,
	bool:fMarker        // значок раздевалки уже создан
}
new Faction[MAX_FACTIONS][E_FACTION];

new gVehFactionDb[MAX_VEHICLES];    // номер машины фракции в таблице fvehicles

// =====================================================================
//  Помощники
// =====================================================================
stock bool:Faction_IsValid(f)
{
	return (f > FACTION_NONE && f < MAX_FACTIONS);
}

stock bool:Faction_IsGang(f)
{
	return (f >= FACTION_GROVE && f < MAX_FACTIONS);
}

stock Faction_GetName(f)
{
	new name[32];
	if (f >= 0 && f < MAX_FACTIONS) strcat(name, gFactionNames[f]);
	return name;
}

stock Faction_GetColor(f)
{
	if (f < 0 || f >= MAX_FACTIONS) return COLOR_CIVIL;
	return gFactionColors[f];
}

stock Faction_GetRankName(f, rank)
{
	new name[24];
	if (!Faction_IsValid(f) || rank < 1 || rank > MAX_RANK)
	{
		strcat(name, "-");
		return name;
	}
	new t = 2;
	if (f == FACTION_POLICE) t = 0;
	else if (f == FACTION_EMS) t = 1;
	strcat(name, gRankNames[t][rank - 1]);
	return name;
}

// Стоят ли два игрока рядом (и в одном мире/интерьере)
stock bool:IsPlayerNearPlayer(playerid, target, Float:range)
{
	if (GetPlayerVirtualWorld(playerid) != GetPlayerVirtualWorld(target)) return false;
	if (GetPlayerInterior(playerid) != GetPlayerInterior(target)) return false;
	new Float:x, Float:y, Float:z;
	GetPlayerPos(target, x, y, z);
	return IsPlayerInRangeOfPoint(playerid, range, x, y, z);
}

// Проверка: игрок во фракции f (и на смене, если needDuty)
stock bool:Faction_Need(playerid, f, bool:needDuty)
{
	if (Player[playerid][pFaction] != f)
	{
		SendClientMessage(playerid, COLOR_GREY, "Эта команда только для фракции «%s».", Faction_GetName(f));
		return false;
	}
	if (needDuty && !Player[playerid][pDuty])
	{
		SendClientMessage(playerid, COLOR_GREY, "Сначала заступи на смену: /duty в раздевалке фракции.");
		return false;
	}
	return true;
}

// Проверка: лидер или заместитель любой фракции
stock bool:Faction_NeedLeader(playerid)
{
	if (!Faction_IsValid(Player[playerid][pFaction]) || Player[playerid][pRank] < MAX_RANK - 1)
	{
		SendClientMessage(playerid, COLOR_GREY, "Это могут только лидер и заместитель фракции (ранг %d+).", MAX_RANK - 1);
		return false;
	}
	return true;
}

// Сообщение всем онлайн-членам фракции
stock Faction_Message(f, colour, const message[])
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pFaction] == f)
		{
			SendClientMessage(i, colour, "%s", message);
		}
	}
}

// Цвет ника: полиция и медики — только на смене, банды — всегда
stock Faction_UpdateColor(playerid)
{
	new f = Player[playerid][pFaction];
	if (Faction_IsGang(f) || (Faction_IsValid(f) && Player[playerid][pDuty]))
		SetPlayerColor(playerid, Faction_GetColor(f));
	else
		SetPlayerColor(playerid, COLOR_CIVIL);
}

// Снять форму и оружие
stock Faction_EndDuty(playerid)
{
	if (!Player[playerid][pDuty]) return;
	Player[playerid][pDuty] = false;
	ResetPlayerWeapons(playerid);
	SetPlayerSkin(playerid, Player[playerid][pSkin]);
	SetPlayerArmour(playerid, 0.0);
	Faction_UpdateColor(playerid);
}

// Принять игрока во фракцию / выгнать (f = FACTION_NONE)
stock Faction_SetMember(playerid, f, rank)
{
	Faction_EndDuty(playerid);
	Player[playerid][pFaction] = f;
	Player[playerid][pRank] = (f == FACTION_NONE) ? 0 : rank;
	Faction_UpdateColor(playerid);
	Account_Save(playerid);
	Account_SetNormalSpawn(playerid);
}

stock Faction_Uncuff(playerid)
{
	if (!Player[playerid][pCuffed]) return;
	Player[playerid][pCuffed] = false;
	SetPlayerSpecialAction(playerid, SPECIAL_ACTION_NONE);
	TogglePlayerControllable(playerid, true);
}

// =====================================================================
//  Загрузка при старте сервера
// =====================================================================
stock Faction_CreateMarker(f)
{
	if (Faction[f][fMarker])
	{
		DestroyPickup(Faction[f][fPickup]);
		Delete3DTextLabel(Faction[f][fLabel]);
	}
	Faction[f][fPickup] = CreatePickup(1275, 1, Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ], 0);
	Faction[f][fLabel] = Create3DTextLabel("%s\n{FFFFFF}/duty — переодеться и взять снаряжение", Faction_GetColor(f),
		Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ] + 0.6, 15.0, 0, true, Faction_GetName(f));
	Faction[f][fMarker] = true;
}

stock Faction_SpawnVehicle(dbid, f, model, Float:x, Float:y, Float:z, Float:a, c1, c2)
{
	new v = CreateVehicle(model, x, y, z, a, c1, c2, FACTION_VEH_RESPAWN, (f == FACTION_POLICE || f == FACTION_EMS));
	if (v == INVALID_VEHICLE_ID) return INVALID_VEHICLE_ID;
	Vehicle_Clear(v);
	gVehFaction[v] = f;
	gVehFactionDb[v] = dbid;
	return v;
}

stock Factions_Init()
{
	// Первый запуск: записываем базы и машины по умолчанию
	new count = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM factions");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (count == 0)
	{
		for (new f = 1; f < MAX_FACTIONS; f++)
		{
			DB_FreeResultSet(DB_ExecuteQuery(gDB,
				"INSERT OR IGNORE INTO factions (id, sx, sy, sz, sa, dx, dy, dz) VALUES (%d, %f, %f, %f, %f, %f, %f, %f)",
				f, gFactionDefBase[f][0], gFactionDefBase[f][1], gFactionDefBase[f][2], gFactionDefBase[f][3],
				gFactionDefBase[f][4], gFactionDefBase[f][5], gFactionDefBase[f][6]));
		}
		for (new i = 0; i < sizeof (gFactionDefVeh); i++)
		{
			DB_FreeResultSet(DB_ExecuteQuery(gDB,
				"INSERT INTO fvehicles (faction, model, x, y, z, a, c1, c2) VALUES (%d, %d, %f, %f, %f, %f, %d, %d)",
				gFactionDefVeh[i][fdFaction], gFactionDefVeh[i][fdModel],
				gFactionDefVeh[i][fdX], gFactionDefVeh[i][fdY], gFactionDefVeh[i][fdZ], gFactionDefVeh[i][fdA],
				gFactionDefVeh[i][fdC1], gFactionDefVeh[i][fdC2]));
		}
	}

	// Базы фракций
	for (new f = 1; f < MAX_FACTIONS; f++)
	{
		Faction[f][fSpawnX] = gFactionDefBase[f][0];
		Faction[f][fSpawnY] = gFactionDefBase[f][1];
		Faction[f][fSpawnZ] = gFactionDefBase[f][2];
		Faction[f][fSpawnA] = gFactionDefBase[f][3];
		Faction[f][fDutyX]  = gFactionDefBase[f][4];
		Faction[f][fDutyY]  = gFactionDefBase[f][5];
		Faction[f][fDutyZ]  = gFactionDefBase[f][6];
	}
	r = DB_ExecuteQuery(gDB, "SELECT id, sx, sy, sz, sa, dx, dy, dz FROM factions");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new f = DB_GetFieldInt(r, 0);
			if (Faction_IsValid(f))
			{
				Faction[f][fSpawnX] = DB_GetFieldFloat(r, 1);
				Faction[f][fSpawnY] = DB_GetFieldFloat(r, 2);
				Faction[f][fSpawnZ] = DB_GetFieldFloat(r, 3);
				Faction[f][fSpawnA] = DB_GetFieldFloat(r, 4);
				Faction[f][fDutyX]  = DB_GetFieldFloat(r, 5);
				Faction[f][fDutyY]  = DB_GetFieldFloat(r, 6);
				Faction[f][fDutyZ]  = DB_GetFieldFloat(r, 7);
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	for (new f = 1; f < MAX_FACTIONS; f++) Faction_CreateMarker(f);

	// Машины фракций
	new loaded = 0;
	r = DB_ExecuteQuery(gDB, "SELECT id, faction, model, x, y, z, a, c1, c2 FROM fvehicles");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new f = DB_GetFieldInt(r, 1);
			if (Faction_IsValid(f))
			{
				if (Faction_SpawnVehicle(DB_GetFieldInt(r, 0), f, DB_GetFieldInt(r, 2),
					DB_GetFieldFloat(r, 3), DB_GetFieldFloat(r, 4), DB_GetFieldFloat(r, 5), DB_GetFieldFloat(r, 6),
					DB_GetFieldInt(r, 7), DB_GetFieldInt(r, 8)) != INVALID_VEHICLE_ID) loaded++;
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Factions] Loaded %d faction vehicles.", loaded);
}

// =====================================================================
//  События (вызываются из rp.pwn и accounts.inc)
// =====================================================================

// Где спавнится член фракции (дом игрока важнее — см. Account_SetNormalSpawn)
stock Factions_GetSpawn(playerid, &Float:x, &Float:y, &Float:z, &Float:a)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return 0;
	x = Faction[f][fSpawnX];
	y = Faction[f][fSpawnY];
	z = Faction[f][fSpawnZ];
	a = Faction[f][fSpawnA];
	return 1;
}

stock Jail_PutInCell(playerid)
{
	SetPlayerInterior(playerid, JAIL_INTERIOR);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerPos(playerid, JAIL_X, JAIL_Y, JAIL_Z);
	ResetPlayerWeapons(playerid);
	Player[playerid][pHouseInside] = -1;
}

stock Jail_Release(playerid)
{
	Player[playerid][pJail] = 0;
	SetPlayerInterior(playerid, 0);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerPos(playerid, JAIL_RELEASE_X, JAIL_RELEASE_Y, JAIL_RELEASE_Z);
	SendClientMessage(playerid, COLOR_GREEN, "Срок закончился. Ты на свободе.");
	Account_Save(playerid);
}

// Из OnPlayerSpawn
stock Factions_OnSpawn(playerid)
{
	Player[playerid][pDuty] = false;
	Faction_Uncuff(playerid);
	Faction_UpdateColor(playerid);
	SetPlayerWantedLevel(playerid, Player[playerid][pWanted]);

	if (Player[playerid][pJail] > 0)
	{
		Jail_PutInCell(playerid);
		SendClientMessage(playerid, COLOR_RED, "Ты в тюрьме. Осталось сидеть: %d мин.", Player[playerid][pJail]);
	}
}

// Из OnPlayerDeath
stock Factions_OnDeath(playerid)
{
	Faction_Uncuff(playerid);
	Player[playerid][pDuty] = false;
}

// Из OnPlayerStateChange. Возвращает 1, если игрока высадили из чужой машины.
stock Factions_OnBecomeDriver(playerid)
{
	new v = GetPlayerVehicleID(playerid);
	new f = gVehFaction[v];
	if (f == FACTION_NONE) return 0;
	if (Player[playerid][pFaction] == f || Player[playerid][pAdmin] >= 3)
	{
		SendClientMessage(playerid, Faction_GetColor(f), "Машина фракции «%s». Двигатель: /engine", Faction_GetName(f));
		return 1;
	}
	RemovePlayerFromVehicle(playerid);
	SendClientMessage(playerid, COLOR_GREY, "Это машина фракции «%s». Водить её могут только её члены.", Faction_GetName(f));
	return 1;
}

// Раз в минуту (из OnMinuteTimer): тюремный срок
stock Factions_OnMinute()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || Player[i][pJail] <= 0) continue;

		Player[i][pJail]--;
		if (Player[i][pJail] <= 0)
		{
			Jail_Release(i);
			continue;
		}
		// Сбежал из камеры — возвращаем
		if (GetPlayerInterior(i) != JAIL_INTERIOR || !IsPlayerInRangeOfPoint(i, 15.0, JAIL_X, JAIL_Y, JAIL_Z))
		{
			Jail_PutInCell(i);
		}
		SendClientMessage(i, COLOR_GREY, "До освобождения: %d мин.", Player[i][pJail]);
	}
}

// Зарплата фракции на PayDay (только госструктуры). Возвращает сумму.
stock Factions_GetSalary(playerid)
{
	new f = Player[playerid][pFaction];
	if (f != FACTION_POLICE && f != FACTION_EMS) return 0;
	return FACTION_SALARY_BASE + Player[playerid][pRank] * FACTION_SALARY_RANK;
}

// =====================================================================
//  Общие команды фракций
// =====================================================================
stock bool:Cmd_FactionChat(playerid, const params[])
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь во фракции.");
	new text[100], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /f [текст] — рация фракции");
	format(msg, sizeof msg, "[Рация] %s %s: %s", Faction_GetRankName(f, Player[playerid][pRank]), GetRPName(playerid), text);
	Faction_Message(f, Faction_GetColor(f), msg);
	return true;
}

stock bool:Cmd_Members(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь во фракции.");
	SendClientMessage(playerid, Faction_GetColor(f), "Онлайн во фракции «%s»:", Faction_GetName(f));
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || Player[i][pFaction] != f) continue;
		new duty[24];
		if (Player[i][pDuty]) strcat(duty, " {33AA33}на смене");
		SendClientMessage(playerid, COLOR_WHITE, " %s [%d] — %s (%d)%s", GetRPName(i), i,
			Faction_GetRankName(f, Player[i][pRank]), Player[i][pRank], duty);
	}
	return true;
}

stock bool:Cmd_Duty(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь во фракции.");
	if (Player[playerid][pJail] > 0) return SendClientMessage(playerid, COLOR_GREY, "Ты в тюрьме.");
	if (!IsPlayerInRangeOfPoint(playerid, FACTION_DUTY_RANGE, Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ]))
		return SendClientMessage(playerid, COLOR_GREY, "Подойди к раздевалке своей фракции (значок футболки на базе).");

	new msg[144];
	if (Player[playerid][pDuty])
	{
		Faction_EndDuty(playerid);
		format(msg, sizeof msg, "%s переодевается в обычную одежду.", GetRPName(playerid));
		SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
		return SendClientMessage(playerid, COLOR_WHITE, "Ты закончил смену.");
	}

	new rank = Player[playerid][pRank];
	if (rank < 1 || rank > MAX_RANK) rank = 1;
	Player[playerid][pDuty] = true;
	SetPlayerSkin(playerid, gFactionSkins[f][rank - 1]);
	ResetPlayerWeapons(playerid);

	switch (f)
	{
		case FACTION_POLICE:
		{
			GivePlayerWeapon(playerid, WEAPON_NITESTICK, 1);
			GivePlayerWeapon(playerid, WEAPON_DEAGLE, 70);
			if (rank >= 3) GivePlayerWeapon(playerid, WEAPON_SHOTGUN, 30);
			if (rank >= 4) GivePlayerWeapon(playerid, WEAPON_MP5, 200);
			SetPlayerArmour(playerid, 100.0);
		}
		case FACTION_EMS:
		{
			GivePlayerWeapon(playerid, WEAPON_FIREEXTINGUISHER, 500);
		}
		default:    // банды
		{
			GivePlayerWeapon(playerid, WEAPON_BAT, 1);
			GivePlayerWeapon(playerid, WEAPON_COLT45, 60);
			if (rank >= 4) GivePlayerWeapon(playerid, WEAPON_DEAGLE, 35);
		}
	}
	Faction_UpdateColor(playerid);
	format(msg, sizeof msg, "%s переодевается в форму.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(playerid, Faction_GetColor(f), "Ты на смене (%s). Команды фракции: /fhelp", Faction_GetRankName(f, rank));
	return true;
}

stock bool:Cmd_Invite(playerid, const params[])
{
	if (!Faction_NeedLeader(playerid)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /invite [id]");
	if (target == playerid || !Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Неверный игрок.");
	if (Player[target][pFaction] != FACTION_NONE) return SendClientMessage(playerid, COLOR_GREY, "Игрок уже состоит во фракции.");
	if (!IsPlayerNearPlayer(playerid, target, 5.0)) return SendClientMessage(playerid, COLOR_GREY, "Игрок слишком далеко.");

	Player[target][pOfferType] = OFFER_INVITE;
	Player[target][pOfferFrom] = playerid;
	Player[target][pOfferFromDb] = Player[playerid][pDbId];
	Player[target][pOfferValue] = Player[playerid][pFaction];
	SendClientMessage(target, COLOR_BLUE, "%s приглашает тебя во фракцию «%s». Принять: /accept",
		GetRPName(playerid), Faction_GetName(Player[playerid][pFaction]));
	SendClientMessage(playerid, COLOR_WHITE, "Ты пригласил %s. Ждём ответа.", GetRPName(target));
	return true;
}

stock bool:Cmd_Uninvite(playerid, const params[])
{
	if (!Faction_NeedLeader(playerid)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	new f = Player[playerid][pFaction];
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /uninvite [id]");
	if (target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Чтобы уйти самому: /fleave");
	if (Player[target][pFaction] != f) return SendClientMessage(playerid, COLOR_GREY, "Игрок не в твоей фракции.");
	if (Player[target][pRank] >= Player[playerid][pRank]) return SendClientMessage(playerid, COLOR_GREY, "Нельзя уволить равного или старшего по рангу.");

	Faction_SetMember(target, FACTION_NONE, 0);
	SendClientMessage(target, COLOR_RED, "%s выгнал тебя из фракции «%s».", GetRPName(playerid), Faction_GetName(f));
	new msg[144];
	format(msg, sizeof msg, "[Фракция] %s уволил %s.", GetRPName(playerid), GetRPName(target));
	Faction_Message(f, Faction_GetColor(f), msg);
	return true;
}

stock bool:Cmd_GiveRank(playerid, const params[])
{
	if (!Faction_NeedLeader(playerid)) return true;
	new pos = 0, rank;
	new target = NextPlayerParam(params, pos);
	new f = Player[playerid][pFaction];
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, rank))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /giverank [id] [ранг 1-%d]", Player[playerid][pRank] - 1);
	if (target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Себе ранг менять нельзя.");
	if (Player[target][pFaction] != f) return SendClientMessage(playerid, COLOR_GREY, "Игрок не в твоей фракции.");
	if (Player[target][pRank] >= Player[playerid][pRank]) return SendClientMessage(playerid, COLOR_GREY, "Нельзя менять ранг равному или старшему.");
	if (rank < 1 || rank >= Player[playerid][pRank])
		return SendClientMessage(playerid, COLOR_GREY, "Ранг должен быть от 1 до %d.", Player[playerid][pRank] - 1);

	Player[target][pRank] = rank;
	Account_Save(target);
	new msg[144];
	format(msg, sizeof msg, "[Фракция] %s назначил %s на ранг «%s» (%d).", GetRPName(playerid), GetRPName(target),
		Faction_GetRankName(f, rank), rank);
	Faction_Message(f, Faction_GetColor(f), msg);
	return true;
}

stock bool:Cmd_FLeave(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь во фракции.");
	Faction_SetMember(playerid, FACTION_NONE, 0);
	SendClientMessage(playerid, COLOR_WHITE, "Ты ушёл из фракции «%s».", Faction_GetName(f));
	new msg[144];
	format(msg, sizeof msg, "[Фракция] %s ушёл из фракции.", GetRPName(playerid));
	Faction_Message(f, Faction_GetColor(f), msg);
	return true;
}

// /accept — принять приглашение или лечение
stock bool:Cmd_Accept(playerid)
{
	new type = Player[playerid][pOfferType];
	new from = Player[playerid][pOfferFrom];
	Player[playerid][pOfferType] = OFFER_NONE;

	if (type == OFFER_NONE) return SendClientMessage(playerid, COLOR_GREY, "Тебе ничего не предлагали.");
	// предложивший мог выйти с сервера — проверяем, что это тот же человек
	if (from < 0 || from >= MAX_PLAYERS || !IsPlayerConnected(from) || !Player[from][pLogged]
		|| Player[from][pDbId] != Player[playerid][pOfferFromDb])
		return SendClientMessage(playerid, COLOR_GREY, "Предложение больше не действует.");

	switch (type)
	{
		case OFFER_INVITE:
		{
			new f = Player[playerid][pOfferValue];
			if (Player[from][pFaction] != f || Player[playerid][pFaction] != FACTION_NONE)
				return SendClientMessage(playerid, COLOR_GREY, "Предложение больше не действует.");
			Faction_SetMember(playerid, f, 1);
			SendClientMessage(playerid, Faction_GetColor(f), "Добро пожаловать во фракцию «%s»! Команды: /fhelp", Faction_GetName(f));
			new msg[144];
			format(msg, sizeof msg, "[Фракция] %s принял %s во фракцию.", GetRPName(from), GetRPName(playerid));
			Faction_Message(f, Faction_GetColor(f), msg);
		}
		case OFFER_HEAL:
		{
			new price = Player[playerid][pOfferValue];
			if (Player[from][pFaction] != FACTION_EMS || !Player[from][pDuty] || !IsPlayerNearPlayer(playerid, from, 5.0))
				return SendClientMessage(playerid, COLOR_GREY, "Медик ушёл или закончил смену.");
			if (GetMoney(playerid) < price) return SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", price);

			GiveMoney(playerid, -price);
			GiveMoney(from, price);
			SetPlayerHealth(playerid, 100.0);
			Account_Save(playerid);
			Account_Save(from);
			new msg[144];
			format(msg, sizeof msg, "%s оказывает медицинскую помощь %s.", GetRPName(from), GetRPName(playerid));
			SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
			SendClientMessage(from, COLOR_GREEN, "Ты вылечил пациента. +$%d.", price);
		}
	}
	return true;
}

// =====================================================================
//  Полиция
// =====================================================================
stock bool:Cmd_Megaphone(playerid, const params[])
{
	if (!Faction_Need(playerid, FACTION_POLICE, true)) return true;
	new text[110], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /m [текст] — мегафон");
	format(msg, sizeof msg, "[Мегафон] %s: %s", GetRPName(playerid), text);
	SendNearbyMessage(playerid, 60.0, COLOR_YELLOW, msg);
	return true;
}

stock bool:Cmd_Suspect(playerid, const params[])
{
	if (!Faction_Need(playerid, FACTION_POLICE, true)) return true;
	new pos = 0, stars, reason[64];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, stars) || stars < 1 || stars > 6 || !RestParam(params, pos, reason))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /su [id] [звёзды 1-6] [причина]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	if (Player[target][pFaction] == FACTION_POLICE && Player[target][pDuty])
		return SendClientMessage(playerid, COLOR_GREY, "Нельзя объявить в розыск полицейского на смене.");

	Player[target][pWanted] = stars;
	SetPlayerWantedLevel(target, stars);
	Account_Save(target);
	SendClientMessage(target, COLOR_RED, "Тебя объявили в розыск (%d зв.). Причина: %s", stars, reason);
	new msg[144];
	format(msg, sizeof msg, "[Диспетчер] %s объявил в розыск %s [%d] — %d зв. Причина: %s",
		GetRPName(playerid), GetRPName(target), target, stars, reason);
	Faction_Message(FACTION_POLICE, Faction_GetColor(FACTION_POLICE), msg);
	return true;
}

stock bool:Cmd_ClearWanted(playerid, const params[])
{
	if (!Faction_Need(playerid, FACTION_POLICE, true)) return true;
	if (Player[playerid][pRank] < 2) return SendClientMessage(playerid, COLOR_GREY, "Снимать розыск можно с ранга 2.");
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /clear [id]");

	Player[target][pWanted] = 0;
	SetPlayerWantedLevel(target, 0);
	Account_Save(target);
	SendClientMessage(target, COLOR_GREEN, "С тебя сняли розыск.");
	new msg[144];
	format(msg, sizeof msg, "[Диспетчер] %s снял розыск с %s.", GetRPName(playerid), GetRPName(target));
	Faction_Message(FACTION_POLICE, Faction_GetColor(FACTION_POLICE), msg);
	return true;
}

stock bool:Cmd_Wanted(playerid)
{
	if (!Faction_Need(playerid, FACTION_POLICE, false)) return true;
	SendClientMessage(playerid, Faction_GetColor(FACTION_POLICE), "Разыскиваемые онлайн:");
	new count = 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || Player[i][pWanted] <= 0) continue;
		SendClientMessage(playerid, COLOR_WHITE, " %s [%d] — %d зв.", GetRPName(i), i, Player[i][pWanted]);
		count++;
	}
	if (count == 0) SendClientMessage(playerid, COLOR_GREY, " Никого нет.");
	return true;
}

stock bool:Cmd_Cuff(playerid, const params[])
{
	if (!Faction_Need(playerid, FACTION_POLICE, true)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /cuff [id]");
	if (!IsPlayerNearPlayer(playerid, target, 2.5)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе.");
	if (IsPlayerInAnyVehicle(target)) return SendClientMessage(playerid, COLOR_GREY, "Сначала выведи человека из машины.");
	if (Player[target][pCuffed]) return SendClientMessage(playerid, COLOR_GREY, "Он уже в наручниках.");

	Player[target][pCuffed] = true;
	TogglePlayerControllable(target, false);
	SetPlayerSpecialAction(target, SPECIAL_ACTION_CUFFED);
	new msg[144];
	format(msg, sizeof msg, "%s надевает наручники на %s.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_Uncuff(playerid, const params[])
{
	if (!Faction_Need(playerid, FACTION_POLICE, true)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /uncuff [id]");
	if (!Player[target][pCuffed]) return SendClientMessage(playerid, COLOR_GREY, "Он не в наручниках.");
	if (!IsPlayerNearPlayer(playerid, target, 3.0)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе.");

	Faction_Uncuff(target);
	new msg[144];
	format(msg, sizeof msg, "%s снимает наручники с %s.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

// Посадить задержанного в свою машину (полицейский сидит за рулём)
stock bool:Cmd_PutCar(playerid, const params[])
{
	if (!Faction_Need(playerid, FACTION_POLICE, true)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /putcar [id] (сидя за рулём)");
	new v = GetPlayerVehicleID(playerid);
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || gVehFaction[v] != FACTION_POLICE)
		return SendClientMessage(playerid, COLOR_GREY, "Сядь за руль полицейской машины.");
	if (!Player[target][pCuffed]) return SendClientMessage(playerid, COLOR_GREY, "Сначала надень наручники: /cuff");
	if (!IsPlayerNearPlayer(playerid, target, 6.0)) return SendClientMessage(playerid, COLOR_GREY, "Задержанный слишком далеко от машины.");

	// ищем свободное место (1 — рядом с водителем, 2 и 3 — сзади)
	new seat = -1;
	for (new s = 2; s >= 1 && seat == -1; s--)
	{
		new bool:busy = false;
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (IsPlayerConnected(i) && GetPlayerVehicleID(i) == v && GetPlayerVehicleSeat(i) == s) { busy = true; break; }
		}
		if (!busy) seat = s;
	}
	if (seat == -1) return SendClientMessage(playerid, COLOR_GREY, "В машине нет свободных мест.");

	PutPlayerInVehicle(target, v, seat);
	TogglePlayerControllable(target, false);
	new msg[144];
	format(msg, sizeof msg, "%s сажает %s в патрульную машину.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_Arrest(playerid, const params[])
{
	if (!Faction_Need(playerid, FACTION_POLICE, true)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /arrest [id]");
	new f = FACTION_POLICE;
	if (!IsPlayerInRangeOfPoint(playerid, 25.0, Faction[f][fSpawnX], Faction[f][fSpawnY], Faction[f][fSpawnZ])
		&& !IsPlayerInRangeOfPoint(playerid, 25.0, Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ]))
		return SendClientMessage(playerid, COLOR_GREY, "Арестовать можно только у участка LSPD.");
	if (!IsPlayerNearPlayer(playerid, target, 8.0)) return SendClientMessage(playerid, COLOR_GREY, "Задержанный слишком далеко.");
	if (!Player[target][pCuffed]) return SendClientMessage(playerid, COLOR_GREY, "Сначала надень наручники: /cuff");
	if (Player[target][pWanted] <= 0) return SendClientMessage(playerid, COLOR_GREY, "Этот человек не в розыске (/su).");

	new stars = Player[target][pWanted];
	new minutes = stars * JAIL_MIN_PER_STAR;
	if (IsPlayerInAnyVehicle(target)) RemovePlayerFromVehicle(target, true);
	Faction_Uncuff(target);
	Player[target][pWanted] = 0;
	SetPlayerWantedLevel(target, 0);
	Player[target][pJail] = minutes;
	Jail_PutInCell(target);
	Account_Save(target);

	GiveMoney(playerid, stars * ARREST_REWARD);
	Account_Save(playerid);

	SendClientMessage(target, COLOR_RED, "Тебя посадили в тюрьму на %d мин.", minutes);
	SendClientMessage(playerid, COLOR_GREEN, "Арест оформлен. Премия: $%d.", stars * ARREST_REWARD);
	new msg[144];
	format(msg, sizeof msg, "[Диспетчер] %s арестовал %s на %d мин.", GetRPName(playerid), GetRPName(target), minutes);
	Faction_Message(FACTION_POLICE, Faction_GetColor(FACTION_POLICE), msg);
	return true;
}

// =====================================================================
//  Медики
// =====================================================================
stock bool:Cmd_Heal(playerid, const params[])
{
	if (!Faction_Need(playerid, FACTION_EMS, true)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /heal [id]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	if (!IsPlayerNearPlayer(playerid, target, 4.0)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе к пациенту.");
	new Float:hp;
	GetPlayerHealth(target, hp);
	if (hp >= 100.0) return SendClientMessage(playerid, COLOR_GREY, "Пациент здоров.");

	Player[target][pOfferType] = OFFER_HEAL;
	Player[target][pOfferFrom] = playerid;
	Player[target][pOfferFromDb] = Player[playerid][pDbId];
	Player[target][pOfferValue] = HEAL_PRICE;
	SendClientMessage(target, COLOR_BLUE, "Медик %s предлагает вылечить тебя за $%d. Согласиться: /accept", GetRPName(playerid), HEAL_PRICE);
	SendClientMessage(playerid, COLOR_WHITE, "Ты предложил лечение %s.", GetRPName(target));
	return true;
}

// =====================================================================
//  Справка
// =====================================================================
stock bool:Cmd_FHelp(playerid)
{
	new text[1400];
	strcat(text, "{FFFF00}Все фракции\n");
	strcat(text, "{FFFFFF}/f — рация   /members — кто онлайн   /duty — смена (в раздевалке)   /fleave — уйти\n");
	strcat(text, "/accept — принять приглашение или лечение\n\n");
	strcat(text, "{FFFF00}Лидер и заместитель (ранг 5-6)\n");
	strcat(text, "{FFFFFF}/invite [id]   /uninvite [id]   /giverank [id] [ранг]\n\n");
	strcat(text, "{3A6EFF}Полиция LSPD\n");
	strcat(text, "{FFFFFF}/su [id] [звёзды] [причина] — розыск   /clear [id] — снять розыск   /wanted — список\n");
	strcat(text, "/cuff /uncuff [id] — наручники   /putcar [id] — посадить в машину\n");
	strcat(text, "/arrest [id] — посадить в тюрьму (у участка)   /m — мегафон\n\n");
	strcat(text, "{FF8080}Больница (EMS)\n");
	strcat(text, "{FFFFFF}/heal [id] — предложить лечение\n\n");
	strcat(text, "{10A010}Банды\n");
	strcat(text, "{FFFFFF}/duty на базе — форма банды и оружие. Войны за районы — скоро.\n\n");
	strcat(text, "{FF66FF}Админам (4+)\n");
	strcat(text, "{FFFFFF}/makeleader /setfaction /setfspawn /setfduty /addfcar /delfcar   (2+) /jail /unjail");
	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "Фракции", "%s", "Закрыть", "", text);
	return true;
}

// =====================================================================
//  Админ-команды фракций
// =====================================================================
stock bool:Cmd_SetFaction(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f, rank = 1;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, f) || f < 0 || f >= MAX_FACTIONS)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setfaction [id] [фракция 0-%d] [ранг 1-%d]  (0 — выгнать)", MAX_FACTIONS - 1, MAX_RANK);
	NextIntParam(params, pos, rank);
	if (rank < 1 || rank > MAX_RANK) rank = 1;
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");

	Faction_SetMember(target, f, rank);
	if (f == FACTION_NONE)
	{
		SendClientMessage(target, COLOR_ADMIN, "Администратор %s убрал тебя из фракции.", GetRPName(playerid));
		SendClientMessage(playerid, COLOR_ADMIN, "%s больше не во фракции.", GetRPName(target));
	}
	else
	{
		SendClientMessage(target, COLOR_ADMIN, "Администратор %s назначил тебя: %s, ранг %d. Команды: /fhelp",
			GetRPName(playerid), Faction_GetName(f), rank);
		SendClientMessage(playerid, COLOR_ADMIN, "%s теперь: %s, ранг %d.", GetRPName(target), Faction_GetName(f), rank);
	}
	return true;
}

stock bool:Cmd_MakeLeader(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, f) || !Faction_IsValid(f))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /makeleader [id] [фракция 1-%d]  (1 LSPD, 2 EMS, 3 Grove, 4 Ballas, 5 Aztecas)", MAX_FACTIONS - 1);
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");

	Faction_SetMember(target, f, MAX_RANK);
	SendClientMessageToAll(Faction_GetColor(f), "%s назначен лидером фракции «%s».", GetRPName(target), Faction_GetName(f));
	return true;
}

stock Faction_SaveBase(f)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"UPDATE factions SET sx = %f, sy = %f, sz = %f, sa = %f, dx = %f, dy = %f, dz = %f WHERE id = %d",
		Faction[f][fSpawnX], Faction[f][fSpawnY], Faction[f][fSpawnZ], Faction[f][fSpawnA],
		Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ], f));
}

stock bool:Cmd_SetFSpawn(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f;
	if (!NextIntParam(params, pos, f) || !Faction_IsValid(f))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setfspawn [фракция 1-%d] — спавн фракции там, где ты стоишь", MAX_FACTIONS - 1);
	if (GetPlayerInterior(playerid) != 0) return SendClientMessage(playerid, COLOR_GREY, "Спавн можно ставить только на улице.");
	GetPlayerPos(playerid, Faction[f][fSpawnX], Faction[f][fSpawnY], Faction[f][fSpawnZ]);
	GetPlayerFacingAngle(playerid, Faction[f][fSpawnA]);
	Faction_SaveBase(f);
	SendClientMessage(playerid, COLOR_ADMIN, "Спавн фракции «%s» перенесён сюда.", Faction_GetName(f));
	return true;
}

stock bool:Cmd_SetFDuty(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f;
	if (!NextIntParam(params, pos, f) || !Faction_IsValid(f))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setfduty [фракция 1-%d] — раздевалка там, где ты стоишь", MAX_FACTIONS - 1);
	if (GetPlayerInterior(playerid) != 0) return SendClientMessage(playerid, COLOR_GREY, "Раздевалку можно ставить только на улице.");
	GetPlayerPos(playerid, Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ]);
	Faction_SaveBase(f);
	Faction_CreateMarker(f);
	SendClientMessage(playerid, COLOR_ADMIN, "Раздевалка фракции «%s» перенесена сюда.", Faction_GetName(f));
	return true;
}

stock bool:Cmd_AddFCar(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f;
	if (!NextIntParam(params, pos, f) || !Faction_IsValid(f))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /addfcar [фракция 1-%d] (сидя в машине из /veh)", MAX_FACTIONS - 1);
	new v = GetPlayerVehicleID(playerid);
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || !gVehAdmin[v])
		return SendClientMessage(playerid, COLOR_GREY, "Создай машину через /veh, поставь её на место и сядь за руль.");

	new model = GetVehicleModel(v), c1, c2;
	new Float:x, Float:y, Float:z, Float:a;
	GetVehiclePos(v, x, y, z);
	GetVehicleZAngle(v, a);
	GetVehicleColours(v, c1, c2);

	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"INSERT INTO fvehicles (faction, model, x, y, z, a, c1, c2) VALUES (%d, %d, %f, %f, %f, %f, %d, %d)",
		f, model, x, y, z, a, c1, c2));
	new dbid = DB_LastInsertId();

	Vehicle_Clear(v);
	DestroyVehicle(v);
	new nv = Faction_SpawnVehicle(dbid, f, model, x, y, z, a, c1, c2);
	if (nv != INVALID_VEHICLE_ID) PutPlayerInVehicle(playerid, nv, 0);
	SendClientMessage(playerid, COLOR_ADMIN, "Машина добавлена фракции «%s» (№%d). Удалить: /delfcar", Faction_GetName(f), dbid);
	return true;
}

stock bool:Cmd_DelFCar(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new v = GetPlayerVehicleID(playerid);
	if (v == 0 || gVehFaction[v] == FACTION_NONE) return SendClientMessage(playerid, COLOR_GREY, "Сядь в машину фракции.");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM fvehicles WHERE id = %d", gVehFactionDb[v]));
	Vehicle_Clear(v);
	DestroyVehicle(v);
	SendClientMessage(playerid, COLOR_ADMIN, "Машина фракции удалена навсегда.");
	return true;
}

stock bool:Cmd_Jail(playerid, const params[])
{
	if (!Admin_CheckPeace(playerid, 2)) return true;
	new pos = 0, minutes, reason[64];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, minutes) || minutes < 1 || minutes > 600)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /jail [id] [минуты 1-600] [причина]");
	if (!RestParam(params, pos, reason)) strcat(reason, "не указана");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");

	Faction_Uncuff(target);
	Faction_EndDuty(target);
	if (IsPlayerInAnyVehicle(target)) RemovePlayerFromVehicle(target, true);
	Player[target][pJail] = minutes;
	Jail_PutInCell(target);
	Account_Save(target);
	SendClientMessageToAll(COLOR_RED, "Администратор %s посадил %s в тюрьму на %d мин. Причина: %s",
		GetRPName(playerid), GetRPName(target), minutes, reason);
	return true;
}

stock bool:Cmd_Unjail(playerid, const params[])
{
	if (!Admin_CheckPeace(playerid, 2)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /unjail [id]");
	if (Player[target][pJail] <= 0) return SendClientMessage(playerid, COLOR_GREY, "Игрок не в тюрьме.");
	Jail_Release(target);
	SendClientMessage(playerid, COLOR_ADMIN, "%s выпущен из тюрьмы.", GetRPName(target));
	return true;
}
