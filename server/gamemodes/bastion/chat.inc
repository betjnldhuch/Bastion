// =====================================================================
//  chat.inc Ч RP-чат.
//  ќбычный текст слышат только игроки р€дом (как в жизни).
//  /me Ч действие, /do Ч описание, /try Ч попытка (удачно/неудачно),
//  /s Ч крик, /b Ч OOC (нероле) р€дом, /pm Ч личное сообщение.
// =====================================================================

// ---------------------------------------------------------------------
//  јнтифлуд: не чаще одного сообщени€ в CHAT_FLOOD_MS и одной команды
//  в CMD_FLOOD_MS. јдмины не ограничены. ѕредупреждение Ч не чаще раза в 2 с.
// ---------------------------------------------------------------------
#define CHAT_FLOOD_MS   1000
#define CMD_FLOOD_MS    500

static gFloodChatAt[MAX_PLAYERS];
static gFloodCmdAt[MAX_PLAYERS];
static gFloodWarnAt[MAX_PLAYERS];

stock Chat_FloodReset(playerid)
{
	new now = GetTickCount();
	gFloodChatAt[playerid] = now - CHAT_FLOOD_MS;
	gFloodCmdAt[playerid] = now - CMD_FLOOD_MS;
	gFloodWarnAt[playerid] = now - 2000;
}

// true Ч сообщение или команду отбросить
stock bool:Chat_IsFlood(playerid, bool:command)
{
	if (Player[playerid][pAdmin] >= 1) return false;
	new now = GetTickCount();
	if (command)
	{
		if (now - gFloodCmdAt[playerid] >= CMD_FLOOD_MS) { gFloodCmdAt[playerid] = now; return false; }
	}
	else if (now - gFloodChatAt[playerid] >= CHAT_FLOOD_MS) { gFloodChatAt[playerid] = now; return false; }
	if (now - gFloodWarnAt[playerid] >= 2000)
	{
		gFloodWarnAt[playerid] = now;
		SendClientMessage(playerid, COLOR_GREY, "Ќе так быстро.");
	}
	return true;
}

// »м€ дл€ чата р€дом: человека в маске не узнать
stock Chat_Name(playerid)
{
	new name[MAX_PLAYER_NAME];
	if (Inv_IsMasked(playerid)) strcat(name, "Ќеизвестный");
	else strcat(name, GetRPName(playerid));
	return name;
}

// ---------------------------------------------------------------------
//   оманды в русской раскладке. ¬ …÷” ≈Ќ клавиша Ђ/ї печатает Ђ.ї, а буквы
//  получаютс€ русскими: Ђ.ыефеыї = Ђ/statsї, Ђ/ыефеыї = Ђ/statsї.
//  ћен€ем раскладку только у имени команды, параметры (текст /me и т.п.) не трогаем.
// ---------------------------------------------------------------------
static stock bool:Chat_IsLetter(c)
{
	c &= 0xFF;
	return (('a' <= c <= 'z') || ('A' <= c <= 'Z') || c >= 0xC0 || c == 0xA8 || c == 0xB8);
}

// Ѕуквы cp1251 а..€ / ј..я (0xC0..0xFF) и Є/® -> клавиши QWERTY на тех же местах
static stock Chat_FixLayout(str[], start, end)
{
	static const layout[] = "f,dult;pbqrkvyjghcnea[wxio]sm'.z";
	for (new i = start; i < end && str[i] != '\0'; i++)
	{
		new c = str[i] & 0xFF;
		if (c >= 0xC0) str[i] = layout[(c - 0xC0) & 31];
		else if (c == 0xA8 || c == 0xB8) str[i] = '`';
	}
}

// true Ч text похож на команду, набранную не в той раскладке; out = исправленна€ Ђ/команда параметрыї
stock bool:Chat_CmdFromLayout(const text[], out[], size = sizeof (out))
{
	new end = 1;
	while (text[end] != '\0' && text[end] != ' ') end++;
	if (text[0] == '.')
	{
		if (!Chat_IsLetter(text[1])) return false;   // Ђ...ї, Ђ. ї Ч обычный текст
	}
	else if (text[0] == '/')
	{
		new bool:ru = false;
		for (new i = 1; i < end; i++)
		{
			new c = text[i] & 0xFF;
			if (c >= 0xC0 || c == 0xA8 || c == 0xB8) { ru = true; break; }
		}
		if (!ru) return false;
	}
	else return false;

	out[0] = '\0';
	strcat(out, text, size);
	out[0] = '/';
	Chat_FixLayout(out, 1, end);
	return true;
}

// »з OnPlayerText
stock Chat_OnText(playerid, const text[])
{
	new full[256], msg[144];
	format(full, sizeof full, "%s говорит: %s", Chat_Name(playerid), text);
	if (strlen(full) < sizeof msg)
	{
		SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_WHITE, full);
	}
	else
	{
		// ƒлинна€ фраза не влезает в одну строку чата (143 символа): переносим по пробелу
		new cut = 120;
		for (new i = 120; i > 80; i--) if (full[i] == ' ') { cut = i; break; }
		strmid(msg, full, 0, cut);
		strcat(msg, " ...");
		SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_WHITE, msg);
		msg[0] = EOS;
		strcat(msg, "... ");
		strcat(msg, full[cut + (full[cut] == ' ' ? 1 : 0)]);
		SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_WHITE, msg);
	}
	SetPlayerChatBubble(playerid, "%s", COLOR_WHITE, CHAT_RADIUS, 5000, text);
	Phone_OnText(playerid, text);   // если говорит по телефону Ч слышит и собеседник (phone.inc)
}

stock bool:Cmd_Me(playerid, const params[])
{
	new text[128], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "»спользование: /me [действие]");
	format(msg, sizeof msg, "%s %s", Chat_Name(playerid), text);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_Do(playerid, const params[])
{
	new text[128], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "»спользование: /do [описание]");
	format(msg, sizeof msg, "%s (%s)", text, Chat_Name(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_Try(playerid, const params[])
{
	new text[110], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "»спользование: /try [действие]");
	if (random(2) == 0) format(msg, sizeof msg, "%s %s {33AA33}(удачно)", Chat_Name(playerid), text);
	else format(msg, sizeof msg, "%s %s {FF6347}(неудачно)", Chat_Name(playerid), text);
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_Shout(playerid, const params[])
{
	new text[128], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "»спользование: /s [текст]");
	format(msg, sizeof msg, "%s кричит: %s!", Chat_Name(playerid), text);
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
