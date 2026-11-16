// =====================================================================
//  vip.inc — VIP с уровнями: Bronze, Silver, Gold, Platinum.
//
//  Подключается из donate.inc. VIP покупается за донат-монеты в /donate
//  (или выдаётся админом: /setvip). Все цифры — в таблице ниже:
//  столбцы идут по уровням 0 (нет VIP), 1 Bronze, 2 Silver, 3 Gold, 4 Platinum.
//
//  Для других модулей:
//    Vip_GetLevel(playerid)      — уровень VIP игрока: 0 (нет) .. 4
//    Vip_HouseBonus(playerid)    — сколько домов сверх обычного лимита
//    Vip_CarBonus(playerid)      — сколько машин сверх обычного лимита
//    Vip_Discount(playerid)      — скидка в магазинах и на услуги, %
//    Vip_Price(playerid, price)  — цена со скидкой VIP
//    Vip_HospitalFee(playerid, fee) — счёт больницы со скидкой VIP
//    Vip_Grant(playerid, level, days) — выдать или продлить VIP
//
//  Игрок:
//    /vip       — статус, привилегии уровней, покупка
//    /vc [текст]— чат VIP
//    /vipbonus  — ежедневный бонус
//    /vipfeed   — поесть (сытость 100)              Silver+
//    /vipfix    — починить свою машину (стоя)       Silver+
//    /viptag    — надпись VIP над головой вкл/выкл  Silver+
//    /vipfuel   — полный бак (стоя)                 Gold+
//    /vipad     — объявление на весь сервер         Gold+
//  Админ 6+: /setvip [id] [уровень 0-4] [дней]
// =====================================================================

#define VIP_MAX             4
#define VIP_DAYS            30          // на сколько дней покупается VIP
#define VIP_TAG_LEVEL       2           // с какого уровня надпись над головой
#define VIP_JOIN_LEVEL      4           // с какого уровня объявление о входе на сервер
#define VIP_DAILY_HOURS     20          // раз во столько часов даётся /vipbonus
#define VIP_TAG_RANGE       15.0

//                                             нет  Bronze  Silver  Gold    Platinum
static const gVipName[VIP_MAX + 1][] =       { "нет", "Bronze", "Silver", "Gold", "Platinum" };
static const gVipHex[VIP_MAX + 1][] =        { "AFAFAF", "CD7F32", "C8D0D8", "FFD700", "7FE8FF" };
static const gVipColor[VIP_MAX + 1] =        { 0xAFAFAFFF, 0xCD7F32FF, 0xC8D0D8FF, 0xFFD700FF, 0x7FE8FFFF };
static const gVipPrice[VIP_MAX + 1] =        { 0,   150,    300,    600,    1200 };  // монет за VIP_DAYS дней
static const gVipPayday[VIP_MAX + 1] =       { 0,   1000,   2000,   3500,   5000 };  // прибавка $ к каждому PayDay
static const gVipExp[VIP_MAX + 1] =          { 0,   0,      1,      1,      2 };     // доп. опыт за PayDay (1 = опыт x2, 2 = x3)
static const gVipDaily[VIP_MAX + 1] =        { 0,   2000,   5000,   10000,  20000 }; // /vipbonus, $
static const gVipDailyCoins[VIP_MAX + 1] =   { 0,   0,      0,      1,      3 };     // /vipbonus, донат-монет
static const gVipHospital[VIP_MAX + 1] =     { 0,   25,     50,     100,    100 };   // скидка на счёт больницы, %
static const gVipHunger[VIP_MAX + 1] =       { 0,   0,      25,     50,     50 };    // голод медленнее на столько %
static const gVipDiscount[VIP_MAX + 1] =     { 0,   0,      5,      10,     15 };    // скидка в магазинах, % (Vip_Discount)
static const gVipHouses[VIP_MAX + 1] =       { 0,   0,      1,      2,      3 };     // доп. дома (Vip_HouseBonus)
static const gVipCars[VIP_MAX + 1] =         { 0,   1,      1,      2,      3 };     // доп. машины (Vip_CarBonus)
// Перезарядка команд в минутах; 0 — команда на этом уровне недоступна
static const gVipFeedCd[VIP_MAX + 1] =       { 0,   0,      60,     45,     30 };
static const gVipFixCd[VIP_MAX + 1] =        { 0,   0,      30,     20,     10 };
static const gVipFuelCd[VIP_MAX + 1] =       { 0,   0,      0,      45,     30 };
static const gVipAdCd[VIP_MAX + 1] =         { 0,   0,      0,      60,     30 };

enum
{
	VCD_DAILY,
	VCD_FEED,
	VCD_FIX,
	VCD_FUEL,
	VCD_AD,
	VCD_COUNT
}

static gVipLevel[MAX_PLAYERS];
static gVipUntil[MAX_PLAYERS];
static gVipWas[MAX_PLAYERS];                    // уровень на прошлой секунде — чтобы сказать «VIP закончился»
static gVipCd[MAX_PLAYERS][VCD_COUNT];          // когда команда снова доступна (unix-время)
static bool:gVipTagOff[MAX_PLAYERS];
static Text3D:gVipTag[MAX_PLAYERS] = { Text3D:INVALID_3DTEXT_ID, ... };
static gVipTagLevel[MAX_PLAYERS];
static gVipTagWorld[MAX_PLAYERS];

forward Vip_Tick();

stock Vip_Init()
{
	DB_AddColumn("accounts", "vip_until", "INTEGER DEFAULT 0");
	DB_AddColumn("accounts", "vip_level", "INTEGER DEFAULT 0");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS vip_cd (account INTEGER NOT NULL, kind INTEGER NOT NULL, until INTEGER DEFAULT 0, PRIMARY KEY (account, kind))"));
	SetTimer("Vip_Tick", 1000, true);
}

// ---------------------------------------------------------------------
//  Для других модулей
stock Vip_GetLevel(playerid)
{
	if (gVipUntil[playerid] <= gettime()) return 0;
	return gVipLevel[playerid];
}

stock Vip_DaysLeft(playerid)
{
	new left = gVipUntil[playerid] - gettime();
	return (left > 0) ? (left / 86400 + 1) : 0;
}

stock Vip_HouseBonus(playerid)  { return gVipHouses[Vip_GetLevel(playerid)]; }
stock Vip_CarBonus(playerid)    { return gVipCars[Vip_GetLevel(playerid)]; }
stock Vip_Discount(playerid)    { return gVipDiscount[Vip_GetLevel(playerid)]; }
stock Vip_Price(playerid, price) { return price - price * Vip_Discount(playerid) / 100; }
stock Vip_HospitalFee(playerid, fee) { return fee - fee * gVipHospital[Vip_GetLevel(playerid)] / 100; }

// needs.inc: 1 — в этот раз сытость не убавлять
stock Vip_SkipHunger(playerid)
{
	new pct = gVipHunger[Vip_GetLevel(playerid)];
	return (pct > 0 && random(100) < pct);
}

// Данные уровня для магазина (donate.inc)
stock Vip_LevelPrice(level) { return (level >= 1 && level <= VIP_MAX) ? gVipPrice[level] : 0; }
stock Vip_LevelName(level)
{
	new name[12];
	if (level >= 0 && level <= VIP_MAX) strcat(name, gVipName[level]);
	return name;
}

// JSON-массив уровней для HTML-окна доната
stock Vip_Json(dest[], size = sizeof dest)
{
	new part[256];
	dest[0] = '\0';
	for (new l = 1; l <= VIP_MAX; l++)
	{
		format(part, sizeof part, "%s{\"n\":\"%s\",\"c\":\"%s\",\"p\":%d,\"pay\":%d,\"exp\":%d,\"day\":%d,\"dc\":%d,\"hosp\":%d,\"hun\":%d,\"disc\":%d,\"h\":%d,\"car\":%d,\"feed\":%d,\"fix\":%d,\"fuel\":%d,\"ad\":%d,\"tag\":%d,\"join\":%d}",
			(l > 1) ? "," : "", gVipName[l], gVipHex[l], gVipPrice[l], gVipPayday[l], gVipExp[l] + 1, gVipDaily[l], gVipDailyCoins[l],
			gVipHospital[l], gVipHunger[l], gVipDiscount[l], gVipHouses[l], gVipCars[l], gVipFeedCd[l], gVipFixCd[l], gVipFuelCd[l], gVipAdCd[l], _:(l >= VIP_TAG_LEVEL), _:(l >= VIP_JOIN_LEVEL));
		strcat(dest, part, size);
	}
}

static Vip_Save(playerid)
{
	if (Player[playerid][pDbId] == 0) return;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE accounts SET vip_level = %d, vip_until = %d WHERE id = %d", gVipLevel[playerid], gVipUntil[playerid], Player[playerid][pDbId]));
}

// Выдать VIP. Тот же уровень — продлевается. Уровень выше — остаток старого
// пересчитывается в дни нового по цене. Уровень ниже действующего — дни
// пересчитываются в дни действующего, уровень не падает.
stock Vip_Grant(playerid, level, days)
{
	if (level < 1 || level > VIP_MAX || days < 1) return;
	new now = gettime(), cur = Vip_GetLevel(playerid), add = days * 86400;
	if (cur == 0)
	{
		gVipLevel[playerid] = level;
		gVipUntil[playerid] = now + add;
	}
	else if (cur == level) gVipUntil[playerid] += add;
	else if (level > cur)
	{
		new left = floatround(float(gVipUntil[playerid] - now) * float(gVipPrice[cur]) / float(gVipPrice[level]));
		gVipLevel[playerid] = level;
		gVipUntil[playerid] = now + left + add;
	}
	else
	{
		new conv = floatround(float(add) * float(gVipPrice[level]) / float(gVipPrice[cur]));
		if (conv < 3600) conv = 3600;
		gVipUntil[playerid] += conv;
	}
	gVipWas[playerid] = gVipLevel[playerid];
	Vip_Save(playerid);
}

stock Vip_Remove(playerid)
{
	gVipLevel[playerid] = 0;
	gVipUntil[playerid] = 0;
	gVipWas[playerid] = 0;
	Vip_Save(playerid);
}

// ---------------------------------------------------------------------
stock Vip_LoadPlayer(playerid)
{
	gVipLevel[playerid] = 0;
	gVipUntil[playerid] = 0;
	gVipTagOff[playerid] = false;
	for (new k = 0; k < VCD_COUNT; k++) gVipCd[playerid][k] = 0;

	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT vip_level, vip_until FROM accounts WHERE id = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			gVipLevel[playerid] = DB_GetFieldInt(r, 0);
			gVipUntil[playerid] = DB_GetFieldInt(r, 1);
		}
		DB_FreeResultSet(r);
	}
	// VIP, купленный до появления уровней, — это Silver (та же цена и бонусы)
	if (gVipUntil[playerid] > gettime() && (gVipLevel[playerid] < 1 || gVipLevel[playerid] > VIP_MAX))
	{
		gVipLevel[playerid] = 2;
		Vip_Save(playerid);
	}

	r = DB_ExecuteQuery(gDB, "SELECT kind, until FROM vip_cd WHERE account = %d", Player[playerid][pDbId]);
	if (r != DBResult:0)
	{
		for (new i = 0, n = DB_GetRowCount(r); i < n; i++)
		{
			new k = DB_GetFieldInt(r, 0);
			if (k >= 0 && k < VCD_COUNT) gVipCd[playerid][k] = DB_GetFieldInt(r, 1);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}

	new level = Vip_GetLevel(playerid);
	gVipWas[playerid] = level;
	if (level == 0) return;
	SendClientMessage(playerid, gVipColor[level], "VIP %s активен ещё %d дн. Меню: /vip%s", gVipName[level], Vip_DaysLeft(playerid),
		(gVipCd[playerid][VCD_DAILY] <= gettime()) ? ", ежедневный бонус: /vipbonus" : "");
	if (level >= VIP_JOIN_LEVEL)
	{
		new msg[96];
		format(msg, sizeof msg, "{%s}[VIP %s] {FFFFFF}%s зашёл на сервер.", gVipHex[level], gVipName[level], GetRPName(playerid));
		SendClientMessageToAll(COLOR_WHITE, msg);
	}
}

// Из PayDay (donate.inc: Donate_PayDay)
stock Vip_PayDay(playerid)
{
	new level = Vip_GetLevel(playerid);
	if (level == 0) return;
	Bank_Salary(playerid, gVipPayday[level]);
	Player[playerid][pExp] += gVipExp[level];
	if (gVipExp[level] > 0) SendClientMessage(playerid, gVipColor[level], "VIP %s: прибавка $%d и опыт x%d.", gVipName[level], gVipPayday[level], gVipExp[level] + 1);
	else SendClientMessage(playerid, gVipColor[level], "VIP %s: прибавка $%d.", gVipName[level], gVipPayday[level]);
}

// ---------------------------------------------------------------------
//  Надпись над головой и конец срока — раз в секунду
static Vip_TagDestroy(playerid)
{
	if (gVipTag[playerid] == Text3D:INVALID_3DTEXT_ID) return;
	Delete3DTextLabel(gVipTag[playerid]);
	gVipTag[playerid] = Text3D:INVALID_3DTEXT_ID;
}

public Vip_Tick()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged])
		{
			Vip_TagDestroy(i);
			gVipWas[i] = 0;
			continue;
		}
		new level = Vip_GetLevel(i);
		if (gVipWas[i] > 0 && level == 0)
			SendClientMessage(i, COLOR_ORANGE, "Срок VIP %s закончился. Продлить: /donate", gVipName[gVipWas[i]]);
		gVipWas[i] = level;

		new bool:show = (level >= VIP_TAG_LEVEL && !gVipTagOff[i] && !Inv_IsMasked(i)
			&& GetPlayerState(i) != PLAYER_STATE_SPECTATING && GetPlayerState(i) != PLAYER_STATE_WASTED);
		new world = GetPlayerVirtualWorld(i);
		if (gVipTag[i] != Text3D:INVALID_3DTEXT_ID && (!show || gVipTagLevel[i] != level || gVipTagWorld[i] != world)) Vip_TagDestroy(i);
		if (show && gVipTag[i] == Text3D:INVALID_3DTEXT_ID)
		{
			new text[24];
			format(text, sizeof text, "VIP %s", gVipName[level]);
			gVipTag[i] = Create3DTextLabel(text, gVipColor[level], 0.0, 0.0, 0.0, VIP_TAG_RANGE, world, true);
			Attach3DTextLabelToPlayer(gVipTag[i], i, 0.0, 0.0, 0.55);
			gVipTagLevel[i] = level;
			gVipTagWorld[i] = world;
		}
	}
	return 1;
}

// ---------------------------------------------------------------------
//  Перезарядки (хранятся в базе, чтобы перезаход их не сбрасывал)
static Vip_SetCd(playerid, kind, seconds)
{
	gVipCd[playerid][kind] = gettime() + seconds;
	if (Player[playerid][pDbId] != 0)
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO vip_cd (account, kind, until) VALUES (%d, %d, %d)", Player[playerid][pDbId], kind, gVipCd[playerid][kind]));
}

// Можно ли пользоваться командой: уровень, перезарядка, состояние игрока
static bool:Vip_CanUse(playerid, kind, const table[], const what[])
{
	new level = Vip_GetLevel(playerid);
	if (level == 0) { SendClientMessage(playerid, COLOR_GREY, "Это привилегия VIP. Купить: /donate"); return false; }
	if (table[level] == 0)
	{
		new need = level;
		while (need <= VIP_MAX && table[need] == 0) need++;
		SendClientMessage(playerid, COLOR_GREY, "%s доступно с VIP %s. У тебя %s. Улучшить: /donate", what, gVipName[(need <= VIP_MAX) ? need : VIP_MAX], gVipName[level]);
		return false;
	}
	if (Wound_IsWounded(playerid) || Player[playerid][pCuffed] || Player[playerid][pJail] > 0 || Prison_IsInmate(playerid) || Admin_IsInAJail(playerid))
	{
		SendClientMessage(playerid, COLOR_GREY, "Сейчас это недоступно.");
		return false;
	}
	new left = gVipCd[playerid][kind] - gettime();
	if (left > 0)
	{
		SendClientMessage(playerid, COLOR_GREY, "%s: снова через %s.", what, Admin_FormatTime(left));
		return false;
	}
	return true;
}

// Своя машина за рулём и стоит на месте — для /vipfix и /vipfuel
static Vip_StoppedVehicle(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_DRIVER)
	{
		SendClientMessage(playerid, COLOR_GREY, "Нужно сидеть за рулём.");
		return INVALID_VEHICLE_ID;
	}
	if (Player[playerid][pWanted] > 0)
	{
		SendClientMessage(playerid, COLOR_GREY, "В розыске это недоступно.");
		return INVALID_VEHICLE_ID;
	}
	new v = GetPlayerVehicleID(playerid);
	if (Fuel_GetSpeed(v) > 2)
	{
		SendClientMessage(playerid, COLOR_GREY, "Сначала остановись.");
		return INVALID_VEHICLE_ID;
	}
	return v;
}

stock bool:Cmd_VipBonus(playerid)
{
	new level = Vip_GetLevel(playerid);
	if (level == 0) return SendClientMessage(playerid, COLOR_GREY, "Ежедневный бонус — привилегия VIP. Купить: /donate");
	new left = gVipCd[playerid][VCD_DAILY] - gettime();
	if (left > 0) return SendClientMessage(playerid, COLOR_GREY, "Бонус уже получен. Следующий через %s.", Admin_FormatTime(left));
	Vip_SetCd(playerid, VCD_DAILY, VIP_DAILY_HOURS * 3600);
	GiveMoney(playerid, gVipDaily[level], "VIP: ежедневный бонус");
	if (gVipDailyCoins[level] > 0)
	{
		Donate_AddCoins(playerid, gVipDailyCoins[level]);
		SendClientMessage(playerid, gVipColor[level], "Ежедневный бонус VIP %s: $%d и %d донат-монет.", gVipName[level], gVipDaily[level], gVipDailyCoins[level]);
	}
	else SendClientMessage(playerid, gVipColor[level], "Ежедневный бонус VIP %s: $%d.", gVipName[level], gVipDaily[level]);
	Account_Save(playerid);
	return true;
}

stock bool:Cmd_VipFeed(playerid)
{
	if (!Vip_CanUse(playerid, VCD_FEED, gVipFeedCd, "Еда VIP")) return true;
	if (Needs_GetHunger(playerid) >= 100) return SendClientMessage(playerid, COLOR_GREY, "Ты не голоден.");
	Vip_SetCd(playerid, VCD_FEED, gVipFeedCd[Vip_GetLevel(playerid)] * 60);
	Needs_Feed(playerid, 100);
	SendClientMessage(playerid, COLOR_GREEN, "Ты перекусил. Сытость: 100%%.");
	return true;
}

stock bool:Cmd_VipFix(playerid)
{
	if (!Vip_CanUse(playerid, VCD_FIX, gVipFixCd, "Починка VIP")) return true;
	new v = Vip_StoppedVehicle(playerid);
	if (v == INVALID_VEHICLE_ID) return true;
	Vip_SetCd(playerid, VCD_FIX, gVipFixCd[Vip_GetLevel(playerid)] * 60);
	RepairVehicle(v);
	SendClientMessage(playerid, COLOR_GREEN, "Машина починена.");
	return true;
}

stock bool:Cmd_VipFuel(playerid)
{
	if (!Vip_CanUse(playerid, VCD_FUEL, gVipFuelCd, "Заправка VIP")) return true;
	new v = Vip_StoppedVehicle(playerid);
	if (v == INVALID_VEHICLE_ID) return true;
	Vip_SetCd(playerid, VCD_FUEL, gVipFuelCd[Vip_GetLevel(playerid)] * 60);
	Fuel_Set(v, float(FUEL_TANK));
	SendClientMessage(playerid, COLOR_GREEN, "Бак заправлен полностью.");
	return true;
}

stock bool:Cmd_VipAd(playerid, const params[])
{
	new text[100];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /vipad [текст объявления]");
	if (!Vip_CanUse(playerid, VCD_AD, gVipAdCd, "Объявление VIP")) return true;
	if (Admin_IsMuted(playerid)) return true;
	new level = Vip_GetLevel(playerid), msg[144];
	Vip_SetCd(playerid, VCD_AD, gVipAdCd[level] * 60);
	if (Player[playerid][pPhone] != 0) format(msg, sizeof msg, "{%s}[Объявление VIP] {FFFFFF}%s. Отправил: %s, тел. %d", gVipHex[level], text, GetRPName(playerid), Player[playerid][pPhone]);
	else format(msg, sizeof msg, "{%s}[Объявление VIP] {FFFFFF}%s. Отправил: %s", gVipHex[level], text, GetRPName(playerid));
	SendClientMessageToAll(COLOR_WHITE, msg);
	return true;
}

stock bool:Cmd_VipChat(playerid, const params[])
{
	new level = Vip_GetLevel(playerid);
	if (level == 0) return SendClientMessage(playerid, COLOR_GREY, "Чат VIP доступен только с VIP. Купить: /donate");
	new text[100], msg[144];
	if (!RestParam(params, 0, text)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /vc [текст]");
	if (Admin_IsMuted(playerid)) return true;
	format(msg, sizeof msg, "{%s}[VIP %s] %s [%d]: {FFFFFF}%s", gVipHex[level], gVipName[level], GetRPName(playerid), playerid, text);
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pLogged] && Vip_GetLevel(i) > 0) SendClientMessage(i, COLOR_WHITE, msg);
	}
	return true;
}

stock bool:Cmd_VipTag(playerid)
{
	new level = Vip_GetLevel(playerid);
	if (level < VIP_TAG_LEVEL) return SendClientMessage(playerid, COLOR_GREY, "Надпись над головой доступна с VIP %s.", gVipName[VIP_TAG_LEVEL]);
	gVipTagOff[playerid] = !gVipTagOff[playerid];
	SendClientMessage(playerid, gVipColor[level], gVipTagOff[playerid] ? "Надпись VIP над головой выключена." : "Надпись VIP над головой включена.");
	return true;
}

stock bool:Cmd_SetVip(playerid, const params[])
{
	if (!Admin_Check(playerid, 6)) return true;
	new pos = 0, level, days;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID || !NextIntParam(params, pos, level) || level < 0 || level > VIP_MAX)
		return SendClientMessage(playerid, COLOR_GREY, "Использование: /setvip [id] [уровень: 0 снять, 1 Bronze, 2 Silver, 3 Gold, 4 Platinum] [дней]");
	if (!Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Игрок ещё не вошёл в аккаунт.");
	if (level == 0)
	{
		Vip_Remove(target);
		Admin_Log(playerid, "setvip", target, "снят", 0);
		SendClientMessage(target, COLOR_ADMIN, "Администратор снял твой VIP.");
		SendClientMessage(playerid, COLOR_ADMIN, "VIP у %s снят.", GetRPName(target));
		return true;
	}
	if (!NextIntParam(params, pos, days) || days < 1 || days > 3650)
		return SendClientMessage(playerid, COLOR_GREY, "Укажи срок в днях: /setvip [id] [уровень] [дней 1-3650]");
	// Админ задаёт уровень и срок ровно, без пересчёта остатка
	gVipLevel[target] = level;
	gVipUntil[target] = gettime() + days * 86400;
	gVipWas[target] = level;
	Vip_Save(target);
	Admin_Log(playerid, "setvip", target, gVipName[level], days);
	SendClientMessage(target, gVipColor[level], "Администратор выдал тебе VIP %s на %d дн. Меню: /vip", gVipName[level], days);
	SendClientMessage(playerid, COLOR_ADMIN, "%s: VIP %s на %d дн.", GetRPName(target), gVipName[level], days);
	return true;
}

// ---------------------------------------------------------------------
//  /vip — обычные окна (на клиенте Bastion открывается страница VIP в окне доната)
static Vip_CdText(minutes)
{
	new s[16];
	if (minutes == 0) strcat(s, "{777777}—");
	else format(s, sizeof s, "раз в %d мин", minutes);
	return s;
}

// Строка таблицы привилегий: название и значение на каждом уровне
static Vip_Row(text[], size, const name[], const fmt[], const table[])
{
	new cell[24];
	strcat(text, "{FFFFFF}", size);
	strcat(text, name, size);
	for (new l = 1; l <= VIP_MAX; l++)
	{
		format(cell, sizeof cell, fmt, table[l]);
		strcat(text, "\t{FFFFFF}", size);
		strcat(text, cell, size);
	}
	strcat(text, "\n", size);
}

static Vip_RowCd(text[], size, const name[], const table[])
{
	strcat(text, "{FFFFFF}", size);
	strcat(text, name, size);
	for (new l = 1; l <= VIP_MAX; l++)
	{
		strcat(text, "\t{FFFFFF}", size);
		strcat(text, Vip_CdText(table[l]), size);
	}
	strcat(text, "\n", size);
}

stock Vip_ShowPerks(playerid)
{
	new text[1900], line[48], exp[VIP_MAX + 1];
	strcat(text, "{FFFFFF}Привилегия");
	for (new l = 1; l <= VIP_MAX; l++)
	{
		format(line, sizeof line, "\t{%s}%s", gVipHex[l], gVipName[l]);
		strcat(text, line);
		exp[l] = gVipExp[l] + 1;
	}
	strcat(text, "\n");
	Vip_Row(text, sizeof text, "Цена, монет", "%d", gVipPrice);
	Vip_Row(text, sizeof text, "К зарплате", "+$%d", gVipPayday);
	Vip_Row(text, sizeof text, "Опыт", "x%d", exp);
	Vip_Row(text, sizeof text, "/vipbonus в день", "$%d", gVipDaily);
	Vip_Row(text, sizeof text, "Скидка в больнице", "%d%%", gVipHospital);
	Vip_Row(text, sizeof text, "Скидка в магазинах", "%d%%", gVipDiscount);
	Vip_Row(text, sizeof text, "Голод медленнее", "%d%%", gVipHunger);
	Vip_Row(text, sizeof text, "Доп. дома", "+%d", gVipHouses);
	Vip_Row(text, sizeof text, "Доп. машины", "+%d", gVipCars);
	Vip_RowCd(text, sizeof text, "/vipfeed (еда)", gVipFeedCd);
	Vip_RowCd(text, sizeof text, "/vipfix (починка)", gVipFixCd);
	Vip_RowCd(text, sizeof text, "/vipfuel (бак)", gVipFuelCd);
	Vip_RowCd(text, sizeof text, "/vipad (объявление)", gVipAdCd);
	strcat(text, "{FFFFFF}Чат VIP /vc");
	for (new l = 1; l <= VIP_MAX; l++) strcat(text, "\t{33AA33}да");
	strcat(text, "\n{FFFFFF}Надпись над головой");
	for (new l = 1, tagFrom = VIP_TAG_LEVEL; l <= VIP_MAX; l++) strcat(text, (l >= tagFrom) ? "\t{33AA33}да" : "\t{777777}—");
	strcat(text, "\n");
	ShowPlayerDialog(playerid, DIALOG_VIP_PERKS, DIALOG_STYLE_TABLIST_HEADERS, "Привилегии VIP на 30 дней", "%s", "Купить", "Назад", text);
}

stock Vip_ShowMenu(playerid)
{
	new level = Vip_GetLevel(playerid), title[64], list[256], line[96];
	if (level > 0) format(title, sizeof title, "{%s}VIP %s{FFFFFF}: ещё %d дн.", gVipHex[level], gVipName[level], Vip_DaysLeft(playerid));
	else strcat(title, "VIP: не активен");
	strcat(list, "Привилегии уровней\n");
	strcat(list, (level > 0) ? "Продлить или улучшить VIP\n" : "Купить VIP\n");
	if (level > 0)
	{
		new left = gVipCd[playerid][VCD_DAILY] - gettime();
		if (left > 0) format(line, sizeof line, "{AFAFAF}Ежедневный бонус (через %s)\n", Admin_FormatTime(left));
		else format(line, sizeof line, "{33AA33}Забрать ежедневный бонус: $%d\n", gVipDaily[level]);
		strcat(list, line);
		strcat(list, "Команды VIP\n");
	}
	ShowPlayerDialog(playerid, DIALOG_VIP, DIALOG_STYLE_LIST, title, "%s", "Выбрать", "Закрыть", list);
}

stock Vip_ShowCommands(playerid)
{
	ShowPlayerDialog(playerid, DIALOG_VIP_INFO, DIALOG_STYLE_MSGBOX, "Команды VIP",
		"{FFD700}/vc [текст]{FFFFFF} — чат VIP\n{FFD700}/vipbonus{FFFFFF} — ежедневный бонус\n{FFD700}/vipfeed{FFFFFF} — поесть (Silver и выше)\n{FFD700}/vipfix{FFFFFF} — починить машину, стоя на месте (Silver и выше)\n{FFD700}/viptag{FFFFFF} — надпись VIP над головой (Silver и выше)\n{FFD700}/vipfuel{FFFFFF} — полный бак, стоя на месте (Gold и выше)\n{FFD700}/vipad [текст]{FFFFFF} — объявление на весь сервер (Gold и выше)",
		"Назад", "");
}

stock Vip_OnDialog(playerid, dialogid, response, listitem)
{
	switch (dialogid)
	{
		case DIALOG_VIP:
		{
			if (!response) return 1;
			switch (listitem)
			{
				case 0: Vip_ShowPerks(playerid);
				case 1: Donate_ShowVip(playerid);
				case 2: { Cmd_VipBonus(playerid); Vip_ShowMenu(playerid); }
				case 3: Vip_ShowCommands(playerid);
			}
			return 1;
		}
		case DIALOG_VIP_PERKS:
		{
			if (response) Donate_ShowVip(playerid);
			else Vip_ShowMenu(playerid);
			return 1;
		}
		case DIALOG_VIP_INFO:
		{
			Vip_ShowMenu(playerid);
			return 1;
		}
	}
	return 0;
}

stock bool:Vip_OnCommand(playerid, const cmd[], const params[])
{
	if (!strcmp(cmd, "/vc", true))       { Cmd_VipChat(playerid, params); return true; }
	if (!strcmp(cmd, "/vipbonus", true)) { Cmd_VipBonus(playerid); return true; }
	if (!strcmp(cmd, "/vipfeed", true))  { Cmd_VipFeed(playerid); return true; }
	if (!strcmp(cmd, "/vipfix", true))   { Cmd_VipFix(playerid); return true; }
	if (!strcmp(cmd, "/vipfuel", true))  { Cmd_VipFuel(playerid); return true; }
	if (!strcmp(cmd, "/vipad", true))    { Cmd_VipAd(playerid, params); return true; }
	if (!strcmp(cmd, "/viptag", true))   { Cmd_VipTag(playerid); return true; }
	if (!strcmp(cmd, "/setvip", true))   { Cmd_SetVip(playerid, params); return true; }
	return false;
}
