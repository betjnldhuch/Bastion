// =====================================================================
//  property.inc — сколько домов и транспорта может быть у игрока,
//  и список своих домов.
//
//  * Лимит = базовый + бонус за уровень VIP. Уровень VIP берётся из одной
//    функции Props_VipLevel: когда в donate.inc появятся уровни, поменять
//    надо только её.
//  * /myhouses — мои дома: метка GPS, выбор главного дома (у него спавн).
//    Команды дома (/house, /sellhouse, /housepay, /lock...) работают с тем
//    домом, в котором или у двери которого стоит игрок (houses.inc).
//  * /cars и /dealerspawn — dealers.inc.
// =====================================================================

#define PROP_HOUSES_BASE        2       // домов без VIP
#define PROP_HOUSES_PER_VIP     1       // + столько за каждый уровень VIP
#define PROP_HOUSES_MAX         6       // но не больше

#define PROP_VEHICLES_BASE      3       // транспорта (машины, мотоциклы, вертолёты) без VIP
#define PROP_VEHICLES_PER_VIP   1
#define PROP_VEHICLES_MAX       10      // не больше CARS_MENU_MAX (dealers.inc)

forward bool:Donate_IsVip(playerid);    // donate.inc подключён позже

static gMyHouses[MAX_PLAYERS][PROP_HOUSES_MAX];     // дома по строкам /myhouses
static gMyHousesCount[MAX_PLAYERS];
static gMyHousePick[MAX_PLAYERS] = { -1, ... };

// Уровень VIP игрока (0 — нет). ЗАГЛУШКА: сейчас VIP один, без уровней.
// Появятся уровни — вернуть здесь уровень из donate.inc
stock Props_VipLevel(playerid)
{
	return Donate_IsVip(playerid) ? 1 : 0;
}

stock Props_MaxHouses(playerid)
{
	new lim = PROP_HOUSES_BASE + PROP_HOUSES_PER_VIP * Props_VipLevel(playerid);
	return (lim > PROP_HOUSES_MAX) ? PROP_HOUSES_MAX : lim;
}

stock Props_MaxVehicles(playerid)
{
	new lim = PROP_VEHICLES_BASE + PROP_VEHICLES_PER_VIP * Props_VipLevel(playerid);
	return (lim > PROP_VEHICLES_MAX) ? PROP_VEHICLES_MAX : lim;
}

// ---------------------------------------------------------------------
//  /myhouses
// ---------------------------------------------------------------------
stock bool:Cmd_MyHouses(playerid)
{
	new list[700] = "Дом\tСчёт дома\tНалог в час\n";
	new line[96], n = 0, mainHouse = House_GetMain(playerid);
	for (new h = 0; h < gHouseCount && n < PROP_HOUSES_MAX; h++)
	{
		if (House[h][hOwner] != Player[playerid][pDbId] || Player[playerid][pDbId] == 0) continue;
		format(line, sizeof line, "№%d%s\t%s$%d\t$%d\n", House[h][hDbId], (h == mainHouse) ? " {FFFF00}(главный)" : "",
			(House[h][hBalance] < 0) ? "{FF6347}" : "{33AA33}", House[h][hBalance], House_GetTax(h));
		strcat(list, line);
		gMyHouses[playerid][n++] = h;
	}
	gMyHousesCount[playerid] = n;
	if (n == 0) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет домов. Купить: /buyhouse у зелёного домика. Твой лимит: %d.", Props_MaxHouses(playerid));

	new title[40];
	format(title, sizeof title, "Мои дома (%d из %d)", n, Props_MaxHouses(playerid));
	ShowPlayerDialog(playerid, DIALOG_MYHOUSES, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Выбрать", "Закрыть", list);
	return true;
}

stock Props_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid == DIALOG_MYHOUSES)
	{
		if (!response || listitem < 0 || listitem >= gMyHousesCount[playerid]) return 1;
		new h = gMyHouses[playerid][listitem];
		if (House[h][hOwner] != Player[playerid][pDbId]) return 1;
		gMyHousePick[playerid] = h;
		new title[24];
		format(title, sizeof title, "Дом №%d", House[h][hDbId]);
		ShowPlayerDialog(playerid, DIALOG_MYHOUSE_ACT, DIALOG_STYLE_LIST, title,
			"Показать на карте (GPS)\nСделать главным: появляться у этого дома", "Выбрать", "Назад");
		return 1;
	}
	if (dialogid == DIALOG_MYHOUSE_ACT)
	{
		new h = gMyHousePick[playerid];
		gMyHousePick[playerid] = -1;
		if (!response) { Cmd_MyHouses(playerid); return 1; }
		if (h < 0 || h >= gHouseCount || House[h][hOwner] != Player[playerid][pDbId]) return 1;
		if (listitem == 0) Gps_SetPoint(playerid, House[h][hX], House[h][hY], House[h][hZ], "Мой дом");
		else if (listitem == 1)
		{
			House_SetMain(playerid, h);
			Account_SetNormalSpawn(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Дом №%d теперь главный: ты будешь появляться у него.", House[h][hDbId]);
		}
		return 1;
	}
	return 0;
}

stock bool:Props_OnCommand(playerid, const cmd[], const params[])
{
	#pragma unused params
	if (!strcmp(cmd, "/cars", true) || !strcmp(cmd, "/mycars", true))       { Cmd_Cars(playerid); return true; }
	if (!strcmp(cmd, "/myhouses", true) || !strcmp(cmd, "/houses", true))   { Cmd_MyHouses(playerid); return true; }
	if (!strcmp(cmd, "/dealerspawn", true))                                 { Cmd_DealerSpawn(playerid); return true; }
	return false;
}
