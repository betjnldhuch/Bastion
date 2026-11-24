// =====================================================================
//  phone.inc — мобильный телефон.
//
//  Телефон покупается в магазине 24/7 (/buy, business.inc). Номер
//  выдаётся случайный из 6 цифр и сохраняется в аккаунте.
//
//  /number         — мой номер (или /number [id] — номер игрока рядом)
//  /call [номер]   — позвонить     /p — ответить     /h — положить трубку
//  /sms [номер] [текст] — сообщение ($SMS_PRICE)
//  /phone или N — телефон с приложениями (phoneapp.inc)
//  /911 [что случилось] — вызов полиции и медиков
//  Во время разговора обычный чат слышит и собеседник.
// =====================================================================

enum
{
	CALL_NONE,
	CALL_OUTGOING,      // я звоню, гудки
	CALL_INCOMING,      // мне звонят
	CALL_TALKING        // идёт разговор
}

// Найти онлайн-игрока по номеру телефона
stock Phone_FindByNumber(number)
{
	if (number <= 0) return INVALID_PLAYER_ID;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Player[i][pPhone] == number) return i;
	}
	return INVALID_PLAYER_ID;
}

// Новый свободный номер
stock Phone_NewNumber()
{
	new number;
	for (new tries = 0; tries < 50; tries++)
	{
		number = 100000 + random(900000);
		new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id FROM accounts WHERE phone = %d LIMIT 1", number);
		new taken = 0;
		if (r != DBResult:0)
		{
			taken = DB_GetRowCount(r);
			DB_FreeResultSet(r);
		}
		if (taken == 0) break;
	}
	return number;
}

// Выдать телефон (из магазина)
stock Phone_Give(playerid)
{
	Player[playerid][pPhone] = Phone_NewNumber();
	Account_Save(playerid);
	SendClientMessage(playerid, COLOR_GREEN, "Твой номер телефона: %d. Команды: /call /sms /number /911", Player[playerid][pPhone]);
}

stock bool:Phone_Need(playerid)
{
	if (Player[playerid][pPhone] != 0) return true;
	SendClientMessage(playerid, COLOR_GREY, "У тебя нет телефона. Купи его в магазине 24/7 (/buy).");
	return false;
}

// Положить трубку (и у собеседника тоже). Вызывается при /h, смерти, выходе.
stock Phone_HangUp(playerid, bool:notify = true)
{
	new other = Player[playerid][pCallWith];
	new bool:wasTalking = (Player[playerid][pCallState] == CALL_TALKING);
	Player[playerid][pCallWith] = INVALID_PLAYER_ID;
	Player[playerid][pCallState] = CALL_NONE;
	if (IsPlayerConnected(playerid) && wasTalking) SetPlayerSpecialAction(playerid, SPECIAL_ACTION_STOPUSECELLPHONE);

	if (other != INVALID_PLAYER_ID && IsPlayerConnected(other) && Player[other][pCallWith] == playerid)
	{
		if (Player[other][pCallState] == CALL_TALKING) SetPlayerSpecialAction(other, SPECIAL_ACTION_STOPUSECELLPHONE);
		Player[other][pCallWith] = INVALID_PLAYER_ID;
		Player[other][pCallState] = CALL_NONE;
		if (notify) SendClientMessage(other, COLOR_GREY, "Собеседник положил трубку.");
		PhoneApp_OnCallChange(other);
	}
}

// Из Chat_OnText: передать сказанное собеседнику. Возвращает 1, если идёт разговор.
stock Phone_OnText(playerid, const text[])
{
	if (Player[playerid][pCallState] != CALL_TALKING) return 0;
	new other = Player[playerid][pCallWith];
	if (other == INVALID_PLAYER_ID || !IsPlayerConnected(other)) return 0;
	SendClientMessage(other, COLOR_YELLOW, "[Телефон] %s: %s", GetRPName(playerid), text);
	Fbi_OnPhoneText(playerid, other, text);     // прослушка ФБР (fbi.inc)
	return 1;
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Cmd_Number(playerid, const params[])
{
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID)
	{
		if (!Phone_Need(playerid)) return true;
		return SendClientMessage(playerid, COLOR_WHITE, "Твой номер: %d. Номер человека рядом: /number [id]", Player[playerid][pPhone]);
	}
	if (target != playerid && !IsPlayerNearPlayer(playerid, target, 4.0))
		return SendClientMessage(playerid, COLOR_GREY, "Спросить номер можно только у человека рядом.");
	if (Player[target][pPhone] == 0) return SendClientMessage(playerid, COLOR_GREY, "У %s нет телефона.", GetRPName(target));

	SendClientMessage(playerid, COLOR_WHITE, "Номер %s: %d", GetRPName(target), Player[target][pPhone]);
	if (target != playerid) SendClientMessage(target, COLOR_GREY, "%s записал твой номер.", GetRPName(playerid));
	return true;
}

stock bool:Cmd_Call(playerid, const params[])
{
	if (!Phone_Need(playerid)) return true;
	if (Player[playerid][pCuffed] || Player[playerid][pJail] > 0) return SendClientMessage(playerid, COLOR_GREY, "Сейчас нельзя звонить.");
	if (Player[playerid][pCallState] != CALL_NONE) return SendClientMessage(playerid, COLOR_GREY, "Ты уже говоришь по телефону. Положить трубку: /h");

	new pos = 0, number;
	if (!NextIntParam(params, pos, number)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /call [номер]  (полиция и скорая: /911)");
	if (number == 911) return SendClientMessage(playerid, COLOR_GREY, "Для вызова экстренных служб используй /911 [что случилось]");
	if (number == Player[playerid][pPhone]) return SendClientMessage(playerid, COLOR_GREY, "Это твой номер.");

	new target = Phone_FindByNumber(number);
	if (target == INVALID_PLAYER_ID || Player[target][pJail] > 0)
		return SendClientMessage(playerid, COLOR_GREY, "Абонент недоступен.");
	if (Player[target][pCallState] != CALL_NONE)
		return SendClientMessage(playerid, COLOR_GREY, "Занято.");

	Player[playerid][pCallWith] = target;
	Player[playerid][pCallState] = CALL_OUTGOING;
	Player[target][pCallWith] = playerid;
	Player[target][pCallState] = CALL_INCOMING;

	new msg[96];
	format(msg, sizeof msg, "%s достаёт телефон и набирает номер.", GetRPName(playerid));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	format(msg, sizeof msg, "У %s звонит телефон.", GetRPName(target));
	SendNearbyMessage(target, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(target, COLOR_YELLOW, "Входящий звонок с номера %d. Y — ответить, N — сбросить", Player[playerid][pPhone]);
	SendClientMessage(playerid, COLOR_WHITE, "Идут гудки... Положить трубку: /h");
	PhoneApp_OnCallStart(playerid, target);     // журнал звонков, экран звонка (phoneapp.inc)
	return true;
}

stock bool:Cmd_PickUp(playerid)
{
	if (Player[playerid][pCallState] != CALL_INCOMING) return SendClientMessage(playerid, COLOR_GREY, "Тебе никто не звонит.");
	new caller = Player[playerid][pCallWith];
	if (caller == INVALID_PLAYER_ID || !IsPlayerConnected(caller) || Player[caller][pCallWith] != playerid)
	{
		Phone_HangUp(playerid, false);
		return SendClientMessage(playerid, COLOR_GREY, "Звонок сорвался.");
	}
	Player[playerid][pCallState] = CALL_TALKING;
	Player[caller][pCallState] = CALL_TALKING;
	SetPlayerSpecialAction(playerid, SPECIAL_ACTION_USECELLPHONE);
	SetPlayerSpecialAction(caller, SPECIAL_ACTION_USECELLPHONE);
	SendClientMessage(playerid, COLOR_GREEN, "Ты ответил на звонок. Говори в обычный чат. Положить трубку: /h");
	SendClientMessage(caller, COLOR_GREEN, "Абонент ответил. Говори в обычный чат. Положить трубку: /h");
	Fbi_OnCallStart(caller, playerid);
	PhoneApp_OnAnswer(caller, playerid);
	return true;
}

stock bool:Cmd_HangUp(playerid)
{
	if (Player[playerid][pCallState] == CALL_NONE) return SendClientMessage(playerid, COLOR_GREY, "Ты не говоришь по телефону.");
	Phone_HangUp(playerid);
	SendClientMessage(playerid, COLOR_WHITE, "Ты положил трубку.");
	return true;
}

stock bool:Cmd_Sms(playerid, const params[])
{
	if (!Phone_Need(playerid)) return true;
	new pos = 0, number, text[100];
	if (!NextIntParam(params, pos, number) || !RestParam(params, pos, text))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /sms [номер] [текст]");
	PhoneApp_SendSms(playerid, number, text);    // хранит переписку, доходит и тем, кто не в сети
	return true;
}

stock bool:Cmd_911(playerid, const params[])
{
	if (!Phone_Need(playerid)) return true;
	new text[100];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /911 [что случилось и где]");

	new msg[144], count = 0;
	format(msg, sizeof msg, "[911] %s [%d], номер %d: %s", GetRPName(playerid), playerid, Player[playerid][pPhone], text);
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged] || !Player[i][pDuty]) continue;
		if (!Faction_IsLaw(Player[i][pFaction]) && !Faction_IsMedic(Player[i][pFaction])) continue;
		SendClientMessage(i, COLOR_RED, "%s", msg);
		count++;
	}
	if (count == 0) SendClientMessage(playerid, COLOR_GREY, "Диспетчер: сейчас нет свободных экипажей. Попробуй позже.");
	else SendClientMessage(playerid, COLOR_GREEN, "Диспетчер: вызов принят, экипажей на смене: %d.", count);
	return true;
}
