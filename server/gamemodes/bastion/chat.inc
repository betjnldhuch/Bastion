// =====================================================================
//  chat.inc Ч RP-чат.
//  ќбычный текст слышат только игроки р€дом (как в жизни).
//  /me Ч действие, /do Ч описание, /try Ч попытка (удачно/неудачно),
//  /s Ч крик, /b Ч OOC (нероле) р€дом, /pm Ч личное сообщение.
// =====================================================================

// »з OnPlayerText
stock Chat_OnText(playerid, const text[])
{
	new msg[144];
	format(msg, sizeof msg, "%s говорит: %s", GetRPName(playerid), text);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_WHITE, msg);
	SetPlayerChatBubble(playerid, "%s", COLOR_WHITE, CHAT_RADIUS, 5000, text);
}

stock bool:Cmd_Me(playerid, const params[])
{
	new text[128], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "»спользование: /me [действие]");
	format(msg, sizeof msg, "%s %s", GetRPName(playerid), text);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_Do(playerid, const params[])
{
	new text[128], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "»спользование: /do [описание]");
	format(msg, sizeof msg, "%s (%s)", text, GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_Try(playerid, const params[])
{
	new text[110], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "»спользование: /try [действие]");
	if (random(2) == 0) format(msg, sizeof msg, "%s %s {33AA33}(удачно)", GetRPName(playerid), text);
	else format(msg, sizeof msg, "%s %s {FF6347}(неудачно)", GetRPName(playerid), text);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_Shout(playerid, const params[])
{
	new text[128], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "»спользование: /s [текст]");
	format(msg, sizeof msg, "%s кричит: %s!", GetRPName(playerid), text);
	SendNearbyMessage(playerid, SHOUT_RADIUS, COLOR_WHITE, msg);
	return true;
}

stock bool:Cmd_OOC(playerid, const params[])
{
	new text[128], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "»спользование: /b [текст]");
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
		return SendClientMessage(playerid, COLOR_GREY, "»спользование: /pm [id игрока] [текст]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "»грок ещЄ не вошЄл в аккаунт.");

	SendClientMessage(target, COLOR_YELLOW, "(( Ћ— от %s [%d]: %s ))", GetRPName(playerid), playerid, text);
	SendClientMessage(playerid, COLOR_YELLOW, "(( Ћ— дл€ %s [%d]: %s ))", GetRPName(target), target, text);
	return true;
}
