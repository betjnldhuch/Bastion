// =====================================================================
//  needs.inc — голод и инвентарь.
//
//  Сытость: от 0 до 100%. Каждые HUNGER_MINUTES минут игры падает на 1%
//  (от полной до нуля примерно за 10 часов игры). Ниже HUNGER_WARN% —
//  напоминание поесть, на нуле каждую минуту отнимается HUNGER_DAMAGE HP
//  (но не ниже 20 HP — от голода не умирают, просто становится плохо).
//  После больницы сытость не ниже 50%.
//  Поесть: бар (сразу) или еда из инвентаря. Хранится в accounts.hunger.
//
//  Инвентарь (сумка): вещи из магазинов 24/7 — еда, вода, пиво, сигареты,
//  аптечки, бинты, ремкомплекты, цветы, кости, маска.
//  /inv (или /i) — открыть: выбрать вещь, потом использовать / передать / выбросить.
//  Выброшенная вещь лежит на земле DROP_MINUTES минут, поднять — ALT или /pickup.
//  Маска прячет ник над головой (снимается в сумке или при смерти).
//  Хранится в таблице inventory (owner, item, amount), у каждой вещи свой
//  максимум (gInvMax).
//  Одежда: купленная в магазине одежды ложится в сумку (до MAX_OUTFITS штук),
//  надеть — из /inv, снятая одежда возвращается в сумку. Таблица outfits.
//  Админы (4+): /givefood [id] — накормить, /giveitem [id] [вещь] [кол-во].
// =====================================================================

#define HUNGER_MINUTES      6
#define HUNGER_WARN         20
#define HUNGER_DAMAGE       2
#define INV_MAX_STACK       5       // для старого кода: максимум по умолчанию
#define MAX_DROPS           100     // вещей на земле одновременно
#define DROP_MINUTES        10
#define DROP_MODEL          1575    // маленький свёрток


enum
{
	INV_NONE,
	INV_BURGER,
	INV_HOTDOG,
	INV_MEDKIT,
	INV_REPAIRKIT,
	INV_PIZZA,
	INV_WATER,
	INV_BEER,
	INV_CIGARETTES,
	INV_BANDAGE,
	INV_FLOWERS,
	INV_DICE,
	INV_MASK,
	INV_MATS,           // материалы: из них делают оружие (factions.inc)
	INV_SEEDS,          // семена конопли: посадить (drugs.inc)
	INV_WEED,           // марихуана, граммы (drugs.inc)
	INV_COCAINE,        // кокаин, граммы (drugs.inc)
	INV_FISH,           // рыба: сдать на работе рыбака (worklife.inc)
	INV_TYPES
}
// Номера вещей (для /giveitem): 1 бургер, 2 хот-дог, 3 аптечка, 4 ремкомплект, 5 пицца, 6 вода,
// 7 пиво, 8 сигареты, 9 бинт, 10 цветы, 11 кости, 12 маска, 13 материалы,
// 14 семена конопли, 15 марихуана, 16 кокаин, 17 рыба
static const gInvNames[INV_TYPES][] = { "", "Бургер", "Хот-дог", "Аптечка", "Ремкомплект", "Пицца", "Вода", "Пиво", "Сигареты", "Бинт", "Цветы", "Игральные кости", "Маска", "Материалы", "Семена конопли", "Марихуана", "Кокаин", "Рыба" };
static const gInvHints[INV_TYPES][] = { "", "+40% сытости", "+25% сытости", "+50 HP", "чинит машину рядом", "+60% сытости", "+10% сытости, +5 HP", "+5 HP, пьянит", "покурить", "+20 HP", "букет в руки", "бросить кости (RP)", "прячет ник", "сделать оружие", "посадить куст", "покурить (эффект, +HP)", "употребить (эффект, бронь)", "сдать рыбаку" };
static const gInvMax[INV_TYPES] = { 0, 5, 5, 3, 3, 3, 5, 5, 20, 5, 3, 1, 1, 500, 10, 50, 20, 30 };

enum E_DROP
{
	drItem,
	drAmount,
	Float:drX, Float:drY, Float:drZ,
	drMinutes,
	drObject,
	Text3D:drLabel
}
static gDrop[MAX_DROPS][E_DROP];
static gInvSelected[MAX_PLAYERS];
static bool:gMasked[MAX_PLAYERS];

static gHunger[MAX_PLAYERS];
static gHungerTick[MAX_PLAYERS];
static gInv[MAX_PLAYERS][INV_TYPES];
#define MAX_OUTFITS         5       // одежды в сумке (не считая надетой)
#define INV_OUTFIT          100     // в меню и на земле: одежда (100 + слот / скин в drAmount)
static gInvMenu[MAX_PLAYERS][INV_TYPES + MAX_OUTFITS + 1];
static gOutfit[MAX_PLAYERS][MAX_OUTFITS];     // скины в сумке, -1 = пусто

// ---------------------------------------------------------------------
//  Загрузка и сохранение
// ---------------------------------------------------------------------
stock Needs_Init()
{
	DB_AddColumn("accounts", "hunger", "INTEGER DEFAULT 100");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS inventory (owner INTEGER NOT NULL, item INTEGER NOT NULL, amount INTEGER DEFAULT 0, PRIMARY KEY (owner, item))"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS outfits (id INTEGER PRIMARY KEY AUTOINCREMENT, owner INTEGER NOT NULL, skin INTEGER NOT NULL)"));
}

// После входа в аккаунт (из Vehicles_LoadForPlayer)
stock Needs_LoadPlayer(playerid)
{
	gHunger[playerid] = 100;
	gHungerTick[playerid] = 0;
	for (new i = 0; i < INV_TYPES; i++) gInv[playerid][i] = 0;
	for (new i = 0; i < MAX_OUTFITS; i++) gOutfit[playerid][i] = -1;
	new DBResult:ro = DB_ExecuteQuery(gDB, "SELECT skin FROM outfits WHERE owner = %d ORDER BY id LIMIT %d", Player[playerid][pDbId], MAX_OUTFITS);
	if (ro != DBResult:0)
	{
		new rows = DB_GetRowCount(ro);
		for (new i = 0; i < rows; i++)
		{
			gOutfit[playerid][i] = DB_GetFieldInt(ro, 0);
			DB_SelectNextRow(ro);
		}
		DB_FreeResultSet(ro);
	}

	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT hunger FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) gHunger[playerid] = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (gHunger[playerid] < 0 || gHunger[playerid] > 100) gHunger[playerid] = 100;

	r = DB_ExecuteQuery(gDB, "SELECT item, amount FROM inventory WHERE owner = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new item = DB_GetFieldInt(r, 0);
			if (item > INV_NONE && item < INV_TYPES) gInv[playerid][item] = DB_GetFieldInt(r, 1);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
}

static Needs_SaveHunger(playerid)
{
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET hunger = %d WHERE id = %d", gHunger[playerid], Player[playerid][pDbId]));
}

static Inv_Save(playerid, item)
{
	if (Player[playerid][pDbId] == 0) return;
	if (gInv[playerid][item] <= 0)
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM inventory WHERE owner = %d AND item = %d", Player[playerid][pDbId], item));
	else
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO inventory (owner, item, amount) VALUES (%d, %d, %d)",
			Player[playerid][pDbId], item, gInv[playerid][item]));
}

// ---------------------------------------------------------------------
//  Сытость
// ---------------------------------------------------------------------
stock Needs_GetHunger(playerid) { return gHunger[playerid]; }

stock Needs_Feed(playerid, amount)
{
	gHunger[playerid] += amount;
	if (gHunger[playerid] > 100) gHunger[playerid] = 100;
	if (gHunger[playerid] < 0) gHunger[playerid] = 0;
	Needs_SaveHunger(playerid);
}

// Из OnPlayerSpawn после смерти: в больнице покормили
stock Needs_OnHospital(playerid)
{
	if (gHunger[playerid] < 50) Needs_Feed(playerid, 50 - gHunger[playerid]);
}

// Раз в минуту (из OnMinuteTimer)
stock Needs_OnMinute()
{
	Inv_DropsTick();
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || GetPlayerState(i) == PLAYER_STATE_SPECTATING) continue;

		if (++gHungerTick[i] >= HUNGER_MINUTES)
		{
			gHungerTick[i] = 0;
			if (gHunger[i] > 0)
			{
				gHunger[i]--;
				Needs_SaveHunger(i);
				if (gHunger[i] == HUNGER_WARN || gHunger[i] == HUNGER_WARN / 2)
					SendClientMessage(i, COLOR_ORANGE, "Ты проголодался (сытость %d%%). Поешь в баре или достань еду: /inv", gHunger[i]);
			}
		}

		if (gHunger[i] == 0)
		{
			new Float:hp;
			GetPlayerHealth(i, hp);
			if (hp > 20.0)
			{
				hp -= float(HUNGER_DAMAGE);
				if (hp < 20.0) hp = 20.0;
				SetPlayerHealth(i, hp);
				GameTextForPlayer(i, "~r~Hungry", 2000, 4);
			}
		}
	}
}

stock bool:Cmd_Hunger(playerid)
{
	SendClientMessage(playerid, COLOR_WHITE, "Сытость: %d%%. Поесть: бар, или еда из инвентаря (/inv).", gHunger[playerid]);
	return true;
}

// ---------------------------------------------------------------------
//  Инвентарь
// ---------------------------------------------------------------------
stock Inv_GetName(item)
{
	new name[24];
	if (item > INV_NONE && item < INV_TYPES) strcat(name, gInvNames[item]);
	return name;
}

stock Inv_Count(playerid, item)
{
	if (item <= INV_NONE || item >= INV_TYPES) return 0;
	return gInv[playerid][item];
}

// Сколько штук этой вещи помещается в сумку
stock Inv_GetMax(item)
{
	if (item <= INV_NONE || item >= INV_TYPES) return 0;
	return gInvMax[item];
}

// Добавить вещь. Возвращает false, если места нет.
stock bool:Inv_Add(playerid, item, amount = 1)
{
	if (item <= INV_NONE || item >= INV_TYPES) return false;
	if (gInv[playerid][item] + amount > gInvMax[item]) return false;
	gInv[playerid][item] += amount;
	Inv_Save(playerid, item);
	return true;
}

static Inv_Take(playerid, item, amount)
{
	gInv[playerid][item] -= amount;
	if (gInv[playerid][item] < 0) gInv[playerid][item] = 0;
	Inv_Save(playerid, item);
}

// Забрать несколько штук (для материалов). false — столько нет
stock bool:Inv_Remove(playerid, item, amount)
{
	if (item <= INV_NONE || item >= INV_TYPES || amount < 1 || gInv[playerid][item] < amount) return false;
	Inv_Take(playerid, item, amount);
	return true;
}

// Забрать одну вещь из сумки. false — такой вещи нет
stock bool:Inv_TakeOne(playerid, item)
{
	if (item <= INV_NONE || item >= INV_TYPES || gInv[playerid][item] <= 0) return false;
	Inv_Take(playerid, item, 1);
	return true;
}

// ---------------------------------------------------------------------
//  Одежда в сумке
// ---------------------------------------------------------------------
stock Outfit_FreeSlots(playerid)
{
	new n = 0;
	for (new i = 0; i < MAX_OUTFITS; i++) if (gOutfit[playerid][i] == -1) n++;
	return n;
}

// Положить одежду в сумку. false — сумка полная
stock bool:Outfit_Add(playerid, skin)
{
	for (new i = 0; i < MAX_OUTFITS; i++)
	{
		if (gOutfit[playerid][i] != -1) continue;
		gOutfit[playerid][i] = skin;
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO outfits (owner, skin) VALUES (%d, %d)", Player[playerid][pDbId], skin));
		return true;
	}
	return false;
}

static Outfit_Remove(playerid, slot)
{
	new skin = gOutfit[playerid][slot];
	if (skin == -1) return;
	gOutfit[playerid][slot] = -1;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM outfits WHERE id = (SELECT id FROM outfits WHERE owner = %d AND skin = %d LIMIT 1)", Player[playerid][pDbId], skin));
}

// Надеть одежду из сумки, снятая ложится на её место
static Outfit_Wear(playerid, slot)
{
	new skin = gOutfit[playerid][slot];
	if (skin == -1) return 0;
	if (Player[playerid][pDuty]) { SendClientMessage(playerid, COLOR_GREY, "Сначала сними рабочую форму (/duty)."); return 0; }
	if (IsPlayerInAnyVehicle(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Переодеться можно только пешком."); return 0; }

	new old = Player[playerid][pSkin];
	Outfit_Remove(playerid, slot);
	gOutfit[playerid][slot] = old;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO outfits (owner, skin) VALUES (%d, %d)", Player[playerid][pDbId], old));

	Player[playerid][pSkin] = skin;
	SetPlayerSkin(playerid, skin);
	Account_Save(playerid);
	Account_SetNormalSpawn(playerid);
	new msg[96];
	format(msg, sizeof msg, "%s переодевается.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return 1;
}

// Сумка списком (на телефоне, /invlist)
stock bool:Inv_ShowList(playerid)
{
	new list[1200] = "Вещь\tКол-во\tЧто даёт\n", line[96], n = 0;
	for (new item = 1; item < INV_TYPES; item++)
	{
		if (gInv[playerid][item] <= 0) continue;
		format(line, sizeof line, "%s\t%d/%d\t%s\n", gInvNames[item], gInv[playerid][item], gInvMax[item], gInvHints[item]);
		strcat(list, line);
		gInvMenu[playerid][n++] = item;
	}
	new canister = Fuel_GetCanister(playerid);
	if (canister > 0)
	{
		format(line, sizeof line, "Канистра бензина\t%d\t/canister у машины\n", canister);
		strcat(list, line);
		gInvMenu[playerid][n++] = INV_NONE;
	}
	for (new i = 0; i < MAX_OUTFITS; i++)
	{
		if (gOutfit[playerid][i] == -1) continue;
		format(line, sizeof line, "Одежда: скин %d\t1\tнадеть\n", gOutfit[playerid][i]);
		strcat(list, line);
		gInvMenu[playerid][n++] = INV_OUTFIT + i;
	}
	if (n == 0) return SendClientMessage(playerid, COLOR_GREY, "Сумка пуста. Еду, воду, аптечки и другие вещи продают в магазинах 24/7, одежду — в магазинах одежды.");
	for (new i = n; i < sizeof (gInvMenu[]); i++) gInvMenu[playerid][i] = -1;

	new title[48];
	format(title, sizeof title, "Сумка (сытость %d%%)", gHunger[playerid]);
	ShowPlayerDialog(playerid, DIALOG_INVENTORY, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Выбрать", "Закрыть", list);
	return true;
}

// Сумка: окно с моделями вещей (списком — /invlist)
stock bool:Cmd_Inv(playerid)
{
	if (Inv_ShowUI(playerid)) return true;
	return Inv_ShowList(playerid);
}

// Маска: ник над головой не виден
stock bool:Inv_IsMasked(playerid) { return gMasked[playerid]; }

static Inv_SetMask(playerid, bool:on)
{
	gMasked[playerid] = on;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (i != playerid && IsPlayerConnected(i)) ShowPlayerNameTagForPlayer(i, playerid, !on);
	}
	Faction_UpdateColor(playerid);      // точка на карте пропадает и возвращается вместе с ником (factions.inc)
}

// Из OnPlayerStreamIn: новый игрок рядом не должен видеть ник в маске
stock Inv_OnStreamIn(playerid, forplayerid)
{
	if (gMasked[playerid]) ShowPlayerNameTagForPlayer(forplayerid, playerid, false);
}

// Из OnPlayerDeath и при выходе: маска слетает
stock Inv_ResetMask(playerid)
{
	if (gMasked[playerid]) Inv_SetMask(playerid, false);
}

static Inv_Heal(playerid, Float:amount)
{
	new Float:hp;
	GetPlayerHealth(playerid, hp);
	hp += amount;
	if (hp > 100.0) hp = 100.0;
	SetPlayerHealth(playerid, hp);
}

static Inv_Use(playerid, item)
{
	if (gInv[playerid][item] <= 0) return;
	if (item >= INV_SEEDS) { Drugs_UseItem(playerid, item); return; }    // семена, наркотики, рыба (drugs.inc)
	new msg[128];
	new bool:consume = true;
	switch (item)
	{
		case INV_BURGER, INV_HOTDOG, INV_PIZZA:
		{
			if (gHunger[playerid] >= 100) { SendClientMessage(playerid, COLOR_GREY, "Ты не голоден."); return; }
			new feed = 25;
			if (item == INV_BURGER) feed = 40;
			else if (item == INV_PIZZA) feed = 60;
			Needs_Feed(playerid, feed);
			ApplyAnimation(playerid, "FOOD", "EAT_Burger", 4.1, false, false, false, false, 0);
			format(msg, sizeof msg, "%s ест: %s.", GetRPName(playerid), gInvNames[item]);
		}
		case INV_WATER:
		{
			Needs_Feed(playerid, 10);
			Inv_Heal(playerid, 5.0);
			ApplyAnimation(playerid, "VENDING", "VEND_Drink2_P", 4.1, false, false, false, false, 0);
			format(msg, sizeof msg, "%s пьёт воду.", GetRPName(playerid));
		}
		case INV_BEER:
		{
			Inv_Heal(playerid, 5.0);
			SetPlayerDrunkLevel(playerid, GetPlayerDrunkLevel(playerid) + 2000);
			ApplyAnimation(playerid, "VENDING", "VEND_Drink2_P", 4.1, false, false, false, false, 0);
			format(msg, sizeof msg, "%s открывает бутылку пива и делает глоток.", GetRPName(playerid));
		}
		case INV_CIGARETTES:
		{
			if (IsPlayerInAnyVehicle(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Выйди из машины."); return; }
			SetPlayerSpecialAction(playerid, SPECIAL_ACTION_SMOKE_CIGGY);
			format(msg, sizeof msg, "%s достаёт сигарету и закуривает.", GetRPName(playerid));
		}
		case INV_MEDKIT, INV_BANDAGE:
		{
			new Float:hp;
			GetPlayerHealth(playerid, hp);
			if (hp >= 100.0) { SendClientMessage(playerid, COLOR_GREY, "Ты и так здоров."); return; }
			if (item == INV_MEDKIT)
			{
				Inv_Heal(playerid, 50.0);
				format(msg, sizeof msg, "%s достаёт аптечку и обрабатывает раны.", GetRPName(playerid));
			}
			else
			{
				Inv_Heal(playerid, 20.0);
				format(msg, sizeof msg, "%s перевязывает рану бинтом.", GetRPName(playerid));
			}
		}
		case INV_REPAIRKIT:
		{
			if (IsPlayerInAnyVehicle(playerid)) { SendClientMessage(playerid, COLOR_GREY, "Выйди из машины и встань рядом с ней."); return; }
			new Float:px, Float:py, Float:pz, v = INVALID_VEHICLE_ID, Float:best = 5.0;
			GetPlayerPos(playerid, px, py, pz);
			for (new i = 1; i < MAX_VEHICLES; i++)
			{
				if (!IsValidVehicle(i)) continue;
				new Float:d = GetVehicleDistanceFromPoint(i, px, py, pz);
				if (d < best) { best = d; v = i; }
			}
			if (v == INVALID_VEHICLE_ID) { SendClientMessage(playerid, COLOR_GREY, "Рядом нет машины."); return; }
			RepairVehicle(v);
			ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, false, false, false, false, 0);
			format(msg, sizeof msg, "%s чинит машину ремкомплектом.", GetRPName(playerid));
		}
		case INV_FLOWERS:
		{
			GivePlayerWeapon(playerid, WEAPON_FLOWER, 1);
			format(msg, sizeof msg, "%s достаёт букет цветов.", GetRPName(playerid));
		}
		case INV_DICE:
		{
			consume = false;
			format(msg, sizeof msg, "%s бросает кости: выпало %d и %d.", GetRPName(playerid), random(6) + 1, random(6) + 1);
		}
		case INV_MASK:
		{
			consume = false;
			Inv_SetMask(playerid, !gMasked[playerid]);
			if (gMasked[playerid]) format(msg, sizeof msg, "%s надевает маску.", GetRPName(playerid));
			else format(msg, sizeof msg, "%s снимает маску.", GetRPName(playerid));
		}
		case INV_MATS:
		{
			Craft_Show(playerid);   // выбор, что и сколько сделать (factions.inc)
			return;
		}
		default: return;
	}
	if (consume) Inv_Take(playerid, item, 1);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
}

// ---------------------------------------------------------------------
//  Передать и выбросить
// ---------------------------------------------------------------------
static Inv_Give(playerid, item, const input[])
{
	new pos = 0, amount = 1;
	new target = NextPlayerParam(input, pos);
	NextIntParam(input, pos, amount);
	if (target == INVALID_PLAYER_ID || target == playerid || !Player[target][pLogged])
	{
		SendClientMessage(playerid, COLOR_GREY, "Нужно ввести id игрока, например: 5 или 5 2");
		return 0;
	}
	if (amount < 1 || amount > gInv[playerid][item])
	{
		SendClientMessage(playerid, COLOR_GREY, "У тебя только %d шт.", gInv[playerid][item]);
		return 0;
	}
	if (!IsPlayerNearPlayer(playerid, target, 3.0))
	{
		SendClientMessage(playerid, COLOR_GREY, "Подойди ближе к игроку.");
		return 0;
	}
	if (item == INV_MASK && gMasked[playerid] && amount >= gInv[playerid][item]) Inv_SetMask(playerid, false);
	if (!Inv_Add(target, item, amount))
	{
		SendClientMessage(playerid, COLOR_GREY, "У %s нет места в сумке.", GetRPName(target));
		return 0;
	}
	Inv_Take(playerid, item, amount);
	new msg[128];
	format(msg, sizeof msg, "%s передаёт %s: %s x%d.", GetRPName(playerid), GetRPName(target), gInvNames[item], amount);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return 1;
}

static Inv_Drop(playerid, item, amount)
{
	if (amount < 1 || amount > gInv[playerid][item])
	{
		SendClientMessage(playerid, COLOR_GREY, "У тебя только %d шт.", gInv[playerid][item]);
		return 0;
	}
	if (GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0 || IsPlayerInAnyVehicle(playerid))
	{
		SendClientMessage(playerid, COLOR_GREY, "Выбросить вещь можно только на улице и пешком.");
		return 0;
	}
	new d = -1;
	for (new i = 0; i < MAX_DROPS; i++)
	{
		if (gDrop[i][drItem] == INV_NONE) { d = i; break; }
	}
	if (d == -1)
	{
		SendClientMessage(playerid, COLOR_GREY, "На земле и так слишком много вещей. Попробуй позже.");
		return 0;
	}
	if (item == INV_MASK && gMasked[playerid] && amount >= gInv[playerid][item]) Inv_SetMask(playerid, false);

	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	gDrop[d][drItem] = item;
	gDrop[d][drAmount] = amount;
	gDrop[d][drX] = x;
	gDrop[d][drY] = y;
	gDrop[d][drZ] = z;
	gDrop[d][drMinutes] = DROP_MINUTES;
	gDrop[d][drObject] = CreateObject(DROP_MODEL, x, y, z - 0.95, 0.0, 0.0, 0.0, 50.0);
	gDrop[d][drLabel] = Create3DTextLabel("{FFFFFF}%s x%d\n{AFAFAF}ALT — поднять", COLOR_WHITE, x, y, z - 0.6, 8.0, 0, true, gInvNames[item], amount);

	Inv_Take(playerid, item, amount);
	ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, false, false, false, false, 0);
	new msg[128];
	format(msg, sizeof msg, "%s кладёт на землю: %s x%d.", GetRPName(playerid), gInvNames[item], amount);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return 1;
}

static Inv_RemoveDrop(d)
{
	if (gDrop[d][drItem] == INV_NONE) return;
	DestroyObject(gDrop[d][drObject]);
	Delete3DTextLabel(gDrop[d][drLabel]);
	gDrop[d][drItem] = INV_NONE;
}

// ALT пешком (keys.inc) или /pickup: поднять вещь с земли. 1 — что-то было рядом
stock Inv_TryPickup(playerid)
{
	if (GetPlayerVirtualWorld(playerid) != 0 || IsPlayerInAnyVehicle(playerid)) return 0;
	for (new d = 0; d < MAX_DROPS; d++)
	{
		if (gDrop[d][drItem] == INV_NONE) continue;
		if (!IsPlayerInRangeOfPoint(playerid, 1.8, gDrop[d][drX], gDrop[d][drY], gDrop[d][drZ])) continue;

		if (gDrop[d][drItem] == INV_OUTFIT)
		{
			new skin = gDrop[d][drAmount];
			if (!Outfit_Add(playerid, skin))
			{
				SendClientMessage(playerid, COLOR_GREY, "В сумке нет места для одежды (максимум %d).", MAX_OUTFITS);
				return 1;
			}
			Inv_RemoveDrop(d);
			ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, false, false, false, false, 0);
			new omsg[128];
			format(omsg, sizeof omsg, "%s поднимает с земли одежду.", GetRPName(playerid));
			SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, omsg);
			return 1;
		}

		new item = gDrop[d][drItem], amount = gDrop[d][drAmount];
		new space = gInvMax[item] - gInv[playerid][item];
		if (space <= 0)
		{
			SendClientMessage(playerid, COLOR_GREY, "В сумке нет места для: %s (максимум %d).", gInvNames[item], gInvMax[item]);
			return 1;
		}
		if (amount > space) amount = space;
		Inv_Add(playerid, item, amount);
		gDrop[d][drAmount] -= amount;
		if (gDrop[d][drAmount] <= 0) Inv_RemoveDrop(d);
		else
		{
			Update3DTextLabelText(gDrop[d][drLabel], COLOR_WHITE, "{FFFFFF}%s x%d\n{AFAFAF}ALT — поднять", gInvNames[item], gDrop[d][drAmount]);
		}
		ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, false, false, false, false, 0);
		new msg[128];
		format(msg, sizeof msg, "%s поднимает с земли: %s x%d.", GetRPName(playerid), gInvNames[item], amount);
		SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
		return 1;
	}
	return 0;
}

stock bool:Cmd_Pickup(playerid)
{
	if (!Inv_TryPickup(playerid)) return SendClientMessage(playerid, COLOR_GREY, "Рядом на земле ничего нет.");
	return true;
}

// Раз в минуту (из Needs_OnMinute): старые вещи на земле исчезают
static Inv_DropsTick()
{
	for (new d = 0; d < MAX_DROPS; d++)
	{
		if (gDrop[d][drItem] == INV_NONE) continue;
		if (--gDrop[d][drMinutes] <= 0) Inv_RemoveDrop(d);
	}
}

static Outfit_ShowActions(playerid, slot)
{
	gInvSelected[playerid] = INV_OUTFIT + slot;
	new title[48];
	format(title, sizeof title, "Одежда: скин %d", gOutfit[playerid][slot]);
	ShowPlayerDialog(playerid, DIALOG_INV_ACTION, DIALOG_STYLE_LIST, title, "Надеть\nПередать игроку\nВыбросить", "Выбрать", "Назад");
}

static Outfit_Drop(playerid, slot)
{
	if (GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0 || IsPlayerInAnyVehicle(playerid))
	{
		SendClientMessage(playerid, COLOR_GREY, "Выбросить вещь можно только на улице и пешком.");
		return 0;
	}
	new d = -1;
	for (new i = 0; i < MAX_DROPS; i++)
	{
		if (gDrop[i][drItem] == INV_NONE) { d = i; break; }
	}
	if (d == -1)
	{
		SendClientMessage(playerid, COLOR_GREY, "На земле и так слишком много вещей. Попробуй позже.");
		return 0;
	}
	new skin = gOutfit[playerid][slot];
	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	gDrop[d][drItem] = INV_OUTFIT;
	gDrop[d][drAmount] = skin;
	gDrop[d][drX] = x;
	gDrop[d][drY] = y;
	gDrop[d][drZ] = z;
	gDrop[d][drMinutes] = DROP_MINUTES;
	gDrop[d][drObject] = CreateObject(DROP_MODEL, x, y, z - 0.95, 0.0, 0.0, 0.0, 50.0);
	gDrop[d][drLabel] = Create3DTextLabel("{FFFFFF}Одежда (скин %d)\n{AFAFAF}ALT — поднять", COLOR_WHITE, x, y, z - 0.6, 8.0, 0, true, skin);
	Outfit_Remove(playerid, slot);
	new msg[96];
	format(msg, sizeof msg, "%s кладёт на землю одежду.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return 1;
}

static Outfit_Give(playerid, slot, const input[])
{
	new pos = 0;
	new target = NextPlayerParam(input, pos);
	if (target == INVALID_PLAYER_ID || target == playerid || !Player[target][pLogged])
	{
		SendClientMessage(playerid, COLOR_GREY, "Нужно ввести id игрока.");
		return 0;
	}
	if (!IsPlayerNearPlayer(playerid, target, 3.0))
	{
		SendClientMessage(playerid, COLOR_GREY, "Подойди ближе к игроку.");
		return 0;
	}
	new skin = gOutfit[playerid][slot];
	if (!Outfit_Add(target, skin))
	{
		SendClientMessage(playerid, COLOR_GREY, "У %s нет места для одежды.", GetRPName(target));
		return 0;
	}
	Outfit_Remove(playerid, slot);
	new msg[128];
	format(msg, sizeof msg, "%s передаёт %s одежду.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return 1;
}

static Outfit_OnDialog(playerid, dialogid, response, listitem, const inputtext[], slot)
{
	if (slot < 0 || slot >= MAX_OUTFITS || gOutfit[playerid][slot] == -1) return 1;
	if (!response)
	{
		if (dialogid == DIALOG_INV_ACTION) Cmd_Inv(playerid);
		else Outfit_ShowActions(playerid, slot);
		return 1;
	}
	switch (dialogid)
	{
		case DIALOG_INV_ACTION:
		{
			switch (listitem)
			{
				case 0: Outfit_Wear(playerid, slot);
				case 1: ShowPlayerDialog(playerid, DIALOG_INV_GIVE, DIALOG_STYLE_INPUT, "Передать одежду",
					"{FFFFFF}Введи id игрока, которому передать одежду (скин %d).", "Передать", "Назад", gOutfit[playerid][slot]);
				case 2: Outfit_Drop(playerid, slot);
			}
		}
		case DIALOG_INV_GIVE: Outfit_Give(playerid, slot, inputtext);
	}
	return 1;
}

static Inv_ShowActions(playerid, item)
{
	gInvSelected[playerid] = item;
	new title[48];
	format(title, sizeof title, "%s (%d шт.)", gInvNames[item], gInv[playerid][item]);
	new use[24];
	if (item == INV_MASK && gMasked[playerid]) strcat(use, "Снять маску");
	else if (item == INV_MASK) strcat(use, "Надеть маску");
	else if (item == INV_DICE) strcat(use, "Бросить кости");
	else strcat(use, "Использовать");
	ShowPlayerDialog(playerid, DIALOG_INV_ACTION, DIALOG_STYLE_LIST, title, "%s\nПередать игроку\nВыбросить", "Выбрать", "Назад", use);
}

stock Needs_OnDialog(playerid, dialogid, response, listitem, const inputtext[] = "")
{
	if (dialogid < DIALOG_INVENTORY || dialogid > DIALOG_INV_DROP) return 0;

	if (dialogid == DIALOG_INVENTORY)
	{
		if (!response || listitem < 0 || listitem >= sizeof (gInvMenu[])) return 1;
		new item = gInvMenu[playerid][listitem];
		if (item >= INV_OUTFIT)
		{
			Outfit_ShowActions(playerid, item - INV_OUTFIT);
			return 1;
		}
		if (item == INV_NONE)
		{
			SendClientMessage(playerid, COLOR_GREY, "Канистру заливают командой /canister, стоя у машины.");
			return 1;
		}
		if (item > INV_NONE && item < INV_TYPES) Inv_ShowActions(playerid, item);
		return 1;
	}

	new item = gInvSelected[playerid];
	if (item >= INV_OUTFIT) return Outfit_OnDialog(playerid, dialogid, response, listitem, inputtext, item - INV_OUTFIT);
	if (item <= INV_NONE || item >= INV_TYPES || gInv[playerid][item] <= 0) return 1;
	if (!response)
	{
		if (dialogid == DIALOG_INV_ACTION) Cmd_Inv(playerid);
		else Inv_ShowActions(playerid, item);
		return 1;
	}

	switch (dialogid)
	{
		case DIALOG_INV_ACTION:
		{
			switch (listitem)
			{
				case 0: Inv_Use(playerid, item);
				case 1: ShowPlayerDialog(playerid, DIALOG_INV_GIVE, DIALOG_STYLE_INPUT, "Передать",
					"{FFFFFF}Кому передать: %s?\nВведи id игрока и, если нужно, количество (у тебя %d шт.).\nНапример: {FFFF00}5{FFFFFF} или {FFFF00}5 2", "Передать", "Назад", gInvNames[item], gInv[playerid][item]);
				case 2:
				{
					if (gInv[playerid][item] == 1) Inv_Drop(playerid, item, 1);
					else ShowPlayerDialog(playerid, DIALOG_INV_DROP, DIALOG_STYLE_INPUT, "Выбросить",
						"{FFFFFF}Сколько выбросить: %s? (у тебя %d шт.)", "Выбросить", "Назад", gInvNames[item], gInv[playerid][item]);
				}
			}
		}
		case DIALOG_INV_GIVE: Inv_Give(playerid, item, inputtext);
		case DIALOG_INV_DROP: Inv_Drop(playerid, item, strval(inputtext));
	}
	return 1;
}

// ---------------------------------------------------------------------
//  Админам
// ---------------------------------------------------------------------
stock bool:Cmd_GiveFood(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Использование: /givefood [id]");
	Needs_Feed(target, 100);
	Admin_Log(playerid, "givefood", target);
	SendClientMessage(target, COLOR_ADMIN, "Администратор тебя накормил.");
	SendClientMessage(playerid, COLOR_ADMIN, "%s накормлен.", GetRPName(target));
	return true;
}

stock bool:Cmd_GiveItem(playerid, const params[])
{
	if (!Admin_Check(playerid, 4)) return true;
	new pos = 0, item, amount = 1;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !Player[target][pLogged] || !NextIntParam(params, pos, item) || item <= INV_NONE || item >= INV_TYPES)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /giveitem [id] [вещь 1-13] [кол-во]. 1 бургер 2 хот-дог 3 аптечка 4 ремкомплект 5 пицца 6 вода 7 пиво 8 сигареты 9 бинт 10 цветы 11 кости 12 маска 13 материалы");
	NextIntParam(params, pos, amount);
	if (amount < 1) amount = 1;
	if (!Inv_Add(target, item, amount)) return SendClientMessage(playerid, COLOR_GREY, "Не помещается (максимум %d штук).", gInvMax[item]);
	Admin_Log(playerid, "giveitem", target, gInvNames[item], amount);
	SendClientMessage(target, COLOR_ADMIN, "Администратор выдал тебе: %s x%d.", gInvNames[item], amount);
	SendClientMessage(playerid, COLOR_ADMIN, "Выдано %s: %s x%d.", GetRPName(target), gInvNames[item], amount);
	return true;
}


// ---------------------------------------------------------------------
//  Сумка на экране: сетка 6x3 с 3D-моделями вещей и пустыми ячейками,
//  справа карточка выбранной вещи и кнопки USE / GIVE / DROP.
//  Клик по ячейке — выбрать, кнопка — действие, крестик или ESC — закрыть.
//  Надписи латиницей: стандартный шрифт текстдравов не знает кириллицу.
// ---------------------------------------------------------------------
#define INV_UI_CELLS        18
#define INV_UI_COLS         6

// Окно
#define INV_UI_X            120.0
#define INV_UI_Y            112.0
#define INV_UI_R            520.0
#define INV_UI_CELL         44.0
#define INV_UI_STEP         47.0
#define INV_UI_GRID_X       127.0
#define INV_UI_GRID_Y       138.0
#define INV_UI_INFO_X       414.0       // карточка вещи справа
#define INV_UI_INFO_R       514.0

#define INV_UI_COL_BG       0x0E1218E6
#define INV_UI_COL_HEAD     0x1A212BFF
#define INV_UI_COL_ACCENT   0xFFCC66FF
#define INV_UI_COL_CELL     0x222A35FF
#define INV_UI_COL_CELL_SEL 0x5A4A22FF
#define INV_UI_COL_EMPTY    0x161C24FF
#define INV_UI_COL_BTN      0x2E3846FF
#define INV_UI_COL_BTN_OFF  0x1C222BFF
#define INV_UI_COL_LABEL    0x9AA4B2FF

// Модели вещей для окна (по INV_*)
static const gInvModels[INV_TYPES] = { 0, 2703, 19346, 11738, 19921, 2702, 19570, 1544, 19896, 11747, 325, 1851, 19801, 1271, 1577, 1578, 1575, 19630 };
static const gInvNamesEn[INV_TYPES][] = { "", "Burger", "Hot dog", "Medkit", "Repair kit", "Pizza", "Water", "Beer", "Cigarettes", "Bandage", "Flowers", "Dice", "Mask", "Materials", "Weed seeds", "Marijuana", "Cocaine", "Fish" };
static const gInvHintsEn[INV_TYPES][] = { "", "+40% food", "+25% food", "+50 HP", "Repairs a car nearby", "+60% food", "+10% food, +5 HP", "+5 HP, makes you drunk", "Have a smoke", "+20 HP", "Bouquet in your hands", "Roll the dice (RP)", "Hides your nickname", "Craft a weapon", "Plant a bush", "Smoke: effect, +HP", "Use: effect, armour", "Sell at the fishing job" };

// Постоянные текстдравы окна
enum
{
	IUI_BG,
	IUI_HEAD,
	IUI_ACCENT,
	IUI_TITLE,
	IUI_SLOTS,
	IUI_FOOD,
	IUI_CLOSE,
	IUI_FOOTER,
	IUI_INFO_BG,
	IUI_PREVIEW,
	IUI_NAME,
	IUI_COUNT,
	IUI_HINT,
	IUI_BTN_USE,
	IUI_BTN_GIVE,
	IUI_BTN_DROP,
	IUI_TD_COUNT
}

static bool:gInvUiOpen[MAX_PLAYERS];
static PlayerText:gInvUiTD[MAX_PLAYERS][IUI_TD_COUNT];
static PlayerText:gInvUiCell[MAX_PLAYERS][INV_UI_CELLS];     // все 18 ячеек (пустые тоже)
static PlayerText:gInvUiCount[MAX_PLAYERS][INV_UI_CELLS];
static gInvUiItem[MAX_PLAYERS][INV_UI_CELLS];                // INV_*, INV_NONE = канистра, INV_OUTFIT + слот
static gInvUiModel[MAX_PLAYERS][INV_UI_CELLS];
static gInvUiAmount[MAX_PLAYERS][INV_UI_CELLS];
static gInvUiCells[MAX_PLAYERS];                             // сколько ячеек заняты
static gInvUiSel[MAX_PLAYERS] = { -1, ... };

static PlayerText:InvUi_Box(playerid, Float:x, Float:y, Float:x2, Float:lines, colour)
{
	new PlayerText:td = CreatePlayerTextDraw(playerid, x, y, "_");
	PlayerTextDrawLetterSize(playerid, td, 0.0, lines);
	PlayerTextDrawTextSize(playerid, td, x2, 0.0);
	PlayerTextDrawUseBox(playerid, td, true);
	PlayerTextDrawBoxColour(playerid, td, colour);
	PlayerTextDrawShow(playerid, td);
	return td;
}

static PlayerText:InvUi_Text(playerid, Float:x, Float:y, const text[], TEXT_DRAW_FONT:font, Float:sx, Float:sy, colour, TEXT_DRAW_ALIGN:align = TEXT_DRAW_ALIGN_LEFT)
{
	new PlayerText:td = CreatePlayerTextDraw(playerid, x, y, text);
	PlayerTextDrawFont(playerid, td, font);
	PlayerTextDrawLetterSize(playerid, td, sx, sy);
	PlayerTextDrawColour(playerid, td, colour);
	PlayerTextDrawAlignment(playerid, td, align);
	PlayerTextDrawSetOutline(playerid, td, 1);
	PlayerTextDrawSetShadow(playerid, td, 0);
	PlayerTextDrawBackgroundColour(playerid, td, 0x000000AA);
	PlayerTextDrawSetProportional(playerid, td, true);
	PlayerTextDrawShow(playerid, td);
	return td;
}

// Кнопка: текст по центру на плашке, кликабельная
static PlayerText:InvUi_Button(playerid, Float:cx, Float:y, const text[])
{
	new PlayerText:td = CreatePlayerTextDraw(playerid, cx, y, text);
	PlayerTextDrawFont(playerid, td, TEXT_DRAW_FONT_2);
	PlayerTextDrawLetterSize(playerid, td, 0.19, 0.95);
	PlayerTextDrawAlignment(playerid, td, TEXT_DRAW_ALIGN_CENTER);
	PlayerTextDrawColour(playerid, td, 0xFFFFFFFF);
	PlayerTextDrawSetOutline(playerid, td, 0);
	PlayerTextDrawSetShadow(playerid, td, 0);
	PlayerTextDrawSetProportional(playerid, td, true);
	PlayerTextDrawUseBox(playerid, td, true);
	PlayerTextDrawBoxColour(playerid, td, INV_UI_COL_BTN);
	PlayerTextDrawTextSize(playerid, td, 10.0, INV_UI_INFO_R - INV_UI_INFO_X - 12.0);   // по центру: высота, ширина
	PlayerTextDrawSetSelectable(playerid, td, true);
	PlayerTextDrawShow(playerid, td);
	return td;
}

static InvUi_SetButton(playerid, idx, const text[], bool:enabled)
{
	new PlayerText:td = gInvUiTD[playerid][idx];
	PlayerTextDrawSetString(playerid, td, text);
	PlayerTextDrawBoxColour(playerid, td, enabled ? INV_UI_COL_BTN : INV_UI_COL_BTN_OFF);
	PlayerTextDrawColour(playerid, td, enabled ? 0xFFFFFFFF : 0x5C6672FF);
	PlayerTextDrawShow(playerid, td);
}

// Карточка справа: выбранная вещь или подсказка «выбери вещь»
static InvUi_ShowInfo(playerid)
{
	new sel = gInvUiSel[playerid];
	new buf[64];
	if (sel < 0 || sel >= gInvUiCells[playerid])
	{
		PlayerTextDrawHide(playerid, gInvUiTD[playerid][IUI_PREVIEW]);
		if (gInvUiCells[playerid] > 0)
		{
			PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_NAME], "Select an item");
			PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_HINT], "Click a cell on the left");
		}
		else
		{
			PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_NAME], "Bag is empty");
			PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_HINT], "Food and items: 24/7 shops");
		}
		PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_COUNT], "_");
		InvUi_SetButton(playerid, IUI_BTN_USE, "USE", false);
		InvUi_SetButton(playerid, IUI_BTN_GIVE, "GIVE", false);
		InvUi_SetButton(playerid, IUI_BTN_DROP, "DROP", false);
		return;
	}

	new item = gInvUiItem[playerid][sel];
	PlayerTextDrawSetPreviewModel(playerid, gInvUiTD[playerid][IUI_PREVIEW], gInvUiModel[playerid][sel]);
	if (item >= INV_OUTFIT) PlayerTextDrawSetPreviewRot(playerid, gInvUiTD[playerid][IUI_PREVIEW], 0.0, 0.0, 0.0, 1.0);
	else PlayerTextDrawSetPreviewRot(playerid, gInvUiTD[playerid][IUI_PREVIEW], -15.0, 0.0, -30.0, 1.0);
	PlayerTextDrawShow(playerid, gInvUiTD[playerid][IUI_PREVIEW]);

	if (item >= INV_OUTFIT)
	{
		format(buf, sizeof buf, "Outfit #%d", gInvUiModel[playerid][sel]);
		PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_NAME], buf);
		PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_COUNT], "Clothes");
		PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_HINT], "Change your skin");
		InvUi_SetButton(playerid, IUI_BTN_USE, "WEAR", true);
		InvUi_SetButton(playerid, IUI_BTN_GIVE, "GIVE", true);
		InvUi_SetButton(playerid, IUI_BTN_DROP, "DROP", true);
	}
	else if (item == INV_NONE)
	{
		PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_NAME], "Fuel can");
		format(buf, sizeof buf, "%d L", gInvUiAmount[playerid][sel]);
		PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_COUNT], buf);
		PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_HINT], "/canister near a car");
		InvUi_SetButton(playerid, IUI_BTN_USE, "USE", false);
		InvUi_SetButton(playerid, IUI_BTN_GIVE, "GIVE", false);
		InvUi_SetButton(playerid, IUI_BTN_DROP, "DROP", false);
	}
	else
	{
		PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_NAME], gInvNamesEn[item]);
		format(buf, sizeof buf, "%d / %d", gInv[playerid][item], gInvMax[item]);
		PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_COUNT], buf);
		PlayerTextDrawSetString(playerid, gInvUiTD[playerid][IUI_HINT], gInvHintsEn[item]);
		if (item == INV_MASK && gMasked[playerid]) InvUi_SetButton(playerid, IUI_BTN_USE, "TAKE OFF", true);
		else if (item == INV_MASK) InvUi_SetButton(playerid, IUI_BTN_USE, "PUT ON", true);
		else if (item == INV_DICE) InvUi_SetButton(playerid, IUI_BTN_USE, "ROLL", true);
		else InvUi_SetButton(playerid, IUI_BTN_USE, "USE", true);
		InvUi_SetButton(playerid, IUI_BTN_GIVE, "GIVE", true);
		InvUi_SetButton(playerid, IUI_BTN_DROP, "DROP", true);
	}
}

static InvUi_HighlightCell(playerid, cell, bool:on)
{
	if (cell < 0 || cell >= gInvUiCells[playerid]) return;
	PlayerTextDrawBackgroundColour(playerid, gInvUiCell[playerid][cell], on ? INV_UI_COL_CELL_SEL : INV_UI_COL_CELL);
	PlayerTextDrawShow(playerid, gInvUiCell[playerid][cell]);
	if (gInvUiCount[playerid][cell] != INVALID_PLAYER_TEXT_DRAW) PlayerTextDrawShow(playerid, gInvUiCount[playerid][cell]);
}

// Удалить текстдравы окна, не снимая курсор (для перерисовки)
static InvUi_Destroy(playerid)
{
	for (new i = 0; i < IUI_TD_COUNT; i++) PlayerTextDrawDestroy(playerid, gInvUiTD[playerid][i]);
	for (new i = 0; i < INV_UI_CELLS; i++)
	{
		PlayerTextDrawDestroy(playerid, gInvUiCell[playerid][i]);
		if (gInvUiCount[playerid][i] != INVALID_PLAYER_TEXT_DRAW) PlayerTextDrawDestroy(playerid, gInvUiCount[playerid][i]);
		gInvUiCount[playerid][i] = INVALID_PLAYER_TEXT_DRAW;
	}
	gInvUiCells[playerid] = 0;
}

// Открыть окно. keepItem — после действия оставить выбранной ту же вещь
stock Inv_ShowUI(playerid, keepItem = -1)
{
	if (gInvUiOpen[playerid]) InvUi_Destroy(playerid);

	new n = 0;
	for (new item = 1; item < INV_TYPES && n < INV_UI_CELLS; item++)
	{
		if (gInv[playerid][item] <= 0) continue;
		gInvUiItem[playerid][n] = item;
		gInvUiModel[playerid][n] = gInvModels[item];
		gInvUiAmount[playerid][n] = gInv[playerid][item];
		n++;
	}
	new canister = Fuel_GetCanister(playerid);
	if (canister > 0 && n < INV_UI_CELLS)
	{
		gInvUiItem[playerid][n] = INV_NONE;
		gInvUiModel[playerid][n] = 1650;
		gInvUiAmount[playerid][n] = canister;
		n++;
	}
	for (new i = 0; i < MAX_OUTFITS && n < INV_UI_CELLS; i++)
	{
		if (gOutfit[playerid][i] == -1) continue;
		gInvUiItem[playerid][n] = INV_OUTFIT + i;
		gInvUiModel[playerid][n] = gOutfit[playerid][i];
		gInvUiAmount[playerid][n] = 0;
		n++;
	}
	gInvUiCells[playerid] = n;

	gInvUiSel[playerid] = -1;
	if (keepItem != -1)
	{
		for (new i = 0; i < n; i++) if (gInvUiItem[playerid][i] == keepItem) { gInvUiSel[playerid] = i; break; }
	}

	new buf[48];
	// Фон, шапка, полоска-акцент
	gInvUiTD[playerid][IUI_BG]     = InvUi_Box(playerid, INV_UI_X, INV_UI_Y, INV_UI_R, 23.6, INV_UI_COL_BG);
	gInvUiTD[playerid][IUI_HEAD]   = InvUi_Box(playerid, INV_UI_X, INV_UI_Y, INV_UI_R, 1.9, INV_UI_COL_HEAD);
	gInvUiTD[playerid][IUI_ACCENT] = InvUi_Box(playerid, INV_UI_X, INV_UI_Y + 20.0, INV_UI_R, -0.3, INV_UI_COL_ACCENT);
	gInvUiTD[playerid][IUI_TITLE]  = InvUi_Text(playerid, INV_UI_X + 7.0, INV_UI_Y + 3.0, "BAG", TEXT_DRAW_FONT_2, 0.30, 1.3, INV_UI_COL_ACCENT);
	format(buf, sizeof buf, "%d / %d", n, INV_UI_CELLS);
	gInvUiTD[playerid][IUI_SLOTS]  = InvUi_Text(playerid, INV_UI_X + 40.0, INV_UI_Y + 6.0, buf, TEXT_DRAW_FONT_1, 0.20, 0.9, INV_UI_COL_LABEL);
	format(buf, sizeof buf, "FOOD ~%c~%d%%", gHunger[playerid] <= 20 ? 'r' : (gHunger[playerid] <= 50 ? 'y' : 'g'), gHunger[playerid]);
	gInvUiTD[playerid][IUI_FOOD]   = InvUi_Text(playerid, INV_UI_R - 22.0, INV_UI_Y + 6.0, buf, TEXT_DRAW_FONT_2, 0.18, 0.85, 0xFFFFFFFF, TEXT_DRAW_ALIGN_RIGHT);

	// Крестик
	new PlayerText:td = InvUi_Text(playerid, INV_UI_R - 13.0, INV_UI_Y + 3.0, "X", TEXT_DRAW_FONT_1, 0.35, 1.3, 0xFF6347FF);
	PlayerTextDrawTextSize(playerid, td, INV_UI_R - 2.0, 14.0);
	PlayerTextDrawSetSelectable(playerid, td, true);
	PlayerTextDrawShow(playerid, td);
	gInvUiTD[playerid][IUI_CLOSE] = td;

	gInvUiTD[playerid][IUI_FOOTER] = InvUi_Text(playerid, (INV_UI_X + INV_UI_R) / 2.0, INV_UI_Y + 196.0,
		"Click an item, then an action. ESC - close. List: /invlist", TEXT_DRAW_FONT_1, 0.17, 0.8, INV_UI_COL_LABEL, TEXT_DRAW_ALIGN_CENTER);

	// Ячейки: занятые — 3D-модель, пустые — тёмная плашка
	for (new i = 0; i < INV_UI_CELLS; i++)
	{
		new Float:x = INV_UI_GRID_X + float(i % INV_UI_COLS) * INV_UI_STEP;
		new Float:y = INV_UI_GRID_Y + float(i / INV_UI_COLS) * INV_UI_STEP;
		gInvUiCount[playerid][i] = INVALID_PLAYER_TEXT_DRAW;
		td = CreatePlayerTextDraw(playerid, x, y, "_");
		PlayerTextDrawFont(playerid, td, TEXT_DRAW_FONT_MODEL_PREVIEW);
		PlayerTextDrawUseBox(playerid, td, true);
		PlayerTextDrawBoxColour(playerid, td, 0x00000000);
		PlayerTextDrawTextSize(playerid, td, INV_UI_CELL, INV_UI_CELL);
		if (i < n)
		{
			PlayerTextDrawBackgroundColour(playerid, td, i == gInvUiSel[playerid] ? INV_UI_COL_CELL_SEL : INV_UI_COL_CELL);
			PlayerTextDrawSetPreviewModel(playerid, td, gInvUiModel[playerid][i]);
			if (gInvUiItem[playerid][i] >= INV_OUTFIT) PlayerTextDrawSetPreviewRot(playerid, td, 0.0, 0.0, 0.0, 1.0);
			else PlayerTextDrawSetPreviewRot(playerid, td, -15.0, 0.0, -30.0, 1.0);
			PlayerTextDrawSetSelectable(playerid, td, true);
		}
		else
		{
			// пустая ячейка: модель-пустышка на тёмном фоне
			PlayerTextDrawBackgroundColour(playerid, td, INV_UI_COL_EMPTY);
			PlayerTextDrawSetPreviewModel(playerid, td, 19300);
		}
		PlayerTextDrawShow(playerid, td);
		gInvUiCell[playerid][i] = td;

		if (i < n && gInvUiAmount[playerid][i] > 0)
		{
			if (gInvUiItem[playerid][i] == INV_NONE) format(buf, sizeof buf, "%dL", gInvUiAmount[playerid][i]);
			else format(buf, sizeof buf, "x%d", gInvUiAmount[playerid][i]);
			gInvUiCount[playerid][i] = InvUi_Text(playerid, x + INV_UI_CELL - 2.0, y + INV_UI_CELL - 11.0, buf, TEXT_DRAW_FONT_1, 0.20, 0.9, 0xFFFFFFFF, TEXT_DRAW_ALIGN_RIGHT);
		}
	}

	// Карточка вещи справа
	new Float:cx = (INV_UI_INFO_X + INV_UI_INFO_R) / 2.0;
	gInvUiTD[playerid][IUI_INFO_BG] = InvUi_Box(playerid, INV_UI_INFO_X, INV_UI_GRID_Y, INV_UI_INFO_R, 20.1, INV_UI_COL_HEAD);
	td = CreatePlayerTextDraw(playerid, INV_UI_INFO_X + 15.0, INV_UI_GRID_Y + 2.0, "_");
	PlayerTextDrawFont(playerid, td, TEXT_DRAW_FONT_MODEL_PREVIEW);
	PlayerTextDrawUseBox(playerid, td, true);
	PlayerTextDrawBoxColour(playerid, td, 0x00000000);
	PlayerTextDrawBackgroundColour(playerid, td, 0x00000000);
	PlayerTextDrawTextSize(playerid, td, 70.0, 64.0);
	PlayerTextDrawSetPreviewModel(playerid, td, 19300);
	gInvUiTD[playerid][IUI_PREVIEW] = td;
	gInvUiTD[playerid][IUI_NAME]  = InvUi_Text(playerid, cx, INV_UI_GRID_Y + 70.0, "_", TEXT_DRAW_FONT_2, 0.20, 1.0, 0xFFFFFFFF, TEXT_DRAW_ALIGN_CENTER);
	gInvUiTD[playerid][IUI_COUNT] = InvUi_Text(playerid, cx, INV_UI_GRID_Y + 82.0, "_", TEXT_DRAW_FONT_1, 0.20, 0.9, INV_UI_COL_ACCENT, TEXT_DRAW_ALIGN_CENTER);
	gInvUiTD[playerid][IUI_HINT]  = InvUi_Text(playerid, cx, INV_UI_GRID_Y + 93.0, "_", TEXT_DRAW_FONT_1, 0.17, 0.8, INV_UI_COL_LABEL, TEXT_DRAW_ALIGN_CENTER);
	PlayerTextDrawTextSize(playerid, gInvUiTD[playerid][IUI_HINT], 0.0, INV_UI_INFO_R - INV_UI_INFO_X - 6.0);    // перенос строк по ширине карточки
	gInvUiTD[playerid][IUI_BTN_USE]  = InvUi_Button(playerid, cx, INV_UI_GRID_Y + 116.0, "USE");
	gInvUiTD[playerid][IUI_BTN_GIVE] = InvUi_Button(playerid, cx, INV_UI_GRID_Y + 134.0, "GIVE");
	gInvUiTD[playerid][IUI_BTN_DROP] = InvUi_Button(playerid, cx, INV_UI_GRID_Y + 152.0, "DROP");
	InvUi_ShowInfo(playerid);

	if (!gInvUiOpen[playerid])
	{
		gInvUiOpen[playerid] = true;
		SelectTextDraw(playerid, INV_UI_COL_ACCENT);
		printf("[inv-ui] id=%d cells=%d mobile=%d", playerid, n, IsPlayerMobile[playerid] ? 1 : 0);
	}
	return 1;
}

stock Inv_CloseUI(playerid)
{
	if (!gInvUiOpen[playerid]) return;
	gInvUiOpen[playerid] = false;
	InvUi_Destroy(playerid);
	CancelSelectTextDraw(playerid);
}

// При выходе игрока: текстдравы удаляет сервер, только сбросить флаг
stock Inv_UiReset(playerid)
{
	gInvUiOpen[playerid] = false;
	gInvUiCells[playerid] = 0;
	gInvUiSel[playerid] = -1;
}

// Кнопки карточки
static InvUi_DoAction(playerid, action)
{
	new sel = gInvUiSel[playerid];
	if (sel < 0 || sel >= gInvUiCells[playerid]) return;
	new item = gInvUiItem[playerid][sel];

	if (item == INV_NONE)
	{
		SendClientMessage(playerid, COLOR_GREY, "Канистру заливают командой /canister, стоя у машины.");
		return;
	}
	if (item >= INV_OUTFIT)
	{
		new slot = item - INV_OUTFIT;
		if (gOutfit[playerid][slot] == -1) return;
		gInvSelected[playerid] = item;
		switch (action)
		{
			case 0: { Outfit_Wear(playerid, slot); Inv_ShowUI(playerid); }
			case 1:
			{
				Inv_CloseUI(playerid);
				ShowPlayerDialog(playerid, DIALOG_INV_GIVE, DIALOG_STYLE_INPUT, "Передать одежду",
					"{FFFFFF}Введи id игрока, которому передать одежду (скин %d).", "Передать", "Назад", gOutfit[playerid][slot]);
			}
			case 2: { Outfit_Drop(playerid, slot); Inv_ShowUI(playerid); }
		}
		return;
	}
	if (item <= INV_NONE || item >= INV_TYPES || gInv[playerid][item] <= 0) return;
	gInvSelected[playerid] = item;
	switch (action)
	{
		case 0: { Inv_Use(playerid, item); Inv_ShowUI(playerid, item); }
		case 1:
		{
			Inv_CloseUI(playerid);
			ShowPlayerDialog(playerid, DIALOG_INV_GIVE, DIALOG_STYLE_INPUT, "Передать",
				"{FFFFFF}Кому передать: %s?\nВведи id игрока и, если нужно, количество (у тебя %d шт.).\nНапример: {FFFF00}5{FFFFFF} или {FFFF00}5 2", "Передать", "Назад", gInvNames[item], gInv[playerid][item]);
		}
		case 2:
		{
			if (gInv[playerid][item] == 1) { Inv_Drop(playerid, item, 1); Inv_ShowUI(playerid); }
			else
			{
				Inv_CloseUI(playerid);
				ShowPlayerDialog(playerid, DIALOG_INV_DROP, DIALOG_STYLE_INPUT, "Выбросить",
					"{FFFFFF}Сколько выбросить: %s? (у тебя %d шт.)", "Выбросить", "Назад", gInvNames[item], gInv[playerid][item]);
			}
		}
	}
}

// Из OnPlayerClickPlayerTextDraw
stock Inv_OnClickPlayerTD(playerid, PlayerText:td)
{
	if (!gInvUiOpen[playerid]) return 0;
	if (td == gInvUiTD[playerid][IUI_CLOSE])
	{
		Inv_CloseUI(playerid);
		return 1;
	}
	if (td == gInvUiTD[playerid][IUI_BTN_USE])  { InvUi_DoAction(playerid, 0); return 1; }
	if (td == gInvUiTD[playerid][IUI_BTN_GIVE]) { InvUi_DoAction(playerid, 1); return 1; }
	if (td == gInvUiTD[playerid][IUI_BTN_DROP]) { InvUi_DoAction(playerid, 2); return 1; }
	for (new i = 0; i < gInvUiCells[playerid]; i++)
	{
		if (td != gInvUiCell[playerid][i]) continue;
		if (gInvUiSel[playerid] != i)
		{
			InvUi_HighlightCell(playerid, gInvUiSel[playerid], false);
			gInvUiSel[playerid] = i;
			InvUi_HighlightCell(playerid, i, true);
			InvUi_ShowInfo(playerid);
		}
		return 1;
	}
	return 1;
}

// Из OnPlayerClickTextDraw: ESC во время выбора мышью
stock Inv_OnClickTD(playerid, Text:td)
{
	if (td != INVALID_TEXT_DRAW || !gInvUiOpen[playerid]) return 0;
	Inv_CloseUI(playerid);
	return 1;
}
