// =====================================================================
//  fbi.inc — спецсредства ФБР.
//
//  /disguise [скин] — маскировка (на смене): любая гражданская одежда, ник и точка
//                     на карте как у обычного игрока. Снять: /disguise.
//  /tap [номер]     — прослушка телефона (ранг 3+) на TAP_MINUTES минут: звонки и SMS
//                     этого номера приходят агенту. Снять: /untap.
//  /track [id]      — слежка (ранг 2+): метка человека на карте на TRACK_MINUTES минут. Снять: /track.
//  /dossier [id]    — досье: фракция, ранг, розыск, телефон, дом, машина.
//  Всё, кроме досье, пишется в журнал фракции.
// =====================================================================

#define TAP_MINUTES         30
#define TAP_RANK            3
#define TRACK_MINUTES       5
#define TRACK_RANK          2
#define TRACK_ICON_SLOT     98      // gps.inc отдаёт маркерам слоты 0..97
#define TRACK_INTERVAL      3000    // как часто двигается метка (мс)
#define COLOR_TAP           0x8FBC8FFF

static bool:gDisguised[MAX_PLAYERS];
static gTapNumber[MAX_PLAYERS];             // какой номер слушает агент (0 — никакой)
static gTapUntil[MAX_PLAYERS];
static gTrackTarget[MAX_PLAYERS] = { INVALID_PLAYER_ID, ... };
static gTrackUntil[MAX_PLAYERS];
static gTrackTimer[MAX_PLAYERS] = { -1, ... };

static bool:Fbi_Is(playerid, rank = 1)
{
	if (Player[playerid][pFaction] != FACTION_FBI)
	{
		SendClientMessage(playerid, COLOR_GREY, "Это доступно только сотрудникам ФБР.");
		return false;
	}
	if (Player[playerid][pRank] < rank)
	{
		SendClientMessage(playerid, COLOR_GREY, "Нужен ранг %d или выше.", rank);
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------
//  Маскировка
// ---------------------------------------------------------------------
stock bool:Fbi_IsDisguised(playerid)
{
	return gDisguised[playerid] && Player[playerid][pDuty];
}

// Из Faction_EndDuty и при выходе: маскировка снимается вместе с формой
stock Fbi_ResetDisguise(playerid)
{
	gDisguised[playerid] = false;
}

stock bool:Cmd_Disguise(playerid, const params[])
{
	if (!Fbi_Is(playerid)) return true;
	if (!Player[playerid][pDuty]) return SendClientMessage(playerid, COLOR_GREY, "Маскировка — только на смене (/duty).");
	new msg[96];
	new pos = 0, skin;
	if (!NextIntParam(params, pos, skin))
	{
		if (!gDisguised[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Использование: /disguise [скин 1-311] — переодеться в гражданское. Снять: /disguise");
		gDisguised[playerid] = false;
		new rank = Player[playerid][pRank];
		if (rank < 1 || rank > MAX_RANK) rank = 1;
		SetPlayerSkin(playerid, gRankSkin[FACTION_FBI][rank - 1]);
		Faction_UpdateColor(playerid);
		format(msg, sizeof msg, "%s переодевается обратно в форму.", GetRPName(playerid));
		SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
		return true;
	}
	if (skin < 1 || skin > 311 || skin == 74) return SendClientMessage(playerid, COLOR_GREY, "Скин от 1 до 311.");
	gDisguised[playerid] = true;
	SetPlayerSkin(playerid, skin);
	Faction_UpdateColor(playerid);
	format(msg, sizeof msg, "%s переодевается в гражданскую одежду.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(playerid, COLOR_WHITE, "Ты в маскировке: на карте и над головой ты как обычный гражданский. Снять: /disguise");
	format(msg, sizeof msg, "%s ушёл в маскировку.", GetRPName(playerid));
	Faction_Log(FACTION_FBI, msg);
	return true;
}

// ---------------------------------------------------------------------
//  Прослушка
// ---------------------------------------------------------------------
stock bool:Cmd_Tap(playerid, const params[])
{
	if (!Fbi_Is(playerid, TAP_RANK)) return true;
	new pos = 0, number;
	if (!NextIntParam(params, pos, number) || number <= 0)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /tap [номер телефона] — слушать %d мин. Снять: /untap", TAP_MINUTES);
	gTapNumber[playerid] = number;
	gTapUntil[playerid] = gettime() + TAP_MINUTES * 60;
	SendClientMessage(playerid, COLOR_TAP, "Прослушка номера %d на %d мин. Звонки и SMS будут приходить сюда. Снять: /untap", number, TAP_MINUTES);
	new msg[96];
	format(msg, sizeof msg, "%s поставил на прослушку номер %d.", GetRPName(playerid), number);
	Faction_Log(FACTION_FBI, msg);
	return true;
}

stock bool:Cmd_Untap(playerid)
{
	if (gTapNumber[playerid] == 0) return SendClientMessage(playerid, COLOR_GREY, "Ты никого не слушаешь.");
	SendClientMessage(playerid, COLOR_TAP, "Прослушка номера %d снята.", gTapNumber[playerid]);
	gTapNumber[playerid] = 0;
	return true;
}

// Передать перехваченное всем агентам, которые слушают один из двух номеров
static Fbi_TapSend(numA, numB, const text[])
{
	new now = gettime();
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gTapNumber[i] == 0) continue;
		if (!IsPlayerConnected(i) || gTapUntil[i] < now || Player[i][pFaction] != FACTION_FBI)
		{
			if (IsPlayerConnected(i) && gTapUntil[i] < now) SendClientMessage(i, COLOR_TAP, "Прослушка номера %d закончилась.", gTapNumber[i]);
			gTapNumber[i] = 0;
			continue;
		}
		if (gTapNumber[i] == numA || gTapNumber[i] == numB) SendClientMessage(i, COLOR_TAP, "%s", text);
	}
}

// Из phone.inc: звонок начался, реплика в разговоре, SMS
stock Fbi_OnCallStart(caller, callee)
{
	new msg[96];
	format(msg, sizeof msg, "[Прослушка] Звонок: %d -> %d", Player[caller][pPhone], Player[callee][pPhone]);
	Fbi_TapSend(Player[caller][pPhone], Player[callee][pPhone], msg);
}

stock Fbi_OnPhoneText(speaker, listener, const text[])
{
	new msg[144];
	format(msg, sizeof msg, "[Прослушка] %d: %s", Player[speaker][pPhone], text);
	Fbi_TapSend(Player[speaker][pPhone], Player[listener][pPhone], msg);
}

stock Fbi_OnSms(sender, receiver, const text[])
{
	new msg[144];
	format(msg, sizeof msg, "[Прослушка] SMS %d -> %d: %s", Player[sender][pPhone], Player[receiver][pPhone], text);
	Fbi_TapSend(Player[sender][pPhone], Player[receiver][pPhone], msg);
}

// ---------------------------------------------------------------------
//  Слежка
// ---------------------------------------------------------------------
static Fbi_TrackStop(playerid, bool:notify)
{
	if (gTrackTimer[playerid] != -1) { KillTimer(gTrackTimer[playerid]); gTrackTimer[playerid] = -1; }
	if (gTrackTarget[playerid] != INVALID_PLAYER_ID && IsPlayerConnected(playerid)) RemovePlayerMapIcon(playerid, TRACK_ICON_SLOT);
	gTrackTarget[playerid] = INVALID_PLAYER_ID;
	if (notify && IsPlayerConnected(playerid)) SendClientMessage(playerid, COLOR_TAP, "Слежка закончена.");
}

forward OnFbiTrack(playerid);
public OnFbiTrack(playerid)
{
	new t = gTrackTarget[playerid];
	if (t == INVALID_PLAYER_ID || !IsPlayerConnected(playerid) || !IsPlayerConnected(t) || gettime() > gTrackUntil[playerid])
	{
		Fbi_TrackStop(playerid, true);
		return 1;
	}
	new Float:x, Float:y, Float:z;
	if (GetPlayerInterior(t) != 0) return 1;     // в здании метка стоит у последнего места на улице
	GetPlayerPos(t, x, y, z);
	SetPlayerMapIcon(playerid, TRACK_ICON_SLOT, x, y, z, 0, 0xFF3333FF, MAPICON_GLOBAL);
	return 1;
}

stock bool:Cmd_Track(playerid, const params[])
{
	if (!Fbi_Is(playerid, TRACK_RANK)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID)
	{
		if (gTrackTarget[playerid] != INVALID_PLAYER_ID) { Fbi_TrackStop(playerid, true); return true; }
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /track [id] — метка на карте на %d мин. Снять: /track", TRACK_MINUTES);
	}
	if (target == playerid) return SendClientMessage(playerid, COLOR_GREY, "За собой следить незачем.");
	Fbi_TrackStop(playerid, false);
	gTrackTarget[playerid] = target;
	gTrackUntil[playerid] = gettime() + TRACK_MINUTES * 60;
	gTrackTimer[playerid] = SetTimerEx("OnFbiTrack", TRACK_INTERVAL, true, "i", playerid);
	OnFbiTrack(playerid);
	SendClientMessage(playerid, COLOR_TAP, "Слежка за %s [%d]: красная метка на карте, %d мин. Снять: /track", GetRPName(target), target, TRACK_MINUTES);
	new msg[96];
	format(msg, sizeof msg, "%s установил слежку за %s.", GetRPName(playerid), GetRPName(target));
	Faction_Log(FACTION_FBI, msg);
	return true;
}

// ---------------------------------------------------------------------
//  Досье
// ---------------------------------------------------------------------
stock bool:Cmd_Dossier(playerid, const params[])
{
	if (!Fbi_Is(playerid)) return true;
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /dossier [id]");

	new text[512], line[128];
	format(line, sizeof line, "{FFFFFF}Имя: {FFFF66}%s\n{FFFFFF}Уровень: %d\n", GetRPName(target), Player[target][pLevel]);
	strcat(text, line);
	new f = Player[target][pFaction];
	if (Faction_IsValid(f)) format(line, sizeof line, "Организация: %s, %s\n", Faction_GetName(f), Faction_GetRankName(f, Player[target][pRank]));
	else format(line, sizeof line, "Организация: нет\n");
	strcat(text, line);
	format(line, sizeof line, "Розыск: %d зв.\n", Player[target][pWanted]);
	strcat(text, line);
	if (Player[target][pPhone] != 0) format(line, sizeof line, "Телефон: %d\n", Player[target][pPhone]);
	else format(line, sizeof line, "Телефон: нет\n");
	strcat(text, line);
	new h = House_GetOwned(target);
	if (h != -1) format(line, sizeof line, "Дом: №%d\n", House[h][hDbId]);
	else format(line, sizeof line, "Дом: нет\n");
	strcat(text, line);
	new v = gPlayerVeh[target];
	if (v != INVALID_VEHICLE_ID && GetVehicleModel(v) != 0)
	{
		new name[32];
		Hud_GetVehicleName(GetVehicleModel(v), name);
		format(line, sizeof line, "Машина: %s\n", name);
	}
	else format(line, sizeof line, "Машина: нет\n");
	strcat(text, line);
	if (Player[target][pJail] > 0)
	{
		format(line, sizeof line, "{FF6347}Сейчас в тюрьме: %d мин.\n", Player[target][pJail]);
		strcat(text, line);
	}
	ShowPlayerDialog(playerid, DIALOG_ADMIN_INFO, DIALOG_STYLE_MSGBOX, "Досье ФБР", "%s", "Закрыть", "", text);
	return true;
}

stock Fbi_OnDisconnect(playerid)
{
	gDisguised[playerid] = false;
	gTapNumber[playerid] = 0;
	Fbi_TrackStop(playerid, false);
	// агенты, которые следили за ушедшим, увидят конец слежки на следующем тике
}
