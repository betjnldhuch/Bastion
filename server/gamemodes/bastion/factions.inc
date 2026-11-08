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
//
//  Основа фракций (всё хранится в базе rp.db):
//    - название, цвет, названия рангов, скины и зарплаты рангов
//      (таблицы factions и faction_ranks). При первом запуске туда
//      записываются значения по умолчанию из этого файла.
//    - общак (бюджет) фракции: /fbank на базе. Госструктуры получают
//      FACTION_STATE_BUDGET каждый PayDay, банды — FACTION_TURF_BANK
//      за каждый свой район. Зарплаты на PayDay платятся из общака.
//    - склад фракции: /fstorage на базе. Лидер и зам закупают снаряжение
//      за деньги общака, члены фракции берут его с нужного ранга.
//    - выговоры: FACTION_MAX_WARNS выговоров — автоматическое увольнение.
//    - журнал действий фракции (кто кого принял, уволил, что взял со склада).
//    - панель лидера /lmenu: состав (и оффлайн), ранги, зарплаты, скины,
//      выговоры, общак, склад, журнал.
//  Админы (4+): /fedit [фракция] name|color|bank — название, цвет, общак.
// =====================================================================

#define FACTION_DUTY_RANGE      3.0     // как близко подойти к раздевалке
#define FACTION_VEH_RESPAWN     300     // через сколько секунд брошенная машина фракции вернётся
#define FACTION_BASE_RANGE      10.0    // как близко к раздевалке работают /fbank и /fstorage

#define FACTION_STATE_BUDGET    15000   // в общак полиции и больницы каждый PayDay
#define FACTION_TURF_BANK       300     // в общак банды за каждый её район каждый PayDay
#define FACTION_START_BANK      30000   // стартовый общак госструктур (только при первом запуске)
#define FACTION_BANK_MAX        100000000
#define FACTION_MAX_WARNS       3       // столько выговоров — и игрок уволен
#define FACTION_MAX_SALARY      20000   // максимальная зарплата ранга, которую может поставить лидер
#define FACTION_LOG_SHOW        25      // сколько последних записей журнала показывать
#define FACTION_LIST_MAX        40      // сколько членов фракции показывать в /lmenu

// Окна (диалоги) фракций. Номера с 700, чтобы не пересекаться с config.inc.
enum
{
	DIALOG_LMENU = 700,
	DIALOG_FBANK,
	DIALOG_FBANK_DEPOSIT,
	DIALOG_FBANK_WITHDRAW,
	DIALOG_FSTORE,
	DIALOG_FSTORE_BUY,
	DIALOG_FSTORE_BUY_QTY,
	DIALOG_FSTORE_RANK,
	DIALOG_LMEMBERS,
	DIALOG_LMEMBER_ACT,
	DIALOG_LMEMBER_RANK,
	DIALOG_LMEMBER_WARN,
	DIALOG_LRANKS,
	DIALOG_LRANK_ACT,
	DIALOG_LRANK_NAME,
	DIALOG_LRANK_SALARY,
	DIALOG_LRANK_SKIN,
	DIALOG_LLOGS,
	DIALOG_FACTION_LAST
}

// Снаряжение на складе фракции
enum
{
	FITEM_ARMOUR,
	FITEM_MEDKIT,
	FITEM_DEAGLE,
	FITEM_SHOTGUN,
	FITEM_MP5,
	FITEM_M4,
	FITEM_RIFLE,
	MAX_FITEMS
}
static const gFItemName[MAX_FITEMS][24] =
{
	"Бронежилет",
	"Аптечка",
	"Desert Eagle",
	"Дробовик",
	"MP5",
	"M4",
	"Винтовка"
};
static const WEAPON:gFItemWeapon[MAX_FITEMS] =
{
	WEAPON_FIST, WEAPON_FIST, WEAPON_DEAGLE, WEAPON_SHOTGUN, WEAPON_MP5, WEAPON_M4, WEAPON_RIFLE
};
static const gFItemAmmo[MAX_FITEMS]  = { 0,   0,   35,  20,  150,  150,  15   };
static const gFItemPrice[MAX_FITEMS] = { 500, 300, 600, 800, 1200, 2000, 2500 };   // цена закупки за 1 шт. из общака

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
	OFFER_HEAL,         // медик предлагает лечение
	OFFER_HANDSHAKE     // пожать руку
}

// Значения по умолчанию. Потом их меняют лидер (/lmenu) и админ (/fedit) — они хранятся в базе.
static const gFactionDefName[MAX_FACTIONS][32] =
{
	"Нет",
	"Полиция LSPD",
	"Больница LS (EMS)",
	"Grove Street Families",
	"The Ballas",
	"Varrios Los Aztecas"
};

static const gFactionDefColor[MAX_FACTIONS] =
{
	COLOR_CIVIL,
	0x3A6EFFFF,     // полиция — синий
	0xFF8080FF,     // медики — розовый
	0x10A010FF,     // Grove — зелёный
	0xB040FFFF,     // Ballas — фиолетовый
	0x00C8F0FF      // Aztecas — голубой
};

// Скин для каждого ранга (1..6)
static const gFactionDefSkin[MAX_FACTIONS][MAX_RANK] =
{
	{ 0,   0,   0,   0,   0,   0   },
	{ 280, 280, 281, 282, 288, 265 },   // полиция
	{ 274, 274, 275, 275, 276, 70  },   // медики
	{ 105, 106, 105, 106, 107, 270 },   // Grove
	{ 102, 103, 102, 104, 103, 104 },   // Ballas
	{ 114, 115, 114, 116, 115, 116 }    // Aztecas
};

// Названия рангов: 0 — полиция, 1 — медики, 2 — банды
static const gDefRankNames[3][MAX_RANK][24] =
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
	fMk,                // маркер раздевалки (markers.inc)
	bool:fMarker,       // значок раздевалки уже создан
	fName[32],          // название (из базы)
	fColor,             // цвет (из базы)
	fBank,              // общак
	fStoreRank          // с какого ранга можно брать со склада
}
new Faction[MAX_FACTIONS][E_FACTION];

// Ранги (из базы faction_ranks). Индекс ранга: 0..MAX_RANK-1 (ранг 1 = индекс 0)
new gRankName[MAX_FACTIONS][MAX_RANK][24];
new gRankSkin[MAX_FACTIONS][MAX_RANK];
new gRankSalary[MAX_FACTIONS][MAX_RANK];

// Склад фракции
new gFStore[MAX_FACTIONS][MAX_FITEMS];

// Что игрок выбрал в окнах /lmenu
new gLMemberIds[MAX_PLAYERS][FACTION_LIST_MAX];  // номера аккаунтов в списке состава
new gLSelMember[MAX_PLAYERS];                    // выбранный аккаунт
new gLSelRank[MAX_PLAYERS];                      // выбранный ранг (1..6)
new gLSelItem[MAX_PLAYERS];                      // выбранный предмет склада
new gFactionBudgetHour = -1;                     // час последнего начисления бюджета

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
	if (f == FACTION_NONE) strcat(name, "Нет");
	else if (Faction_IsValid(f)) strcat(name, Faction[f][fName]);
	return name;
}

stock Faction_GetColor(f)
{
	if (!Faction_IsValid(f)) return COLOR_CIVIL;
	return Faction[f][fColor];
}

stock Faction_GetRankName(f, rank)
{
	new name[24];
	if (!Faction_IsValid(f) || rank < 1 || rank > MAX_RANK)
	{
		strcat(name, "-");
		return name;
	}
	strcat(name, gRankName[f][rank - 1]);
	return name;
}

// Госструктура (получает бюджет от штата)
stock bool:Faction_IsState(f)
{
	return (f == FACTION_POLICE || f == FACTION_EMS);
}

// Найти игрока онлайн по номеру аккаунта. INVALID_PLAYER_ID — не в сети.
stock Faction_FindOnline(dbid)
{
	if (dbid <= 0) return INVALID_PLAYER_ID;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pDbId] == dbid) return i;
	}
	return INVALID_PLAYER_ID;
}

// Стоит ли игрок на базе своей фракции (у раздевалки)
stock bool:Faction_AtBase(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return false;
	if (GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0) return false;
	return IsPlayerInRangeOfPoint(playerid, FACTION_BASE_RANGE, Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ]);
}

// ---------------------------------------------------------------------
//  Журнал, общак, склад, выговоры — запись в базу
// ---------------------------------------------------------------------

// Записать строку в журнал фракции (видно в /lmenu > Журнал)
stock Faction_Log(f, const text[])
{
	if (!Faction_IsValid(f)) return;
	new day, month, year, hour, minute, second, line[160];
	getdate(year, month, day);
	gettime(hour, minute, second);
	format(line, sizeof line, "%02d.%02d %02d:%02d  %s", day, month, hour, minute, text);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO faction_logs (faction, date, text) VALUES (%d, %d, '%q')", f, gettime(), line));
}

stock Faction_SaveBank(f)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE factions SET bank = %d WHERE id = %d", Faction[f][fBank], f));
}

// Изменить общак на amount (может быть отрицательным). false — не хватает денег.
stock bool:Faction_AddBank(f, amount)
{
	if (!Faction_IsValid(f)) return false;
	new nb = Faction[f][fBank] + amount;
	if (nb < 0 || nb > FACTION_BANK_MAX) return false;
	Faction[f][fBank] = nb;
	Faction_SaveBank(f);
	return true;
}

stock Faction_SaveStore(f, item)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO faction_storage (faction, item, amount) VALUES (%d, %d, %d)",
		f, item, gFStore[f][item]));
}

stock Faction_SaveRank(f, rank)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE faction_ranks SET name = '%q', skin = %d, salary = %d WHERE faction = %d AND rank = %d",
		gRankName[f][rank - 1], gRankSkin[f][rank - 1], gRankSalary[f][rank - 1], f, rank));
}

stock Faction_SaveInfo(f)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE factions SET name = '%q', color = %d, store_rank = %d WHERE id = %d",
		Faction[f][fName], Faction[f][fColor], Faction[f][fStoreRank], f));
}

// Выговоры хранятся в accounts.fwarns
stock Faction_GetWarns(dbid)
{
	new w = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT fwarns FROM accounts WHERE id = %d", dbid);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) w = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return w;
}

stock Faction_SetWarns(dbid, warns)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET fwarns = %d WHERE id = %d", warns, dbid));
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
	if (Player[playerid][pFaction] != f) Faction_SetWarns(Player[playerid][pDbId], 0);   // новая фракция — выговоры с нуля
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
	new text[MK_TEXT_LEN];
	format(text, sizeof text, "%s\n{FFFFFF}ALT — переодеться и взять снаряжение", Faction_GetName(f));
	if (Faction[f][fMarker])
	{
		Marker_SetPos(Faction[f][fMk], Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ]);
	}
	else
	{
		Faction[f][fMk] = Marker_Create(MK_FACTION, f, Faction_GetName(f), 1275,
			Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ], 0, 0, GPS_FACTIONS);
		Faction[f][fMarker] = true;
	}
	Marker_SetLabel(Faction[f][fMk], text, Faction_GetColor(f));
}

// Админ перенёс маркер раздевалки (/markers)
stock Faction_OnMarkerMoved(f, Float:x, Float:y, Float:z)
{
	if (f < 1 || f >= MAX_FACTIONS) return;
	Faction[f][fDutyX] = x;
	Faction[f][fDutyY] = y;
	Faction[f][fDutyZ] = z;
	Faction_SaveBase(f);
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
	Factions_CreateTables();

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
	// Значки раздевалок создаются в Factions_LoadData (нужны названия и цвета)

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

	Factions_LoadData();
}

// Таблицы основы фракций: название, цвет, общак, ранги, склад, журнал, выговоры.
// Старая база дополняется сама, данные игроков не трогаются.
stock Factions_CreateTables()
{
	DB_AddColumn("factions", "name", "TEXT");
	DB_AddColumn("factions", "color", "INTEGER DEFAULT 0");
	DB_AddColumn("factions", "bank", "INTEGER DEFAULT 0");
	DB_AddColumn("factions", "store_rank", "INTEGER DEFAULT 2");
	DB_AddColumn("accounts", "fwarns", "INTEGER DEFAULT 0");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS faction_ranks (faction INTEGER NOT NULL, rank INTEGER NOT NULL, name TEXT, skin INTEGER DEFAULT 0, salary INTEGER DEFAULT 0, PRIMARY KEY (faction, rank))"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS faction_storage (faction INTEGER NOT NULL, item INTEGER NOT NULL, amount INTEGER DEFAULT 0, PRIMARY KEY (faction, item))"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS faction_logs (id INTEGER PRIMARY KEY AUTOINCREMENT, faction INTEGER NOT NULL, date INTEGER DEFAULT 0, text TEXT)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS faction_logs_f ON faction_logs (faction, id)"));
}

// Загрузить название, цвет, общак, ранги и склад. Пустые значения
// заполняются значениями по умолчанию (первый запуск после обновления).
stock Factions_LoadData()
{
	for (new f = 1; f < MAX_FACTIONS; f++)
	{
		// Название и цвет по умолчанию — только если их ещё нет в базе
		new startBank = Faction_IsState(f) ? FACTION_START_BANK : 0;
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE factions SET name = '%q', color = %d, bank = %d WHERE id = %d AND name IS NULL",
			gFactionDefName[f], gFactionDefColor[f], startBank, f));

		// Ранги по умолчанию
		new t = 2;
		if (f == FACTION_POLICE) t = 0;
		else if (f == FACTION_EMS) t = 1;
		for (new rk = 1; rk <= MAX_RANK; rk++)
		{
			new salary = Faction_IsState(f) ? (FACTION_SALARY_BASE + rk * FACTION_SALARY_RANK) : 0;
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR IGNORE INTO faction_ranks (faction, rank, name, skin, salary) VALUES (%d, %d, '%q', %d, %d)",
				f, rk, gDefRankNames[t][rk - 1], gFactionDefSkin[f][rk - 1], salary));
		}

		// Запасные значения на случай пустой базы
		Faction[f][fName][0] = '\0';
		strcat(Faction[f][fName], gFactionDefName[f], 32);
		Faction[f][fColor] = gFactionDefColor[f];
		Faction[f][fBank] = 0;
		Faction[f][fStoreRank] = 2;
		for (new rk = 0; rk < MAX_RANK; rk++)
		{
			gRankName[f][rk][0] = '\0';
			strcat(gRankName[f][rk], gDefRankNames[t][rk], 24);
			gRankSkin[f][rk] = gFactionDefSkin[f][rk];
			gRankSalary[f][rk] = 0;
		}
		for (new it = 0; it < MAX_FITEMS; it++) gFStore[f][it] = 0;
	}

	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, name, color, bank, store_rank FROM factions");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new f = DB_GetFieldInt(r, 0);
			if (Faction_IsValid(f))
			{
				new name[32];
				DB_GetFieldString(r, 1, name, sizeof name);
				if (name[0] != '\0')
				{
					Faction[f][fName][0] = '\0';
					strcat(Faction[f][fName], name, 32);
				}
				Faction[f][fColor] = DB_GetFieldInt(r, 2);
				Faction[f][fBank] = DB_GetFieldInt(r, 3);
				Faction[f][fStoreRank] = DB_GetFieldInt(r, 4);
				if (Faction[f][fStoreRank] < 1 || Faction[f][fStoreRank] > MAX_RANK) Faction[f][fStoreRank] = 2;
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}

	r = DB_ExecuteQuery(gDB, "SELECT faction, rank, name, skin, salary FROM faction_ranks");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new f = DB_GetFieldInt(r, 0), rk = DB_GetFieldInt(r, 1);
			if (Faction_IsValid(f) && rk >= 1 && rk <= MAX_RANK)
			{
				new name[24];
				DB_GetFieldString(r, 2, name, sizeof name);
				if (name[0] != '\0')
				{
					gRankName[f][rk - 1][0] = '\0';
					strcat(gRankName[f][rk - 1], name, 24);
				}
				gRankSkin[f][rk - 1] = DB_GetFieldInt(r, 3);
				gRankSalary[f][rk - 1] = DB_GetFieldInt(r, 4);
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}

	r = DB_ExecuteQuery(gDB, "SELECT faction, item, amount FROM faction_storage");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new f = DB_GetFieldInt(r, 0), it = DB_GetFieldInt(r, 1);
			if (Faction_IsValid(f) && it >= 0 && it < MAX_FITEMS) gFStore[f][it] = DB_GetFieldInt(r, 2);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}

	// Значки раздевалок создавались до загрузки названий — пересоздаём с новыми
	for (new f = 1; f < MAX_FACTIONS; f++) Faction_CreateMarker(f);
	print("[Factions] Loaded names, ranks, banks and storage.");
}

// Бюджет на PayDay: госструктурам — от штата, бандам — с районов.
// Вызывается из Factions_OnMinute до зарплат (OnMinuteTimer вызывает её раньше PayDay).
stock Factions_PayDayBudget()
{
	new msg[144];
	for (new f = 1; f < MAX_FACTIONS; f++)
	{
		new income = 0;
		if (Faction_IsState(f)) income = FACTION_STATE_BUDGET;
		else if (Faction_IsGang(f)) income = Turf_CountOwned(f) * FACTION_TURF_BANK;
		if (income <= 0) continue;
		if (!Faction_AddBank(f, income)) continue;
		format(msg, sizeof msg, "PayDay: в общак поступило $%d. Общак: $%d.", income, Faction[f][fBank]);
		Faction_Log(f, msg);
	}
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
		SendClientMessage(playerid, Faction_GetColor(f), "Машина фракции «%s». Двигатель: 2, шлагбаум: H", Faction_GetName(f));
		return 1;
	}
	RemovePlayerFromVehicle(playerid);
	SendClientMessage(playerid, COLOR_GREY, "Это машина фракции «%s». Водить её могут только её члены.", Faction_GetName(f));
	return 1;
}

// Раз в минуту (из OnMinuteTimer): тюремный срок
stock Factions_OnMinute()
{
	// Начало PayDay (в :00) — сначала бюджет в общаки, потом зарплаты из них
	new hour, minute;
	gettime(hour, minute);
	if (minute == 0 && hour != gFactionBudgetHour)
	{
		gFactionBudgetHour = hour;
		Factions_PayDayBudget();
	}

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
// Зарплата фракции на PayDay. Размер задаёт лидер для каждого ранга (/lmenu > Ранги),
// деньги берутся из общака. Если в общаке не хватает — зарплаты нет.
stock Factions_GetSalary(playerid)
{
	new f = Player[playerid][pFaction];
	new rank = Player[playerid][pRank];
	if (!Faction_IsValid(f) || rank < 1 || rank > MAX_RANK) return 0;
	new salary = gRankSalary[f][rank - 1];
	if (salary <= 0) return 0;
	if (!Faction_AddBank(f, -salary))
	{
		SendClientMessage(playerid, COLOR_GREY, "В общаке фракции не хватает денег на зарплату ($%d). Сообщи лидеру.", salary);
		return 0;
	}
	return salary;
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
	SetPlayerSkin(playerid, gRankSkin[f][rank - 1]);
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
	SendClientMessage(target, COLOR_BLUE, "%s приглашает тебя во фракцию «%s». Принять: Y (или /accept)",
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
	Faction_Log(f, msg[10]);
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
	Faction_Log(f, msg[10]);
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
	Faction_Log(f, msg[10]);
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
			Faction_Log(f, msg[10]);
		}
		case OFFER_HEAL:
		{
			new price = Player[playerid][pOfferValue];
			if (Player[from][pFaction] != FACTION_EMS || !Player[from][pDuty] || !IsPlayerNearPlayer(playerid, from, 5.0))
				return SendClientMessage(playerid, COLOR_GREY, "Медик ушёл или закончил смену.");
			if (GetMoney(playerid) < price) return SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", price);

			GiveMoney(playerid, -price, "лечение у медика");
			GiveMoney(from, price, "оплата лечения");
			SetPlayerHealth(playerid, 100.0);
			Account_Save(playerid);
			Account_Save(from);
			new msg[144];
			format(msg, sizeof msg, "%s оказывает медицинскую помощь %s.", GetRPName(from), GetRPName(playerid));
			SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
			SendClientMessage(from, COLOR_GREEN, "Ты вылечил пациента. +$%d.", price);
		}
		case OFFER_HANDSHAKE:
		{
			if (!IsPlayerNearPlayer(playerid, from, 2.5)) return SendClientMessage(playerid, COLOR_GREY, "Человек отошёл.");
			ApplyAnimation(playerid, "GANGS", "hndshkfa", 4.1, false, false, false, false, 0, SYNC_ALL);
			ApplyAnimation(from, "GANGS", "hndshkfa", 4.1, false, false, false, false, 0, SYNC_ALL);
			new msg[144];
			format(msg, sizeof msg, "%s пожимает руку %s.", GetRPName(from), GetRPName(playerid));
			SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
		}
	}
	return true;
}

// /deny (или N) — отказаться от предложения
stock bool:Cmd_Deny(playerid)
{
	if (Player[playerid][pOfferType] == OFFER_NONE) return SendClientMessage(playerid, COLOR_GREY, "Тебе ничего не предлагали.");
	new from = Player[playerid][pOfferFrom];
	Player[playerid][pOfferType] = OFFER_NONE;
	if (from >= 0 && from < MAX_PLAYERS && IsPlayerConnected(from) && Player[from][pDbId] == Player[playerid][pOfferFromDb])
		SendClientMessage(from, COLOR_GREY, "%s отказался от твоего предложения.", GetRPName(playerid));
	SendClientMessage(playerid, COLOR_WHITE, "Ты отказался.");
	return true;
}

// Предложить пожать руку (из меню взаимодействия)
stock Faction_OfferHandshake(playerid, target)
{
	Player[target][pOfferType] = OFFER_HANDSHAKE;
	Player[target][pOfferFrom] = playerid;
	Player[target][pOfferFromDb] = Player[playerid][pDbId];
	Player[target][pOfferValue] = 0;
	SendClientMessage(target, COLOR_BLUE, "%s протягивает тебе руку. Y — пожать, N — отказаться.", GetRPName(playerid));
	SendClientMessage(playerid, COLOR_WHITE, "Ты протянул руку %s.", GetRPName(target));
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

// Обыск: оружие и деньги задержанного
stock bool:Cmd_Frisk(playerid, const params[])
{
	if (!Faction_Need(playerid, FACTION_POLICE, true)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /frisk [id]");
	if (!IsPlayerNearPlayer(playerid, target, 2.5)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе.");

	new msg[144];
	format(msg, sizeof msg, "%s обыскивает %s.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(playerid, COLOR_WHITE, "Обыск %s: наличные $%d.", GetRPName(target), GetMoney(target));

	new WEAPON:weapon, ammo, name[32], found = 0;
	for (new s = 0; s < 13; s++)
	{
		GetPlayerWeaponData(target, WEAPON_SLOT:s, weapon, ammo);
		if (weapon == WEAPON:0 || ammo <= 0) continue;
		GetWeaponName(weapon, name, sizeof name);
		SendClientMessage(playerid, COLOR_WHITE, " Оружие: %s (%d патр.)", name, ammo);
		found++;
	}
	if (found == 0) SendClientMessage(playerid, COLOR_WHITE, " Оружия нет.");
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

	GiveMoney(playerid, stars * ARREST_REWARD, "премия за арест");
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
	SendClientMessage(target, COLOR_BLUE, "Медик %s предлагает вылечить тебя за $%d. Согласиться: Y (или /accept)", GetRPName(playerid), HEAL_PRICE);
	SendClientMessage(playerid, COLOR_WHITE, "Ты предложил лечение %s.", GetRPName(target));
	return true;
}

// =====================================================================
//  Общак фракции (/fbank)
// =====================================================================
stock bool:Cmd_FBank(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь во фракции.");
	if (!Faction_AtBase(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Общак доступен только на базе фракции (у раздевалки).");
	new title[64];
	format(title, sizeof title, "Общак фракции — $%d", Faction[f][fBank]);
	ShowPlayerDialog(playerid, DIALOG_FBANK, DIALOG_STYLE_LIST, title,
		"Положить деньги в общак\nСнять деньги из общака {AFAFAF}(лидер)", "Выбрать", "Закрыть");
	return true;
}

// =====================================================================
//  Склад фракции (/fstorage)
// =====================================================================
stock bool:Cmd_FStorage(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь во фракции.");
	if (!Faction_AtBase(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Склад доступен только на базе фракции (у раздевалки).");

	new body[900], line[96];
	strcat(body, "Снаряжение\tНа складе\tЦена закупки\n");
	for (new it = 0; it < MAX_FITEMS; it++)
	{
		format(line, sizeof line, "%s\t%d шт.\t$%d\n", gFItemName[it], gFStore[f][it], gFItemPrice[it]);
		strcat(body, line);
	}
	strcat(body, "{FFFF00}» Закупить снаряжение из общака\t{AFAFAF}лидер и зам\t");
	new title[64];
	format(title, sizeof title, "Склад фракции (брать можно с ранга %d)", Faction[f][fStoreRank]);
	ShowPlayerDialog(playerid, DIALOG_FSTORE, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Взять", "Закрыть", body);
	return true;
}

stock Faction_TakeItem(playerid, it)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f) || it < 0 || it >= MAX_FITEMS) return 0;
	if (!Faction_AtBase(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от склада.");
	if (Player[playerid][pRank] < Faction[f][fStoreRank])
		return SendClientMessage(playerid, COLOR_GREY, "Брать со склада можно с ранга %d.", Faction[f][fStoreRank]);
	if (Faction_IsState(f) && !Player[playerid][pDuty])
		return SendClientMessage(playerid, COLOR_GREY, "Сначала заступи на смену: /duty.");
	if (gFStore[f][it] <= 0) return SendClientMessage(playerid, COLOR_GREY, "На складе нет: %s. Лидер может закупить в /fstorage.", gFItemName[it]);

	switch (it)
	{
		case FITEM_ARMOUR: SetPlayerArmour(playerid, 100.0);
		case FITEM_MEDKIT: SetPlayerHealth(playerid, 100.0);
		default: GivePlayerWeapon(playerid, gFItemWeapon[it], gFItemAmmo[it]);
	}
	gFStore[f][it]--;
	Faction_SaveStore(f, it);

	new msg[144];
	format(msg, sizeof msg, "%s берёт со склада: %s.", GetRPName(playerid), gFItemName[it]);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	format(msg, sizeof msg, "%s взял со склада: %s (осталось %d).", GetRPName(playerid), gFItemName[it], gFStore[f][it]);
	Faction_Log(f, msg);
	return 1;
}

stock Faction_ShowBuyList(playerid)
{
	new f = Player[playerid][pFaction];
	new body[700], line[96];
	strcat(body, "Снаряжение\tЦена за 1 шт.\tНа складе\n");
	for (new it = 0; it < MAX_FITEMS; it++)
	{
		format(line, sizeof line, "%s\t$%d\t%d шт.\n", gFItemName[it], gFItemPrice[it], gFStore[f][it]);
		strcat(body, line);
	}
	new title[64];
	format(title, sizeof title, "Закупка (общак: $%d)", Faction[f][fBank]);
	ShowPlayerDialog(playerid, DIALOG_FSTORE_BUY, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Выбрать", "Назад", body);
}

// =====================================================================
//  Панель лидера (/lmenu)
// =====================================================================
stock bool:Faction_IsLeader(playerid)
{
	return (Faction_IsValid(Player[playerid][pFaction]) && Player[playerid][pRank] >= MAX_RANK);
}

stock bool:Cmd_LMenu(playerid)
{
	if (!Faction_NeedLeader(playerid)) return true;
	new f = Player[playerid][pFaction];
	new title[64];
	format(title, sizeof title, "Панель лидера — %s", Faction_GetName(f));
	ShowPlayerDialog(playerid, DIALOG_LMENU, DIALOG_STYLE_LIST, title,
		"Состав фракции (и оффлайн)\nРанги, зарплаты и скины {AFAFAF}(лидер)\nОбщак: {33AA33}$%d {AFAFAF}(на базе)\nСклад {AFAFAF}(на базе)\nДоступ к складу: с ранга %d {AFAFAF}(лидер)\nЖурнал фракции",
		"Выбрать", "Закрыть", Faction[f][fBank], Faction[f][fStoreRank]);
	return true;
}

// Список членов фракции из базы (онлайн и оффлайн)
stock Faction_ShowMembers(playerid)
{
	new f = Player[playerid][pFaction];
	new body[3200], line[96], name[MAX_PLAYER_NAME];
	strcat(body, "Имя\tРанг\tВыговоры\tСтатус\n");
	for (new i = 0; i < FACTION_LIST_MAX; i++) gLMemberIds[playerid][i] = 0;

	new count = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, name, frank, fwarns FROM accounts WHERE faction = %d ORDER BY frank DESC, name LIMIT %d", f, FACTION_LIST_MAX);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new dbid = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 1, name, sizeof name);
			new rank = DB_GetFieldInt(r, 2);
			new online = Faction_FindOnline(dbid);
			if (online != INVALID_PLAYER_ID) rank = Player[online][pRank];
			if (online != INVALID_PLAYER_ID)
				format(line, sizeof line, "%s\t%d. %s\t%d/%d\t{33AA33}онлайн [%d]\n", name, rank, Faction_GetRankName(f, rank), DB_GetFieldInt(r, 3), FACTION_MAX_WARNS, online);
			else
				format(line, sizeof line, "%s\t%d. %s\t%d/%d\t{AFAFAF}оффлайн\n", name, rank, Faction_GetRankName(f, rank), DB_GetFieldInt(r, 3), FACTION_MAX_WARNS);
			strcat(body, line);
			gLMemberIds[playerid][count++] = dbid;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	if (count == 0) return SendClientMessage(playerid, COLOR_GREY, "Во фракции никого нет.");
	new title[64];
	format(title, sizeof title, "Состав фракции (%d)", count);
	ShowPlayerDialog(playerid, DIALOG_LMEMBERS, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Выбрать", "Назад", body);
	return 1;
}

// Прочитать члена фракции: имя, ранг, фракцию (онлайн — из памяти, оффлайн — из базы)
stock bool:Faction_GetMember(dbid, name[], &rank, &faction, size = sizeof (name))
{
	name[0] = '\0';
	rank = 0;
	faction = FACTION_NONE;
	new online = Faction_FindOnline(dbid);
	if (online != INVALID_PLAYER_ID)
	{
		GetPlayerName(online, name, size);
		rank = Player[online][pRank];
		faction = Player[online][pFaction];
		return true;
	}
	new bool:found = false;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT name, frank, faction FROM accounts WHERE id = %d", dbid);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			DB_GetFieldString(r, 0, name, size);
			rank = DB_GetFieldInt(r, 1);
			faction = DB_GetFieldInt(r, 2);
			found = true;
		}
		DB_FreeResultSet(r);
	}
	return found;
}

// Можно ли лидеру/заму управлять этим членом фракции
stock bool:Faction_CanManage(playerid, dbid, name[], &rank, size = sizeof (name))
{
	new faction;
	if (!Faction_NeedLeader(playerid)) return false;
	if (!Faction_GetMember(dbid, name, rank, faction, size) || faction != Player[playerid][pFaction])
	{
		SendClientMessage(playerid, COLOR_GREY, "Этот человек уже не в твоей фракции.");
		return false;
	}
	if (dbid == Player[playerid][pDbId])
	{
		SendClientMessage(playerid, COLOR_GREY, "Себя менять нельзя.");
		return false;
	}
	if (rank >= Player[playerid][pRank])
	{
		SendClientMessage(playerid, COLOR_GREY, "Нельзя управлять равным или старшим по рангу.");
		return false;
	}
	return true;
}

stock Faction_ShowMemberActions(playerid)
{
	new name[MAX_PLAYER_NAME], rank;
	if (!Faction_CanManage(playerid, gLSelMember[playerid], name, rank)) return;
	new title[96];
	format(title, sizeof title, "%s — %s (выговоры: %d/%d)",
		name, Faction_GetRankName(Player[playerid][pFaction], rank), Faction_GetWarns(gLSelMember[playerid]), FACTION_MAX_WARNS);
	ShowPlayerDialog(playerid, DIALOG_LMEMBER_ACT, DIALOG_STYLE_LIST, title,
		"Изменить ранг\nВыговор\nСнять выговор\n{FF6347}Уволить", "Выбрать", "Назад");
}

// Уволить (онлайн или оффлайн)
stock Faction_Fire(playerid, dbid, const name[], const reason[])
{
	new f = Player[playerid][pFaction];
	new target = Faction_FindOnline(dbid);
	if (target != INVALID_PLAYER_ID)
	{
		Faction_SetMember(target, FACTION_NONE, 0);
		SendClientMessage(target, COLOR_RED, "%s уволил тебя из фракции «%s». Причина: %s", GetRPName(playerid), Faction_GetName(f), reason);
	}
	else
	{
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET faction = 0, frank = 0, fwarns = 0 WHERE id = %d", dbid));
	}
	new msg[144];
	format(msg, sizeof msg, "%s уволил %s. Причина: %s", GetRPName(playerid), name, reason);
	Faction_Log(f, msg);
	format(msg, sizeof msg, "[Фракция] %s уволил %s. Причина: %s", GetRPName(playerid), name, reason);
	Faction_Message(f, Faction_GetColor(f), msg);
}

stock Faction_SetMemberRank(playerid, dbid, const name[], rank)
{
	new f = Player[playerid][pFaction];
	new target = Faction_FindOnline(dbid);
	if (target != INVALID_PLAYER_ID)
	{
		Player[target][pRank] = rank;
		Account_Save(target);
	}
	else
	{
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET frank = %d WHERE id = %d", rank, dbid));
	}
	new msg[144];
	format(msg, sizeof msg, "%s назначил %s на ранг «%s» (%d).", GetRPName(playerid), name, Faction_GetRankName(f, rank), rank);
	Faction_Log(f, msg);
	format(msg, sizeof msg, "[Фракция] %s назначил %s на ранг «%s» (%d).", GetRPName(playerid), name, Faction_GetRankName(f, rank), rank);
	Faction_Message(f, Faction_GetColor(f), msg);
}

stock Faction_ShowRanks(playerid)
{
	new f = Player[playerid][pFaction];
	new body[800], line[96];
	strcat(body, "Ранг\tНазвание\tЗарплата\tСкин\n");
	for (new rk = 1; rk <= MAX_RANK; rk++)
	{
		format(line, sizeof line, "%d\t%s\t$%d\t%d\n", rk, gRankName[f][rk - 1], gRankSalary[f][rk - 1], gRankSkin[f][rk - 1]);
		strcat(body, line);
	}
	ShowPlayerDialog(playerid, DIALOG_LRANKS, DIALOG_STYLE_TABLIST_HEADERS, "Ранги (зарплата — из общака на PayDay)", "%s", "Изменить", "Назад", body);
}

stock Faction_ShowLogs(playerid)
{
	new f = Player[playerid][pFaction];
	new body[3600], line[160];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT text FROM faction_logs WHERE faction = %d ORDER BY id DESC LIMIT %d", f, FACTION_LOG_SHOW);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			DB_GetFieldString(r, 0, line, sizeof line);
			strcat(body, line);
			strcat(body, "\n");
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	if (body[0] == '\0') strcat(body, "Журнал пока пуст.");
	ShowPlayerDialog(playerid, DIALOG_LLOGS, DIALOG_STYLE_MSGBOX, "Журнал фракции (последние записи)", "%s", "Назад", "", body);
}

// Проверка длины и символов в названии (без цветовых кодов и знака %)
stock bool:Faction_IsGoodName(const text[], minLen, maxLen)
{
	new len = strlen(text);
	if (len < minLen || len > maxLen) return false;
	for (new i = 0; i < len; i++)
	{
		if (text[i] == '{' || text[i] == '}' || text[i] == '%' || text[i] == '\t' || text[i] == '\n') return false;
	}
	return true;
}

// =====================================================================
//  Окна фракций. Вызывается из OnDialogResponse (rp.pwn).
//  Возвращает 1, если окно наше.
// =====================================================================
stock Factions_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid < DIALOG_LMENU || dialogid >= DIALOG_FACTION_LAST) return 0;
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return 1;
	new amount, msg[144];

	switch (dialogid)
	{
		case DIALOG_FBANK:
		{
			if (!response) return 1;
			if (listitem == 0)
			{
				ShowPlayerDialog(playerid, DIALOG_FBANK_DEPOSIT, DIALOG_STYLE_INPUT, "Положить в общак",
					"Сколько положить в общак фракции?\nУ тебя: $%d", "Положить", "Назад", GetMoney(playerid));
			}
			else
			{
				if (!Faction_IsLeader(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Снимать деньги из общака может только лидер."); return 1; }
				ShowPlayerDialog(playerid, DIALOG_FBANK_WITHDRAW, DIALOG_STYLE_INPUT, "Снять из общака",
					"Сколько снять из общака?\nВ общаке: $%d", "Снять", "Назад", Faction[f][fBank]);
			}
		}
		case DIALOG_FBANK_DEPOSIT:
		{
			if (!response) { Cmd_FBank(playerid); return 1; }
			if (!Faction_AtBase(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от базы."); return 1; }
			if (!IsNumeric(inputtext) || (amount = strval(inputtext)) < 1 || amount > 1000000)
			{
				SendClientMessage(playerid, COLOR_GREY, "Сумма от $1 до $1000000.");
				return 1;
			}
			if (GetMoney(playerid) < amount) { SendClientMessage(playerid, COLOR_GREY, "У тебя нет столько денег."); return 1; }
			if (!Faction_AddBank(f, amount)) { SendClientMessage(playerid, COLOR_GREY, "Общак переполнен."); return 1; }
			GiveMoney(playerid, -amount);
			Account_Save(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Ты положил $%d в общак. В общаке: $%d.", amount, Faction[f][fBank]);
			format(msg, sizeof msg, "%s положил в общак $%d (стало $%d).", GetRPName(playerid), amount, Faction[f][fBank]);
			Faction_Log(f, msg);
		}
		case DIALOG_FBANK_WITHDRAW:
		{
			if (!response) { Cmd_FBank(playerid); return 1; }
			if (!Faction_IsLeader(playerid)) return 1;
			if (!Faction_AtBase(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от базы."); return 1; }
			if (!IsNumeric(inputtext) || (amount = strval(inputtext)) < 1 || amount > 1000000)
			{
				SendClientMessage(playerid, COLOR_GREY, "Сумма от $1 до $1000000.");
				return 1;
			}
			if (!Faction_AddBank(f, -amount)) { SendClientMessage(playerid, COLOR_GREY, "В общаке нет столько денег."); return 1; }
			GiveMoney(playerid, amount);
			Account_Save(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Ты снял $%d из общака. В общаке: $%d.", amount, Faction[f][fBank]);
			format(msg, sizeof msg, "%s снял из общака $%d (осталось $%d).", GetRPName(playerid), amount, Faction[f][fBank]);
			Faction_Log(f, msg);
			format(msg, sizeof msg, "[Фракция] Лидер %s снял из общака $%d.", GetRPName(playerid), amount);
			Faction_Message(f, Faction_GetColor(f), msg);
		}
		case DIALOG_FSTORE:
		{
			if (!response) return 1;
			if (listitem >= 0 && listitem < MAX_FITEMS)
			{
				Faction_TakeItem(playerid, listitem);
				return 1;
			}
			if (!Faction_NeedLeader(playerid)) return 1;
			Faction_ShowBuyList(playerid);
		}
		case DIALOG_FSTORE_BUY:
		{
			if (!response) { Cmd_FStorage(playerid); return 1; }
			if (listitem < 0 || listitem >= MAX_FITEMS) return 1;
			gLSelItem[playerid] = listitem;
			ShowPlayerDialog(playerid, DIALOG_FSTORE_BUY_QTY, DIALOG_STYLE_INPUT, "Закупка",
				"%s — $%d за 1 шт.\nСколько закупить? (1-100)\nВ общаке: $%d", "Купить", "Назад",
				gFItemName[listitem], gFItemPrice[listitem], Faction[f][fBank]);
		}
		case DIALOG_FSTORE_BUY_QTY:
		{
			if (!response) { Faction_ShowBuyList(playerid); return 1; }
			if (!Faction_NeedLeader(playerid)) return 1;
			if (!Faction_AtBase(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от базы."); return 1; }
			new it = gLSelItem[playerid];
			if (it < 0 || it >= MAX_FITEMS) return 1;
			if (!IsNumeric(inputtext) || (amount = strval(inputtext)) < 1 || amount > 100)
			{
				SendClientMessage(playerid, COLOR_GREY, "Количество от 1 до 100.");
				return 1;
			}
			new cost = amount * gFItemPrice[it];
			if (!Faction_AddBank(f, -cost)) { SendClientMessage(playerid, COLOR_GREY, "В общаке не хватает денег: нужно $%d.", cost); return 1; }
			gFStore[f][it] += amount;
			Faction_SaveStore(f, it);
			SendClientMessage(playerid, COLOR_GREEN, "Закуплено: %s x%d за $%d. На складе: %d.", gFItemName[it], amount, cost, gFStore[f][it]);
			format(msg, sizeof msg, "%s закупил на склад: %s x%d за $%d.", GetRPName(playerid), gFItemName[it], amount, cost);
			Faction_Log(f, msg);
			Cmd_FStorage(playerid);
		}
		case DIALOG_FSTORE_RANK:
		{
			if (!response) { Cmd_LMenu(playerid); return 1; }
			if (!Faction_IsLeader(playerid)) return 1;
			if (!IsNumeric(inputtext) || (amount = strval(inputtext)) < 1 || amount > MAX_RANK)
			{
				SendClientMessage(playerid, COLOR_GREY, "Ранг от 1 до %d.", MAX_RANK);
				return 1;
			}
			Faction[f][fStoreRank] = amount;
			Faction_SaveInfo(f);
			SendClientMessage(playerid, COLOR_GREEN, "Теперь брать со склада можно с ранга %d.", amount);
			format(msg, sizeof msg, "%s разрешил брать со склада с ранга %d.", GetRPName(playerid), amount);
			Faction_Log(f, msg);
			Cmd_LMenu(playerid);
		}
		case DIALOG_LMENU:
		{
			if (!response) return 1;
			if (!Faction_NeedLeader(playerid)) return 1;
			switch (listitem)
			{
				case 0: Faction_ShowMembers(playerid);
				case 1:
				{
					if (!Faction_IsLeader(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Ранги и зарплаты настраивает только лидер."); return 1; }
					Faction_ShowRanks(playerid);
				}
				case 2: Cmd_FBank(playerid);
				case 3: Cmd_FStorage(playerid);
				case 4:
				{
					if (!Faction_IsLeader(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Это может только лидер."); return 1; }
					ShowPlayerDialog(playerid, DIALOG_FSTORE_RANK, DIALOG_STYLE_INPUT, "Доступ к складу",
						"С какого ранга можно брать снаряжение со склада? (1-%d)\nСейчас: с ранга %d", "Сохранить", "Назад",
						MAX_RANK, Faction[f][fStoreRank]);
				}
				case 5: Faction_ShowLogs(playerid);
			}
		}
		case DIALOG_LLOGS:
		{
			Cmd_LMenu(playerid);
		}
		case DIALOG_LMEMBERS:
		{
			if (!response) { Cmd_LMenu(playerid); return 1; }
			if (listitem < 0 || listitem >= FACTION_LIST_MAX || gLMemberIds[playerid][listitem] == 0) return 1;
			gLSelMember[playerid] = gLMemberIds[playerid][listitem];
			Faction_ShowMemberActions(playerid);
		}
		case DIALOG_LMEMBER_ACT:
		{
			if (!response) { Faction_ShowMembers(playerid); return 1; }
			new name[MAX_PLAYER_NAME], rank;
			if (!Faction_CanManage(playerid, gLSelMember[playerid], name, rank)) return 1;
			switch (listitem)
			{
				case 0:
				{
					ShowPlayerDialog(playerid, DIALOG_LMEMBER_RANK, DIALOG_STYLE_INPUT, "Ранг",
						"Новый ранг для %s (1-%d)\nСейчас: %d. %s", "Сохранить", "Назад",
						name, Player[playerid][pRank] - 1, rank, Faction_GetRankName(f, rank));
				}
				case 1:
				{
					ShowPlayerDialog(playerid, DIALOG_LMEMBER_WARN, DIALOG_STYLE_INPUT, "Выговор",
						"Выговор для %s. Напиши причину.\nНа %d-м выговоре игрок будет уволен.", "Выдать", "Назад",
						name, FACTION_MAX_WARNS);
				}
				case 2:
				{
					new warns = Faction_GetWarns(gLSelMember[playerid]);
					if (warns <= 0) { SendClientMessage(playerid, COLOR_GREY, "У него нет выговоров."); Faction_ShowMemberActions(playerid); return 1; }
					Faction_SetWarns(gLSelMember[playerid], warns - 1);
					format(msg, sizeof msg, "%s снял выговор с %s (%d/%d).", GetRPName(playerid), name, warns - 1, FACTION_MAX_WARNS);
					Faction_Log(f, msg);
					SendClientMessage(playerid, COLOR_GREEN, "Выговор снят. Осталось: %d/%d.", warns - 1, FACTION_MAX_WARNS);
					new target = Faction_FindOnline(gLSelMember[playerid]);
					if (target != INVALID_PLAYER_ID) SendClientMessage(target, COLOR_GREEN, "%s снял с тебя выговор (%d/%d).", GetRPName(playerid), warns - 1, FACTION_MAX_WARNS);
					Faction_ShowMemberActions(playerid);
				}
				case 3:
				{
					Faction_Fire(playerid, gLSelMember[playerid], name, "решение руководства");
				}
			}
		}
		case DIALOG_LMEMBER_RANK:
		{
			if (!response) { Faction_ShowMemberActions(playerid); return 1; }
			new name[MAX_PLAYER_NAME], rank;
			if (!Faction_CanManage(playerid, gLSelMember[playerid], name, rank)) return 1;
			if (!IsNumeric(inputtext) || (amount = strval(inputtext)) < 1 || amount >= Player[playerid][pRank])
			{
				SendClientMessage(playerid, COLOR_GREY, "Ранг должен быть от 1 до %d.", Player[playerid][pRank] - 1);
				return 1;
			}
			Faction_SetMemberRank(playerid, gLSelMember[playerid], name, amount);
			Faction_ShowMemberActions(playerid);
		}
		case DIALOG_LMEMBER_WARN:
		{
			if (!response) { Faction_ShowMemberActions(playerid); return 1; }
			new name[MAX_PLAYER_NAME], rank, reason[64];
			if (!Faction_CanManage(playerid, gLSelMember[playerid], name, rank)) return 1;
			if (!RestParam(inputtext, 0, reason) || !Faction_IsGoodName(reason, 2, 60))
			{
				SendClientMessage(playerid, COLOR_GREY, "Напиши причину: от 2 до 60 символов, без фигурных скобок и знака процента.");
				return 1;
			}
			new warns = Faction_GetWarns(gLSelMember[playerid]) + 1;
			new target = Faction_FindOnline(gLSelMember[playerid]);
			if (warns >= FACTION_MAX_WARNS)
			{
				new why[96];
				format(why, sizeof why, "%d/%d выговоров (последний: %s)", warns, FACTION_MAX_WARNS, reason);
				Faction_Fire(playerid, gLSelMember[playerid], name, why);
				return 1;
			}
			Faction_SetWarns(gLSelMember[playerid], warns);
			format(msg, sizeof msg, "%s выдал выговор %s (%d/%d). Причина: %s", GetRPName(playerid), name, warns, FACTION_MAX_WARNS, reason);
			Faction_Log(f, msg);
			SendClientMessage(playerid, COLOR_WHITE, "Выговор выдан (%d/%d).", warns, FACTION_MAX_WARNS);
			if (target != INVALID_PLAYER_ID)
				SendClientMessage(target, COLOR_RED, "%s выдал тебе выговор (%d/%d). Причина: %s", GetRPName(playerid), warns, FACTION_MAX_WARNS, reason);
			Faction_ShowMemberActions(playerid);
		}
		case DIALOG_LRANKS:
		{
			if (!response) { Cmd_LMenu(playerid); return 1; }
			if (!Faction_IsLeader(playerid) || listitem < 0 || listitem >= MAX_RANK) return 1;
			gLSelRank[playerid] = listitem + 1;
			new title[64];
			format(title, sizeof title, "Ранг %d — %s", listitem + 1, gRankName[f][listitem]);
			ShowPlayerDialog(playerid, DIALOG_LRANK_ACT, DIALOG_STYLE_LIST, title,
				"Название\nЗарплата: $%d\nСкин формы: %d", "Выбрать", "Назад",
				gRankSalary[f][listitem], gRankSkin[f][listitem]);
		}
		case DIALOG_LRANK_ACT:
		{
			if (!response) { Faction_ShowRanks(playerid); return 1; }
			if (!Faction_IsLeader(playerid)) return 1;
			new rk = gLSelRank[playerid];
			if (rk < 1 || rk > MAX_RANK) return 1;
			switch (listitem)
			{
				case 0: ShowPlayerDialog(playerid, DIALOG_LRANK_NAME, DIALOG_STYLE_INPUT, "Название ранга",
					"Новое название ранга %d (2-23 символа)\nСейчас: %s", "Сохранить", "Назад", rk, gRankName[f][rk - 1]);
				case 1: ShowPlayerDialog(playerid, DIALOG_LRANK_SALARY, DIALOG_STYLE_INPUT, "Зарплата ранга",
					"Зарплата ранга %d на PayDay (0-%d). Платится из общака.\nСейчас: $%d", "Сохранить", "Назад", rk, FACTION_MAX_SALARY, gRankSalary[f][rk - 1]);
				case 2: ShowPlayerDialog(playerid, DIALOG_LRANK_SKIN, DIALOG_STYLE_INPUT, "Скин ранга",
					"Скин формы для ранга %d (0-311)\nСейчас: %d", "Сохранить", "Назад", rk, gRankSkin[f][rk - 1]);
			}
		}
		case DIALOG_LRANK_NAME, DIALOG_LRANK_SALARY, DIALOG_LRANK_SKIN:
		{
			if (!response) { Faction_ShowRanks(playerid); return 1; }
			if (!Faction_IsLeader(playerid)) return 1;
			new rk = gLSelRank[playerid];
			if (rk < 1 || rk > MAX_RANK) return 1;
			if (dialogid == DIALOG_LRANK_NAME)
			{
				new text[24];
				if (!RestParam(inputtext, 0, text) || !Faction_IsGoodName(text, 2, 23))
				{
					SendClientMessage(playerid, COLOR_GREY, "Название: от 2 до 23 символов, без фигурных скобок и знака процента.");
					return 1;
				}
				gRankName[f][rk - 1][0] = '\0';
				strcat(gRankName[f][rk - 1], text, 24);
				format(msg, sizeof msg, "%s переименовал ранг %d в «%s».", GetRPName(playerid), rk, text);
			}
			else if (dialogid == DIALOG_LRANK_SALARY)
			{
				if (!IsNumeric(inputtext) || (amount = strval(inputtext)) < 0 || amount > FACTION_MAX_SALARY)
				{
					SendClientMessage(playerid, COLOR_GREY, "Зарплата от $0 до $%d.", FACTION_MAX_SALARY);
					return 1;
				}
				gRankSalary[f][rk - 1] = amount;
				format(msg, sizeof msg, "%s поставил зарплату рангу %d: $%d.", GetRPName(playerid), rk, amount);
			}
			else
			{
				if (!IsNumeric(inputtext) || (amount = strval(inputtext)) < 0 || amount > 311 || amount == 74)
				{
					SendClientMessage(playerid, COLOR_GREY, "Скин от 0 до 311 (кроме 74).");
					return 1;
				}
				gRankSkin[f][rk - 1] = amount;
				format(msg, sizeof msg, "%s поставил скин %d рангу %d.", GetRPName(playerid), amount, rk);
			}
			Faction_SaveRank(f, rk);
			Faction_Log(f, msg);
			SendClientMessage(playerid, COLOR_GREEN, "Сохранено.");
			Faction_ShowRanks(playerid);
		}
	}
	return 1;
}

// =====================================================================
//  Админ: /fedit [фракция] name [название] | color [RRGGBB] | bank [сумма]
// =====================================================================
stock bool:Cmd_FEdit(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f, what[16], value[32];
	if (!NextIntParam(params, pos, f) || !Faction_IsValid(f) || !NextParam(params, pos, what) || !RestParam(params, pos, value))
	{
		SendClientMessage(playerid, COLOR_GREY, "Использование: /fedit [фракция 1-%d] name [название] | color [RRGGBB] | bank [сумма]", MAX_FACTIONS - 1);
		return true;
	}

	new msg[144];
	if (!strcmp(what, "name", true))
	{
		if (!Faction_IsGoodName(value, 2, 31)) return SendClientMessage(playerid, COLOR_GREY, "Название: от 2 до 31 символа, без фигурных скобок и знака процента.");
		Faction[f][fName][0] = '\0';
		strcat(Faction[f][fName], value, 32);
		format(msg, sizeof msg, "Администратор %s переименовал фракцию в «%s».", GetRPName(playerid), value);
	}
	else if (!strcmp(what, "color", true))
	{
		new rgb = 0, len = strlen(value);
		if (len != 6) return SendClientMessage(playerid, COLOR_GREY, "Цвет — 6 знаков RRGGBB, например 3A6EFF.");
		for (new i = 0; i < 6; i++)
		{
			new c = value[i], d;
			if (c >= '0' && c <= '9') d = c - '0';
			else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
			else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
			else return SendClientMessage(playerid, COLOR_GREY, "Цвет — 6 знаков RRGGBB, например 3A6EFF.");
			rgb = (rgb << 4) | d;
		}
		Faction[f][fColor] = (rgb << 8) | 0xFF;
		format(msg, sizeof msg, "Администратор %s сменил цвет фракции на %s.", GetRPName(playerid), value);
	}
	else if (!strcmp(what, "bank", true))
	{
		if (!IsNumeric(value)) return SendClientMessage(playerid, COLOR_GREY, "Сумма — число.");
		new amount = strval(value);
		if (amount < 0 || amount > FACTION_BANK_MAX) return SendClientMessage(playerid, COLOR_GREY, "Сумма от 0 до %d.", FACTION_BANK_MAX);
		Faction[f][fBank] = amount;
		Faction_SaveBank(f);
		format(msg, sizeof msg, "Администратор %s установил общак: $%d.", GetRPName(playerid), amount);
	}
	else return SendClientMessage(playerid, COLOR_GREY, "Можно менять: name, color, bank.");

	Faction_SaveInfo(f);
	Faction_CreateMarker(f);
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pFaction] == f) Faction_UpdateColor(i);
	}
	Faction_Log(f, msg);
	SendClientMessage(playerid, COLOR_ADMIN, "Фракция %d: %s", f, msg);
	return true;
}

// =====================================================================
//  Команды основы фракций. Вызывается из Commands_Process (commands.inc).
//  Возвращает 1, если команда наша.
// =====================================================================
stock Factions_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/lmenu", true))       { Cmd_LMenu(playerid); return 1; }
	if (!strcmp(cmd, "/fbank", true))       { Cmd_FBank(playerid); return 1; }
	if (!strcmp(cmd, "/fstorage", true) || !strcmp(cmd, "/fsklad", true)) { Cmd_FStorage(playerid); return 1; }
	if (!strcmp(cmd, "/fedit", true))       { Cmd_FEdit(playerid, params); return 1; }
	return 0;
}

// =====================================================================
//  Справка
// =====================================================================
stock bool:Cmd_FHelp(playerid)
{
	new text[2000];
	strcat(text, "{FFFF00}Все фракции\n");
	strcat(text, "{FFFFFF}/f — рация   /members — кто онлайн   /duty — смена (в раздевалке)   /fleave — уйти\n");
	strcat(text, "/accept — принять приглашение или лечение\n");
	strcat(text, "/fbank — общак (на базе)   /fstorage — склад: оружие, бронежилет, аптечка (на базе)\n\n");
	strcat(text, "{FFFF00}Лидер и заместитель (ранг 5-6)\n");
	strcat(text, "{FFFFFF}/lmenu — панель лидера: состав (и оффлайн), выговоры, ранги, зарплаты, склад, журнал\n");
	strcat(text, "/invite [id]   /uninvite [id]   /giverank [id] [ранг]\n\n");
	strcat(text, "{3A6EFF}Полиция LSPD\n");
	strcat(text, "{FFFFFF}/su [id] [звёзды] [причина] — розыск   /clear [id] — снять розыск   /wanted — список\n");
	strcat(text, "/cuff /uncuff [id] — наручники   /putcar [id] — посадить в машину\n");
	strcat(text, "/arrest [id] — посадить в тюрьму (у участка)   /m — мегафон\n");
	strcat(text, "/impound [штраф] — эвакуировать личную машину на штрафстоянку (стоя рядом)\n\n");
	strcat(text, "{FF8080}Больница (EMS)\n");
	strcat(text, "{FFFFFF}/heal [id] — предложить лечение\n\n");
	strcat(text, "{10A010}Банды\n");
	strcat(text, "{FFFFFF}/duty на базе — форма банды и оружие. /turfs /capture — войны за районы, районы пополняют общак.\n\n");
	strcat(text, "{FF66FF}Админам (4+)\n");
	strcat(text, "{FFFFFF}/makeleader /setfaction /setfspawn /setfduty /addfcar /delfcar /fedit   (2+) /jail /unjail");
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
	new msg[144];
	format(msg, sizeof msg, "Администратор %s назначил лидером %s.", GetRPName(playerid), GetRPName(target));
	Faction_Log(f, msg);
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
	if (!Admin_Check(playerid, 2)) return true;
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
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /unjail [id]");
	if (Player[target][pJail] <= 0) return SendClientMessage(playerid, COLOR_GREY, "Игрок не в тюрьме.");
	Jail_Release(target);
	SendClientMessage(playerid, COLOR_ADMIN, "%s выпущен из тюрьмы.", GetRPName(target));
	return true;
}
