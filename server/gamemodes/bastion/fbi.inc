// =====================================================================
//  fbi.inc — спецсредства ФБР.
//
//  /disguise [скин] — маскировка (на смене): любая гражданская одежда, ник и точка
//                     на карте как у обычного игрока. Снять: /disguise.
//  /fdisguise [фракция] [ранг] — маскировка под члена организации: её форма этого ранга и её цвет.
//  /tap [номер]     — прослушка телефона (ранг 3+) на TAP_MINUTES минут: звонки и SMS
//                     этого номера приходят агенту.
//  /tapf [фракция]  — прослушка рации организации (ранг 3+); у банды ещё и общий чат банд /gd.
//                     Снять любую прослушку: /untap.
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
static gDisguiseF[MAX_PLAYERS];             // под какую организацию замаскирован (0 — гражданский)
static gTapFaction[MAX_PLAYERS];            // какую рацию слушает агент (0 — никакую)
static gTapFUntil[MAX_PLAYERS];
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

// Цвет ника и точки на карте в маскировке
stock Fbi_DisguiseColor(playerid)
{
	new f = gDisguiseF[playerid];
	if (Faction_IsValid(f)) return Faction_GetColor(f);
	return COLOR_CIVIL;
}

// Из Faction_EndDuty и при выходе: маскировка снимается вместе с формой
stock Fbi_ResetDisguise(playerid)
{
	gDisguised[playerid] = false;
	gDisguiseF[playerid] = 0;
}

// /fdisguise [фракция] [ранг] — форма и цвет чужой организации
stock bool:Cmd_FDisguise(playerid, const params[])
{
	if (!Fbi_Is(playerid)) return true;
	if (!Player[playerid][pDuty]) return SendClientMessage(playerid, COLOR_GREY, "Маскировка — только на смене (/duty).");
	new pos = 0, f, rank;
	if (!NextIntParam(params, pos, f) || !NextIntParam(params, pos, rank))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /fdisguise [фракция] [ранг 1-%d] — форма организации (номера: /flist). Снять: /disguise", MAX_RANK);
	if (!Faction_IsValid(f)) return SendClientMessage(playerid, COLOR_GREY, "Такой организации нет. Номера: /flist");
	if (rank < 1 || rank > MAX_RANK) return SendClientMessage(playerid, COLOR_GREY, "Ранг от 1 до %d.", MAX_RANK);
	gDisguised[playerid] = true;
	gDisguiseF[playerid] = f;
	SetPlayerSkin(playerid, gRankSkin[f][rank - 1]);
	Faction_UpdateColor(playerid);
	new msg[128];
	format(msg, sizeof msg, "%s переодевается.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(playerid, COLOR_WHITE, "Ты в форме «%s» (%s). Снять: /disguise", Faction_GetName(f), Faction_GetRankName(f, rank));
	format(msg, sizeof msg, "%s замаскировался под «%s».", GetRPName(playerid), Faction_GetName(f));
	Faction_Log(FACTION_FBI, msg);
	return true;
}

stock bool:Cmd_Disguise(playerid, const params[])
{
	if (!Fbi_Is(playerid)) return true;
	if (!Player[playerid][pDuty]) return SendClientMessage(playerid, COLOR_GREY, "Маскировка — только на смене (/duty).");
	new msg[96];
	new pos = 0, skin;
	if (!NextIntParam(params, pos, skin))
	{
		if (!gDisguised[playerid]) return SendClientMessage(playerid, COLOR_GREY, "Использование: /disguise [скин 1-311] — гражданское; /fdisguise [фракция] [ранг] — форма организации. Снять: /disguise");
		Fbi_ResetDisguise(playerid);
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
	gDisguiseF[playerid] = 0;
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
	if (gTapNumber[playerid] == 0 && gTapFaction[playerid] == 0) return SendClientMessage(playerid, COLOR_GREY, "Ты никого не слушаешь.");
	if (gTapNumber[playerid] != 0) SendClientMessage(playerid, COLOR_TAP, "Прослушка номера %d снята.", gTapNumber[playerid]);
	if (gTapFaction[playerid] != 0) SendClientMessage(playerid, COLOR_TAP, "Прослушка рации снята.");
	gTapNumber[playerid] = 0;
	gTapFaction[playerid] = 0;
	return true;
}

// /tapf [фракция] — слушать рацию организации (у банд — ещё и /gd)
stock bool:Cmd_TapF(playerid, const params[])
{
	if (!Fbi_Is(playerid, TAP_RANK)) return true;
	new pos = 0, f;
	if (!NextIntParam(params, pos, f)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /tapf [фракция] — слушать рацию %d мин (номера: /flist). Снять: /untap", TAP_MINUTES);
	if (!Faction_IsValid(f) || f == FACTION_FBI) return SendClientMessage(playerid, COLOR_GREY, "Такую организацию слушать нельзя. Номера: /flist");
	gTapFaction[playerid] = f;
	gTapFUntil[playerid] = gettime() + TAP_MINUTES * 60;
	if (Faction_IsGang(f)) SendClientMessage(playerid, COLOR_TAP, "Прослушка рации «%s» и чата банд /gd на %d мин. Снять: /untap", Faction_GetName(f), TAP_MINUTES);
	else SendClientMessage(playerid, COLOR_TAP, "Прослушка рации «%s» на %d мин. Снять: /untap", Faction_GetName(f), TAP_MINUTES);
	new msg[96];
	format(msg, sizeof msg, "%s поставил на прослушку рацию «%s».", GetRPName(playerid), Faction_GetName(f));
	Faction_Log(FACTION_FBI, msg);
	return true;
}

// Агент i ещё слушает рацию (срок не вышел)
static bool:Fbi_TapFActive(i)
{
	if (gTapFaction[i] == 0) return false;
	if (!IsPlayerConnected(i) || Player[i][pFaction] != FACTION_FBI || gTapFUntil[i] < gettime())
	{
		if (IsPlayerConnected(i)) SendClientMessage(i, COLOR_TAP, "Прослушка рации закончилась.");
		gTapFaction[i] = 0;
		return false;
	}
	return true;
}

// Из Cmd_FactionChat: рация организации f
stock Fbi_OnRadio(f, const text[])
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gTapFaction[i] == f && Fbi_TapFActive(i)) SendClientMessage(i, COLOR_TAP, "[Прослушка] %s", text);
	}
}

// Из Cmd_GangChat: общий чат банд — тем, кто слушает любую банду
stock Fbi_OnGangChat(const text[])
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gTapFaction[i] != 0 && Faction_IsGang(gTapFaction[i]) && Fbi_TapFActive(i)) SendClientMessage(i, COLOR_TAP, "[Прослушка] %s", text);
	}
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
	Fbi_ResetDisguise(playerid);
	gTapNumber[playerid] = 0;
	gTapFaction[playerid] = 0;
	Fbi_TrackStop(playerid, false);
	// агенты, которые следили за ушедшим, увидят конец слежки на следующем тике
}
