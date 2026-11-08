// =====================================================================
//  keys.inc — управление кнопками (как на популярных RP-серверах).
//
//  Пешком:
//    ALT — действие рядом: вход в здание (LSPD), дверь дома, покупка дома, банкомат, бизнес,
//          раздевалка фракции, работа, автосалон, автошкола, канистра на АЗС, шлагбаум.
//          Если рядом ничего нет, но перед тобой игрок — меню взаимодействия.
//          Если держишь игрока на прицеле (правая кнопка мыши) — меню для него.
//    Y   — принять предложение / ответить на звонок
//    N   — отказаться / сбросить звонок
//  В машине (за рулём):
//    2   — двигатель
//    H   — на АЗС заправка, на СТО меню СТО, иначе шлагбаум или ворота своей фракции
//
//  Сервер видит только игровые кнопки GTA, поэтому E, L, цифры 1-9 и т.п.
//  использовать нельзя. Все команды (/enter, /bank, ...) тоже работают.
// =====================================================================

#define INTERACT_RANGE      2.0     // как близко должен стоять игрок для меню

// Пункты меню взаимодействия
enum
{
	ACT_PAY,
	ACT_NUMBER,
	ACT_HANDSHAKE,
	ACT_INVITE,
	ACT_CUFF,
	ACT_UNCUFF,
	ACT_FRISK,
	ACT_HEAL
}
#define MAX_ACTS            8

static gInteractTarget[MAX_PLAYERS] = { INVALID_PLAYER_ID, ... };
static gInteractTargetDb[MAX_PLAYERS];
static gInteractActs[MAX_PLAYERS][MAX_ACTS];
static gBizMenuActs[MAX_PLAYERS][4];

// Нажата ли кнопка именно сейчас (а не держится)
stock bool:KeyPressed(newkeys, oldkeys, key)
{
	return ((newkeys & key) == key && (oldkeys & key) != key);
}

// Игрок перед нами (в пределах range и примерно по направлению взгляда)
stock Keys_GetPlayerInFront(playerid, Float:range)
{
	new aim = GetPlayerTargetPlayer(playerid);
	if (aim != INVALID_PLAYER_ID && IsPlayerConnected(aim) && Player[aim][pLogged] && IsPlayerNearPlayer(playerid, aim, 5.0)) return aim;

	new Float:px, Float:py, Float:pz, Float:a;
	GetPlayerPos(playerid, px, py, pz);
	GetPlayerFacingAngle(playerid, a);
	new Float:fx = -floatsin(a, degrees), Float:fy = floatcos(a, degrees);   // куда смотрит игрок

	new best = INVALID_PLAYER_ID, Float:bestDist = range + 1.0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (i == playerid || !IsPlayerConnected(i) || !Player[i][pLogged]) continue;
		if (!IsPlayerNearPlayer(playerid, i, range)) continue;
		new Float:x, Float:y, Float:z;
		GetPlayerPos(i, x, y, z);
		new Float:dx = x - px, Float:dy = y - py;
		new Float:dist = floatsqroot(dx * dx + dy * dy);
		if (dist < 0.1) dist = 0.1;
		// косинус угла между взглядом и направлением на игрока: > 0.5 значит ±60°
		if ((dx * fx + dy * fy) / dist < 0.5) continue;
		if (dist < bestDist)
		{
			best = i;
			bestDist = dist;
		}
	}
	return best;
}

// ---------------------------------------------------------------------
//  Меню взаимодействия с игроком
// ---------------------------------------------------------------------
stock Keys_ShowInteract(playerid, target)
{
	new list[512], n = 0;
	new f = Player[playerid][pFaction];

	gInteractActs[playerid][n++] = ACT_PAY;       strcat(list, "Передать деньги\n");
	gInteractActs[playerid][n++] = ACT_NUMBER;    strcat(list, "Показать свой номер телефона\n");
	gInteractActs[playerid][n++] = ACT_HANDSHAKE; strcat(list, "Пожать руку\n");
	if (Faction_IsValid(f) && Player[playerid][pRank] >= MAX_RANK - 1 && Player[target][pFaction] == FACTION_NONE)
	{
		gInteractActs[playerid][n++] = ACT_INVITE; strcat(list, "Пригласить во фракцию\n");
	}
	if (f == FACTION_POLICE && Player[playerid][pDuty])
	{
		if (Player[target][pCuffed]) { gInteractActs[playerid][n++] = ACT_UNCUFF; strcat(list, "{FF6347}Снять наручники\n"); }
		else { gInteractActs[playerid][n++] = ACT_CUFF; strcat(list, "{FF6347}Надеть наручники\n"); }
		gInteractActs[playerid][n++] = ACT_FRISK; strcat(list, "{FF6347}Обыскать\n");
	}
	if (f == FACTION_EMS && Player[playerid][pDuty])
	{
		gInteractActs[playerid][n++] = ACT_HEAL; strcat(list, "{FF8080}Предложить лечение\n");
	}
	for (new i = n; i < MAX_ACTS; i++) gInteractActs[playerid][i] = -1;

	gInteractTarget[playerid] = target;
	gInteractTargetDb[playerid] = Player[target][pDbId];
	ShowPlayerDialog(playerid, DIALOG_INTERACT, DIALOG_STYLE_LIST, GetRPName(target), "%s", "Выбрать", "Закрыть", list);
}

// Меню бизнеса (ALT у бизнеса)
stock Keys_ShowBizMenu(playerid, b)
{
	new list[256], n = 0;
	gBizMenuActs[playerid][n++] = 0; strcat(list, "Купить товар\n");
	if (Biz[b][bOwner] == 0) { gBizMenuActs[playerid][n++] = 1; strcat(list, "Купить этот бизнес\n"); }
	if (Biz[b][bOwner] != 0 && Biz[b][bOwner] == Player[playerid][pDbId])
	{
		gBizMenuActs[playerid][n++] = 2; strcat(list, "Забрать деньги из кассы\n");
		gBizMenuActs[playerid][n++] = 3; strcat(list, "Информация\n");
	}
	if (n == 1) { Cmd_Buy(playerid); return; }      // чужой бизнес — сразу магазин
	for (new i = n; i < 4; i++) gBizMenuActs[playerid][i] = -1;
	ShowPlayerDialog(playerid, DIALOG_BIZ_MENU, DIALOG_STYLE_LIST, Biz[b][bName], "%s", "Выбрать", "Закрыть", list);
}

// Ответы на окна этого модуля (из OnDialogResponse)
stock Keys_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	switch (dialogid)
	{
		case DIALOG_CONFIRM_HOUSE:
		{
			if (response) Cmd_BuyHouse(playerid);
			return 1;
		}
		case DIALOG_BIZ_MENU:
		{
			if (!response || listitem < 0 || listitem >= 4) return 1;
			switch (gBizMenuActs[playerid][listitem])
			{
				case 0: Cmd_Buy(playerid);
				case 1: Cmd_BuyBiz(playerid);
				case 2: Cmd_BizCash(playerid);
				case 3: Cmd_BizInfo(playerid);
			}
			return 1;
		}
		case DIALOG_INTERACT, DIALOG_INTERACT_PAY:
		{
			if (!response) return 1;
			new target = gInteractTarget[playerid];
			if (target == INVALID_PLAYER_ID || !IsPlayerConnected(target) || Player[target][pDbId] != gInteractTargetDb[playerid])
			{
				SendClientMessage(playerid, COLOR_GREY, "Игрок ушёл.");
				return 1;
			}
			new params[32];
			if (dialogid == DIALOG_INTERACT_PAY)
			{
				if (!IsNumeric(inputtext))
				{
					SendClientMessage(playerid, COLOR_GREY, "Нужно ввести число.");
					return 1;
				}
				format(params, sizeof params, "%d %d", target, strval(inputtext));
				Commands_Pay(playerid, params);    // commands.inc
				return 1;
			}
			if (listitem < 0 || listitem >= MAX_ACTS) return 1;
			format(params, sizeof params, "%d", target);
			switch (gInteractActs[playerid][listitem])
			{
				case ACT_PAY:
				{
					ShowPlayerDialog(playerid, DIALOG_INTERACT_PAY, DIALOG_STYLE_INPUT, "Передать деньги",
						"{FFFFFF}Сколько передать игроку %s?\nУ тебя: $%d", "Передать", "Отмена", GetRPName(target), GetMoney(playerid));
				}
				case ACT_NUMBER:
				{
					if (Player[playerid][pPhone] == 0)
					{
						SendClientMessage(playerid, COLOR_GREY, "У тебя нет телефона.");
						return 1;
					}
					SendClientMessage(target, COLOR_WHITE, "%s показывает тебе свой номер: %d", GetRPName(playerid), Player[playerid][pPhone]);
					SendClientMessage(playerid, COLOR_WHITE, "Ты показал свой номер %s.", GetRPName(target));
				}
				case ACT_HANDSHAKE: Faction_OfferHandshake(playerid, target);
				case ACT_INVITE:    Cmd_Invite(playerid, params);
				case ACT_CUFF:      Cmd_Cuff(playerid, params);
				case ACT_UNCUFF:    Cmd_Uncuff(playerid, params);
				case ACT_FRISK:     Cmd_Frisk(playerid, params);
				case ACT_HEAL:      Cmd_Heal(playerid, params);
			}
			return 1;
		}
	}
	return 0;
}

// ---------------------------------------------------------------------
//  ALT пешком: что рядом — то и делаем
// ---------------------------------------------------------------------
stock Keys_Interact(playerid)
{
	// 0. Вход/выход в здание с интерьером (doors.inc)
	if (Doors_Try(playerid)) return;

	// 1. Внутри дома у двери — выйти
	new h = Player[playerid][pHouseInside];
	if (h != -1 && IsPlayerInRangeOfPoint(playerid, 2.0, House[h][hIX], House[h][hIY], House[h][hIZ]))
	{
		Cmd_Exit(playerid);
		return;
	}
	// 1а. Внутри своего дома — меню дома (шкаф, сейф, налог, аренда)
	if (House_TryMenu(playerid)) return;

	// 2. Дверь дома: продаётся — предложить купить, иначе войти
	h = House_GetNearby(playerid);
	if (h != -1)
	{
		if (House[h][hOwner] == 0)
		{
			ShowPlayerDialog(playerid, DIALOG_CONFIRM_HOUSE, DIALOG_STYLE_MSGBOX, "Покупка дома",
				"{FFFFFF}Дом №%d продаётся.\nЦена: {FFFF00}$%d\n\n{FFFFFF}Купить?", "Купить", "Отмена", House[h][hDbId], House[h][hPrice]);
		}
		else Cmd_Enter(playerid);
		return;
	}

	// 3. Банкомат
	if (Atm_GetNearby(playerid) != -1)
	{
		Bank_ShowMenu(playerid);
		return;
	}

	// 4. Бизнес
	new b = Biz_GetNearby(playerid);
	if (b != -1)
	{
		Keys_ShowBizMenu(playerid, b);
		return;
	}

	// 5. Раздевалка своей фракции
	new f = Player[playerid][pFaction];
	if (Faction_IsValid(f) && IsPlayerInRangeOfPoint(playerid, FACTION_DUTY_RANGE, Faction[f][fDutyX], Faction[f][fDutyY], Faction[f][fDutyZ]))
	{
		Cmd_Duty(playerid);
		return;
	}

	// 6. Работа и автосалон
	if (Jobs_GetNearbyHQ(playerid) != JOB_NONE)
	{
		Cmd_GetJob(playerid);
		return;
	}
	if (IsPlayerInRangeOfPoint(playerid, 3.0, DEALER_X, DEALER_Y, DEALER_Z))
	{
		Cmd_BuyCar(playerid);
		return;
	}
	if (Licenses_IsNearSchool(playerid))
	{
		Cmd_Exam(playerid);
		return;
	}

	// 6а. Вещь на земле (needs.inc)
	if (Inv_TryPickup(playerid)) return;

	// 7. Штрафстоянка (garage.inc)
	if (Garage_TryImpoundLot(playerid)) return;

	// 7а. АЗС — купить канистру (fuel.inc)
	if (Fuel_TryCanisterShop(playerid)) return;

	// 8. Шлагбаум пешком
	if (Gate_GetNearby(playerid, 4.0) != -1)
	{
		Gates_TryOpen(playerid);
		return;
	}

	// 9. Игрок перед нами
	new target = Keys_GetPlayerInFront(playerid, INTERACT_RANGE);
	if (target != INVALID_PLAYER_ID) Keys_ShowInteract(playerid, target);
}

// Из OnPlayerKeyStateChange
stock Keys_OnKeyStateChange(playerid, newkeys, oldkeys)
{
	if (!Player[playerid][pLogged]) return 0;
	new PLAYER_STATE:st = GetPlayerState(playerid);

	if (st == PLAYER_STATE_ONFOOT)
	{
		if (Player[playerid][pCuffed]) return 0;
		if (KeyPressed(newkeys, oldkeys, _:KEY_WALK))
		{
			Keys_Interact(playerid);
			return 1;
		}
	}
	else if (st == PLAYER_STATE_DRIVER)
	{
		if (KeyPressed(newkeys, oldkeys, _:KEY_SUBMISSION))
		{
			Cmd_Engine(playerid);
			return 1;
		}
		if (KeyPressed(newkeys, oldkeys, _:KEY_CROUCH))     // H в машине (это ещё и гудок)
		{
			// на АЗС — заправка, на СТО — меню СТО, иначе шлагбаум
			if (!Fuel_TryFill(playerid) && !Garage_TryOpen(playerid)) Gates_TryOpen(playerid);
			return 1;
		}
	}

	// Y / N работают и пешком, и в машине
	if (KeyPressed(newkeys, oldkeys, _:KEY_YES))
	{
		if (Player[playerid][pCallState] == CALL_INCOMING) Cmd_PickUp(playerid);
		else if (Player[playerid][pOfferType] != OFFER_NONE) Cmd_Accept(playerid);
		else if (st == PLAYER_STATE_ONFOOT) Cmd_Inv(playerid);      // Y пешком — сумка
		return 1;
	}
	if (KeyPressed(newkeys, oldkeys, _:KEY_NO))
	{
		if (Player[playerid][pCallState] != CALL_NONE) Cmd_HangUp(playerid);
		else if (Player[playerid][pOfferType] != OFFER_NONE) Cmd_Deny(playerid);
		return 1;
	}
	return 0;
}
