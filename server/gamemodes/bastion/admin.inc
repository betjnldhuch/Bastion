// =====================================================================
//  admin.inc — админ-команды.
//
//  Как стать первым админом:
//  1. В config.json задай свой rcon-пароль.
//  2. В игре введи: /rcon login ТВОЙ_ПАРОЛЬ
//  3. Введи: /makeadmin [свой id] 5
//
//  Уровни: 1 — хелпер, 2 — модератор, 3 — админ, 4 — ст. админ, 5 — владелец
// =====================================================================

// Проверка уровня. Пример: if (!Admin_Check(playerid, 3)) return true;
stock bool:Admin_Check(playerid, level)
{
	if (Player[playerid][pAdmin] >= level) return true;
	SendClientMessage(playerid, COLOR_GREY, "Недостаточно прав (нужен уровень админа %d).", level);
	return false;
}

stock bool:Cmd_AdminChat(playerid, const params[])
{
	if (!Admin_Check(playerid, 1)) return true;
	new text[110], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /a [текст]");
	format(msg, sizeof msg, "[A%d] %s: %s", Player[playerid][pAdmin], GetRPName(playerid), text);
	SendAdminMessage(COLOR_ADMIN, msg);
	return true;
}

stock bool:Cmd_MakeAdmin(playerid, const params[])
{
	// Можно через RCON-вход или админу 5 уровня
	if (!IsPlayerAdmin(playerid) && Player[playerid][pAdmin] < 5)
		return SendClientMessage(playerid, COLOR_GREY, "Нужен RCON-вход (/rcon login) или админка 5 уровня.");

	new pos = 0, level;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, level) || level < 0 || level > 5)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /makeadmin [id] [уровень 0-5]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");

	Player[target][pAdmin] = level;
	Account_Save(target);
	SendClientMessage(target, COLOR_ADMIN, "%s выдал тебе админку %d уровня.", GetRPName(playerid), level);
	SendClientMessage(playerid, COLOR_ADMIN, "Ты выдал %s админку %d уровня.", GetRPName(target), level);
	return true;
}

stock bool:Cmd_Goto(playerid, const params[])
{
	if (!Admin_Check(playerid, 1)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /goto [id]");

	new Float:x, Float:y, Float:z;
	GetPlayerPos(target, x, y, z);
	SetPlayerInterior(playerid, GetPlayerInterior(target));
	SetPlayerVirtualWorld(playerid, GetPlayerVirtualWorld(target));
	SetPlayerPos(playerid, x + 1.0, y, z);
	Player[playerid][pHouseInside] = Player[target][pHouseInside];
	return true;
}

stock bool:Cmd_GetHere(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /gethere [id]");

	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	SetPlayerInterior(target, GetPlayerInterior(playerid));
	SetPlayerVirtualWorld(target, GetPlayerVirtualWorld(playerid));
	SetPlayerPos(target, x + 1.0, y, z);
	Player[target][pHouseInside] = Player[playerid][pHouseInside];
	SendClientMessage(target, COLOR_ADMIN, "Тебя телепортировал администратор %s.", GetRPName(playerid));
	return true;
}

stock bool:Cmd_Kick(playerid, const params[])
{
	if (!Admin_Check(playerid, 1)) return true;
	new pos = 0, reason[64];
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /kick [id] [причина]");
	if (!RestParam(params, pos, reason)) strcat(reason, "не указана");

	SendClientMessageToAll(COLOR_RED, "Администратор %s кикнул %s. Причина: %s", GetRPName(playerid), GetRPName(target), reason);
	KickWithDelay(target);
	return true;
}

stock bool:Cmd_Veh(playerid, const params[])
{
	if (!Admin_Check(playerid, 3)) return true;
	new pos = 0, model, c1 = -1, c2 = -1;
	if (!NextIntParam(params, pos, model) || model < 400 || model > 611)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /veh [модель 400-611] [цвет1] [цвет2]");
	NextIntParam(params, pos, c1);
	NextIntParam(params, pos, c2);

	new Float:x, Float:y, Float:z, Float:a;
	GetPlayerPos(playerid, x, y, z);
	GetPlayerFacingAngle(playerid, a);
	new v = CreateVehicle(model, x, y, z + 0.5, a, c1, c2, -1);
	if (v == INVALID_VEHICLE_ID) return SendClientMessage(playerid, COLOR_RED, "Не удалось создать машину.");

	Vehicle_Clear(v);
	gVehAdmin[v] = true;
	LinkVehicleToInterior(v, GetPlayerInterior(playerid));
	SetVehicleVirtualWorld(v, GetPlayerVirtualWorld(playerid));
	PutPlayerInVehicle(playerid, v, 0);
	SendClientMessage(playerid, COLOR_ADMIN, "Машина %d создана (id %d). Удалить: /dveh", model, v);
	return true;
}

stock bool:Cmd_DVeh(playerid)
{
	if (!Admin_Check(playerid, 3)) return true;
	new v = GetPlayerVehicleID(playerid);
	if (v == 0 || !gVehAdmin[v]) return SendClientMessage(playerid, COLOR_GREY, "Сядь в машину, созданную через /veh.");
	Vehicle_Clear(v);
	DestroyVehicle(v);
	SendClientMessage(playerid, COLOR_ADMIN, "Машина удалена.");
	return true;
}

stock bool:Cmd_GiveMoney(playerid, const params[])
{
	if (!Admin_Check(playerid, 5)) return true;
	new pos = 0, amount;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, amount))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /givemoney [id] [сумма]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");

	GiveMoney(target, amount);
	Account_Save(target);
	SendClientMessage(target, COLOR_ADMIN, "Администратор %s выдал тебе $%d.", GetRPName(playerid), amount);
	SendClientMessage(playerid, COLOR_ADMIN, "Ты выдал %s $%d.", GetRPName(target), amount);
	return true;
}

stock bool:Cmd_SetHP(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0, hp;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, hp) || hp < 0 || hp > 100)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /sethp [id] [0-100]");
	SetPlayerHealth(target, float(hp));
	SendClientMessage(playerid, COLOR_ADMIN, "Здоровье %s: %d.", GetRPName(target), hp);
	return true;
}

stock bool:Cmd_SetSkin(playerid, const params[])
{
	if (!Admin_Check(playerid, 2)) return true;
	new pos = 0, skin;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, skin) || skin < 0 || skin > 311)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setskin [id] [скин 0-311]");
	Player[target][pSkin] = skin;
	SetPlayerSkin(target, skin);
	Account_Save(target);
	Account_SetNormalSpawn(target);
	SendClientMessage(playerid, COLOR_ADMIN, "Скин %s изменён на %d.", GetRPName(target), skin);
	return true;
}

stock bool:Cmd_Admins(playerid)
{
	SendClientMessage(playerid, COLOR_ADMIN, "Администраторы онлайн:");
	new count = 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pAdmin] > 0)
		{
			SendClientMessage(playerid, COLOR_WHITE, " %s [%d] — уровень %d", GetRPName(i), i, Player[i][pAdmin]);
			count++;
		}
	}
	if (count == 0) SendClientMessage(playerid, COLOR_GREY, " Сейчас никого нет.");
	return true;
}
