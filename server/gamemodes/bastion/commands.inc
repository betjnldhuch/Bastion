// =====================================================================
//  commands.inc — список всех команд и общие команды игрока.
//
//  Как добавить свою команду:
//  1. Напиши функцию:   stock bool:Cmd_Hello(playerid) { SendClientMessage(playerid, -1, "Привет!"); return true; }
//  2. Добавь строку в Commands_Process ниже:
//         if (!strcmp(cmd, "/hello", true)) { Cmd_Hello(playerid); return 1; }
//  3. Скомпилируй (F5) и перезапусти сервер.
// =====================================================================

// Справка: разделы со всеми командами (help.inc)
stock bool:Cmd_Help(playerid, const params[] = "")
{
	if (params[0] != '\0') return Help_Search(playerid, params);
	return Help_Show(playerid);
}

stock bool:Cmd_Stats(playerid)
{
	new house[40], phone[16];
	if (Player[playerid][pPhone] == 0) strcat(phone, "нет");
	else format(phone, sizeof phone, "%d", Player[playerid][pPhone]);
	new h = House_GetOwned(playerid);
	if (h != -1) format(house, sizeof house, "№%d", House[h][hDbId]);
	else if (House_GetRented(playerid) != -1) format(house, sizeof house, "снимает комнату в №%d", House[House_GetRented(playerid)][hDbId]);
	else strcat(house, "нет");

	// нарушения: варны админов, выговоры в организации, запрет на вступление после увольнения
	new f = Player[playerid][pFaction];
	new warns = Admin_GetWarns(playerid);
	new fwarns = f ? Faction_GetWarns(Player[playerid][pDbId]) : 0, fwarnMax = f ? Org_WarnLimit(f) : 0;
	new bl[160];
	Faction_BlForPlayer(Player[playerid][pDbId], bl, sizeof bl);     // чёрные списки организаций

	new vipLevel = Vip_GetLevel(playerid);
	if (UI_Has(playerid))
	{
		// HTML-окно «stats» (index.html): значок VIP рядом с ником
		new json[1400], e[5][64], blj[200];
		UI_JsonEscape(blj, bl);
		UI_JsonEscape(e[0], Job_GetName(Player[playerid][pJob]));
		UI_JsonEscape(e[1], Faction_GetName(f));
		UI_JsonEscape(e[2], Faction_GetRankName(f, Player[playerid][pRank]));
		UI_JsonEscape(e[3], Family_GetPlayerText(playerid));
		UI_JsonEscape(e[4], house);
		format(json, sizeof json,
			"{\"name\":\"%s\",\"level\":%d,\"exp\":%d,\"expMax\":%d,\"money\":%d,\"bank\":%d,\"phone\":\"%s\",\"job\":\"%s\",\"faction\":\"%s\",\"rank\":\"%s\",\"family\":\"%s\",\"wanted\":%d,\"debt\":%d,\"house\":\"%s\",\"admin\":%d,\"vipLevel\":%d,\"vip\":%d,\"warns\":%d,\"warnMax\":%d,\"fwarns\":%d,\"fwarnMax\":%d,\"bl\":\"%s\"}",
			GetRPName(playerid), Player[playerid][pLevel], Player[playerid][pExp], Player[playerid][pLevel] * EXP_PER_LEVEL,
			GetMoney(playerid), Player[playerid][pBank], phone, e[0], e[1], e[2], e[3], Player[playerid][pWanted], Debts_Total(playerid),
			e[4], Player[playerid][pAdmin], vipLevel, Vip_DaysLeft(playerid), warns, ADMIN_WARNS_FOR_BAN, fwarns, fwarnMax, blj);
		UI_Show(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "stats", json, "Закрыть");
		return true;
	}
	new vip[48];
	if (vipLevel > 0) format(vip, sizeof vip, "%s, ещё %d дн.", Vip_LevelName(vipLevel), Vip_DaysLeft(playerid));
	else strcat(vip, "нет");

	new extra[320];
	format(extra, sizeof extra, "\nВарны: %d/%d", warns, ADMIN_WARNS_FOR_BAN);
	if (f) format(extra, sizeof extra, "%s\nВыговоры в организации: %d/%d", extra, fwarns, fwarnMax);
	if (bl[0]) format(extra, sizeof extra, "%s\n{E5484D}Чёрный список: %s{FFFFFF}", extra, bl);
	if (Player[playerid][pAdmin] > 0) format(extra, sizeof extra, "%s\nАдминка: %d", extra, Player[playerid][pAdmin]);
	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "Статистика",
		"{FFFFFF}Имя: {33CCFF}%s\n{FFFFFF}Уровень: %d (опыт %d/%d)\nДеньги: $%d (в банке $%d)\nДолг штату: $%d\nVIP: %s\nТелефон: %s\nРабота: %s\nФракция: %s (%s)\nСемья: %s\nДом: %s\nРозыск: %d зв.%s\n\n{8C9098}Выписка из тира: /skills · документы: /passport",
		"Закрыть", "",
		GetRPName(playerid), Player[playerid][pLevel], Player[playerid][pExp], Player[playerid][pLevel] * EXP_PER_LEVEL,
		GetMoney(playerid), Player[playerid][pBank], Debts_Total(playerid), vip, phone, Job_GetName(Player[playerid][pJob]),
		Faction_GetName(f), Faction_GetRankName(f, Player[playerid][pRank]),
		Family_GetPlayerText(playerid), house, Player[playerid][pWanted], extra);
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

	new reason[64];
	format(reason, sizeof reason, "/pay -> %s", GetRPName(target));
	GiveMoney(playerid, -amount, reason);
	format(reason, sizeof reason, "/pay <- %s", GetRPName(playerid));
	GiveMoney(target, amount, reason);
	Account_Save(playerid);
	Account_Save(target);

	new msg[144];
	format(msg, sizeof msg, "%s передаёт деньги %s.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(target, COLOR_GREEN, "Ты получил $%d от %s.", amount, GetRPName(playerid));
	return true;
}

// Для меню взаимодействия (keys.inc): та же /pay
stock Commands_Pay(playerid, const params[])
{
	Cmd_Pay(playerid, params);
}

// Команды-действия: раненый и задержанный в наручниках выполнить их не могут
// (иначе из-под ареста уходят через /enter у своего дома, а лёжа торгуют и работают)
static bool:Commands_IsAction(const cmd[])
{
	static const list[][12] =
	{
		"/enter", "/exit", "/lock", "/house", "/buyhouse", "/sellhouse", "/rent",
		"/work", "/getjob", "/taxi", "/exam", "/duty", "/capture", "/rob",
		"/buy", "/buycar", "/sellcar", "/park", "/engine", "/fill", "/canister",
		"/tune", "/unimpound", "/bank", "/atm", "/pay", "/fbank", "/fstorage",
		"/buybiz", "/sellbiz", "/bizcash", "/bizmenu", "/biz", "/sell", "/inv", "/i", "/invlist", "/pickup",
		"/aid", "/carry", "/fish", "/robvan", "/pwork", "/escape", "/marry", "/divorce",
		"/sbank", "/selldrugs", "/tie", "/lead", "/gcar", "/hold", "/hideout", "/handsup"
	};
	for (new i = 0; i < sizeof (list); i++)
	{
		if (!strcmp(cmd, list[i], true)) return true;
	}
	return false;
}

// ---------------------------------------------------------------------
//  Главный разбор команд. Вызывается из OnPlayerCommandText.
//  Возвращает 1 — команда найдена, 0 — неизвестная команда.
// ---------------------------------------------------------------------
//  quiet = true: неизвестная команда не пишет ошибку, а возвращает 0
//  (так команда в русской раскладке «.ыефеы» уходит обычным текстом в чат).
stock Commands_Process(playerid, const input[], bool:quiet = false)
{
	if (!Player[playerid][pLogged])
	{
		SendClientMessage(playerid, COLOR_GREY, "Сначала войди в аккаунт.");
		return 1;
	}

	// Копия строки: у игроков вырезаем цветовые коды {RRGGBB}, чтобы в /me, /do и т.п.
	// нельзя было подделать сообщение сервера или админа
	new cmdtext[145];
	strcat(cmdtext, input);
	if (Player[playerid][pAdmin] < 1) StripColorCodes(cmdtext);

	new cmd[32], pos = 0;
	NextParam(cmdtext, pos, cmd);
	// cmd = "/команда", а cmdtext[pos] — всё, что после неё (параметры)

	// --- администрация (admin.inc): мут, /report, все админ-команды ---
	if (Admin_OnCommand(playerid, cmd, cmdtext[pos])) return 1;

	if ((Player[playerid][pCuffed] || Wound_IsWounded(playerid)) && Commands_IsAction(cmd))
	{
		if (Player[playerid][pCuffed]) SendClientMessage(playerid, COLOR_GREY, "Ты в наручниках и не можешь этого сделать.");
		else SendClientMessage(playerid, COLOR_GREY, "Ты ранен и не можешь этого сделать.");
		return 1;
	}

	// --- общее ---
	if (!strcmp(cmd, "/help", true))        { Cmd_Help(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/stats", true))       { Cmd_Stats(playerid); return 1; }
	if (!strcmp(cmd, "/bot", true))         { Cmd_Bot(playerid); return 1; }
	if (!strcmp(cmd, "/time", true))        { Cmd_Time(playerid); return 1; }
	if (!strcmp(cmd, "/taxes", true))       { Cmd_Taxes(playerid); return 1; }
	if (!strcmp(cmd, "/debts", true))       { Cmd_Debts(playerid); return 1; }
	if (!strcmp(cmd, "/settax", true))      { Cmd_SetTax(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/moneylog", true))    { Cmd_MoneyLog(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/pay", true))         { Cmd_Pay(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/fps", true))         { Cmd_Fps(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/fpslist", true))     { Cmd_FpsList(playerid); return 1; }

	// --- разработка в игре (dev.inc) ---
	if (!strcmp(cmd, "/pos", true))         { Cmd_Pos(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/points", true))      { Cmd_Points(playerid); return 1; }
	if (!strcmp(cmd, "/delpoint", true))    { Cmd_DelPoint(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/dev", true))         { Cmd_Dev(playerid); return 1; }
	if (!strcmp(cmd, "/mo", true))          { Cmd_Mo(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/mt", true))          { Cmd_Mt(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/markers", true) || !strcmp(cmd, "/mk", true)) { Cmd_Markers(playerid); return 1; }
	if (!strcmp(cmd, "/gps", true))         { Cmd_Gps(playerid); return 1; }
	if (!strcmp(cmd, "/quest", true) || !strcmp(cmd, "/quests", true)) { Cmd_Quest(playerid); return 1; }
	if (Daily_OnCommand(playerid, cmd)) return 1;                   // /daily (daily.inc)
	if (Season_OnCommand(playerid, cmd, cmdtext[pos])) return 1;   // /season, /newseason, /seasonxp (season.inc)
	if (FamWar_OnCommand(playerid, cmd, cmdtext[pos])) return 1;   // /famwar, /famwars, /famtop (famwar.inc)
	if (Court_OnCommand(playerid, cmd, cmdtext[pos])) return 1;    // /lawyer, /free, /appeal, /cases (court.inc)
	if (Auction_OnCommand(playerid, cmd, cmdtext[pos])) return 1;  // /auction, /bid, /unauction (auction.inc)

	// --- чат (chat.inc) ---
	if (!strcmp(cmd, "/me", true))          { Cmd_Me(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/do", true))          { Cmd_Do(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/try", true))         { Cmd_Try(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/s", true))           { Cmd_Shout(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/b", true))           { Cmd_OOC(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/pm", true))          { Cmd_PM(playerid, cmdtext[pos]); return 1; }

	// --- новые системы: точки, наркотики, работы, казино, тюрьма, больница, свадьбы ---
	if (!strcmp(cmd, "/place", true))       { Cmd_Place(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/places", true))      { Cmd_Places(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/unplace", true))     { Cmd_Unplace(playerid); return 1; }
	if (!strcmp(cmd, "/gotoplace", true))   { Cmd_GotoPlace(playerid, cmdtext[pos]); return 1; }
	if (Docs_OnCommand(playerid, cmd, cmdtext[pos])) return 1;      // /passport, /showpass, /showmedcard, /showlic (docs.inc)
	if (Drugs_OnCommand(playerid, cmd, cmdtext[pos])) return 1;
	if (WorkLife_OnCommand(playerid, cmd)) return 1;
	if (Casino_OnCommand(playerid, cmd)) return 1;
	if (Prison_OnCommand(playerid, cmd, cmdtext[pos])) return 1;
	if (Hospital_OnCommand(playerid, cmd, cmdtext[pos])) return 1;
	if (Marriage_OnCommand(playerid, cmd, cmdtext[pos])) return 1;
	if (Donate_OnCommand(playerid, cmd, cmdtext[pos])) return 1;
	if (Skills_OnCommand(playerid, cmd, cmdtext[pos])) return 1;    // /skills, /setskill (skills.inc)

	// --- работа (jobs.inc) ---
	if (!strcmp(cmd, "/getjob", true))      { Cmd_GetJob(playerid); return 1; }
	if (!strcmp(cmd, "/quitjob", true))     { Cmd_QuitJob(playerid); return 1; }
	if (!strcmp(cmd, "/jobstat", true) || !strcmp(cmd, "/jobs", true)) { Cmd_JobStat(playerid); return 1; }
	if (!strcmp(cmd, "/taxi", true))        { Cmd_Taxi(playerid); return 1; }
	if (!strcmp(cmd, "/inv", true) || !strcmp(cmd, "/i", true)) { Cmd_Inv(playerid); return 1; }
	if (!strcmp(cmd, "/invlist", true))     { Inv_ShowList(playerid); return 1; }
	if (!strcmp(cmd, "/hunger", true))      { Cmd_Hunger(playerid); return 1; }
	if (!strcmp(cmd, "/pickup", true))      { Cmd_Pickup(playerid); return 1; }
	if (!strcmp(cmd, "/gotaxi", true))      { Cmd_GoTaxi(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/repair", true))      { Cmd_Repair(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/refill", true))      { Cmd_Refill(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/work", true))        { Cmd_Work(playerid); return 1; }

	// --- транспорт (vehicles.inc) ---
	if (!strcmp(cmd, "/engine", true))      { Cmd_Engine(playerid); return 1; }
	if (!strcmp(cmd, "/buycar", true))      { Cmd_BuyCar(playerid); return 1; }
	if (!strcmp(cmd, "/park", true))        { Cmd_Park(playerid); return 1; }
	if (!strcmp(cmd, "/sellcar", true))     { Cmd_SellCar(playerid); return 1; }
	if (Props_OnCommand(playerid, cmd, cmdtext[pos])) return 1;     // /cars, /myhouses, /dealerspawn
	if (!strcmp(cmd, "/fill", true))        { Cmd_Fill(playerid); return 1; }
	if (!strcmp(cmd, "/fuel", true))        { Cmd_Fuel(playerid); return 1; }
	if (!strcmp(cmd, "/canister", true))    { Cmd_Canister(playerid); return 1; }
	if (!strcmp(cmd, "/speedo", true))      { Cmd_Speedo(playerid); return 1; }
	if (!strcmp(cmd, "/hud", true))         { Cmd_Hud(playerid); return 1; }
	if (!strcmp(cmd, "/exam", true))        { Cmd_Exam(playerid); return 1; }
	if (!strcmp(cmd, "/licenses", true))    { Cmd_Licenses(playerid); return 1; }
	if (!strcmp(cmd, "/showlic", true))     { Cmd_ShowLic(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/checklic", true))    { Cmd_CheckLic(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/takelic", true))     { Cmd_TakeLic(playerid, cmdtext[pos]); return 1; }

	// --- дома (houses.inc) ---
	if (!strcmp(cmd, "/setspawn", true))    { Cmd_SetSpawn(playerid); return 1; }
	if (!strcmp(cmd, "/rec", true) || !strcmp(cmd, "/reconnect", true)) { Cmd_Rec(playerid); return 1; }
	if (!strcmp(cmd, "/buyhouse", true))    { Cmd_BuyHouse(playerid); return 1; }
	if (!strcmp(cmd, "/sellhouse", true))   { Cmd_SellHouse(playerid); return 1; }
	if (!strcmp(cmd, "/enter", true))       { Cmd_Enter(playerid); return 1; }
	if (!strcmp(cmd, "/exit", true))        { Cmd_Exit(playerid); return 1; }
	if (!strcmp(cmd, "/lock", true))        { Cmd_Lock(playerid); return 1; }
	if (!strcmp(cmd, "/house", true))       { Cmd_House(playerid); return 1; }
	if (!strcmp(cmd, "/disguise", true))    { Cmd_Disguise(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/tap", true))         { Cmd_Tap(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/tapf", true))        { Cmd_TapF(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/fdisguise", true))   { Cmd_FDisguise(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/untap", true))       { Cmd_Untap(playerid); return 1; }
	if (!strcmp(cmd, "/track", true))       { Cmd_Track(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/dossier", true))     { Cmd_Dossier(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/makestorage", true) || !strcmp(cmd, "/makestorgage", true)) { Cmd_MakeStorage(playerid); return 1; }
	if (!strcmp(cmd, "/housepay", true))    { Cmd_HousePay(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/rent", true))        { Cmd_Rent(playerid); return 1; }
	if (!strcmp(cmd, "/unrent", true))      { Cmd_Unrent(playerid); return 1; }
	if (!strcmp(cmd, "/setrent", true))     { Cmd_SetRent(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/unrentmoped", true)) { Cmd_UnrentMoped(playerid); return 1; }
	if (!strcmp(cmd, "/addmoped", true))    { Cmd_AddMoped(playerid); return 1; }
	if (!strcmp(cmd, "/delmoped", true))    { Cmd_DelMoped(playerid); return 1; }
	if (!strcmp(cmd, "/mopeds", true))      { Cmd_Mopeds(playerid); return 1; }
	if (!strcmp(cmd, "/mopedprice", true))  { Cmd_MopedPrice(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/tenants", true))     { Cmd_Tenants(playerid); return 1; }
	if (!strcmp(cmd, "/evict", true))       { Cmd_Evict(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/evictall", true))    { Cmd_EvictAll(playerid); return 1; }

	// --- СТО и штрафстоянка (garage.inc) ---
	if (!strcmp(cmd, "/tune", true))        { Cmd_Tune(playerid); return 1; }
	if (!strcmp(cmd, "/impound", true))     { Cmd_Impound(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/unimpound", true))   { Cmd_Unimpound(playerid); return 1; }
	if (!strcmp(cmd, "/addgarage", true))   { Cmd_AddGarage(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delgarage", true))   { Cmd_DelGarage(playerid); return 1; }

	// --- фракции (factions.inc) ---
	if (Factions_OnCommand(playerid, cmd, cmdtext[pos])) return 1;   // /lmenu /fbank /fstorage /fedit
	if (Families_OnCommand(playerid, cmd, cmdtext[pos])) return 1;   // /fam... /fc (families.inc)
	if (!strcmp(cmd, "/mark", true))        { Cmd_Mark(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/celldoor", true))    { Cmd_CellDoor(playerid); return 1; }
	if (!strcmp(cmd, "/bribe", true))       { Cmd_Bribe(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/fine", true))        { Cmd_Fine(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/fhelp", true))       { Cmd_FHelp(playerid); return 1; }
	if (!strcmp(cmd, "/f", true))           { Cmd_FactionChat(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/members", true))     { Cmd_MembersWindow(playerid); return 1; }
	if (!strcmp(cmd, "/fr", true) || !strcmp(cmd, "/faction", true)) { FrMenu_Show(playerid); return 1; }
	if (!strcmp(cmd, "/duty", true))        { Cmd_Duty(playerid, cmdtext[pos] != '\0' && !strcmp(cmdtext[pos], "civ", true)); return 1; }
	if (!strcmp(cmd, "/invite", true))      { Cmd_Invite(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/uninvite", true))    { Cmd_Uninvite(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/black", true))       { Cmd_Bl(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/unblack", true))     { Cmd_UnBl(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/blacklist", true))   { Cmd_BList(playerid); return 1; }
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
	if (!strcmp(cmd, "/revive", true))      { Cmd_Revive(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/wounds", true))      { Cmd_Wounds(playerid); return 1; }
	if (!strcmp(cmd, "/aid", true))         { Cmd_Aid(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/dead", true))        { Cmd_Dead(playerid); return 1; }
	if (!strcmp(cmd, "/drag", true))        { Cmd_Drag(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/carry", true))       { Cmd_Carry(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setfaction", true))  { Cmd_SetFaction(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/makeleader", true))  { Cmd_MakeLeader(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setfspawn", true))   { Cmd_SetFSpawn(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/addfspawn", true))   { Cmd_AddFSpawn(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delfspawn", true))   { Cmd_DelFSpawn(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/fspawns", true) || !strcmp(cmd, "/fspawnlist", true)) { Cmd_FSpawns(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/fspawnaccess", true)) { Cmd_FSpawnAccess(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/fspawnsquad", true)) { Cmd_FSpawnSquad(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setfduty", true))    { Cmd_SetFDuty(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setfint", true))     { Cmd_SetFInt(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/adddoor", true) || !strcmp(cmd, "/adddor", true)) { Cmd_AddDoor(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setdoor", true))     { Cmd_SetDoor(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/deldoor", true))     { Cmd_DelDoor(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/doors", true))       { Cmd_Doors(playerid); return 1; }
	if (!strcmp(cmd, "/doorfaction", true)) { Cmd_DoorFaction(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/offices", true))     { Cmd_Offices(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delfint", true))     { Cmd_DelFInt(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/fintlock", true))    { Cmd_FIntLock(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/createfaction", true)) { Cmd_CreateFaction(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delfaction", true))  { Cmd_DelFaction(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/flist", true))       { Cmd_FList(playerid); return 1; }
	if (!strcmp(cmd, "/setftype", true))    { Cmd_SetFType(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/gov", true))         { Cmd_Gov(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/fhide", true))       { Cmd_FHide(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/fcolor", true))      { Cmd_FColor(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/addfactory", true))  { Cmd_AddFactory(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setfpoint", true))   { Cmd_SetFPoint(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setfowner", true))   { Cmd_SetFOwner(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delfactory", true))  { Cmd_DelFactory(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/factories", true))   { Cmd_Factories(playerid); return 1; }
	if (!strcmp(cmd, "/setfstore", true))   { Cmd_SetFStore(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/dropcrate", true))   { Cmd_DropCrate(playerid); return 1; }
	if (!strcmp(cmd, "/unload", true))      { Cmd_Supply(playerid); return 1; }
	if (!strcmp(cmd, "/sabotage", true))    { Cmd_Sabotage(playerid); return 1; }
	if (!strcmp(cmd, "/contracts", true))   { Cmd_Contracts(playerid); return 1; }
	if (!strcmp(cmd, "/market", true))      { Cmd_Market(playerid); return 1; }
	if (!strcmp(cmd, "/srvset", true))      { Cmd_SrvSet(playerid); return 1; }
	if (!strcmp(cmd, "/setarmystore", true)) { Cmd_SetSPoint(playerid, SP_ARMY); return 1; }
	if (!strcmp(cmd, "/setbm", true) || !strcmp(cmd, "/addbm", true)) { Cmd_AddBM(playerid); return 1; }
	if (!strcmp(cmd, "/delbm", true))       { Cmd_DelBM(playerid); return 1; }
	if (!strcmp(cmd, "/squads", true))      { Cmd_Squads(playerid); return 1; }
	if (!strcmp(cmd, "/squadname", true))   { Cmd_SquadName(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setsquad", true))    { Cmd_SetSquad(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/setrankname", true)) { Cmd_SetRankName(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/bmprice", true))     { Cmd_BMPrice(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/bm", true))          { Cmd_BM(playerid); return 1; }
	if (!strcmp(cmd, "/steal", true))       { Cmd_Steal(playerid); return 1; }
	if (!strcmp(cmd, "/mats", true))        { Cmd_Mats(playerid); return 1; }
	if (!strcmp(cmd, "/d", true))           { Cmd_DeptChat(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/gd", true))          { Cmd_GangChat(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/supply", true))      { Cmd_Supply(playerid); return 1; }
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
	if (!strcmp(cmd, "/addfuel", true))     { Cmd_AddFuel(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delfuel", true))     { Cmd_DelFuel(playerid); return 1; }
	if (!strcmp(cmd, "/setfuel", true))     { Cmd_SetFuel(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/givelic", true))     { Cmd_GiveLic(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/givefood", true))    { Cmd_GiveFood(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/giveitem", true))    { Cmd_GiveItem(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/addexampoint", true)) { Cmd_AddExamPoint(playerid); return 1; }
	if (!strcmp(cmd, "/clearexam", true))   { Cmd_ClearExam(playerid); return 1; }
	if (!strcmp(cmd, "/delatm", true))      { Cmd_DelAtm(playerid); return 1; }

	// --- телефон (phone.inc) ---
	if (!strcmp(cmd, "/number", true))      { Cmd_Number(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/call", true))        { Cmd_Call(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/p", true))           { Cmd_PickUp(playerid); return 1; }
	if (!strcmp(cmd, "/h", true))           { Cmd_HangUp(playerid); return 1; }
	if (!strcmp(cmd, "/sms", true))         { Cmd_Sms(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/911", true))         { Cmd_911(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/phone", true) || !strcmp(cmd, "/tel", true)) { Cmd_Phone(playerid); return 1; }
	if (!strcmp(cmd, "/ad", true))          { Cmd_Ad(playerid, cmdtext[pos]); return 1; }

	// --- бизнесы (business.inc) ---
	if (!strcmp(cmd, "/buy", true))         { Cmd_Buy(playerid); return 1; }
	if (!strcmp(cmd, "/rob", true))         { Cmd_Rob(playerid); return 1; }
	// --- похищения (kidnap.inc) ---
	if (!strcmp(cmd, "/tie", true))         { Cmd_Tie(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/untie", true))       { Cmd_Untie(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/lead", true))        { Cmd_Lead(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/gcar", true))        { Cmd_GCar(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/hold", true))        { Cmd_Hold(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/hideout", true))     { Cmd_Hideout(playerid); return 1; }
	if (!strcmp(cmd, "/sethideout", true))  { Cmd_SetHideout(playerid); return 1; }
	if (!strcmp(cmd, "/ransom", true))      { Cmd_Ransom(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/payransom", true))   { Cmd_PayRansom(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/release", true))     { Cmd_Release(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/hostages", true))    { Cmd_Hostages(playerid); return 1; }
	if (!strcmp(cmd, "/handsup", true) || !strcmp(cmd, "/hu", true)) { Cmd_HandsUp(playerid); return 1; }
	if (!strcmp(cmd, "/buybiz", true))      { Cmd_BuyBiz(playerid); return 1; }
	if (!strcmp(cmd, "/sellbiz", true))     { Cmd_SellBiz(playerid); return 1; }
	if (!strcmp(cmd, "/bizinfo", true))     { Cmd_BizInfo(playerid); return 1; }
	if (!strcmp(cmd, "/bizlock", true))     { Cmd_BizLock(playerid); return 1; }
	if (!strcmp(cmd, "/gotobiz", true))     { Cmd_GotoBiz(playerid, cmdtext[pos], false); return 1; }
	if (!strcmp(cmd, "/gotobizint", true))  { Cmd_GotoBiz(playerid, cmdtext[pos], true); return 1; }
	if (!strcmp(cmd, "/bizlist", true))     { Cmd_BizList(playerid); return 1; }
	if (!strcmp(cmd, "/bizcash", true))     { Cmd_BizCash(playerid); return 1; }
	if (!strcmp(cmd, "/bizmenu", true) || !strcmp(cmd, "/biz", true)) { Cmd_BizMenu(playerid); return 1; }
	if (!strcmp(cmd, "/showbiz", true))     { Cmd_ShowBiz(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/sell", true))        { Cmd_BizSell(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/bizquit", true))     { Cmd_BizQuit(playerid); return 1; }
	if (!strcmp(cmd, "/topbiz", true))      { Cmd_TopBiz(playerid); return 1; }
	if (!strcmp(cmd, "/checkbiz", true))    { Cmd_CheckBiz(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/biztypes", true))    { Cmd_BizTypes(playerid); return 1; }
	if (!strcmp(cmd, "/createbiz", true))   { Cmd_CreateBiz(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delbiz", true))      { Cmd_DelBiz(playerid); return 1; }
	if (!strcmp(cmd, "/setbizint", true))   { Cmd_SetBizInt(playerid, cmdtext[pos]); return 1; }

	// --- кнопки и шлагбаумы (keys.inc, gates.inc) ---
	if (!strcmp(cmd, "/deny", true))        { Cmd_Deny(playerid); return 1; }
	if (!strcmp(cmd, "/frisk", true))       { Cmd_Frisk(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/addgate", true))     { Cmd_AddGate(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delgate", true))     { Cmd_DelGate(playerid); return 1; }

	// --- админы (admin.inc): остальные команды — в Admin_OnCommand ---
	if (!strcmp(cmd, "/createhouse", true))
	{
		if (Admin_Check(playerid, 4)) Cmd_CreateHouse(playerid, cmdtext[pos]);
		return 1;
	}
	if (!strcmp(cmd, "/delhouse", true) || !strcmp(cmd, "/dellhouse", true))
	{
		if (Admin_Check(playerid, 4)) Cmd_DelHouse(playerid, cmdtext[pos]);
		return 1;
	}
	if (!strcmp(cmd, "/sethouseint", true))
	{
		if (Admin_Check(playerid, 4)) Cmd_SetHouseInt(playerid, cmdtext[pos]);
		return 1;
	}
	if (!strcmp(cmd, "/addspawn", true))    { if (Admin_Check(playerid, 4)) Cmd_AddSpawn(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delspawn", true))    { if (Admin_Check(playerid, 4)) Cmd_DelSpawn(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/spawns", true))      { if (Admin_Check(playerid, 3)) Cmd_Spawns(playerid); return 1; }
	if (!strcmp(cmd, "/gotospawn", true))   { if (Admin_Check(playerid, 3)) Cmd_GotoSpawn(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/gotohouse", true))
	{
		if (Admin_Check(playerid, 3)) Cmd_GotoHouse(playerid, cmdtext[pos]);
		return 1;
	}

	if (quiet) return 0;
	SendClientMessage(playerid, COLOR_GREY, "Неизвестная команда. Список команд: /help");
	return 1;
}
