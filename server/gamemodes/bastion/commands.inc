// =====================================================================
//  commands.inc Ч список всех команд и общие команды игрока.
//
//   ак добавить свою команду:
//  1. Ќапиши функцию:   stock bool:Cmd_Hello(playerid) { SendClientMessage(playerid, -1, "ѕривет!"); return true; }
//  2. ƒобавь строку в Commands_Process ниже:
//         if (!strcmp(cmd, "/hello", true)) { Cmd_Hello(playerid); return 1; }
//  3. —компилируй (F5) и перезапусти сервер.
// =====================================================================

stock bool:Cmd_Help(playerid)
{
	new text[2700];
	strcat(text, "{FFFF00} нопки\n");
	strcat(text, "{FFFFFF}ALT Ч действие р€дом (дом, банкомат, магазин, работа) или меню игрока перед тобой\n");
	strcat(text, "Y Ч прин€ть / ответить на звонок   N Ч отказатьс€   ¬ машине: 2 Ч двигатель, H Ч шлагбаум\n\n");
	strcat(text, "{FFFF00}ќбщее\n");
	strcat(text, "{FFFFFF}/stats Ч статистика   /time Ч врем€   /taxes Ч налоги и казна   /pay Ч передать деньги   /fps Ч твой FPS\n/report Ч вопрос или жалоба администрации   /admins Ч админы онлайн\n");
	strcat(text, "/inv Ч сумка: использовать, передать, выбросить (подн€ть: ALT или /pickup)   /hunger Ч сытость (поесть: бар или еда из 24/7)\n\n");
	strcat(text, "{FFFF00}RP-чат\n");
	strcat(text, "{FFFFFF}ѕросто пиши в чат Ч теб€ услышат игроки р€дом.\n");
	strcat(text, "/me Ч действие   /do Ч описание   /try Ч попытка   /s Ч крик   /b Ч OOC   /pm Ч Ћ—\n\n");
	strcat(text, "{FFFF00}–абота\n");
	strcat(text, "{FFFFFF}/getjob Ч устроитьс€   /work Ч начать/закончить смену   /quitjob Ч уволитьс€\n");
	strcat(text, "√рузчик и дальнобойщик Ч порт. ѕицца и механик Ч Idlewood. “акси и автобус Ч Unity Station.\n/taxi Ч вызвать такси   ћеханику: /repair /refill [id]   “аксисту: /gotaxi [id]\n\n");
	strcat(text, "{FFFF00}“ранспорт\n");
	strcat(text, "{FFFFFF}/engine Ч двигатель   /buycar Ч автосалон   /park Ч парковка   /lock Ч замок   /sellcar\n");
	strcat(text, "ј«—: H в машине Ч заправка, ALT Ч канистра, /canister Ч залить, /fuel Ч бак, /speedo Ч спидометр\n");
	strcat(text, "—“ќ: H за рулЄм (или /tune) Ч ремонт, покраска, нитро, диски   Ўтрафсто€нка у LSPD: ALT или /unimpound\n");
	strcat(text, "јвтошкола у мэрии: /exam Ч права и лицензии, /licenses Ч мои, /showlic Ч показать\n\n");
	strcat(text, "{FFFF00}ƒом\n");
	strcat(text, "{FFFFFF}/buyhouse   /sellhouse   /enter   /exit   /lock   /house Ч меню (шкаф, сейф, счЄт) Ч или ALT внутри\n");
	strcat(text, "/housepay Ч оплатить налог   /rent /unrent Ч сн€ть комнату   ¬ладельцу: /setrent /tenants /evict /evictall\n\n");
	strcat(text, "{FFFF00}‘ракции\n");
	strcat(text, "{FFFFFF}ѕолици€, больница, банды Grove / Ballas / Aztecas. ¬се команды фракций: /fhelp\n\n");
	strcat(text, "{FFFF00}ƒеньги, телефон, бизнес\n");
	strcat(text, "{FFFFFF}/bank Ч банкомат   /buy Ч купить в магазине   /call /sms /p /h /number /911 Ч телефон\n");
	strcat(text, "/buybiz /sellbiz /bizinfo /bizcash Ч свой бизнес   /turfs /capture Ч районы банд\n\n");
	strcat(text, "{FFFF00}јдминам\n");
	strcat(text, "{FFFFFF}/ahelp Ч команды администрации по уровн€м   /amenu Ч панель (удобно с телефона)\n");
	strcat(text, "/createhouse /createbiz /delbiz /addatm /delatm /addturf /setturf /delturf /addgate /delgate /addfuel /delfuel /setfuel /givelic /addexampoint /clearexam /givefood /giveitem /addgarage /delgarage /fpslist\n–азработка в игре: /pos /points /dev /mo Ч справка по маппингу");
	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "ѕомощь", "%s", "«акрыть", "", text);
	return true;
}

stock bool:Cmd_Stats(playerid)
{
	new house[32], car[32], phone[16];
	if (Player[playerid][pPhone] == 0) strcat(phone, "нет");
	else format(phone, sizeof phone, "%d", Player[playerid][pPhone]);
	new h = House_GetOwned(playerid);
	if (h != -1) format(house, sizeof house, "є%d", House[h][hDbId]);
	else if (House_GetRented(playerid) != -1) format(house, sizeof house, "снимает комнату в є%d", House[House_GetRented(playerid)][hDbId]);
	else strcat(house, "нет");
	if (gPlayerVeh[playerid] == INVALID_VEHICLE_ID) strcat(car, "нет");
	else format(car, sizeof car, "модель %d", GetVehicleModel(gPlayerVeh[playerid]));

	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "—татистика",
		"{FFFFFF}»м€: {33CCFF}%s\n{FFFFFF}”ровень: %d (опыт %d/%d)\nƒеньги: $%d (в банке $%d)\n“елефон: %s\n–абота: %s\n‘ракци€: %s (%s)\n–озыск: %d зв.\n—ытость: %d%%\nƒом: %s\nћашина: %s\nјдминка: %d",
		"«акрыть", "",
		GetRPName(playerid), Player[playerid][pLevel], Player[playerid][pExp], Player[playerid][pLevel] * EXP_PER_LEVEL,
		GetMoney(playerid), Player[playerid][pBank], phone, Job_GetName(Player[playerid][pJob]),
		Faction_GetName(Player[playerid][pFaction]), Faction_GetRankName(Player[playerid][pFaction], Player[playerid][pRank]),
		Player[playerid][pWanted], Needs_GetHunger(playerid), house, car, Player[playerid][pAdmin]);
	return true;
}

stock bool:Cmd_Time(playerid)
{
	new hour, minute;
	gettime(hour, minute);
	SendClientMessage(playerid, COLOR_WHITE, "—ейчас %02d:%02d. «арплата (PayDay) Ч каждый час в :00.", hour, minute);
	return true;
}

stock bool:Cmd_Pay(playerid, const params[])
{
	new pos = 0, amount;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, amount))
		return SendClientMessage(playerid, COLOR_GREY, "»спользование: /pay [id] [сумма]");
	if (target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Ќельз€ передать деньги самому себе.");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "»грок ещЄ не вошЄл в аккаунт.");
	if (amount < 1 || amount > 100000) return SendClientMessage(playerid, COLOR_GREY, "—умма от $1 до $100000.");
	if (GetMoney(playerid) < amount) return SendClientMessage(playerid, COLOR_GREY, "” теб€ нет столько денег.");

	new Float:x, Float:y, Float:z;
	GetPlayerPos(target, x, y, z);
	if (!IsPlayerInRangeOfPoint(playerid, 5.0, x, y, z)) return SendClientMessage(playerid, COLOR_GREY, "»грок слишком далеко.");

	new reason[64];
	format(reason, sizeof reason, "/pay -> %s", GetRPName(target));
	GiveMoney(playerid, -amount, reason);
	format(reason, sizeof reason, "/pay <- %s", GetRPName(playerid));
	GiveMoney(target, amount, reason);
	Account_Save(playerid);
	Account_Save(target);

	new msg[144];
	format(msg, sizeof msg, "%s передаЄт деньги %s.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(target, COLOR_GREEN, "“ы получил $%d от %s.", amount, GetRPName(playerid));
	return true;
}

// ƒл€ меню взаимодействи€ (keys.inc): та же /pay
stock Commands_Pay(playerid, const params[])
{
	Cmd_Pay(playerid, params);
}

// ---------------------------------------------------------------------
//  √лавный разбор команд. ¬ызываетс€ из OnPlayerCommandText.
//  ¬озвращает 1 Ч команда найдена, 0 Ч неизвестна€ команда.
// ---------------------------------------------------------------------
stock Commands_Process(playerid, const cmdtext[])
{
	if (!Player[playerid][pLogged])
	{
		SendClientMessage(playerid, COLOR_GREY, "—начала войди в аккаунт.");
		return 1;
	}

	new cmd[32], pos = 0;
	NextParam(cmdtext, pos, cmd);
	// cmd = "/команда", а cmdtext[pos] Ч всЄ, что после неЄ (параметры)

	// --- администраци€ (admin.inc): мут, /report, все админ-команды ---
	if (Admin_OnCommand(playerid, cmd, cmdtext[pos])) return 1;

	// --- общее ---
	if (!strcmp(cmd, "/help", true))        { Cmd_Help(playerid); return 1; }
	if (!strcmp(cmd, "/stats", true))       { Cmd_Stats(playerid); return 1; }
	if (!strcmp(cmd, "/time", true))        { Cmd_Time(playerid); return 1; }
	if (!strcmp(cmd, "/taxes", true))       { Cmd_Taxes(playerid); return 1; }
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
	if (!strcmp(cmd, "/taxi", true))        { Cmd_Taxi(playerid); return 1; }
	if (!strcmp(cmd, "/inv", true) || !strcmp(cmd, "/i", true)) { Cmd_Inv(playerid); return 1; }
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
	if (!strcmp(cmd, "/fill", true))        { Cmd_Fill(playerid); return 1; }
	if (!strcmp(cmd, "/fuel", true))        { Cmd_Fuel(playerid); return 1; }
	if (!strcmp(cmd, "/canister", true))    { Cmd_Canister(playerid); return 1; }
	if (!strcmp(cmd, "/speedo", true))      { Cmd_Speedo(playerid); return 1; }
	if (!strcmp(cmd, "/exam", true))        { Cmd_Exam(playerid); return 1; }
	if (!strcmp(cmd, "/licenses", true))    { Cmd_Licenses(playerid); return 1; }
	if (!strcmp(cmd, "/showlic", true))     { Cmd_ShowLic(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/checklic", true))    { Cmd_CheckLic(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/takelic", true))     { Cmd_TakeLic(playerid, cmdtext[pos]); return 1; }

	// --- дома (houses.inc) ---
	if (!strcmp(cmd, "/buyhouse", true))    { Cmd_BuyHouse(playerid); return 1; }
	if (!strcmp(cmd, "/sellhouse", true))   { Cmd_SellHouse(playerid); return 1; }
	if (!strcmp(cmd, "/enter", true))       { Cmd_Enter(playerid); return 1; }
	if (!strcmp(cmd, "/exit", true))        { Cmd_Exit(playerid); return 1; }
	if (!strcmp(cmd, "/lock", true))        { Cmd_Lock(playerid); return 1; }
	if (!strcmp(cmd, "/house", true))       { Cmd_House(playerid); return 1; }
	if (!strcmp(cmd, "/housepay", true))    { Cmd_HousePay(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/rent", true))        { Cmd_Rent(playerid); return 1; }
	if (!strcmp(cmd, "/unrent", true))      { Cmd_Unrent(playerid); return 1; }
	if (!strcmp(cmd, "/setrent", true))     { Cmd_SetRent(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/tenants", true))     { Cmd_Tenants(playerid); return 1; }
	if (!strcmp(cmd, "/evict", true))       { Cmd_Evict(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/evictall", true))    { Cmd_EvictAll(playerid); return 1; }

	// --- —“ќ и штрафсто€нка (garage.inc) ---
	if (!strcmp(cmd, "/tune", true))        { Cmd_Tune(playerid); return 1; }
	if (!strcmp(cmd, "/impound", true))     { Cmd_Impound(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/unimpound", true))   { Cmd_Unimpound(playerid); return 1; }
	if (!strcmp(cmd, "/addgarage", true))   { Cmd_AddGarage(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delgarage", true))   { Cmd_DelGarage(playerid); return 1; }

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

	// --- бизнесы (business.inc) ---
	if (!strcmp(cmd, "/buy", true))         { Cmd_Buy(playerid); return 1; }
	if (!strcmp(cmd, "/buybiz", true))      { Cmd_BuyBiz(playerid); return 1; }
	if (!strcmp(cmd, "/sellbiz", true))     { Cmd_SellBiz(playerid); return 1; }
	if (!strcmp(cmd, "/bizinfo", true))     { Cmd_BizInfo(playerid); return 1; }
	if (!strcmp(cmd, "/bizcash", true))     { Cmd_BizCash(playerid); return 1; }
	if (!strcmp(cmd, "/createbiz", true))   { Cmd_CreateBiz(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delbiz", true))      { Cmd_DelBiz(playerid); return 1; }

	// --- кнопки и шлагбаумы (keys.inc, gates.inc) ---
	if (!strcmp(cmd, "/deny", true))        { Cmd_Deny(playerid); return 1; }
	if (!strcmp(cmd, "/frisk", true))       { Cmd_Frisk(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/addgate", true))     { Cmd_AddGate(playerid, cmdtext[pos]); return 1; }
	if (!strcmp(cmd, "/delgate", true))     { Cmd_DelGate(playerid); return 1; }

	// --- админы (admin.inc): остальные команды Ч в Admin_OnCommand ---
	if (!strcmp(cmd, "/createhouse", true))
	{
		if (Admin_Check(playerid, 4)) Cmd_CreateHouse(playerid, cmdtext[pos]);
		return 1;
	}

	SendClientMessage(playerid, COLOR_GREY, "Ќеизвестна€ команда. —писок команд: /help");
	return 1;
}
