// =====================================================================
//  commands.inc — список всех команд и общие команды игрока.
//
//  Как добавить свою команду:
//  1. Напиши функцию:   stock bool:Cmd_Hello(playerid) { SendClientMessage(playerid, -1, "Привет!"); return true; }
//  2. Добавь строку в Commands_Process ниже:
//         if (!strcmp(cmd, "/hello", true)) { Cmd_Hello(playerid); return 1; }
//  3. Скомпилируй (F5) и перезапусти сервер.
// =====================================================================

stock bool:Cmd_Help(playerid)
{
	new text[2200];
	strcat(text, "{FFFF00}Общее\n");
	strcat(text, "{FFFFFF}/stats — статистика   /time — время   /pay — передать деньги   /admins — админы\n\n");
	strcat(text, "{FFFF00}RP-чат\n");
	strcat(text, "{FFFFFF}Просто пиши в чат — тебя услышат игроки рядом.\n");
	strcat(text, "/me — действие   /do — описание   /try — попытка   /s — крик   /b — OOC   /pm — ЛС\n\n");
	strcat(text, "{FFFF00}Работа\n");
	strcat(text, "{FFFFFF}/getjob — устроиться   /work — начать/закончить смену   /quitjob — уволиться\n");
	strcat(text, "Грузчик — порт Ocean Docks. Развозчик пиццы — пиццерия Idlewood.\n\n");
	strcat(text, "{FFFF00}Транспорт\n");
	strcat(text, "{FFFFFF}/engine — двигатель   /buycar — автосалон   /park — парковка   /lock — замок   /sellcar\n\n");
	strcat(text, "{FFFF00}Дом\n");
	strcat(text, "{FFFFFF}/buyhouse   /sellhouse   /enter   /exit   /lock\n\n");
	strcat(text, "{FFFF00}Фракции\n");
	strcat(text, "{FFFFFF}Полиция, больница, банды Grove / Ballas / Aztecas. Все команды фракций: /fhelp\n\n");
	strcat(text, "{FFFF00}Деньги, телефон, бизнес\n");
	strcat(text, "{FFFFFF}/bank — банкомат   /buy — купить в магазине   /call /sms /p /h /number /911 — телефон\n");
	strcat(text, "/buybiz /sellbiz /bizinfo /bizcash — свой бизнес   /turfs /capture — районы банд\n\n");
	strcat(text, "{FFFF00}Админам\n");
	strcat(text, "{FFFFFF}/a /goto /gethere /kick /sethp /setskin /veh /dveh /createhouse /givemoney /makeadmin /jail /makeleader\n");
	strcat(text, "/ban /unban /createbiz /delbiz /addatm /delatm /addturf /setturf /delturf");
	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "Помощь", "%s", "Закрыть", "", text);
	return true;
}

stock bool:Cmd_Stats(playerid)
{
	new house[32], car[32], phone[16];
	if (Player[playerid][pPhone] == 0) strcat(phone, "нет");
	else format(phone, sizeof phone, "%d", Player[playerid][pPhone]);
	new h = House_GetOwned(playerid);
	if (h == -1) strcat(house, "нет");
	else format(house, sizeof house, "№%d", House[h][hDbId]);
	if (gPlayerVeh[playerid] == INVALID_VEHICLE_ID) strcat(car, "нет");
	else format(car, sizeof car, "модель %d", GetVehicleModel(gPlayerVeh[playerid]));

	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "Статистика",
		"{FFFFFF}Имя: {33CCFF}%s\n{FFFFFF}Уровень: %d (опыт %d/%d)\nДеньги: $%d (в банке $%d)\nТелефон: %s\nРабота: %s\nФракция: %s (%s)\nРозыск: %d зв.\nДом: %s\nМашина: %s\nАдминка: %d",
		"Закрыть", "",
		GetRPName(playerid), Player[playerid][pLevel], Player[playerid][pExp], Player[playerid][pLevel] * EXP_PER_LEVEL,
		GetMoney(playerid), Player[playerid][pBank], phone, Job_GetName(Player[playerid][pJob]),
		Faction_GetName(Player[playerid][pFaction]), Faction_GetRankName(Player[playerid][pFaction], Player[playerid][pRank]),
		Player[playerid][pWanted], house, car, Player[playerid][pAdmin]);
	return true;
}

stock bool:Cmd_Time(playerid)
{
	new hour, minute;
	gettime(hour, minute);
	SendClientMessage(playerid, COLOR_WHITE, "Сейчас %02d:%02d. Зарплата (PayDay) — каждый час в :00.", hour, minute);
	return true;
}

stock bool:Cmd_Pay(playerid, const params[])
{
	new pos = 0, amount;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, amount))
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /pay [id] [сумма]");
	if (target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Нельзя передать деньги самому себе.");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	if (amount < 1 || amount > 100000) return SendClientMessage(playerid, COLOR_GREY, "Сумма от $1 до $100000.");
	if (GetMoney(playerid) < amount) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет столько денег.");

	new Float:x, Float:y, Float:z;
	GetPlayerPos(target, x, y, z);
	if (!IsPlayerInRangeOfPoint(playerid, 5.0, x, y, z)) return SendClientMessage(playerid, COLOR_GREY, "Игрок слишком далеко.");

	GiveMoney(playerid, -amount);
	GiveMoney(target, amount);
	Account_Save(playerid);
	Account_Save(target);

	new msg[144];
	format(msg, sizeof msg, "%s передаёт деньги %s.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(target, COLOR_GREEN, "Ты получил $%d от %s.", amount, GetRPName(playerid));
	return true;
}

// ---------------------------------------------------------------------
//  Главный разбор команд. Вызывается из OnPlayerCommandText.
//  Возвращает 1 — команда найдена, 0 — неизвестная команда.
// ---------------------------------------------------------------------
stock Commands_Process(playerid, const cmdtext[])
{
	if (!Player[playerid][pLogged])
	{
		SendClientMessage(playerid, COLOR_GREY, "Сначала войди в аккаунт.");
		return 1;
	}

	new cmd[32], pos = 0;
	NextParam(cmdtext, pos, cmd);
	// cmd = "/команда", а cmdtext[pos] — всё, что после неё (параметры)

	// --- общее ---
	if (!strcmp(cmd, "/help", true))        { Cmd_Help(playerid); return 1; }
	if (!strcmp(cmd, "/stats", true))       { Cmd_Stats(playerid); return 1; }
	if (!strcmp(cmd, "/time", true))        { Cmd_Time(playerid); return 1; }
	if (!strcmp(cmd, "/pay", true))         { Cmd_Pay(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/admins", true))      { Cmd_Admins(playerid); return 1; }

	// --- чат (chat.inc) ---
	if (!strcmp(cmd, "/me", true))          { Cmd_Me(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/do", true))          { Cmd_Do(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/try", true))         { Cmd_Try(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/s", true))           { Cmd_Shout(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/b", true))           { Cmd_OOC(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/pm", true))          { Cmd_PM(playerid, cmdtext[pos]); return 1; }

	// --- работа (jobs.inc) ---
	if (!strcmp(cmd, "/getjob", true))      { Cmd_GetJob(playerid); return 1; }
	if (!strcmp(cmd, "/quitjob", true))     { Cmd_QuitJob(playerid); return 1; }
	if (!strcmp(cmd, "/work", true))        { Cmd_Work(playerid); return 1; }

	// --- транспорт (vehicles.inc) ---
	if (!strcmp(cmd, "/engine", true))      { Cmd_Engine(playerid); return 1; }
	if (!strcmp(cmd, "/buycar", true))      { Cmd_BuyCar(playerid); return 1; }
	if (!strcmp(cmd, "/park", true))        { Cmd_Park(playerid); return 1; }
	if (!strcmp(cmd, "/sellcar", true))     { Cmd_SellCar(playerid); return 1; }

	// --- дома (houses.inc) ---
	if (!strcmp(cmd, "/buyhouse", true))    { Cmd_BuyHouse(playerid); return 1; }
	if (!strcmp(cmd, "/sellhouse", true))   { Cmd_SellHouse(playerid); return 1; }
	if (!strcmp(cmd, "/enter", true))       { Cmd_Enter(playerid); return 1; }
	if (!strcmp(cmd, "/exit", true))        { Cmd_Exit(playerid); return 1; }
	if (!strcmp(cmd, "/lock", true))        { Cmd_Lock(playerid); return 1; }

	// --- фракции (factions.inc) ---
	if (!strcmp(cmd, "/fhelp", true))       { Cmd_FHelp(playerid); return 1; }
	if (!strcmp(cmd, "/f", true))           { Cmd_FactionChat(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/members", true))     { Cmd_Members(playerid); return 1; }
	if (!strcmp(cmd, "/duty", true))        { Cmd_Duty(playerid); return 1; }
	if (!strcmp(cmd, "/invite", true))      { Cmd_Invite(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/uninvite", true))    { Cmd_Uninvite(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/giverank", true))    { Cmd_GiveRank(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/fleave", true))      { Cmd_FLeave(playerid); return 1; }
	if (!strcmp(cmd, "/accept", true))      { Cmd_Accept(playerid); return 1; }
	if (!strcmp(cmd, "/m", true))           { Cmd_Megaphone(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/su", true))          { Cmd_Suspect(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/clear", true))       { Cmd_ClearWanted(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/wanted", true))      { Cmd_Wanted(playerid); return 1; }
	if (!strcmp(cmd, "/cuff", true))        { Cmd_Cuff(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/uncuff", true))      { Cmd_Uncuff(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/putcar", true))      { Cmd_PutCar(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/arrest", true))      { Cmd_Arrest(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/heal", true))        { Cmd_Heal(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setfaction", true))  { Cmd_SetFaction(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/makeleader", true))  { Cmd_MakeLeader(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setfspawn", true))   { Cmd_SetFSpawn(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setfduty", true))    { Cmd_SetFDuty(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/addfcar", true))     { Cmd_AddFCar(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delfcar", true))     { Cmd_DelFCar(playerid); return 1; }
	if (!strcmp(cmd, "/jail", true))        { Cmd_Jail(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/unjail", true))      { Cmd_Unjail(playerid, cmdtext[pos]); return 1; }

	// --- районы банд (turfs.inc) ---
	if (!strcmp(cmd, "/turfs", true))       { Cmd_Turfs(playerid); return 1; }
	if (!strcmp(cmd, "/capture", true))     { Cmd_Capture(playerid); return 1; }
	if (!strcmp(cmd, "/addturf", true))     { Cmd_AddTurf(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setturf", true))     { Cmd_SetTurf(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delturf", true))     { Cmd_DelTurf(playerid); return 1; }

	// --- банк (bank.inc) ---
	if (!strcmp(cmd, "/bank", true) || !strcmp(cmd, "/atm", true)) { Cmd_Bank(playerid); return 1; }
	if (!strcmp(cmd, "/addatm", true))      { Cmd_AddAtm(playerid); return 1; }
	if (!strcmp(cmd, "/delatm", true))      { Cmd_DelAtm(playerid); return 1; }

	// --- телефон (phone.inc) ---
	if (!strcmp(cmd, "/number", true))      { Cmd_Number(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/call", true))        { Cmd_Call(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/p", true))           { Cmd_PickUp(playerid); return 1; }
	if (!strcmp(cmd, "/h", true))           { Cmd_HangUp(playerid); return 1; }
	if (!strcmp(cmd, "/sms", true))         { Cmd_Sms(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/911", true))         { Cmd_911(playerid, cmdtext[pos]); return 1; }

	// --- бизнесы (business.inc) ---
	if (!strcmp(cmd, "/buy", true))         { Cmd_Buy(playerid); return 1; }
	if (!strcmp(cmd, "/buybiz", true))      { Cmd_BuyBiz(playerid); return 1; }
	if (!strcmp(cmd, "/sellbiz", true))     { Cmd_SellBiz(playerid); return 1; }
	if (!strcmp(cmd, "/bizinfo", true))     { Cmd_BizInfo(playerid); return 1; }
	if (!strcmp(cmd, "/bizcash", true))     { Cmd_BizCash(playerid); return 1; }
	if (!strcmp(cmd, "/createbiz", true))   { Cmd_CreateBiz(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delbiz", true))      { Cmd_DelBiz(playerid); return 1; }

	// --- админы (admin.inc) ---
	if (!strcmp(cmd, "/ban", true))         { Cmd_Ban(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/unban", true))       { Cmd_Unban(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/a", true))           { Cmd_AdminChat(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/makeadmin", true))   { Cmd_MakeAdmin(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/goto", true))        { Cmd_Goto(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/gethere", true))     { Cmd_GetHere(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/kick", true))        { Cmd_Kick(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/veh", true))         { Cmd_Veh(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/dveh", true))        { Cmd_DVeh(playerid); return 1; }
	if (!strcmp(cmd, "/givemoney", true))   { Cmd_GiveMoney(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/sethp", true))       { Cmd_SetHP(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setskin", true))     { Cmd_SetSkin(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/createhouse", true))
	{
		if (Admin_Check(playerid, 4)) Cmd_CreateHouse(playerid, cmdtext[pos]);
		return 1;
	}

	SendClientMessage(playerid, COLOR_GREY, "Неизвестная команда. Список команд: /help");
	return 1;
}
