// =====================================================================
//  bizstat.inc — финансовая выписка бизнеса и история дохода по дням.
//
//  Каждое поступление в кассу (BizX_AddTill) пишется в biz_income по дням.
//  Выписка: владелец открывает её себе (меню бизнеса > «Финансовая выписка»)
//  или показывает игроку рядом: /showbiz [id]. Окно — документ (index.html,
//  kind "bizstat"), иначе обычный диалог.
//  У каждой выписки свой код. Цифры на момент выдачи сохраняются в
//  biz_statements, и любой игрок проверяет подлинность: /checkbiz [код].
//  Скрин с другими цифрами при том же коде сразу видно.
// =====================================================================

#define BIZS_DAYS           7           // дней в выписке
#define BIZS_SHOW_RANGE     3.0
#define BIZS_TZ             (3 * 3600)  // день считается по Москве
#define BIZS_KEEP_DAYS      60          // сколько дней хранить историю дохода

static BizS_Today()
{
	return (gettime() + BIZS_TZ) / 86400;
}

stock BizS_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS biz_income (biz INTEGER, day INTEGER, amount INTEGER DEFAULT 0, PRIMARY KEY (biz, day))"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS biz_statements (code TEXT PRIMARY KEY, biz INTEGER, name TEXT, type INTEGER, owner TEXT, issuer INTEGER, issued INTEGER, till INTEGER, week INTEGER, earned INTEGER, stock INTEGER, stockmax INTEGER, markup INTEGER, days TEXT)"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM biz_income WHERE day < %d", BizS_Today() - BIZS_KEEP_DAYS));
}

// Деньги пришли в кассу бизнеса b (из BizX_AddTill)
stock BizS_Log(b, amount)
{
	if (amount <= 0) return;
	new day = BizS_Today(), id = Biz[b][bDbId];
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR IGNORE INTO biz_income (biz, day, amount) VALUES (%d, %d, 0)", id, day));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "UPDATE biz_income SET amount = amount + %d WHERE biz = %d AND day = %d", amount, id, day));
}

// Доход по дням: days[0] — сегодня, days[BIZS_DAYS-1] — неделю назад. Возвращает сумму.
static BizS_Days(dbid, days[BIZS_DAYS])
{
	for (new i = 0; i < BIZS_DAYS; i++) days[i] = 0;
	new today = BizS_Today(), sum = 0;
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT day, amount FROM biz_income WHERE biz = %d AND day > %d", dbid, today - BIZS_DAYS);
	if (r == DBResult:0) return 0;
	new rows = DB_GetRowCount(r);
	for (new i = 0; i < rows; i++)
	{
		new k = today - DB_GetFieldInt(r, 0);
		if (k >= 0 && k < BIZS_DAYS) { days[k] = DB_GetFieldInt(r, 1); sum += days[k]; }
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	return sum;
}

static BizS_DateTime(dest[], size, stamp)
{
	stamp += BIZS_TZ;
	new z = stamp / 86400 + 719468;
	new era = z / 146097;
	new doe = z - era * 146097;
	new yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	new y = yoe + era * 400;
	new doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	new mp = (5 * doy + 2) / 153;
	new d = doy - (153 * mp + 2) / 5 + 1;
	new m = mp < 10 ? mp + 3 : mp - 9;
	if (m <= 2) y++;
	new sec = stamp % 86400;
	format(dest, size, "%02d.%02d.%d %02d:%02d", d, m, y, sec / 3600, (sec / 60) % 60);
}

// Что лежит в выписке (свежей или из базы при проверке)
enum E_BIZS
{
	bsName[32],
	bsType,
	bsNum,
	bsOwner[MAX_PLAYER_NAME],
	bsIssued,
	bsTill,
	bsWeek,
	bsEarned,
	bsStock,
	bsStockMax,
	bsMarkup,
	bsCode[8],
	bsDays[BIZS_DAYS]
}

static BizS_Send(viewer, const s[E_BIZS], bool:own, bool:verified)
{
	new issued[24];
	BizS_DateTime(issued, sizeof issued, s[bsIssued]);
	new owner[MAX_PLAYER_NAME];
	format(owner, sizeof owner, "%s", s[bsOwner]);
	for (new i = 0; owner[i]; i++) if (owner[i] == '_') owner[i] = ' ';

	if (UI_Has(viewer))
	{
		new json[900], en[72], et[48], eo[56], days[96];
		UI_JsonEscape(en, s[bsName]);
		UI_JsonEscape(et, Biz_GetTypeName(s[bsType]));
		UI_JsonEscape(eo, owner);
		format(days, sizeof days, "%d,%d,%d,%d,%d,%d,%d", s[bsDays][6], s[bsDays][5], s[bsDays][4], s[bsDays][3], s[bsDays][2], s[bsDays][1], s[bsDays][0]);
		format(json, sizeof json,
			"{\"kind\":\"bizstat\",\"own\":%d,\"verified\":%d,\"name\":\"%s\",\"type\":\"%s\",\"num\":\"%d\",\"owner\":\"%s\",\"issued\":\"%s\",\"till\":%d,\"week\":%d,\"total\":%d,\"stock\":%d,\"stockMax\":%d,\"markup\":%d,\"code\":\"%s\",\"days\":[%s]}",
			own, verified, en, et, s[bsNum], eo, issued, s[bsTill], s[bsWeek], s[bsEarned], s[bsStock], s[bsStockMax], s[bsMarkup], s[bsCode], days);
		UI_Show(viewer, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "document", json, "Закрыть");
		return;
	}
	new text[700], line[96];
	format(text, sizeof text, "%s{8C9098}ФИНАНСОВАЯ ВЫПИСКА · BASTION BANK\n\n{8C9098}Бизнес: {FFFFFF}%s (№%d)\n{8C9098}Вид: {FFFFFF}%s\n{8C9098}Владелец: {FFFFFF}%s\n{8C9098}Выдана: {FFFFFF}%s\n\n",
		verified ? ("{4CD07D}ПОДЛИННАЯ ВЫПИСКА\n") : (""), s[bsName], s[bsNum], Biz_GetTypeName(s[bsType]), owner, issued);
	format(line, sizeof line, "{8C9098}В кассе: {33AA33}$%d\n{8C9098}Доход за %d дн.: {FFFFFF}$%d\n{8C9098}Доход за всё время: {FFFFFF}$%d\n", s[bsTill], BIZS_DAYS, s[bsWeek], s[bsEarned]);
	strcat(text, line);
	if (s[bsStockMax] > 0)
	{
		format(line, sizeof line, "{8C9098}Склад: {FFFFFF}$%d из $%d\n{8C9098}Цены: {FFFFFF}%d%%\n", s[bsStock], s[bsStockMax], s[bsMarkup]);
		strcat(text, line);
	}
	format(line, sizeof line, "\n{8C9098}Код проверки: {FF7A1A}%s{8C9098}  (/checkbiz %s)", s[bsCode], s[bsCode]);
	strcat(text, line);
	ShowPlayerDialog(viewer, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "{FF7A1A}BASTION {FFFFFF}· Выписка", "%s", "Закрыть", "", text);
}

// Новая выписка по бизнесу b от игрока playerid, сохраняется в базе
static BizS_Make(playerid, b, s[E_BIZS])
{
	static const abc[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
	format(s[bsName], 32, "%s", Biz[b][bName]);
	s[bsType] = Biz[b][bType];
	s[bsNum] = Biz[b][bDbId];
	format(s[bsOwner], MAX_PLAYER_NAME, "%s", Biz[b][bOwnerName]);
	s[bsIssued] = gettime();
	s[bsTill] = Biz[b][bTill];
	s[bsEarned] = Biz[b][bEarned];
	s[bsStockMax] = BizX_UsesStock(b) ? BizX_StockMax(b) : 0;
	s[bsStock] = s[bsStockMax] > 0 ? Biz[b][bStock] : 0;
	s[bsMarkup] = Biz[b][bMarkup];
	new days[BIZS_DAYS];
	s[bsWeek] = BizS_Days(Biz[b][bDbId], days);
	for (new i = 0; i < BIZS_DAYS; i++) s[bsDays][i] = days[i];
	new dstr[96];
	format(dstr, sizeof dstr, "%d,%d,%d,%d,%d,%d,%d", days[0], days[1], days[2], days[3], days[4], days[5], days[6]);

	for (new tries = 0; tries < 5; tries++)
	{
		for (new i = 0; i < 6; i++) s[bsCode][i] = abc[random(sizeof (abc) - 1)];
		s[bsCode][6] = EOS;
		new DBResult:r = DB_ExecuteQuery(gDB,
			"INSERT OR IGNORE INTO biz_statements (code, biz, name, type, owner, issuer, issued, till, week, earned, stock, stockmax, markup, days) VALUES ('%s', %d, '%q', %d, '%q', %d, %d, %d, %d, %d, %d, %d, %d, '%s')",
			s[bsCode], s[bsNum], s[bsName], s[bsType], s[bsOwner], Player[playerid][pDbId], s[bsIssued], s[bsTill], s[bsWeek], s[bsEarned], s[bsStock], s[bsStockMax], s[bsMarkup], dstr);
		DB_FreeResultSet(r);
		new DBResult:c = DB_ExecuteQuery(gDB, "SELECT issued FROM biz_statements WHERE code = '%s'", s[bsCode]);
		new bool:mine = false;
		if (c != DBResult:0)
		{
			mine = DB_GetRowCount(c) > 0 && DB_GetFieldInt(c, 0) == s[bsIssued];
			DB_FreeResultSet(c);
		}
		if (mine) break;
	}
}

// Владелец смотрит выписку сам (меню бизнеса)
stock BizS_ShowOwn(playerid, b)
{
	new s[E_BIZS];
	BizS_Make(playerid, b, s);
	BizS_Send(playerid, s, true, false);
}

// /showbiz [id] — показать выписку игроку рядом
stock bool:Cmd_ShowBiz(playerid, const params[])
{
	new b = Biz_GetOwned(playerid);
	if (b == -1) return SendClientMessage(playerid, COLOR_GREY, "У тебя нет бизнеса.");
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) return SendClientMessage(playerid, COLOR_GREY, "Использование: /showbiz [id игрока рядом]. Себе: /bizmenu > Финансовая выписка");
	if (target == playerid || !Player[target][pLogged]) return SendClientMessage(playerid, COLOR_GREY, "Нужно ввести id другого игрока рядом.");
	if (!IsPlayerNearPlayer(playerid, target, BIZS_SHOW_RANGE)) return SendClientMessage(playerid, COLOR_GREY, "Подойди ближе к игроку.");
	new s[E_BIZS];
	BizS_Make(playerid, b, s);
	BizS_Send(target, s, false, false);
	new msg[144];
	format(msg, sizeof msg, "%s показывает финансовую выписку бизнеса %s.", GetRPName(playerid), GetRPName(target));
	SendNearbyMessage(playerid, CHAT_RADIUS, COLOR_PURPLE, msg);
	SendClientMessage(playerid, COLOR_GREY, "Код выписки: %s. Любой может проверить её: /checkbiz %s", s[bsCode], s[bsCode]);
	return true;
}

// /checkbiz [код] — проверить выписку: показывает цифры, сохранённые при выдаче
stock bool:Cmd_CheckBiz(playerid, const params[])
{
	new code[8], n = 0;
	for (new i = 0; params[i] && n < 6; i++)
	{
		new ch = params[i];
		if (ch == ' ') { if (n) break; continue; }
		if (ch >= 'a' && ch <= 'z') ch -= 32;
		if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))) break;
		code[n++] = ch;
	}
	code[n] = EOS;
	if (n != 6) return SendClientMessage(playerid, COLOR_GREY, "Использование: /checkbiz [код из выписки, 6 знаков]");
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT biz, name, type, owner, issued, till, week, earned, stock, stockmax, markup, days FROM biz_statements WHERE code = '%s'", code);
	if (r == DBResult:0 || DB_GetRowCount(r) == 0)
	{
		if (r != DBResult:0) DB_FreeResultSet(r);
		return SendClientMessage(playerid, COLOR_RED, "Выписки с кодом %s нет. Скорее всего, она поддельная.", code);
	}
	new s[E_BIZS], dstr[96];
	s[bsNum] = DB_GetFieldInt(r, 0);
	DB_GetFieldString(r, 1, s[bsName], 32);
	s[bsType] = DB_GetFieldInt(r, 2);
	DB_GetFieldString(r, 3, s[bsOwner], MAX_PLAYER_NAME);
	s[bsIssued] = DB_GetFieldInt(r, 4);
	s[bsTill] = DB_GetFieldInt(r, 5);
	s[bsWeek] = DB_GetFieldInt(r, 6);
	s[bsEarned] = DB_GetFieldInt(r, 7);
	s[bsStock] = DB_GetFieldInt(r, 8);
	s[bsStockMax] = DB_GetFieldInt(r, 9);
	s[bsMarkup] = DB_GetFieldInt(r, 10);
	DB_GetFieldString(r, 11, dstr, sizeof dstr);
	DB_FreeResultSet(r);
	format(s[bsCode], 8, "%s", code);
	// дни через запятую: сегодня первым
	new k = 0, v = 0, bool:any = false;
	for (new i = 0; k < BIZS_DAYS; i++)
	{
		new ch = dstr[i];
		if (ch >= '0' && ch <= '9') { v = v * 10 + (ch - '0'); any = true; }
		else
		{
			if (any) s[bsDays][k++] = v;
			v = 0; any = false;
			if (ch == EOS) break;
		}
	}
	while (k < BIZS_DAYS) s[bsDays][k++] = 0;
	BizS_Send(playerid, s, false, true);
	return true;
}
