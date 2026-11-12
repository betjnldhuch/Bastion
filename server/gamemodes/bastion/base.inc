// =====================================================================
//  base.inc — данные игроков и общие функции-помощники.
// =====================================================================

// Работы
enum
{
	JOB_NONE,
	JOB_LOADER,     // грузчик в порту
	JOB_PIZZA,      // развозчик пиццы
	JOB_TAXI,       // таксист
	JOB_BUS,        // водитель автобуса
	JOB_TRUCKER,    // дальнобойщик
	JOB_MECHANIC    // механик
}

// Фракции (подробности — в factions.inc)
#define MAX_FACTIONS        21          // 0 = нет фракции, 1..7 — постоянные, 8..20 — созданные в игре (/createfaction)
#define FACTION_BUILTIN     8           // с этого номера идут фракции, созданные в игре
#define MAX_RANK            10          // 10 = лидер, 9 = заместитель
enum
{
	FACTION_NONE,
	FACTION_POLICE,     // 1 — Полиция LSPD
	FACTION_EMS,        // 2 — Больница
	FACTION_GROVE,      // 3 — Grove Street Families
	FACTION_BALLAS,     // 4 — The Ballas
	FACTION_AZTECAS,    // 5 — Varrios Los Aztecas
	FACTION_FBI,        // 6 — ФБР (силовая структура, права как у полиции)
	FACTION_ARMY        // 7 — Армия (возит припасы на склады госструктур)
}
// Силовая структура: полиция, ФБР и созданные в игре фракции этого типа
// (наручники, розыск, арест, штрафы, решётки камер). Сама функция — в factions.inc.
forward bool:Faction_IsLaw(f);

// Всё, что мы знаем об игроке. Поля с "сохраняется" пишутся в базу.
enum E_PLAYER
{
	pDbId,              // номер аккаунта в базе (0 = нет аккаунта)
	bool:pLogged,       // вошёл ли игрок в аккаунт
	bool:pAuthShown,    // уже показали окно входа/регистрации
	pPassHash[65],      // хеш пароля из базы
	pSalt[17],          // "соль" пароля из базы
	pLoginTries,        // сколько раз ошибся паролем

	pMoney,             // сохраняется
	pLevel,             // сохраняется
	pExp,               // сохраняется
	pSkin,              // сохраняется
	pAdmin,             // сохраняется (0 = игрок, 1..5 = админ)
	pJob,               // сохраняется (JOB_*)
	pFaction,           // сохраняется (FACTION_*)
	pRank,              // сохраняется (1..10)
	pWanted,            // сохраняется (звёзды розыска 0..6)
	pJail,              // сохраняется (минут тюрьмы осталось)
	bool:pDuty,         // на смене во фракции (в форме)
	bool:pCuffed,       // в наручниках
	pOfferType,         // что предложили (OFFER_* в factions.inc)
	pOfferFrom,         // кто предложил (id игрока)
	pOfferFromDb,       // аккаунт того, кто предложил (защита от подмены id)
	pOfferValue,        // фракция или цена
	pBank,              // сохраняется (деньги в банке)
	pPhone,             // сохраняется (номер телефона, 0 = нет телефона)
	pCallWith,          // с кем разговор / звонок (INVALID_PLAYER_ID = нет)
	pCallState,         // 0 — нет, 1 — звоню, 2 — мне звонят, 3 — разговор

	bool:pDied,         // умер — появится в больнице
	pHouseInside,       // id дома, внутри которого стоит игрок (-1 = на улице)
	pWorking,           // идёт ли рабочая смена (0 = нет)
	pWorkStep,          // шаг работы
	pWorkVehicle        // машина, на которой работает (пицца)
}
new Player[MAX_PLAYERS][E_PLAYER];

// Сбросить данные слота игрока (вызывается при входе на сервер)
stock Player_Reset(playerid)
{
	static const empty[E_PLAYER];
	Player[playerid] = empty;
	Player[playerid][pLevel] = 1;
	Player[playerid][pSkin] = SKIN_MALE;
	Player[playerid][pHouseInside] = -1;
	Player[playerid][pWorkVehicle] = INVALID_VEHICLE_ID;
	Player[playerid][pCallWith] = INVALID_PLAYER_ID;
}

// Имя игрока для RP-сообщений: "Ivan_Petrov" -> "Ivan Petrov".
// Пример: SendClientMessage(playerid, COLOR_WHITE, "Привет, %s", GetRPName(playerid));
stock GetRPName(playerid)
{
	new name[MAX_PLAYER_NAME];
	GetPlayerName(playerid, name, sizeof name);
	for (new i = 0; name[i] != '\0'; i++)
	{
		if (name[i] == '_') name[i] = ' ';
	}
	return name;
}

// ---------------------------------------------------------------------
//  Деньги. ВСЕГДА меняй деньги только через GiveMoney — тогда
//  сервер знает точную сумму, и читерские деньги ничего не дадут.
// ---------------------------------------------------------------------
//  reason — за что (пишется в журнал денег, economy.inc: /moneylog).
stock GiveMoney(playerid, amount, const reason[] = "")
{
	Player[playerid][pMoney] += amount;
	ResetPlayerMoney(playerid);
	GivePlayerMoney(playerid, Player[playerid][pMoney]);
	Econ_LogMoney(playerid, amount, reason);
}

stock GetMoney(playerid)
{
	return Player[playerid][pMoney];
}

// ---------------------------------------------------------------------
//  Разбор параметров команды.
//  Пример: в "/pay 3 500" после имени команды params = " 3 500".
//  new pos = 0, a[16];
//  NextParam(params, pos, a);  -> a = "3"
//  NextParam(params, pos, a);  -> a = "500"
// ---------------------------------------------------------------------
stock bool:NextParam(const text[], &pos, dest[], size = sizeof (dest))
{
	new len = strlen(text);
	while (pos < len && (text[pos] == ' ' || text[pos] == '\t')) pos++;
	new start = pos;
	while (pos < len && text[pos] != ' ' && text[pos] != '\t') pos++;
	strmid(dest, text, start, pos, size);
	return (pos > start);
}

// Остаток строки после pos (для текста сообщений: /me, /pm и т.д.)
stock bool:RestParam(const text[], pos, dest[], size = sizeof (dest))
{
	new len = strlen(text);
	while (pos < len && (text[pos] == ' ' || text[pos] == '\t')) pos++;
	strmid(dest, text, pos, len, size);
	return (pos < len);
}

stock bool:IsNumeric(const text[])
{
	new i = 0;
	if (text[0] == '-') i = 1;
	if (text[i] == '\0') return false;
	for (; text[i] != '\0'; i++)
	{
		if (text[i] < '0' || text[i] > '9') return false;
	}
	return true;
}

// Прочитать ID игрока из параметров. Возвращает INVALID_PLAYER_ID, если нет такого.
stock NextPlayerParam(const text[], &pos)
{
	new buf[16];
	if (!NextParam(text, pos, buf) || !IsNumeric(buf)) return INVALID_PLAYER_ID;
	new id = strval(buf);
	if (id < 0 || id >= MAX_PLAYERS || !IsPlayerConnected(id)) return INVALID_PLAYER_ID;
	return id;
}

// Прочитать число из параметров. Возвращает false, если это не число.
stock bool:NextIntParam(const text[], &pos, &value)
{
	new buf[16];
	if (!NextParam(text, pos, buf) || !IsNumeric(buf)) return false;
	value = strval(buf);
	return true;
}

// Случайная строка из букв и цифр — для "соли" пароля
stock RandomString(dest[], length, size = sizeof (dest))
{
	static const chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	if (length >= size) length = size - 1;
	for (new i = 0; i < length; i++)
	{
		dest[i] = chars[random(sizeof (chars) - 1)];
	}
	dest[length] = '\0';
}

// Вырезать из текста цветовые коды вида {RRGGBB}
stock StripColorCodes(text[])
{
	new len = strlen(text), i = 0;
	while (i + 7 < len)
	{
		if (text[i] == '{' && text[i + 7] == '}')
		{
			new bool:hex = true;
			for (new j = i + 1; j < i + 7; j++)
			{
				new c = text[j];
				if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
				{
					hex = false;
					break;
				}
			}
			if (hex)
			{
				strdel(text, i, i + 8);
				len -= 8;
				continue;
			}
		}
		i++;
	}
}

// Есть ли в нике "_" (Имя_Фамилия)
stock bool:IsRPName(const name[])
{
	new under = strfind(name, "_");
	if (under < 1) return false;                 // нет "_" или стоит в начале
	if (name[under + 1] == '\0') return false;   // "_" в конце
	return true;
}

// Отложенный кик: чтобы игрок успел увидеть сообщение
forward DelayedKick(playerid);
public DelayedKick(playerid)
{
	Kick(playerid);
	return 1;
}

stock KickWithDelay(playerid)
{
	SetTimerEx("DelayedKick", 500, false, "i", playerid);
}

// Отправить сообщение всем игрокам рядом (локальный чат)
stock SendNearbyMessage(playerid, Float:radius, colour, const message[])
{
	new Float:x, Float:y, Float:z;
	GetPlayerPos(playerid, x, y, z);
	new world = GetPlayerVirtualWorld(playerid);
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged]) continue;
		if (GetPlayerVirtualWorld(i) != world) continue;
		if (!IsPlayerInRangeOfPoint(i, radius, x, y, z)) continue;
		SendClientMessage(i, colour, "%s", message);
	}
}

// Сообщение всем админам
stock SendAdminMessage(colour, const message[])
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i) && Player[i][pAdmin] > 0)
		{
			SendClientMessage(i, colour, "%s", message);
		}
	}
}
