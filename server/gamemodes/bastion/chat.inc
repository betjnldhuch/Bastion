// =====================================================================
//  chat.inc — RP-чат.
//  Обычный текст слышат только игроки рядом (как в жизни).
//  /me — действие, /do — описание, /try — попытка (удачно/неудачно),
//  /s — крик, /b — OOC (нероле) рядом, /pm — личное сообщение.
// =====================================================================

// Имя для чата рядом: человека в маске не узнать
stock Chat_Name(playerid)
{
	new name[MAX_PLAYER_NAME];
	if (Inv_IsMasked(playerid)) strcat(name, "Неизвестный");
	else strcat(name, GetRPName(playerid));
	return name;
}

// Из OnPlayerText
stock Chat_OnText(playerid, const text[])
{
	new msg[144];
	format(msg, sizeof msg, "%s говорит: %s", Chat_Name(playerid), text);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_WHITE, msg);
	SetPlayerChatBubble(playerid, "%s", COLOR_WHITE, CHAT_RADIUS, 5000, text);
	Phone_OnText(playerid, text);   // если говорит по телефону — слышит и собеседник (phone.inc)
}

stock bool:Cmd_Me(playerid, const params[])
{
	new text[128], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /me [действие]");
	format(msg, sizeof msg, "%s %s", Chat_Name(playerid), text);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_Do(playerid, const params[])
{
	new text[128], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /do [описание]");
	format(msg, sizeof msg, "%s (%s)", text, Chat_Name(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_Try(playerid, const params[])
{
	new text[110], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /try [действие]");
	if (random(2) == 0) format(msg, sizeof msg, "%s %s {33AA33}(удачно)", Chat_Name(playerid), text);
	else format(msg, sizeof msg, "%s %s {FF6347}(неудачно)", Chat_Name(playerid), text);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_Shout(playerid, const params[])
{
	new text[128], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /s [текст]");
	format(msg, sizeof msg, "%s кричит: %s!", Chat_Name(playerid), text);
	SendNearbyMessage(playerid, SHOUT_RADIUS, COLOR_WHITE, msg);
	return true;
}

stock bool:Cmd_OOC(playerid, const params[])
{
	new text[128], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /b [текст]");
	format(msg, sizeof msg, "(( %s [%d]: %s ))", GetRPName(playerid), playerid, text);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_GREY, msg);
	return true;
}

stock bool:Cmd_PM(playerid, const params[])
{
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	new text[110];
	if (target == INVALID_PLAYER_ID || !RestParam(params, pos, text))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /pm [id игрока] [текст]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");

	SendClientMessage(target, COLOR_YELLOW, "(( ЛС от %s [%d]: %s ))", GetRPName(playerid), playerid, text);
	SendClientMessage(playerid, COLOR_YELLOW, "(( ЛС для %s [%d]: %s ))", GetRPName(target), target, text);
	return true;
}
