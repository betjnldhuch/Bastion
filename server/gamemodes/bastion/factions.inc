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
//  Вход в здание фракции: /setfint на улице у двери и ещё раз внутри (раздел в конце файла).
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
//    - «Управление» (/fr > Управление или /lmenu): состав (и оффлайн), карточки
//      участников, права рангов и личные права, ранги, зарплаты, скины, выговоры,
//      общак, склад, журнал с фильтром. Права — fmanage.inc (FP_*).
//  Админы (4+): /fedit [фракция] name|color|bank|mats — название, цвет, общак, материалы.
//
//  Фракции 1..7 постоянные, 8..50 админ создаёт в игре: /createfaction [тип] [название],
//  /delfaction, /flist. Права фракции задаёт её тип (FTYPE_*), а не номер.
//  Завод, склад армии, кражи банд, чёрный рынок, чаты /d и /gd — раздел «Материалы» в конце файла.
//  Оружие на смене не выдаётся: его берут со склада. Госструктурам для закупки на склад
//  нужны материалы, их привозит армия (/supply). Цвет фракции: /fcolor. Всё это — в конце файла.
// =====================================================================

#define FACTION_DUTY_RANGE      3.0     // как близко подойти к раздевалке
#define FACTION_VEH_RESPAWN     300     // через сколько секунд брошенная машина фракции вернётся
#define FACTION_BASE_RANGE      10.0    // как близко к раздевалке работают /fbank и /fstorage

#define FACTION_STATE_BUDGET    15000   // в общак полиции и больницы каждый PayDay
#define FACTION_TURF_BANK       300     // в общак банды за каждый её район каждый PayDay
#define FACTION_START_BANK      30000   // стартовый общак госструктур (только при первом запуске)
#define FACTION_BANK_MAX        100000000
#define FACTION_MAX_WARNS       3       // столько выговоров — и игрок уволен
#define FACTION_FIRE_BAN_DAYS   7       // уволенного не берут ни в одну организацию столько дней (/fleave — без запрета)
#define FACTION_MAX_SALARY      20000   // максимальная зарплата ранга, которую может поставить лидер
// Общак полиции и больницы пополняет штат (FACTION_STATE_BUDGET), это не личные деньги лидера:
#define FACTION_STATE_MAX_SALARY 3000   // потолок зарплаты ранга в госструктуре
#define FACTION_STATE_WITHDRAW  0       // может ли лидер госструктуры снимать общак наличными
#define FACTION_LOG_SHOW        25      // сколько последних записей журнала показывать
#define FACTION_LIST_MAX        40      // сколько членов фракции показывать в /lmenu

forward bool:Cmd_LMenu(playerid);
forward bool:Cmd_Orgs(playerid);
forward bool:Cmd_SWarn(playerid, const params[]);
forward bool:Cmd_UnSWarn(playerid, const params[]);
forward bool:Cmd_FMute(playerid, const params[]);
forward bool:Cmd_DeptAccess(playerid, const params[]);
forward bool:Cmd_FLimit(playerid, const params[]);

// Тип фракции: от него зависят права (а не от номера фракции)
enum
{
	FTYPE_NONE,         // фракции нет (свободный номер)
	FTYPE_LAW,          // силовая структура: права полиции
	FTYPE_MEDIC,        // медики: лечение, вызовы раненых
	FTYPE_GANG,         // банда: районы, войны
	FTYPE_CIVIL,        // гос. организация без особых прав (мэрия, СМИ и т.п.)
	FTYPE_ARMY,         // армия: возит материалы
	FTYPE_PRISON,       // охрана тюрьмы (prison.inc)
	FTYPE_FBI,          // спецслужба: права полиции + спецсредства ФБР (fbi.inc), смена в штатском
	FTYPE_GOV           // правительство: бюджет штата, /gov — объявления, лидер меняет налоги (/settax)
}

// Материалы: армия возит их с базы снабжения на свой склад, а оттуда — госструктурам
#define SUPPLY_REF              200     // ref маркеров точек снабжения (MK_FACTION): 200 + SP_*
#define SUPPLY_RANGE            15.0    // как близко подъехать к заводу или складу
#define SUPPLY_TRUCK_CAP        1000    // сколько материалов входит в одну машину армии
#define SUPPLY_PAY_HAUL         30      // водителю за каждые 100 материалов, привезённых с завода на склад армии
#define SUPPLY_PAY_DELIVER      50      // водителю за каждые 100 материалов, доставленных другой фракции
// Расчёт: смена бойца (бронежилет + Desert Eagle + MP5) = 110 материалов; 10 активных бойцов по два
// комплекта в час тратят около 2200. Один рабочий делает ящик (50) примерно за 40 секунд, то есть
// около 4500 в час: одного-двух рабочих хватает на все госструктуры. Цифры завода — в конце файла.
#define STEAL_MIN_ARMY          2       // склад грабят, только когда в сети столько военных
#define BM_RANGE                4.0     // как близко подойти к торговцу чёрного рынка
#define BM_SKIN                 28      // скин торговца
#define BM_MAT_PRICE            2       // сколько торговец платит за 1 материал (меняется: /bmprice 0)
#define BM_SEED_PRICE           90      // чёрный рынок: семена конопли (у уличного дилера дешевле)
#define BM_WEED_PRICE           50      // чёрный рынок: 1 г марихуаны (продаётся по 5 г)
#define BM_COKE_PRICE           300     // чёрный рынок: 1 г кокаина (всем, не только бандам)
// Точки снабжения: их ставит админ (/setarmystore, /addbm) или двигает в /markers
#define MAX_BM                  10      // сколько торговцев чёрного рынка
#define BM_REF                  100000  // ref маркера торговца: 100000 + номер в базе
#define MAX_SQUADS              5       // взводов во фракции
enum
{
	SP_FACTORY,         // больше не используется: заводы ставятся через /addfactory
	SP_ARMY,            // склад армии
	SP_BM,              // торговец чёрного рынка
	MAX_SPOINTS
}
#define SUPPLY_SRC_BASE         1       // груз взят на базе снабжения
#define SUPPLY_SRC_ARMY         2       // груз взят со склада армии
#define FACTION_MATS_MAX        20000   // сколько материалов вмещает склад фракции
#define ARMY_MATS_MAX           100000  // сколько вмещает склад армии
#define FACTION_START_MATS      2000    // материалы новой госструктуры

// Окна (диалоги) фракций — в dialogs.inc

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
	FITEM_COLT,
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
	"Винтовка",
	"Пистолет 9мм"
};
static const WEAPON:gFItemWeapon[MAX_FITEMS] =
{
	WEAPON_FIST, WEAPON_FIST, WEAPON_DEAGLE, WEAPON_SHOTGUN, WEAPON_MP5, WEAPON_M4, WEAPON_RIFLE, WEAPON_COLT45
};
static const gFItemAmmo[MAX_FITEMS]  = { 0,   0,   35,  20,  150,  150,  15,   60  };
static const gFItemPrice[MAX_FITEMS] = { 500, 300, 600, 800, 1200, 2000, 2500, 400 };   // цена закупки за 1 шт. из общака
static const gFItemMats[MAX_FITEMS]  = { 20,  10,  30,  40,  60,   100,  120,  20  };   // материалов на 1 шт. (только госструктуры)

// Тюремная камера (в здании LSPD, интерьер 6)
#define JAIL_X                  264.0
#define JAIL_Y                  84.3    // середина между двумя камерами: от неё проверяется, на месте ли заключённый
#define JAIL_CELL1_Y            86.6    // камера у входа
#define JAIL_CELL2_Y            82.0    // камера в центре
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
	OFFER_CARRY,        // понести раненого (wounded.inc)
	OFFER_HANDSHAKE,    // пожать руку
	OFFER_BRIBE,        // взятка полицейскому от заключённого (law.inc)
	OFFER_DRUGS,        // купить наркотики с рук (drugs.inc)
	OFFER_MARRY         // предложение руки и сердца (family.inc)
}

// Значения по умолчанию. Потом их меняют лидер (/lmenu) и админ (/fedit) — они хранятся в базе.
static const gFactionDefName[FACTION_BUILTIN][32] =
{
	"Нет",
	"Полиция LSPD",
	"Больница LS (EMS)",
	"Grove Street Families",
	"The Ballas",
	"Varrios Los Aztecas",
	"ФБР",
	"Армия"
};

static const gFactionDefColor[FACTION_BUILTIN] =
{
	COLOR_CIVIL,
	0x3A6EFFFF,     // полиция — синий
	0xFF8080FF,     // медики — розовый
	0x10A010FF,     // Grove — зелёный
	0xB040FFFF,     // Ballas — фиолетовый
	0x00C8F0FF,     // Aztecas — голубой
	0x6C8CD5FF,     // ФБР — серо-синий
	0x6B8E23FF      // армия — оливковый
};

// Скин для каждого ранга (1..10)
static const gFactionDefSkin[FACTION_BUILTIN][MAX_RANK] =
{
	{ 0,   0,   0,   0,   0,   0,   0,   0,   0,   0   },
	{ 280, 280, 281, 282, 282, 283, 283, 288, 288, 265 },   // полиция
	{ 274, 274, 275, 275, 275, 276, 276, 276, 276, 70  },   // медики
	{ 105, 106, 105, 106, 105, 106, 107, 107, 107, 270 },   // Grove
	{ 102, 103, 102, 104, 102, 103, 104, 103, 103, 104 },   // Ballas
	{ 114, 115, 114, 116, 114, 115, 116, 115, 115, 116 },   // Aztecas
	{ 286, 286, 286, 286, 165, 165, 166, 166, 166, 165 },   // ФБР
	{ 287, 287, 287, 287, 287, 287, 287, 287, 287, 287 }    // армия
};

// Названия рангов: 0 — полиция, 1 — медики, 2 — банды, 3 — ФБР, 4 — армия, 5 — гос. организация
static const gDefRankNames[7][MAX_RANK][24] =
{
	{ "Кадет", "Офицер", "Сержант", "Лейтенант", "Ст. лейтенант", "Капитан", "Майор", "Подполковник", "Зам. шефа", "Шеф полиции" },
	{ "Интерн", "Фельдшер", "Врач", "Хирург", "Ст. хирург", "Зав. отделением", "Нач. смены", "Консультант", "Зам. главврача", "Главврач" },
	{ "Новичок", "Боец", "Бывалый", "Старший", "Смотрящий", "Авторитет", "Советник", "Капитан", "Правая рука", "Лидер" },
	{ "Стажёр", "Мл. агент", "Агент", "Ст. агент", "Спец. агент", "Ст. спец. агент", "Нач. отдела", "Инспектор", "Зам. директора", "Директор ФБР" },
	{ "Рядовой", "Ефрейтор", "Сержант", "Ст. сержант", "Прапорщик", "Лейтенант", "Капитан", "Майор", "Полковник", "Генерал" },
	{ "Стажёр", "Мл. сотрудник", "Сотрудник", "Ст. сотрудник", "Специалист", "Вед. специалист", "Нач. отдела", "Советник", "Заместитель", "Руководитель" },
	{ "Курсант", "Мл. надзиратель", "Надзиратель", "Ст. надзиратель", "Нач. смены", "Нач. блока", "Нач. охраны", "Зам. начальника", "Заместитель", "Начальник тюрьмы" }
};

// Базы по умолчанию: спавн (x, y, z, угол) и раздевалка /duty (x, y, z).
// Координаты примерные — их можно поправить в игре через /setfspawn и /setfduty.
static const Float:gFactionDefBase[FACTION_BUILTIN][7] =
{
	{ 0.0, 0.0, 0.0, 0.0,          0.0, 0.0, 0.0 },
	{ 1544.0, -1675.5, 13.6, 90.0,   1553.0, -1675.5, 16.2 },   // LSPD, Pershing Square
	{ HOSPITAL_X, HOSPITAL_Y, HOSPITAL_Z, 90.0,   1172.0, -1323.3, 15.4 },   // All Saints
	{ 2512.0, -1673.0, 13.5, 90.0,   2495.9, -1690.0, 14.8 },   // Grove Street
	{ 2220.0, -1160.0, 25.8, 270.0,  2233.8, -1159.8, 25.9 },   // Jefferson Motel
	{ 1975.0, -2040.0, 13.5, 0.0,    1978.0, -2036.0, 13.5 },   // El Corona
	{ 1481.0, -1771.0, 18.8, 0.0,    1481.0, -1766.5, 18.8 },   // ФБР: у мэрии (переставить: /setfspawn, /setfduty, /setfint)
	{ 2745.0, -2440.0, 13.7, 180.0,  2750.0, -2440.0, 13.7 }    // армия: Ocean Docks, примерно (переставить: /setfspawn 7, /setfduty 7)
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
	fSpawnInt,          // интерьер спавна (0 — улица)
	fSpawnWorld,        // мир спавна
	Float:fDutyX,
	Float:fDutyY,
	Float:fDutyZ,
	fDutyInt,           // интерьер раздевалки (0 — улица)
	fDutyWorld,         // мир раздевалки
	fMk,                // маркер раздевалки (markers.inc)
	bool:fMarker,       // значок раздевалки уже создан
	fName[32],          // название (из базы)
	fColor,             // цвет (из базы)
	fBank,              // общак
	fStoreRank,         // с какого ранга можно брать со склада
	fType,              // тип (FTYPE_*); FTYPE_NONE — номер свободен
	fMats               // материалы на складе (привозит армия)
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

// Точки снабжения (SP_*): завод, склад армии, чёрный рынок. Места по умолчанию примерные.
new Float:gSpX[MAX_SPOINTS] = { 2200.0, 2760.0, 2447.0 };
new Float:gSpY[MAX_SPOINTS] = { -2250.0, -2440.0, -1975.0 };
new Float:gSpZ[MAX_SPOINTS] = { 13.6, 13.7, 13.55 };
new Float:gSpA[MAX_SPOINTS];
new gSpWorld[MAX_SPOINTS];
new gSpInt[MAX_SPOINTS];
new gSpMk[MAX_SPOINTS] = { -1, ... };
new gBMPrice[MAX_FITEMS];           // цены чёрного рынка (0 — не продаётся)
new gBMMatPrice = BM_MAT_PRICE;     // сколько торговец платит за 1 припас
new bool:gStealing[MAX_PLAYERS];    // стоит на складе армии и крадёт материалы
new gStealTotal[MAX_PLAYERS];       // сколько украл за этот заход
new gCrate[MAX_PLAYERS];            // ящик в руках: 0 — нет, 1 — только что сделан на заводе, 2 — взят со склада
new gCrateFc[MAX_PLAYERS];          // на каком заводе сделан ящик
new gWorkStage[MAX_PLAYERS];        // работа на заводе: 0 — нет, 1 — несёт сырьё, 2 — стоит у станка
new gWorkFc[MAX_PLAYERS];
new gCapFc[MAX_PLAYERS] = { -1, ... };  // какой завод захватывает
new gCapLeft[MAX_PLAYERS];
new gCraftSel[MAX_PLAYERS];
new gFColorSel[MAX_PLAYERS];        // какой фракции админ выбирает цвет (/fcolor)

// =====================================================================
//  Помощники
// =====================================================================
// Номер в границах массива (сама фракция может быть ещё не создана)
stock bool:Faction_InRange(f)
{
	return (f > FACTION_NONE && f < MAX_FACTIONS);
}

// Фракция существует
stock bool:Faction_IsValid(f)
{
	return (f > FACTION_NONE && f < MAX_FACTIONS && Faction[f][fType] != FTYPE_NONE);
}

stock bool:Faction_IsGang(f)
{
	return (f > FACTION_NONE && f < MAX_FACTIONS && Faction[f][fType] == FTYPE_GANG);
}

// Силовая структура (объявлена в base.inc)
stock bool:Faction_IsLaw(f)
{
	return (f > FACTION_NONE && f < MAX_FACTIONS && (Faction[f][fType] == FTYPE_LAW || Faction[f][fType] == FTYPE_FBI));
}

// Спецслужба (ФБР и фракции этого типа): маскировка, прослушка, слежка, досье, смена в штатском
stock bool:Faction_IsFbi(f)
{
	return (f > FACTION_NONE && f < MAX_FACTIONS && Faction[f][fType] == FTYPE_FBI);
}

// Правительство
stock bool:Faction_IsGov(f)
{
	return (f > FACTION_NONE && f < MAX_FACTIONS && Faction[f][fType] == FTYPE_GOV);
}

stock bool:Faction_IsMedic(f)
{
	return (f > FACTION_NONE && f < MAX_FACTIONS && Faction[f][fType] == FTYPE_MEDIC);
}

stock bool:Faction_IsArmy(f)
{
	return (f > FACTION_NONE && f < MAX_FACTIONS && Faction[f][fType] == FTYPE_ARMY);
}

// Тип постоянной фракции (1..7)
stock Faction_BuiltinType(f)
{
	switch (f)
	{
		case FACTION_POLICE: return FTYPE_LAW;
		case FACTION_FBI: return FTYPE_FBI;
		case FACTION_EMS: return FTYPE_MEDIC;
		case FACTION_GROVE, FACTION_BALLAS, FACTION_AZTECAS: return FTYPE_GANG;
		case FACTION_ARMY: return FTYPE_ARMY;
	}
	return FTYPE_NONE;
}

// Какой набор названий рангов по умолчанию (строка gDefRankNames)
stock Faction_RankSet(f)
{
	switch (Faction[f][fType])
	{
		case FTYPE_FBI: return 3;
		case FTYPE_GOV: return 5;
		case FTYPE_LAW: return 0;
		case FTYPE_MEDIC: return 1;
		case FTYPE_ARMY: return 4;
		case FTYPE_CIVIL: return 5;
		case FTYPE_PRISON: return 6;
	}
	return 2;
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
	if (f <= FACTION_NONE || f >= MAX_FACTIONS) return false;
	return (Faction[f][fType] != FTYPE_NONE && Faction[f][fType] != FTYPE_GANG);
}

// Потолок зарплаты ранга (у госструктур ниже: их общак — бюджет штата)
stock Faction_MaxSalary(f)
{
	if (Faction_IsState(f)) return FACTION_STATE_MAX_SALARY;
	return FACTION_MAX_SALARY;
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
	if (GetPlayerVirtualWorld(playerid) != Faction[f][fDutyWorld] || GetPlayerInterior(playerid) != Faction[f][fDutyInt]) return false;
	return IsPlayerInRangeOfPoint(playerid, FACTION_BASE_RANGE, Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ]);
}

// ---------------------------------------------------------------------
//  Журнал, общак, склад, выговоры — запись в базу
// ---------------------------------------------------------------------

// Записать строку в журнал фракции (видно в /lmenu > Журнал)
stock Faction_Log(f, const text[], kind = FLOG_OTHER)
{
	if (!Faction_IsValid(f)) return;
	new day, month, year, hour, minute, second, line[160];
	getdate(year, month, day);
	gettime(hour, minute, second);
	format(line, sizeof line, "%02d.%02d %02d:%02d  %s", day, month, hour, minute, text);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO faction_logs (faction, date, text, kind) VALUES (%d, %d, '%q', %d)", f, gettime(), line, kind));
	Log_OnFaction(f, Faction[f][fName], text);
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

stock Faction_SaveMats(f)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE factions SET mats = %d WHERE id = %d", Faction[f][fMats], f));
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

// Запрет на вступление в организации после увольнения (accounts.fban_until)
stock Faction_BanUntil(dbid)
{
	new until = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT fban_until FROM accounts WHERE id = %d", dbid);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) until = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return (until > gettime()) ? until : 0;
}

stock Faction_BanFired(dbid)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET fban_until = %d WHERE id = %d", gettime() + FACTION_FIRE_BAN_DAYS * 86400, dbid));
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

// Команда силовиков: полиция или ФБР на смене
stock bool:Faction_NeedLaw(playerid)
{
	if (!Faction_IsLaw(Player[playerid][pFaction]))
	{
		SendClientMessage(playerid, COLOR_GREY, "Эта команда только для полиции и ФБР.");
		return false;
	}
	if (!Player[playerid][pDuty])
	{
		SendClientMessage(playerid, COLOR_GREY, "Сначала заступи на смену: /duty в раздевалке фракции.");
		return false;
	}
	return true;
}

// Команда медиков: любая фракция типа «медики» на смене
stock bool:Faction_NeedMedic(playerid)
{
	if (!Faction_IsMedic(Player[playerid][pFaction]))
	{
		SendClientMessage(playerid, COLOR_GREY, "Эта команда только для медиков.");
		return false;
	}
	if (!Player[playerid][pDuty])
	{
		SendClientMessage(playerid, COLOR_GREY, "Сначала заступи на смену: /duty в раздевалке фракции.");
		return false;
	}
	return true;
}

// Вызов медикам: всем медикам онлайн
stock Faction_MessageMedic(const message[])
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Faction_IsMedic(Player[i][pFaction]))
			SendClientMessage(i, Faction_GetColor(FACTION_EMS), "%s", message);
	}
}

// Сообщение диспетчера: всем полицейским и агентам ФБР онлайн
stock Faction_MessageLaw(const message[])
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Faction_IsLaw(Player[i][pFaction]))
			SendClientMessage(i, Faction_GetColor(FACTION_POLICE), "%s", message);
	}
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

// Цвет ника и точки на карте: полиция и медики — только на смене, банды — всегда
forward bool:Fbi_IsDisguised(playerid);
forward bool:Faction_IsHidden(f);
stock Faction_UpdateColor(playerid)
{
	new f = Player[playerid][pFaction];
	new color = COLOR_CIVIL;
	if (Faction_IsGang(f) || (Faction_IsValid(f) && Player[playerid][pDuty])) color = Faction_GetColor(f);
	if (Faction_IsHidden(f)) color = COLOR_CIVIL;          // скрытую фракцию по цвету не узнать
	if (Fbi_IsDisguised(playerid)) color = Fbi_DisguiseColor(playerid);  // агент ФБР в маскировке (fbi.inc)
	if (Inv_IsMasked(playerid)) color &= 0xFFFFFF00;    // в маске: точки на карте нет ни у кого (needs.inc)
	SetPlayerColor(playerid, color);
}

// Снять форму и оружие
stock Faction_EndDuty(playerid)
{
	if (!Player[playerid][pDuty]) return;
	Player[playerid][pDuty] = false;
	Fbi_ResetDisguise(playerid);
	ResetPlayerWeapons(playerid);
	SetPlayerSkin(playerid, Player[playerid][pSkin]);
	SetPlayerArmour(playerid, 0.0);
	Faction_UpdateColor(playerid);
}

// Принять игрока во фракцию / выгнать (f = FACTION_NONE)
stock Faction_SetMember(playerid, f, rank)
{
	Faction_EndDuty(playerid);
	if (Player[playerid][pFaction] != f)
	{
		Faction_SetWarns(Player[playerid][pDbId], 0);   // новая фракция — выговоры с нуля
		Squad_Set(playerid, 0);                         // и без взвода
		FPerm_OnFactionChange(playerid, f);             // личные права и заметка — с нуля (fmanage.inc)
	}
	Player[playerid][pFaction] = f;
	Player[playerid][pRank] = (f == FACTION_NONE) ? 0 : rank;
	Faction_UpdateColor(playerid);
	Account_Save(playerid);
	Account_SetNormalSpawn(playerid);
	Markers_SyncPlayer(playerid);       // точки, которые видит только своя фракция
}

stock Faction_Uncuff(playerid)
{
	if (!Player[playerid][pCuffed]) return;
	Player[playerid][pCuffed] = false;
	SetPlayerSpecialAction(playerid, SPECIAL_ACTION_NONE);
	TogglePlayerControllable(playerid, true);
}

// =====================================================================
//  Скрытые фракции: их нет ни в одном списке, точки видят только свои
//  (и главный админ), цвет ника у них всегда гражданский. /fhide [№] — админ 7.
// =====================================================================
#define FACTION_SECRET_ID       (MAX_FACTIONS - 1)  // последний номер: сюда переехала фракция 8
static bool:gFHidden[MAX_FACTIONS];

stock bool:Faction_IsHidden(f)
{
	return (f > FACTION_NONE && f < MAX_FACTIONS && gFHidden[f]);
}

// Чьи точки фракции f: скрытая — только своя (0 — видят все)
static Faction_MarkerOwner(f)
{
	return Faction_IsHidden(f) ? f : 0;
}

static Factions_LoadHidden()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS faction_hidden (id INTEGER PRIMARY KEY, hidden INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR IGNORE INTO faction_hidden (id, hidden) VALUES (%d, 1)", FACTION_SECRET_ID));
	for (new f = 0; f < MAX_FACTIONS; f++) gFHidden[f] = false;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id FROM faction_hidden WHERE hidden = 1");
	if (r == DBResult:0) return;
	for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
	{
		new f = DB_GetFieldInt(r, 0);
		if (f > FACTION_NONE && f < MAX_FACTIONS) gFHidden[f] = true;
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
}

// /fhide [№] — скрыть фракцию или вернуть в списки
stock bool:Cmd_FHide(playerid, const params[])
{
	if (!Admin_Check(playerid, ADMIN_MAX_LEVEL)) return true;
	new pos = 0, f;
	if (!NextIntParam(params, pos, f) || !Faction_IsValid(f))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /fhide [№ фракции] — скрыть из всех списков или вернуть. Номера: /flist");
	gFHidden[f] = !gFHidden[f];
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO faction_hidden (id, hidden) VALUES (%d, %d)", f, gFHidden[f] ? 1 : 0));
	Faction_CreateMarker(f);
	FDoor_Build(f);
	FStore_Build(f);
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pFaction] == f) Faction_UpdateColor(i);
	}
	SendClientMessage(playerid, COLOR_ADMIN, "Фракция %d «%s» теперь %s.", f, Faction_GetName(f), gFHidden[f] ? ("скрыта: её точки видят только свои") : ("видна всем"));
	return true;
}


forward Factions_Migrate();

// Перенос фракции на другой номер. Делается при запуске, до загрузки всего остального
// (из OnGameModeInit сразу после DB_Init), один раз: строка в faction_moves помечается done.
// Первый перенос — 8 -> последний номер, чтобы после 7 шли обычные 8, 9...
stock Factions_RunMoves()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS faction_moves (from_id INTEGER NOT NULL, to_id INTEGER NOT NULL, done INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO faction_moves (from_id, to_id) SELECT 8, %d WHERE NOT EXISTS (SELECT 1 FROM faction_moves WHERE from_id = 8 AND to_id = %d)",
		FACTION_SECRET_ID, FACTION_SECRET_ID));
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT rowid, from_id, to_id FROM faction_moves WHERE done = 0 ORDER BY rowid");
	if (r == DBResult:0) return;
	new rows = DB_GetRowCount(r);
	new ids[8][3], n = 0;
	for (new i = 0; i < rows && n < sizeof ids; i++)
	{
		ids[n][0] = DB_GetFieldInt(r, 0);
		ids[n][1] = DB_GetFieldInt(r, 1);
		ids[n][2] = DB_GetFieldInt(r, 2);
		n++;
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	for (new k = 0; k < n; k++)
	{
		new a = ids[k][1], b = ids[k][2];
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE faction_moves SET done = 1 WHERE rowid = %d", ids[k][0]));
		if (a < FACTION_BUILTIN || b < FACTION_BUILTIN || a >= MAX_FACTIONS || b >= MAX_FACTIONS || a == b) continue;
		new bool:hasA = false, bool:hasB = false;
		r = DB_ExecuteQuery(gDB, "SELECT (SELECT COUNT(*) FROM factions WHERE id = %d), (SELECT COUNT(*) FROM factions WHERE id = %d)", a, b);
		if (r != DBResult:0)
		{
			if (DB_GetRowCount(r) > 0) { hasA = DB_GetFieldInt(r, 0) > 0; hasB = DB_GetFieldInt(r, 1) > 0; }
			DB_FreeResultSet(r);
		}
		if (!hasA || hasB) { printf("[Factions] move %d -> %d skipped (no faction %d or %d is taken).", a, b, a, b); continue; }
		Faction_MoveRows(a, b);
		printf("[Factions] Faction %d moved to number %d.", a, b);
	}
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
		// переехала в другое место или здание: значок пересоздаём, старый забываем
		Marker_Destroy(Faction[f][fMk], true);
		Faction[f][fMarker] = false;
	}
	if (!Faction[f][fMarker])
	{
		Faction[f][fMk] = Marker_Create(MK_FACTION, f, Faction_GetName(f), 1275,
			Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ], Faction[f][fDutyWorld], Faction[f][fDutyInt], GPS_FACTIONS);
		Faction[f][fMarker] = true;
	}
	Marker_SetLabel(Faction[f][fMk], text, Faction_GetColor(f));
	Marker_SetFaction(Faction[f][fMk], Faction_MarkerOwner(f));
}

// Админ перенёс маркер раздевалки (/markers)
stock Faction_OnMarkerMoved(f, Float:x, Float:y, Float:z)
{
	if (f >= BM_REF)        // торговцы чёрного рынка
	{
		BM_OnMarkerMoved(f - BM_REF, x, y, z);
		return;
	}
	if (f >= 300)   // склады организаций и точки заводов
	{
		Factory_OnMarkerMoved(f, x, y, z);
		return;
	}
	if (f >= SUPPLY_REF && f < SUPPLY_REF + MAX_SPOINTS)    // склад армии, чёрный рынок
	{
		new i = f - SUPPLY_REF;
		gSpX[i] = x;
		gSpY[i] = y;
		gSpZ[i] = z;
		gSpA[i] = Marker_GetAngle(gSpMk[i]);
		SPoint_Save(i);
		return;
	}
	if (!Faction_IsValid(f)) return;
	Faction[f][fDutyX] = x;
	Faction[f][fDutyY] = y;
	Faction[f][fDutyZ] = z;
	Faction_SaveBase(f);
}

stock Faction_SpawnVehicle(dbid, f, model, Float:x, Float:y, Float:z, Float:a, c1, c2)
{
	new v = CreateVehicle(model, x, y, z, a, c1, c2, FACTION_VEH_RESPAWN, Faction_IsState(f));
	if (v == INVALID_VEHICLE_ID) return INVALID_VEHICLE_ID;
	Vehicle_Clear(v);
	gVehFaction[v] = f;
	gVehFactionDb[v] = dbid;
	return v;
}

stock Factions_Init()
{
	Factions_CreateTables();
	Factions_LoadHidden();
	Factions_Migrate();

	// Какие фракции есть: 1..7 постоянные, остальные — созданные в игре (строка в базе)
	for (new f = 1; f < MAX_FACTIONS; f++)
	{
		Faction[f][fType] = (f < FACTION_BUILTIN) ? Faction_BuiltinType(f) : FTYPE_NONE;
		Faction[f][fMarker] = false;
	}
	new DBResult:tr = DB_ExecuteQuery(gDB, "SELECT id, ftype FROM factions WHERE id >= %d", FACTION_BUILTIN);
	if (tr != DBResult:0)
	{
		new trows = DB_GetRowCount(tr);
		for (new i = 0; i < trows; i++)
		{
			new f = DB_GetFieldInt(tr, 0), type = DB_GetFieldInt(tr, 1);
			if (f >= FACTION_BUILTIN && f < MAX_FACTIONS && type >= FTYPE_LAW && type <= FTYPE_GOV && type != FTYPE_ARMY) Faction[f][fType] = type;
			DB_SelectNextRow(tr);
		}
		DB_FreeResultSet(tr);
	}

	// Первый запуск: записываем базы и машины по умолчанию
	new count = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM factions");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	// Строка в базе для каждой постоянной фракции (новая фракция в старой базе появится сама)
	for (new f = 1; f < FACTION_BUILTIN; f++)
	{
		DB_FreeResultSet(DB_ExecuteQuery(gDB,
			"INSERT OR IGNORE INTO factions (id, sx, sy, sz, sa, dx, dy, dz) VALUES (%d, %f, %f, %f, %f, %f, %f, %f)",
			f, gFactionDefBase[f][0], gFactionDefBase[f][1], gFactionDefBase[f][2], gFactionDefBase[f][3],
			gFactionDefBase[f][4], gFactionDefBase[f][5], gFactionDefBase[f][6]));
	}
	if (count == 0)
	{
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
	for (new f = 1; f < FACTION_BUILTIN; f++)
	{
		Faction[f][fSpawnX] = gFactionDefBase[f][0];
		Faction[f][fSpawnY] = gFactionDefBase[f][1];
		Faction[f][fSpawnZ] = gFactionDefBase[f][2];
		Faction[f][fSpawnA] = gFactionDefBase[f][3];
		Faction[f][fDutyX]  = gFactionDefBase[f][4];
		Faction[f][fDutyY]  = gFactionDefBase[f][5];
		Faction[f][fDutyZ]  = gFactionDefBase[f][6];
	}
	r = DB_ExecuteQuery(gDB, "SELECT id, sx, sy, sz, sa, dx, dy, dz, dint, dworld, sint, sworld FROM factions");
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
				Faction[f][fDutyInt]   = DB_GetFieldInt(r, 8);
				Faction[f][fDutyWorld] = DB_GetFieldInt(r, 9);
				Faction[f][fSpawnInt]   = DB_GetFieldInt(r, 10);
				Faction[f][fSpawnWorld] = DB_GetFieldInt(r, 11);
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
	FPerm_Init();           // права рангов (fmanage.inc), после загрузки фракций
}

// Таблицы основы фракций: название, цвет, общак, ранги, склад, журнал, выговоры.
// Старая база дополняется сама, данные игроков не трогаются.
stock Factions_CreateTables()
{
	DB_AddColumn("factions", "name", "TEXT");
	DB_AddColumn("factions", "color", "INTEGER DEFAULT 0");
	DB_AddColumn("factions", "bank", "INTEGER DEFAULT 0");
	DB_AddColumn("factions", "store_rank", "INTEGER DEFAULT 2");
	DB_AddColumn("factions", "dint", "INTEGER DEFAULT 0");
	DB_AddColumn("factions", "dworld", "INTEGER DEFAULT 0");
	DB_AddColumn("factions", "sint", "INTEGER DEFAULT 0");
	DB_AddColumn("factions", "sworld", "INTEGER DEFAULT 0");
	DB_AddColumn("factions", "ftype", "INTEGER DEFAULT 0");     // тип фракции, созданной в игре
	DB_AddColumn("factions", "mats", "INTEGER DEFAULT 2000");   // материалы на складе
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS faction_doors (faction INTEGER PRIMARY KEY, has_out INTEGER DEFAULT 0, ox REAL DEFAULT 0, oy REAL DEFAULT 0, oz REAL DEFAULT 0, oa REAL DEFAULT 0, oint INTEGER DEFAULT 0, oworld INTEGER DEFAULT 0, has_in INTEGER DEFAULT 0, ix REAL DEFAULT 0, iy REAL DEFAULT 0, iz REAL DEFAULT 0, ia REAL DEFAULT 0, iint INTEGER DEFAULT 0, iworld INTEGER DEFAULT 0, closed INTEGER DEFAULT 0)"));
	DB_AddColumn("accounts", "fwarns", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "fban_until", "INTEGER DEFAULT 0");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS faction_ranks (faction INTEGER NOT NULL, rank INTEGER NOT NULL, name TEXT, skin INTEGER DEFAULT 0, salary INTEGER DEFAULT 0, PRIMARY KEY (faction, rank))"));

	// Переход с 6 рангов на 10 (один раз, версия базы 2): лидер 6 -> 10, заместитель 5 -> 9.
	// Названия, скины и зарплаты этих рангов переезжают вместе с ними, ранги 5-8 создаются новые.
	if (DB_GetVersion() < 2)
	{
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET frank = 10 WHERE frank = 6"));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET frank = 9 WHERE frank = 5"));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE faction_ranks SET rank = 10 WHERE rank = 6"));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE faction_ranks SET rank = 9 WHERE rank = 5"));
		// чтобы новые ранги 5-8 не получали больше старших
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE faction_ranks SET salary = salary + %d WHERE rank >= 9 AND salary > 0", 4 * FACTION_SALARY_RANK));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE faction_ranks SET name = 'Зам. шефа' WHERE faction = %d AND rank = 9 AND name = 'Капитан'", FACTION_POLICE));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE factions SET store_rank = store_rank + 4 WHERE store_rank >= 5"));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "PRAGMA user_version = 2"));
		print("[Factions] Ranks migrated: 6 -> 10");
	}
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
		if (f >= FACTION_BUILTIN)
		{
			// Созданные в игре: всё лежит в базе, тут только запасные значения
			Faction[f][fName][0] = '\0';
			strcat(Faction[f][fName], "Фракция", 32);
			Faction[f][fColor] = COLOR_WHITE;
			Faction[f][fBank] = 0;
			Faction[f][fStoreRank] = 2;
			Faction[f][fMats] = 0;
			for (new rk = 0; rk < MAX_RANK; rk++)
			{
				gRankName[f][rk][0] = '\0';
				strcat(gRankName[f][rk], "Ранг", 24);
				gRankSkin[f][rk] = 0;
				gRankSalary[f][rk] = 0;
			}
			for (new it = 0; it < MAX_FITEMS; it++) gFStore[f][it] = 0;
			continue;
		}
		// Армия возит материалы другим, поэтому её склад с самого начала больше
		if (f == FACTION_ARMY) DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE factions SET mats = 20000 WHERE id = %d AND name IS NULL", f));
		// Название и цвет по умолчанию — только если их ещё нет в базе
		new startBank = Faction_IsState(f) ? FACTION_START_BANK : 0;
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE factions SET name = '%q', color = %d, bank = %d WHERE id = %d AND name IS NULL",
			gFactionDefName[f], gFactionDefColor[f], startBank, f));

		// Ранги по умолчанию
		new t = Faction_RankSet(f);
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
		Faction[f][fMats] = 0;
		for (new rk = 0; rk < MAX_RANK; rk++)
		{
			gRankName[f][rk][0] = '\0';
			strcat(gRankName[f][rk], gDefRankNames[t][rk], 24);
			gRankSkin[f][rk] = gFactionDefSkin[f][rk];
			gRankSalary[f][rk] = 0;
		}
		for (new it = 0; it < MAX_FITEMS; it++) gFStore[f][it] = 0;
	}

	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, name, color, bank, store_rank, mats FROM factions");
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
				Faction[f][fMats] = DB_GetFieldInt(r, 5);
				if (Faction[f][fMats] < 0) Faction[f][fMats] = 0;
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
	for (new f = 1; f < MAX_FACTIONS; f++)
	{
		if (Faction_IsValid(f)) Faction_CreateMarker(f);
	}
	FDoors_Load();      // входы в здания фракций (/setfint)
	Supply_Init();      // завод, склад армии, чёрный рынок
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
		Faction_Log(f, msg, FLOG_MONEY);
	}
}

// =====================================================================
//  События (вызываются из rp.pwn и accounts.inc)
// =====================================================================

// Где спавнится член фракции (дом игрока важнее — см. Account_SetNormalSpawn)
stock Factions_GetSpawn(playerid, &Float:x, &Float:y, &Float:z, &Float:a)
{
	new interior, world;
	return Factions_GetSpawnEx(playerid, x, y, z, a, interior, world);
}

// То же с интерьером и миром: доп. спавн, выбранный в /setspawn (spawns.inc), иначе основная база
stock Factions_GetSpawnEx(playerid, &Float:x, &Float:y, &Float:z, &Float:a, &interior, &world)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return 0;
	if (Spawns_GetFactionExtra(playerid, x, y, z, a, interior, world)) return 1;
	x = Faction[f][fSpawnX];
	y = Faction[f][fSpawnY];
	z = Faction[f][fSpawnZ];
	a = Faction[f][fSpawnA];
	interior = Faction[f][fSpawnInt];
	world = Faction[f][fSpawnWorld];
	return 1;
}

stock Jail_PutInCell(playerid)
{
	if (Prison_TryPutInCell(playerid)) return;     // большой срок — в федеральную тюрьму (prison.inc)
	if (Place_First(PL_KPZ) != -1)                 // камеры КПЗ, которые админ поставил сам (/place kpz)
	{
		Place_Teleport(playerid, Place_Random(PL_KPZ));
		ResetPlayerWeapons(playerid);
		Player[playerid][pHouseInside] = -1;
		return;
	}
	SetPlayerInterior(playerid, JAIL_INTERIOR);
	SetPlayerVirtualWorld(playerid, 0);
	// две камеры с решётками (law.inc): заключённых делим между ними
	if (playerid % 2 == 0) SetPlayerPos(playerid, JAIL_X, JAIL_CELL1_Y, JAIL_Z);
	else SetPlayerPos(playerid, JAIL_X, JAIL_CELL2_Y, JAIL_Z);
	ResetPlayerWeapons(playerid);
	Player[playerid][pHouseInside] = -1;
}

stock Jail_Release(playerid)
{
	Player[playerid][pJail] = 0;
	if (Prison_TryRelease(playerid)) return;       // из тюрьмы выпускают у её ворот (prison.inc)
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

	// Спавн фракции может стоять внутри здания (/setfspawn): OnPlayerSpawn уже вернул игрока на улицу
	new Float:sx, Float:sy, Float:sz, Float:sa, sint, sworld;
	if (Factions_GetSpawnEx(playerid, sx, sy, sz, sa, sint, sworld) && (sint != 0 || sworld != 0)
		&& IsPlayerInRangeOfPoint(playerid, 3.0, sx, sy, sz))
	{
		SetPlayerInterior(playerid, sint);
		SetPlayerVirtualWorld(playerid, sworld);
	}

	if (Player[playerid][pJail] > 0)
	{
		Jail_PutInCell(playerid);
		SendClientMessage(playerid, COLOR_RED, "Ты в тюрьме. Осталось сидеть: %d мин.", Player[playerid][pJail]);
	}
}

// Из OnPlayerDeath
stock Factions_OnDeath(playerid)
{
	Supply_ResetPlayer(playerid);   // краденое теряется
	Faction_Uncuff(playerid);
	Player[playerid][pDuty] = false;
}

// Из OnPlayerStateChange. Возвращает 1, если игрока высадили из чужой машины.
stock Factions_OnBecomeDriver(playerid)
{
	new v = GetPlayerVehicleID(playerid);
	new f = gVehFaction[v];
	if (f == FACTION_NONE) return 0;
	if (Player[playerid][pAdmin] >= 3 || (Player[playerid][pFaction] == f && FPerm_Has(playerid, FP_VEHICLE)))
	{
		SendClientMessage(playerid, Faction_GetColor(f), "Машина фракции «%s». Двигатель: 2, шлагбаум: H", Faction_GetName(f));
		return 1;
	}
	// Машину с ящиками может угнать член другой организации (перехват груза)
	if (gVehCargo[v] > 0 && Faction_IsValid(Player[playerid][pFaction]))
	{
		SendClientMessage(playerid, COLOR_YELLOW, "Чужая машина с грузом (%d материалов). Довези до склада своей организации и разгрузи: H.", gVehCargo[v]);
		return 1;
	}
	RemovePlayerFromVehicle(playerid);
	if (Player[playerid][pFaction] == f)
		SendClientMessage(playerid, COLOR_GREY, "У тебя нет права «Транспорт организации». Его выдают лидер и зам: /fr > Управление.");
	else
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
		// Ушёл от камеры — возвращаем (кроме случая, когда дверь открыл полицейский или взята взятка, law.inc)
		if (Prison_OnMinute(i)) { }     // заключённый федеральной тюрьмы (prison.inc)
		else if (Place_First(PL_KPZ) != -1)
		{
			if (Place_Nearby(i, PL_KPZ, 15.0) == -1 && !Law_MayLeaveCell(i)) Jail_PutInCell(i);
		}
		else if (!Law_MayLeaveCell(i) && (GetPlayerInterior(i) != JAIL_INTERIOR || !IsPlayerInRangeOfPoint(i, 15.0, JAIL_X, JAIL_Y, JAIL_Z)))
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
	if (salary > Faction_MaxSalary(f)) salary = Faction_MaxSalary(f);   // ставка, сохранённая до появления потолка
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
	if (FPerm_IsMuted(playerid)) return true;  // мут рации (fmanage.inc, /fmute)
	format(msg, sizeof msg, "[Рация] %s %s: %s", Faction_GetRankName(f, Player[playerid][pRank]), GetRPName(playerid), text);
	Faction_Message(f, Faction_GetColor(f), msg);
	Fbi_OnRadio(f, msg);        // прослушка ФБР (fbi.inc)
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

forward Fbi_SetPlain(playerid);    // fbi.inc: агент на смене в штатском
stock bool:Cmd_Duty(playerid, bool:plain = false)
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
	if (plain && !Faction_IsFbi(f)) plain = false;
	Player[playerid][pDuty] = true;
	ResetPlayerWeapons(playerid);
	if (plain)
	{
		// ФБР в штатском: своя одежда, ник и точка как у гражданского, все команды фракции работают
		Fbi_SetPlain(playerid);
		SetPlayerSkin(playerid, Player[playerid][pSkin]);
		GivePlayerWeapon(playerid, WEAPON_NITESTICK, 1);
		Faction_UpdateColor(playerid);
		SendClientMessage(playerid, Faction_GetColor(f), "Ты на смене в штатском (%s): для всех ты обычный гражданский, команды ФБР работают. Форма: /disguise", Faction_GetRankName(f, rank));
		SendClientMessage(playerid, COLOR_WHITE, "Оружие и бронежилет лежат на складе фракции: /fstorage. Закончить смену: /duty в раздевалке.");
		return true;
	}
	SetPlayerSkin(playerid, gRankSkin[f][rank - 1]);

	// Оружие и броню на смене не выдаём: их берут со склада фракции (/fstorage).
	// Силовикам остаётся только дубинка.
	if (Faction_IsLaw(f)) GivePlayerWeapon(playerid, WEAPON_NITESTICK, 1);
	Faction_UpdateColor(playerid);
	format(msg, sizeof msg, "%s переодевается в форму.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(playerid, Faction_GetColor(f), "Ты на смене (%s). Команды фракции: /fhelp", Faction_GetRankName(f, rank));
	SendClientMessage(playerid, COLOR_WHITE, "Оружие и бронежилет лежат на складе фракции: /fstorage.");
	return true;
}

stock bool:Cmd_Invite(playerid, const params[])
{
	if (!FPerm_Need(playerid, FP_INVITE)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /invite [id]");
	if (target == playerid || !Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Неверный игрок.");
	if (Player[target][pFaction] != FACTION_NONE) return SendClientMessage(playerid, COLOR_GREY, "Игрок уже состоит во фракции.");
	if (!IsPlayerNearPlayer(playerid, target, 5.0)) return SendClientMessage(playerid, COLOR_GREY, "Игрок слишком далеко.");
	new banUntil = Faction_BanUntil(Player[target][pDbId]);
	if (banUntil)
	{
		new date[16];
		Docs_Date(date, sizeof date, banUntil);
		return SendClientMessage(playerid, COLOR_GREY, "Его уволили из организации: принять можно с %s.", date);
	}

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
	if (!FPerm_Need(playerid, FP_FIRE)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	new f = Player[playerid][pFaction];
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /uninvite [id]");
	if (target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Чтобы уйти самому: /fleave");
	if (Player[target][pFaction] != f) return SendClientMessage(playerid, COLOR_GREY, "Игрок не в твоей фракции.");
	if (Player[target][pRank] >= Player[playerid][pRank]) return SendClientMessage(playerid, COLOR_GREY, "Нельзя уволить равного или старшего по рангу.");

	Faction_SetMember(target, FACTION_NONE, 0);
	Faction_BanFired(Player[target][pDbId]);
	SendClientMessage(target, COLOR_RED, "%s выгнал тебя из фракции «%s». В организации не примут %d дн.", GetRPName(playerid), Faction_GetName(f), FACTION_FIRE_BAN_DAYS);
	new msg[144];
	format(msg, sizeof msg, "[Фракция] %s уволил %s.", GetRPName(playerid), GetRPName(target));
	Faction_Message(f, Faction_GetColor(f), msg);
	Faction_Log(f, msg[10], FLOG_STAFF);
	return true;
}

stock bool:Cmd_GiveRank(playerid, const params[])
{
	if (!FPerm_Need(playerid, FP_RANK)) return true;
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
	Faction_Log(f, msg[10], FLOG_STAFF);
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
	Faction_Log(f, msg[10], FLOG_STAFF);
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
			if (Player[from][pFaction] != f || Player[playerid][pFaction] != FACTION_NONE || Faction_BanUntil(Player[playerid][pDbId]))
				return SendClientMessage(playerid, COLOR_GREY, "Предложение больше не действует.");
			Faction_SetMember(playerid, f, 1);
			SendClientMessage(playerid, Faction_GetColor(f), "Добро пожаловать во фракцию «%s»! Команды: /fhelp", Faction_GetName(f));
			new msg[144];
			format(msg, sizeof msg, "[Фракция] %s принял %s во фракцию.", GetRPName(from), GetRPName(playerid));
			Faction_Message(f, Faction_GetColor(f), msg);
			Faction_Log(f, msg[10], FLOG_STAFF);
		}
		case OFFER_HEAL:
		{
			new price = Player[playerid][pOfferValue];
			if (!Faction_IsMedic(Player[from][pFaction]) || !Player[from][pDuty] || !IsPlayerNearPlayer(playerid, from, 5.0))
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
		case OFFER_CARRY: Wound_AcceptCarry(playerid, from);
		case OFFER_BRIBE: Law_AcceptBribe(playerid, from, Player[playerid][pOfferValue]);
		case OFFER_DRUGS: Drugs_AcceptSale(playerid, from, Player[playerid][pOfferValue]);
		case OFFER_MARRY: Marry_Accept(playerid, from);
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
	if (!Faction_NeedLaw(playerid)) return true;
	new text[110], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /m [текст] — мегафон");
	format(msg, sizeof msg, "[Мегафон] %s: %s", GetRPName(playerid), text);
	SendNearbyMessage(playerid, 60.0, COLOR_YELLOW, msg);
	return true;
}

stock bool:Cmd_Suspect(playerid, const params[])
{
	if (!Faction_NeedLaw(playerid)) return true;
	new pos = 0, stars, reason[64];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, stars) || stars < 1 || stars > 6 || !RestParam(params, pos, reason))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /su [id] [звёзды 1-6] [причина]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	if (Faction_IsLaw(Player[target][pFaction]) && Player[target][pDuty])
		return SendClientMessage(playerid, COLOR_GREY, "Нельзя объявить в розыск полицейского на смене.");

	Player[target][pWanted] = stars;
	SetPlayerWantedLevel(target, stars);
	Account_Save(target);
	SendClientMessage(target, COLOR_RED, "Тебя объявили в розыск (%d зв.). Причина: %s", stars, reason);
	new msg[144];
	format(msg, sizeof msg, "[Диспетчер] %s объявил в розыск %s [%d] — %d зв. Причина: %s",
		GetRPName(playerid), GetRPName(target), target, stars, reason);
	Faction_MessageLaw(msg);
	return true;
}

stock bool:Cmd_ClearWanted(playerid, const params[])
{
	if (!Faction_NeedLaw(playerid)) return true;
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
	Faction_MessageLaw(msg);
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
	if (!Prison_IsGuardOnDuty(playerid) && !Faction_NeedLaw(playerid)) return true;   // охрана тюрьмы тоже (prison.inc)
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
	if (!Prison_IsGuardOnDuty(playerid) && !Faction_NeedLaw(playerid)) return true;   // охрана тюрьмы тоже (prison.inc)
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
	if (!Prison_IsGuardOnDuty(playerid) && !Faction_NeedLaw(playerid)) return true;   // охрана тюрьмы тоже (prison.inc)
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
	Drugs_FriskInfo(playerid, target);     // наркотики (drugs.inc)
	return true;
}

// Посадить задержанного в свою машину (полицейский сидит за рулём)
stock bool:Cmd_PutCar(playerid, const params[])
{
	if (!Faction_NeedLaw(playerid)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /putcar [id] (сидя за рулём)");
	new v = GetPlayerVehicleID(playerid);
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER || !Faction_IsLaw(gVehFaction[v]))
		return SendClientMessage(playerid, COLOR_GREY, "Сядь за руль служебной машины полиции или ФБР.");
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
	if (!Faction_NeedLaw(playerid)) return true;
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
	Law_ClearMark(target);
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
	Faction_MessageLaw(msg);
	return true;
}

// =====================================================================
//  Медики
// =====================================================================
stock bool:Cmd_Heal(playerid, const params[])
{
	if (!Faction_NeedMedic(playerid)) return true;
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
		"Положить деньги в общак\nСнять деньги из общака {AFAFAF}(по праву)", "Выбрать", "Закрыть");
	return true;
}

// =====================================================================
//  Склад фракции (/fstorage)
// =====================================================================
// Что лежит на складе фракции. Медикам оружие и броня не нужны: у них только аптечки,
// которые они делают из материалов (медикаменты) и берут в сумку для /aid.
static bool:Faction_StoreHas(f, it)
{
	if (Faction_IsMedic(f)) return (it == FITEM_MEDKIT);
	return (it >= 0 && it < MAX_FITEMS);
}

// Цена закупки за 1 шт.: госструктуры платят из общака, банды и медики делают сами из материалов
static Faction_StoreCost(f, it)
{
	if (!Faction_IsState(f) || Faction_IsMedic(f)) return 0;
	return gFItemPrice[it];
}

// Строка окна склада -> предмет. MAX_FITEMS — строка «Пополнить», -1 — нет такой строки.
static Faction_StoreRowItem(f, row)
{
	new n = 0;
	for (new it = 0; it < MAX_FITEMS; it++)
	{
		if (!Faction_StoreHas(f, it)) continue;
		if (n++ == row) return it;
	}
	return (row == n) ? MAX_FITEMS : -1;
}

stock bool:Cmd_FStorage(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь во фракции.");
	if (!Faction_AtBase(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Склад доступен только на базе фракции (у раздевалки).");

	new body[900], line[96];
	new bool:medic = Faction_IsMedic(f);
	strcat(body, "Снаряжение\tНа складе\tЦена закупки\n");
	for (new it = 0; it < MAX_FITEMS; it++)
	{
		if (!Faction_StoreHas(f, it)) continue;
		if (medic) format(line, sizeof line, "%s (в сумку)\t%d шт.\t%d мат.\n", gFItemName[it], gFStore[f][it], gFItemMats[it]);
		else format(line, sizeof line, "%s\t%d шт.\t$%d\n", gFItemName[it], gFStore[f][it], gFItemPrice[it]);
		strcat(body, line);
	}
	if (medic) strcat(body, "{FFFF00}» Сделать медикаменты из материалов\t{AFAFAF}по праву\t");
	else strcat(body, "{FFFF00}» Пополнить склад (закупка, у банд — изготовление)\t{AFAFAF}по праву\t");
	new title[64];
	format(title, sizeof title, "Склад: материалов %d", Faction[f][fMats]);
	ShowPlayerDialog(playerid, DIALOG_FSTORE, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Взять", "Закрыть", body);
	return true;
}

stock Faction_TakeItem(playerid, it)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f) || it < 0 || it >= MAX_FITEMS) return 0;
	if (!Faction_AtBase(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от склада.");
	if (!FPerm_Need(playerid, FP_STORE_TAKE)) return 1;
	if (Faction_IsState(f) && !Player[playerid][pDuty])
		return SendClientMessage(playerid, COLOR_GREY, "Сначала заступи на смену: /duty.");
	if (!Faction_StoreHas(f, it)) return 1;
	if (gFStore[f][it] <= 0) return SendClientMessage(playerid, COLOR_GREY, "На складе нет: %s. Лидер может закупить в /fstorage.", gFItemName[it]);

	if (Faction_IsMedic(f))
	{
		// медик берёт аптечку в сумку: ей он лечит раненых (/aid)
		if (!Inv_Add(playerid, INV_MEDKIT, 1))
			return SendClientMessage(playerid, COLOR_GREY, "В сумке нет места для аптечки (до %d шт.).", Inv_GetMax(INV_MEDKIT));
	}
	else switch (it)
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
	Faction_Log(f, msg, FLOG_MONEY);
	return 1;
}

stock Faction_ShowBuyList(playerid)
{
	new f = Player[playerid][pFaction];
	new body[900], line[96];
	new bool:gov = Faction_IsState(f);    // госструктуры платят из общака, банды делают оружие сами — только из материалов
	if (Faction_IsMedic(f)) gov = false;    // медики делают медикаменты сами — только из материалов
	strcat(body, "Снаряжение\tЦена за 1 шт.\tМатериалов за 1 шт.\tНа складе\n");
	for (new it = 0; it < MAX_FITEMS; it++)
	{
		if (!Faction_StoreHas(f, it)) continue;
		format(line, sizeof line, "%s\t$%d\t%d\t%d шт.\n", gFItemName[it], gov ? gFItemPrice[it] : 0, gFItemMats[it], gFStore[f][it]);
		strcat(body, line);
	}
	new title[64];
	format(title, sizeof title, "Закупка (общак: $%d, материалов: %d)", Faction[f][fBank], Faction[f][fMats]);
	ShowPlayerDialog(playerid, DIALOG_FSTORE_BUY, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Выбрать", "Назад", body);
}

// =====================================================================
//  Панель лидера (/lmenu)
// =====================================================================
stock bool:Faction_IsLeader(playerid)
{
	return (Faction_IsValid(Player[playerid][pFaction]) && Player[playerid][pRank] >= MAX_RANK);
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
				format(line, sizeof line, "%s\t%d. %s\t%d/%d\t{33AA33}онлайн [%d]\n", name, rank, Faction_GetRankName(f, rank), DB_GetFieldInt(r, 3), Org_WarnLimit(f), online);
			else
				format(line, sizeof line, "%s\t%d. %s\t%d/%d\t{AFAFAF}оффлайн\n", name, rank, Faction_GetRankName(f, rank), DB_GetFieldInt(r, 3), Org_WarnLimit(f));
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

// Уволить (онлайн или оффлайн)
stock Faction_Fire(playerid, dbid, const name[], const reason[])
{
	new f = Player[playerid][pFaction];
	new target = Faction_FindOnline(dbid);
	if (target != INVALID_PLAYER_ID)
	{
		Faction_SetMember(target, FACTION_NONE, 0);
		SendClientMessage(target, COLOR_RED, "%s уволил тебя из фракции «%s». Причина: %s. В организации не примут %d дн.", GetRPName(playerid), Faction_GetName(f), reason, FACTION_FIRE_BAN_DAYS);
	}
	else
	{
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET faction = 0, frank = 0, fwarns = 0, fsquad = 0 WHERE id = %d", dbid));
		FPerm_ClearOffline(dbid);
	}
	Faction_BanFired(dbid);
	new msg[144];
	format(msg, sizeof msg, "%s уволил %s. Причина: %s", GetRPName(playerid), name, reason);
	Faction_Log(f, msg, FLOG_STAFF);
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
	Faction_Log(f, msg, FLOG_STAFF);
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
	if (FM_OnDialog(playerid, dialogid, response, listitem, inputtext)) return 1;   // «Управление» (fmanage.inc)
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
				if (!FPerm_Need(playerid, FP_BANK_OUT)) return 1;
#if !FACTION_STATE_WITHDRAW
				if (Faction_IsState(f)) { SendClientMessage(playerid, COLOR_GREY, "Бюджет госструктуры наличными не выдают: он идёт на зарплаты и снаряжение."); return 1; }
#endif
				ShowPlayerDialog(playerid, DIALOG_FBANK_WITHDRAW, DIALOG_STYLE_INPUT, "Снять из общака",
					"Сколько снять из общака?\nВ общаке: $%d", "Снять", "Назад", Faction[f][fBank]);
			}
		}
		case DIALOG_FBANK_DEPOSIT:
		{
			if (!response) { Cmd_FBank(playerid); return 1; }
			if (!FPerm_Need(playerid, FP_BANK_IN)) return 1;
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
			Faction_Log(f, msg, FLOG_MONEY);
		}
		case DIALOG_FBANK_WITHDRAW:
		{
			if (!response) { Cmd_FBank(playerid); return 1; }
			if (!FPerm_Need(playerid, FP_BANK_OUT)) return 1;
#if !FACTION_STATE_WITHDRAW
			if (Faction_IsState(f)) return 1;
#endif
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
			Faction_Log(f, msg, FLOG_MONEY);
			format(msg, sizeof msg, "[Фракция] %s снял из общака $%d.", GetRPName(playerid), amount);
			Faction_Message(f, Faction_GetColor(f), msg);
		}
		case DIALOG_FSTORE:
		{
			if (!response) return 1;
			new it = Faction_StoreRowItem(f, listitem);
			if (it == -1) return 1;
			if (it < MAX_FITEMS)
			{
				Faction_TakeItem(playerid, it);
				return 1;
			}
			if (!FPerm_Need(playerid, FP_STORE_BUY)) return 1;
			Faction_ShowBuyList(playerid);
		}
		case DIALOG_FSTORE_BUY:
		{
			if (!response) { Cmd_FStorage(playerid); return 1; }
			new it = Faction_StoreRowItem(f, listitem);
			if (it < 0 || it >= MAX_FITEMS) return 1;
			gLSelItem[playerid] = it;
			ShowPlayerDialog(playerid, DIALOG_FSTORE_BUY_QTY, DIALOG_STYLE_INPUT, "Закупка",
				"%s — $%d и %d материалов за 1 шт.\nСколько закупить? (1-100)\nВ общаке: $%d, материалов: %d", "Купить", "Назад",
				gFItemName[it], Faction_StoreCost(f, it), gFItemMats[it], Faction[f][fBank], Faction[f][fMats]);
		}
		case DIALOG_FSTORE_BUY_QTY:
		{
			if (!response) { Faction_ShowBuyList(playerid); return 1; }
			if (!FPerm_Need(playerid, FP_STORE_BUY)) return 1;
			if (!Faction_AtBase(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от базы."); return 1; }
			new it = gLSelItem[playerid];
			if (it < 0 || it >= MAX_FITEMS || !Faction_StoreHas(f, it)) return 1;
			if (!IsNumeric(inputtext) || (amount = strval(inputtext)) < 1 || amount > 100)
			{
				SendClientMessage(playerid, COLOR_GREY, "Количество от 1 до 100.");
				return 1;
			}
			new cost = amount * Faction_StoreCost(f, it);   // банды и медики делают сами, без денег
			new need = amount * gFItemMats[it];
			if (Faction[f][fMats] < need)
			{
				SendClientMessage(playerid, COLOR_GREY, "Не хватает материалов: нужно %d, на складе %d. Госструктурам их возит армия, банды добывают сами.", need, Faction[f][fMats]);
				return 1;
			}
			if (!Faction_AddBank(f, -cost)) { SendClientMessage(playerid, COLOR_GREY, "В общаке не хватает денег: нужно $%d.", cost); return 1; }
			if (need > 0)
			{
				Faction[f][fMats] -= need;
				Faction_SaveMats(f);
			}
			gFStore[f][it] += amount;
			Faction_SaveStore(f, it);
			SendClientMessage(playerid, COLOR_GREEN, "Закуплено: %s x%d за $%d. На складе: %d.", gFItemName[it], amount, cost, gFStore[f][it]);
			format(msg, sizeof msg, "%s закупил на склад: %s x%d за $%d.", GetRPName(playerid), gFItemName[it], amount, cost);
			Faction_Log(f, msg, FLOG_MONEY);
			Cmd_FStorage(playerid);
		}
		case DIALOG_FSTORE_RANK:
		{
			if (!response) { Cmd_LMenu(playerid); return 1; }
			if (!FPerm_CanEdit(playerid)) return 1;
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
		case DIALOG_LMEMBERS:
		{
			if (!response) { Cmd_LMenu(playerid); return 1; }
			if (listitem < 0 || listitem >= FACTION_LIST_MAX || gLMemberIds[playerid][listitem] == 0) return 1;
			gLSelMember[playerid] = gLMemberIds[playerid][listitem];
			Faction_ShowMemberActions(playerid);
		}
		case DIALOG_LMEMBER_RANK:
		{
			if (!response) { Faction_ShowMemberActions(playerid); return 1; }
			new name[MAX_PLAYER_NAME], rank;
			if (!FM_CanAct(playerid, FP_RANK, name, rank)) return 1;
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
			if (!FM_CanAct(playerid, FP_WARN, name, rank)) return 1;
			if (!RestParam(inputtext, 0, reason) || !Faction_IsGoodName(reason, 2, 60))
			{
				SendClientMessage(playerid, COLOR_GREY, "Напиши причину: от 2 до 60 символов, без фигурных скобок и знака процента.");
				return 1;
			}
			new warns = Faction_GetWarns(gLSelMember[playerid]) + 1;
			new target = Faction_FindOnline(gLSelMember[playerid]);
			if (warns >= Org_WarnLimit(f))
			{
				new why[96];
				format(why, sizeof why, "%d/%d выговоров (последний: %s)", warns, Org_WarnLimit(f), reason);
				Faction_Fire(playerid, gLSelMember[playerid], name, why);
				return 1;
			}
			Faction_SetWarns(gLSelMember[playerid], warns);
			format(msg, sizeof msg, "%s выдал выговор %s (%d/%d). Причина: %s", GetRPName(playerid), name, warns, Org_WarnLimit(f), reason);
			Faction_Log(f, msg, FLOG_WARN);
			SendClientMessage(playerid, COLOR_WHITE, "Выговор выдан (%d/%d).", warns, Org_WarnLimit(f));
			if (target != INVALID_PLAYER_ID)
				SendClientMessage(target, COLOR_RED, "%s выдал тебе выговор (%d/%d). Причина: %s", GetRPName(playerid), warns, Org_WarnLimit(f), reason);
			Faction_ShowMemberActions(playerid);
		}
		case DIALOG_LRANKS:
		{
			if (!response) { Cmd_LMenu(playerid); return 1; }
			if (!FPerm_Need(playerid, FP_RANKS) || listitem < 0 || listitem >= MAX_RANK) return 1;
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
			if (!FPerm_Need(playerid, FP_RANKS)) return 1;
			new rk = gLSelRank[playerid];
			if (rk < 1 || rk > MAX_RANK) return 1;
			switch (listitem)
			{
				case 0: ShowPlayerDialog(playerid, DIALOG_LRANK_NAME, DIALOG_STYLE_INPUT, "Название ранга",
					"Новое название ранга %d (2-23 символа)\nСейчас: %s", "Сохранить", "Назад", rk, gRankName[f][rk - 1]);
				case 1: ShowPlayerDialog(playerid, DIALOG_LRANK_SALARY, DIALOG_STYLE_INPUT, "Зарплата ранга",
					"Зарплата ранга %d на PayDay (0-%d). Платится из общака.\nСейчас: $%d", "Сохранить", "Назад", rk, Faction_MaxSalary(f), gRankSalary[f][rk - 1]);
				case 2: ShowPlayerDialog(playerid, DIALOG_LRANK_SKIN, DIALOG_STYLE_INPUT, "Скин ранга",
					"Скин формы для ранга %d (0-311)\nСейчас: %d", "Сохранить", "Назад", rk, gRankSkin[f][rk - 1]);
			}
		}
		case DIALOG_LRANK_NAME, DIALOG_LRANK_SALARY, DIALOG_LRANK_SKIN:
		{
			if (!response) { Faction_ShowRanks(playerid); return 1; }
			if (!FPerm_Need(playerid, FP_RANKS)) return 1;
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
				if (!IsNumeric(inputtext) || (amount = strval(inputtext)) < 0 || amount > Faction_MaxSalary(f))
				{
					SendClientMessage(playerid, COLOR_GREY, "Зарплата от $0 до $%d.", Faction_MaxSalary(f));
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
		SendClientMessage(playerid, COLOR_GREY, "Использование: /fedit [фракция 1-%d] name [название] | color [RRGGBB] | bank [сумма] | mats [материалы]", MAX_FACTIONS - 1);
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
	else if (!strcmp(what, "mats", true))
	{
		if (!IsNumeric(value)) return SendClientMessage(playerid, COLOR_GREY, "Материалы — число.");
		new amount = strval(value);
		if (amount < 0 || amount > ARMY_MATS_MAX) return SendClientMessage(playerid, COLOR_GREY, "Материалы от 0 до %d.", ARMY_MATS_MAX);
		Faction[f][fMats] = amount;
		Faction_SaveMats(f);
		format(msg, sizeof msg, "Администратор %s установил материалы на складе: %d.", GetRPName(playerid), amount);
	}
	else return SendClientMessage(playerid, COLOR_GREY, "Можно менять: name, color, bank, mats.");

	Faction_SaveInfo(f);
	Faction_CreateMarker(f);
	Turfs_Recolor();
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
	if (!strcmp(cmd, "/orgs", true))        { Cmd_Orgs(playerid); return 1; }      // orgs.inc
	if (!strcmp(cmd, "/swarn", true))       { Cmd_SWarn(playerid, params); return 1; }
	if (!strcmp(cmd, "/unswarn", true))     { Cmd_UnSWarn(playerid, params); return 1; }
	if (!strcmp(cmd, "/fmute", true))       { Cmd_FMute(playerid, params); return 1; }     // fmanage.inc
	if (!strcmp(cmd, "/deptaccess", true))  { Cmd_DeptAccess(playerid, params); return 1; } // orgs.inc
	if (!strcmp(cmd, "/flimit", true))      { Cmd_FLimit(playerid, params); return 1; }      // orgs.inc
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
	new text[4000];
	strcat(text, "{FFFF00}Все фракции\n");
	strcat(text, "{FFFFFF}/fr — окно фракции   /f — рация   /members — кто онлайн   /duty — смена (в раздевалке)   /fleave — уйти\n");
	strcat(text, "/accept — принять приглашение или лечение\n");
	strcat(text, "/fbank — общак (на базе)   /fstorage — склад: оружие, бронежилет, аптечка (на базе)\n\n");
	strcat(text, "{FFFF00}Лидер и заместитель (ранг 9-10)\n");
	strcat(text, "{FFFFFF}/lmenu — «Управление»: состав и карточки, права рангов и личные, выговоры, ранги, склад, журнал\n");
	strcat(text, "{FFFFFF}/swarn [id] [причина], /unswarn [id] — спец-выговор сотруднику другой организации (если есть доступ)\n");
	strcat(text, "{FFFFFF}/fmute [id] [минуты] [причина] — мут рации /f (0 — снять), /deptaccess — доступ другой фракции к /d, /flimit — её лимиты выговоров\n");
	strcat(text, "/invite [id]   /uninvite [id]   /giverank [id] [ранг]\n\n");
	strcat(text, "{3A6EFF}Полиция LSPD\n");
	strcat(text, "{FFFFFF}/su [id] [звёзды] [причина] — розыск   /clear [id] — снять розыск   /wanted — список\n");
	strcat(text, "/cuff /uncuff [id] — наручники   /putcar [id] — посадить в машину\n");
	strcat(text, "/arrest [id] — посадить в тюрьму (у участка)   /m — мегафон\n");
	strcat(text, "/mark [id] — метка задержания: выйдет из игры — попадёт в деморган\n");
	strcat(text, "/fine [id] [сумма] [причина] — штраф   /celldoor — открыть дверь участка заключённым\n");
	strcat(text, "/impound [штраф] [причина] — эвакуировать личную машину на штрафстоянку (стоя рядом)\n\n");
	strcat(text, "{FF8080}Больница (EMS)\n");
	strcat(text, "{FFFFFF}/heal [id] — предложить лечение\n\n");
	strcat(text, "{10A010}Банды\n");
	strcat(text, "{FFFFFF}/duty на базе — форма банды. /turfs /capture — войны за районы, районы пополняют общак.\n");
	strcat(text, "ALT на маркере склада армии — красть материалы в сумку. Оружие из них: сумка, «использовать».\n");
	strcat(text, "Заводы можно захватывать: ALT на маркере завода (ранг 7+). Торговец чёрного рынка — ALT. /gd — чат банд.\n\n");
	strcat(text, "{FFFF00}Госструктуры\n{FFFFFF}/d — общий чат госструктур (департамент)\n\n");
	strcat(text, "{4682B4}ФБР\n{FFFFFF}/disguise [скин] — маскировка в гражданское   /fdisguise [фракция] [ранг] — в форму организации (на смене)\n/dossier [id] — досье   /track [id] — слежка на карте (ранг 2+)\n/tap [номер] — прослушка телефона   /tapf [фракция] — рация организации, у банд и /gd (ранг 3+)   /untap\n\n");
	strcat(text, "{6B8E23}Армия\n");
	strcat(text, "{FFFFFF}На заводе: ALT у сырья, у станка, потом ящик на склад завода. Со склада ящик кладут в грузовик сзади (ALT).\n");
	strcat(text, "За рулём на складе организации — H или /unload. Без материалов госструктурам не закупить оружие.\n\n");
	strcat(text, "{FF66FF}Админам (4+)\n");
	strcat(text, "{FFFFFF}/makeleader /setfaction /setfspawn /setfduty /setfint /delfint /fintlock /addfcar /delfcar /fedit\n/createfaction /delfaction /flist /fcolor /setarmystore /setfstore /addbm /delbm /bmprice /setrankname\n/addfactory /setfpoint /setfowner /delfactory /factories   (2+) /jail /unjail");
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
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, f) || f < 0 || f >= MAX_FACTIONS || (f != FACTION_NONE && !Faction_IsValid(f)))
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
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /makeleader [id] [фракция 1-%d]  (1 LSPD, 2 EMS, 3 Grove, 4 Ballas, 5 Aztecas, 6 ФБР)", MAX_FACTIONS - 1);
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
		"UPDATE factions SET sx = %f, sy = %f, sz = %f, sa = %f, dx = %f, dy = %f, dz = %f, dint = %d, dworld = %d, sint = %d, sworld = %d WHERE id = %d",
		Faction[f][fSpawnX], Faction[f][fSpawnY], Faction[f][fSpawnZ], Faction[f][fSpawnA],
		Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ], Faction[f][fDutyInt], Faction[f][fDutyWorld],
		Faction[f][fSpawnInt], Faction[f][fSpawnWorld], f));
}

stock bool:Cmd_SetFSpawn(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f;
	if (!NextIntParam(params, pos, f) || !Faction_IsValid(f))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setfspawn [фракция 1-%d] — спавн фракции там, где ты стоишь", MAX_FACTIONS - 1);
	// спавн может быть внутри здания: запоминаем интерьер и мир
	GetPlayerPos(playerid, Faction[f][fSpawnX], Faction[f][fSpawnY], Faction[f][fSpawnZ]);
	GetPlayerFacingAngle(playerid, Faction[f][fSpawnA]);
	Faction[f][fSpawnInt] = GetPlayerInterior(playerid);
	Faction[f][fSpawnWorld] = GetPlayerVirtualWorld(playerid);
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
	// раздевалка может быть внутри здания (например, в участке LSPD): запоминаем интерьер и мир
	GetPlayerPos(playerid, Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ]);
	Faction[f][fDutyInt] = GetPlayerInterior(playerid);
	Faction[f][fDutyWorld] = GetPlayerVirtualWorld(playerid);
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

// =====================================================================
//  Вход в здание фракции (админ 4+)
//
//  /setfint [фракция]  — на улице у двери: точка входа; внутри интерьера: точка выхода.
//                        Вставать у самой двери спиной к ней: игрок появится на шаг впереди.
//                        Вторым словом можно указать сторону сам: out (снаружи) или in (внутри).
//  /delfint [фракция]  — убрать вход.
//  /fintlock [фракция] — пускать только своих (переключатель).
//  Попасть в интерьер: /gotoint (список стандартных интерьеров GTA).
//  Маркеры входа видны в /markers, там их можно подвинуть.
// =====================================================================
#define FDOOR_REF           1000    // ref маркеров MK_DOOR: 1000 + фракция * 2 (внутри: + 1)

enum E_FDOOR
{
	bool:feOut,                 // точка снаружи задана
	Float:feOX, Float:feOY, Float:feOZ, Float:feOA, feOInt, feOWorld,
	bool:feIn,                  // точка внутри задана
	Float:feIX, Float:feIY, Float:feIZ, Float:feIA, feIInt, feIWorld,
	bool:feClosed,              // только для своих
	feMkOut, feMkIn
}
static gFDoor[MAX_FACTIONS][E_FDOOR];

static FDoor_Build(f)
{
	if (gFDoor[f][feMkOut] != -1) { Marker_Destroy(gFDoor[f][feMkOut]); gFDoor[f][feMkOut] = -1; }
	if (gFDoor[f][feMkIn] != -1)  { Marker_Destroy(gFDoor[f][feMkIn]);  gFDoor[f][feMkIn] = -1; }
	if (!gFDoor[f][feOut] || !gFDoor[f][feIn]) return;

	new label[96], name[32];
	new Float:x = gFDoor[f][feOX], Float:y = gFDoor[f][feOY], Float:z = gFDoor[f][feOZ];
	gFDoor[f][feMkOut] = Marker_Create(MK_DOOR, FDOOR_REF + f * 2, Faction_GetName(f), DOOR_PICKUP_MODEL, x, y, z,
		gFDoor[f][feOWorld], gFDoor[f][feOInt], GPS_NONE, gFDoor[f][feOA]);
	format(label, sizeof label, "{FFFF66}%s\n{FFFFFF}ALT — войти", Faction_GetName(f));
	if (gFDoor[f][feClosed]) strcat(label, " (только свои)");
	Marker_SetLabel(gFDoor[f][feMkOut], label);

	x = gFDoor[f][feIX]; y = gFDoor[f][feIY]; z = gFDoor[f][feIZ];
	strcat(name, Faction_GetName(f), 22);
	strcat(name, ", выход");
	gFDoor[f][feMkIn] = Marker_Create(MK_DOOR, FDOOR_REF + f * 2 + 1, name, DOOR_EXIT_MODEL, x, y, z,
		gFDoor[f][feIWorld], gFDoor[f][feIInt], GPS_NONE, gFDoor[f][feIA]);
	Marker_SetLabel(gFDoor[f][feMkIn], "{FFFFFF}ALT — выйти", COLOR_WHITE, 10.0);
	Marker_SetFaction(gFDoor[f][feMkOut], Faction_MarkerOwner(f));
	Marker_SetFaction(gFDoor[f][feMkIn], Faction_MarkerOwner(f));
}

static FDoor_Save(f)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"INSERT OR REPLACE INTO faction_doors (faction, has_out, ox, oy, oz, oa, oint, oworld, has_in, ix, iy, iz, ia, iint, iworld, closed) VALUES (%d, %d, %f, %f, %f, %f, %d, %d, %d, %f, %f, %f, %f, %d, %d, %d)",
		f, gFDoor[f][feOut] ? 1 : 0, gFDoor[f][feOX], gFDoor[f][feOY], gFDoor[f][feOZ], gFDoor[f][feOA], gFDoor[f][feOInt], gFDoor[f][feOWorld],
		gFDoor[f][feIn] ? 1 : 0, gFDoor[f][feIX], gFDoor[f][feIY], gFDoor[f][feIZ], gFDoor[f][feIA], gFDoor[f][feIInt], gFDoor[f][feIWorld],
		gFDoor[f][feClosed] ? 1 : 0));
}

// Забыть правки маркеров входа из /markers: точку только что поставили заново
static FDoor_ForgetMarkers(f)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM markers WHERE type = %d AND ref IN (%d, %d)",
		MK_DOOR, FDOOR_REF + f * 2, FDOOR_REF + f * 2 + 1));
}

// Входы по умолчанию для ФБР и армии: ставятся один раз (потом их правят /setfint или убирают /delfint).
// Внутри — стандартные интерьеры GTA в своём мире, чтобы не пересекаться с другими.
static FDoor_SetDefault(f, Float:ox, Float:oy, Float:oz, Float:oa, inter, Float:ix, Float:iy, Float:iz, Float:ia)
{
	gFDoor[f][feOut] = true;
	gFDoor[f][feOX] = ox; gFDoor[f][feOY] = oy; gFDoor[f][feOZ] = oz; gFDoor[f][feOA] = oa;
	gFDoor[f][feOInt] = 0; gFDoor[f][feOWorld] = 0;
	gFDoor[f][feIn] = true;
	gFDoor[f][feIX] = ix; gFDoor[f][feIY] = iy; gFDoor[f][feIZ] = iz; gFDoor[f][feIA] = ia;
	gFDoor[f][feIInt] = inter; gFDoor[f][feIWorld] = 100 + f;
	gFDoor[f][feClosed] = true;
	FDoor_Save(f);
}

static FDoors_Defaults()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS faction_door_defaults (faction INTEGER PRIMARY KEY)"));
	new n = 0;
	static const fs[] = { FACTION_FBI, FACTION_ARMY };
	for (new k = 0; k < sizeof fs; k++)
	{
		new f = fs[k];
		new DBResult:r = DB_ExecuteQuery(gDB, "SELECT 1 FROM faction_door_defaults WHERE faction = %d", f);
		new bool:done = false;
		if (r != DBResult:0) { done = DB_GetRowCount(r) > 0; DB_FreeResultSet(r); }
		if (done) continue;
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO faction_door_defaults (faction) VALUES (%d)", f));
		if (gFDoor[f][feOut] || gFDoor[f][feIn]) continue;     // вход уже ставили руками
		// ФБР: у мэрии -> интерьер полиции Лас-Вентураса; армия: Ocean Docks -> склад
		if (f == FACTION_FBI) FDoor_SetDefault(f, 1478.0, -1771.0, 18.8, 180.0,   3, 288.47, 170.06, 1007.17, 0.0);
		else FDoor_SetDefault(f, 2748.0, -2445.0, 13.7, 180.0,   18, 1290.41, 1.95, 1001.02, 0.0);
		n++;
	}
	return n;
}

stock FDoors_Load()
{
	for (new f = 0; f < MAX_FACTIONS; f++)
	{
		gFDoor[f][feOut] = false;
		gFDoor[f][feIn] = false;
		gFDoor[f][feClosed] = false;
		gFDoor[f][feMkOut] = -1;
		gFDoor[f][feMkIn] = -1;
	}
	new n = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT faction, has_out, ox, oy, oz, oa, oint, oworld, has_in, ix, iy, iz, ia, iint, iworld, closed FROM faction_doors");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new f = DB_GetFieldInt(r, 0);
			if (Faction_IsValid(f))
			{
				gFDoor[f][feOut]    = DB_GetFieldInt(r, 1) != 0;
				gFDoor[f][feOX]     = DB_GetFieldFloat(r, 2);
				gFDoor[f][feOY]     = DB_GetFieldFloat(r, 3);
				gFDoor[f][feOZ]     = DB_GetFieldFloat(r, 4);
				gFDoor[f][feOA]     = DB_GetFieldFloat(r, 5);
				gFDoor[f][feOInt]   = DB_GetFieldInt(r, 6);
				gFDoor[f][feOWorld] = DB_GetFieldInt(r, 7);
				gFDoor[f][feIn]     = DB_GetFieldInt(r, 8) != 0;
				gFDoor[f][feIX]     = DB_GetFieldFloat(r, 9);
				gFDoor[f][feIY]     = DB_GetFieldFloat(r, 10);
				gFDoor[f][feIZ]     = DB_GetFieldFloat(r, 11);
				gFDoor[f][feIA]     = DB_GetFieldFloat(r, 12);
				gFDoor[f][feIInt]   = DB_GetFieldInt(r, 13);
				gFDoor[f][feIWorld] = DB_GetFieldInt(r, 14);
				gFDoor[f][feClosed] = DB_GetFieldInt(r, 15) != 0;
				if (gFDoor[f][feOut] && gFDoor[f][feIn]) n++;
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	n += FDoors_Defaults();
	for (new f = 1; f < MAX_FACTIONS; f++) FDoor_Build(f);
	printf("[Factions] %d faction entrances.", n);
}

// ALT, /enter, /exit у входа фракции (из Doors_Try). true — сработало.
stock bool:FDoor_Try(playerid)
{
	for (new f = 1; f < MAX_FACTIONS; f++)
	{
		if (gFDoor[f][feMkOut] == -1 || gFDoor[f][feMkIn] == -1) continue;
		if (Marker_InRange(playerid, gFDoor[f][feMkOut], DOOR_RANGE))
		{
			if (gFDoor[f][feClosed] && Player[playerid][pFaction] != f && Player[playerid][pAdmin] < 1)
			{
				SendClientMessage(playerid, COLOR_GREY, "Сюда пускают только своих: %s.", Faction_GetName(f));
				return true;
			}
			if (Law_TryLeave(playerid)) Door_SpawnAt(playerid, gFDoor[f][feMkIn]);
			return true;
		}
		if (Marker_InRange(playerid, gFDoor[f][feMkIn], DOOR_RANGE))
		{
			if (Law_TryLeave(playerid)) Door_SpawnAt(playerid, gFDoor[f][feMkOut]);
			return true;
		}
	}
	return false;
}

stock bool:Cmd_SetFInt(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f, side[8];
	if (!NextIntParam(params, pos, f) || !Faction_IsValid(f))
	{
		SendClientMessage(playerid, COLOR_GREY, "Использование: /setfint [фракция 1-%d] [out/in — необязательно]", MAX_FACTIONS - 1);
		SendClientMessage(playerid, COLOR_GREY, "На улице у двери — вход, внутри интерьера — выход. Вставай спиной к двери. В интерьер: /gotoint");
		return true;
	}
	new bool:inside = GetPlayerInterior(playerid) != 0;
	if (NextParam(params, pos, side))
	{
		if (!strcmp(side, "in", true)) inside = true;
		else if (!strcmp(side, "out", true)) inside = false;
		else return SendClientMessage(playerid, COLOR_GREY, "Сторона: out (снаружи) или in (внутри).");
	}
	new Float:x, Float:y, Float:z, Float:a;
	GetPlayerPos(playerid, x, y, z);
	GetPlayerFacingAngle(playerid, a);
	if (inside)
	{
		gFDoor[f][feIn] = true;
		gFDoor[f][feIX] = x; gFDoor[f][feIY] = y; gFDoor[f][feIZ] = z; gFDoor[f][feIA] = a;
		gFDoor[f][feIInt] = GetPlayerInterior(playerid);
		gFDoor[f][feIWorld] = GetPlayerVirtualWorld(playerid);
	}
	else
	{
		gFDoor[f][feOut] = true;
		gFDoor[f][feOX] = x; gFDoor[f][feOY] = y; gFDoor[f][feOZ] = z; gFDoor[f][feOA] = a;
		gFDoor[f][feOInt] = GetPlayerInterior(playerid);
		gFDoor[f][feOWorld] = GetPlayerVirtualWorld(playerid);
	}
	FDoor_Save(f);
	FDoor_ForgetMarkers(f);
	FDoor_Build(f);
	if (inside) SendClientMessage(playerid, COLOR_ADMIN, "«%s»: точка внутри (выход) сохранена.", Faction_GetName(f));
	else SendClientMessage(playerid, COLOR_ADMIN, "«%s»: точка снаружи (вход) сохранена.", Faction_GetName(f));
	if (!gFDoor[f][feIn]) SendClientMessage(playerid, COLOR_GREY, "Осталась точка внутри: зайди в интерьер (/gotoint) и введи /setfint %d ещё раз.", f);
	else if (!gFDoor[f][feOut]) SendClientMessage(playerid, COLOR_GREY, "Осталась точка снаружи: встань у двери на улице и введи /setfint %d ещё раз.", f);
	else SendClientMessage(playerid, COLOR_ADMIN, "Вход работает: ALT у стрелки. Раздевалку и спавн можно поставить внутри: /setfduty %d, /setfspawn %d", f, f);
	return true;
}

stock bool:Cmd_DelFInt(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f;
	if (!NextIntParam(params, pos, f) || !Faction_IsValid(f))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /delfint [фракция 1-%d] — убрать вход в здание фракции", MAX_FACTIONS - 1);
	gFDoor[f][feOut] = false;
	gFDoor[f][feIn] = false;
	gFDoor[f][feClosed] = false;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM faction_doors WHERE faction = %d", f));
	FDoor_ForgetMarkers(f);
	FDoor_Build(f);
	SendClientMessage(playerid, COLOR_ADMIN, "Вход в здание фракции «%s» убран.", Faction_GetName(f));
	if (Faction[f][fSpawnInt] != 0 || Faction[f][fDutyInt] != 0)
		SendClientMessage(playerid, COLOR_GREY, "Спавн или раздевалка этой фракции стоят внутри здания: перенеси их (/setfspawn, /setfduty).");
	return true;
}

stock bool:Cmd_FIntLock(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f;
	if (!NextIntParam(params, pos, f) || !Faction_IsValid(f))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /fintlock [фракция 1-%d] — пускать в здание только своих (переключатель)", MAX_FACTIONS - 1);
	if (!gFDoor[f][feOut] || !gFDoor[f][feIn]) return SendClientMessage(playerid, COLOR_GREY, "У этой фракции нет входа. Поставить: /setfint");
	gFDoor[f][feClosed] = !gFDoor[f][feClosed];
	FDoor_Save(f);
	FDoor_Build(f);
	if (gFDoor[f][feClosed]) SendClientMessage(playerid, COLOR_ADMIN, "«%s»: вход теперь только для своих.", Faction_GetName(f));
	else SendClientMessage(playerid, COLOR_ADMIN, "«%s»: вход теперь открыт для всех.", Faction_GetName(f));
	return true;
}

// =====================================================================
//  Создание и удаление фракций в игре (админ 4+)
//  /createfaction [тип] [название] — новая фракция; спавн и раздевалка там, где стоишь
//  /delfaction [№] confirm         — удалить созданную в игре фракцию со всем её имуществом
//  /flist                          — список фракций
//  Дальше фракция настраивается как обычная: /setfspawn, /setfduty, /setfint,
//  /addfcar, /fcolor, /fedit, /makeleader; ранги, скины и зарплаты — лидер в /lmenu.
// =====================================================================
static const gFTypeName[9][24] =
{
	"нет", "силовая структура", "медики", "банда", "гос. организация", "армия", "тюрьма", "спецслужба", "правительство"
};
static const gFTypeColor[9] = { 0xFFFFFFFF, 0x3A6EFFFF, 0xFF8080FF, 0xE0A030FF, 0xC8C8C8FF, 0x6B8E23FF, 0xD2691EFF, 0x2F4F7FFF, 0xE8E8E8FF };
static const gFTypeSkin[9]  = { 0, 280, 274, 21, 17, 287, 71, 286, 147 };

stock bool:Cmd_CreateFaction(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, type, name[32];
	if (!NextIntParam(params, pos, type) || type < FTYPE_LAW || type > FTYPE_GOV || type == FTYPE_ARMY || !RestParam(params, pos, name))
	{
		SendClientMessage(playerid, COLOR_GREY, "Использование: /createfaction [тип] [название]");
		SendClientMessage(playerid, COLOR_GREY, "Типы: 1 — силовая (права полиции), 2 — медики, 3 — банда, 4 — гос. организация без особых прав, 6 — тюрьма (охрана),");
		SendClientMessage(playerid, COLOR_GREY, "7 — спецслужба (как ФБР: маскировка, прослушка, слежка, штатское), 8 — правительство (/gov, налоги). Сменить тип: /setftype");
		return true;
	}
	if (!Faction_IsGoodName(name, 2, 31)) return SendClientMessage(playerid, COLOR_GREY, "Название: от 2 до 31 символа, без фигурных скобок и знака процента.");

	new f = 0;
	for (new i = FACTION_BUILTIN; i < MAX_FACTIONS; i++)
	{
		if (Faction[i][fType] == FTYPE_NONE) { f = i; break; }
	}
	if (f == 0) return SendClientMessage(playerid, COLOR_GREY, "Свободных номеров нет (всего фракций может быть %d). Удалить лишнюю: /delfaction", MAX_FACTIONS - 1);

	Faction_WipeRows(f);    // на всякий случай: остатки прежней фракции с этим номером
	Faction[f][fType] = type;
	Faction[f][fName][0] = '\0';
	strcat(Faction[f][fName], name, 32);
	Faction[f][fColor] = gFTypeColor[type];
	Faction[f][fBank] = Faction_IsState(f) ? FACTION_START_BANK : 0;
	Faction[f][fMats] = Faction_IsState(f) ? FACTION_START_MATS : 0;
	Faction[f][fStoreRank] = 2;
	Faction[f][fMarker] = false;

	// спавн и раздевалка — там, где стоит админ; потом их можно переставить
	GetPlayerPos(playerid, Faction[f][fSpawnX], Faction[f][fSpawnY], Faction[f][fSpawnZ]);
	GetPlayerFacingAngle(playerid, Faction[f][fSpawnA]);
	Faction[f][fSpawnInt] = GetPlayerInterior(playerid);
	Faction[f][fSpawnWorld] = GetPlayerVirtualWorld(playerid);
	Faction[f][fDutyX] = Faction[f][fSpawnX];
	Faction[f][fDutyY] = Faction[f][fSpawnY];
	Faction[f][fDutyZ] = Faction[f][fSpawnZ];
	Faction[f][fDutyInt] = Faction[f][fSpawnInt];
	Faction[f][fDutyWorld] = Faction[f][fSpawnWorld];

	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"INSERT OR REPLACE INTO factions (id, sx, sy, sz, sa, dx, dy, dz, dint, dworld, sint, sworld, name, color, bank, store_rank, ftype, mats) VALUES (%d, %f, %f, %f, %f, %f, %f, %f, %d, %d, %d, %d, '%q', %d, %d, %d, %d, %d)",
		f, Faction[f][fSpawnX], Faction[f][fSpawnY], Faction[f][fSpawnZ], Faction[f][fSpawnA],
		Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ], Faction[f][fDutyInt], Faction[f][fDutyWorld],
		Faction[f][fSpawnInt], Faction[f][fSpawnWorld], Faction[f][fName], Faction[f][fColor], Faction[f][fBank],
		Faction[f][fStoreRank], type, Faction[f][fMats]));

	new t = Faction_RankSet(f);
	for (new rk = 0; rk < MAX_RANK; rk++)
	{
		gRankName[f][rk][0] = '\0';
		strcat(gRankName[f][rk], gDefRankNames[t][rk], 24);
		gRankSkin[f][rk] = gFTypeSkin[type];
		gRankSalary[f][rk] = Faction_IsState(f) ? (FACTION_SALARY_BASE + (rk + 1) * FACTION_SALARY_RANK) : 0;
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO faction_ranks (faction, rank, name, skin, salary) VALUES (%d, %d, '%q', %d, %d)",
			f, rk + 1, gRankName[f][rk], gRankSkin[f][rk], gRankSalary[f][rk]));
	}
	for (new it = 0; it < MAX_FITEMS; it++) gFStore[f][it] = 0;

	gFDoor[f][feOut] = false;
	gFDoor[f][feIn] = false;
	gFDoor[f][feClosed] = false;
	Faction_CreateMarker(f);

	new msg[144];
	format(msg, sizeof msg, "Администратор %s создал фракцию (%s).", GetRPName(playerid), gFTypeName[type]);
	Faction_Log(f, msg);
	SendClientMessage(playerid, COLOR_ADMIN, "Создана фракция №%d «%s» (%s). Спавн и раздевалка стоят здесь.", f, Faction[f][fName], gFTypeName[type]);
	SendClientMessage(playerid, COLOR_ADMIN, "Дальше: /setfspawn %d, /setfduty %d, /setfint %d, /addfcar %d, /fcolor %d, /makeleader [id] %d", f, f, f, f, f, f);
	return true;
}

// Стереть из базы всё, что относится к фракции f
stock Faction_WipeRows(f)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM factions WHERE id = %d", f));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM faction_ranks WHERE faction = %d", f));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM faction_storage WHERE faction = %d", f));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM faction_logs WHERE faction = %d", f));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM faction_doors WHERE faction = %d", f));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM fvehicles WHERE faction = %d", f));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET faction = 0, frank = 0, fwarns = 0, fperm_on = 0, fperm_off = 0, fnote = NULL WHERE faction = %d", f));
	FPerm_ResetFaction(f);  // права рангов по умолчанию (fmanage.inc)
	Org_Reset(f);           // настройки владельца (orgs.inc)
}

stock bool:Cmd_DelFaction(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f, word[16];
	if (!NextIntParam(params, pos, f) || f < FACTION_BUILTIN || !Faction_IsValid(f))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /delfaction [№] confirm — удалить можно только фракцию, созданную в игре (№%d-%d). Список: /flist", FACTION_BUILTIN, MAX_FACTIONS - 1);
	if (!NextParam(params, pos, word) || strcmp(word, "confirm", true) != 0)
	{
		SendClientMessage(playerid, COLOR_GREY, "Фракция «%s» будет удалена навсегда: состав, общак, склад, машины, вход.", Faction_GetName(f));
		SendClientMessage(playerid, COLOR_GREY, "Если уверен, напиши: /delfaction %d confirm", f);
		return true;
	}

	new name[32];
	strcat(name, Faction[f][fName], 32);

	// 1. члены фракции онлайн (оффлайн — строкой в базе, см. Faction_WipeRows)
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || Player[i][pFaction] != f) continue;
		Faction_SetMember(i, FACTION_NONE, 0);
		SendClientMessage(i, COLOR_ADMIN, "Фракция «%s» расформирована администрацией.", name);
	}
	// 2. машины
	for (new v = 1; v < MAX_VEHICLES; v++)
	{
		if (gVehFaction[v] != f || !IsValidVehicle(v)) continue;
		Vehicle_Clear(v);
		DestroyVehicle(v);
	}
	// 3. районы, вход в здание, значок раздевалки
	Turfs_FreeFaction(f);
	Factories_FreeFaction(f);   // её заводы остаются без владельца
	FStore_Remove(f);
	gFDoor[f][feOut] = false;
	gFDoor[f][feIn] = false;
	gFDoor[f][feClosed] = false;
	FDoor_Build(f);
	FDoor_ForgetMarkers(f);
	if (Faction[f][fMarker])
	{
		Marker_Destroy(Faction[f][fMk], true);
		Faction[f][fMarker] = false;
	}
	// 4. база
	Faction_WipeRows(f);
	Faction[f][fType] = FTYPE_NONE;
	Faction[f][fBank] = 0;
	Faction[f][fMats] = 0;
	for (new it = 0; it < MAX_FITEMS; it++) gFStore[f][it] = 0;

	SendClientMessage(playerid, COLOR_ADMIN, "Фракция №%d «%s» удалена.", f, name);
	return true;
}

stock bool:Cmd_FList(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	SendClientMessage(playerid, COLOR_ADMIN, "Фракции (№ — название, тип, общак, материалы):");
	for (new f = 1; f < MAX_FACTIONS; f++)
	{
		if (!Faction_IsValid(f)) continue;
		if (Faction_IsHidden(f) && Player[playerid][pAdmin] < ADMIN_MAX_LEVEL) continue;
		SendClientMessage(playerid, Faction_GetColor(f), " %d — %s, %s, $%d, материалов %d%s", f, Faction_GetName(f),
			gFTypeName[Faction[f][fType]], Faction[f][fBank], Faction[f][fMats], Faction_IsHidden(f) ? (" {AAAAAA}(скрыта)") : (""));
	}
	return true;
}

// =====================================================================
//  Материалы: заводы, ящики, склады организаций, кража, захват, чёрный рынок, чаты
//
//  Заводы ставит админ: /addfactory [вид] [название], точки — /setfpoint [№] raw|bench|store|owner.
//    вид 1 — военный (на военной части): работает и берёт ящики только владелец, захватить нельзя;
//    вид 2 — обычный: его захватывают. Если владелец — госструктура, работать может любой
//            (кроме банд); если банда — только её члены.
//  Работа (всё по ALT): взять сырьё -> станок (анимация, в руках появляется ящик) -> отнести
//  ящик на склад завода. За ящик платят FACTORY_PAY, на склад завода падает CRATE_MATS материалов.
//
//  Перевозка: на складе завода владелец (а у госзавода ещё и армия) берёт ящик, встаёт сзади
//  грузовика или фургона и кладёт (ALT). За рулём на складе организации — H или /unload.
//  Разгружать можно на складе своей организации; армия — ещё и на складах госструктур (ей платят).
//  Машину с ящиками может угнать член другой организации и разгрузить у себя — это перехват.
//
//  Склад организации ставит админ: /setfstore [фракция] (у армии это /setarmystore).
//  ALT на нём пешком: положить материалы из сумки, взять ящик для перевозки.
//  Банды на складе армии: ALT — и пока стоишь на маркере, в сумку идёт 1-5 материалов в секунду.
//
//  Материалы лежат в сумке (INV_MATS, needs.inc). «Использовать» — сделать оружие: это может любой.
//
//  Захват (вид 2): когда рядом нет живых членов организации-владельца, ранг FACTORY_CAPTURE_RANK+
//  жмёт ALT на маркере завода и стоит FACTORY_CAPTURE_TIME секунд. Завод переходит его организации
//  и работает, материалы на складе остаются. Ранг 9+ сразу получает выбор: оставить или закрыть
//  (позже — ALT на маркере). Закрытый завод перестаёт быть заводом: точки пропадают, остаётся
//  только склад, пока с него не вывезут ящики. Вернуть завод может админ: /setfowner.
//
//  Чёрный рынок: торговцы (/addbm, /delbm), ALT или /bm рядом. Цены: /bmprice.
//  /d — общий чат госструктур, /gd — общий чат банд.
// =====================================================================
forward Supply_OnSecond();
forward Factory_OnBenchDone(playerid);
forward bool:Cmd_BM(playerid);
forward bool:Wound_IsWounded(playerid);    // wounded.inc

#define CRATE_MATS              50      // материалов в одном ящике
#define CRATE_MODEL             1271
#define CRATE_SLOT              8       // слот прикреплённого объекта (9 занят работами)
#define CARGO_TRUCK             1000    // материалов в грузовик (20 ящиков)
#define CARGO_VAN               300     // материалов в фургон или пикап (6 ящиков)
#define FACTORY_PAY             15      // рабочему за ящик (один круг — около 40 секунд)
#define FACTORY_BENCH_TIME      8       // секунд у станка
#define FACTORY_STOCK_MAX       20000   // сколько материалов вмещает склад завода
#define FACTORY_POINT_RANGE     2.5
#define FACTORY_CLEAR_RANGE     60.0    // в этом радиусе от маркера не должно быть живых защитников
#define FACTORY_CAPTURE_TIME    60      // секунд стоять на маркере
#define FACTORY_CAPTURE_RANK    7
#define FACTORY_CAPTURE_PAUSE   1800    // пауза между захватами одного завода, секунд
#define MAX_FACTORIES           30
#define FKIND_MILITARY          1
#define FKIND_COMMON            2
#define STORE_REF               300     // ref маркера склада организации: 300 + фракция
#define FACTORY_REF             400     // ref маркеров завода: 400 + номер завода * 4 + точка

enum
{
	FP_OWNER,           // маркер завода: владелец, захват
	FP_RAW,             // сырьё
	FP_BENCH,           // станок
	FP_STORE,           // склад ящиков
	MAX_FPOINTS
}
enum E_FACTORY
{
	bool:fcUsed,
	fcDbId,
	fcName[32],
	fcKind,
	fcOwner,            // фракция-владелец (0 — нет)
	bool:fcClosed,      // оборудование уничтожено
	fcStock,            // материалов на складе завода
	fcLastCap,          // когда захватывали в последний раз
	fcCapBy             // кто сейчас захватывает
}
new Factory[MAX_FACTORIES][E_FACTORY];
new bool:gFcSet[MAX_FACTORIES][MAX_FPOINTS];
new Float:gFcX[MAX_FACTORIES][MAX_FPOINTS];
new Float:gFcY[MAX_FACTORIES][MAX_FPOINTS];
new Float:gFcZ[MAX_FACTORIES][MAX_FPOINTS];
new gFcW[MAX_FACTORIES][MAX_FPOINTS];
new gFcI[MAX_FACTORIES][MAX_FPOINTS];
new gFcMk[MAX_FACTORIES][MAX_FPOINTS];
new gFcSel[MAX_PLAYERS];
new gFcList[MAX_PLAYERS][MAX_FACTIONS];     // кому можно передать завод (строки окна)

// Склады организаций (/setfstore). Не задан — складом считается раздевалка.
new bool:gFsSet[MAX_FACTIONS];
new Float:gFsX[MAX_FACTIONS];
new Float:gFsY[MAX_FACTIONS];
new Float:gFsZ[MAX_FACTIONS];
new gFsW[MAX_FACTIONS];
new gFsI[MAX_FACTIONS];
new gFsMk[MAX_FACTIONS];

// ---------------------------------------------------------------------
//  Точки снабжения: склад армии и торговец чёрного рынка
// ---------------------------------------------------------------------
stock SPoint_Save(i)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO supply_points (id, x, y, z, a, world, interior) VALUES (%d, %f, %f, %f, %f, %d, %d)",
		i, gSpX[i], gSpY[i], gSpZ[i], gSpA[i], gSpWorld[i], gSpInt[i]));
}

// Поставить маркер точки (заново)
stock SPoint_Build(i)
{
	if (i == SP_FACTORY || i == SP_BM) return;    // заводы — /addfactory, торговцы — /addbm
	if (gSpMk[i] != -1) Marker_Destroy(gSpMk[i]);
	if (i == SP_ARMY)
	{
		gSpMk[i] = Marker_Create(MK_FACTION, SUPPLY_REF + i, "Склад армии", 1242, gSpX[i], gSpY[i], gSpZ[i], gSpWorld[i], gSpInt[i], GPS_FACTIONS, gSpA[i]);
		Marker_SetLabel(gSpMk[i], "Склад армии\n{FFFFFF}ALT — материалы и ящики\nЗа рулём с грузом: H — разгрузить", 0x6B8E23FF);
	}
	else
	{
		gSpMk[i] = Marker_Create(MK_FACTION, SUPPLY_REF + i, "Чёрный рынок", 0, gSpX[i], gSpY[i], gSpZ[i], gSpWorld[i], gSpInt[i], GPS_NONE, gSpA[i]);
		Marker_SetLabel(gSpMk[i], "Торговец\n{FFFFFF}ALT — посмотреть товар", 0xC0A060FF, 10.0, 1.2);
	}
	// маркер мог быть сдвинут в /markers: берём то, что получилось
	gSpA[i] = Marker_GetAngle(gSpMk[i]);
	gSpWorld[i] = Marker_GetWorld(gSpMk[i]);
	gSpInt[i] = Marker_GetInterior(gSpMk[i]);
}

// Стоит ли игрок у точки снабжения
stock Supply_AtPoint(playerid, i, Float:range)
{
	if (GetPlayerVirtualWorld(playerid) != gSpWorld[i] || GetPlayerInterior(playerid) != gSpInt[i]) return 0;
	return IsPlayerInRangeOfPoint(playerid, range, gSpX[i], gSpY[i], gSpZ[i]) ? 1 : 0;
}

// /setarmystore, /setbm — перенести точку туда, где стоит админ
stock bool:Cmd_SetSPoint(playerid, i)
{
	if (!Admin_Check(playerid, 4)) return true;
	if (i != SP_ARMY) return true;
	GetPlayerPos(playerid, gSpX[i], gSpY[i], gSpZ[i]);
	GetPlayerFacingAngle(playerid, gSpA[i]);
	gSpWorld[i] = GetPlayerVirtualWorld(playerid);
	gSpInt[i] = GetPlayerInterior(playerid);
	SPoint_Save(i);
	if (gSpMk[i] != -1)
	{
		Marker_Destroy(gSpMk[i], true);     // прежние правки из /markers больше не нужны
		gSpMk[i] = -1;
	}
	SPoint_Build(i);
	SendClientMessage(playerid, COLOR_ADMIN, "Склад армии перенесён сюда. Чтобы сюда заезжали грузовики, ставь его на улице.");
	return true;
}

// ---------------------------------------------------------------------
//  Склады организаций
// ---------------------------------------------------------------------
stock FStore_Get(f, &Float:x, &Float:y, &Float:z, &world, &interior)
{
	if (Faction_IsArmy(f))
	{
		x = gSpX[SP_ARMY]; y = gSpY[SP_ARMY]; z = gSpZ[SP_ARMY];
		world = gSpWorld[SP_ARMY]; interior = gSpInt[SP_ARMY];
	}
	else if (gFsSet[f])
	{
		x = gFsX[f]; y = gFsY[f]; z = gFsZ[f];
		world = gFsW[f]; interior = gFsI[f];
	}
	else
	{
		x = Faction[f][fDutyX]; y = Faction[f][fDutyY]; z = Faction[f][fDutyZ];
		world = Faction[f][fDutyWorld]; interior = Faction[f][fDutyInt];
	}
}

stock FStore_At(playerid, f, Float:range)
{
	new Float:x, Float:y, Float:z, world, interior;
	FStore_Get(f, x, y, z, world, interior);
	if (GetPlayerVirtualWorld(playerid) != world || GetPlayerInterior(playerid) != interior) return 0;
	return IsPlayerInRangeOfPoint(playerid, range, x, y, z) ? 1 : 0;
}

stock FStore_Build(f)
{
	if (gFsMk[f] != -1) { Marker_Destroy(gFsMk[f]); gFsMk[f] = -1; }
	if (!gFsSet[f] || !Faction_IsValid(f)) return;
	new text[MK_TEXT_LEN], name[32];
	strcat(name, "Склад: ");
	strcat(name, Faction_GetName(f), 32);
	gFsMk[f] = Marker_Create(MK_FACTION, STORE_REF + f, name, 1242, gFsX[f], gFsY[f], gFsZ[f], gFsW[f], gFsI[f], GPS_NONE);
	format(text, sizeof text, "Склад: %s\n{FFFFFF}ALT — материалы и ящики\nЗа рулём с грузом: H — разгрузить", Faction_GetName(f));
	Marker_SetLabel(gFsMk[f], text, Faction_GetColor(f));
	Marker_SetFaction(gFsMk[f], Faction_MarkerOwner(f));
}

stock FStore_Save(f)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO faction_stores (faction, x, y, z, world, interior) VALUES (%d, %f, %f, %f, %d, %d)",
		f, gFsX[f], gFsY[f], gFsZ[f], gFsW[f], gFsI[f]));
}

// Фракцию удалили
stock FStore_Remove(f)
{
	if (gFsMk[f] != -1) { Marker_Destroy(gFsMk[f], true); gFsMk[f] = -1; }
	gFsSet[f] = false;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM faction_stores WHERE faction = %d", f));
}

stock bool:Cmd_SetFStore(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f;
	if (!NextIntParam(params, pos, f) || !Faction_IsValid(f))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setfstore [фракция] — склад организации здесь: сюда разгружают машины и кладут материалы. Номера: /flist");
	if (Faction_IsArmy(f)) return Cmd_SetSPoint(playerid, SP_ARMY);
	GetPlayerPos(playerid, gFsX[f], gFsY[f], gFsZ[f]);
	gFsW[f] = GetPlayerVirtualWorld(playerid);
	gFsI[f] = GetPlayerInterior(playerid);
	gFsSet[f] = true;
	FStore_Save(f);
	if (gFsMk[f] != -1) { Marker_Destroy(gFsMk[f], true); gFsMk[f] = -1; }
	FStore_Build(f);
	SendClientMessage(playerid, COLOR_ADMIN, "Склад фракции «%s» теперь здесь. Чтобы сюда заезжали грузовики, ставь его на улице.", Faction_GetName(f));
	return true;
}

// ---------------------------------------------------------------------
//  Заводы: данные
// ---------------------------------------------------------------------
stock Factory_Find(dbid)
{
	for (new i = 0; i < MAX_FACTORIES; i++)
	{
		if (Factory[i][fcUsed] && Factory[i][fcDbId] == dbid) return i;
	}
	return -1;
}

stock Factory_Save(i)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE factories SET name = '%q', kind = %d, owner = %d, closed = %d, stock = %d WHERE id = %d",
		Factory[i][fcName], Factory[i][fcKind], Factory[i][fcOwner], Factory[i][fcClosed] ? 1 : 0, Factory[i][fcStock], Factory[i][fcDbId]));
}

stock Factory_SavePoint(i, p)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO factory_points (factory, point, x, y, z, world, interior) VALUES (%d, %d, %f, %f, %f, %d, %d)",
		Factory[i][fcDbId], p, gFcX[i][p], gFcY[i][p], gFcZ[i][p], gFcW[i][p], gFcI[i][p]));
}

// Подпись точки завода
stock Factory_Label(i, p)
{
	if (gFcMk[i][p] == -1) return;
	new text[MK_TEXT_LEN];
	switch (p)
	{
		case FP_OWNER:
		{
			if (Factory[i][fcOwner] != FACTION_NONE) format(text, sizeof text, "%s\n{FFFFFF}Владелец: %s", Factory[i][fcName], Faction_GetName(Factory[i][fcOwner]));
			else format(text, sizeof text, "%s\n{FFFFFF}Владельца нет", Factory[i][fcName]);
			if (Factory[i][fcClosed]) strcat(text, "\n{FF6347}Закрыт: оборудование уничтожено");
			if (Factory[i][fcKind] == FKIND_MILITARY) strcat(text, "\n{AFAFAF}Военный завод");
			else strcat(text, "\n{AFAFAF}ALT — захват (ранг 7+)");
		}
		case FP_RAW: strcat(text, "Сырьё\n{FFFFFF}ALT — взять");
		case FP_BENCH: strcat(text, "Станок\n{FFFFFF}ALT — сделать ящик");
		default:
		{
			if (Factory[i][fcClosed]) format(text, sizeof text, "Склад закрытого завода\n{FFFFFF}Осталось ящиков: %d\nALT — забрать ящик", Factory[i][fcStock] / CRATE_MATS);
			else format(text, sizeof text, "Склад завода\n{FFFFFF}Ящиков: %d\nALT — сдать или взять ящик", Factory[i][fcStock] / CRATE_MATS);
		}
	}
	Marker_SetLabel(gFcMk[i][p], text, 0xE0A030FF);
}

stock Factory_BuildPoint(i, p)
{
	if (gFcMk[i][p] != -1) { Marker_Destroy(gFcMk[i][p]); gFcMk[i][p] = -1; }
	if (!Factory[i][fcUsed] || !gFcSet[i][p]) return;
	// Закрытый завод — просто интерьер: точек нет. Склад виден, пока с него не вывезли ящики.
	if (Factory[i][fcClosed] && (p != FP_STORE || Factory[i][fcStock] < CRATE_MATS)) return;
	new model = 1239, gps = GPS_NONE;
	if (p == FP_OWNER) { model = 1274; gps = GPS_FACTIONS; }
	gFcMk[i][p] = Marker_Create(MK_FACTION, FACTORY_REF + Factory[i][fcDbId] * 4 + p, Factory[i][fcName], model,
		gFcX[i][p], gFcY[i][p], gFcZ[i][p], gFcW[i][p], gFcI[i][p], gps);
	Factory_Label(i, p);
}

stock Factory_At(playerid, i, p, Float:range)
{
	if (!gFcSet[i][p]) return 0;
	if (GetPlayerVirtualWorld(playerid) != gFcW[i][p] || GetPlayerInterior(playerid) != gFcI[i][p]) return 0;
	return IsPlayerInRangeOfPoint(playerid, range, gFcX[i][p], gFcY[i][p], gFcZ[i][p]) ? 1 : 0;
}

// Админ сдвинул маркер в /markers (из Faction_OnMarkerMoved)
stock Factory_OnMarkerMoved(ref, Float:x, Float:y, Float:z)
{
	if (ref >= FACTORY_REF)
	{
		new k = ref - FACTORY_REF;
		new i = Factory_Find(k / 4), p = k % 4;
		if (i == -1) return;
		gFcX[i][p] = x;
		gFcY[i][p] = y;
		gFcZ[i][p] = z;
		Factory_SavePoint(i, p);
		return;
	}
	new f = ref - STORE_REF;
	if (f < 1 || f >= MAX_FACTIONS || !gFsSet[f]) return;
	gFsX[f] = x;
	gFsY[f] = y;
	gFsZ[f] = z;
	FStore_Save(f);
}

stock Factories_Load()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS factories (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, kind INTEGER DEFAULT 2, owner INTEGER DEFAULT 0, closed INTEGER DEFAULT 0, stock INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS factory_points (factory INTEGER NOT NULL, point INTEGER NOT NULL, x REAL DEFAULT 0, y REAL DEFAULT 0, z REAL DEFAULT 0, world INTEGER DEFAULT 0, interior INTEGER DEFAULT 0, PRIMARY KEY (factory, point))"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS faction_stores (faction INTEGER PRIMARY KEY, x REAL DEFAULT 0, y REAL DEFAULT 0, z REAL DEFAULT 0, world INTEGER DEFAULT 0, interior INTEGER DEFAULT 0)"));

	for (new i = 0; i < MAX_FACTORIES; i++)
	{
		Factory[i][fcUsed] = false;
		Factory[i][fcCapBy] = INVALID_PLAYER_ID;
		for (new p = 0; p < MAX_FPOINTS; p++)
		{
			gFcSet[i][p] = false;
			gFcMk[i][p] = -1;
		}
	}
	for (new f = 0; f < MAX_FACTIONS; f++)
	{
		gFsSet[f] = false;
		gFsMk[f] = -1;
	}

	new n = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, name, kind, owner, closed, stock FROM factories");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new k = 0; k < rows && n < MAX_FACTORIES; k++)
		{
			Factory[n][fcUsed] = true;
			Factory[n][fcDbId] = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 1, Factory[n][fcName], 32);
			Factory[n][fcKind] = DB_GetFieldInt(r, 2);
			Factory[n][fcOwner] = DB_GetFieldInt(r, 3);
			if (!Faction_IsValid(Factory[n][fcOwner])) Factory[n][fcOwner] = FACTION_NONE;
			Factory[n][fcClosed] = DB_GetFieldInt(r, 4) != 0;
			Factory[n][fcStock] = DB_GetFieldInt(r, 5);
			Factory[n][fcLastCap] = 0;
			n++;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	r = DB_ExecuteQuery(gDB, "SELECT factory, point, x, y, z, world, interior FROM factory_points");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new k = 0; k < rows; k++)
		{
			new i = Factory_Find(DB_GetFieldInt(r, 0)), p = DB_GetFieldInt(r, 1);
			if (i != -1 && p >= 0 && p < MAX_FPOINTS)
			{
				gFcSet[i][p] = true;
				gFcX[i][p] = DB_GetFieldFloat(r, 2);
				gFcY[i][p] = DB_GetFieldFloat(r, 3);
				gFcZ[i][p] = DB_GetFieldFloat(r, 4);
				gFcW[i][p] = DB_GetFieldInt(r, 5);
				gFcI[i][p] = DB_GetFieldInt(r, 6);
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	for (new i = 0; i < MAX_FACTORIES; i++)
	{
		if (!Factory[i][fcUsed]) continue;
		for (new p = 0; p < MAX_FPOINTS; p++) Factory_BuildPoint(i, p);
	}

	r = DB_ExecuteQuery(gDB, "SELECT faction, x, y, z, world, interior FROM faction_stores");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new k = 0; k < rows; k++)
		{
			new f = DB_GetFieldInt(r, 0);
			if (Faction_IsValid(f))
			{
				gFsSet[f] = true;
				gFsX[f] = DB_GetFieldFloat(r, 1);
				gFsY[f] = DB_GetFieldFloat(r, 2);
				gFsZ[f] = DB_GetFieldFloat(r, 3);
				gFsW[f] = DB_GetFieldInt(r, 4);
				gFsI[f] = DB_GetFieldInt(r, 5);
				FStore_Build(f);
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Factions] %d factories.", n);
}

// Фракцию удалили: её заводы остаются без владельца
stock Factories_FreeFaction(f)
{
	for (new i = 0; i < MAX_FACTORIES; i++)
	{
		if (!Factory[i][fcUsed] || Factory[i][fcOwner] != f) continue;
		Factory[i][fcOwner] = FACTION_NONE;
		Factory_Save(i);
		Factory_Label(i, FP_OWNER);
	}
}

stock Supply_Init()
{
	static bool:started = false;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS supply_points (id INTEGER PRIMARY KEY, x REAL DEFAULT 0, y REAL DEFAULT 0, z REAL DEFAULT 0, a REAL DEFAULT 0, world INTEGER DEFAULT 0, interior INTEGER DEFAULT 0)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS bm_prices (item INTEGER PRIMARY KEY, price INTEGER DEFAULT 0)"));

	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, x, y, z, a, world, interior FROM supply_points");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new n = 0; n < rows; n++)
		{
			new i = DB_GetFieldInt(r, 0);
			if (i >= 0 && i < MAX_SPOINTS)
			{
				gSpX[i] = DB_GetFieldFloat(r, 1);
				gSpY[i] = DB_GetFieldFloat(r, 2);
				gSpZ[i] = DB_GetFieldFloat(r, 3);
				gSpA[i] = DB_GetFieldFloat(r, 4);
				gSpWorld[i] = DB_GetFieldInt(r, 5);
				gSpInt[i] = DB_GetFieldInt(r, 6);
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}

	// Цены чёрного рынка по умолчанию — втрое дороже закупки для фракций
	for (new it = 0; it < MAX_FITEMS; it++) gBMPrice[it] = gFItemPrice[it] * 3;
	gBMMatPrice = BM_MAT_PRICE;
	r = DB_ExecuteQuery(gDB, "SELECT item, price FROM bm_prices");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new n = 0; n < rows; n++)
		{
			new it = DB_GetFieldInt(r, 0), price = DB_GetFieldInt(r, 1);
			if (price < 0) price = 0;
			if (it == -1) gBMMatPrice = price;
			else if (it >= 0 && it < MAX_FITEMS) gBMPrice[it] = price;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}

	for (new i = 0; i < MAX_SPOINTS; i++) SPoint_Build(i);
	BM_Load();
	Factories_Load();
	if (!started)
	{
		started = true;
		SetTimer("Supply_OnSecond", 1000, true);
	}
}

// ---------------------------------------------------------------------
//  Ящик в руках
// ---------------------------------------------------------------------
stock Crate_Give(playerid, kind, fc)
{
	gCrate[playerid] = kind;
	gCrateFc[playerid] = fc;
	SetPlayerAttachedObject(playerid, CRATE_SLOT, CRATE_MODEL, 1, 0.11, 0.36, 0.0, 0.0, 90.0, 0.0, 0.6, 0.6, 0.6);
	SetPlayerSpecialAction(playerid, SPECIAL_ACTION_CARRY);
}

stock Crate_Drop(playerid)
{
	if (gCrate[playerid] == 0) return;
	gCrate[playerid] = 0;
	if (!IsPlayerConnected(playerid)) return;
	RemovePlayerAttachedObject(playerid, CRATE_SLOT);
	if (GetPlayerSpecialAction(playerid) == SPECIAL_ACTION_CARRY) SetPlayerSpecialAction(playerid, SPECIAL_ACTION_NONE);
}

stock bool:Cmd_DropCrate(playerid)
{
	if (gCrate[playerid] == 0) return SendClientMessage(playerid, COLOR_GREY, "У тебя в руках нет ящика.");
	Crate_Drop(playerid);
	SendClientMessage(playerid, COLOR_GREY, "Ты бросил ящик. Он пропал.");
	return true;
}

// Сколько материалов входит в машину этой модели (0 — ящики в неё не кладут)
stock Cargo_Cap(model)
{
	switch (model)
	{
		case 414, 455, 456, 433, 578, 498, 499, 609, 427, 428: return CARGO_TRUCK;
		case 413, 440, 482, 459, 418, 422, 478, 543, 554, 582: return CARGO_VAN;
	}
	return 0;
}

// Ближайший грузовик или фургон (0 — нет рядом)
stock Cargo_NearVehicle(playerid)
{
	new Float:x, Float:y, Float:z, best = 0, Float:bestDist = 7.0;
	GetPlayerPos(playerid, x, y, z);
	new world = GetPlayerVirtualWorld(playerid);
	for (new v = 1; v < MAX_VEHICLES; v++)
	{
		if (!IsValidVehicle(v) || GetVehicleVirtualWorld(v) != world) continue;
		if (Cargo_Cap(GetVehicleModel(v)) == 0) continue;
		new Float:d = GetVehicleDistanceFromPoint(v, x, y, z);
		if (d < bestDist) { bestDist = d; best = v; }
	}
	return best;
}

// Стоит ли игрок сзади машины
stock Cargo_IsBehind(playerid, v)
{
	new Float:x, Float:y, Float:z, Float:vx, Float:vy, Float:vz, Float:a;
	GetPlayerPos(playerid, x, y, z);
	GetVehiclePos(v, vx, vy, vz);
	GetVehicleZAngle(v, a);
	// вперёд машина смотрит по (sin(-a), cos(-a)); сзади — значит против этого направления
	return ((x - vx) * floatsin(-a, degrees) + (y - vy) * floatcos(-a, degrees) < 0.0) ? 1 : 0;
}

// ---------------------------------------------------------------------
//  Работа на заводе
// ---------------------------------------------------------------------
stock Factory_CanWork(playerid, i)
{
	new owner = Factory[i][fcOwner], f = Player[playerid][pFaction];
	if (Factory[i][fcClosed]) { SendClientMessage(playerid, COLOR_GREY, "Завод закрыт: оборудование уничтожено."); return 0; }
	if (!Faction_IsValid(owner)) { SendClientMessage(playerid, COLOR_GREY, "У завода нет владельца, он не работает."); return 0; }
	if (Factory[i][fcKind] == FKIND_MILITARY || !Faction_IsState(owner))
	{
		if (f != owner) { SendClientMessage(playerid, COLOR_GREY, "Здесь работают только свои: %s.", Faction_GetName(owner)); return 0; }
		return 1;
	}
	// обычный завод у госструктуры: работают простые люди
	if (Faction_IsGang(f)) { SendClientMessage(playerid, COLOR_GREY, "Этот завод держит государство. Бандам тут работы нет."); return 0; }
	return 1;
}

stock Factory_TakeRaw(playerid, i)
{
	if (!Factory_CanWork(playerid, i)) return;
	if (gWorkStage[playerid] == 2) { SendClientMessage(playerid, COLOR_GREY, "Ты уже у станка."); return; }
	if (Factory[i][fcStock] + CRATE_MATS > FACTORY_STOCK_MAX) { SendClientMessage(playerid, COLOR_GREY, "Склад завода забит. Пусть сначала вывезут ящики."); return; }
	gWorkStage[playerid] = 1;
	gWorkFc[playerid] = i;
	ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, false, false, false, false, 0);
	SendClientMessage(playerid, COLOR_WHITE, "Ты взял сырьё. Отнеси его к станку (ALT).");
}

stock Factory_UseBench(playerid, i)
{
	if (!Factory_CanWork(playerid, i)) return;
	if (gWorkStage[playerid] == 2) { SendClientMessage(playerid, COLOR_GREY, "Ты уже работаешь. Подожди."); return; }
	if (gWorkStage[playerid] != 1 || gWorkFc[playerid] != i) { SendClientMessage(playerid, COLOR_GREY, "Сначала возьми сырьё."); return; }
	gWorkStage[playerid] = 2;
	ApplyAnimation(playerid, "BOMBER", "BOM_Plant_Loop", 4.1, true, false, false, false, 0);
	GameTextForPlayer(playerid, "~y~Working...", FACTORY_BENCH_TIME * 1000, 3);
	SetTimerEx("Factory_OnBenchDone", FACTORY_BENCH_TIME * 1000, false, "i", playerid);
}

public Factory_OnBenchDone(playerid)
{
	if (!IsPlayerConnected(playerid) || gWorkStage[playerid] != 2) return 1;
	new i = gWorkFc[playerid];
	gWorkStage[playerid] = 0;
	ClearAnimations(playerid);
	if (!Factory[i][fcUsed] || !Factory_At(playerid, i, FP_BENCH, 5.0) || GetPlayerState(playerid) != PLAYER_STATE_ONFOOT || Wound_IsWounded(playerid))
	{
		SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от станка. Сырьё испорчено.");
		return 1;
	}
	Crate_Give(playerid, 1, i);
	SendClientMessage(playerid, COLOR_WHITE, "Ящик готов. Отнеси его на склад завода (ALT).");
	return 1;
}

stock Factory_TakeCrate(playerid, i)
{
	new owner = Factory[i][fcOwner], f = Player[playerid][pFaction];
	if (!Faction_IsValid(f) || (f != owner && !(Faction_IsState(owner) && Faction_IsArmy(f))))
	{
		SendClientMessage(playerid, COLOR_GREY, "Ящики отсюда берёт владелец завода, а с госзавода ещё и армия. Сюда их сдают рабочие.");
		return;
	}
	if (Factory[i][fcStock] < CRATE_MATS) { SendClientMessage(playerid, COLOR_GREY, "На складе завода нет готовых ящиков."); return; }
	Factory[i][fcStock] -= CRATE_MATS;
	Factory_Save(i);
	Factory_Label(i, FP_STORE);
	if (Factory[i][fcClosed] && Factory[i][fcStock] < CRATE_MATS) Factory_BuildPoint(i, FP_STORE);   // всё вывезли: склад пропадает
	Crate_Give(playerid, 2, i);
	SendClientMessage(playerid, COLOR_WHITE, "Ты взял ящик (%d материалов). Встань сзади грузовика или фургона и положи: ALT.", CRATE_MATS);
}

// ---------------------------------------------------------------------
//  Захват завода
// ---------------------------------------------------------------------
stock Factory_Defenders(i)
{
	new owner = Factory[i][fcOwner], n = 0;
	if (!Faction_IsValid(owner)) return 0;
	for (new p = 0; p < MAX_PLAYERS; p++)
	{
		if (!IsPlayerConnected(p) || !Player[p][pLogged] || Player[p][pFaction] != owner) continue;
		if (Wound_IsWounded(p) || GetPlayerState(p) == PLAYER_STATE_WASTED || GetPlayerState(p) == PLAYER_STATE_SPECTATING) continue;
		if (Factory_At(p, i, FP_OWNER, FACTORY_CLEAR_RANGE)) n++;
	}
	return n;
}

stock Capture_Stop(playerid)
{
	new i = gCapFc[playerid];
	gCapFc[playerid] = -1;
	if (i >= 0 && i < MAX_FACTORIES && Factory[i][fcCapBy] == playerid) Factory[i][fcCapBy] = INVALID_PLAYER_ID;
}

// ALT на маркере завода
stock Factory_OwnerAlt(playerid, i)
{
	new owner = Factory[i][fcOwner], f = Player[playerid][pFaction], msg[144];
	SendClientMessage(playerid, COLOR_WHITE, "Завод «%s». На складе материалов: %d (ящиков: %d).", Factory[i][fcName], Factory[i][fcStock], Factory[i][fcStock] / CRATE_MATS);
	if (!Faction_IsValid(f)) return;
	if (f == owner)
	{
		if (Factory[i][fcClosed]) { SendClientMessage(playerid, COLOR_GREY, "Завод закрыт. Материалы со склада можно вывезти."); return; }
		if (!FPerm_Has(playerid, FP_FACTORY)) return;
		gFcSel[playerid] = i;
		if (Factory[i][fcKind] == FKIND_MILITARY) return;   // военный завод не закрывают и не передают
		// лидер и заместитель: передать завод другой организации или закрыть
		ShowPlayerDialog(playerid, DIALOG_FMANAGE, DIALOG_STYLE_LIST, Factory[i][fcName],
			"Передать завод другой организации\nЗакрыть завод {AFAFAF}(уничтожить оборудование)", "Выбрать", "Отмена");
		return;
	}
	if (Factory[i][fcKind] == FKIND_MILITARY) { SendClientMessage(playerid, COLOR_GREY, "Это завод на военной части. Его не захватить."); return; }
	if (!Faction_IsGang(f) && !Faction_IsLaw(f) && !Faction_IsArmy(f)) { SendClientMessage(playerid, COLOR_GREY, "Захватывать заводы могут силовики, армия и банды."); return; }
	if (Faction_IsState(f) && Faction_IsState(owner)) { SendClientMessage(playerid, COLOR_GREY, "Завод уже у государства."); return; }
	if (Player[playerid][pRank] < FACTORY_CAPTURE_RANK) { SendClientMessage(playerid, COLOR_GREY, "Захватить завод может ранг %d и выше.", FACTORY_CAPTURE_RANK); return; }
	if (Faction_IsState(f) && !Player[playerid][pDuty]) { SendClientMessage(playerid, COLOR_GREY, "Сначала заступи на смену: /duty."); return; }
	if (gCapFc[playerid] != -1) { SendClientMessage(playerid, COLOR_GREY, "Ты уже захватываешь. Стой на маркере."); return; }
	if (Factory[i][fcCapBy] != INVALID_PLAYER_ID) { SendClientMessage(playerid, COLOR_GREY, "Этот завод уже захватывают."); return; }
	new now = gettime();
	if (now - Factory[i][fcLastCap] < FACTORY_CAPTURE_PAUSE)
	{
		SendClientMessage(playerid, COLOR_GREY, "Завод недавно захватывали. Подожди ещё %d мин.", (FACTORY_CAPTURE_PAUSE - (now - Factory[i][fcLastCap])) / 60 + 1);
		return;
	}
	new n = Factory_Defenders(i);
	if (n > 0) { SendClientMessage(playerid, COLOR_GREY, "Завод ещё не зачищен: рядом защитников — %d.", n); return; }

	gCapFc[playerid] = i;
	gCapLeft[playerid] = FACTORY_CAPTURE_TIME;
	Factory[i][fcCapBy] = playerid;
	SendClientMessage(playerid, COLOR_YELLOW, "Захват начат. Стой на маркере %d сек. Вернутся защитники — захват сорвётся.", FACTORY_CAPTURE_TIME);
	if (Faction_IsValid(owner))
	{
		format(msg, sizeof msg, "[Тревога] Завод «%s» захватывают: %s! Осталось %d сек.", Factory[i][fcName], Faction_GetName(f), FACTORY_CAPTURE_TIME);
		Faction_Message(owner, COLOR_RED, msg);
	}
}

stock Capture_Tick(playerid)
{
	new i = gCapFc[playerid], f = Player[playerid][pFaction], msg[144], buf[48];
	if (i < 0 || i >= MAX_FACTORIES || !Factory[i][fcUsed] || !Faction_IsValid(f)) { Capture_Stop(playerid); return; }
	if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT || Wound_IsWounded(playerid) || !Factory_At(playerid, i, FP_OWNER, 6.0))
	{
		Capture_Stop(playerid);
		SendClientMessage(playerid, COLOR_RED, "Захват сорван: ты ушёл с маркера.");
		return;
	}
	if (Factory_Defenders(i) > 0)
	{
		Capture_Stop(playerid);
		SendClientMessage(playerid, COLOR_RED, "Захват сорван: вернулись защитники.");
		return;
	}
	gCapLeft[playerid]--;
	if (gCapLeft[playerid] > 0)
	{
		format(buf, sizeof buf, "~r~Capture~n~~w~%d s", gCapLeft[playerid]);
		GameTextForPlayer(playerid, buf, 1100, 3);
		return;
	}

	new old = Factory[i][fcOwner];
	Capture_Stop(playerid);
	Factory[i][fcOwner] = f;
	Factory[i][fcClosed] = false;       // новый владелец ставит оборудование заново
	Factory[i][fcLastCap] = gettime();
	Factory_Save(i);
	for (new p = 0; p < MAX_FPOINTS; p++) Factory_Label(i, p);
	GameTextForPlayer(playerid, "~g~Captured", 2500, 3);
	format(msg, sizeof msg, "[Фракция] %s захватил завод «%s». На складе материалов: %d.", GetRPName(playerid), Factory[i][fcName], Factory[i][fcStock]);
	Faction_Message(f, Faction_GetColor(f), msg);
	Faction_Log(f, msg[10]);
	if (Faction_IsValid(old))
	{
		format(msg, sizeof msg, "[Фракция] Завод «%s» потерян: его захватили %s.", Factory[i][fcName], Faction_GetName(f));
		Faction_Message(old, COLOR_RED, msg);
		Faction_Log(old, msg[10]);
	}
	if (FPerm_Has(playerid, FP_FACTORY))
	{
		gFcSel[playerid] = i;
		ShowPlayerDialog(playerid, DIALOG_FCLOSE, DIALOG_STYLE_MSGBOX, "Завод захвачен",
			"{FFFFFF}Завод «%s» захвачен. Что с ним сделать?\n\n{33AA33}Оставить{FFFFFF} — завод переходит вашей организации и работает.\n{FF6347}Закрыть{FFFFFF} — оборудование уничтожено: это больше не завод, а просто помещение.\nОставшиеся ящики можно вывезти со склада.",
			"Закрыть", "Оставить", Factory[i][fcName]);
	}
	else
		SendClientMessage(playerid, COLOR_WHITE, "Завод ваш и работает. Передать или закрыть его может тот, у кого есть право «завод»: ALT на маркере.");
}

// ---------------------------------------------------------------------
//  Кража материалов со склада армии (банды)
// ---------------------------------------------------------------------
stock Supply_ArmyOnline()
{
	new n = 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Faction_IsArmy(Player[i][pFaction])) n++;
	}
	return n;
}

// Тревога: армии и силовикам
stock Supply_Alarm(const message[])
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged]) continue;
		if (Faction_IsArmy(Player[i][pFaction]) || Faction_IsLaw(Player[i][pFaction])) SendClientMessage(i, COLOR_RED, "%s", message);
	}
}

stock Steal_Stop(playerid, const why[])
{
	if (!gStealing[playerid]) return;
	gStealing[playerid] = false;
	Faction_SaveMats(FACTION_ARMY);
	if (!IsPlayerConnected(playerid)) return;
	SendClientMessage(playerid, COLOR_YELLOW, "%s Украдено за этот раз: %d. В сумке материалов: %d.", why, gStealTotal[playerid], Inv_Count(playerid, INV_MATS));
	if (gStealTotal[playerid] > 0)
	{
		new msg[144];
		format(msg, sizeof msg, "Со склада украли %d материалов (осталось %d).", gStealTotal[playerid], Faction[FACTION_ARMY][fMats]);
		Faction_Log(FACTION_ARMY, msg);
	}
}

// ALT на маркере склада армии
stock Steal_Toggle(playerid)
{
	if (gStealing[playerid]) { Steal_Stop(playerid, "Ты перестал красть."); return; }
	if (Player[playerid][pJail] > 0) return;
	if (Faction[FACTION_ARMY][fMats] <= 0) { SendClientMessage(playerid, COLOR_GREY, "Склад армии пуст."); return; }
	if (Supply_ArmyOnline() < STEAL_MIN_ARMY) { SendClientMessage(playerid, COLOR_GREY, "Склад грабят, только когда в сети хотя бы %d военных.", STEAL_MIN_ARMY); return; }
	if (Inv_Count(playerid, INV_MATS) >= Inv_GetMax(INV_MATS)) { SendClientMessage(playerid, COLOR_GREY, "Сумка полная: больше %d материалов не унести.", Inv_GetMax(INV_MATS)); return; }
	gStealing[playerid] = true;
	gStealTotal[playerid] = 0;
	SendClientMessage(playerid, COLOR_YELLOW, "Ты крадёшь материалы. Стой на маркере: в сумку идёт 1-5 в секунду. ALT ещё раз — перестать.");
	new msg[144];
	format(msg, sizeof msg, "[Тревога] Грабят склад армии! Грабитель: %s [%d].", GetRPName(playerid), playerid);
	Supply_Alarm(msg);
}

stock Steal_Tick(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT || Wound_IsWounded(playerid) || Player[playerid][pCuffed]) { Steal_Stop(playerid, "Кража сорвалась."); return; }
	if (!Supply_AtPoint(playerid, SP_ARMY, 3.0)) { Steal_Stop(playerid, "Ты сошёл с маркера."); return; }
	new n = 1 + random(5);
	new room = Inv_GetMax(INV_MATS) - Inv_Count(playerid, INV_MATS);
	if (n > room) n = room;
	if (n > Faction[FACTION_ARMY][fMats]) n = Faction[FACTION_ARMY][fMats];
	if (n <= 0)
	{
		if (room <= 0) Steal_Stop(playerid, "Сумка полная.");
		else Steal_Stop(playerid, "Склад армии пуст.");
		return;
	}
	if (!Inv_Add(playerid, INV_MATS, n)) { Steal_Stop(playerid, "Сумка полная."); return; }
	Faction[FACTION_ARMY][fMats] -= n;
	gStealTotal[playerid] += n;
	if (gStealTotal[playerid] % 20 < n) Faction_SaveMats(FACTION_ARMY);   // в базу — не каждую секунду
	new buf[32];
	format(buf, sizeof buf, "~r~+%d", n);
	GameTextForPlayer(playerid, buf, 1100, 3);
}

// Смерть или выход: всё, чем был занят, обрывается
stock Supply_ResetPlayer(playerid)
{
	if (gStealing[playerid]) Steal_Stop(playerid, "Кража сорвалась.");
	if (gCapFc[playerid] != -1) Capture_Stop(playerid);
	Crate_Drop(playerid);
	gWorkStage[playerid] = 0;
}

// Раз в секунду: ящик в руках, кража, захват
public Supply_OnSecond()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i))
		{
			if (gStealing[i] || gCapFc[i] != -1 || gCrate[i] != 0) Supply_ResetPlayer(i);
			continue;
		}
		if (gCrate[i] != 0 && (GetPlayerState(i) != PLAYER_STATE_ONFOOT || Wound_IsWounded(i)))
		{
			Crate_Drop(i);
			SendClientMessage(i, COLOR_GREY, "Ты уронил ящик. Он пропал.");
		}
		if (gStealing[i]) Steal_Tick(i);
		if (gCapFc[i] != -1) Capture_Tick(i);
	}
	return 1;
}

// ---------------------------------------------------------------------
//  ALT пешком (из Keys_Interact). 1 — сработало.
// ---------------------------------------------------------------------
// Склад организации: только положить материалы. Ящики отсюда не выдаются,
// иначе их можно было бы гонять по кругу (склад -> машина -> склад) ради оплаты.
stock FStore_Menu(playerid)
{
	new f = Player[playerid][pFaction];
	if (Inv_Count(playerid, INV_MATS) <= 0)
	{
		SendClientMessage(playerid, COLOR_WHITE, "Склад организации: материалов %d. Положить можно материалы из сумки.", Faction[f][fMats]);
		return;
	}
	ShowPlayerDialog(playerid, DIALOG_FSTOREPT_PUT, DIALOG_STYLE_INPUT, "Положить материалы",
		"На складе: %d\nСколько материалов положить из сумки? В сумке: %d", "Положить", "Отмена", Faction[f][fMats], Inv_Count(playerid, INV_MATS));
}

stock Factory_TryAlt(playerid)
{
	new f = Player[playerid][pFaction], msg[144];

	// 1. В руках ящик: на склад завода, на склад своей организации или в кузов
	if (gCrate[playerid] != 0)
	{
		for (new i = 0; i < MAX_FACTORIES; i++)
		{
			if (!Factory[i][fcUsed] || !Factory_At(playerid, i, FP_STORE, FACTORY_POINT_RANGE)) continue;
			if (Factory[i][fcClosed]) { SendClientMessage(playerid, COLOR_GREY, "Завод закрыт: ящики отсюда только вывозят."); return 1; }
			if (Factory[i][fcStock] + CRATE_MATS > FACTORY_STOCK_MAX) { SendClientMessage(playerid, COLOR_GREY, "Склад завода забит."); return 1; }
			Factory[i][fcStock] += CRATE_MATS;
			Factory_Save(i);
			Factory_Label(i, FP_STORE);
			if (gCrate[playerid] == 1 && gCrateFc[playerid] == i)
			{
				GiveMoney(playerid, FACTORY_PAY, "работа на заводе");
				SendClientMessage(playerid, COLOR_GREEN, "Ящик сдан на склад завода: +$%d. Ящиков на складе: %d.", FACTORY_PAY, Factory[i][fcStock] / CRATE_MATS);
			}
			else SendClientMessage(playerid, COLOR_WHITE, "Ящик вернулся на склад завода. Ящиков: %d.", Factory[i][fcStock] / CRATE_MATS);
			Crate_Drop(playerid);
			return 1;
		}
		if (Faction_IsValid(f) && FStore_At(playerid, f, 3.0))
		{
			new cap = Faction_IsArmy(f) ? ARMY_MATS_MAX : FACTION_MATS_MAX;
			if (Faction[f][fMats] + CRATE_MATS > cap) { SendClientMessage(playerid, COLOR_GREY, "Склад организации полон."); return 1; }
			Faction[f][fMats] += CRATE_MATS;
			Faction_SaveMats(f);
			Crate_Drop(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Ящик сдан на склад организации. Материалов на складе: %d.", Faction[f][fMats]);
			format(msg, sizeof msg, "%s принёс на склад ящик (%d материалов, на складе %d).", GetRPName(playerid), CRATE_MATS, Faction[f][fMats]);
			Faction_Log(f, msg);
			return 1;
		}
		new v = Cargo_NearVehicle(playerid);
		if (v != 0)
		{
			new cap = Cargo_Cap(GetVehicleModel(v));
			if (!Cargo_IsBehind(playerid, v)) { SendClientMessage(playerid, COLOR_GREY, "Встань сзади машины."); return 1; }
			if (gVehCargo[v] + CRATE_MATS > cap) { SendClientMessage(playerid, COLOR_GREY, "Кузов полон (%d из %d).", gVehCargo[v], cap); return 1; }
			gVehCargo[v] += CRATE_MATS;
			Crate_Drop(playerid);
			ApplyAnimation(playerid, "CARRY", "putdwn", 4.1, false, false, false, false, 0);
			SendClientMessage(playerid, COLOR_GREEN, "Ящик в кузове: %d из %d материалов.", gVehCargo[v], cap);
			return 1;
		}
		SendClientMessage(playerid, COLOR_GREY, "Ящик кладут в грузовик или фургон (сзади), на склад завода или своей организации. Бросить: /dropcrate");
		return 1;
	}

	// 2. Точки заводов
	for (new i = 0; i < MAX_FACTORIES; i++)
	{
		if (!Factory[i][fcUsed]) continue;
		if (Factory_At(playerid, i, FP_RAW, FACTORY_POINT_RANGE))   { Factory_TakeRaw(playerid, i); return 1; }
		if (Factory_At(playerid, i, FP_BENCH, FACTORY_POINT_RANGE)) { Factory_UseBench(playerid, i); return 1; }
		if (Factory_At(playerid, i, FP_STORE, FACTORY_POINT_RANGE)) { Factory_TakeCrate(playerid, i); return 1; }
		if (Factory_At(playerid, i, FP_OWNER, FACTORY_POINT_RANGE)) { Factory_OwnerAlt(playerid, i); return 1; }
	}

	// 3. Склад армии для банд — кража; склад своей организации — меню
	if (Faction_IsGang(f) && Supply_AtPoint(playerid, SP_ARMY, 3.0)) { Steal_Toggle(playerid); return 1; }
	if (Faction_IsValid(f) && (gFsSet[f] || Faction_IsArmy(f)) && FStore_At(playerid, f, 3.0)) { FStore_Menu(playerid); return 1; }

	// 4. Торговец чёрного рынка
	if (BM_Near(playerid, BM_RANGE) != -1) { Cmd_BM(playerid); return 1; }
	return 0;
}

// ---------------------------------------------------------------------
//  Разгрузка: за рулём на складе организации (H или /unload). 1 — сработало.
// ---------------------------------------------------------------------
stock Factory_TryUnload(playerid, bool:verbose)
{
	new v = GetPlayerVehicleID(playerid), f = Player[playerid][pFaction];
	if (v == 0 || GetPlayerState(playerid) != PLAYER_STATE_DRIVER)
	{
		if (verbose) SendClientMessage(playerid, COLOR_GREY, "Разгружают за рулём, на складе организации.");
		return 0;
	}
	if (gVehCargo[v] <= 0)
	{
		if (verbose) SendClientMessage(playerid, COLOR_GREY, "В машине нет ящиков.");
		return 0;
	}
	if (!Faction_IsValid(f))
	{
		if (verbose) SendClientMessage(playerid, COLOR_GREY, "Разгружать можно только на складе своей организации.");
		return 0;
	}
	new t = 0, bool:denied = false;
	for (new k = 1; k < MAX_FACTIONS; k++)
	{
		if (!Faction_IsValid(k) || !FStore_At(playerid, k, SUPPLY_RANGE)) continue;
		if (k == f || (Faction_IsArmy(f) && Faction_IsState(k))) { t = k; break; }
		denied = true;
	}
	if (t == 0)
	{
		if (denied) { SendClientMessage(playerid, COLOR_GREY, "Это чужой склад. Разгружать можно на складе своей организации."); return 1; }
		if (verbose) SendClientMessage(playerid, COLOR_GREY, "Рядом нет склада организации. В кузове материалов: %d.", gVehCargo[v]);
		return 0;
	}

	new cap = Faction_IsArmy(t) ? ARMY_MATS_MAX : FACTION_MATS_MAX;
	new room = cap - Faction[t][fMats], n = gVehCargo[v], pay = 0, msg[144];
	if (room <= 0) { SendClientMessage(playerid, COLOR_GREY, "Склад «%s» полон.", Faction_GetName(t)); return 1; }
	if (n > room) n = room;
	Faction[t][fMats] += n;
	Faction_SaveMats(t);
	gVehCargo[v] -= n;
	if (Faction_IsArmy(f) && Player[playerid][pDuty])
	{
		if (t == f) pay = (n / 100) * SUPPLY_PAY_HAUL;
		else pay = (n / 100) * SUPPLY_PAY_DELIVER;
		if (pay > 0) GiveMoney(playerid, pay, "доставка материалов");
	}
	SendClientMessage(playerid, COLOR_GREEN, "Разгружено на склад «%s»: %d. В кузове осталось: %d. Оплата: $%d.", Faction_GetName(t), n, gVehCargo[v], pay);
	format(msg, sizeof msg, "%s привёз на склад %d материалов (на складе %d).", GetRPName(playerid), n, Faction[t][fMats]);
	Faction_Log(t, msg);
	format(msg, sizeof msg, "[Фракция] На склад привезли %d материалов. На складе: %d.", n, Faction[t][fMats]);
	Faction_Message(t, Faction_GetColor(t), msg);
	return 1;
}

// /unload, /supply
stock bool:Cmd_Supply(playerid)
{
	Factory_TryUnload(playerid, true);
	return true;
}

stock bool:Cmd_Steal(playerid)
{
	return SendClientMessage(playerid, COLOR_GREY, "Красть можно только со склада армии: встань на его маркер и нажми ALT.");
}

stock bool:Cmd_Mats(playerid)
{
	return SendClientMessage(playerid, COLOR_WHITE, "В сумке материалов: %d из %d. Сделать оружие: сумка (Y), «использовать». Сдать: ALT на складе организации.",
		Inv_Count(playerid, INV_MATS), Inv_GetMax(INV_MATS));
}

// ---------------------------------------------------------------------
//  Оружие из материалов (сумка -> «использовать»)
// ---------------------------------------------------------------------
stock Craft_Show(playerid)
{
	new body[700], line[64], title[64];
	strcat(body, "Что сделать\tМатериалов\n");
	for (new it = 0; it < MAX_FITEMS; it++)
	{
		format(line, sizeof line, "%s\t%d\n", gFItemName[it], gFItemMats[it]);
		strcat(body, line);
	}
	format(title, sizeof title, "Материалов в сумке: %d", Inv_Count(playerid, INV_MATS));
	ShowPlayerDialog(playerid, DIALOG_CRAFT, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Выбрать", "Закрыть", body);
}

stock Craft_Make(playerid, it, qty)
{
	new need = gFItemMats[it] * qty;
	if (!Inv_Remove(playerid, INV_MATS, need))
	{
		SendClientMessage(playerid, COLOR_GREY, "Не хватает материалов: нужно %d, в сумке %d.", need, Inv_Count(playerid, INV_MATS));
		return;
	}
	switch (it)
	{
		case FITEM_ARMOUR: SetPlayerArmour(playerid, 100.0);
		case FITEM_MEDKIT: SetPlayerHealth(playerid, 100.0);
		default: GivePlayerWeapon(playerid, gFItemWeapon[it], gFItemAmmo[it] * qty);
	}
	new msg[144];
	format(msg, sizeof msg, "%s что-то мастерит из деталей.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(playerid, COLOR_GREEN, "Сделано: %s x%d. Потрачено материалов: %d, осталось %d.", gFItemName[it], qty, need, Inv_Count(playerid, INV_MATS));
}

// Окна заводов, складов и крафта. Вызывается из OnDialogResponse (rp.pwn). 1 — окно наше.
stock Factory_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid < DIALOG_CRAFT || dialogid > DIALOG_LOCKER) return 0;
	new f = Player[playerid][pFaction], msg[144];
	switch (dialogid)
	{
		case DIALOG_CRAFT:
		{
			if (!response || listitem < 0 || listitem >= MAX_FITEMS) return 1;
			if (Wound_IsWounded(playerid) || Player[playerid][pCuffed] || Player[playerid][pJail] > 0) return 1;
			gCraftSel[playerid] = listitem;
			if (listitem == FITEM_ARMOUR || listitem == FITEM_MEDKIT) { Craft_Make(playerid, listitem, 1); return 1; }
			ShowPlayerDialog(playerid, DIALOG_CRAFT_QTY, DIALOG_STYLE_INPUT, "Сколько сделать",
				"%s: %d материалов за штуку, в каждой %d патронов.\nСколько сделать? (1-10)\nВ сумке материалов: %d", "Сделать", "Назад",
				gFItemName[listitem], gFItemMats[listitem], gFItemAmmo[listitem], Inv_Count(playerid, INV_MATS));
		}
		case DIALOG_CRAFT_QTY:
		{
			if (!response) { Craft_Show(playerid); return 1; }
			new it = gCraftSel[playerid];
			if (it < 0 || it >= MAX_FITEMS) return 1;
			if (Wound_IsWounded(playerid) || Player[playerid][pCuffed] || Player[playerid][pJail] > 0) return 1;
			if (!IsNumeric(inputtext) || strval(inputtext) < 1 || strval(inputtext) > 10) { SendClientMessage(playerid, COLOR_GREY, "Количество от 1 до 10."); return 1; }
			Craft_Make(playerid, it, strval(inputtext));
		}
		case DIALOG_FSTOREPT:
		{
			if (!response) return 1;
			if (!Faction_IsValid(f) || !FStore_At(playerid, f, 5.0)) { SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от склада."); return 1; }
			// окно больше не используется: склад организации сразу спрашивает, сколько положить
		}
		case DIALOG_FSTOREPT_PUT:
		{
			if (!response) return 1;
			if (!Faction_IsValid(f) || !FStore_At(playerid, f, 5.0)) { SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от склада."); return 1; }
			if (!IsNumeric(inputtext) || strval(inputtext) < 1) { SendClientMessage(playerid, COLOR_GREY, "Введи число больше нуля."); return 1; }
			new n = strval(inputtext);
			new cap = Faction_IsArmy(f) ? ARMY_MATS_MAX : FACTION_MATS_MAX;
			if (n > Inv_Count(playerid, INV_MATS)) n = Inv_Count(playerid, INV_MATS);
			if (n > cap - Faction[f][fMats]) n = cap - Faction[f][fMats];
			if (n <= 0 || !Inv_Remove(playerid, INV_MATS, n)) { SendClientMessage(playerid, COLOR_GREY, "Положить нечего или склад полон."); return 1; }
			Faction[f][fMats] += n;
			Faction_SaveMats(f);
			SendClientMessage(playerid, COLOR_GREEN, "Положено на склад: %d. На складе материалов: %d, в сумке: %d.", n, Faction[f][fMats], Inv_Count(playerid, INV_MATS));
			format(msg, sizeof msg, "%s положил на склад %d материалов (на складе %d).", GetRPName(playerid), n, Faction[f][fMats]);
			Faction_Log(f, msg);
		}
		case DIALOG_LOCKER:
		{
			if (!response || !Faction_IsValid(f)) return 1;
			if (!IsPlayerInRangeOfPoint(playerid, FACTION_DUTY_RANGE + 2.0, Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ])) { SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от раздевалки."); return 1; }
			if (listitem == 0) Cmd_Duty(playerid);
			else if (listitem == 2) Cmd_Duty(playerid, true);
			else Cmd_FStorage(playerid);
		}
		case DIALOG_FMANAGE:
		{
			if (!response) return 1;
			new i = gFcSel[playerid];
			if (i < 0 || i >= MAX_FACTORIES || !Factory[i][fcUsed] || Factory[i][fcClosed] || Factory[i][fcOwner] != f || !FPerm_Has(playerid, FP_FACTORY)) return 1;
			if (Factory[i][fcKind] == FKIND_MILITARY) return 1;
			if (listitem == 1)
			{
				ShowPlayerDialog(playerid, DIALOG_FCLOSE, DIALOG_STYLE_MSGBOX, "Закрыть завод",
					"{FFFFFF}Закрыть завод «%s» и уничтожить оборудование?\n\nЭто больше не будет заводом: точки пропадут, работать и захватывать его будет нельзя.\nОставшиеся ящики можно вывезти со склада. Вернуть завод может только администратор.",
					"Закрыть", "Отмена", Factory[i][fcName]);
				return 1;
			}
			new body[1800], line[48], n = 0;
			for (new t = 1; t < MAX_FACTIONS; t++)
			{
				if (t == f || !Faction_IsValid(t) || Faction_IsHidden(t)) continue;
				format(line, sizeof line, "{%06x}%s\n", Faction_GetColor(t) >>> 8, Faction_GetName(t));
				strcat(body, line);
				gFcList[playerid][n++] = t;
			}
			for (new k = n; k < MAX_FACTIONS; k++) gFcList[playerid][k] = 0;
			if (n == 0) { SendClientMessage(playerid, COLOR_GREY, "Передавать некому."); return 1; }
			ShowPlayerDialog(playerid, DIALOG_FTRANSFER, DIALOG_STYLE_LIST, "Кому передать завод", "%s", "Передать", "Отмена", body);
		}
		case DIALOG_FTRANSFER:
		{
			if (!response || listitem < 0 || listitem >= MAX_FACTIONS) return 1;
			new i = gFcSel[playerid], t = gFcList[playerid][listitem];
			if (i < 0 || i >= MAX_FACTORIES || !Factory[i][fcUsed] || Factory[i][fcClosed] || Factory[i][fcOwner] != f || !FPerm_Has(playerid, FP_FACTORY)) return 1;
			if (Factory[i][fcKind] == FKIND_MILITARY || t == f || !Faction_IsValid(t)) return 1;
			if (!Factory_At(playerid, i, FP_OWNER, 6.0)) { SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от маркера завода."); return 1; }
			if (Factory[i][fcCapBy] != INVALID_PLAYER_ID) { SendClientMessage(playerid, COLOR_GREY, "Завод сейчас захватывают. Сначала отбейте его."); return 1; }
			Factory[i][fcOwner] = t;
			Factory_Save(i);
			for (new p = 0; p < MAX_FPOINTS; p++) Factory_Label(i, p);
			format(msg, sizeof msg, "[Фракция] %s передал завод «%s»: %s.", GetRPName(playerid), Factory[i][fcName], Faction_GetName(t));
			Faction_Message(f, Faction_GetColor(f), msg);
			Faction_Log(f, msg[10]);
			format(msg, sizeof msg, "[Фракция] Организации передан завод «%s» (от: %s). Материалов на складе: %d.", Factory[i][fcName], Faction_GetName(f), Factory[i][fcStock]);
			Faction_Message(t, Faction_GetColor(t), msg);
			Faction_Log(t, msg[10]);
		}
		case DIALOG_FCLOSE:
		{
			if (!response) return 1;
			new i = gFcSel[playerid];
			if (i < 0 || i >= MAX_FACTORIES || !Factory[i][fcUsed] || Factory[i][fcOwner] != f || !FPerm_Has(playerid, FP_FACTORY)) return 1;
			if (Factory[i][fcKind] == FKIND_MILITARY) return 1;     // военный завод не закрывают
			if (!Factory_At(playerid, i, FP_OWNER, 6.0)) { SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от маркера завода."); return 1; }
			Factory[i][fcClosed] = true;
			Factory_Save(i);
			if (Factory[i][fcCapBy] != INVALID_PLAYER_ID) Capture_Stop(Factory[i][fcCapBy]);
			for (new p = 0; p < MAX_FPOINTS; p++) Factory_BuildPoint(i, p);    // точки завода пропадают
			format(msg, sizeof msg, "[Фракция] %s закрыл завод «%s»: оборудование уничтожено.", GetRPName(playerid), Factory[i][fcName]);
			Faction_Message(f, Faction_GetColor(f), msg);
			Faction_Log(f, msg[10]);
		}
	}
	return 1;
}

// ---------------------------------------------------------------------
//  Админ-команды заводов
// ---------------------------------------------------------------------
stock bool:Cmd_AddFactory(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, kind, name[32];
	if (!NextIntParam(params, pos, kind) || kind < FKIND_MILITARY || kind > FKIND_COMMON || !RestParam(params, pos, name))
	{
		SendClientMessage(playerid, COLOR_GREY, "Использование: /addfactory [вид] [название] — маркер завода там, где стоишь.");
		SendClientMessage(playerid, COLOR_GREY, "Виды: 1 — военный (владеет армия, захватить нельзя), 2 — обычный (можно захватывать).");
		return true;
	}
	if (!Faction_IsGoodName(name, 2, 31)) return SendClientMessage(playerid, COLOR_GREY, "Название: от 2 до 31 символа, без фигурных скобок и знака процента.");
	new i = -1;
	for (new k = 0; k < MAX_FACTORIES; k++)
	{
		if (!Factory[k][fcUsed]) { i = k; break; }
	}
	if (i == -1) return SendClientMessage(playerid, COLOR_GREY, "Достигнут лимит заводов (%d).", MAX_FACTORIES);

	new owner = (kind == FKIND_MILITARY) ? FACTION_ARMY : FACTION_NONE;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO factories (name, kind, owner, closed, stock) VALUES ('%q', %d, %d, 0, 0)", name, kind, owner));
	Factory[i][fcUsed] = true;
	Factory[i][fcDbId] = DB_LastInsertId();
	Factory[i][fcName][0] = '\0';
	strcat(Factory[i][fcName], name, 32);
	Factory[i][fcKind] = kind;
	Factory[i][fcOwner] = owner;
	Factory[i][fcClosed] = false;
	Factory[i][fcStock] = 0;
	Factory[i][fcLastCap] = 0;
	Factory[i][fcCapBy] = INVALID_PLAYER_ID;
	for (new p = 0; p < MAX_FPOINTS; p++)
	{
		gFcSet[i][p] = false;
		gFcMk[i][p] = -1;
	}
	GetPlayerPos(playerid, gFcX[i][FP_OWNER], gFcY[i][FP_OWNER], gFcZ[i][FP_OWNER]);
	gFcW[i][FP_OWNER] = GetPlayerVirtualWorld(playerid);
	gFcI[i][FP_OWNER] = GetPlayerInterior(playerid);
	gFcSet[i][FP_OWNER] = true;
	Factory_SavePoint(i, FP_OWNER);
	Factory_BuildPoint(i, FP_OWNER);

	new n = Factory[i][fcDbId];
	SendClientMessage(playerid, COLOR_ADMIN, "Завод №%d «%s» создан, его маркер стоит здесь.", n, name);
	SendClientMessage(playerid, COLOR_ADMIN, "Расставь точки: /setfpoint %d raw (сырьё), /setfpoint %d bench (станок), /setfpoint %d store (склад ящиков).", n, n, n);
	if (kind == FKIND_COMMON) SendClientMessage(playerid, COLOR_ADMIN, "Владельца пока нет: /setfowner %d [фракция] или пусть завод захватят.", n);
	return true;
}

stock bool:Cmd_SetFPoint(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, n, word[16], p = -1;
	if (NextIntParam(params, pos, n) && NextParam(params, pos, word))
	{
		if (!strcmp(word, "raw", true)) p = FP_RAW;
		else if (!strcmp(word, "bench", true)) p = FP_BENCH;
		else if (!strcmp(word, "store", true)) p = FP_STORE;
		else if (!strcmp(word, "owner", true)) p = FP_OWNER;
	}
	new i = (p == -1) ? -1 : Factory_Find(n);
	if (i == -1)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setfpoint [№ завода] [raw|bench|store|owner] — сырьё, станок, склад ящиков, маркер завода. Список: /factories");
	GetPlayerPos(playerid, gFcX[i][p], gFcY[i][p], gFcZ[i][p]);
	gFcW[i][p] = GetPlayerVirtualWorld(playerid);
	gFcI[i][p] = GetPlayerInterior(playerid);
	gFcSet[i][p] = true;
	Factory_SavePoint(i, p);
	if (gFcMk[i][p] != -1) { Marker_Destroy(gFcMk[i][p], true); gFcMk[i][p] = -1; }
	Factory_BuildPoint(i, p);
	SendClientMessage(playerid, COLOR_ADMIN, "Завод №%d «%s»: точка «%s» теперь здесь.", n, Factory[i][fcName], word);
	return true;
}

stock bool:Cmd_SetFOwner(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, n, f;
	if (!NextIntParam(params, pos, n) || !NextIntParam(params, pos, f) || Factory_Find(n) == -1 || (f != FACTION_NONE && !Faction_IsValid(f)))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setfowner [№ завода] [фракция, 0 — ничей]. Заводы: /factories, фракции: /flist");
	new i = Factory_Find(n);
	Factory[i][fcOwner] = f;
	Factory[i][fcClosed] = false;
	Factory_Save(i);
	for (new p = 0; p < MAX_FPOINTS; p++) Factory_BuildPoint(i, p);    // закрытый завод снова становится заводом
	if (f == FACTION_NONE) SendClientMessage(playerid, COLOR_ADMIN, "Завод №%d «%s» теперь без владельца.", n, Factory[i][fcName]);
	else SendClientMessage(playerid, COLOR_ADMIN, "Завод №%d «%s» теперь у фракции «%s» и работает.", n, Factory[i][fcName], Faction_GetName(f));
	return true;
}

stock bool:Cmd_DelFactory(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, n;
	if (!NextIntParam(params, pos, n) || Factory_Find(n) == -1)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /delfactory [№ завода]. Список: /factories");
	new i = Factory_Find(n);
	for (new p = 0; p < MAX_FPOINTS; p++)
	{
		if (gFcMk[i][p] != -1) { Marker_Destroy(gFcMk[i][p], true); gFcMk[i][p] = -1; }
		gFcSet[i][p] = false;
	}
	if (Factory[i][fcCapBy] != INVALID_PLAYER_ID) Capture_Stop(Factory[i][fcCapBy]);
	Factory[i][fcUsed] = false;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM factories WHERE id = %d", n));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM factory_points WHERE factory = %d", n));
	SendClientMessage(playerid, COLOR_ADMIN, "Завод №%d удалён.", n);
	return true;
}

stock bool:Cmd_Factories(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new count = 0;
	SendClientMessage(playerid, COLOR_ADMIN, "Заводы (№ — название, вид, владелец, материалов на складе):");
	for (new i = 0; i < MAX_FACTORIES; i++)
	{
		if (!Factory[i][fcUsed]) continue;
		count++;
		SendClientMessage(playerid, COLOR_WHITE, " %d — %s, вид %d, %s, материалов %d, закрыт: %d, точек задано: %d из 4", Factory[i][fcDbId], Factory[i][fcName],
			Factory[i][fcKind], Faction_GetName(Factory[i][fcOwner]), Factory[i][fcStock], Factory[i][fcClosed] ? 1 : 0,
			(gFcSet[i][0] ? 1 : 0) + (gFcSet[i][1] ? 1 : 0) + (gFcSet[i][2] ? 1 : 0) + (gFcSet[i][3] ? 1 : 0));
	}
	if (count == 0) SendClientMessage(playerid, COLOR_GREY, " Заводов пока нет. Создать: /addfactory [вид] [название]");
	return true;
}

// ---------------------------------------------------------------------
//  Чёрный рынок
// ---------------------------------------------------------------------
stock bool:Cmd_BM(playerid)
{
	if (BM_Near(playerid, BM_RANGE) == -1) return SendClientMessage(playerid, COLOR_GREY, "Здесь нет торговца. Чёрный рынок ещё надо найти.");
	if (Faction_IsState(Player[playerid][pFaction]) && Player[playerid][pDuty])
		return SendClientMessage(playerid, COLOR_GREY, "Торговец не разговаривает с людьми в форме.");
	new body[1200], line[96];
	strcat(body, "Товар\tЦена\n");
	for (new it = 0; it < MAX_FITEMS; it++)
	{
		if (gBMPrice[it] > 0) format(line, sizeof line, "%s\t{33AA33}$%d\n", gFItemName[it], gBMPrice[it]);
		else format(line, sizeof line, "{AFAFAF}%s\t{AFAFAF}нет в продаже\n", gFItemName[it]);
		strcat(body, line);
	}
	// наркотики: торговцу всё равно, кто ты (у уличного дилера кокаин — только бандам)
	format(line, sizeof line, "Семена конопли\t{33AA33}$%d\n", BM_SEED_PRICE);
	strcat(body, line);
	format(line, sizeof line, "Марихуана, 5 г\t{33AA33}$%d\n", BM_WEED_PRICE * 5);
	strcat(body, line);
	format(line, sizeof line, "Кокаин, 1 г\t{33AA33}$%d\n", BM_COKE_PRICE);
	strcat(body, line);
	format(line, sizeof line, "{FFFF00}Продать материалы из сумки (%d шт.)\t$%d за 1 шт.", Inv_Count(playerid, INV_MATS), gBMMatPrice);
	strcat(body, line);
	ShowPlayerDialog(playerid, DIALOG_BM, DIALOG_STYLE_TABLIST_HEADERS, "Чёрный рынок", "%s", "Выбрать", "Уйти", body);
	return true;
}

// Окно /bm. Вызывается из OnDialogResponse (rp.pwn). 1 — окно наше.
stock BM_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid != DIALOG_BM) return 0;
	if (!response) return 1;
	if (BM_Near(playerid, BM_RANGE + 2.0) == -1) { SendClientMessage(playerid, COLOR_GREY, "Ты отошёл от торговца."); return 1; }
	if (Faction_IsState(Player[playerid][pFaction]) && Player[playerid][pDuty]) return 1;

	if (listitem >= 0 && listitem < MAX_FITEMS)
	{
		new price = gBMPrice[listitem];
		if (price <= 0) { SendClientMessage(playerid, COLOR_GREY, "Этого сейчас нет в продаже."); return 1; }
		if (GetMoney(playerid) < price) { SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", price); return 1; }
		GiveMoney(playerid, -price, "чёрный рынок");
		switch (listitem)
		{
			case FITEM_ARMOUR: SetPlayerArmour(playerid, 100.0);
			case FITEM_MEDKIT: SetPlayerHealth(playerid, 100.0);
			default: GivePlayerWeapon(playerid, gFItemWeapon[listitem], gFItemAmmo[listitem]);
		}
		SendClientMessage(playerid, COLOR_GREEN, "Куплено: %s за $%d.", gFItemName[listitem], price);
		Cmd_BM(playerid);
		return 1;
	}
	if (listitem >= MAX_FITEMS && listitem < MAX_FITEMS + 3)
	{
		new d = listitem - MAX_FITEMS;
		new item = (d == 0) ? INV_SEEDS : ((d == 1) ? INV_WEED : INV_COCAINE);
		new amount = (d == 1) ? 5 : 1;
		new price = (d == 0) ? BM_SEED_PRICE : ((d == 1) ? BM_WEED_PRICE * 5 : BM_COKE_PRICE);
		if (GetMoney(playerid) < price) { SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", price); return 1; }
		if (!Inv_Add(playerid, item, amount)) { SendClientMessage(playerid, COLOR_GREY, "Больше в сумку не влезет (%s: до %d).", Inv_GetName(item), Inv_GetMax(item)); return 1; }
		GiveMoney(playerid, -price, "чёрный рынок: наркотики");
		SendClientMessage(playerid, COLOR_GREEN, "Куплено: %s x%d за $%d. В сумке: %d.", Inv_GetName(item), amount, price, Inv_Count(playerid, item));
		Account_Save(playerid);
		Cmd_BM(playerid);
		return 1;
	}

	// последняя строка — продать материалы из сумки
	new n = Inv_Count(playerid, INV_MATS);
	if (n <= 0) { SendClientMessage(playerid, COLOR_GREY, "В сумке нет материалов."); return 1; }
	if (gBMMatPrice <= 0) { SendClientMessage(playerid, COLOR_GREY, "Торговец сейчас не берёт материалы."); return 1; }
	if (!Inv_Remove(playerid, INV_MATS, n)) return 1;
	GiveMoney(playerid, n * gBMMatPrice, "продажа материалов на чёрном рынке");
	SendClientMessage(playerid, COLOR_GREEN, "Продано материалов: %d за $%d.", n, n * gBMMatPrice);
	return 1;
}

// /bmprice — цены чёрного рынка (админ 4+)
stock bool:Cmd_BMPrice(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, item, price;
	if (!NextIntParam(params, pos, item) || !NextIntParam(params, pos, price) || item < 0 || item > MAX_FITEMS || price < 0 || price > 10000000)
	{
		SendClientMessage(playerid, COLOR_ADMIN, "Цены чёрного рынка. Изменить: /bmprice [№] [цена], цена 0 — снять с продажи.");
		SendClientMessage(playerid, COLOR_WHITE, " 0 — скупка материалов: $%d за 1 шт.", gBMMatPrice);
		for (new it = 0; it < MAX_FITEMS; it++)
			SendClientMessage(playerid, COLOR_WHITE, " %d — %s: $%d", it + 1, gFItemName[it], gBMPrice[it]);
		return true;
	}
	if (item == 0)
	{
		gBMMatPrice = price;
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO bm_prices (item, price) VALUES (-1, %d)", price));
		SendClientMessage(playerid, COLOR_ADMIN, "Торговец теперь платит $%d за 1 материал.", price);
		return true;
	}
	gBMPrice[item - 1] = price;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO bm_prices (item, price) VALUES (%d, %d)", item - 1, price));
	SendClientMessage(playerid, COLOR_ADMIN, "Чёрный рынок: %s теперь $%d.", gFItemName[item - 1], price);
	return true;
}

// ---------------------------------------------------------------------
//  Общие чаты: /d — госструктуры, /gd — банды
// ---------------------------------------------------------------------
stock bool:Cmd_DeptChat(playerid, const params[])
{
	new f = Player[playerid][pFaction];
	if (!Org_DeptCanWrite(f))
		return SendClientMessage(playerid, COLOR_GREY, Org_DeptCanRead(f) ? ("Твоей организации можно только читать /d.") : ("Этот чат только для госструктур."));
	new text[90], msg[160];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /d [текст] — общий чат госструктур");
	new squad[24];
	Squad_GetName(playerid, squad);
	if (squad[0] != '\0') format(msg, sizeof msg, "[Департамент] %s | %s | %s | %s: %s", Faction_GetName(f), Faction_GetRankName(f, Player[playerid][pRank]), squad, GetRPName(playerid), text);
	else format(msg, sizeof msg, "[Департамент] %s | %s | %s: %s", Faction_GetName(f), Faction_GetRankName(f, Player[playerid][pRank]), GetRPName(playerid), text);
	msg[143] = '\0';
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Org_DeptCanRead(Player[i][pFaction])) SendClientMessage(i, 0xFF9E7AFF, "%s", msg);
	}
	return true;
}

stock bool:Cmd_GangChat(playerid, const params[])
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsGang(f)) return SendClientMessage(playerid, COLOR_GREY, "Этот чат только для банд.");
	new text[90], msg[160];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /gd [текст] — общий чат банд");
	format(msg, sizeof msg, "[Улицы] %s | %s: %s", Faction_GetName(f), GetRPName(playerid), text);
	msg[143] = '\0';
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Faction_IsGang(Player[i][pFaction])) SendClientMessage(i, 0xC0A060FF, "%s", msg);
	}
	Fbi_OnGangChat(msg);        // прослушка ФБР (fbi.inc)
	return true;
}


// =====================================================================
//  Цвет фракции: /fcolor [фракция] — палитра или свой код RRGGBB (админ 4+)
// =====================================================================
static const gFColorVal[16] =
{
	0x3A6EFFFF, 0x00C8F0FF, 0x6C8CD5FF, 0x10A010FF, 0x6B8E23FF, 0x33CC99FF, 0xFFD700FF, 0xFF8C00FF,
	0xE0A030FF, 0xFF4040FF, 0xFF8080FF, 0xFF66CCFF, 0xB040FFFF, 0x8B5A2BFF, 0xC8C8C8FF, 0xFFFFFFFF
};
static const gFColorName[16][20] =
{
	"Синий", "Голубой", "Серо-синий", "Зелёный", "Оливковый", "Бирюзовый", "Жёлтый", "Оранжевый",
	"Песочный", "Красный", "Розовый", "Малиновый", "Фиолетовый", "Коричневый", "Серый", "Белый"
};

stock FColor_Show(playerid)
{
	new f = gFColorSel[playerid];
	if (!Faction_IsValid(f)) return;
	new body[900], line[48], title[64];
	for (new i = 0; i < sizeof gFColorVal; i++)
	{
		format(line, sizeof line, "{%06x}%s\n", gFColorVal[i] >>> 8, gFColorName[i]);
		strcat(body, line);
	}
	strcat(body, "{FFFFFF}Свой цвет (ввести код RRGGBB)");
	format(title, sizeof title, "Цвет: %s", Faction_GetName(f));
	ShowPlayerDialog(playerid, DIALOG_FCOLOR, DIALOG_STYLE_LIST, title, "%s", "Выбрать", "Закрыть", body);
}

stock bool:Cmd_FColor(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f;
	if (!NextIntParam(params, pos, f) || !Faction_IsValid(f))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /fcolor [фракция] — выбрать цвет из палитры. Номера фракций: /flist");
	gFColorSel[playerid] = f;
	FColor_Show(playerid);
	return true;
}

// Поставить фракции цвет и обновить всё, где он виден
stock FColor_Apply(playerid, f, color)
{
	Faction[f][fColor] = color;
	Faction_SaveInfo(f);
	Faction_CreateMarker(f);
	Turfs_Recolor();
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pFaction] == f) Faction_UpdateColor(i);
	}
	new msg[144];
	format(msg, sizeof msg, "Администратор %s сменил цвет фракции на %06x.", GetRPName(playerid), color >>> 8);
	Faction_Log(f, msg);
	SendClientMessage(playerid, color, "Цвет фракции «%s» теперь такой.", Faction_GetName(f));
}

// Окна /fcolor. Вызывается из OnDialogResponse (rp.pwn). 1 — окно наше.
stock FColor_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid != DIALOG_FCOLOR && dialogid != DIALOG_FCOLOR_HEX) return 0;
	if (Player[playerid][pAdmin] < 4) return 1;
	new f = gFColorSel[playerid];
	if (!Faction_IsValid(f)) return 1;

	if (dialogid == DIALOG_FCOLOR)
	{
		if (!response) return 1;
		if (listitem >= 0 && listitem < sizeof gFColorVal)
		{
			FColor_Apply(playerid, f, gFColorVal[listitem]);
			return 1;
		}
		ShowPlayerDialog(playerid, DIALOG_FCOLOR_HEX, DIALOG_STYLE_INPUT, "Свой цвет",
			"Введи код цвета: 6 знаков RRGGBB, например 3A6EFF.", "Сохранить", "Назад");
		return 1;
	}

	if (!response) { FColor_Show(playerid); return 1; }
	new rgb = 0, bool:ok = (strlen(inputtext) == 6);
	for (new i = 0; ok && i < 6; i++)
	{
		new c = inputtext[i], d = 0;
		if (c >= '0' && c <= '9') d = c - '0';
		else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
		else ok = false;
		rgb = (rgb << 4) | d;
	}
	if (!ok)
	{
		SendClientMessage(playerid, COLOR_GREY, "Цвет — 6 знаков RRGGBB, например 3A6EFF.");
		ShowPlayerDialog(playerid, DIALOG_FCOLOR_HEX, DIALOG_STYLE_INPUT, "Свой цвет",
			"Введи код цвета: 6 знаков RRGGBB, например 3A6EFF.", "Сохранить", "Назад");
		return 1;
	}
	FColor_Apply(playerid, f, (rgb << 8) | 0xFF);
	return 1;
}

// =====================================================================
//  Раздевалка (ALT): переодеться или взять снаряжение со склада
// =====================================================================
stock Locker_Show(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return;
	new list[160];
	if (Player[playerid][pDuty]) strcat(list, "Переодеться в свою одежду {AFAFAF}(закончить смену)\n");
	else strcat(list, "Переодеться в форму {AFAFAF}(начать смену)\n");
	strcat(list, "Взять снаряжение {AFAFAF}(оружие, бронежилет, аптечка)");
	if (Faction_IsFbi(f) && !Player[playerid][pDuty]) strcat(list, "\nЗаступить в штатском {AFAFAF}(без формы, тайно)");
	ShowPlayerDialog(playerid, DIALOG_LOCKER, DIALOG_STYLE_LIST, Faction_GetName(f), "%s", "Выбрать", "Закрыть", list);
}

// =====================================================================
//  Взводы фракции: у каждой до MAX_SQUADS взводов. Лидер и зам (9+):
//  /squads — список, /squadname [№] [название], /setsquad [id] [№] (0 — без взвода).
//  В базе: accounts.fsquad = фракция * 10 + номер взвода (так взвод не «переедет»
//  в другую фракцию, если игрока уволили, пока он был не в сети).
// =====================================================================
new gSquad[MAX_PLAYERS];
new gSquadName[MAX_FACTIONS][MAX_SQUADS][24];
new bool:gSquadsLoaded = false;

stock Squads_LoadNames()
{
	gSquadsLoaded = true;
	DB_AddColumn("accounts", "fsquad", "INTEGER DEFAULT 0");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS faction_squads (faction INTEGER NOT NULL, squad INTEGER NOT NULL, name TEXT, PRIMARY KEY (faction, squad))"));
	for (new f = 0; f < MAX_FACTIONS; f++)
	{
		for (new q = 0; q < MAX_SQUADS; q++) format(gSquadName[f][q], 24, "%d взвод", q + 1);
	}
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT faction, squad, name FROM faction_squads");
	if (r == DBResult:0) return;
	new rows = DB_GetRowCount(r);
	for (new k = 0; k < rows; k++)
	{
		new f = DB_GetFieldInt(r, 0), q = DB_GetFieldInt(r, 1);
		if (f >= 0 && f < MAX_FACTIONS && q >= 1 && q <= MAX_SQUADS) DB_GetFieldString(r, 2, gSquadName[f][q - 1], 24);
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
}

// Из Account_FinishLogin
stock Squad_Load(playerid)
{
	if (!gSquadsLoaded) Squads_LoadNames();
	gSquad[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT fsquad FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		new v = DB_GetFieldInt(r, 0);
		if (v / 10 == Player[playerid][pFaction] && v % 10 >= 1 && v % 10 <= MAX_SQUADS) gSquad[playerid] = v % 10;
	}
	DB_FreeResultSet(r);
}

stock Squad_Of(playerid) { return gSquad[playerid]; }
stock Squads_Max() { return MAX_SQUADS; }

stock Squad_Set(playerid, q)
{
	gSquad[playerid] = q;
	if (Player[playerid][pDbId] == 0) return;
	new v = (q > 0) ? (Player[playerid][pFaction] * 10 + q) : 0;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET fsquad = %d WHERE id = %d", v, Player[playerid][pDbId]));
}

// Название взвода игрока (пусто — без взвода)
stock Squad_GetName(playerid, dest[24])
{
	dest[0] = '\0';
	new f = Player[playerid][pFaction], q = gSquad[playerid];
	if (!gSquadsLoaded) Squads_LoadNames();
	if (!Faction_IsValid(f) || q < 1 || q > MAX_SQUADS) return;
	strcat(dest, gSquadName[f][q - 1], 24);
}

stock bool:Cmd_Squads(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь во фракции.");
	if (!gSquadsLoaded) Squads_LoadNames();
	SendClientMessage(playerid, Faction_GetColor(f), "Взводы фракции «%s»:", Faction_GetName(f));
	for (new q = 1; q <= MAX_SQUADS; q++)
	{
		new n = 0;
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pFaction] == f && gSquad[i] == q) n++;
		}
		SendClientMessage(playerid, COLOR_WHITE, " %d — %s (в сети: %d)", q, gSquadName[f][q - 1], n);
	}
	SendClientMessage(playerid, COLOR_GREY, "Лидер и зам: /squadname [№] [название], /setsquad [id] [№] (0 — без взвода).");
	return true;
}

stock bool:Cmd_SquadName(playerid, const params[])
{
	if (!FPerm_Need(playerid, FP_SQUADS)) return true;
	new f = Player[playerid][pFaction], pos = 0, q, name[24];
	if (!NextIntParam(params, pos, q) || q < 1 || q > MAX_SQUADS || !RestParam(params, pos, name))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /squadname [№ 1-%d] [название]", MAX_SQUADS);
	if (!Faction_IsGoodName(name, 2, 23)) return SendClientMessage(playerid, COLOR_GREY, "Название: от 2 до 23 символов, без фигурных скобок и знака процента.");
	if (!gSquadsLoaded) Squads_LoadNames();
	gSquadName[f][q - 1][0] = '\0';
	strcat(gSquadName[f][q - 1], name, 24);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO faction_squads (faction, squad, name) VALUES (%d, %d, '%q')", f, q, name));
	new msg[144];
	format(msg, sizeof msg, "%s назвал взвод %d: «%s».", GetRPName(playerid), q, name);
	Faction_Log(f, msg, FLOG_STAFF);
	SendClientMessage(playerid, COLOR_GREEN, "Взвод %d теперь называется «%s».", q, name);
	return true;
}

stock bool:Cmd_SetSquad(playerid, const params[])
{
	if (!FPerm_Need(playerid, FP_SQUADS)) return true;
	new f = Player[playerid][pFaction], pos = 0, q;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, q) || q < 0 || q > MAX_SQUADS)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setsquad [id] [№ взвода 1-%d, 0 — без взвода]. Список: /squads", MAX_SQUADS);
	if (!Player[target][pLogged] || Player[target][pFaction] != f) return SendClientMessage(playerid, COLOR_GREY, "Этот игрок не в твоей фракции.");
	if (target != playerid && Player[target][pRank] >= Player[playerid][pRank]) return SendClientMessage(playerid, COLOR_GREY, "Нельзя управлять равным или старшим по рангу.");
	if (!gSquadsLoaded) Squads_LoadNames();
	Squad_Set(target, q);
	new msg[144];
	if (q == 0)
	{
		SendClientMessage(target, Faction_GetColor(f), "%s убрал тебя из взвода.", GetRPName(playerid));
		format(msg, sizeof msg, "%s убрал %s из взвода.", GetRPName(playerid), GetRPName(target));
	}
	else
	{
		SendClientMessage(target, Faction_GetColor(f), "%s перевёл тебя во взвод: %s.", GetRPName(playerid), gSquadName[f][q - 1]);
		format(msg, sizeof msg, "%s перевёл %s во взвод «%s».", GetRPName(playerid), GetRPName(target), gSquadName[f][q - 1]);
	}
	Faction_Log(f, msg, FLOG_STAFF);
	SendClientMessage(playerid, COLOR_GREEN, "Готово.");
	return true;
}

// /setrankname [фракция] [ранг] [название] — админ (4+) переименовывает ранг любой фракции
stock bool:Cmd_SetRankName(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, f, rk, name[24];
	if (!NextIntParam(params, pos, f) || !Faction_IsValid(f) || !NextIntParam(params, pos, rk) || rk < 1 || rk > MAX_RANK || !RestParam(params, pos, name))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setrankname [фракция] [ранг 1-%d] [название]. Лидер меняет ранги сам: /lmenu, «Ранги».", MAX_RANK);
	if (!Faction_IsGoodName(name, 2, 23)) return SendClientMessage(playerid, COLOR_GREY, "Название: от 2 до 23 символов, без фигурных скобок и знака процента.");
	gRankName[f][rk - 1][0] = '\0';
	strcat(gRankName[f][rk - 1], name, 24);
	Faction_SaveRank(f, rk);
	new msg[144];
	format(msg, sizeof msg, "Администратор %s переименовал ранг %d в «%s».", GetRPName(playerid), rk, name);
	Faction_Log(f, msg);
	SendClientMessage(playerid, COLOR_ADMIN, "Фракция «%s»: ранг %d теперь «%s».", Faction_GetName(f), rk, name);
	return true;
}

// =====================================================================
//  Торговцы чёрного рынка: /addbm — поставить здесь, /delbm — убрать ближайшего (админ 4+)
// =====================================================================
enum E_BM
{
	bool:bmUsed,
	bmDbId,
	Float:bmX, Float:bmY, Float:bmZ, Float:bmA,
	bmWorld, bmInt,
	bmActor,
	bmMk
}
new gBm[MAX_BM][E_BM];

stock BM_Build(i)
{
	if (gBm[i][bmActor] != INVALID_ACTOR_ID) { DestroyActor(gBm[i][bmActor]); gBm[i][bmActor] = INVALID_ACTOR_ID; }
	if (gBm[i][bmMk] != -1) { Marker_Destroy(gBm[i][bmMk]); gBm[i][bmMk] = -1; }
	if (!gBm[i][bmUsed]) return;
	gBm[i][bmMk] = Marker_Create(MK_FACTION, BM_REF + gBm[i][bmDbId], "Чёрный рынок", 0, gBm[i][bmX], gBm[i][bmY], gBm[i][bmZ],
		gBm[i][bmWorld], gBm[i][bmInt], GPS_NONE, gBm[i][bmA]);
	Marker_SetLabel(gBm[i][bmMk], "Торговец\n{FFFFFF}ALT — посмотреть товар", 0xC0A060FF, 10.0, 1.2);
	gBm[i][bmA] = Marker_GetAngle(gBm[i][bmMk]);
	gBm[i][bmActor] = CreateActor(BM_SKIN, gBm[i][bmX], gBm[i][bmY], gBm[i][bmZ], gBm[i][bmA]);
	if (gBm[i][bmActor] == INVALID_ACTOR_ID) return;
	SetActorVirtualWorld(gBm[i][bmActor], gBm[i][bmWorld]);
	SetActorInvulnerable(gBm[i][bmActor], true);
}

stock BM_Save(i)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE bm_points SET x = %f, y = %f, z = %f, a = %f, world = %d, interior = %d WHERE id = %d",
		gBm[i][bmX], gBm[i][bmY], gBm[i][bmZ], gBm[i][bmA], gBm[i][bmWorld], gBm[i][bmInt], gBm[i][bmDbId]));
}

stock BM_Load()
{
	for (new i = 0; i < MAX_BM; i++)
	{
		gBm[i][bmUsed] = false;
		gBm[i][bmActor] = INVALID_ACTOR_ID;
		gBm[i][bmMk] = -1;
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS bm_points (id INTEGER PRIMARY KEY AUTOINCREMENT, x REAL, y REAL, z REAL, a REAL DEFAULT 0, world INTEGER DEFAULT 0, interior INTEGER DEFAULT 0)"));
	// Перенос старого единственного торговца из /setbm (одна строка, один раз)
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM bm_points");
	new count = 0;
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (count == 0)
	{
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO bm_points (x, y, z, a, world, interior) SELECT x, y, z, a, world, interior FROM supply_points WHERE id = %d", SP_BM));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM supply_points WHERE id = %d", SP_BM));
	}
	new n = 0;
	r = DB_ExecuteQuery(gDB, "SELECT id, x, y, z, a, world, interior FROM bm_points");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new k = 0; k < rows && n < MAX_BM; k++)
		{
			gBm[n][bmUsed] = true;
			gBm[n][bmDbId] = DB_GetFieldInt(r, 0);
			gBm[n][bmX] = DB_GetFieldFloat(r, 1);
			gBm[n][bmY] = DB_GetFieldFloat(r, 2);
			gBm[n][bmZ] = DB_GetFieldFloat(r, 3);
			gBm[n][bmA] = DB_GetFieldFloat(r, 4);
			gBm[n][bmWorld] = DB_GetFieldInt(r, 5);
			gBm[n][bmInt] = DB_GetFieldInt(r, 6);
			BM_Build(n);
			n++;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Factions] %d black market traders.", n);
}

// Ближайший торговец в радиусе (-1 — нет)
stock BM_Near(playerid, Float:range)
{
	new world = GetPlayerVirtualWorld(playerid), interior = GetPlayerInterior(playerid);
	for (new i = 0; i < MAX_BM; i++)
	{
		if (!gBm[i][bmUsed] || gBm[i][bmWorld] != world || gBm[i][bmInt] != interior) continue;
		if (IsPlayerInRangeOfPoint(playerid, range, gBm[i][bmX], gBm[i][bmY], gBm[i][bmZ])) return i;
	}
	return -1;
}

// Админ сдвинул маркер торговца в /markers
stock BM_OnMarkerMoved(dbid, Float:x, Float:y, Float:z)
{
	for (new i = 0; i < MAX_BM; i++)
	{
		if (!gBm[i][bmUsed] || gBm[i][bmDbId] != dbid) continue;
		gBm[i][bmX] = x;
		gBm[i][bmY] = y;
		gBm[i][bmZ] = z;
		gBm[i][bmA] = Marker_GetAngle(gBm[i][bmMk]);
		BM_Save(i);
		if (gBm[i][bmActor] != INVALID_ACTOR_ID) DestroyActor(gBm[i][bmActor]);
		gBm[i][bmActor] = CreateActor(BM_SKIN, x, y, z, gBm[i][bmA]);
		if (gBm[i][bmActor] != INVALID_ACTOR_ID)
		{
			SetActorVirtualWorld(gBm[i][bmActor], gBm[i][bmWorld]);
			SetActorInvulnerable(gBm[i][bmActor], true);
		}
		return;
	}
}

stock bool:Cmd_AddBM(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new i = -1;
	for (new k = 0; k < MAX_BM; k++)
	{
		if (!gBm[k][bmUsed]) { i = k; break; }
	}
	if (i == -1) return SendClientMessage(playerid, COLOR_GREY, "Торговцев уже %d — это предел. Убрать лишнего: /delbm", MAX_BM);
	GetPlayerPos(playerid, gBm[i][bmX], gBm[i][bmY], gBm[i][bmZ]);
	GetPlayerFacingAngle(playerid, gBm[i][bmA]);
	gBm[i][bmWorld] = GetPlayerVirtualWorld(playerid);
	gBm[i][bmInt] = GetPlayerInterior(playerid);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO bm_points (x, y, z, a, world, interior) VALUES (%f, %f, %f, %f, %d, %d)",
		gBm[i][bmX], gBm[i][bmY], gBm[i][bmZ], gBm[i][bmA], gBm[i][bmWorld], gBm[i][bmInt]));
	gBm[i][bmDbId] = DB_LastInsertId();
	gBm[i][bmUsed] = true;
	gBm[i][bmActor] = INVALID_ACTOR_ID;
	gBm[i][bmMk] = -1;
	BM_Build(i);
	// встать в сторону, чтобы не застрять в торговце
	SetPlayerPos(playerid, gBm[i][bmX] + 1.0 * floatsin(-gBm[i][bmA], degrees), gBm[i][bmY] + 1.0 * floatcos(-gBm[i][bmA], degrees), gBm[i][bmZ]);
	SendClientMessage(playerid, COLOR_ADMIN, "Торговец чёрного рынка поставлен (№%d). Цены общие для всех: /bmprice. Убрать: /delbm рядом с ним.", gBm[i][bmDbId]);
	return true;
}

stock bool:Cmd_DelBM(playerid)
{
	if (!Admin_Check(playerid, 4)) return true;
	new i = BM_Near(playerid, 5.0);
	if (i == -1) return SendClientMessage(playerid, COLOR_GREY, "Подойди к торговцу, которого хочешь убрать.");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM bm_points WHERE id = %d", gBm[i][bmDbId]));
	if (gBm[i][bmMk] != -1) { Marker_Destroy(gBm[i][bmMk], true); gBm[i][bmMk] = -1; }
	gBm[i][bmUsed] = false;
	BM_Build(i);
	SendClientMessage(playerid, COLOR_ADMIN, "Торговец убран.");
	return true;
}

// =====================================================================
//  Окно фракции: /fr (и /faction). Информация, состав, ранги, общак,
//  склад, панель лидера. /members — окно с участниками онлайн.
// =====================================================================
stock Faction_CountAll(f)
{
	new count = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM accounts WHERE faction = %d", f);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	return count;
}

static Faction_CountOnline(f, &duty)
{
	new n = 0;
	duty = 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || Player[i][pFaction] != f) continue;
		n++;
		if (Player[i][pDuty]) duty++;
	}
	return n;
}

// /members: фракция, а если её нет — семья
stock bool:Cmd_MembersWindow(playerid)
{
	if (Faction_IsValid(Player[playerid][pFaction])) { FrMenu_Online(playerid, false); return true; }
	if (Family_Has(playerid)) { FamMenu_Online(playerid, false); return true; }
	SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь ни во фракции, ни в семье.");
	return true;
}

stock FrMenu_Show(playerid)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь во фракции. Семья: /family");
	new body[900], line[128], title[64], duty;
	new online = Faction_CountOnline(f, duty);
	format(line, sizeof line, "Информация\t{AFAFAF}твой ранг: %s\n", Faction_GetRankName(f, Player[playerid][pRank]));
	strcat(body, line);
	format(line, sizeof line, "Участники онлайн\t{33AA33}%d {AFAFAF}(на смене %d)\n", online, duty);
	strcat(body, line);
	format(line, sizeof line, "Весь состав\t%d чел.\n", Faction_CountAll(f));
	strcat(body, line);
	strcat(body, "Ранги и зарплаты\t\n");
	format(line, sizeof line, "Общак\t{33AA33}$%d {AFAFAF}(на базе)\n", Faction[f][fBank]);
	strcat(body, line);
	format(line, sizeof line, "Склад\t{AFAFAF}материалов %d (на базе)\n", Faction[f][fMats]);
	strcat(body, line);
	if (FPerm_IsManager(playerid)) strcat(body, "{FFFF00}Управление\t{AFAFAF}состав, права, журнал\n");
	else strcat(body, "{AFAFAF}Управление\t{AFAFAF}лидер, зам и кому выдали права\n");
	strcat(body, "Все команды фракций\t{AFAFAF}/fhelp\n");
	strcat(body, "{FF6347}Покинуть фракцию\t");
	format(title, sizeof title, "{%06x}%s", Faction_GetColor(f) >>> 8, Faction_GetName(f));
	ShowPlayerDialog(playerid, DIALOG_FR_MENU, DIALOG_STYLE_TABLIST, title, "%s", "Выбрать", "Закрыть", body);
	return 1;
}

static FrMenu_Info(playerid)
{
	new f = Player[playerid][pFaction], rank = Player[playerid][pRank];
	new leaders[96], deputies[96], name[MAX_PLAYER_NAME];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT name, frank FROM accounts WHERE faction = %d AND frank >= %d ORDER BY frank DESC, name LIMIT 6", f, MAX_RANK - 1);
	if (r != DBResult:0)
	{
		for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
		{
			DB_GetFieldString(r, 0, name, sizeof name);
			if (DB_GetFieldInt(r, 1) >= MAX_RANK)
			{
				if (leaders[0] != '\0') strcat(leaders, ", ");
				strcat(leaders, name);
			}
			else
			{
				if (deputies[0] != '\0') strcat(deputies, ", ");
				strcat(deputies, name);
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	if (leaders[0] == '\0') strcat(leaders, "нет");
	if (deputies[0] == '\0') strcat(deputies, "нет");

	new duty, online = Faction_CountOnline(f, duty);
	new salary = (rank >= 1 && rank <= MAX_RANK) ? gRankSalary[f][rank - 1] : 0;
	if (salary > Faction_MaxSalary(f)) salary = Faction_MaxSalary(f);
	if (salary < 0) salary = 0;

	new text[1500], line[160];
	format(line, sizeof line, "{AFAFAF}Фракция: {%06x}%s\n", Faction_GetColor(f) >>> 8, Faction_GetName(f));
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Тип: {FFFFFF}%s\n", gFTypeName[Faction[f][fType]]);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Лидер: {FFFFFF}%s\n", leaders);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Заместители: {FFFFFF}%s\n", deputies);
	strcat(text, line);
	format(line, sizeof line, "\n{AFAFAF}Состав: {FFFFFF}%d {AFAFAF}(в игре: {33AA33}%d{AFAFAF}, на смене: {33AA33}%d{AFAFAF})\n", Faction_CountAll(f), online, duty);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Общак: {33AA33}$%d\n", Faction[f][fBank]);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Материалы: {FFFFFF}%d / %d\n", Faction[f][fMats], FACTION_MATS_MAX);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Склад: {FFFFFF}%s\n", FPerm_Has(playerid, FP_STORE_TAKE) ? ("тебе можно брать") : ("тебе брать нельзя"));
	strcat(text, line);
	format(line, sizeof line, "\n{AFAFAF}Твой ранг: {FFFFFF}%s (%d из %d)\n", Faction_GetRankName(f, rank), rank, MAX_RANK);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Зарплата: {33AA33}$%d {AFAFAF}за PayDay (из общака)\n", salary);
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Выговоры: {FFFFFF}%d / %d\n", Faction_GetWarns(Player[playerid][pDbId]), Org_WarnLimit(f));
	strcat(text, line);
	format(line, sizeof line, "{AFAFAF}Смена: %s\n", Player[playerid][pDuty] ? ("{33AA33}на смене") : ("{FFFFFF}не на смене {AFAFAF}(/duty в раздевалке)"));
	strcat(text, line);
	strcat(text, "\n{AFAFAF}Рация: {FFFFFF}/f [текст]");
	ShowPlayerDialog(playerid, DIALOG_FR_BACK, DIALOG_STYLE_MSGBOX, "Информация о фракции", "%s", "Назад", "", text);
}

// Онлайн участники фракции. fromMenu: «Назад» вернёт в меню
stock FrMenu_Online(playerid, bool:fromMenu)
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsValid(f)) return SendClientMessage(playerid, COLOR_GREY, "Ты не состоишь во фракции.");
	new body[3000], line[128], count = 0, duty = 0;
	strcat(body, "Игрок\tРанг\tСмена\tТелефон\n");
	for (new rk = MAX_RANK; rk >= 1; rk--)     // сверху старшие
	{
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (!IsPlayerConnected(i) || !Player[i][pLogged] || Player[i][pFaction] != f || Player[i][pRank] != rk) continue;
			new phone[16];
			if (Player[i][pPhone] > 0) format(phone, sizeof phone, "%d", Player[i][pPhone]);
			else strcat(phone, "{AFAFAF}нет");
			format(line, sizeof line, "%s [%d]\t%d. %s\t%s\t%s\n", GetRPName(i), i, rk, Faction_GetRankName(f, rk),
				Player[i][pDuty] ? ("{33AA33}на смене") : ("{AFAFAF}—"), phone);
			if (strlen(body) + strlen(line) >= sizeof body) break;
			strcat(body, line);
			count++;
			if (Player[i][pDuty]) duty++;
		}
	}
	new title[80];
	format(title, sizeof title, "%s — в игре %d, на смене %d", Faction_GetName(f), count, duty);
	ShowPlayerDialog(playerid, DIALOG_FR_ONLINE, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", fromMenu ? ("Назад") : ("Меню"), "Закрыть", body);
	return 1;
}

// Весь состав без управления (для рядовых; лидеру — /lmenu)
static FrMenu_All(playerid)
{
	new f = Player[playerid][pFaction];
	new body[3200], line[128], name[MAX_PLAYER_NAME], count = 0;
	strcat(body, "Имя\tРанг\tСтатус\n");
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, name, frank FROM accounts WHERE faction = %d ORDER BY frank DESC, name LIMIT %d", f, FACTION_LIST_MAX);
	if (r != DBResult:0)
	{
		for (new i = 0, rows = DB_GetRowCount(r); i < rows; i++)
		{
			DB_GetFieldString(r, 1, name, sizeof name);
			new rank = DB_GetFieldInt(r, 2), online = Faction_FindOnline(DB_GetFieldInt(r, 0));
			if (online != INVALID_PLAYER_ID)
			{
				rank = Player[online][pRank];
				format(line, sizeof line, "%s\t%d. %s\t{33AA33}в игре [%d]\n", name, rank, Faction_GetRankName(f, rank), online);
			}
			else format(line, sizeof line, "%s\t%d. %s\t{AFAFAF}не в игре\n", name, rank, Faction_GetRankName(f, rank));
			strcat(body, line);
			count++;
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	new title[64];
	format(title, sizeof title, "Состав фракции (%d)", count);
	ShowPlayerDialog(playerid, DIALOG_FR_BACK, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Назад", "Закрыть", body);
}

static FrMenu_Ranks(playerid)
{
	new f = Player[playerid][pFaction], body[900], line[128];
	strcat(body, "Ранг\tНазвание\tЗарплата\n");
	for (new rk = MAX_RANK; rk >= 1; rk--)
	{
		new salary = gRankSalary[f][rk - 1];
		if (salary > Faction_MaxSalary(f)) salary = Faction_MaxSalary(f);
		if (salary < 0) salary = 0;
		if (rk == Player[playerid][pRank])
			format(line, sizeof line, "{FFFF00}%d\t{FFFF00}%s (ты)\t{FFFF00}$%d\n", rk, Faction_GetRankName(f, rk), salary);
		else
			format(line, sizeof line, "%d\t%s\t$%d\n", rk, Faction_GetRankName(f, rk), salary);
		strcat(body, line);
	}
	ShowPlayerDialog(playerid, DIALOG_FR_BACK, DIALOG_STYLE_TABLIST_HEADERS, "Ранги и зарплаты (за PayDay)", "%s", "Назад", "Закрыть", body);
}

stock FrMenu_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid < DIALOG_FR_MENU || dialogid > DIALOG_FR_LEAVE) return 0;
	if (!Faction_IsValid(Player[playerid][pFaction])) return 1;

	switch (dialogid)
	{
		case DIALOG_FR_MENU:
		{
			if (!response) return 1;
			switch (listitem)
			{
				case 0: FrMenu_Info(playerid);
				case 1: FrMenu_Online(playerid, true);
				case 2:
				{
					if (FPerm_IsManager(playerid)) Faction_ShowMembers(playerid);
					else FrMenu_All(playerid);
				}
				case 3: FrMenu_Ranks(playerid);
				case 4: Cmd_FBank(playerid);
				case 5: Cmd_FStorage(playerid);
				case 6: Cmd_LMenu(playerid);
				case 7: Cmd_FHelp(playerid);
				case 8:
				{
					ShowPlayerDialog(playerid, DIALOG_FR_LEAVE, DIALOG_STYLE_MSGBOX, "Покинуть фракцию",
						"Точно уйти из «%s»?\nВернуться можно только по приглашению.", "Уйти", "Назад", Faction_GetName(Player[playerid][pFaction]));
				}
			}
		}
		case DIALOG_FR_BACK, DIALOG_FR_ONLINE:
		{
			if (response) FrMenu_Show(playerid);
		}
		case DIALOG_FR_LEAVE:
		{
			if (!response) { FrMenu_Show(playerid); return 1; }
			Cmd_FLeave(playerid);
		}
	}
	return 1;
}

// Перенос строк фракции a на номер b (из Factions_RunMoves)
static Faction_MoveRows(a, b)
{
	static const tables[][] = { "faction_ranks", "faction_storage", "faction_logs", "faction_doors", "faction_door_defaults",
		"fvehicles", "faction_stores", "faction_squads", "gates", "faction_perms" };
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE factions SET id = %d WHERE id = %d", b, a));
	for (new i = 0; i < sizeof tables; i++)
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE %s SET faction = %d WHERE faction = %d", tables[i], b, a));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET faction = %d WHERE faction = %d", b, a));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET fsquad = %d + (fsquad %% 10) WHERE fsquad / 10 = %d", b * 10, a));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE turfs SET owner = %d WHERE owner = %d", b, a));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE factories SET owner = %d WHERE owner = %d", b, a));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE doors SET faction = %d WHERE faction = %d", b, a));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE markers SET faction = %d WHERE faction = %d", b, a));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO faction_hidden (id, hidden) SELECT %d, hidden FROM faction_hidden WHERE id = %d", b, a));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM faction_hidden WHERE id = %d", a));
	// правки маркеров из /markers: раздевалка, склад, вход в здание
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE markers SET ref = %d WHERE type = %d AND ref = %d", b, MK_FACTION, a));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE markers SET ref = %d WHERE type = %d AND ref = %d", STORE_REF + b, MK_FACTION, STORE_REF + a));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE markers SET ref = %d WHERE type = %d AND ref = %d", FDOOR_REF + b * 2, MK_DOOR, FDOOR_REF + a * 2));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE markers SET ref = %d WHERE type = %d AND ref = %d", FDOOR_REF + b * 2 + 1, MK_DOOR, FDOOR_REF + a * 2 + 1));
}

// /gov [текст] — объявление правительства всем (ранг 5+ правительства)
stock bool:Cmd_Gov(playerid, const params[])
{
	new f = Player[playerid][pFaction];
	if (!Faction_IsGov(f)) return SendClientMessage(playerid, COLOR_GREY, "Это доступно только правительству.");
	if (Player[playerid][pRank] < 5) return SendClientMessage(playerid, COLOR_GREY, "Объявления делает ранг 5 и выше.");
	if (params[0] == '\0') return SendClientMessage(playerid, COLOR_GREY, "Использование: /gov [текст] — объявление правительства для всех");
	new msg[144];
	format(msg, sizeof msg, "[Правительство] %s (%s): %s", GetRPName(playerid), Faction_GetRankName(f, Player[playerid][pRank]), params);
	SendClientMessageToAll(0xE8E8E8FF, msg);
	return true;
}

// Разовые правки базы при запуске (каждая — один раз, имя пишется в server_migrations)
public Factions_Migrate()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS server_migrations (name TEXT PRIMARY KEY)"));
	new bool:done = false;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM server_migrations WHERE name = 'secret_faction_fbi'");
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) done = DB_GetFieldInt(r, 0) > 0;
		DB_FreeResultSet(r);
	}
	if (!done)
	{
	// скрытая фракция (бывшая 8) — не полиция, а спецслужба как ФБР
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE factions SET ftype = %d WHERE id = %d AND ftype = %d", FTYPE_FBI, FACTION_SECRET_ID, FTYPE_LAW));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR IGNORE INTO server_migrations (name) VALUES ('secret_faction_fbi')"));
	}
	Factions_MigrateInteriors();
	return 1;
}

// Армия и банды из тесных залов interiors.inc переезжают в большие интерьеры GTA (у каждой банды свой).
// Трогаем только то, что стоит в старом зале: вход внутри, спавн, раздевалку и их правки из /markers.
static const Float:gOrgIntMove[][] =
{
	// фракция, центр старого зала x, y, новый интерьер, x, y, z
	{ 7.0, 2650.0, -2850.0, 18.0, 1290.41,    1.95, 1001.02 },  // армия -> большой склад
	{ 3.0, 2450.0, -2850.0,  5.0, 1267.84, -776.96, 1091.91 },  // Grove -> особняк Мэдд Догга
	{ 4.0, 2450.0, -2850.0,  2.0, 2536.53, -1294.84, 1044.12 }, // Ballas -> наркопритон Биг Смоука
	{ 5.0, 2450.0, -2850.0,  3.0, -2636.72, 1402.92, 906.46 }   // Aztecas -> клуб Джиззи
};

static Factions_MigrateInteriors()
{
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM server_migrations WHERE name = 'org_interiors_v2'");
	new bool:done = false;
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) done = DB_GetFieldInt(r, 0) > 0;
		DB_FreeResultSet(r);
	}
	if (done) return;
	// спавн у мэрии совпадал с автошколой: кто там спавнится и имеет свой офис — спавнится внутри офиса
	DB_FreeResultSet(DB_ExecuteQuery(gDB,
		"UPDATE factions SET sx = (SELECT d.ix FROM faction_doors d WHERE d.faction = factions.id), sy = (SELECT d.iy FROM faction_doors d WHERE d.faction = factions.id), sz = (SELECT d.iz FROM faction_doors d WHERE d.faction = factions.id), sa = (SELECT d.ia FROM faction_doors d WHERE d.faction = factions.id), sint = (SELECT d.iint FROM faction_doors d WHERE d.faction = factions.id), sworld = (SELECT d.iworld FROM faction_doors d WHERE d.faction = factions.id) WHERE abs(sx - %f) < 6 AND abs(sy - %f) < 6 AND EXISTS (SELECT 1 FROM faction_doors d WHERE d.faction = factions.id AND d.has_in = 1)",
		LIC_SCHOOL_X, LIC_SCHOOL_Y));
	for (new i = 0; i < sizeof gOrgIntMove; i++)
	{
		new f = floatround(gOrgIntMove[i][0]), inter = floatround(gOrgIntMove[i][3]);
		new Float:cx = gOrgIntMove[i][1], Float:cy = gOrgIntMove[i][2];
		new Float:nx = gOrgIntMove[i][4], Float:ny = gOrgIntMove[i][5], Float:nz = gOrgIntMove[i][6];
		DB_FreeResultSet(DB_ExecuteQuery(gDB,
			"UPDATE faction_doors SET iint = %d, ix = %f, iy = %f, iz = %f, ia = 0 WHERE faction = %d AND iint = 1 AND iz > 1400 AND abs(ix - %f) < 40 AND abs(iy - %f) < 40",
			inter, nx, ny, nz, f, cx, cy));
		DB_FreeResultSet(DB_ExecuteQuery(gDB,
			"UPDATE factions SET sint = %d, sx = %f, sy = %f, sz = %f WHERE id = %d AND sint = 1 AND sz > 1400 AND abs(sx - %f) < 40 AND abs(sy - %f) < 40",
			inter, nx, ny, nz, f, cx, cy));
		DB_FreeResultSet(DB_ExecuteQuery(gDB,
			"UPDATE factions SET dint = %d, dx = %f, dy = %f, dz = %f WHERE id = %d AND dint = 1 AND dz > 1400 AND abs(dx - %f) < 40 AND abs(dy - %f) < 40",
			inter, nx + 1.5, ny, nz, f, cx, cy));
		// правки маркеров (/markers) в старом зале больше не нужны
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM markers WHERE type = %d AND ref = %d AND z > 1400", MK_FACTION, f));
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM markers WHERE type = %d AND ref = %d AND z > 1400", MK_DOOR, FDOOR_REF + f * 2 + 1));
	}
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR IGNORE INTO server_migrations (name) VALUES ('org_interiors_v2')"));
	print("[Factions] Army and gang bases moved to large GTA interiors.");
}

// /setftype [№] [тип] — сменить тип фракции, созданной в игре (админ 7)
stock bool:Cmd_SetFType(playerid, const params[])
{
	if (!Admin_Check(playerid, ADMIN_MAX_LEVEL)) return true;
	new pos = 0, f, type;
	if (!NextIntParam(params, pos, f) || !NextIntParam(params, pos, type) || !Faction_IsValid(f) || f < FACTION_BUILTIN
		|| type < FTYPE_LAW || type > FTYPE_GOV || type == FTYPE_ARMY)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setftype [№ фракции 8+] [тип: 1 силовая, 2 медики, 3 банда, 4 гос., 6 тюрьма, 7 спецслужба, 8 правительство]");
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pFaction] == f) Faction_EndDuty(i);
	}
	Faction[f][fType] = type;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE factions SET ftype = %d WHERE id = %d", type, f));
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pFaction] == f) Faction_UpdateColor(i);
	}
	SendClientMessage(playerid, COLOR_ADMIN, "Фракция %d «%s» теперь: %s. Участники сняты со смены.", f, Faction_GetName(f), gFTypeName[type]);
	return true;
}
