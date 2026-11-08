/*
 * platform_detect.pwn
 * Определение платформы клиента (ПК / мобильный) и мягкий античит
 * с допуском на сетевые задержки.
 *
 * ---------------------------------------------------------------------------
 * РЕКОМЕНДУЕМЫЕ СЕРВЕРНЫЕ КОМПОНЕНТЫ
 * ---------------------------------------------------------------------------
 *  1. Pawn.RakNet  - перехват входящих/исходящих пакетов и RPC.
 *                    Нужен для чтения полей ClientJoin, фильтрации битых
 *                    синк-пакетов и точечной подмены данных для мобильных.
 *  2. fixes.inc    - сборник исправлений стандартных функций (спавн,
 *                    оружие, интерьеры, диалоги). Подключать ПЕРВЫМ include.
 *  3. crashdetect  - только на тестовом сервере: трассировка ошибок AMX.
 *  4. Античит      - любой, где пороги настраиваются. Ниже пример логики:
 *                    grace-период после телепорта/спавна, порог зависит
 *                    от пинга и потерь пакетов, кик только после серии
 *                    нарушений со «затуханием» счётчика.
 *
 * ИЗВЕСТНЫЕ ОГРАНИЧЕНИЯ МОБИЛЬНОГО КЛИЕНТА (проверяйте на своей сборке):
 *  - выбор TextDraw мышью (SelectTextDraw) работает нестабильно / нет;
 *  - кастомные модели и часть стилей диалогов могут не поддерживаться;
 *  - раскладка клавиш (KEY_*) отличается: не завязывайте критичную
 *    логику на редкие сочетания клавиш, дублируйте командами/диалогами;
 *  - выше средний пинг и потери пакетов -> скачки позиции в синке.
 *
 * ВАЖНО: все методы определения платформы - ЭВРИСТИКИ. Модифицированный
 * клиент может подделать любой из признаков. Не используйте результат
 * для банов, только для адаптации интерфейса и порогов античита.
 * ---------------------------------------------------------------------------
 */

// #include <fixes>          // если используется - строго первым
#include <a_samp>
#tryinclude <Pawn.RakNet>    // необязателен; блок ниже компилируется только с ним

// В части версий a_samp.inc натив не объявлен - раскомментируйте при ошибке
// native SendClientCheck(playerid, type, memAddr, memOffset, byteCount);

forward @OnPlayerConnect(playerid);
forward @OnPlayerDisconnect(playerid, reason);
forward @OnPlayerSpawn(playerid);

// ---------------------------------------------------------------------------
// Константы
// ---------------------------------------------------------------------------
#define PLATFORM_UNKNOWN        (0)
#define PLATFORM_PC             (1)
#define PLATFORM_MOBILE         (2)

// Тип клиентской проверки, на который ПК-клиент присылает ответ,
// а мобильный - как правило, нет.
#define CLIENT_CHECK_TYPE       (0x48)
#define CLIENT_CHECK_TIMEOUT    (3000)  // мс ожидания ответа

#define AC_GRACE_AFTER_TP       (3000)  // мс игнорирования проверок после SetPlayerPos/спавна
#define AC_MAX_STRIKES          (5)     // нарушений до кика
#define AC_STRIKE_DECAY         (10000) // мс, через которые снимается 1 нарушение

// ---------------------------------------------------------------------------
// Данные игрока
// ---------------------------------------------------------------------------
enum E_PLAYER_NET
{
    pnPlatform,
    bool:pnCheckPending,
    pnCheckTimer,
    pnVersion[24],
    pnSerial[48],
    pnGraceUntil,       // GetTickCount(), до которого античит молчит
    pnLastTeleportTick, // GetTickCount() последнего серверного телепорта
    pnStrikes,
    pnLastStrikeTick,
    Float:pnLastX,
    Float:pnLastY,
    Float:pnLastZ,
    pnLastPosTick
};
static gNet[MAX_PLAYERS][E_PLAYER_NET];

// Глобальный флаг устройства: проверка в любой части мода
// через if (IsPlayerMobile[playerid]). Заполняется в OnPlayerPlatformDetected,
// обнуляется в OnPlayerConnect и OnPlayerDisconnect.
new bool:IsPlayerMobile[MAX_PLAYERS];

forward OnPlatformCheckTimeout(playerid);
forward OnPlayerPlatformDetected(playerid, platform);

// ---------------------------------------------------------------------------
// Публичный API модуля
// ---------------------------------------------------------------------------
stock GetPlayerPlatform(playerid)
{
    if (!IsPlayerConnected(playerid)) return PLATFORM_UNKNOWN;
    return gNet[playerid][pnPlatform];
}

// Время (мс) с последнего серверного телепорта; -1, если телепорта не было
stock GetPlayerLastTeleportAgo(playerid)
{
    if (gNet[playerid][pnLastTeleportTick] == 0) return -1;
    return GetTickCount() - gNet[playerid][pnLastTeleportTick];
}

// Обёртка телепорта: фиксирует время телепорта, выдаёт grace-период
// и переносит опорную точку античита в новую позицию.
// Используйте ВМЕСТО SetPlayerPos во всём моде.
stock SafeSetPlayerPos(playerid, Float:x, Float:y, Float:z)
{
    if (!IsPlayerConnected(playerid)) return 0;

    new now = GetTickCount();
    gNet[playerid][pnLastTeleportTick] = now;

    // Мобильному клиенту нужно больше времени на подгрузку мира
    new grace = AC_GRACE_AFTER_TP;
    if (IsPlayerMobile[playerid]) grace += 2000;
    // При высоком пинге добавляем ещё запас
    new ping = GetPlayerPing(playerid);
    if (ping > 200) grace += ping * 2;

    gNet[playerid][pnGraceUntil] = now + grace;
    gNet[playerid][pnLastX] = x;
    gNet[playerid][pnLastY] = y;
    gNet[playerid][pnLastZ] = z;
    gNet[playerid][pnLastPosTick] = now;
    return SetPlayerPos(playerid, x, y, z);
}

// Аналог для телепорта в транспорте и смены интерьера
stock SafeSetVehiclePos(playerid, vehicleid, Float:x, Float:y, Float:z)
{
    SafeSetPlayerPos(playerid, x, y, z);  // обновляет таймстамп и grace
    return SetVehiclePos(vehicleid, x, y, z);
}

/*
 * Пример использования в моде:
 *
 *   CMD:gethere / админ-телепорт / вход в интерьер:
 *       SetPlayerInterior(playerid, 3);
 *       SafeSetPlayerPos(playerid, 384.8, 173.8, 1008.4);
 *
 *   Своя проверка в античите:
 *       new ago = GetPlayerLastTeleportAgo(playerid);
 *       if (ago != -1 && ago < 5000) return 1; // недавно телепортирован - пропуск
 */

// ---------------------------------------------------------------------------
// Подключение: сбор признаков и запуск проверки
// ---------------------------------------------------------------------------
public @OnPlayerConnect(playerid)
{
    // Сброс состояния слота (слоты переиспользуются)
    static const reset[E_PLAYER_NET];
    gNet[playerid] = reset;
    IsPlayerMobile[playerid] = false;

    // Признак 1: строка версии клиента (только логируем - легко подделать)
    GetPlayerVersion(playerid, gNet[playerid][pnVersion], 24);

    // Признак 2: серийник клиента. У части мобильных сборок он
    // имеет фиксированный/нетипичный вид - собирайте статистику в логе,
    // прежде чем на него полагаться.
    gpci(playerid, gNet[playerid][pnSerial], 48);

    printf("[net] id=%d ver='%s' serial='%s'",
        playerid, gNet[playerid][pnVersion], gNet[playerid][pnSerial]);

    // Признак 3 (основной): клиентская проверка.
    // Ответ пришёл -> ПК. Тайм-аут -> вероятно мобильный клиент.
    gNet[playerid][pnCheckPending] = true;
    SendClientCheck(playerid, CLIENT_CHECK_TYPE, 0, 0, 2);
    gNet[playerid][pnCheckTimer] =
        SetTimerEx("OnPlatformCheckTimeout", CLIENT_CHECK_TIMEOUT, false, "i", playerid);

    return 1;
}

public OnClientCheckResponse(playerid, actionid, memaddr, retndata)
{
    if (actionid == CLIENT_CHECK_TYPE && gNet[playerid][pnCheckPending])
    {
        gNet[playerid][pnCheckPending] = false;
        KillTimer(gNet[playerid][pnCheckTimer]);
        gNet[playerid][pnPlatform] = PLATFORM_PC;
        CallLocalFunction("OnPlayerPlatformDetected", "ii", playerid, PLATFORM_PC);
    }
    return 1;
}

public OnPlatformCheckTimeout(playerid)
{
    if (!IsPlayerConnected(playerid) || !gNet[playerid][pnCheckPending]) return 0;

    gNet[playerid][pnCheckPending] = false;
    gNet[playerid][pnPlatform] = PLATFORM_MOBILE;
    CallLocalFunction("OnPlayerPlatformDetected", "ii", playerid, PLATFORM_MOBILE);
    return 1;
}

public @OnPlayerDisconnect(playerid, reason)
{
    if (gNet[playerid][pnCheckPending]) KillTimer(gNet[playerid][pnCheckTimer]);
    gNet[playerid][pnCheckPending] = false;
    gNet[playerid][pnPlatform] = PLATFORM_UNKNOWN;
    gNet[playerid][pnLastTeleportTick] = 0;

    // Авто-обнуление флага: слот получит следующий игрок
    IsPlayerMobile[playerid] = false;
    return 1;
}

// Точка адаптации: здесь включайте упрощённый UI, меняйте пороги и т.п.
public OnPlayerPlatformDetected(playerid, platform)
{
    // Глобальный флаг для остального кода мода
    IsPlayerMobile[playerid] = (platform == PLATFORM_MOBILE);

    printf("[net] id=%d platform=%s", playerid,
        (platform == PLATFORM_MOBILE) ? ("mobile") : ("pc"));

    if (IsPlayerMobile[playerid])
    {
        // Пример: не использовать SelectTextDraw, давать меню диалогом
        SendClientMessage(playerid, 0xA9C4E4FF,
            "Мобильный режим: меню доступно через /menu");
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Спавн: сразу выдаём grace-период, чтобы античит не реагировал
// на подгрузку мира у медленного клиента
// ---------------------------------------------------------------------------
public @OnPlayerSpawn(playerid)
{
    SafeSetPlayerPos(playerid, 0.0, 0.0, 3.0);
    return 1;
}

// ---------------------------------------------------------------------------
// Античит скорости/телепорта с допуском на задержки
// ---------------------------------------------------------------------------
static Float:AC_GetSpeedLimit(playerid)
{
    // Базовый предел, м/с, для пешехода с запасом
    new Float:limit = 25.0;

    // Транспорт - отдельный предел
    if (IsPlayerInAnyVehicle(playerid)) limit = 90.0;

    // Поправка на пинг: чем выше задержка, тем больше «рывок» между пакетами
    new ping = GetPlayerPing(playerid);
    if (ping > 150) limit *= 1.0 + float(ping - 150) / 300.0;

    // Поправка на потери пакетов
    new Float:loss = NetStats_PacketLossPercent(playerid);
    if (loss > 2.0) limit *= 1.0 + loss / 20.0;

    // Мобильный клиент - дополнительный запас
    if (IsPlayerMobile[playerid]) limit *= 1.3;

    return limit;
}

static AC_AddStrike(playerid, const reason[])
{
    new now = GetTickCount();

    // «Затухание»: за каждые AC_STRIKE_DECAY мс без нарушений снимаем одно
    new decay = (now - gNet[playerid][pnLastStrikeTick]) / AC_STRIKE_DECAY;
    gNet[playerid][pnStrikes] -= decay;
    if (gNet[playerid][pnStrikes] < 0) gNet[playerid][pnStrikes] = 0;

    gNet[playerid][pnStrikes]++;
    gNet[playerid][pnLastStrikeTick] = now;

    printf("[ac] id=%d %s strikes=%d", playerid, reason, gNet[playerid][pnStrikes]);

    if (gNet[playerid][pnStrikes] >= AC_MAX_STRIKES)
    {
        SendClientMessage(playerid, 0xFF6347FF, "Отключение: подозрительная синхронизация.");
        // Кик откладываем, чтобы сообщение успело дойти до клиента
        SetTimerEx("AC_DelayedKick", 500, false, "i", playerid);
    }
}

forward AC_DelayedKick(playerid);
public AC_DelayedKick(playerid)
{
    Kick(playerid);
    return 1;
}

public OnPlayerUpdate(playerid)
{
    if (GetPlayerState(playerid) == PLAYER_STATE_SPECTATING) return 1;

    new now = GetTickCount();
    new Float:x, Float:y, Float:z;
    GetPlayerPos(playerid, x, y, z);

    // В grace-периоде только обновляем опорную точку
    if (now < gNet[playerid][pnGraceUntil] || gNet[playerid][pnLastPosTick] == 0)
    {
        gNet[playerid][pnLastX] = x;
        gNet[playerid][pnLastY] = y;
        gNet[playerid][pnLastZ] = z;
        gNet[playerid][pnLastPosTick] = now;
        return 1;
    }

    new dt = now - gNet[playerid][pnLastPosTick];
    if (dt < 250) return 1; // слишком частые пакеты не оцениваем - шум

    new Float:dist = VectorSize(
        x - gNet[playerid][pnLastX],
        y - gNet[playerid][pnLastY],
        z - gNet[playerid][pnLastZ]);

    new Float:speed = dist / (float(dt) / 1000.0);

    if (speed > AC_GetSpeedLimit(playerid))
    {
        AC_AddStrike(playerid, "speed");
    }

    gNet[playerid][pnLastX] = x;
    gNet[playerid][pnLastY] = y;
    gNet[playerid][pnLastZ] = z;
    gNet[playerid][pnLastPosTick] = now;
    return 1;
}

// ---------------------------------------------------------------------------
// Опционально: чтение полей входящего RPC подключения через Pawn.RakNet.
// Сигнатуры зависят от версии плагина - сверьте с его include.
// Используется только для логирования/статистики.
// ---------------------------------------------------------------------------
#if defined PAWNRAKNET_INC_
const RPC_CLIENT_JOIN = 25;

IRPC:RPC_CLIENT_JOIN(playerid, BitStream:bs)
{
    new version, mod, nameLen, name[MAX_PLAYER_NAME + 1], challenge;
    new authLen, auth[48], verLen, clientVer[32];

    BS_ReadValue(bs,
        PR_INT32,  version,
        PR_UINT8,  mod,
        PR_UINT8,  nameLen);
    if (nameLen > MAX_PLAYER_NAME) return 0;           // битый пакет - отбрасываем
    BS_ReadValue(bs,
        PR_STRING, name, nameLen,
        PR_UINT32, challenge,
        PR_UINT8,  authLen);
    if (authLen >= sizeof auth) return 0;
    BS_ReadValue(bs,
        PR_STRING, auth, authLen,
        PR_UINT8,  verLen);
    if (verLen >= sizeof clientVer) return 0;
    BS_ReadValue(bs, PR_STRING, clientVer, verLen);

    printf("[rpc] join id=%d netver=%d mod=%d clientver='%s'",
        playerid, version, mod, clientVer);

    return 1; // 1 - пропустить пакет дальше
}
#endif
