// =====================================================================
//  families.inc — семьи: организации, которые создают сами игроки.
//
//  Семья — это не фракция: её не выдаёт админ, у неё нет базы, формы и
//  зарплат. Игрок сам создаёт её за FAMILY_PRICE, сам называет тип
//  («Семья», «Банда», «Клан», «Картель» — любое слово) и приглашает людей.
//  Состоять можно и во фракции, и в семье одновременно. Лимита на число
//  людей нет.
//
//  В семье FAMILY_RANKS рангов: 6 — глава, 5 — заместитель.
//  Глава и заместитель принимают и выгоняют, глава меняет названия
//  рангов, цвет и снимает деньги из общака.
//
//  Семейный дом: глава встаёт у своего дома и вводит /famhouse. Все в семье входят
//  в него (даже закрытый), закрывают /lock и могут появляться у него (/setspawn).
//  /famhouse off — убрать. Хранится в families.house (номер дома в базе).
//  Команды: /famhelp
//  Данные: таблицы families и family_ranks, поля accounts.family и famrank.
// =====================================================================

#define MAX_FAMILIES        200
#define FAMILY_RANKS        6           // 6 — глава, 5 — заместитель
#define FAMILY_PRICE        50000       // цена создания (уходит в казну)
#define FAMILY_MIN_LEVEL    3           // с какого уровня игрока можно создать семью
#define FAMILY_BANK_MAX     100000000
#define FAMILY_COLOR        0xE0B040FF  // цвет семьи по умолчанию (чат семьи)

enum E_FAMILY
{
	famDb,              // номер в таблице families (0 — слот свободен)
	famName[32],
	famType[16],        // «Семья», «Банда» — как назвал создатель
	famColor,
	famBank,
	famOwner,           // аккаунт главы
	famHouse            // номер семейного дома в базе (0 — нет)
}
static Family[MAX_FAMILIES][E_FAMILY];
static gFamRankName[MAX_FAMILIES][FAMILY_RANKS][24];

static gFam[MAX_PLAYERS] = {-1, ...};       // слот семьи игрока (-1 — не состоит)
static gFamRank[MAX_PLAYERS];
static gFamInvite[MAX_PLAYERS] = {-1, ...}; // в какую семью пригласили
static gFamInviteDb[MAX_PLAYERS];           // номер семьи в базе на момент приглашения

static const gFamDefRanks[FAMILY_RANKS][24] =
{
	"Новичок", "Участник", "Доверенный", "Старший", "Заместитель", "Глава"
};

// ---------------------------------------------------------------------
//  Запуск и вход игрока
// ---------------------------------------------------------------------
stock Families_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS families (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE COLLATE NOCASE, type TEXT, color INTEGER DEFAULT 0, bank INTEGER DEFAULT 0, owner INTEGER DEFAULT 0, created INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS family_ranks (family INTEGER NOT NULL, rank INTEGER NOT NULL, name TEXT, PRIMARY KEY (family, rank))"));
	DB_AddColumn("accounts", "family", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "famrank", "INTEGER DEFAULT 0");
	DB_AddColumn("families", "house", "INTEGER DEFAULT 0");

	new loaded = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, name, type, color, bank, owner, house FROM families ORDER BY id LIMIT %d", MAX_FAMILIES);
	if (r != DBResult:0)
	{
		for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
		{
			Family[loaded][famDb] = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 1, Family[loaded][famName], 32);
			DB_GetFieldString(r, 2, Family[loaded][famType], 16);
			Family[loaded][famColor] = DB_GetFieldInt(r, 3);
			Family[loaded][famBank] = DB_GetFieldInt(r, 4);
			Family[loaded][famOwner] = DB_GetFieldInt(r, 5);
			Family[loaded][famHouse] = DB_GetFieldInt(r, 6);
			Family_DefaultRanks(loaded);
			loaded++;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}

	r = DB_ExecuteQuery(gDB, "SELECT family, rank, name FROM family_ranks");
	if (r != DBResult:0)
	{
		new name[24];
		for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
		{
			new s = Family_FindByDb(DB_GetFieldInt(r, 0)), rk = DB_GetFieldInt(r, 1);
			if (s != -1 && rk >= 1 && rk <= FAMILY_RANKS)
			{
				DB_GetFieldString(r, 2, name, sizeof name);
				if (name[0] != '\0')
				{
					gFamRankName[s][rk - 1][0] = '\0';
					strcat(gFamRankName[s][rk - 1], name, 24);
				}
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Families] Loaded %d families.", loaded);
}

static Family_DefaultRanks(s)
{
	for (new rk = 0; rk < FAMILY_RANKS; rk++)
	{
		gFamRankName[s][rk][0] = '\0';
		strcat(gFamRankName[s][rk], gFamDefRanks[rk], 24);
	}
}

static Family_FindByDb(dbid)
{
	if (dbid <= 0) return -1;
	for (new s = 0; s < MAX_FAMILIES; s++)
	{
		if (Family[s][famDb] == dbid) return s;
	}
	return -1;
}

stock Families_OnConnect(playerid)
{
	gFam[playerid] = -1;
	gFamRank[playerid] = 0;
	gFamInvite[playerid] = -1;
}

// После входа в аккаунт (accounts.inc)
stock Families_OnLogin(playerid)
{
	gFam[playerid] = -1;
	gFamRank[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT family, famrank FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		gFam[playerid] = Family_FindByDb(DB_GetFieldInt(r, 0));
		gFamRank[playerid] = DB_GetFieldInt(r, 1);
	}
	DB_FreeResultSet(r);
	if (gFam[playerid] == -1) gFamRank[playerid] = 0;
	else
	{
		if (gFamRank[playerid] < 1 || gFamRank[playerid] > FAMILY_RANKS) gFamRank[playerid] = 1;
		SendClientMessage(playerid, Family[gFam[playerid]][famColor], "%s «%s»: ты %s. Чат: /fc, команды: /famhelp",
			Family[gFam[playerid]][famType], Family[gFam[playerid]][famName], gFamRankName[gFam[playerid]][gFamRank[playerid] - 1]);
	}
}

// ---------------------------------------------------------------------
//  Помощники
// ---------------------------------------------------------------------
// Название семьи игрока для /stats и т.п. («нет», если не состоит)
stock Family_GetPlayerText(playerid)
{
	new text[64], s = gFam[playerid];
	if (s == -1) strcat(text, "нет");
	else format(text, sizeof text, "%s «%s»", Family[s][famType], Family[s][famName]);
	return text;
}

// Семейный дом игрока (индекс в House[], -1 — нет)
stock Family_HouseOf(playerid)
{
	new s = gFam[playerid];
	if (s == -1 || Family[s][famHouse] == 0) return -1;
	for (new h = 0; h < gHouseCount; h++)
	{
		if (House[h][hDbId] != Family[s][famHouse]) continue;
		if (House[h][hOwner] == 0) return -1;       // дом продали — семейного больше нет
		return h;
	}
	return -1;
}

// Игрок может войти в закрытый дом h как член семьи
stock bool:Family_IsHouseMember(playerid, h)
{
	return h != -1 && Family_HouseOf(playerid) == h;
}

static Family_HouseText(playerid)
{
	new t[24], h = Family_HouseOf(playerid);
	if (h == -1) strcat(t, "нет (/famhouse)");
	else format(t, sizeof t, "№%d", House[h][hDbId]);
	return t;
}

static Family_SaveMember(playerid)
{
	new s = gFam[playerid];
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET family = %d, famrank = %d WHERE id = %d",
		(s == -1) ? 0 : Family[s][famDb], gFamRank[playerid], Player[playerid][pDbId]));
}

static Family_SaveInfo(s)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE families SET name = '%q', type = '%q', color = %d, bank = %d, owner = %d WHERE id = %d",
		Family[s][famName], Family[s][famType], Family[s][famColor], Family[s][famBank], Family[s][famOwner], Family[s][famDb]));
}

static Family_Message(s, const message[])
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && gFam[i] == s) SendClientMessage(i, Family[s][famColor], "%s", message);
	}
}

// Игрок в семье? Если нужен ранг и его нет — пишет почему
static bool:Family_Need(playerid, rank)
{
	if (gFam[playerid] == -1)
	{
		SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь в семье. Создать свою: /famcreate");
		return false;
	}
	if (gFamRank[playerid] < rank)
	{
		if (rank >= FAMILY_RANKS) SendClientMessage(playerid, COLOR_GREY, "Это может только глава семьи.");
		else SendClientMessage(playerid, COLOR_GREY, "Это могут только глава и заместитель (ранг %d+).", rank);
		return false;
	}
	return true;
}

static bool:Cmd_FamHouse(playerid, const params[])
{
	if (!Family_Need(playerid, FAMILY_RANKS)) return true;
	new s = gFam[playerid], pos = 0, word[8], msg[144];
	if (NextParam(params, pos, word) && !strcmp(word, "off", true))
	{
		if (Family[s][famHouse] == 0) return SendClientMessage(playerid, COLOR_GREY, "Семейного дома нет.");
		Family[s][famHouse] = 0;
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE families SET house = 0 WHERE id = %d", Family[s][famDb]));
		format(msg, sizeof msg, "[%s] %s убрал семейный дом.", Family[s][famType], GetRPName(playerid));
		Family_Message(s, msg);
		return true;
	}
	new h = House_GetNearby(playerid);
	if (h == -1 && Player[playerid][pHouseInside] != -1 && House_IsInside(playerid, Player[playerid][pHouseInside])) h = Player[playerid][pHouseInside];
	if (h == -1) return SendClientMessage(playerid, COLOR_GREY, "Встань у двери своего дома (или зайди в него). Убрать семейный дом: /famhouse off");
	if (House[h][hOwner] != Player[playerid][pDbId]) return SendClientMessage(playerid, COLOR_GREY, "Семейным можно сделать только свой дом.");
	Family[s][famHouse] = House[h][hDbId];
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE families SET house = %d WHERE id = %d", Family[s][famHouse], Family[s][famDb]));
	format(msg, sizeof msg, "[%s] %s сделал дом №%d семейным. Входить может вся семья, спавн: /setspawn.", Family[s][famType], GetRPName(playerid), House[h][hDbId]);
	Family_Message(s, msg);
	return true;
}

static Family_Members(s)
{
	new count = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM accounts WHERE family = %d", Family[s][famDb]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return count;
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
static bool:Cmd_FamCreate(playerid, const params[])
{
	new pos = 0, type[16], name[32];
	if (!NextParam(params, pos, type) || !RestParam(params, pos, name))
	{
		SendClientMessage(playerid, COLOR_GREY, "Использование: /famcreate [тип одним словом] [название]. Пример: /famcreate Семья Corleone");
		return SendClientMessage(playerid, COLOR_GREY, "Цена $%d наличными, нужен уровень %d.", FAMILY_PRICE, FAMILY_MIN_LEVEL);
	}
	if (gFam[playerid] != -1) return SendClientMessage(playerid, COLOR_GREY, "Ты уже состоишь в семье. Сначала выйди: /famleave");
	if (Player[playerid][pLevel] < FAMILY_MIN_LEVEL) return SendClientMessage(playerid, COLOR_GREY, "Создать семью можно с %d уровня.", FAMILY_MIN_LEVEL);
	if (!Faction_IsGoodName(type, 3, 15)) return SendClientMessage(playerid, COLOR_GREY, "Тип: одно слово от 3 до 15 символов (Семья, Банда, Клан).");
	if (!Faction_IsGoodName(name, 3, 30)) return SendClientMessage(playerid, COLOR_GREY, "Название: от 3 до 30 символов, без { } и %%.");
	if (GetMoney(playerid) < FAMILY_PRICE) return SendClientMessage(playerid, COLOR_GREY, "Нужно $%d наличными.", FAMILY_PRICE);

	new s = -1;
	for (new i = 0; i < MAX_FAMILIES; i++)
	{
		if (Family[i][famDb] == 0) { if (s == -1) s = i; }
		else if (!strcmp(Family[i][famName], name, true)) return SendClientMessage(playerid, COLOR_GREY, "Такое название уже занято.");
	}
	if (s == -1) return SendClientMessage(playerid, COLOR_GREY, "На сервере уже слишком много семей. Сообщи администрации.");

	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO families (name, type, color, bank, owner, created) VALUES ('%q', '%q', %d, 0, %d, %d)",
		name, type, FAMILY_COLOR, Player[playerid][pDbId], gettime()));
	new dbid = DB_LastInsertId();
	if (dbid <= 0) return SendClientMessage(playerid, COLOR_GREY, "Не удалось создать семью. Попробуй другое название.");

	Family[s][famDb] = dbid;
	Family[s][famName][0] = '\0';
	strcat(Family[s][famName], name, 32);
	Family[s][famType][0] = '\0';
	strcat(Family[s][famType], type, 16);
	Family[s][famColor] = FAMILY_COLOR;
	Family[s][famBank] = 0;
	Family[s][famOwner] = Player[playerid][pDbId];
	Family_DefaultRanks(s);

	GiveMoney(playerid, -FAMILY_PRICE, "создание семьи");
	Econ_ToTreasury(FAMILY_PRICE);
	gFam[playerid] = s;
	gFamRank[playerid] = FAMILY_RANKS;
	Family_SaveMember(playerid);
	Account_Save(playerid);

	SendClientMessage(playerid, COLOR_GREEN, "Создано: %s «%s». Ты глава. Пригласить: /faminvite [id], все команды: /famhelp", type, name);
	return true;
}

static bool:Cmd_FamInvite(playerid, const params[])
{
	if (!Family_Need(playerid, FAMILY_RANKS - 1)) return true;
	new pos = 0, s = gFam[playerid];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /faminvite [id]");
	if (target == playerid || !Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Неверный игрок.");
	if (gFam[target] != -1) return SendClientMessage(playerid, COLOR_GREY, "Игрок уже состоит в семье.");
	if (!IsPlayerNearPlayer(playerid, target, 5.0)) return SendClientMessage(playerid, COLOR_GREY, "Игрок слишком далеко.");

	gFamInvite[target] = s;
	gFamInviteDb[target] = Family[s][famDb];
	SendClientMessage(target, Family[s][famColor], "%s приглашает тебя: %s «%s». Принять: /famaccept",
		GetRPName(playerid), Family[s][famType], Family[s][famName]);
	SendClientMessage(playerid, COLOR_WHITE, "Ты пригласил %s. Ждём ответа.", GetRPName(target));
	return true;
}

static bool:Cmd_FamAccept(playerid)
{
	new s = gFamInvite[playerid];
	gFamInvite[playerid] = -1;
	if (s == -1 || Family[s][famDb] == 0 || Family[s][famDb] != gFamInviteDb[playerid])
		return SendClientMessage(playerid, COLOR_GREY, "Тебя никуда не приглашали или приглашение устарело.");
	if (gFam[playerid] != -1) return SendClientMessage(playerid, COLOR_GREY, "Ты уже состоишь в семье.");

	gFam[playerid] = s;
	gFamRank[playerid] = 1;
	Family_SaveMember(playerid);
	new msg[144];
	format(msg, sizeof msg, "[%s] %s вступил в «%s».", Family[s][famType], GetRPName(playerid), Family[s][famName]);
	Family_Message(s, msg);
	return true;
}

static bool:Cmd_FamKick(playerid, const params[])
{
	if (!Family_Need(playerid, FAMILY_RANKS - 1)) return true;
	new pos = 0, s = gFam[playerid];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /famkick [id]. Кого нет в игре: /famoffkick [Имя_Фамилия]");
	if (target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Чтобы уйти самому: /famleave");
	if (gFam[target] != s) return SendClientMessage(playerid, COLOR_GREY, "Игрок не в твоей семье.");
	if (gFamRank[target] >= gFamRank[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Нельзя выгнать равного или старшего по рангу.");

	new msg[144];
	format(msg, sizeof msg, "[%s] %s выгнал %s.", Family[s][famType], GetRPName(playerid), GetRPName(target));
	Family_Message(s, msg);
	gFam[target] = -1;
	gFamRank[target] = 0;
	Family_SaveMember(target);
	return true;
}

// Выгнать того, кого нет в игре
static bool:Cmd_FamOffKick(playerid, const params[])
{
	if (!Family_Need(playerid, FAMILY_RANKS - 1)) return true;
	new pos = 0, name[MAX_PLAYER_NAME], s = gFam[playerid];
	if (!NextParam(params, pos, name)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /famoffkick [Имя_Фамилия]");

	new dbid = 0, rank = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, famrank FROM accounts WHERE name = '%q' AND family = %d LIMIT 1", name, Family[s][famDb]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			dbid = DB_GetFieldInt(r, 0);
			rank = DB_GetFieldInt(r, 1);
		}
		DB_FreeResultSet(r);
	}
	if (dbid == 0) return SendClientMessage(playerid, COLOR_GREY, "В твоей семье нет такого человека.");
	if (dbid == Player[playerid][pDbId]) return SendClientMessage(playerid, COLOR_GREY, "Чтобы уйти самому: /famleave");
	if (rank >= gFamRank[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Нельзя выгнать равного или старшего по рангу.");
	if (Faction_FindOnline(dbid) != INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Этот человек сейчас в игре: /famkick [id]");

	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET family = 0, famrank = 0 WHERE id = %d", dbid));
	new msg[144];
	format(msg, sizeof msg, "[%s] %s выгнал %s (не в игре).", Family[s][famType], GetRPName(playerid), name);
	Family_Message(s, msg);
	return true;
}

static bool:Cmd_FamRank(playerid, const params[])
{
	if (!Family_Need(playerid, FAMILY_RANKS - 1)) return true;
	new pos = 0, rank, s = gFam[playerid];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, rank))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /famrank [id] [ранг 1-%d]", gFamRank[playerid] - 1);
	if (target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Себе ранг менять нельзя.");
	if (gFam[target] != s) return SendClientMessage(playerid, COLOR_GREY, "Игрок не в твоей семье.");
	if (gFamRank[target] >= gFamRank[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Нельзя менять ранг равному или старшему.");
	if (rank < 1 || rank >= gFamRank[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Ранг должен быть от 1 до %d.", gFamRank[playerid] - 1);

	gFamRank[target] = rank;
	Family_SaveMember(target);
	new msg[144];
	format(msg, sizeof msg, "[%s] %s назначил %s: «%s» (%d).", Family[s][famType], GetRPName(playerid), GetRPName(target), gFamRankName[s][rank - 1], rank);
	Family_Message(s, msg);
	return true;
}

static bool:Cmd_FamLeave(playerid)
{
	if (!Family_Need(playerid, 1)) return true;
	new s = gFam[playerid];
	if (gFamRank[playerid] >= FAMILY_RANKS)
		return SendClientMessage(playerid, COLOR_GREY, "Глава не может просто уйти. Передай семью: /famleader [id] или распусти её: /famdisband");
	new msg[144];
	format(msg, sizeof msg, "[%s] %s вышел из «%s».", Family[s][famType], GetRPName(playerid), Family[s][famName]);
	Family_Message(s, msg);
	gFam[playerid] = -1;
	gFamRank[playerid] = 0;
	Family_SaveMember(playerid);
	return true;
}

static bool:Cmd_FamLeader(playerid, const params[])
{
	if (!Family_Need(playerid, FAMILY_RANKS)) return true;
	new pos = 0, s = gFam[playerid];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /famleader [id] — передать семью другому её участнику");
	if (gFam[target] != s) return SendClientMessage(playerid, COLOR_GREY, "Игрок не в твоей семье.");

	gFamRank[target] = FAMILY_RANKS;
	gFamRank[playerid] = FAMILY_RANKS - 1;
	Family[s][famOwner] = Player[target][pDbId];
	Family_SaveMember(target);
	Family_SaveMember(playerid);
	Family_SaveInfo(s);
	new msg[144];
	format(msg, sizeof msg, "[%s] %s передал главенство %s.", Family[s][famType], GetRPName(playerid), GetRPName(target));
	Family_Message(s, msg);
	return true;
}

static bool:Cmd_FamDisband(playerid, const params[])
{
	if (!Family_Need(playerid, FAMILY_RANKS)) return true;
	new pos = 0, word[16], s = gFam[playerid];
	if (!NextParam(params, pos, word) || strcmp(word, "да", true) != 0)
		return SendClientMessage(playerid, COLOR_GREY, "Семья будет удалена навсегда, общак вернётся тебе. Подтверди: /famdisband да");

	new msg[144], bank = Family[s][famBank], dbid = Family[s][famDb];
	format(msg, sizeof msg, "[%s] %s распустил «%s».", Family[s][famType], GetRPName(playerid), Family[s][famName]);
	Family_Message(s, msg);

	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gFam[i] == s) { gFam[i] = -1; gFamRank[i] = 0; }
		if (gFamInvite[i] == s) gFamInvite[i] = -1;
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET family = 0, famrank = 0 WHERE family = %d", dbid));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM family_ranks WHERE family = %d", dbid));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM families WHERE id = %d", dbid));
	Family[s][famDb] = 0;
	Family[s][famBank] = 0;
	if (bank > 0)
	{
		GiveMoney(playerid, bank, "общак распущенной семьи");
		Account_Save(playerid);
	}
	return true;
}

static bool:Cmd_Fam(playerid)
{
	if (!Family_Need(playerid, 1)) return true;
	new s = gFam[playerid], text[1200], line[96], online = 0;
	format(text, sizeof text, "{FFFFFF}%s «%s»\nУчастников: %d\nОбщак: {33AA33}$%d{FFFFFF}\nТвой ранг: %s (%d)\nСемейный дом: %s\n\n{FFFF00}Сейчас в игре\n{FFFFFF}",
		Family[s][famType], Family[s][famName], Family_Members(s), Family[s][famBank], gFamRankName[s][gFamRank[playerid] - 1], gFamRank[playerid], Family_HouseText(playerid));
	for (new i = 0; i < MAX_PLAYERS && online < 25; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || gFam[i] != s) continue;
		format(line, sizeof line, "%s [%d] — %s\n", GetRPName(i), i, gFamRankName[s][gFamRank[i] - 1]);
		strcat(text, line);
		online++;
	}
	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, Family[s][famName], "%s", "Закрыть", "", text);
	return true;
}

static bool:Cmd_FamChat(playerid, const params[])
{
	if (!Family_Need(playerid, 1)) return true;
	new text[110], msg[144], s = gFam[playerid];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /fc [текст]");
	format(msg, sizeof msg, "[%s] %s %s: %s", Family[s][famType], gFamRankName[s][gFamRank[playerid] - 1], GetRPName(playerid), text);
	Family_Message(s, msg);
	return true;
}

static bool:Cmd_FamBank(playerid, const params[])
{
	if (!Family_Need(playerid, 1)) return true;
	new pos = 0, what[8], amount, s = gFam[playerid], msg[144];
	if (!NextParam(params, pos, what) || !NextIntParam(params, pos, amount) || amount < 1)
		return SendClientMessage(playerid, COLOR_GREY, "Общак: $%d. Положить: /fambank put [сумма]. Снять (глава): /fambank take [сумма]", Family[s][famBank]);

	if (!strcmp(what, "put", true))
	{
		if (GetMoney(playerid) < amount) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет столько наличных.");
		if (Family[s][famBank] + amount > FAMILY_BANK_MAX) return SendClientMessage(playerid, COLOR_GREY, "Общак переполнен.");
		GiveMoney(playerid, -amount, "в общак семьи");
		Family[s][famBank] += amount;
		format(msg, sizeof msg, "[%s] %s положил в общак $%d. В общаке: $%d.", Family[s][famType], GetRPName(playerid), amount, Family[s][famBank]);
	}
	else if (!strcmp(what, "take", true))
	{
		if (gFamRank[playerid] < FAMILY_RANKS) return SendClientMessage(playerid, COLOR_GREY, "Снимать деньги из общака может только глава.");
		if (Family[s][famBank] < amount) return SendClientMessage(playerid, COLOR_GREY, "В общаке только $%d.", Family[s][famBank]);
		Family[s][famBank] -= amount;
		GiveMoney(playerid, amount, "из общака семьи");
		format(msg, sizeof msg, "[%s] %s снял из общака $%d. В общаке: $%d.", Family[s][famType], GetRPName(playerid), amount, Family[s][famBank]);
	}
	else return SendClientMessage(playerid, COLOR_GREY, "Использование: /fambank put [сумма] или /fambank take [сумма]");

	Family_SaveInfo(s);
	Account_Save(playerid);
	Family_Message(s, msg);
	return true;
}

static bool:Cmd_FamRankName(playerid, const params[])
{
	if (!Family_Need(playerid, FAMILY_RANKS)) return true;
	new pos = 0, rank, name[24], s = gFam[playerid];
	if (!NextIntParam(params, pos, rank) || rank < 1 || rank > FAMILY_RANKS || !RestParam(params, pos, name))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /famrankname [ранг 1-%d] [название]", FAMILY_RANKS);
	if (!Faction_IsGoodName(name, 2, 23)) return SendClientMessage(playerid, COLOR_GREY, "Название ранга: от 2 до 23 символов, без { } и %%.");

	gFamRankName[s][rank - 1][0] = '\0';
	strcat(gFamRankName[s][rank - 1], name, 24);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO family_ranks (family, rank, name) VALUES (%d, %d, '%q')", Family[s][famDb], rank, name));
	SendClientMessage(playerid, COLOR_GREEN, "Ранг %d теперь называется «%s».", rank, name);
	return true;
}

static bool:Cmd_FamColor(playerid, const params[])
{
	if (!Family_Need(playerid, FAMILY_RANKS)) return true;
	new pos = 0, hex[16], s = gFam[playerid], value = 0;
	if (!NextParam(params, pos, hex) || strlen(hex) != 6)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /famcolor [RRGGBB], например /famcolor E0B040");
	for (new i = 0; i < 6; i++)
	{
		new c = hex[i], d;
		if (c >= '0' && c <= '9') d = c - '0';
		else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
		else return SendClientMessage(playerid, COLOR_GREY, "Цвет — шесть знаков 0-9 и A-F, например E0B040.");
		value = value * 16 + d;
	}
	Family[s][famColor] = (value << 8) | 0xFF;
	Family_SaveInfo(s);
	SendClientMessage(playerid, Family[s][famColor], "Цвет семьи изменён.");
	return true;
}

static bool:Cmd_FamHelp(playerid)
{
	new text[1200], line[96];
	strcat(text, "{FFFF00}Семьи\n{FFFFFF}Семью создаёт сам игрок. Можно состоять и во фракции, и в семье.\n\n");
	format(line, sizeof line, "/famcreate [тип] [название] — создать ($%d, уровень %d+)\n", FAMILY_PRICE, FAMILY_MIN_LEVEL);
	strcat(text, line);
	strcat(text, "/fam — состав и общак   /fc [текст] — чат семьи\n");
	strcat(text, "/famaccept — принять приглашение   /famleave — выйти\n");
	strcat(text, "/fambank put [сумма] — положить в общак\n\n");
	strcat(text, "{FFFF00}Глава и заместитель (ранг 5-6)\n{FFFFFF}");
	strcat(text, "/faminvite [id]   /famkick [id]   /famoffkick [Имя_Фамилия]\n/famrank [id] [ранг]\n\n");
	strcat(text, "{FFFF00}Только глава (ранг 6)\n{FFFFFF}");
	strcat(text, "/fambank take [сумма] — снять из общака\n/famrankname [ранг] [название]   /famcolor [RRGGBB]\n/famhouse — свой дом сделать семейным (off — убрать)\n");
	strcat(text, "/famleader [id] — передать семью   /famdisband — распустить");
	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "Семьи", "%s", "Закрыть", "", text);
	return true;
}

// Из commands.inc. Возвращает 1, если команда наша.
stock Families_OnCommand(playerid, const cmd[], const params[])
{
	if (strcmp(cmd, "/fam", true, 4) != 0 && strcmp(cmd, "/fc", true) != 0) return 0;

	if (!strcmp(cmd, "/fam", true))         { Cmd_Fam(playerid); return 1; }
	if (!strcmp(cmd, "/fc", true))          { Cmd_FamChat(playerid, params); return 1; }
	if (!strcmp(cmd, "/famhelp", true))     { Cmd_FamHelp(playerid); return 1; }
	if (!strcmp(cmd, "/famcreate", true))   { Cmd_FamCreate(playerid, params); return 1; }
	if (!strcmp(cmd, "/faminvite", true))   { Cmd_FamInvite(playerid, params); return 1; }
	if (!strcmp(cmd, "/famaccept", true))   { Cmd_FamAccept(playerid); return 1; }
	if (!strcmp(cmd, "/famkick", true))     { Cmd_FamKick(playerid, params); return 1; }
	if (!strcmp(cmd, "/famoffkick", true))  { Cmd_FamOffKick(playerid, params); return 1; }
	if (!strcmp(cmd, "/famrank", true))     { Cmd_FamRank(playerid, params); return 1; }
	if (!strcmp(cmd, "/famleave", true))    { Cmd_FamLeave(playerid); return 1; }
	if (!strcmp(cmd, "/famleader", true))   { Cmd_FamLeader(playerid, params); return 1; }
	if (!strcmp(cmd, "/famdisband", true))  { Cmd_FamDisband(playerid, params); return 1; }
	if (!strcmp(cmd, "/fambank", true))     { Cmd_FamBank(playerid, params); return 1; }
	if (!strcmp(cmd, "/famrankname", true)) { Cmd_FamRankName(playerid, params); return 1; }
	if (!strcmp(cmd, "/famcolor", true))    { Cmd_FamColor(playerid, params); return 1; }
	if (!strcmp(cmd, "/famhouse", true))    { Cmd_FamHouse(playerid, params); return 1; }
	return 0;
}
