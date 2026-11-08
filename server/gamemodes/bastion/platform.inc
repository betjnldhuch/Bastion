// =====================================================================
//  platform.inc — определение платформы игрока: ПК или телефон.
//
//  Основано на platform_detect.pwn владельца сервера. Как работает:
//  при входе сервер шлёт клиенту проверку SendClientCheck. ПК-клиент
//  SA-MP на неё отвечает, мобильный обычно молчит. Нет ответа за 3 с —
//  считаем игрока мобильным.
//
//  В любом месте мода:  if (IsPlayerMobile[playerid]) { ... }
//                       GetPlayerPlatform(playerid) -> PLATFORM_*
//
//  ВАЖНО: это эвристика, модифицированный клиент может её обмануть.
//  Используй только для удобства (упрощённые меню, пороги), не для банов.
//
//  Античит скорости из platform_detect.pwn сюда пока НЕ перенесён:
//  мод телепортирует игроков обычным SetPlayerPos (дома, /goto, спавн),
//  и такой античит кикал бы честных игроков. Его место — ветка
//  feature/server-accounts-admin, вместе с переводом телепортов на
//  безопасную функцию.
// =====================================================================

#define PLATFORM_UNKNOWN        (0)
#define PLATFORM_PC             (1)
#define PLATFORM_MOBILE         (2)

#define PLATFORM_CHECK_TYPE     (0x48)
#define PLATFORM_CHECK_TIMEOUT  (3000)   // мс ожидания ответа клиента

new bool:IsPlayerMobile[MAX_PLAYERS];
static gPlatform[MAX_PLAYERS];
static bool:gPlatformPending[MAX_PLAYERS];
static gPlatformTimer[MAX_PLAYERS];

forward OnPlatformCheckTimeout(playerid);

stock GetPlayerPlatform(playerid)
{
	if (!IsPlayerConnected(playerid)) return PLATFORM_UNKNOWN;
	return gPlatform[playerid];
}

static Platform_Set(playerid, platform)
{
	gPlatformPending[playerid] = false;
	gPlatform[playerid] = platform;
	IsPlayerMobile[playerid] = (platform == PLATFORM_MOBILE);
	printf("[platform] id=%d %s", playerid, (platform == PLATFORM_MOBILE) ? ("mobile") : ("pc"));
}

// Вызывается из OnPlayerConnect (rp.pwn)
stock Platform_OnConnect(playerid)
{
	gPlatform[playerid] = PLATFORM_UNKNOWN;
	IsPlayerMobile[playerid] = false;

	new version[24];
	GetPlayerVersion(playerid, version, sizeof version);
	printf("[platform] id=%d client='%s'", playerid, version);

	gPlatformPending[playerid] = true;
	SendClientCheck(playerid, PLATFORM_CHECK_TYPE, 0, 0, 2);
	gPlatformTimer[playerid] = SetTimerEx("OnPlatformCheckTimeout", PLATFORM_CHECK_TIMEOUT, false, "i", playerid);
}

// Вызывается из OnPlayerDisconnect (rp.pwn)
stock Platform_OnDisconnect(playerid)
{
	if (gPlatformPending[playerid]) KillTimer(gPlatformTimer[playerid]);
	gPlatformPending[playerid] = false;
	gPlatform[playerid] = PLATFORM_UNKNOWN;
	IsPlayerMobile[playerid] = false;
}

public OnClientCheckResponse(playerid, actionid, memaddr, retndata)
{
	if (actionid == PLATFORM_CHECK_TYPE && gPlatformPending[playerid])
	{
		KillTimer(gPlatformTimer[playerid]);
		Platform_Set(playerid, PLATFORM_PC);
	}
	return 1;
}

public OnPlatformCheckTimeout(playerid)
{
	if (!IsPlayerConnected(playerid) || !gPlatformPending[playerid]) return 0;
	Platform_Set(playerid, PLATFORM_MOBILE);
	return 1;
}
