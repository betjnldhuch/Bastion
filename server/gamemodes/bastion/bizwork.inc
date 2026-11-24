// =====================================================================
//  bizwork.inc — сотрудники бизнеса, налог и улучшения.
//
//  Сотрудники (меню бизнеса > «Сотрудники»):
//    заместитель — открывает меню бизнеса (/bizmenu): склад, цены, продавцы,
//      история, выписка; нанимает продавцов и выдаёт им премии.
//      Касса, название, улучшения и продажа бизнеса — только у владельца.
//    продавец — продаёт товар покупателю рядом: /sell [id]. Цену ставит сам,
//      но не ниже минимальной, которую задал владелец («Продавцы: цены и процент»).
//      С каждой продажи продавец получает процент владельца (0 — без процента,
//      тогда платят премиями: «Сотрудники» > сотрудник > «Премия», деньги из кассы).
//    Сотрудники хранятся в biz_staff вместе с владельцем, который их нанял:
//    бизнес сменил владельца — старые сотрудники больше не работают.
//
//  Налог: каждый PayDay BIZ_TAX_PERMILLE‰ от цены бизнеса из кассы. Не хватило —
//  остаток в долг. Долг не гасится BIZ_TAX_CLOSE_PD PayDay подряд — бизнес закрыт
//  (не торгует, не приносит доход), BIZ_TAX_SEIZE_PD — штат забирает бизнес.
//  Погасить: меню бизнеса > «Налог».
//
//  Улучшения (только владелец, остаются у бизнеса при продаже):
//    рекламное агентство — доход за PayDay +UPG_ADS_PCT%;
//    налоговый консультант — налог вдвое ниже;
//    большой склад — склад вдвое больше.
// =====================================================================

forward bool:BizW_HasUpg(b, bit);
forward bool:BizW_Closed(b);
forward bool:BizX_UsesStock(b);

#define BW_SELLER           1
#define BW_DEPUTY           2
#define BIZW_MAX_SELLERS    6
#define BIZW_MAX_DEPUTIES   2
#define BIZW_COMM_MAX       50          // процент продавцу: от 0 до
#define BIZW_SELLMIN_MIN    50          // мин. цена у продавцов: от 50% базовой
#define BIZW_SELLMIN_MAX    150         // до 150%
#define BIZW_SELL_MAX       300         // и продавец не может ставить дороже 300% базовой
#define BIZW_RANGE          4.0         // продавец и покупатель рядом
#define BIZW_BONUS_MAX      1000000

#define BIZ_TAX_PERMILLE    2           // налог за PayDay, ‰ от цены бизнеса (40 000 -> $80)
#define BIZ_TAX_CLOSE_PD    24          // долг столько PayDay подряд — бизнес закрыт
#define BIZ_TAX_SEIZE_PD    72          // а столько — штат забирает бизнес

#define UPG_ADS             1
#define UPG_TAX             2
#define UPG_STOCK           4
#define UPG_COUNT           3
#define UPG_ADS_PCT         50

static const gUpgBit[UPG_COUNT] = { UPG_ADS, UPG_TAX, UPG_STOCK };
static const gUpgPct[UPG_COUNT] = { 30, 20, 15 };          // цена, % от цены бизнеса
static const gUpgName[UPG_COUNT][32] = { "Рекламное агентство", "Налоговый консультант", "Большой склад" };
static const gUpgDesc[UPG_COUNT][40] = { "доход за PayDay +50%", "налог вдвое ниже", "склад вдвое больше" };

static gStaffAcc[MAX_PLAYERS][BIZW_MAX_SELLERS + BIZW_MAX_DEPUTIES];   // строка окна сотрудников -> аккаунт
static gStaffPick[MAX_PLAYERS];                 // выбранный сотрудник
static gHireBiz[MAX_PLAYERS], gHireRole[MAX_PLAYERS], gHireBy[MAX_PLAYERS];
static gSellList[MAX_PLAYERS][20];              // продавец: строка окна -> товар
static gSellBuyer[MAX_PLAYERS], gSellItem[MAX_PLAYERS], gSellBiz[MAX_PLAYERS];
static gSOFrom[MAX_PLAYERS], gSOItem[MAX_PLAYERS], gSOPrice[MAX_PLAYERS], gSOBiz[MAX_PLAYERS], gSOTime[MAX_PLAYERS];

stock BizW_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS biz_staff (acc INTEGER PRIMARY KEY, biz INTEGER, owner INTEGER, role INTEGER, name TEXT, sold INTEGER DEFAULT 0, earned INTEGER DEFAULT 0)"));
	for (new i = 0; i < MAX_PLAYERS; i++) { gHireBy[i] = 0; gSOFrom[i] = 0; }
}

stock BizW_OnConnect(playerid)
{
	gHireBy[playerid] = 0;
	gSOFrom[playerid] = 0;
	gSellBiz[playerid] = 0;
}

stock bool:BizW_HasUpg(b, bit) { return (Biz[b][bUpg] & bit) != 0; }
stock bool:BizW_Closed(b) { return Biz[b][bOwner] != 0 && Biz[b][bTaxPD] >= BIZ_TAX_CLOSE_PD; }
stock BizW_SellMin(b) { return (Biz[b][bSellMin] < BIZW_SELLMIN_MIN || Biz[b][bSellMin] > BIZW_SELLMIN_MAX) ? 100 : Biz[b][bSellMin]; }

stock BizW_UpgCount(b)
{
	new n = 0;
	for (new i = 0; i < UPG_COUNT; i++) if (Biz[b][bUpg] & gUpgBit[i]) n++;
	return n;
}

stock BizW_Tax(b)
{
	new tax = Biz[b][bPrice] / 1000 * BIZ_TAX_PERMILLE;
	if (BizW_HasUpg(b, UPG_TAX)) tax /= 2;
	return tax;
}

// Бизнес ушёл штату: долги, сотрудники и настройки продавцов сбрасываются
stock BizW_Reset(b)
{
	Biz[b][bTaxDebt] = 0;
	Biz[b][bTaxPD] = 0;
	Biz[b][bSellMin] = 100;
	Biz[b][bComm] = 10;
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM biz_staff WHERE biz = %d", Biz[b][bDbId]));
	BizX_Save(b);
}

// Должность игрока: BW_SELLER / BW_DEPUTY / 0, в b — его бизнес
stock BizW_RoleOf(playerid, &b)
{
	b = -1;
	if (Player[playerid][pDbId] == 0) return 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT biz, owner, role FROM biz_staff WHERE acc = %d", Player[playerid][pDbId]);
	if (r == DBResult:0) return 0;
	new role = 0;
	if (DB_GetRowCount(r) > 0)
	{
		new bb = Biz_FindByDbId(DB_GetFieldInt(r, 0));
		if (bb != -1 && Biz[bb][bOwner] != 0 && Biz[bb][bOwner] == DB_GetFieldInt(r, 1)) { b = bb; role = DB_GetFieldInt(r, 2); }
	}
	DB_FreeResultSet(r);
	return role;
}

// Сколько сотрудников (role 0 — всех)
stock BizW_StaffCount(b, role)
{
	new DBResult:r;
	if (role) r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM biz_staff WHERE biz = %d AND owner = %d AND role = %d", Biz[b][bDbId], Biz[b][bOwner], role);
	else r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM biz_staff WHERE biz = %d AND owner = %d", Biz[b][bDbId], Biz[b][bOwner]);
	if (r == DBResult:0) return 0;
	new n = DB_GetFieldInt(r, 0);
	DB_FreeResultSet(r);
	return n;
}

static bool:BizW_IsOwner(playerid, b) { return Player[playerid][pDbId] != 0 && Biz[b][bOwner] == Player[playerid][pDbId]; }

// ---------------------------------------------------------------------
//  Налог (из BizX_PayDay)
// ---------------------------------------------------------------------
stock BizW_PayDayTax(b)
{
	if (Biz[b][bOwner] == 0) return;
	new due = BizW_Tax(b) + Biz[b][bTaxDebt];
	new pay = (Biz[b][bTill] < due) ? Biz[b][bTill] : due;
	Biz[b][bTill] -= pay;
	Econ_ToTreasury(pay);
	Biz[b][bTaxDebt] = due - pay;
	new owner = Bank_FindOnline(Biz[b][bOwner]);
	if (Biz[b][bTaxDebt] <= 0) Biz[b][bTaxPD] = 0;
	else
	{
		Biz[b][bTaxPD]++;
		if (Biz[b][bTaxPD] >= BIZ_TAX_SEIZE_PD)
		{
			if (owner != INVALID_PLAYER_ID) SendClientMessage(owner, COLOR_RED, "Штат забрал «%s» за долг по налогу ($%d).", Biz[b][bName], Biz[b][bTaxDebt]);
			Biz[b][bOwner] = 0;
			Biz[b][bOwnerName][0] = '\0';
			Biz[b][bTill] = 0;
			Biz[b][bMarkup] = 100;
			Biz[b][bStock] = 0;
			Biz_Save(b);
			BizW_Reset(b);
			Biz_Refresh(b);
			return;
		}
		if (owner != INVALID_PLAYER_ID)
		{
			if (Biz[b][bTaxPD] >= BIZ_TAX_CLOSE_PD)
				SendClientMessage(owner, COLOR_RED, "«%s» закрыт за долг по налогу $%d. Через %d PayDay штат заберёт бизнес. Оплатить: /bizmenu > Налог", Biz[b][bName], Biz[b][bTaxDebt], BIZ_TAX_SEIZE_PD - Biz[b][bTaxPD]);
			else
				SendClientMessage(owner, COLOR_ORANGE, "В кассе «%s» не хватило на налог. Долг $%d. Через %d PayDay бизнес закроют. Оплатить: /bizmenu > Налог", Biz[b][bName], Biz[b][bTaxDebt], BIZ_TAX_CLOSE_PD - Biz[b][bTaxPD]);
		}
	}
	BizX_Save(b);
}

stock BizW_ShowTax(playerid, b)
{
	if (Biz[b][bTaxDebt] <= 0)
	{
		ShowPlayerDialog(playerid, DIALOG_BIZX_INFO, DIALOG_STYLE_MSGBOX, "Налог",
			"{FFFFFF}Налог: {FFFF00}$%d{FFFFFF} за PayDay (%d‰ от цены бизнеса%s).\nСписывается из кассы сам. Долгов нет.\n\n{AFAFAF}Не хватит денег в кассе — появится долг.\nДолг %d PayDay подряд — бизнес закроют, %d — заберёт штат.",
			"Назад", "Закрыть", BizW_Tax(b), BIZ_TAX_PERMILLE, BizW_HasUpg(b, UPG_TAX) ? (", со скидкой консультанта") : (""), BIZ_TAX_CLOSE_PD, BIZ_TAX_SEIZE_PD);
		return 1;
	}
	ShowPlayerDialog(playerid, DIALOG_BIZW_TAX, DIALOG_STYLE_MSGBOX, "Налог",
		"{FFFFFF}Долг по налогу: {FF6347}$%d{FFFFFF}, не платится %d PayDay.\n%s\nОплатить наличными сейчас? У тебя: $%d",
		"Оплатить", "Назад", Biz[b][bTaxDebt], Biz[b][bTaxPD],
		BizW_Closed(b) ? ("{FF6347}Бизнес закрыт, пока долг не погашен.\n") : (""), GetMoney(playerid));
	return 1;
}

// ---------------------------------------------------------------------
//  Улучшения
// ---------------------------------------------------------------------
stock BizW_ShowUpg(playerid, b)
{
	new list[512] = "Улучшение\tЧто даёт\tЦена\n", line[128];
	for (new i = 0; i < UPG_COUNT; i++)
	{
		if (Biz[b][bUpg] & gUpgBit[i]) format(line, sizeof line, "%s\t%s\t{33AA33}куплено\n", gUpgName[i], gUpgDesc[i]);
		else format(line, sizeof line, "%s\t%s\t$%d\n", gUpgName[i], gUpgDesc[i], Biz[b][bPrice] / 100 * gUpgPct[i]);
		strcat(list, line);
	}
	ShowPlayerDialog(playerid, DIALOG_BIZW_UPG, DIALOG_STYLE_TABLIST_HEADERS, "Улучшения бизнеса", "%s", "Купить", "Назад", list);
	return 1;
}

// ---------------------------------------------------------------------
//  Продавцы: минимальная цена и процент
// ---------------------------------------------------------------------
stock BizW_ShowSellerCfg(playerid, b)
{
	ShowPlayerDialog(playerid, DIALOG_BIZW_SELLCFG, DIALOG_STYLE_INPUT, "Продавцы",
		"{FFFFFF}Продавцы продают товар игрокам: /sell [id]. Цену ставят сами, но не ниже минимальной.\nЦены у кассы сейчас {FFFF00}%d%%{FFFFFF}. Поставь у продавцов ниже — к ним будут идти охотнее.\n\nСейчас: мин. цена {FFFF00}%d%%{FFFFFF} от обычной, продавцу {FFFF00}%d%%{FFFFFF} с продажи.\n\nВведи через пробел мин. цену (%d–%d) и процент продавцу (0–%d), например: {FFFF00}90 10\n{AFAFAF}Процент 0 — продавцы без процента, платишь им премиями.",
		"Сохранить", "Назад", Biz[b][bMarkup], BizW_SellMin(b), Biz[b][bComm], BIZW_SELLMIN_MIN, BIZW_SELLMIN_MAX, BIZW_COMM_MAX);
	return 1;
}

// ---------------------------------------------------------------------
//  Сотрудники
// ---------------------------------------------------------------------
stock BizW_ShowStaff(playerid, b)
{
	new list[1024] = "Сотрудник\tДолжность\tПродаж\tЗаработал\n", line[128], n = 0;
	strcat(list, "{FF9900}+ Нанять игрока\t \t \t \n");
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT acc, role, name, sold, earned FROM biz_staff WHERE biz = %d AND owner = %d ORDER BY role DESC, sold DESC", Biz[b][bDbId], Biz[b][bOwner]);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r), name[MAX_PLAYER_NAME];
		for (new i = 0; i < rows && n < sizeof (gStaffAcc[]); i++)
		{
			gStaffAcc[playerid][n++] = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 2, name, sizeof name);
			format(line, sizeof line, "%s%s\t%s\t%d\t$%d\n", Bank_FindOnline(gStaffAcc[playerid][n - 1]) != INVALID_PLAYER_ID ? ("{FFFFFF}") : ("{8C9098}"), name,
				DB_GetFieldInt(r, 1) == BW_DEPUTY ? ("заместитель") : ("продавец"), DB_GetFieldInt(r, 3), DB_GetFieldInt(r, 4));
			strcat(list, line);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	for (new i = n; i < sizeof (gStaffAcc[]); i++) gStaffAcc[playerid][i] = 0;
	ShowPlayerDialog(playerid, DIALOG_BIZW_STAFF, DIALOG_STYLE_TABLIST_HEADERS, "Сотрудники", "%s", "Выбрать", "Назад", list);
	return 1;
}

static BizW_StaffRole(b, acc, name[], size)
{
	name[0] = EOS;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT role, name FROM biz_staff WHERE acc = %d AND biz = %d AND owner = %d", acc, Biz[b][bDbId], Biz[b][bOwner]);
	if (r == DBResult:0) return 0;
	new role = 0;
	if (DB_GetRowCount(r) > 0) { role = DB_GetFieldInt(r, 0); DB_GetFieldString(r, 1, name, size); }
	DB_FreeResultSet(r);
	return role;
}

static BizW_ShowStaffAct(playerid, b)
{
	new name[MAX_PLAYER_NAME], role = BizW_StaffRole(b, gStaffPick[playerid], name, sizeof name);
	if (!role) return BizW_ShowStaff(playerid, b);
	new list[256];
	format(list, sizeof list, "Выдать премию из кассы (в кассе $%d)\n", Biz[b][bTill]);
	if (BizW_IsOwner(playerid, b)) strcat(list, role == BW_DEPUTY ? ("Сделать продавцом\n") : ("Сделать заместителем\n"));
	strcat(list, "Уволить");
	ShowPlayerDialog(playerid, DIALOG_BIZW_STAFFACT, DIALOG_STYLE_LIST, name, "%s", "Выбрать", "Назад", list);
	return 1;
}

// ---------------------------------------------------------------------
//  /sell [id] — продавец продаёт товар игроку рядом
// ---------------------------------------------------------------------
stock bool:Cmd_BizSell(playerid, const params[])
{
	new b = Biz_GetOwned(playerid);
	if (b == -1) BizW_RoleOf(playerid, b);
	if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "Ты не работаешь в бизнесе. Продавцов нанимает владелец: /bizmenu > Сотрудники.");
	if (!BizX_UsesStock(b)) return SendClientMessage(playerid, COLOR_GREY, "В этом бизнесе продавцы не нужны.");
	if (Biz_GetNearby(playerid) != b && !Biz_IsInside(playerid, b)) return SendClientMessage(playerid, COLOR_GREY, "Продавать можно только у своего бизнеса или внутри.");
	if (BizW_Closed(b)) return SendClientMessage(playerid, COLOR_GREY, "Бизнес закрыт за неуплату налогов.");
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /sell [id покупателя рядом]");
	if (target == playerid || !Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Нужно ввести id другого игрока рядом.");
	if (!IsPlayerNearPlayer(playerid, target, BIZW_RANGE)) return SendClientMessage(playerid, COLOR_GREY, "Покупатель должен стоять рядом.");

	new list[1024] = "Товар\tУ кассы\tМин. цена\n", line[80], n = 0, cnt = Biz_ItemCount();
	for (new i = 0; i < cnt && n < sizeof (gSellList[]); i++)
	{
		if (Biz_ItemType(i) != Biz[b][bType]) continue;
		format(line, sizeof line, "%s\t$%d\t$%d\n", Biz_ItemName(i), BizX_Price(b, Biz_ItemBase(i)), BizW_MinPrice(b, i));
		strcat(list, line);
		gSellList[playerid][n++] = i;
	}
	if (n == 0) return SendClientMessage(playerid, COLOR_GREY, "Здесь нечего продавать.");
	gSellBuyer[playerid] = target;
	gSellBiz[playerid] = Biz[b][bDbId];
	ShowPlayerDialog(playerid, DIALOG_BIZW_SELLITEM, DIALOG_STYLE_TABLIST_HEADERS, "Что продаём?", "%s", "Дальше", "Отмена", list);
	return true;
}

stock BizW_MinPrice(b, item)
{
	new p = Biz_ItemBase(item) * BizW_SellMin(b) / 100;
	return p < 1 ? 1 : p;
}

// Продавец всё ещё может продавать в бизнесе b (dbid)
static BizW_SellerBiz(playerid, dbid)
{
	new b = Biz_GetOwned(playerid);
	if (b == -1) BizW_RoleOf(playerid, b);
	if (b == -1 || Biz[b][bDbId] != dbid) return -1;
	if (Biz_GetNearby(playerid) != b && !Biz_IsInside(playerid, b)) return -1;
	return b;
}

static BizW_Hire(playerid)
{
	new ownerDb = gHireBy[playerid];
	gHireBy[playerid] = 0;
	new b = Biz_FindByDbId(gHireBiz[playerid]);
	if (b == -1 || Biz[b][bOwner] == 0) return SendClientMessage(playerid, COLOR_GREY, "Этот бизнес уже не нанимает.");
	new boss = Bank_FindOnline(ownerDb);
	new role = gHireRole[playerid];
	if (BizW_IsOwner(playerid, b)) return 1;
	new sb;
	if (BizW_RoleOf(playerid, sb)) return SendClientMessage(playerid, COLOR_GREY, "Ты уже работаешь в другом бизнесе. Уволиться: /bizquit");
	if (BizW_StaffCount(b, role) >= (role == BW_DEPUTY ? BIZW_MAX_DEPUTIES : BIZW_MAX_SELLERS))
		return SendClientMessage(playerid, COLOR_GREY, "Мест больше нет.");
	new name[MAX_PLAYER_NAME];
	GetPlayerName(playerid, name, sizeof name);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO biz_staff (acc, biz, owner, role, name, sold, earned) VALUES (%d, %d, %d, %d, '%q', 0, 0)",
		Player[playerid][pDbId], Biz[b][bDbId], Biz[b][bOwner], role, name));
	SendClientMessage(playerid, COLOR_GREEN, "Ты теперь %s в «%s». %s", role == BW_DEPUTY ? ("заместитель") : ("продавец"), Biz[b][bName],
		role == BW_DEPUTY ? ("Меню бизнеса: /bizmenu. Продавать: /sell [id]") : ("Продавать покупателю рядом: /sell [id]. Уволиться: /bizquit"));
	if (boss != INVALID_PLAYER_ID) SendClientMessage(boss, COLOR_GREEN, "%s принял предложение работы.", GetRPName(playerid));
	return 1;
}

// /bizquit — уволиться самому
stock bool:Cmd_BizQuit(playerid)
{
	new b, role = BizW_RoleOf(playerid, b);
	if (!role) return SendClientMessage(playerid, COLOR_GREY, "Ты нигде не работаешь.");
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM biz_staff WHERE acc = %d", Player[playerid][pDbId]));
	SendClientMessage(playerid, COLOR_WHITE, "Ты уволился из «%s».", Biz[b][bName]);
	new boss = Bank_FindOnline(Biz[b][bOwner]);
	if (boss != INVALID_PLAYER_ID) SendClientMessage(boss, COLOR_ORANGE, "%s уволился из «%s».", GetRPName(playerid), Biz[b][bName]);
	return true;
}

// Покупатель согласился на предложение продавца
static BizW_CompleteSale(buyer)
{
	new seller = gSOFrom[buyer];
	gSOFrom[buyer] = 0;
	if (seller == 0) return 1;
	seller--;
	new item = gSOItem[buyer], price = gSOPrice[buyer];
	if (!IsPlayerConnected(seller) || !Player[seller][pLogged]) return SendClientMessage(buyer, COLOR_GREY, "Продавец ушёл.");
	new b = BizW_SellerBiz(seller, gSOBiz[buyer]);
	if (b == -1 || BizW_Closed(b)) return SendClientMessage(buyer, COLOR_GREY, "Продавец больше не может продать этот товар.");
	if (!IsPlayerNearPlayer(seller, buyer, BIZW_RANGE)) return SendClientMessage(buyer, COLOR_GREY, "Продавец отошёл.");
	if (gettime() - gSOTime[buyer] > 60) return SendClientMessage(buyer, COLOR_GREY, "Предложение устарело.");
	new base = Biz_ItemBase(item);
	if (Biz[b][bStock] < BizX_StockCost(base))
	{
		SendClientMessage(seller, COLOR_GREY, "На складе нет товара. Закупить: /bizmenu > Закупить товар.");
		return SendClientMessage(buyer, COLOR_GREY, "Товара нет на складе.");
	}
	if (GetMoney(buyer) < price) { SendClientMessage(seller, COLOR_GREY, "У покупателя не хватило денег."); return SendClientMessage(buyer, COLOR_GREY, "Не хватает денег. Нужно $%d.", price); }
	if (!Biz_CanBuy(buyer, b, item)) { SendClientMessage(seller, COLOR_GREY, "Покупатель не может это купить."); return 1; }

	new comm = BizW_IsOwner(seller, b) ? 0 : price * Biz[b][bComm] / 100;
	GiveMoney(buyer, -price, "покупка у продавца");
	Biz_GiveItem(buyer, item, price);
	if (comm > 0) GiveMoney(seller, comm, "процент продавца");
	BizX_Sale(b, price - comm, base);
	if (!BizW_IsOwner(seller, b))
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE biz_staff SET sold = sold + 1, earned = earned + %d WHERE acc = %d", comm, Player[seller][pDbId]));
	Account_Save(buyer);
	Account_Save(seller);
	new name[24];
	name = Biz_ItemName(item);
	SendClientMessage(buyer, COLOR_GREEN, "Ты купил у %s: %s за $%d.", GetRPName(seller), name, price);
	if (comm > 0) SendClientMessage(seller, COLOR_GREEN, "Продано: %s за $%d. Твой процент: $%d.", name, price, comm);
	else SendClientMessage(seller, COLOR_GREEN, "Продано: %s за $%d.", name, price);
	new msg[144];
	format(msg, sizeof msg, "%s продаёт %s товар.", GetRPName(seller), GetRPName(buyer));
	SendNearbyMessage(seller, CHAT_RADIUS, COLOR_PURPLE, msg);
	return 1;
}

// ---------------------------------------------------------------------
//  Окна
// ---------------------------------------------------------------------
stock BizW_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid < DIALOG_BIZW_TAX || dialogid > DIALOG_BIZW_SELLOFFER) return 0;

	// окна покупателя и нанимаемого: бизнеса у них нет
	if (dialogid == DIALOG_BIZW_SELLOFFER)
	{
		if (!response)
		{
			new s = gSOFrom[playerid] - 1;
			gSOFrom[playerid] = 0;
			if (s >= 0 && IsPlayerConnected(s)) SendClientMessage(s, COLOR_GREY, "%s отказался от покупки.", GetRPName(playerid));
			return 1;
		}
		BizW_CompleteSale(playerid);
		return 1;
	}
	if (dialogid == DIALOG_BIZW_HIREOFFER)
	{
		if (gHireBy[playerid] == 0) return 1;
		if (!response)
		{
			new boss = Bank_FindOnline(gHireBy[playerid]);
			gHireBy[playerid] = 0;
			if (boss != INVALID_PLAYER_ID) SendClientMessage(boss, COLOR_GREY, "%s отказался от работы.", GetRPName(playerid));
			return 1;
		}
		BizW_Hire(playerid);
		return 1;
	}
	if (dialogid == DIALOG_BIZW_SELLITEM || dialogid == DIALOG_BIZW_SELLPRICE)
	{
		if (!response) return 1;
		new b = BizW_SellerBiz(playerid, gSellBiz[playerid]);
		if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "Продавать можно только у своего бизнеса или внутри.");
		new buyer = gSellBuyer[playerid];
		if (!IsPlayerConnected(buyer) || !Player[buyer][pLogged] || !IsPlayerNearPlayer(playerid, buyer, BIZW_RANGE))
			return SendClientMessage(playerid, COLOR_GREY, "Покупатель отошёл.");
		if (dialogid == DIALOG_BIZW_SELLITEM)
		{
			if (listitem < 0 || listitem >= sizeof (gSellList[])) return 1;
			new item = gSellList[playerid][listitem];
			if (item < 0 || item >= Biz_ItemCount() || Biz_ItemType(item) != Biz[b][bType]) return 1;
			gSellItem[playerid] = item;
			ShowPlayerDialog(playerid, DIALOG_BIZW_SELLPRICE, DIALOG_STYLE_INPUT, "Цена",
				"{FFFFFF}%s\nУ кассы: $%d. Минимум у продавца: {FFFF00}$%d{FFFFFF}.%s\n\nЗа сколько продаёшь?",
				"Предложить", "Отмена", Biz_ItemName(item), BizX_Price(b, Biz_ItemBase(item)), BizW_MinPrice(b, item),
				(!BizW_IsOwner(playerid, b) && Biz[b][bComm] > 0) ? ("\nТвой процент с продажи есть в меню владельца.") : (""));
			return 1;
		}
		new item = gSellItem[playerid];
		if (!IsNumeric(inputtext) || strlen(inputtext) > 9) return SendClientMessage(playerid, COLOR_GREY, "Нужно ввести цену числом.");
		new price = strval(inputtext), lo = BizW_MinPrice(b, item), hi = Biz_ItemBase(item) * BIZW_SELL_MAX / 100;
		if (price < lo || price > hi) return SendClientMessage(playerid, COLOR_GREY, "Цена — от $%d до $%d.", lo, hi);
		gSOFrom[buyer] = playerid + 1;
		gSOItem[buyer] = item;
		gSOPrice[buyer] = price;
		gSOBiz[buyer] = Biz[b][bDbId];
		gSOTime[buyer] = gettime();
		ShowPlayerDialog(buyer, DIALOG_BIZW_SELLOFFER, DIALOG_STYLE_MSGBOX, Biz_IconCaption(Biz[b][bType], Biz[b][bName]),
			"{FFFFFF}Продавец %s предлагает:\n{FF9900}%s{FFFFFF} за {FFFF00}$%d{FFFFFF}\n\n{AFAFAF}У кассы этот товар стоит $%d.\n\n{FFFFFF}Купить?",
			"Купить", "Отказаться", GetRPName(playerid), Biz_ItemName(item), price, BizX_Price(b, Biz_ItemBase(item)));
		SendClientMessage(playerid, COLOR_WHITE, "Ты предложил %s: %s за $%d. Ждём ответа.", GetRPName(buyer), Biz_ItemName(item), price);
		return 1;
	}

	// дальше — окна меню бизнеса (владелец или заместитель)
	new b = BizX_MineW(playerid);
	if (b == -1) return 1;
	new bool:own = BizW_IsOwner(playerid, b);
	switch (dialogid)
	{
		case DIALOG_BIZW_TAX:
		{
			if (!response) return BizW_Menu(playerid);
			new debt = Biz[b][bTaxDebt];
			if (debt <= 0) return BizW_Menu(playerid);
			if (GetMoney(playerid) < debt) return SendClientMessage(playerid, COLOR_GREY, "Не хватает наличных. Нужно $%d.", debt);
			GiveMoney(playerid, -debt, "налог на бизнес");
			Econ_ToTreasury(debt);
			Biz[b][bTaxDebt] = 0;
			Biz[b][bTaxPD] = 0;
			BizX_Save(b);
			Account_Save(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Долг по налогу $%d оплачен. «%s» снова работает.", debt, Biz[b][bName]);
			return BizW_Menu(playerid);
		}
		case DIALOG_BIZW_UPG:
		{
			if (!response || !own) return BizW_Menu(playerid);
			if (listitem < 0 || listitem >= UPG_COUNT) return 1;
			if (Biz[b][bUpg] & gUpgBit[listitem]) { SendClientMessage(playerid, COLOR_GREY, "Уже куплено."); return BizW_ShowUpg(playerid, b); }
			new cost = Biz[b][bPrice] / 100 * gUpgPct[listitem];
			if (GetMoney(playerid) < cost) return SendClientMessage(playerid, COLOR_GREY, "Не хватает наличных. Нужно $%d.", cost);
			GiveMoney(playerid, -cost, "улучшение бизнеса");
			Econ_ToTreasury(cost);
			Biz[b][bUpg] |= gUpgBit[listitem];
			BizX_Save(b);
			Account_Save(playerid);
			SendClientMessage(playerid, COLOR_GREEN, "Куплено: %s (%s).", gUpgName[listitem], gUpgDesc[listitem]);
			return BizW_ShowUpg(playerid, b);
		}
		case DIALOG_BIZW_SELLCFG:
		{
			if (!response) return BizW_Menu(playerid);
			new pos = 0, mn, cm;
			if (!NextIntParam(inputtext, pos, mn) || !NextIntParam(inputtext, pos, cm))
				return SendClientMessage(playerid, COLOR_GREY, "Введи два числа через пробел, например: 90 10");
			if (mn < BIZW_SELLMIN_MIN || mn > BIZW_SELLMIN_MAX) return SendClientMessage(playerid, COLOR_GREY, "Мин. цена — от %d до %d%%.", BIZW_SELLMIN_MIN, BIZW_SELLMIN_MAX);
			if (cm < 0 || cm > BIZW_COMM_MAX) return SendClientMessage(playerid, COLOR_GREY, "Процент продавцу — от 0 до %d.", BIZW_COMM_MAX);
			Biz[b][bSellMin] = mn;
			Biz[b][bComm] = cm;
			BizX_Save(b);
			SendClientMessage(playerid, COLOR_GREEN, "Продавцы: цены от %d%% обычных, продавцу %d%% с продажи.", mn, cm);
			return BizW_Menu(playerid);
		}
		case DIALOG_BIZW_STAFF:
		{
			if (!response) return BizW_Menu(playerid);
			if (listitem == 0)
			{
				ShowPlayerDialog(playerid, DIALOG_BIZW_HIRE, DIALOG_STYLE_INPUT, "Нанять",
					"{FFFFFF}Игрок должен стоять рядом.\nПродавцов до %d, заместителей до %d.\n\nВведи id игрока%s, например: {FFFF00}12%s",
					"Предложить", "Назад", BIZW_MAX_SELLERS, BIZW_MAX_DEPUTIES,
					own ? (" и должность: 1 — продавец, 2 — заместитель") : (""), own ? (" 1") : (""));
				return 1;
			}
			new i = listitem - 1;
			if (i < 0 || i >= sizeof (gStaffAcc[]) || gStaffAcc[playerid][i] == 0) return 1;
			gStaffPick[playerid] = gStaffAcc[playerid][i];
			return BizW_ShowStaffAct(playerid, b);
		}
		case DIALOG_BIZW_STAFFACT:
		{
			if (!response) return BizW_ShowStaff(playerid, b);
			new name[MAX_PLAYER_NAME], acc = gStaffPick[playerid], role = BizW_StaffRole(b, acc, name, sizeof name);
			if (!role) return BizW_ShowStaff(playerid, b);
			if (!own && role == BW_DEPUTY && listitem != 0) return SendClientMessage(playerid, COLOR_GREY, "Заместителями распоряжается владелец.");
			if (listitem == 0)
			{
				ShowPlayerDialog(playerid, DIALOG_BIZW_BONUS, DIALOG_STYLE_INPUT, "Премия",
					"{FFFFFF}Премия %s из кассы бизнеса.\nВ кассе: {33AA33}$%d{FFFFFF}. Сотрудник должен быть в игре.\n\nСумма:",
					"Выдать", "Назад", name, Biz[b][bTill]);
				return 1;
			}
			new act = listitem;
			if (!own) act++;     // у заместителя нет строки «Сделать ...»
			if (act == 1)
			{
				new nr = role == BW_DEPUTY ? BW_SELLER : BW_DEPUTY;
				if (BizW_StaffCount(b, nr) >= (nr == BW_DEPUTY ? BIZW_MAX_DEPUTIES : BIZW_MAX_SELLERS)) return SendClientMessage(playerid, COLOR_GREY, "Мест на этой должности больше нет.");
				DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE biz_staff SET role = %d WHERE acc = %d", nr, acc));
				new t = Bank_FindOnline(acc);
				if (t != INVALID_PLAYER_ID) SendClientMessage(t, COLOR_ORANGE, "Ты теперь %s в «%s».", nr == BW_DEPUTY ? ("заместитель") : ("продавец"), Biz[b][bName]);
				SendClientMessage(playerid, COLOR_GREEN, "%s теперь %s.", name, nr == BW_DEPUTY ? ("заместитель") : ("продавец"));
				return BizW_ShowStaff(playerid, b);
			}
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM biz_staff WHERE acc = %d", acc));
			new t = Bank_FindOnline(acc);
			if (t != INVALID_PLAYER_ID) SendClientMessage(t, COLOR_ORANGE, "Тебя уволили из «%s».", Biz[b][bName]);
			SendClientMessage(playerid, COLOR_WHITE, "%s уволен.", name);
			return BizW_ShowStaff(playerid, b);
		}
		case DIALOG_BIZW_BONUS:
		{
			if (!response) return BizW_ShowStaffAct(playerid, b);
			new name[MAX_PLAYER_NAME], acc = gStaffPick[playerid];
			if (!BizW_StaffRole(b, acc, name, sizeof name)) return BizW_ShowStaff(playerid, b);
			if (!IsNumeric(inputtext) || strlen(inputtext) > 9) return SendClientMessage(playerid, COLOR_GREY, "Нужно ввести сумму числом.");
			new sum = strval(inputtext);
			if (sum < 1 || sum > BIZW_BONUS_MAX) return SendClientMessage(playerid, COLOR_GREY, "Премия — от $1 до $%d.", BIZW_BONUS_MAX);
			if (Biz[b][bTill] < sum) return SendClientMessage(playerid, COLOR_GREY, "В кассе только $%d.", Biz[b][bTill]);
			new t = Bank_FindOnline(acc);
			if (t == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "%s не в игре.", name);
			Biz[b][bTill] -= sum;
			BizX_Save(b);
			GiveMoney(t, sum, "премия от бизнеса");
			Account_Save(t);
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE biz_staff SET earned = earned + %d WHERE acc = %d", sum, acc));
			SendClientMessage(t, COLOR_GREEN, "Премия от «%s»: $%d (выдал %s).", Biz[b][bName], sum, GetRPName(playerid));
			SendClientMessage(playerid, COLOR_GREEN, "Премия $%d выдана: %s.", sum, name);
			return BizW_ShowStaff(playerid, b);
		}
		case DIALOG_BIZW_HIRE:
		{
			if (!response) return BizW_ShowStaff(playerid, b);
			new pos = 0, role = BW_SELLER;
			new target = NextPlayerParam(inputtext, pos);
			if (own && !NextIntParam(inputtext, pos, role)) role = BW_SELLER;
			if (role != BW_SELLER && role != BW_DEPUTY) return SendClientMessage(playerid, COLOR_GREY, "Должность: 1 — продавец, 2 — заместитель.");
			if (target == INVALID_PLAYER_ID || target == playerid || !Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Такого игрока нет.");
			if (!IsPlayerNearPlayer(playerid, target, BIZW_RANGE)) return SendClientMessage(playerid, COLOR_GREY, "Игрок должен стоять рядом.");
			if (BizW_StaffCount(b, role) >= (role == BW_DEPUTY ? BIZW_MAX_DEPUTIES : BIZW_MAX_SELLERS)) return SendClientMessage(playerid, COLOR_GREY, "Мест на этой должности больше нет.");
			gHireBy[target] = Biz[b][bOwner];
			gHireBiz[target] = Biz[b][bDbId];
			gHireRole[target] = role;
			ShowPlayerDialog(target, DIALOG_BIZW_HIREOFFER, DIALOG_STYLE_MSGBOX, "Работа",
				"{FFFFFF}%s предлагает тебе работу в {FF9900}%s{FFFFFF} (%s).\nДолжность: {FFFF00}%s{FFFFFF}.\n\nПродавец получает %d%% с каждой продажи и премии от владельца.\n\nСогласиться?",
				"Да", "Нет", GetRPName(playerid), Biz[b][bName], Biz_GetTypeName(Biz[b][bType]), role == BW_DEPUTY ? ("заместитель") : ("продавец"), Biz[b][bComm]);
			SendClientMessage(playerid, COLOR_WHITE, "Предложение отправлено %s.", GetRPName(target));
			return 1;
		}
	}
	return 1;
}

// Бизнес из меню для владельца или заместителя (bizplus.inc хранит, какой открыт)
static BizX_MineW(playerid)
{
	new b = Biz_GetOwned(playerid);
	if (b != -1) return b;
	if (BizW_RoleOf(playerid, b) == BW_DEPUTY) return b;
	return -1;
}

static BizW_Menu(playerid)
{
	Cmd_BizMenu(playerid);
	return 1;
}
