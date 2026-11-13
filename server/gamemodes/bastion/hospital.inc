// =====================================================================
//  hospital.inc — больница: платное лечение, страховка, медкарта.
//
//  Регистратура (/place clinic, ALT рядом):
//    лечение до 100 HP — HOSP_TREAT_PRICE (со страховкой HOSP_TREAT_INSURED);
//    страховка на HOSP_INSURE_DAYS дней — HOSP_INSURE_PRICE: счёт больницы после
//      смерти не берут, лечение дешевле;
//    медкарта на HOSP_MEDCARD_DAYS дней — HOSP_MEDCARD_PRICE: нужна для лицензии
//      на оружие. С наркозависимостью медкарту не дают;
//    лечение зависимости — HOSP_REHAB_PRICE.
//  Деньги идут в общак больницы (фракция 2).
//  Медик на смене (ранг 3+): /givemedcard [id] — выписать медкарту, деньги ему.
//  Все: /medcard — своя медкарта и страховка, /showmedcard [id] — показать.
// =====================================================================

#define HOSP_TREAT_PRICE        300
#define HOSP_TREAT_INSURED      50
#define HOSP_INSURE_PRICE       2500
#define HOSP_INSURE_DAYS        7
#define HOSP_MEDCARD_PRICE      600
#define HOSP_MEDCARD_DAYS       30
#define HOSP_REHAB_PRICE        1500
#define HOSP_MEDIC_MEDCARD      400     // медик выписывает медкарту: столько платит пациент (медику)

static gInsured[MAX_PLAYERS];           // до какого времени (gettime) действует страховка
static gMedcard[MAX_PLAYERS];           // до какого времени действует медкарта

stock Hospital_Init()
{
	DB_AddColumn("accounts", "insured", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "medcard", "INTEGER DEFAULT 0");
}

stock Hospital_LoadPlayer(playerid)
{
	gInsured[playerid] = 0;
	gMedcard[playerid] = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT insured, medcard FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return;
	if (DB_GetRowCount(r) > 0)
	{
		gInsured[playerid] = DB_GetFieldInt(r, 0);
		gMedcard[playerid] = DB_GetFieldInt(r, 1);
	}
	DB_FreeResultSet(r);
}

static Hospital_Save(playerid)
{
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET insured = %d, medcard = %d WHERE id = %d", gInsured[playerid], gMedcard[playerid], Player[playerid][pDbId]));
}

stock bool:Hospital_IsInsured(playerid) { return gInsured[playerid] > gettime(); }
stock bool:Hospital_HasMedcard(playerid) { return gMedcard[playerid] > gettime(); }

static Hospital_DaysLeft(until)
{
	new left = until - gettime();
	if (left <= 0) return 0;
	return left / 86400 + 1;
}

// Деньги больнице: в общак медиков
static Hospital_Income(amount)
{
	if (Faction_IsValid(FACTION_EMS))
	{
		Faction[FACTION_EMS][fBank] += amount;
		Faction_SaveBank(FACTION_EMS);
	}
	else Econ_ToTreasury(amount);
}

// После смерти (из OnPlayerSpawn): счёт больницы
stock Hospital_Bill(playerid)
{
	if (Hospital_IsInsured(playerid))
	{
		SendClientMessage(playerid, COLOR_WHITE, "Тебя вылечили в больнице. Лечение оплатила страховка.");
		return;
	}
	GiveMoney(playerid, -HOSPITAL_FEE, "счёт больницы");
	Hospital_Income(HOSPITAL_FEE);
	SendClientMessage(playerid, COLOR_WHITE, "Тебя вылечили в больнице. Счёт за лечение: $%d. Страховка — в регистратуре.", HOSPITAL_FEE);
}

// ---------------------------------------------------------------------
//  Регистратура
// ---------------------------------------------------------------------
static Hospital_ShowDesk(playerid)
{
	new list[512], line[128];
	strcat(list, "Услуга\tЦена\n");
	format(line, sizeof line, "Лечение (до 100 HP)\t$%d\n", Hospital_IsInsured(playerid) ? HOSP_TREAT_INSURED : HOSP_TREAT_PRICE);
	strcat(list, line);
	if (Hospital_IsInsured(playerid)) format(line, sizeof line, "Продлить страховку (+%d дн., сейчас ещё %d дн.)\t$%d\n", HOSP_INSURE_DAYS, Hospital_DaysLeft(gInsured[playerid]), HOSP_INSURE_PRICE);
	else format(line, sizeof line, "Страховка на %d дн.\t$%d\n", HOSP_INSURE_DAYS, HOSP_INSURE_PRICE);
	strcat(list, line);
	if (Hospital_HasMedcard(playerid)) format(line, sizeof line, "Медкарта (есть, ещё %d дн.) — продлить\t$%d\n", Hospital_DaysLeft(gMedcard[playerid]), HOSP_MEDCARD_PRICE);
	else format(line, sizeof line, "Медкарта на %d дн. (нужна для оружия)\t$%d\n", HOSP_MEDCARD_DAYS, HOSP_MEDCARD_PRICE);
	strcat(list, line);
	format(line, sizeof line, "Лечение от зависимости (сейчас %d%%)\t$%d\n", Drugs_GetAddict(playerid), HOSP_REHAB_PRICE);
	strcat(list, line);
	ShowPlayerDialog(playerid, DIALOG_HOSPITAL, DIALOG_STYLE_TABLIST_HEADERS, "Регистратура больницы", "%s", "Выбрать", "Закрыть", list);
}

static bool:Hospital_Pay(playerid, price, const reason[])
{
	if (GetMoney(playerid) < price) { SendClientMessage(playerid, COLOR_GREY, "Не хватает денег. Нужно $%d.", price); return false; }
	GiveMoney(playerid, -price, reason);
	Hospital_Income(price);
	Account_Save(playerid);
	return true;
}

static Hospital_OnPick(playerid, listitem)
{
	if (Place_Nearby(playerid, PL_CLINIC, 3.5) == -1) return;
	new now = gettime();
	switch (listitem)
	{
		case 0:
		{
			new Float:hp;
			GetPlayerHealth(playerid, hp);
			if (hp >= 100.0) { SendClientMessage(playerid, COLOR_GREY, "Ты здоров."); return; }
			if (!Hospital_Pay(playerid, Hospital_IsInsured(playerid) ? HOSP_TREAT_INSURED : HOSP_TREAT_PRICE, "лечение в больнице")) return;
			SetPlayerHealth(playerid, 100.0);
			SendClientMessage(playerid, COLOR_GREEN, "Врач осмотрел тебя и обработал раны. Здоровье: 100.");
		}
		case 1:
		{
			if (!Hospital_Pay(playerid, HOSP_INSURE_PRICE, "медицинская страховка")) return;
			gInsured[playerid] = ((gInsured[playerid] > now) ? gInsured[playerid] : now) + HOSP_INSURE_DAYS * 86400;
			Hospital_Save(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Страховка оформлена: ещё %d дн.", Hospital_DaysLeft(gInsured[playerid]));
		}
		case 2:
		{
			if (Drugs_GetAddict(playerid) >= ADDICT_WITHDRAW) { SendClientMessage(playerid, COLOR_RED, "Врач: «Не годен. Сначала вылечи зависимость»."); return; }
			if (!Hospital_Pay(playerid, HOSP_MEDCARD_PRICE, "медкарта")) return;
			gMedcard[playerid] = ((gMedcard[playerid] > now) ? gMedcard[playerid] : now) + HOSP_MEDCARD_DAYS * 86400;
			Hospital_Save(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Медосмотр пройден, медкарта действует ещё %d дн.", Hospital_DaysLeft(gMedcard[playerid]));
		}
		case 3:
		{
			if (Drugs_GetAddict(playerid) <= 0) { SendClientMessage(playerid, COLOR_GREY, "Зависимости нет."); return; }
			if (!Hospital_Pay(playerid, HOSP_REHAB_PRICE, "лечение зависимости")) return;
			Drugs_Cure(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Курс капельниц пройден: зависимость снята.");
		}
	}
}

// ---------------------------------------------------------------------
//  Команды
// ---------------------------------------------------------------------
stock bool:Cmd_Medcard(playerid)
{
	if (Hospital_HasMedcard(playerid)) SendClientMessage(playerid, COLOR_WHITE, "Медкарта: действует ещё %d дн.", Hospital_DaysLeft(gMedcard[playerid]));
	else SendClientMessage(playerid, COLOR_GREY, "Медкарты нет. Её выдают в регистратуре больницы или медики.");
	if (Hospital_IsInsured(playerid)) SendClientMessage(playerid, COLOR_WHITE, "Страховка: ещё %d дн.", Hospital_DaysLeft(gInsured[playerid]));
	else SendClientMessage(playerid, COLOR_GREY, "Страховки нет.");
	return true;
}

stock bool:Cmd_ShowMedcard(playerid, const params[])
{
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /showmedcard [id]");
	if (!IsPlayerNearPlayer(playerid, target, 4.0)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе.");
	if (!Hospital_HasMedcard(playerid)) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет медкарты.");
	SendClientMessage(target, COLOR_WHITE, "%s показывает медкарту: годен, действует ещё %d дн.", GetRPName(playerid), Hospital_DaysLeft(gMedcard[playerid]));
	new msg[96];
	format(msg, sizeof msg, "%s показывает медкарту %s.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	return true;
}

stock bool:Cmd_GiveMedcard(playerid, const params[])
{
	if (!Faction_IsMedic(Player[playerid][pFaction]) || !Player[playerid][pDuty]) return SendClientMessage(playerid, COLOR_GREY, "Медкарту выписывает медик на смене.");
	if (Player[playerid][pRank] < 3) return SendClientMessage(playerid, COLOR_GREY, "Медкарты выписывают с ранга 3.");
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || target == playerid) return SendClientMessage(playerid, COLOR_GREY, "Использование: /givemedcard [id]  (пациент платит $%d)", HOSP_MEDIC_MEDCARD);
	if (!IsPlayerNearPlayer(playerid, target, 3.0)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе.");
	if (Drugs_GetAddict(target) >= ADDICT_WITHDRAW) return SendClientMessage(playerid, COLOR_ORANGE, "Анализы показывают наркотики: пациент не годен.");
	if (GetMoney(target) < HOSP_MEDIC_MEDCARD) return SendClientMessage(playerid, COLOR_GREY, "У пациента не хватает денег ($%d).", HOSP_MEDIC_MEDCARD);
	GiveMoney(target, -HOSP_MEDIC_MEDCARD, "медкарта у медика");
	GiveMoney(playerid, HOSP_MEDIC_MEDCARD, "выписал медкарту");
	new now = gettime();
	gMedcard[target] = ((gMedcard[target] > now) ? gMedcard[target] : now) + HOSP_MEDCARD_DAYS * 86400;
	Hospital_Save(target);
	Account_Save(target);
	Account_Save(playerid);
	new msg[128];
	format(msg, sizeof msg, "%s проводит медосмотр %s и выписывает медкарту.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(target, COLOR_GREEN, "Медкарта выдана на %d дн. (-$%d).", HOSP_MEDCARD_DAYS, HOSP_MEDIC_MEDCARD);
	return true;
}

stock bool:Hospital_TryAlt(playerid)
{
	if (Place_Nearby(playerid, PL_CLINIC, 2.5) == -1) return false;
	Hospital_ShowDesk(playerid);
	return true;
}

stock Hospital_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid != DIALOG_HOSPITAL) return 0;
	if (response) Hospital_OnPick(playerid, listitem);
	return 1;
}

stock bool:Hospital_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/medcard", true))     { Cmd_Medcard(playerid); return true; }
	if (!strcmp(cmd, "/showmedcard", true)) { Cmd_ShowMedcard(playerid, params); return true; }
	if (!strcmp(cmd, "/givemedcard", true)) { Cmd_GiveMedcard(playerid, params); return true; }
	return false;
}
