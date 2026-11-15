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

// Права в семье (раздел «Управление» в конце файла): номер бита в маске
#define FMP_INVITE          0
#define FMP_KICK            1
#define FMP_RANK            2
#define FMP_WARN            3
#define FMP_BANK_IN         4
#define FMP_BANK_OUT        5
#define FMP_HOUSE           6
#define FMP_RANKNAMES       7
#define FMP_LOG             8
#define FMP_MUTE            9
#define FMP_COUNT           10
#define FMP_ALL             ((1 << FMP_COUNT) - 1)
#define FAMILY_MAX_WARNS    3           // на третьем выговоре — выгоняют из семьи

static const gFamPermName[FMP_COUNT][36] =
{
	"Принимать",
	"Выгонять",
	"Менять ранг (ниже своего)",
	"Выговоры: выдать и снять",
	"Общак: положить",
	"Общак: снять",
	"Семейный дом: вход, замок, спавн",
	"Названия рангов",
	"Журнал",
	"Мут чата /fc"
};
static gFamRankPerm[MAX_FAMILIES][FAMILY_RANKS];   // права рангов (индекс: ранг - 1)
static gFamPOn[MAX_PLAYERS];                        // личные права: включено
static gFamPOff[MAX_PLAYERS];                       // личные права: выключено
static gFamMgOn[MAX_PLAYERS];                       // личные права выбранного участника (для окна)
static gFamMgOff[MAX_PLAYERS];
static gFamInfoBack[MAX_PLAYERS];                   // куда вернуться из окна-информации
static gFamLogKind[MAX_PLAYERS];
static gFamLogName[MAX_PLAYERS][MAX_PLAYER_NAME];
static gFamMuteUntil[MAX_PLAYERS];                 // мут чата /fc до (unix-время)
static gFamDisabled[MAX_FAMILIES];                  // права, отключённые владельцем для всей семьи (/orgs)

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
	FamPerm_Init();         // права рангов (раздел «Управление»)
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
	FamPerm_LoadPlayer(playerid);
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
	if (!FamPerm_Has(playerid, FMP_HOUSE)) return -1;  // право «Семейный дом» (вход, замок, спавн)
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
	FamPerm_ResetSlot(s);
	gFamDisabled[s] = 0;

	GiveMoney(playerid, -FAMILY_PRICE, "создание семьи");
	Econ_ToTreasury(FAMILY_PRICE);
	gFam[playerid] = s;
	gFamRank[playerid] = FAMILY_RANKS;
	Family_SaveMember(playerid);
	FamPerm_OnJoinLeave(playerid, true);
	Account_Save(playerid);

	SendClientMessage(playerid, COLOR_GREEN, "Создано: %s «%s». Ты глава. Пригласить: /faminvite [id], все команды: /famhelp", type, name);
	return true;
}

static bool:Cmd_FamInvite(playerid, const params[])
{
	if (!FamPerm_Need(playerid, FMP_INVITE)) return true;
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
	FamPerm_OnJoinLeave(playerid, true);
	new msg[144];
	format(msg, sizeof msg, "[%s] %s вступил в «%s».", Family[s][famType], GetRPName(playerid), Family[s][famName]);
	Fam_Announce(s, FLOG_STAFF, msg);
	return true;
}

static bool:Cmd_FamKick(playerid, const params[])
{
	if (!FamPerm_Need(playerid, FMP_KICK)) return true;
	new pos = 0, s = gFam[playerid];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /famkick [id]. Кого нет в игре: /famoffkick [Имя_Фамилия]");
	if (target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Чтобы уйти самому: /famleave");
	if (gFam[target] != s) return SendClientMessage(playerid, COLOR_GREY, "Игрок не в твоей семье.");
	if (gFamRank[target] >= gFamRank[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Нельзя выгнать равного или старшего по рангу.");

	new msg[144];
	format(msg, sizeof msg, "[%s] %s выгнал %s.", Family[s][famType], GetRPName(playerid), GetRPName(target));
	Fam_Announce(s, FLOG_STAFF, msg);
	gFam[target] = -1;
	gFamRank[target] = 0;
	Family_SaveMember(target);
	FamPerm_OnJoinLeave(target, false);
	return true;
}

// Выгнать того, кого нет в игре
static bool:Cmd_FamOffKick(playerid, const params[])
{
	if (!FamPerm_Need(playerid, FMP_KICK)) return true;
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

	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET family = 0, famrank = 0, famperm_on = 0, famperm_off = 0, famwarns = 0, famnote = NULL, famjoin = 0 WHERE id = %d", dbid));
	new msg[144];
	format(msg, sizeof msg, "[%s] %s выгнал %s (не в игре).", Family[s][famType], GetRPName(playerid), name);
	Fam_Announce(s, FLOG_STAFF, msg);
	return true;
}

static bool:Cmd_FamRank(playerid, const params[])
{
	if (!FamPerm_Need(playerid, FMP_RANK)) return true;
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
	Fam_Announce(s, FLOG_STAFF, msg);
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
	Fam_Announce(s, FLOG_STAFF, msg);
	gFam[playerid] = -1;
	gFamRank[playerid] = 0;
	Family_SaveMember(playerid);
	FamPerm_OnJoinLeave(playerid, false);
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
	Fam_Announce(s, FLOG_STAFF, msg);
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
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET family = 0, famrank = 0, famperm_on = 0, famperm_off = 0, famwarns = 0, famnote = NULL, famjoin = 0 WHERE family = %d", dbid));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM family_ranks WHERE family = %d", dbid));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM family_perms WHERE family = %d", dbid));
	for (new i = 0; i < MAX_PLAYERS; i++) if (gFam[i] == -1) { gFamPOn[i] = 0; gFamPOff[i] = 0; }
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

static bool:Cmd_FamChat(playerid, const params[])
{
	if (!Family_Need(playerid, 1)) return true;
	new text[110], msg[144], s = gFam[playerid];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /fc [текст]");
	if (gFamMuteUntil[playerid] > gettime())
		return SendClientMessage(playerid, COLOR_GREY, "Тебе запретили писать в чат семьи ещё на %d мин.", (gFamMuteUntil[playerid] - gettime()) / 60 + 1);
	format(msg, sizeof msg, "[%s] %s %s: %s", Family[s][famType], gFamRankName[s][gFamRank[playerid] - 1], GetRPName(playerid), text);
	Family_Message(s, msg);
	return true;
}

static bool:Cmd_FamBank(playerid, const params[])
{
	if (!Family_Need(playerid, 1)) return true;
	new pos = 0, what[8], amount, s = gFam[playerid], msg[144];
	if (!NextParam(params, pos, what) || !NextIntParam(params, pos, amount) || amount < 1)
		return SendClientMessage(playerid, COLOR_GREY, "Общак: $%d. Положить: /fambank put [сумма]. Снять (по праву): /fambank take [сумма]", Family[s][famBank]);

	if (!strcmp(what, "put", true))
	{
		if (!FamPerm_Need(playerid, FMP_BANK_IN)) return true;
		if (GetMoney(playerid) < amount) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет столько наличных.");
		if (Family[s][famBank] + amount > FAMILY_BANK_MAX) return SendClientMessage(playerid, COLOR_GREY, "Общак переполнен.");
		GiveMoney(playerid, -amount, "в общак семьи");
		Family[s][famBank] += amount;
		format(msg, sizeof msg, "[%s] %s положил в общак $%d. В общаке: $%d.", Family[s][famType], GetRPName(playerid), amount, Family[s][famBank]);
	}
	else if (!strcmp(what, "take", true))
	{
		if (!FamPerm_Need(playerid, FMP_BANK_OUT)) return true;
		if (Family[s][famBank] < amount) return SendClientMessage(playerid, COLOR_GREY, "В общаке только $%d.", Family[s][famBank]);
		Family[s][famBank] -= amount;
		GiveMoney(playerid, amount, "из общака семьи");
		format(msg, sizeof msg, "[%s] %s снял из общака $%d. В общаке: $%d.", Family[s][famType], GetRPName(playerid), amount, Family[s][famBank]);
	}
	else return SendClientMessage(playerid, COLOR_GREY, "Использование: /fambank put [сумма] или /fambank take [сумма]");

	Family_SaveInfo(s);
	Account_Save(playerid);
	Fam_Announce(s, FLOG_MONEY, msg);
	return true;
}

static bool:Cmd_FamRankName(playerid, const params[])
{
	if (!FamPerm_Need(playerid, FMP_RANKNAMES)) return true;
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
	strcat(text, "/family — окно семьи: состав, общак, ранги   /fc [текст] — чат семьи\n");
	strcat(text, "/famaccept — принять приглашение   /famleave — выйти\n");
	strcat(text, "/fambank put [сумма] — положить в общак\n\n");
	strcat(text, "{FFFF00}По правам (их раздают глава и зам: /family > Управление)\n{FFFFFF}");
	strcat(text, "/faminvite [id]   /famkick [id]   /famoffkick [Имя_Фамилия]\n/famrank [id] [ранг]   /fambank take [сумма]   /famrankname [ранг] [название]\n");
	strcat(text, "/fammute [id] [минуты] [причина] — мут чата /fc (0 — снять)\n");
	strcat(text, "В «Управлении»: карточки участников, выговоры, права рангов и личные, журнал\n\n");
	strcat(text, "{FFFF00}Только глава (ранг 6)\n{FFFFFF}");
	strcat(text, "/famcolor [RRGGBB]   /famhouse — свой дом сделать семейным (off — убрать)\n");
	strcat(text, "/famleader [id] — передать семью   /famdisband — распустить");
	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "Семьи", "%s", "Закрыть", "", text);
	return true;
}

// Из commands.inc. Возвращает 1, если команда наша.
// /fammute [id] [минуты] [причина] — мут чата семьи, 0 — снять
static bool:Cmd_FamMute(playerid, const params[])
{
	if (!FamPerm_Need(playerid, FMP_MUTE)) return true;
	new s = gFam[playerid], pos = 0, mins, reason[64], msg[144];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, mins) || mins < 0 || mins > 1440)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /fammute [id] [минуты 1-1440, 0 — снять] [причина]");
	if (!Player[target][pLogged] || gFam[target] != s) return SendClientMessage(playerid, COLOR_GREY, "Игрок не в твоей семье.");
	if (target == playerid || gFamRank[target] >= gFamRank[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Нельзя управлять равным или старшим по рангу.");
	if (!RestParam(params, pos, reason)) format(reason, sizeof reason, "без причины");
	gFamMuteUntil[target] = (mins > 0) ? (gettime() + mins * 60) : 0;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET fammute_until = %d WHERE id = %d", gFamMuteUntil[target], Player[target][pDbId]));
	if (mins > 0)
	{
		format(msg, sizeof msg, "%s запретил %s писать в чат семьи на %d мин. Причина: %s", GetRPName(playerid), GetRPName(target), mins, reason);
		SendClientMessage(target, COLOR_RED, "%s запретил тебе писать в чат семьи на %d мин. Причина: %s", GetRPName(playerid), mins, reason);
	}
	else
	{
		format(msg, sizeof msg, "%s снял мут чата семьи с %s.", GetRPName(playerid), GetRPName(target));
		SendClientMessage(target, COLOR_GREEN, "%s разрешил тебе снова писать в чат семьи.", GetRPName(playerid));
	}
	Fam_Log(s, FLOG_STAFF, msg);
	SendClientMessage(playerid, COLOR_GREEN, "Готово.");
	return true;
}

stock Families_OnCommand(playerid, const cmd[], const params[])
{
	if (strcmp(cmd, "/fam", true, 4) != 0 && strcmp(cmd, "/fc", true) != 0) return 0;

	if (!strcmp(cmd, "/fam", true) || !strcmp(cmd, "/family", true)) { FamMenu_Show(playerid); return 1; }
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
	if (!strcmp(cmd, "/fammute", true))     { Cmd_FamMute(playerid, params); return 1; }
	return 0;
}

// =====================================================================
//  Окно семьи: /family (и /fam). Информация, состав, общак, ранги.
//  Глава и заместитель управляют участниками прямо из списка.
//  Действия идут через те же команды (/famkick, /famrank...), поэтому
//  все проверки и сообщения семье — в одном месте.
// =====================================================================
#define FAM_LIST_MAX        40

static gFamListDb[MAX_PLAYERS][FAM_LIST_MAX];  // номера аккаунтов в списке «Весь состав»
static gFamSelDb[MAX_PLAYERS];                  // выбранный участник
static gFamSelRank[MAX_PLAYERS];                // выбранный ранг (окно «Ранги»)

static Family_OnlineCount(s)
{
	new n = 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && gFam[i] == s) n++;
	}
	return n;
}

// Состоит ли игрок в семье (для /members)
stock Family_Has(playerid)
{
	return (gFam[playerid] != -1) ? 1 : 0;
}

stock FamMenu_Show(playerid)
{
	if (!Family_Need(playerid, 1)) return 1;
	new s = gFam[playerid], body[800], line[128], title[64];
	format(line, sizeof line, "Информация\t{AFAFAF}твой ранг: %s\n", gFamRankName[s][gFamRank[playerid] - 1]);
	strcat(body, line);
	format(line, sizeof line, "Участники онлайн\t{33AA33}%d\n", Family_OnlineCount(s));
	strcat(body, line);
	format(line, sizeof line, "Весь состав\t%d чел.\n", Family_Members(s));
	strcat(body, line);
	format(line, sizeof line, "Общак\t{33AA33}$%d\n", Family[s][famBank]);
	strcat(body, line);
	strcat(body, "Ранги\t{AFAFAF}названия\n");
	if (FamPerm_Has(playerid, FMP_INVITE)) strcat(body, "Пригласить игрока\t{AFAFAF}по id, рядом\n");
	else strcat(body, "{AFAFAF}Пригласить игрока\t{AFAFAF}по праву\n");
	if (FamPerm_IsManager(playerid)) strcat(body, "{FFFF00}Управление\t{AFAFAF}карточки, права, журнал\n");
	else strcat(body, "{AFAFAF}Управление\t{AFAFAF}глава, зам и кому выдали права\n");
	strcat(body, "Все команды семьи\t{AFAFAF}/famhelp\n");
	if (gFamRank[playerid] >= FAMILY_RANKS) strcat(body, "{FF6347}Распустить семью\t");
	else strcat(body, "{FF6347}Покинуть семью\t");
	format(title, sizeof title, "%s «%s»", Family[s][famType], Family[s][famName]);
	ShowPlayerDialog(playerid, DIALOG_FAM_MENU, DIALOG_STYLE_TABLIST, title, "%s", "Выбрать", "Закрыть", body);
	return 1;
}

static FamMenu_Info(playerid)
{
	new s = gFam[playerid], owner[MAX_PLAYER_NAME], created = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT a.name, f.created FROM families f LEFT JOIN accounts a ON a.id = f.owner WHERE f.id = %d", Family[s][famDb]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			DB_GetFieldString(r, 0, owner, sizeof owner);
			created = DB_GetFieldInt(r, 1);
		}
		DB_FreeResultSet(r);
	}
	if (owner[0] == '\0') strcat(owner, "—");

	new text[1200], line[160];
	format(line, sizeof line, "{AFAFAF}Название: {%06x}%s\n", Family[s][famColor] >>> 8, Family[s][famName]);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Тип: {FFFFFF}%s\n", Family[s][famType]);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Глава: {FFFFFF}%s\n", owner);
	strcat(text, line);
	if (created > 0)
	{
		format(line, sizeof line, "{AFAFAF}Основана: {FFFFFF}%d дн. назад\n", (gettime() - created) / 86400);
		strcat(text, line);
	}
	format(line, sizeof line, "\n{AFAFAF}Участников: {FFFFFF}%d {AFAFAF}(в игре: {33AA33}%d{AFAFAF})\n", Family_Members(s), Family_OnlineCount(s));
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Общак: {33AA33}$%d\n", Family[s][famBank]);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Семейный дом: {FFFFFF}%s\n", Family_HouseText(playerid));
	strcat(text, line);
	format(line, sizeof line, "\n{AFAFAF}Твой ранг: {FFFFFF}%s (%d из %d)\n", gFamRankName[s][gFamRank[playerid] - 1], gFamRank[playerid], FAMILY_RANKS);
	strcat(text, line);
	if (gFamRank[playerid] >= FAMILY_RANKS) strcat(text, "{AFAFAF}Права: {FFFFFF}все\n");
	else
	{
		format(line, sizeof line, "{AFAFAF}Права: {FFFFFF}%d из %d {AFAFAF}(Управление > Мои права)\n", FamPerm_Count(FamPerm_Mask(playerid)), FMP_COUNT);
		strcat(text, line);
	}
	strcat(text, "\n{AFAFAF}Чат семьи: {FFFFFF}/fc [текст]");
	ShowPlayerDialog(playerid, DIALOG_FAM_BACK, DIALOG_STYLE_MSGBOX, "Информация о семье", "%s", "Назад", "", text);
}

// Онлайн участники. fromMenu: «Назад» вернёт в меню семьи
stock FamMenu_Online(playerid, bool:fromMenu)
{
	if (!Family_Need(playerid, 1)) return 1;
	new s = gFam[playerid], body[3000], line[128], count = 0;
	strcat(body, "Игрок\tРанг\tУровень\tТелефон\n");
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || gFam[i] != s) continue;
		if (Player[i][pPhone] > 0)
			format(line, sizeof line, "%s [%d]\t%d. %s\t%d\t%d\n", GetRPName(i), i, gFamRank[i], gFamRankName[s][gFamRank[i] - 1], Player[i][pLevel], Player[i][pPhone]);
		else
			format(line, sizeof line, "%s [%d]\t%d. %s\t%d\t{AFAFAF}нет\n", GetRPName(i), i, gFamRank[i], gFamRankName[s][gFamRank[i] - 1], Player[i][pLevel]);
		if (strlen(body) + strlen(line) >= sizeof body) break;
		strcat(body, line);
		count++;
	}
	new title[64];
	format(title, sizeof title, "%s «%s» — в игре %d", Family[s][famType], Family[s][famName], count);
	ShowPlayerDialog(playerid, DIALOG_FAM_ONLINE, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", fromMenu ? ("Назад") : ("Меню"), "Закрыть", body);
	return 1;
}

static FamMenu_All(playerid)
{
	new s = gFam[playerid], body[3200], line[128], name[MAX_PLAYER_NAME], count = 0;
	strcat(body, "Имя\tРанг\tСтатус\n");
	for (new i = 0; i < FAM_LIST_MAX; i++) gFamListDb[playerid][i] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, name, famrank, level FROM accounts WHERE family = %d ORDER BY famrank DESC, name LIMIT %d", Family[s][famDb], FAM_LIST_MAX);
	if (r != DBResult:0)
	{
		for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
		{
			new dbid = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 1, name, sizeof name);
			new rank = DB_GetFieldInt(r, 2), online = Faction_FindOnline(dbid);
			if (online != INVALID_PLAYER_ID) rank = gFamRank[online];
			if (rank < 1 || rank > FAMILY_RANKS) rank = 1;
			if (online != INVALID_PLAYER_ID)
				format(line, sizeof line, "%s\t%d. %s\t{33AA33}в игре [%d]\n", name, rank, gFamRankName[s][rank - 1], online);
			else
				format(line, sizeof line, "%s\t%d. %s\t{AFAFAF}не в игре\n", name, rank, gFamRankName[s][rank - 1]);
			strcat(body, line);
			gFamListDb[playerid][count++] = dbid;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	new title[64];
	format(title, sizeof title, "Состав семьи (%d)", count);
	ShowPlayerDialog(playerid, DIALOG_FAM_ALL, DIALOG_STYLE_TABLIST_HEADERS, title, "%s",
		FamPerm_IsManager(playerid) ? ("Выбрать") : ("Назад"), "Закрыть", body);
}

// Имя и ранг участника семьи по номеру аккаунта. false — уже не в этой семье
static bool:Family_GetMember(s, dbid, name[], &rank, size = sizeof (name))
{
	name[0] = '\0';
	rank = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT name, famrank FROM accounts WHERE id = %d AND family = %d", dbid, Family[s][famDb]);
	if (r == DBResult:0) return false;
	new bool:ok = (DB_GetRowCount(r) > 0);
	if (ok)
	{
		DB_GetFieldString(r, 0, name, size);
		rank = DB_GetFieldInt(r, 1);
	}
	DB_FreeResultSet(r);
	new online = Faction_FindOnline(dbid);
	if (online != INVALID_PLAYER_ID) rank = gFamRank[online];
	if (rank < 1 || rank > FAMILY_RANKS) rank = 1;
	return ok;
}

static FamMenu_Ranks(playerid)
{
	new s = gFam[playerid], body[600], line[96];
	strcat(body, "Ранг\tНазвание\tПрава\n");
	for (new rk = FAMILY_RANKS; rk >= 1; rk--)
	{
		format(line, sizeof line, "%d\t%s\t{AFAFAF}%s\n", rk, gFamRankName[s][rk - 1],
			(rk == FAMILY_RANKS) ? ("глава") : ((rk == FAMILY_RANKS - 1) ? ("заместитель") : ("участник")));
		strcat(body, line);
	}
	ShowPlayerDialog(playerid, DIALOG_FAM_RANKS, DIALOG_STYLE_TABLIST_HEADERS, "Ранги семьи", "%s",
		FamPerm_Has(playerid, FMP_RANKNAMES) ? ("Изменить") : ("Назад"), "Закрыть", body);
}

static FamMenu_Bank(playerid)
{
	new s = gFam[playerid], title[64];
	format(title, sizeof title, "Общак — $%d", Family[s][famBank]);
	ShowPlayerDialog(playerid, DIALOG_FAM_BANK, DIALOG_STYLE_LIST, title,
		"Положить деньги\nСнять деньги {AFAFAF}(по праву)", "Выбрать", "Назад");
}

stock Families_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid < DIALOG_FAM_MENU || dialogid > DIALOG_FAM_LEAVE) return 0;
	if (gFam[playerid] == -1) return 1;     // пока окно было открыто, игрока выгнали
	new s = gFam[playerid], cmd[64];
	if (FamMg_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;

	switch (dialogid)
	{
		case DIALOG_FAM_MENU:
		{
			if (!response) return 1;
			switch (listitem)
			{
				case 0: FamMenu_Info(playerid);
				case 1: FamMenu_Online(playerid, true);
				case 2: FamMenu_All(playerid);
				case 3: FamMenu_Bank(playerid);
				case 4: FamMenu_Ranks(playerid);
				case 5:
				{
					if (!FamPerm_Need(playerid, FMP_INVITE)) return 1;
					ShowPlayerDialog(playerid, DIALOG_FAM_INVITE, DIALOG_STYLE_INPUT, "Пригласить в семью",
						"Введи id игрока. Он должен стоять рядом с тобой.", "Пригласить", "Назад");
				}
				case 6: FamMg_Show(playerid);
				case 7: Cmd_FamHelp(playerid);
				case 8:
				{
					if (gFamRank[playerid] >= FAMILY_RANKS)
						ShowPlayerDialog(playerid, DIALOG_FAM_LEAVE, DIALOG_STYLE_MSGBOX, "Распустить семью",
							"{FF6347}Семья будет удалена навсегда.{FFFFFF}\nВсе участники выйдут из неё, общак ($%d) вернётся тебе.\n\nТочно распустить?",
							"Распустить", "Назад", Family[s][famBank]);
					else
						ShowPlayerDialog(playerid, DIALOG_FAM_LEAVE, DIALOG_STYLE_MSGBOX, "Покинуть семью",
							"Точно выйти из «%s»?\nВернуться можно только по новому приглашению.", "Выйти", "Назад", Family[s][famName]);
				}
			}
		}
		case DIALOG_FAM_BACK:
		{
			FamMenu_Show(playerid);
		}
		case DIALOG_FAM_ONLINE:
		{
			if (response) FamMenu_Show(playerid);
		}
		case DIALOG_FAM_ALL:
		{
			if (!response) return 1;
			if (!FamPerm_IsManager(playerid)) { FamMenu_Show(playerid); return 1; }
			if (listitem < 0 || listitem >= FAM_LIST_MAX || gFamListDb[playerid][listitem] == 0) return 1;
			gFamSelDb[playerid] = gFamListDb[playerid][listitem];
			if (gFamSelDb[playerid] == Player[playerid][pDbId])
			{
				SendClientMessage(playerid, COLOR_GREY, "Это ты. Выйти из семьи — в меню /family.");
				FamMenu_All(playerid);
				return 1;
			}
			FamMenu_MemberActions(playerid);
		}
		case DIALOG_FAM_ACT:
		{
			if (!response) { FamMenu_All(playerid); return 1; }
			FamMg_OnCard(playerid, listitem);
		}
		case DIALOG_FAM_RANKSET:
		{
			if (!response) { FamMenu_MemberActions(playerid); return 1; }
			new rank = strval(inputtext), name[MAX_PLAYER_NAME], cur;
			if (!FamMg_CanAct(playerid, FMP_RANK, name, cur)) return 1;
			if (rank < 1 || rank >= gFamRank[playerid])
			{
				SendClientMessage(playerid, COLOR_GREY, "Ранг должен быть от 1 до %d.", gFamRank[playerid] - 1);
				FamMenu_MemberActions(playerid);
				return 1;
			}
			new online = Faction_FindOnline(gFamSelDb[playerid]);
			if (online != INVALID_PLAYER_ID)
			{
				format(cmd, sizeof cmd, "%d %d", online, rank);
				Cmd_FamRank(playerid, cmd);
			}
			else
			{
				DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET famrank = %d WHERE id = %d AND family = %d", rank, gFamSelDb[playerid], Family[s][famDb]));
				new msg[144];
				format(msg, sizeof msg, "[%s] %s назначил %s: «%s» (%d).", Family[s][famType], GetRPName(playerid), name, gFamRankName[s][rank - 1], rank);
				Fam_Announce(s, FLOG_STAFF, msg);
			}
			FamMenu_All(playerid);
		}
		case DIALOG_FAM_BANK:
		{
			if (!response) { FamMenu_Show(playerid); return 1; }
			if (listitem == 0)
				ShowPlayerDialog(playerid, DIALOG_FAM_BANK_IN, DIALOG_STYLE_INPUT, "Положить в общак",
					"Сколько положить в общак?\nУ тебя: $%d", "Положить", "Назад", GetMoney(playerid));
			else
			{
				if (!FamPerm_Need(playerid, FMP_BANK_OUT)) { FamMenu_Bank(playerid); return 1; }
				ShowPlayerDialog(playerid, DIALOG_FAM_BANK_OUT, DIALOG_STYLE_INPUT, "Снять из общака",
					"Сколько снять?\nВ общаке: $%d", "Снять", "Назад", Family[s][famBank]);
			}
		}
		case DIALOG_FAM_BANK_IN, DIALOG_FAM_BANK_OUT:
		{
			if (!response) { FamMenu_Bank(playerid); return 1; }
			new amount = strval(inputtext);
			if (amount < 1) { SendClientMessage(playerid, COLOR_GREY, "Введи сумму больше нуля."); FamMenu_Bank(playerid); return 1; }
			format(cmd, sizeof cmd, "%s %d", (dialogid == DIALOG_FAM_BANK_IN) ? ("put") : ("take"), amount);
			Cmd_FamBank(playerid, cmd);
			FamMenu_Bank(playerid);
		}
		case DIALOG_FAM_INVITE:
		{
			if (!response) { FamMenu_Show(playerid); return 1; }
			Cmd_FamInvite(playerid, inputtext);
		}
		case DIALOG_FAM_RANKS:
		{
			if (!response) return 1;
			if (!FamPerm_Has(playerid, FMP_RANKNAMES)) { FamMenu_Show(playerid); return 1; }
			if (listitem < 0 || listitem >= FAMILY_RANKS) return 1;
			gFamSelRank[playerid] = FAMILY_RANKS - listitem;
			ShowPlayerDialog(playerid, DIALOG_FAM_RANKNAME, DIALOG_STYLE_INPUT, "Название ранга",
				"Новое название для ранга %d.\nСейчас: %s", "Сохранить", "Назад", gFamSelRank[playerid], gFamRankName[s][gFamSelRank[playerid] - 1]);
		}
		case DIALOG_FAM_RANKNAME:
		{
			if (response && inputtext[0] != '\0')
			{
				format(cmd, sizeof cmd, "%d %s", gFamSelRank[playerid], inputtext);
				Cmd_FamRankName(playerid, cmd);
			}
			FamMenu_Ranks(playerid);
		}
		case DIALOG_FAM_LEAVE:
		{
			if (!response) { FamMenu_Show(playerid); return 1; }
			if (gFamRank[playerid] >= FAMILY_RANKS) Cmd_FamDisband(playerid, "да");
			else Cmd_FamLeave(playerid);
		}
	}
	return 1;
}

// =====================================================================
//  Управление семьёй: права рангов и личные права, карточка участника,
//  выговоры, журнал с фильтром. Окно: /family > Управление.
//
//  Как во фракциях (fmanage.inc): глава может всё, у рангов 1..5 свой
//  набор прав (family_perms), зам (ранг 5) по умолчанию может всё,
//  глава ему отключает. Поверх ранга — личные права
//  (accounts.famperm_on / famperm_off). Права меняют только глава и зам;
//  зам — рангам 1..4 и людям младше себя, и только те права, что есть у него.
//  Только у главы: цвет, семейный дом (/famhouse), передать главенство, распустить.
// =====================================================================

static stock FamPerm_Default(rank)
{
	if (rank >= FAMILY_RANKS - 1) return FMP_ALL;
	return (1 << FMP_BANK_IN) | (1 << FMP_HOUSE);
}

static FamPerm_ResetSlot(s)
{
	for (new rk = 1; rk <= FAMILY_RANKS; rk++) gFamRankPerm[s][rk - 1] = FamPerm_Default(rk);
}

static FamPerm_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS family_perms (family INTEGER NOT NULL, rank INTEGER NOT NULL, mask INTEGER DEFAULT 0, PRIMARY KEY (family, rank))"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS family_logs (id INTEGER PRIMARY KEY AUTOINCREMENT, family INTEGER NOT NULL, date INTEGER DEFAULT 0, kind INTEGER DEFAULT 0, text TEXT)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE INDEX IF NOT EXISTS family_logs_f ON family_logs (family, id)"));
	new DBResult:r;
	DB_AddColumn("accounts", "famperm_on", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "famperm_off", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "famwarns", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "famjoin", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "famnote", "TEXT");
	DB_AddColumn("accounts", "fammute_until", "INTEGER DEFAULT 0");
	DB_AddColumn("families", "disabled", "INTEGER DEFAULT 0");
	for (new s = 0; s < MAX_FAMILIES; s++) { FamPerm_ResetSlot(s); gFamDisabled[s] = 0; }
	r = DB_ExecuteQuery(gDB, "SELECT id, disabled FROM families");
	if (r != DBResult:0)
	{
		for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
		{
			new s = Family_FindByDb(DB_GetFieldInt(r, 0));
			if (s != -1) gFamDisabled[s] = DB_GetFieldInt(r, 1) & FMP_ALL;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	r = DB_ExecuteQuery(gDB, "SELECT family, rank, mask FROM family_perms");
	if (r != DBResult:0)
	{
		for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
		{
			new s = Family_FindByDb(DB_GetFieldInt(r, 0)), rk = DB_GetFieldInt(r, 1);
			if (s != -1 && rk >= 1 && rk < FAMILY_RANKS) gFamRankPerm[s][rk - 1] = DB_GetFieldInt(r, 2) & FMP_ALL;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
}

static FamPerm_LoadPlayer(playerid)
{
	gFamPOn[playerid] = 0;
	gFamPOff[playerid] = 0;
	gFamMuteUntil[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT famperm_on, famperm_off, fammute_until FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		gFamPOn[playerid] = DB_GetFieldInt(r, 0) & FMP_ALL;
		gFamPOff[playerid] = DB_GetFieldInt(r, 1) & FMP_ALL;
		gFamMuteUntil[playerid] = DB_GetFieldInt(r, 2);
	}
	DB_FreeResultSet(r);
}

// Вступил в семью или вышел из неё: личные права, выговоры и заметка — с нуля
static FamPerm_OnJoinLeave(playerid, bool:joined)
{
	gFamMuteUntil[playerid] = 0;
	gFamPOn[playerid] = 0;
	gFamPOff[playerid] = 0;
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET famperm_on = 0, famperm_off = 0, famwarns = 0, famnote = NULL, fammute_until = 0, famjoin = %d WHERE id = %d",
		joined ? gettime() : 0, Player[playerid][pDbId]));
}

static FamPerm_MaskOf(s, rank, on, off)
{
	if (s < 0 || s >= MAX_FAMILIES || rank < 1) return 0;
	if (rank >= FAMILY_RANKS) return FMP_ALL & ~gFamDisabled[s];
	return (gFamRankPerm[s][rank - 1] | on) & ~off & ~gFamDisabled[s] & FMP_ALL;
}

static FamPerm_Mask(playerid)
{
	if (gFam[playerid] == -1) return 0;
	return FamPerm_MaskOf(gFam[playerid], gFamRank[playerid], gFamPOn[playerid], gFamPOff[playerid]);
}

static FamPerm_Has(playerid, p)
{
	return (FamPerm_Mask(playerid) >> p) & 1;
}

static FamPerm_Need(playerid, p)
{
	if (gFam[playerid] == -1)
	{
		SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь в семье. Создать свою: /famcreate");
		return 0;
	}
	if (FamPerm_Has(playerid, p)) return 1;
	SendClientMessage(playerid, COLOR_GREY, "У тебя нет права «%s». Права выдают глава и зам: /family > Управление.", gFamPermName[p]);
	return 0;
}

static FamPerm_IsManager(playerid)
{
	if (gFam[playerid] == -1) return 0;
	if (gFamRank[playerid] >= FAMILY_RANKS - 1) return 1;
	return (FamPerm_Mask(playerid) & ~((1 << FMP_BANK_IN) | (1 << FMP_HOUSE))) != 0;
}

static FamPerm_CanEdit(playerid)
{
	return (gFam[playerid] != -1 && gFamRank[playerid] >= FAMILY_RANKS - 1);
}

static FamPerm_Count(m)
{
	new n = 0;
	for (new p = 0; p < FMP_COUNT; p++) if ((m >> p) & 1) n++;
	return n;
}

static Fam_Log(s, kind, const text[])
{
	if (s < 0 || Family[s][famDb] == 0) return;
	new day, month, year, hour, minute, second, line[160];
	getdate(year, month, day);
	gettime(hour, minute, second);
	format(line, sizeof line, "%02d.%02d %02d:%02d  %s", day, month, hour, minute, text);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO family_logs (family, date, kind, text) VALUES (%d, %d, %d, '%q')", Family[s][famDb], gettime(), kind, line));
}

// Сообщение семье и запись в журнал (текст сообщения начинается с «[Тип] »)
static Fam_Announce(s, kind, const msg[])
{
	Family_Message(s, msg);
	new p = strfind(msg, "] ");
	if (p != -1) Fam_Log(s, kind, msg[p + 2]);
	else Fam_Log(s, kind, msg);
}

// ---------------------------------------------------------------------
//  Окно «Управление»
// ---------------------------------------------------------------------
static FamMg_Show(playerid)
{
	if (!FamPerm_IsManager(playerid))
	{
		SendClientMessage(playerid, COLOR_GREY, "Управление доступно главе, заму и тем, кому они выдали права.");
		return;
	}
	new body[600], line[96];
	strcat(body, "Состав и карточки участников\n");
	strcat(body, FamPerm_CanEdit(playerid) ? ("Права рангов\n") : ("{777777}Права рангов (глава и зам)\n"));
	strcat(body, "Журнал\n");
	format(line, sizeof line, "Мои права {AFAFAF}(%d из %d)\n", FamPerm_Count(FamPerm_Mask(playerid)), FMP_COUNT);
	strcat(body, line);
	ShowPlayerDialog(playerid, DIALOG_FAM_MANAGE, DIALOG_STYLE_LIST, "Управление семьёй", "%s", "Выбрать", "Назад", body);
}

static FamMg_MyPerms(playerid)
{
	new text[900], line[80], m = FamPerm_Mask(playerid);
	for (new p = 0; p < FMP_COUNT; p++)
	{
		format(line, sizeof line, "%s%s\n", ((m >> p) & 1) ? ("{33AA33}[+] {FFFFFF}") : ("{777777}[  ] "), gFamPermName[p]);
		strcat(text, line);
	}
	gFamInfoBack[playerid] = 0;
	ShowPlayerDialog(playerid, DIALOG_FAM_INFO, DIALOG_STYLE_MSGBOX, "Мои права в семье", "%s", "Назад", "", text);
}

static FamMg_Ranks(playerid)
{
	new s = gFam[playerid], body[600], line[96];
	strcat(body, "Ранг\tПрав\n");
	for (new rk = 1; rk < gFamRank[playerid] && rk < FAMILY_RANKS; rk++)
	{
		format(line, sizeof line, "%d. %s\t%d из %d\n", rk, gFamRankName[s][rk - 1], FamPerm_Count(gFamRankPerm[s][rk - 1]), FMP_COUNT);
		strcat(body, line);
	}
	ShowPlayerDialog(playerid, DIALOG_FAM_PRANKS, DIALOG_STYLE_TABLIST_HEADERS, "Права рангов", "%s", "Открыть", "Назад", body);
}

static FamMg_RankPerms(playerid)
{
	new s = gFam[playerid], rk = gFamSelRank[playerid], body[1000], line[112], my = FamPerm_Mask(playerid);
	strcat(body, "Право\tРангу\n");
	for (new p = 0; p < FMP_COUNT; p++)
	{
		format(line, sizeof line, "%s%s\t%s\n", ((my >> p) & 1) ? ("{FFFFFF}") : ("{777777}"), gFamPermName[p],
			((gFamRankPerm[s][rk - 1] >> p) & 1) ? ("{33AA33}вкл") : ("{FF6347}выкл"));
		strcat(body, line);
	}
	new title[64];
	format(title, sizeof title, "Права: %d. %s", rk, gFamRankName[s][rk - 1]);
	ShowPlayerDialog(playerid, DIALOG_FAM_PRANK, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Вкл/выкл", "Назад", body);
}

// Карточка участника: всё, что можно сделать с выбранным человеком
static FamMenu_MemberActions(playerid)
{
	new s = gFam[playerid], name[MAX_PLAYER_NAME], rank;
	if (!Family_GetMember(s, gFamSelDb[playerid], name, rank)) { FamMenu_All(playerid); return; }
	new online = Faction_FindOnline(gFamSelDb[playerid]);
	new my = FamPerm_Mask(playerid), bool:younger = (rank < gFamRank[playerid]);
	new title[64], body[600], line[96];
	if (online != INVALID_PLAYER_ID) format(title, sizeof title, "%s [%d] — %s", name, online, gFamRankName[s][rank - 1]);
	else format(title, sizeof title, "%s — %s (не в игре)", name, gFamRankName[s][rank - 1]);
	strcat(body, "Информация\n");
	format(line, sizeof line, "%sИзменить ранг\n", (younger && ((my >> FMP_RANK) & 1)) ? ("") : ("{777777}"));
	strcat(body, line);
	format(line, sizeof line, "%sВыдать выговор {AFAFAF}(%d/%d)\n", (younger && ((my >> FMP_WARN) & 1)) ? ("") : ("{777777}"), FamMg_GetWarns(gFamSelDb[playerid]), FAMILY_MAX_WARNS);
	strcat(body, line);
	format(line, sizeof line, "%sСнять выговор\n", (younger && ((my >> FMP_WARN) & 1)) ? ("") : ("{777777}"));
	strcat(body, line);
	format(line, sizeof line, "%sЛичные права\n", (younger && FamPerm_CanEdit(playerid)) ? ("") : ("{777777}"));
	strcat(body, line);
	format(line, sizeof line, "%sЗаметка\n", FamPerm_CanEdit(playerid) ? ("") : ("{777777}"));
	strcat(body, line);
	format(line, sizeof line, "%sВыгнать из семьи\n", (younger && ((my >> FMP_KICK) & 1)) ? ("{FF6347}") : ("{777777}"));
	strcat(body, line);
	strcat(body, (gFamRank[playerid] >= FAMILY_RANKS) ? ("Передать главенство") : ("{777777}Передать главенство (глава)"));
	ShowPlayerDialog(playerid, DIALOG_FAM_ACT, DIALOG_STYLE_LIST, title, "%s", "Выбрать", "Назад", body);
}

static FamMg_GetWarns(dbid)
{
	new w = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT famwarns FROM accounts WHERE id = %d", dbid);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) w = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return w;
}

static FamMg_LoadMemberPerms(playerid, dbid)
{
	gFamMgOn[playerid] = 0;
	gFamMgOff[playerid] = 0;
	new online = Faction_FindOnline(dbid);
	if (online != INVALID_PLAYER_ID) { gFamMgOn[playerid] = gFamPOn[online]; gFamMgOff[playerid] = gFamPOff[online]; return; }
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT famperm_on, famperm_off FROM accounts WHERE id = %d", dbid);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		gFamMgOn[playerid] = DB_GetFieldInt(r, 0) & FMP_ALL;
		gFamMgOff[playerid] = DB_GetFieldInt(r, 1) & FMP_ALL;
	}
	DB_FreeResultSet(r);
}

static FamMg_Ago(ts, dest[], size)
{
	if (ts <= 0) { format(dest, size, "неизвестно"); return; }
	new diff = gettime() - ts;
	if (diff < 0) diff = 0;
	if (diff < 3600) format(dest, size, "%d мин. назад", diff / 60);
	else if (diff < 86400) format(dest, size, "%d ч. назад", diff / 3600);
	else format(dest, size, "%d дн. назад", diff / 86400);
}

static FamMg_CardInfo(playerid)
{
	new s = gFam[playerid], name[MAX_PLAYER_NAME], rank, dbid = gFamSelDb[playerid];
	if (!Family_GetMember(s, dbid, name, rank)) return;
	new joined = 0, seen = 0, warns = 0, note[64];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT famjoin, last_seen, famwarns, famnote FROM accounts WHERE id = %d", dbid);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			joined = DB_GetFieldInt(r, 0);
			seen = DB_GetFieldInt(r, 1);
			warns = DB_GetFieldInt(r, 2);
			DB_GetFieldString(r, 3, note, sizeof note);
		}
		DB_FreeResultSet(r);
	}
	FamMg_LoadMemberPerms(playerid, dbid);
	new online = Faction_FindOnline(dbid), text[1400], line[128], d[24];
	format(line, sizeof line, "{AFAFAF}Имя: {FFFFFF}%s\n{AFAFAF}Ранг: {FFFFFF}%d. %s\n", name, rank, gFamRankName[s][rank - 1]);
	strcat(text, line);
	FamMg_Ago(joined, d, sizeof d);
	format(line, sizeof line, "{AFAFAF}Вступил: {FFFFFF}%s\n", d);
	strcat(text, line);
	if (online != INVALID_PLAYER_ID) format(line, sizeof line, "{AFAFAF}Статус: {33AA33}в игре [%d]\n", online);
	else { FamMg_Ago(seen, d, sizeof d); format(line, sizeof line, "{AFAFAF}Последний вход: {FFFFFF}%s\n", d); }
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Выговоры: {FFFFFF}%d / %d\n{AFAFAF}Заметка: {FFFFFF}%s\n", warns, FAMILY_MAX_WARNS, (note[0] != '\0') ? note : ("нет"));
	strcat(text, line);
	new m = FamPerm_MaskOf(s, rank, gFamMgOn[playerid], gFamMgOff[playerid]);
	format(line, sizeof line, "\n{AFAFAF}Права (%d из %d):\n", FamPerm_Count(m), FMP_COUNT);
	strcat(text, line);
	for (new p = 0; p < FMP_COUNT; p++)
	{
		if (!((m >> p) & 1)) continue;
		format(line, sizeof line, "{FFFFFF}• %s%s\n", gFamPermName[p], (((gFamMgOn[playerid] >> p) & 1) && rank < FAMILY_RANKS) ? (" {AFAFAF}(лично)") : (""));
		strcat(text, line);
	}
	gFamInfoBack[playerid] = 1;
	ShowPlayerDialog(playerid, DIALOG_FAM_INFO, DIALOG_STYLE_MSGBOX, "Карточка участника", "%s", "Назад", "", text);
}

static FamMg_Personal(playerid)
{
	new s = gFam[playerid], rank = gFamSelRank[playerid], body[1400], line[128], st[48];
	strcat(body, "Право\tСостояние\n");
	for (new p = 0; p < FMP_COUNT; p++)
	{
		if ((gFamMgOn[playerid] >> p) & 1) format(st, sizeof st, "{33AA33}вкл лично");
		else if ((gFamMgOff[playerid] >> p) & 1) format(st, sizeof st, "{FF6347}выкл лично");
		else format(st, sizeof st, "{AFAFAF}как у ранга: %s", ((gFamRankPerm[s][rank - 1] >> p) & 1) ? ("вкл") : ("выкл"));
		format(line, sizeof line, "%s%s\t%s\n", FamPerm_Has(playerid, p) ? ("{FFFFFF}") : ("{777777}"), gFamPermName[p], st);
		strcat(body, line);
	}
	ShowPlayerDialog(playerid, DIALOG_FAM_PPERMS, DIALOG_STYLE_TABLIST_HEADERS, "Личные права (вкл > выкл > как у ранга)", "%s", "Сменить", "Назад", body);
}

// Можно ли действовать с выбранным участником: есть право и он младше
static FamMg_CanAct(playerid, p, name[], &rank, size = sizeof (name))
{
	if (!FamPerm_Need(playerid, p)) return 0;
	if (!Family_GetMember(gFam[playerid], gFamSelDb[playerid], name, rank, size)) { SendClientMessage(playerid, COLOR_GREY, "Этот человек уже не в семье."); return 0; }
	if (gFamSelDb[playerid] == Player[playerid][pDbId] || rank >= gFamRank[playerid])
	{
		SendClientMessage(playerid, COLOR_GREY, "Нельзя управлять равным или старшим по рангу.");
		return 0;
	}
	return 1;
}

static FamMg_Kick(playerid, dbid, const name[], const why[])
{
	new s = gFam[playerid], online = Faction_FindOnline(dbid), msg[144];
	if (online != INVALID_PLAYER_ID)
	{
		gFam[online] = -1;
		gFamRank[online] = 0;
		Family_SaveMember(online);
		FamPerm_OnJoinLeave(online, false);
		SendClientMessage(online, COLOR_RED, "Тебя выгнали из «%s». Причина: %s", Family[s][famName], why);
	}
	else DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET family = 0, famrank = 0, famperm_on = 0, famperm_off = 0, famwarns = 0, famnote = NULL, famjoin = 0 WHERE id = %d", dbid));
	format(msg, sizeof msg, "[%s] %s выгнал %s. Причина: %s", Family[s][famType], GetRPName(playerid), name, why);
	Fam_Announce(s, FLOG_STAFF, msg);
}

static FamMg_OnCard(playerid, listitem)
{
	new s = gFam[playerid], name[MAX_PLAYER_NAME], rank, msg[144];
	switch (listitem)
	{
		case 0: FamMg_CardInfo(playerid);
		case 1:
		{
			if (!FamMg_CanAct(playerid, FMP_RANK, name, rank)) { FamMenu_MemberActions(playerid); return; }
			ShowPlayerDialog(playerid, DIALOG_FAM_RANKSET, DIALOG_STYLE_INPUT, "Изменить ранг",
				"Новый ранг для %s (1-%d).\nСейчас: %d. %s", "Сохранить", "Назад", name, gFamRank[playerid] - 1, rank, gFamRankName[s][rank - 1]);
		}
		case 2:
		{
			if (!FamMg_CanAct(playerid, FMP_WARN, name, rank)) { FamMenu_MemberActions(playerid); return; }
			ShowPlayerDialog(playerid, DIALOG_FAM_WARN, DIALOG_STYLE_INPUT, "Выговор",
				"Выговор для %s. Напиши причину.\nНа %d-м выговоре человека выгонят из семьи.", "Выдать", "Назад", name, FAMILY_MAX_WARNS);
		}
		case 3:
		{
			if (!FamMg_CanAct(playerid, FMP_WARN, name, rank)) { FamMenu_MemberActions(playerid); return; }
			new w = FamMg_GetWarns(gFamSelDb[playerid]);
			if (w <= 0) { SendClientMessage(playerid, COLOR_GREY, "У него нет выговоров."); FamMenu_MemberActions(playerid); return; }
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET famwarns = %d WHERE id = %d", w - 1, gFamSelDb[playerid]));
			format(msg, sizeof msg, "%s снял выговор с %s (%d/%d).", GetRPName(playerid), name, w - 1, FAMILY_MAX_WARNS);
			Fam_Log(s, FLOG_WARN, msg);
			SendClientMessage(playerid, COLOR_GREEN, "Выговор снят (%d/%d).", w - 1, FAMILY_MAX_WARNS);
			new t = Faction_FindOnline(gFamSelDb[playerid]);
			if (t != INVALID_PLAYER_ID) SendClientMessage(t, COLOR_GREEN, "%s снял с тебя выговор в семье (%d/%d).", GetRPName(playerid), w - 1, FAMILY_MAX_WARNS);
			FamMenu_MemberActions(playerid);
		}
		case 4:
		{
			if (!FamPerm_CanEdit(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Личные права выдают только глава и зам."); FamMenu_MemberActions(playerid); return; }
			if (!Family_GetMember(s, gFamSelDb[playerid], name, rank) || rank >= gFamRank[playerid])
			{
				SendClientMessage(playerid, COLOR_GREY, "Нельзя управлять равным или старшим по рангу.");
				FamMenu_MemberActions(playerid);
				return;
			}
			FamMg_LoadMemberPerms(playerid, gFamSelDb[playerid]);
			gFamSelRank[playerid] = rank;
			FamMg_Personal(playerid);
		}
		case 5:
		{
			if (!FamPerm_CanEdit(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Заметки пишут только глава и зам."); FamMenu_MemberActions(playerid); return; }
			if (!Family_GetMember(s, gFamSelDb[playerid], name, rank)) return;
			ShowPlayerDialog(playerid, DIALOG_FAM_NOTE, DIALOG_STYLE_INPUT, "Заметка",
				"Заметка о %s (видят глава и зам, до 60 символов).\nЧтобы удалить, введи «-».", "Сохранить", "Назад", name);
		}
		case 6:
		{
			if (!FamMg_CanAct(playerid, FMP_KICK, name, rank)) { FamMenu_MemberActions(playerid); return; }
			FamMg_Kick(playerid, gFamSelDb[playerid], name, "решение руководства");
			FamMenu_All(playerid);
		}
		case 7:
		{
			if (gFamRank[playerid] < FAMILY_RANKS) { SendClientMessage(playerid, COLOR_GREY, "Передать семью может только глава."); FamMenu_MemberActions(playerid); return; }
			new online = Faction_FindOnline(gFamSelDb[playerid]), cmd[16];
			if (online == INVALID_PLAYER_ID) { SendClientMessage(playerid, COLOR_GREY, "Передать семью можно только тому, кто сейчас в игре."); return; }
			format(cmd, sizeof cmd, "%d", online);
			Cmd_FamLeader(playerid, cmd);
		}
	}
}

static FamMg_ShowLogs(playerid)
{
	new s = gFam[playerid], body[3600], line[160];
	new DBResult:r;
	if (gFamLogKind[playerid] == -1)
		r = DB_ExecuteQuery(gDB, "SELECT text FROM family_logs WHERE family = %d AND text LIKE '%%%q%%' ORDER BY id DESC LIMIT 25", Family[s][famDb], gFamLogName[playerid]);
	else if (gFamLogKind[playerid] > 0)
		r = DB_ExecuteQuery(gDB, "SELECT text FROM family_logs WHERE family = %d AND kind = %d ORDER BY id DESC LIMIT 25", Family[s][famDb], gFamLogKind[playerid]);
	else
		r = DB_ExecuteQuery(gDB, "SELECT text FROM family_logs WHERE family = %d ORDER BY id DESC LIMIT 25", Family[s][famDb]);
	if (r != DBResult:0)
	{
		for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
		{
			DB_GetFieldString(r, 0, line, sizeof line);
			strcat(body, line);
			strcat(body, "\n");
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	if (body[0] == '\0') strcat(body, "Записей нет.");
	ShowPlayerDialog(playerid, DIALOG_FAM_LOG, DIALOG_STYLE_MSGBOX, "Журнал семьи", "%s", "Назад", "", body);
}

static FamMg_LogMenu(playerid)
{
	ShowPlayerDialog(playerid, DIALOG_FAM_LOGMENU, DIALOG_STYLE_LIST, "Журнал семьи",
		"Все записи\nКадры: приём, выход, ранги\nОбщак\nВыговоры\nПрава\nПо игроку (ввести ник)", "Показать", "Назад");
}

// Ответы окон «Управления» (из Families_OnDialog). 1 — обработано.
static FamMg_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	new s = gFam[playerid], msg[144];
	switch (dialogid)
	{
		case DIALOG_FAM_MANAGE:
		{
			if (!response) { FamMenu_Show(playerid); return 1; }
			if (!FamPerm_IsManager(playerid)) return 1;
			switch (listitem)
			{
				case 0: FamMenu_All(playerid);
				case 1:
				{
					if (!FamPerm_CanEdit(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Права рангов меняют только глава и зам."); FamMg_Show(playerid); return 1; }
					FamMg_Ranks(playerid);
				}
				case 2:
				{
					if (!FamPerm_Need(playerid, FMP_LOG)) { FamMg_Show(playerid); return 1; }
					FamMg_LogMenu(playerid);
				}
				case 3: FamMg_MyPerms(playerid);
			}
		}
		case DIALOG_FAM_INFO:
		{
			if (gFamInfoBack[playerid] == 1) FamMenu_MemberActions(playerid);
			else FamMg_Show(playerid);
		}
		case DIALOG_FAM_PRANKS:
		{
			if (!response) { FamMg_Show(playerid); return 1; }
			new rk = listitem + 1;
			if (!FamPerm_CanEdit(playerid) || rk < 1 || rk >= gFamRank[playerid] || rk >= FAMILY_RANKS) return 1;
			gFamSelRank[playerid] = rk;
			FamMg_RankPerms(playerid);
		}
		case DIALOG_FAM_PRANK:
		{
			if (!response) { FamMg_Ranks(playerid); return 1; }
			new rk = gFamSelRank[playerid];
			if (!FamPerm_CanEdit(playerid) || listitem < 0 || listitem >= FMP_COUNT || rk < 1 || rk >= gFamRank[playerid] || rk >= FAMILY_RANKS) return 1;
			if (!FamPerm_Has(playerid, listitem))
				SendClientMessage(playerid, COLOR_GREY, "Нельзя менять право, которого нет у тебя самого: «%s».", gFamPermName[listitem]);
			else
			{
				gFamRankPerm[s][rk - 1] ^= (1 << listitem);
				DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO family_perms (family, rank, mask) VALUES (%d, %d, %d)", Family[s][famDb], rk, gFamRankPerm[s][rk - 1]));
				format(msg, sizeof msg, "%s %s рангу %d право «%s».", GetRPName(playerid), ((gFamRankPerm[s][rk - 1] >> listitem) & 1) ? ("выдал") : ("забрал у"), rk, gFamPermName[listitem]);
				Fam_Log(s, FLOG_PERM, msg);
			}
			FamMg_RankPerms(playerid);
		}
		case DIALOG_FAM_PPERMS:
		{
			if (!response) { FamMenu_MemberActions(playerid); return 1; }
			new name[MAX_PLAYER_NAME], rank, dbid = gFamSelDb[playerid];
			if (!FamPerm_CanEdit(playerid) || listitem < 0 || listitem >= FMP_COUNT) return 1;
			if (!Family_GetMember(s, dbid, name, rank) || rank >= gFamRank[playerid] || dbid == Player[playerid][pDbId]) return 1;
			if (!FamPerm_Has(playerid, listitem))
			{
				SendClientMessage(playerid, COLOR_GREY, "Нельзя менять право, которого нет у тебя самого: «%s».", gFamPermName[listitem]);
				FamMg_Personal(playerid);
				return 1;
			}
			FamMg_LoadMemberPerms(playerid, dbid);
			new bit = (1 << listitem), what[24];
			if (gFamMgOn[playerid] & bit) { gFamMgOn[playerid] &= ~bit; gFamMgOff[playerid] |= bit; format(what, sizeof what, "выкл"); }
			else if (gFamMgOff[playerid] & bit) { gFamMgOff[playerid] &= ~bit; format(what, sizeof what, "как у ранга"); }
			else { gFamMgOn[playerid] |= bit; format(what, sizeof what, "вкл"); }
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET famperm_on = %d, famperm_off = %d WHERE id = %d", gFamMgOn[playerid], gFamMgOff[playerid], dbid));
			new online = Faction_FindOnline(dbid);
			if (online != INVALID_PLAYER_ID)
			{
				gFamPOn[online] = gFamMgOn[playerid];
				gFamPOff[online] = gFamMgOff[playerid];
				SendClientMessage(online, Family[s][famColor], "%s изменил твоё право в семье «%s»: %s.", GetRPName(playerid), gFamPermName[listitem], what);
			}
			format(msg, sizeof msg, "%s: право «%s» для %s — %s.", GetRPName(playerid), gFamPermName[listitem], name, what);
			Fam_Log(s, FLOG_PERM, msg);
			FamMg_Personal(playerid);
		}
		case DIALOG_FAM_WARN:
		{
			if (!response) { FamMenu_MemberActions(playerid); return 1; }
			new name[MAX_PLAYER_NAME], rank, reason[64];
			if (!FamMg_CanAct(playerid, FMP_WARN, name, rank)) return 1;
			if (!RestParam(inputtext, 0, reason) || !Faction_IsGoodName(reason, 2, 60))
			{
				SendClientMessage(playerid, COLOR_GREY, "Напиши причину: от 2 до 60 символов, без фигурных скобок и знака процента.");
				FamMenu_MemberActions(playerid);
				return 1;
			}
			new w = FamMg_GetWarns(gFamSelDb[playerid]) + 1;
			if (w >= FAMILY_MAX_WARNS)
			{
				new why[96];
				format(why, sizeof why, "%d/%d выговоров (последний: %s)", w, FAMILY_MAX_WARNS, reason);
				FamMg_Kick(playerid, gFamSelDb[playerid], name, why);
				FamMenu_All(playerid);
				return 1;
			}
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET famwarns = %d WHERE id = %d", w, gFamSelDb[playerid]));
			format(msg, sizeof msg, "%s выдал выговор %s (%d/%d). Причина: %s", GetRPName(playerid), name, w, FAMILY_MAX_WARNS, reason);
			Fam_Log(s, FLOG_WARN, msg);
			SendClientMessage(playerid, COLOR_WHITE, "Выговор выдан (%d/%d).", w, FAMILY_MAX_WARNS);
			new t = Faction_FindOnline(gFamSelDb[playerid]);
			if (t != INVALID_PLAYER_ID) SendClientMessage(t, COLOR_RED, "%s выдал тебе выговор в семье (%d/%d). Причина: %s", GetRPName(playerid), w, FAMILY_MAX_WARNS, reason);
			FamMenu_MemberActions(playerid);
		}
		case DIALOG_FAM_NOTE:
		{
			if (response && FamPerm_CanEdit(playerid))
			{
				new name[MAX_PLAYER_NAME], rank, note[64];
				if (!Family_GetMember(s, gFamSelDb[playerid], name, rank)) return 1;
				if (inputtext[0] == '\0' || !strcmp(inputtext, "-"))
				{
					DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET famnote = NULL WHERE id = %d", gFamSelDb[playerid]));
					SendClientMessage(playerid, COLOR_GREEN, "Заметка удалена.");
				}
				else if (!RestParam(inputtext, 0, note) || !Faction_IsGoodName(note, 1, 60))
					SendClientMessage(playerid, COLOR_GREY, "Заметка: до 60 символов, без фигурных скобок и знака процента.");
				else
				{
					DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET famnote = '%q' WHERE id = %d", note, gFamSelDb[playerid]));
					SendClientMessage(playerid, COLOR_GREEN, "Заметка о %s сохранена.", name);
				}
			}
			FamMenu_MemberActions(playerid);
		}
		case DIALOG_FAM_LOGMENU:
		{
			if (!response) { FamMg_Show(playerid); return 1; }
			if (!FamPerm_Need(playerid, FMP_LOG)) return 1;
			if (listitem == 5)
			{
				ShowPlayerDialog(playerid, DIALOG_FAM_LOGNAME, DIALOG_STYLE_INPUT, "Журнал по игроку", "Ник игрока (или его часть):", "Показать", "Назад");
				return 1;
			}
			gFamLogKind[playerid] = listitem;
			FamMg_ShowLogs(playerid);
		}
		case DIALOG_FAM_LOGNAME:
		{
			if (!response) { FamMg_LogMenu(playerid); return 1; }
			new name[MAX_PLAYER_NAME];
			if (!RestParam(inputtext, 0, name) || !Faction_IsGoodName(name, 2, MAX_PLAYER_NAME - 1))
			{
				SendClientMessage(playerid, COLOR_GREY, "Введи ник или его часть (от 2 символов).");
				FamMg_LogMenu(playerid);
				return 1;
			}
			gFamLogName[playerid][0] = '\0';
			strcat(gFamLogName[playerid], name, MAX_PLAYER_NAME);
			gFamLogKind[playerid] = -1;
			FamMg_ShowLogs(playerid);
		}
		case DIALOG_FAM_LOG:
		{
			FamMg_LogMenu(playerid);
		}
		default: return 0;
	}
	return 1;
}

// ---------------------------------------------------------------------
//  Для панели владельца /orgs (orgs.inc)
// ---------------------------------------------------------------------
stock FamAdm_Count()
{
	new n = 0;
	for (new s = 0; s < MAX_FAMILIES; s++) if (Family[s][famDb] != 0) n++;
	return n;
}

stock FamAdm_SlotCount() { return MAX_FAMILIES; }
stock FamAdm_PermCount() { return FMP_COUNT; }

stock FamAdm_PermName(p)
{
	new t[36];
	if (p >= 0 && p < FMP_COUNT) strcat(t, gFamPermName[p]);
	return t;
}

// Данные слота семьи: 0 — слот пуст
stock FamAdm_Slot(s, name[32], &db, &disabled)
{
	name[0] = '\0';
	if (s < 0 || s >= MAX_FAMILIES || Family[s][famDb] == 0) return 0;
	format(name, 32, "%s", Family[s][famName]);
	db = Family[s][famDb];
	disabled = gFamDisabled[s];
	return 1;
}

stock FamAdm_Toggle(s, p)
{
	if (s < 0 || s >= MAX_FAMILIES || Family[s][famDb] == 0 || p < 0 || p >= FMP_COUNT) return;
	gFamDisabled[s] ^= (1 << p);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE families SET disabled = %d WHERE id = %d", gFamDisabled[s], Family[s][famDb]));
	new msg[144];
	format(msg, sizeof msg, "Администрация %s для всей семьи: «%s».", ((gFamDisabled[s] >> p) & 1) ? ("отключила") : ("включила"), gFamPermName[p]);
	Fam_Log(s, FLOG_PERM, msg);
}
