// =====================================================================
//  drugs.inc — наркотики: выращивание, торговля, эффекты, ломка, изъятие.
//
//  Семена конопли продаёт торговец (точка /place dealer, ALT рядом с ним).
//  Посадить: сумка (/inv) > Семена > Использовать. Куст растёт PLANT_GROW_MIN минут,
//  потом ALT рядом — собрать урожай (собрать может любой, кто нашёл куст).
//  Полицейский на смене ALT у куста — уничтожить.
//  Торговец покупает марихуану и продаёт кокаин (кокаин — только членам банд).
//  Продать другому игроку: /selldrugs [id] [weed|coke] [грамм] [цена], он принимает Y или /accept.
//
//  Употребить: из сумки. Марихуана +15 HP, кокаин +25 брони. Эффект на экране:
//  цветная пелена, качка, странная погода. Каждый раз растёт зависимость; при
//  сильной зависимости без дозы начинается ломка: экран серый, качает, отнимает HP.
//  Зависимость сама проходит без употребления, быстрее — у врача (/place clinic).
//
//  Полиция: /frisk показывает наркотики, /seize [id] — изъять всё (на смене).
// =====================================================================


#define DRUG_PLANT_MODEL        19473
#define MAX_PLANTS              150
#define PLANT_GROW_MIN          40      // минут до урожая
#define PLANT_ROT_MIN           180     // столько минут после созревания куст ещё стоит
#define PLANT_PER_PLAYER        5
#define PLANT_YIELD_MIN         5       // грамм с куста
#define PLANT_YIELD_RANDOM      6
#define DEALER_SEED_PRICE       60
#define DEALER_COKE_PRICE       200
#define DEALER_WEED_BUY         30      // столько торговец платит за 1 г марихуаны
#define DRUG_POLICE_REWARD      150     // полицейскому за уничтоженный куст (из казны)

#define DRUG_FX_WEED_SEC        60
#define DRUG_FX_COKE_SEC        45
#define ADDICT_WEED             4       // + к зависимости за дозу
#define ADDICT_COKE             10
#define ADDICT_MAX              100
#define ADDICT_WITHDRAW         40      // с этой зависимостью начинается ломка
#define WITHDRAW_AFTER_MIN      30      // через столько минут без дозы
#define DRUG_USE_COOLDOWN       20      // секунд между дозами

enum
{
	FX_NONE,
	FX_WEED,
	FX_COKE,
	FX_WITHDRAW
}

enum E_PLANT
{
	bool:plnUsed,
	plnDbId,
	plnOwner,                   // аккаунт посадившего
	plnPlanted,                 // время посадки (gettime)
	Float:plnX, Float:plnY, Float:plnZ,
	plnObject,
	Text3D:plnLabel
}
static gPlant[MAX_PLANTS][E_PLANT];

static gAddict[MAX_PLAYERS];
static gLastDose[MAX_PLAYERS];              // gettime() последней дозы
static gFx[MAX_PLAYERS];
static gFxLeft[MAX_PLAYERS];
static gFxTick[MAX_PLAYERS];
static gDoseTick[MAX_PLAYERS];
static PlayerText:gFxTD[MAX_PLAYERS] = { PlayerText:INVALID_PLAYER_TEXT_DRAW, ... };
static gSaleItem[MAX_PLAYERS];              // что предложили купить (у покупателя)
static gSaleAmount[MAX_PLAYERS];
static gDealerPlace[MAX_PLAYERS];

// ---------------------------------------------------------------------
//  Загрузка
// ---------------------------------------------------------------------
stock Drugs_Init()
{
	DB_AddColumn("accounts", "addict", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "last_dose", "INTEGER DEFAULT 0");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS drug_plants (id INTEGER PRIMARY KEY AUTOINCREMENT, owner INTEGER, planted INTEGER, x REAL, y REAL, z REAL)"));
	for (new i = 0; i < MAX_PLANTS; i++) { gPlant[i][plnUsed] = false; gPlant[i][plnLabel] = INVALID_3DTEXT_ID; }

	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, owner, planted, x, y, z FROM drug_plants LIMIT %d", MAX_PLANTS);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			Plant_Add(DB_GetFieldInt(r, 0), DB_GetFieldInt(r, 1), DB_GetFieldInt(r, 2),
				DB_GetFieldFloat(r, 3), DB_GetFieldFloat(r, 4), DB_GetFieldFloat(r, 5));
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	SetTimer("Drugs_OnSecond", 1000, true);
}

stock Drugs_LoadPlayer(playerid)
{
	gAddict[playerid] = 0;
	gLastDose[playerid] = 0;
	gFx[playerid] = FX_NONE;
	gFxLeft[playerid] = 0;
	gDoseTick[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT addict, last_dose FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		gAddict[playerid] = DB_GetFieldInt(r, 0);
		gLastDose[playerid] = DB_GetFieldInt(r, 1);
	}
	DB_FreeResultSet(r);
}

static Drugs_Save(playerid)
{
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET addict = %d, last_dose = %d WHERE id = %d",
		gAddict[playerid], gLastDose[playerid], Player[playerid][pDbId]));
}

stock Drugs_OnDisconnect(playerid)
{
	Drugs_StopFx(playerid);
	if (gFxTD[playerid] != PlayerText:INVALID_PLAYER_TEXT_DRAW)
	{
		PlayerTextDrawDestroy(playerid, gFxTD[playerid]);
		gFxTD[playerid] = PlayerText:INVALID_PLAYER_TEXT_DRAW;
	}
	gSaleAmount[playerid] = 0;
}

stock Drugs_GetAddict(playerid) { return gAddict[playerid]; }

// Врач лечит зависимость (hospital.inc)
stock Drugs_Cure(playerid)
{
	gAddict[playerid] = 0;
	Drugs_StopFx(playerid);
	Drugs_Save(playerid);
}

// ---------------------------------------------------------------------
//  Эффект на экране: цветная пелена во весь экран, качка, погода
// ---------------------------------------------------------------------
static Drugs_Overlay(playerid, colour)
{
	if (gFxTD[playerid] == PlayerText:INVALID_PLAYER_TEXT_DRAW)
	{
		gFxTD[playerid] = CreatePlayerTextDraw(playerid, -10.0, -10.0, "_");
		PlayerTextDrawLetterSize(playerid, gFxTD[playerid], 0.0, 55.0);
		PlayerTextDrawTextSize(playerid, gFxTD[playerid], 660.0, 0.0);
		PlayerTextDrawUseBox(playerid, gFxTD[playerid], true);
	}
	PlayerTextDrawBoxColour(playerid, gFxTD[playerid], colour);
	PlayerTextDrawShow(playerid, gFxTD[playerid]);
}

static Drugs_StartFx(playerid, type, seconds)
{
	gFx[playerid] = type;
	gFxLeft[playerid] = seconds;
	gFxTick[playerid] = 0;
	switch (type)
	{
		case FX_WEED:
		{
			SetPlayerWeather(playerid, 9);              // туман
			SetPlayerDrunkLevel(playerid, 4500);        // камеру качает
			Drugs_Overlay(playerid, 0x30FF3040);
			GameTextForPlayer(playerid, "~g~...", 2000, 1);
		}
		case FX_COKE:
		{
			SetPlayerWeather(playerid, -66);            // кислотные цвета
			SetPlayerDrunkLevel(playerid, 2500);
			Drugs_Overlay(playerid, 0xFFFFFF50);
			GameTextForPlayer(playerid, "~w~!!!", 1500, 1);
		}
		case FX_WITHDRAW:
		{
			SetPlayerDrunkLevel(playerid, 6000);
			Drugs_Overlay(playerid, 0x20202080);
			GameTextForPlayer(playerid, "~r~lomka", 3000, 1);
		}
	}
}

stock Drugs_StopFx(playerid)
{
	if (gFx[playerid] == FX_NONE) return;
	gFx[playerid] = FX_NONE;
	gFxLeft[playerid] = 0;
	if (!IsPlayerConnected(playerid)) return;
	if (gFxTD[playerid] != PlayerText:INVALID_PLAYER_TEXT_DRAW) PlayerTextDrawHide(playerid, gFxTD[playerid]);
	SetPlayerWeather(playerid, gServerWeather);
	SetPlayerDrunkLevel(playerid, 0);
}

forward Drugs_OnSecond();
public Drugs_OnSecond()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gDoseTick[i] > 0) gDoseTick[i]--;
		if (gFx[i] == FX_NONE || !IsPlayerConnected(i)) continue;
		gFxTick[i]++;
		// пелена «дышит»: яркость меняется каждую секунду
		switch (gFx[i])
		{
			case FX_WEED:
			{
				static const cols[] = { 0x30FF3040, 0x60FF2050, 0x20C0FF40, 0xC0FF2050 };
				Drugs_Overlay(i, cols[gFxTick[i] % sizeof (cols)]);
				if (gFxTick[i] % 10 == 0) SetPlayerDrunkLevel(i, 4500);
			}
			case FX_COKE:
			{
				static const cols[] = { 0xFFFFFF60, 0xFF40FF40, 0x40FFFF40, 0xFFFF4050, 0x00000000 };
				Drugs_Overlay(i, cols[gFxTick[i] % sizeof (cols)]);
			}
			case FX_WITHDRAW:
			{
				Drugs_Overlay(i, (gFxTick[i] % 2) ? 0x30303090 : 0x10101060);
			}
		}
		if (--gFxLeft[i] <= 0) Drugs_StopFx(i);
	}
}

// ---------------------------------------------------------------------
//  Раз в минуту: кусты растут, зависимость и ломка
// ---------------------------------------------------------------------
stock Drugs_OnMinute()
{
	new now = gettime();
	for (new p = 0; p < MAX_PLANTS; p++)
	{
		if (!gPlant[p][plnUsed]) continue;
		new mins = (now - gPlant[p][plnPlanted]) / 60;
		if (mins > PLANT_GROW_MIN + PLANT_ROT_MIN) { Plant_Remove(p); continue; }
		Plant_UpdateLabel(p);
	}

	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || gAddict[i] <= 0) continue;
		new since = (now - gLastDose[i]) / 60;
		if (since >= 5 && since % 5 == 0)
		{
			gAddict[i] -= 2;                    // без доз зависимость слабеет
			if (gAddict[i] < 0) gAddict[i] = 0;
			Drugs_Save(i);
		}
		if (gAddict[i] >= ADDICT_WITHDRAW && since >= WITHDRAW_AFTER_MIN && gFx[i] == FX_NONE && !Wound_IsWounded(i))
		{
			new Float:hp;
			GetPlayerHealth(i, hp);
			if (hp > 25.0) SetPlayerHealth(i, hp - 3.0);
			Drugs_StartFx(i, FX_WITHDRAW, 15);
			SendClientMessage(i, COLOR_ORANGE, "Ломка: тебя трясёт и мутит. Нужна доза или врач (регистратура больницы).");
		}
	}
}

// ---------------------------------------------------------------------
//  Употребление и посадка (из сумки, needs.inc)
// ---------------------------------------------------------------------
stock Drugs_UseItem(playerid, item)
{
	switch (item)
	{
		case INV_SEEDS: Plant_Try(playerid);
		case INV_WEED, INV_COCAINE: Drugs_Use(playerid, item);
		case INV_FISH: SendClientMessage(playerid, COLOR_GREY, "Рыбу сдают на работе рыбака: подойди к значку работы и нажми ALT.");
	}
}

static Drugs_Use(playerid, item)
{
	if (IsPlayerInAnyVehicle(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Выйди из машины.");
	if (gDoseTick[playerid] > 0) return SendClientMessage(playerid, COLOR_GREY, "Слишком часто. Подожди %d сек.", gDoseTick[playerid]);
	if (!Inv_Remove(playerid, item, 1)) return 0;
	gDoseTick[playerid] = DRUG_USE_COOLDOWN;
	gLastDose[playerid] = gettime();
	new msg[128];
	if (item == INV_WEED)
	{
		new Float:hp;
		GetPlayerHealth(playerid, hp);
		hp += 15.0;
		if (hp > 100.0) hp = 100.0;
		SetPlayerHealth(playerid, hp);
		ApplyAnimation(playerid, "SMOKING", "M_smk_drag", 4.1, false, false, false, false, 0);
		gAddict[playerid] += ADDICT_WEED;
		Drugs_StartFx(playerid, FX_WEED, DRUG_FX_WEED_SEC);
		format(msg, sizeof msg, "%s скручивает косяк и затягивается.", GetRPName(playerid));
	}
	else
	{
		new Float:ar;
		GetPlayerArmour(playerid, ar);
		ar += 25.0;
		if (ar > 100.0) ar = 100.0;
		SetPlayerArmour(playerid, ar);
		ApplyAnimation(playerid, "PED", "IDLE_tired", 4.1, false, false, false, false, 0);
		gAddict[playerid] += ADDICT_COKE;
		Drugs_StartFx(playerid, FX_COKE, DRUG_FX_COKE_SEC);
		format(msg, sizeof msg, "%s что-то вдыхает с тыльной стороны ладони.", GetRPName(playerid));
	}
	if (gAddict[playerid] > ADDICT_MAX) gAddict[playerid] = ADDICT_MAX;
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	if (gAddict[playerid] >= ADDICT_WITHDRAW)
		SendClientMessage(playerid, COLOR_ORANGE, "Зависимость: %d%%. Без дозы через %d мин. начнётся ломка.", gAddict[playerid], WITHDRAW_AFTER_MIN);
	Drugs_Save(playerid);
	return 1;
}

// ---------------------------------------------------------------------
//  Кусты
// ---------------------------------------------------------------------
static Plant_Add(dbid, owner, planted, Float:x, Float:y, Float:z)
{
	for (new p = 0; p < MAX_PLANTS; p++)
	{
		if (gPlant[p][plnUsed]) continue;
		gPlant[p][plnUsed] = true;
		gPlant[p][plnDbId] = dbid;
		gPlant[p][plnOwner] = owner;
		gPlant[p][plnPlanted] = planted;
		gPlant[p][plnX] = x; gPlant[p][plnY] = y; gPlant[p][plnZ] = z;
		gPlant[p][plnObject] = CreateObject(DRUG_PLANT_MODEL, x, y, z - 1.0, 0.0, 0.0, float(random(360)), 150.0);
		gPlant[p][plnLabel] = Create3DTextLabel("_", 0x9ACD32FF, x, y, z, 8.0, 0, true);
		Plant_UpdateLabel(p);
		return p;
	}
	return -1;
}

static Plant_Percent(p)
{
	new pct = (gettime() - gPlant[p][plnPlanted]) * 100 / (PLANT_GROW_MIN * 60);
	if (pct > 100) pct = 100;
	if (pct < 0) pct = 0;
	return pct;
}

static Plant_UpdateLabel(p)
{
	new text[64];
	new pct = Plant_Percent(p);
	if (pct >= 100) strcat(text, "{9ACD32}Конопля созрела\n{FFFFFF}ALT — собрать");
	else format(text, sizeof text, "{9ACD32}Конопля\n{FFFFFF}растёт: %d%%", pct);
	Update3DTextLabelText(gPlant[p][plnLabel], 0x9ACD32FF, text);
}

static Plant_Remove(p)
{
	if (!gPlant[p][plnUsed]) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM drug_plants WHERE id = %d", gPlant[p][plnDbId]));
	DestroyObject(gPlant[p][plnObject]);
	Delete3DTextLabel(gPlant[p][plnLabel]);
	gPlant[p][plnLabel] = INVALID_3DTEXT_ID;
	gPlant[p][plnUsed] = false;
}

static Plant_Nearby(playerid, Float:range)
{
	if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return -1;
	for (new p = 0; p < MAX_PLANTS; p++)
		if (gPlant[p][plnUsed] && IsPlayerInRangeOfPoint(playerid, range, gPlant[p][plnX], gPlant[p][plnY], gPlant[p][plnZ])) return p;
	return -1;
}

static Plant_Try(playerid)
{
	if (IsPlayerInAnyVehicle(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Выйди из машины.");
	if (GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) return SendClientMessage(playerid, COLOR_GREY, "Сажать можно только на улице.");
	if (Plant_Nearby(playerid, 3.0) != -1) return SendClientMessage(playerid, COLOR_GREY, "Слишком близко к другому кусту.");
	new mine = 0;
	for (new p = 0; p < MAX_PLANTS; p++) if (gPlant[p][plnUsed] && gPlant[p][plnOwner] == Player[playerid][pDbId]) mine++;
	if (mine >= PLANT_PER_PLAYER) return SendClientMessage(playerid, COLOR_GREY, "У тебя уже растёт %d кустов: больше не уследить.", PLANT_PER_PLAYER);
	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	if (!Inv_Remove(playerid, INV_SEEDS, 1)) return 0;
	new now = gettime();
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO drug_plants (owner, planted, x, y, z) VALUES (%d, %d, %f, %f, %f)", Player[playerid][pDbId], now, x, y, z));
	new dbid = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT last_insert_rowid()");
	if (r != DBResult:0) { if (DB_GetRowCount(r) > 0) dbid = DB_GetFieldInt(r, 0); DB_FreeResultSet(r); }
	if (Plant_Add(dbid, Player[playerid][pDbId], now, x, y, z) == -1)
	{
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM drug_plants WHERE id = %d", dbid));
		Inv_Add(playerid, INV_SEEDS, 1);
		return SendClientMessage(playerid, COLOR_GREY, "Сейчас посадить нельзя: слишком много кустов на карте.");
	}
	ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, false, false, false, false, 0);
	SendClientMessage(playerid, COLOR_GREEN, "Ты посадил коноплю. Созреет через %d мин. Не забудь вернуться: урожай может забрать любой.", PLANT_GROW_MIN);
	return 1;
}

// ALT у куста: собрать / уничтожить (полиция на смене)
static bool:Plant_TryAlt(playerid)
{
	new p = Plant_Nearby(playerid, 2.0);
	if (p == -1) return false;
	new msg[128];
	if (Faction_IsLaw(Player[playerid][pFaction]) && Player[playerid][pDuty])
	{
		format(msg, sizeof msg, "%s вырывает куст конопли с корнем.", GetRPName(playerid));
		SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
		Plant_Remove(p);
		if (Econ_FromTreasury(DRUG_POLICE_REWARD))
		{
			GiveMoney(playerid, DRUG_POLICE_REWARD, "премия: уничтожен куст");
			SendClientMessage(playerid, COLOR_GREEN, "Куст уничтожен. Премия из казны: $%d.", DRUG_POLICE_REWARD);
		}
		else SendClientMessage(playerid, COLOR_GREEN, "Куст уничтожен.");
		return true;
	}
	if (Plant_Percent(p) < 100)
	{
		SendClientMessage(playerid, COLOR_GREY, "Куст ещё не созрел: %d%%.", Plant_Percent(p));
		return true;
	}
	new grams = PLANT_YIELD_MIN + random(PLANT_YIELD_RANDOM);
	new room = Inv_GetMax(INV_WEED) - Inv_Count(playerid, INV_WEED);
	if (room <= 0) { SendClientMessage(playerid, COLOR_GREY, "Сумка полная: больше %d г не унести.", Inv_GetMax(INV_WEED)); return true; }
	if (grams > room) grams = room;
	Inv_Add(playerid, INV_WEED, grams);
	ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, false, false, false, false, 0);
	format(msg, sizeof msg, "%s срезает куст и прячет листья в сумку.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(playerid, COLOR_GREEN, "Урожай: %d г марихуаны (в сумке %d г).", grams, Inv_Count(playerid, INV_WEED));
	Plant_Remove(p);
	return true;
}

// ---------------------------------------------------------------------
//  Торговец (ALT у точки dealer)
// ---------------------------------------------------------------------
static Dealer_Show(playerid, pl)
{
	gDealerPlace[playerid] = pl;
	new list[256];
	format(list, sizeof list, "Семена конопли\t$%d\nКокаин, 1 г (только для банд)\t$%d\n{9ACD32}Продать всю марихуану (%d г)\t$%d за 1 г",
		DEALER_SEED_PRICE, DEALER_COKE_PRICE, Inv_Count(playerid, INV_WEED), DEALER_WEED_BUY);
	ShowPlayerDialog(playerid, DIALOG_DRUG_DEALER, DIALOG_STYLE_TABLIST, "Мутный тип", "%s", "Выбрать", "Уйти", list);
}

static bool:Dealer_TryAlt(playerid)
{
	new pl = Place_Nearby(playerid, PL_DEALER, 2.5);
	if (pl == -1) return false;
	if (Faction_IsLaw(Player[playerid][pFaction]) && Player[playerid][pDuty])
	{
		SendClientMessage(playerid, COLOR_GREY, "Торговец смотрит на твою форму и молча отворачивается.");
		return true;
	}
	Dealer_Show(playerid, pl);
	return true;
}

static Dealer_OnPick(playerid, listitem)
{
	new pl = gDealerPlace[playerid];
	if (pl < 0 || Place_Nearby(playerid, PL_DEALER, 3.0) != pl) return;
	switch (listitem)
	{
		case 0:
		{
			if (GetMoney(playerid) < DEALER_SEED_PRICE) { SendClientMessage(playerid, COLOR_GREY, "Не хватает денег."); return; }
			if (!Inv_Add(playerid, INV_SEEDS, 1)) { SendClientMessage(playerid, COLOR_GREY, "Больше семян в сумку не влезет."); return; }
			GiveMoney(playerid, -DEALER_SEED_PRICE, "торговец: семена");
			SendClientMessage(playerid, COLOR_GREEN, "Куплены семена (%d шт.). Посадить: сумка > Семена.", Inv_Count(playerid, INV_SEEDS));
		}
		case 1:
		{
			if (!Faction_IsGang(Player[playerid][pFaction])) { SendClientMessage(playerid, COLOR_GREY, "«Я тебя не знаю. Кокаин — только для своих». (нужно быть в банде)"); return; }
			if (GetMoney(playerid) < DEALER_COKE_PRICE) { SendClientMessage(playerid, COLOR_GREY, "Не хватает денег."); return; }
			if (!Inv_Add(playerid, INV_COCAINE, 1)) { SendClientMessage(playerid, COLOR_GREY, "Больше в сумку не влезет."); return; }
			GiveMoney(playerid, -DEALER_COKE_PRICE, "торговец: кокаин");
			SendClientMessage(playerid, COLOR_GREEN, "Куплен 1 г кокаина (в сумке %d г).", Inv_Count(playerid, INV_COCAINE));
		}
		case 2:
		{
			new n = Inv_Count(playerid, INV_WEED);
			if (n <= 0) { SendClientMessage(playerid, COLOR_GREY, "У тебя нет марихуаны."); return; }
			if (!Inv_Remove(playerid, INV_WEED, n)) return;
			GiveMoney(playerid, n * DEALER_WEED_BUY, "торговец: марихуана");
			SendClientMessage(playerid, COLOR_GREEN, "Торговец забрал %d г марихуаны: +$%d.", n, n * DEALER_WEED_BUY);
		}
	}
	Account_Save(playerid);
	Dealer_Show(playerid, pl);
}

// ---------------------------------------------------------------------
//  Продажа игроку: /selldrugs [id] [weed|coke] [грамм] [цена]
// ---------------------------------------------------------------------
stock bool:Cmd_SellDrugs(playerid, const params[])
{
	new pos = 0, what[8], grams, price;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid || !NextParam(params, pos, what) || !NextIntParam(params, pos, grams) || !NextIntParam(params, pos, price))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /selldrugs [id] [weed|coke] [грамм] [цена за всё]");
	new item;
	if (!strcmp(what, "weed", true)) item = INV_WEED;
	else if (!strcmp(what, "coke", true)) item = INV_COCAINE;
	else return SendClientMessage(playerid, COLOR_GREY, "Что продаёшь: weed (марихуана) или coke (кокаин).");
	if (grams < 1 || price < 0 || price > 100000) return SendClientMessage(playerid, COLOR_GREY, "Неверное количество или цена.");
	if (Inv_Count(playerid, item) < grams) return SendClientMessage(playerid, COLOR_GREY, "У тебя столько нет.");
	if (!IsPlayerNearPlayer(playerid, target, 3.0)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе.");
	if (Player[target][pOfferType] != OFFER_NONE) return SendClientMessage(playerid, COLOR_GREY, "Игроку уже что-то предложили.");
	Player[target][pOfferType] = OFFER_DRUGS;
	Player[target][pOfferFrom] = playerid;
	Player[target][pOfferFromDb] = Player[playerid][pDbId];
	Player[target][pOfferValue] = price;
	gSaleItem[target] = item;
	gSaleAmount[target] = grams;
	SendClientMessage(target, COLOR_YELLOW, "%s предлагает тебе %s, %d г за $%d. Принять: Y или /accept, отказаться: N.",
		GetRPName(playerid), Inv_GetName(item), grams, price);
	SendClientMessage(playerid, COLOR_WHITE, "Предложение отправлено %s.", GetRPName(target));
	return true;
}

// Из Cmd_Accept (factions.inc)
stock Drugs_AcceptSale(playerid, from, price)
{
	new item = gSaleItem[playerid], grams = gSaleAmount[playerid];
	gSaleAmount[playerid] = 0;
	if (grams <= 0 || !IsPlayerNearPlayer(playerid, from, 4.0)) return SendClientMessage(playerid, COLOR_GREY, "Продавец ушёл.");
	if (GetMoney(playerid) < price) return SendClientMessage(playerid, COLOR_GREY, "Не хватает денег.");
	if (Inv_Count(from, item) < grams) return SendClientMessage(playerid, COLOR_GREY, "У продавца уже нет столько.");
	if (Inv_GetMax(item) - Inv_Count(playerid, item) < grams) return SendClientMessage(playerid, COLOR_GREY, "Тебе это некуда положить: сумка полная.");
	Inv_Remove(from, item, grams);
	Inv_Add(playerid, item, grams);
	if (price > 0)
	{
		GiveMoney(playerid, -price, "покупка с рук");
		GiveMoney(from, price, "продажа с рук");
	}
	Account_Save(playerid);
	Account_Save(from);
	new msg[128];
	format(msg, sizeof msg, "%s незаметно передаёт свёрток %s.", GetRPName(from), GetRPName(playerid));
	SendNearbyMessage(from, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(from, COLOR_GREEN, "Сделка: %s купил %d г за $%d.", GetRPName(playerid), grams, price);
	return SendClientMessage(playerid, COLOR_GREEN, "Сделка: %d г (%s) за $%d.", grams, Inv_GetName(item), price);
}

// ---------------------------------------------------------------------
//  Полиция
// ---------------------------------------------------------------------
// Строка для /frisk (factions.inc)
stock Drugs_FriskInfo(playerid, target)
{
	new w = Inv_Count(target, INV_WEED), c = Inv_Count(target, INV_COCAINE), s = Inv_Count(target, INV_SEEDS);
	if (w + c + s == 0) return SendClientMessage(playerid, COLOR_WHITE, " Наркотиков нет.");
	return SendClientMessage(playerid, COLOR_ORANGE, " Наркотики: марихуана %d г, кокаин %d г, семена %d шт. Изъять: /seize %d", w, c, s, target);
}

stock bool:Cmd_Seize(playerid, const params[])
{
	if (!Prison_IsGuardOnDuty(playerid) && (!Faction_IsLaw(Player[playerid][pFaction]) || !Player[playerid][pDuty])) return SendClientMessage(playerid, COLOR_GREY, "Изымать может только полиция, ФБР или охрана тюрьмы на смене.");
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /seize [id]");
	if (!IsPlayerNearPlayer(playerid, target, 2.5)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе.");
	new w = Inv_Count(target, INV_WEED), c = Inv_Count(target, INV_COCAINE), s = Inv_Count(target, INV_SEEDS);
	if (w + c + s == 0) return SendClientMessage(playerid, COLOR_GREY, "У него нет наркотиков.");
	if (w) Inv_Remove(target, INV_WEED, w);
	if (c) Inv_Remove(target, INV_COCAINE, c);
	if (s) Inv_Remove(target, INV_SEEDS, s);
	new msg[144];
	format(msg, sizeof msg, "%s изымает у %s наркотики и упаковывает в пакет для улик.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(playerid, COLOR_GREEN, "Изъято: марихуана %d г, кокаин %d г, семена %d. Розыск выдаётся через /su.", w, c, s);
	SendClientMessage(target, COLOR_RED, "У тебя изъяли наркотики.");
	return true;
}

// ---------------------------------------------------------------------
//  Подключение к общим событиям
// ---------------------------------------------------------------------
stock bool:Drugs_TryAlt(playerid)
{
	if (Plant_TryAlt(playerid)) return true;
	if (Dealer_TryAlt(playerid)) return true;
	return false;
}

stock Drugs_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid != DIALOG_DRUG_DEALER) return 0;
	if (response) Dealer_OnPick(playerid, listitem);
	return 1;
}

stock Drugs_OnDeath(playerid)
{
	Drugs_StopFx(playerid);
}

stock bool:Drugs_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/selldrugs", true)) { Cmd_SellDrugs(playerid, params); return true; }
	if (!strcmp(cmd, "/seize", true))     { Cmd_Seize(playerid, params); return true; }
	return false;
}
