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
//  Инвентарь: вещи, купленные в 24/7 (еда, аптечки, ремкомплекты).
//  /inv (или /i) — открыть и использовать, /hunger — сытость.
//  Хранится в таблице inventory (owner, item, amount), до INV_MAX_STACK штук
//  каждой вещи.
//  Админы (4+): /givefood [id] — накормить, /giveitem [id] [вещь 1-4] [кол-во].
// =====================================================================

#define HUNGER_MINUTES      6
#define HUNGER_WARN         20
#define HUNGER_DAMAGE       2
#define INV_MAX_STACK       5

#define DIALOG_INVENTORY    1130

enum
{
	INV_NONE,
	INV_BURGER,
	INV_HOTDOG,
	INV_MEDKIT,
	INV_REPAIRKIT,
	INV_TYPES
}
static const gInvNames[INV_TYPES][] = { "", "Бургер", "Хот-дог", "Аптечка", "Ремкомплект" };
static const gInvHints[INV_TYPES][] = { "", "+40% сытости", "+25% сытости", "+50 HP", "чинит машину рядом" };

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

// Добавить вещь. Возвращает false, если места нет.
stock bool:Inv_Add(playerid, item, amount = 1)
{
	if (item <= INV_NONE || item >= INV_TYPES) return false;
	if (gInv[playerid][item] + amount > INV_MAX_STACK) return false;
	gInv[playerid][item] += amount;
	Inv_Save(playerid, item);
	return true;
}

stock bool:Cmd_Inv(playerid)
{
	new list[512] = "Вещь\tКол-во\tЧто даёт\n", line[96], n = 0;
	for (new item = 1; item < INV_TYPES; item++)
	{
		if (gInv[playerid][item] <= 0) continue;
		format(line, sizeof line, "%s\t%d\t%s\n", gInvNames[item], gInv[playerid][item], gInvHints[item]);
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
	if (n == 0) return SendClientMessage(playerid, COLOR_GREY, "Инвентарь пуст. Еду, аптечки и ремкомплекты продают в магазинах 24/7.");
	for (new i = n; i < INV_TYPES; i++) gInvMenu[playerid][i] = -1;

	new title[48];
	format(title, sizeof title, "Инвентарь (сытость %d%%)", gHunger[playerid]);
	ShowPlayerDialog(playerid, DIALOG_INVENTORY, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Использовать", "Закрыть", list);
	return true;
}

static Inv_Use(playerid, item)
{
	if (gInv[playerid][item] <= 0) return;
	new msg[96];
	switch (item)
	{
		case INV_BURGER, INV_HOTDOG:
		{
			if (gHunger[playerid] >= 100) { SendClientMessage(playerid, COLOR_GREY, "Ты не голоден."); return; }
			Needs_Feed(playerid, (item == INV_BURGER) ? 40 : 25);
			ApplyAnimation(playerid, "FOOD", "EAT_Burger", 4.1, false, false, false, false, 0);
			format(msg, sizeof msg, "%s ест %s.", GetRPName(playerid), gInvNames[item]);
		}
		case INV_MEDKIT:
		{
			new Float:hp;
			GetPlayerHealth(playerid, hp);
			if (hp >= 100.0) { SendClientMessage(playerid, COLOR_GREY, "Ты и так здоров."); return; }
			hp += 50.0;
			if (hp > 100.0) hp = 100.0;
			SetPlayerHealth(playerid, hp);
			format(msg, sizeof msg, "%s достаёт аптечку и перевязывает раны.", GetRPName(playerid));
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
		default: return;
	}
	gInv[playerid][item]--;
	Inv_Save(playerid, item);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
}

stock Needs_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid != DIALOG_INVENTORY) return 0;
	if (!response || listitem < 0 || listitem >= INV_TYPES) return 1;
	new item = gInvMenu[playerid][listitem];
	if (item == INV_NONE)
	{
		SendClientMessage(playerid, COLOR_GREY, "Канистру заливают командой /canister, стоя у машины.");
		return 1;
	}
	if (item > INV_NONE && item < INV_TYPES) Inv_Use(playerid, item);
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
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /giveitem [id] [1 бургер, 2 хот-дог, 3 аптечка, 4 ремкомплект] [кол-во]");
	NextIntParam(params, pos, amount);
	if (amount < 1) amount = 1;
	if (!Inv_Add(target, item, amount)) return SendClientMessage(playerid, COLOR_GREY, "Не помещается (максимум %d штук).", INV_MAX_STACK);
	Admin_Log(playerid, "giveitem", target, gInvNames[item], amount);
	SendClientMessage(target, COLOR_ADMIN, "Администратор выдал тебе: %s x%d.", gInvNames[item], amount);
	SendClientMessage(playerid, COLOR_ADMIN, "Выдано %s: %s x%d.", GetRPName(target), gInvNames[item], amount);
	return true;
}
