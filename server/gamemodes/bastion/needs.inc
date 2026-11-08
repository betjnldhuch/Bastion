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
//  Админы (4+): /givefood [id] — накормить, /giveitem [id] [вещь] [кол-во].
// =====================================================================

#define HUNGER_MINUTES      6
#define HUNGER_WARN         20
#define HUNGER_DAMAGE       2
#define INV_MAX_STACK       5       // для старого кода: максимум по умолчанию
#define MAX_DROPS           100     // вещей на земле одновременно
#define DROP_MINUTES        10
#define DROP_MODEL          1575    // маленький свёрток

#define DIALOG_INVENTORY    1130
#define DIALOG_INV_ACTION   1131
#define DIALOG_INV_GIVE     1132
#define DIALOG_INV_DROP     1133

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
	INV_TYPES
}
// Номера вещей (для /giveitem): 1 бургер, 2 хот-дог, 3 аптечка, 4 ремкомплект, 5 пицца, 6 вода,
// 7 пиво, 8 сигареты, 9 бинт, 10 цветы, 11 кости, 12 маска
static const gInvNames[INV_TYPES][] = { "", "Бургер", "Хот-дог", "Аптечка", "Ремкомплект", "Пицца", "Вода", "Пиво", "Сигареты", "Бинт", "Цветы", "Игральные кости", "Маска" };
static const gInvHints[INV_TYPES][] = { "", "+40% сытости", "+25% сытости", "+50 HP", "чинит машину рядом", "+60% сытости", "+10% сытости, +5 HP", "+5 HP, пьянит", "покурить", "+20 HP", "букет в руки", "бросить кости (RP)", "прячет ник" };
static const gInvMax[INV_TYPES] = { 0, 5, 5, 3, 3, 3, 5, 5, 20, 5, 3, 1, 1 };

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
static gInvMenu[MAX_PLAYERS][INV_TYPES];

// ---------------------------------------------------------------------
//  Загрузка и сохранение
// ---------------------------------------------------------------------
stock Needs_Init()
{
	DB_AddColumn("accounts", "hunger", "INTEGER DEFAULT 100");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS inventory (owner INTEGER NOT NULL, item INTEGER NOT NULL, amount INTEGER DEFAULT 0, PRIMARY KEY (owner, item))"));
}

// После входа в аккаунт (из Vehicles_LoadForPlayer)
stock Needs_LoadPlayer(playerid)
{
	gHunger[playerid] = 100;
	gHungerTick[playerid] = 0;
	for (new i = 0; i < INV_TYPES; i++) gInv[playerid][i] = 0;

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

stock bool:Cmd_Inv(playerid)
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
	if (n == 0) return SendClientMessage(playerid, COLOR_GREY, "Сумка пуста. Еду, воду, аптечки и другие вещи продают в магазинах 24/7.");
	for (new i = n; i < INV_TYPES; i++) gInvMenu[playerid][i] = -1;

	new title[48];
	format(title, sizeof title, "Сумка (сытость %d%%)", gHunger[playerid]);
	ShowPlayerDialog(playerid, DIALOG_INVENTORY, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Выбрать", "Закрыть", list);
	return true;
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
		if (!response || listitem < 0 || listitem >= INV_TYPES) return 1;
		new item = gInvMenu[playerid][listitem];
		if (item == INV_NONE)
		{
			SendClientMessage(playerid, COLOR_GREY, "Канистру заливают командой /canister, стоя у машины.");
			return 1;
		}
		if (item > INV_NONE && item < INV_TYPES) Inv_ShowActions(playerid, item);
		return 1;
	}

	new item = gInvSelected[playerid];
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
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /giveitem [id] [вещь 1-12] [кол-во]. 1 бургер 2 хот-дог 3 аптечка 4 ремкомплект 5 пицца 6 вода 7 пиво 8 сигареты 9 бинт 10 цветы 11 кости 12 маска");
	NextIntParam(params, pos, amount);
	if (amount < 1) amount = 1;
	if (!Inv_Add(target, item, amount)) return SendClientMessage(playerid, COLOR_GREY, "Не помещается (максимум %d штук).", gInvMax[item]);
	Admin_Log(playerid, "giveitem", target, gInvNames[item], amount);
	SendClientMessage(target, COLOR_ADMIN, "Администратор выдал тебе: %s x%d.", gInvNames[item], amount);
	SendClientMessage(playerid, COLOR_ADMIN, "Выдано %s: %s x%d.", GetRPName(target), gInvNames[item], amount);
	return true;
}
