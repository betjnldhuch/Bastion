// =====================================================================
//  config.inc — все настройки сервера в одном месте.
//  Меняй цифры здесь, а не по всему коду.
// =====================================================================

#define SERVER_NAME         "Bastion Online"
#define MODE_TEXT           "Bastion Online v1.1"
// Пока Peace и War работают на одном и том же моде без различий.
// Название сервера в списке задаётся в config.json ("name").

// Захват районов банд разрешён с CAPTURE_HOUR_FROM:00 до CAPTURE_HOUR_TO:00
#define CAPTURE_HOUR_FROM   Cfg(CFG_CAPTURE_FROM) // по умолчанию 16, /srvset
#define CAPTURE_HOUR_TO     Cfg(CFG_CAPTURE_TO) // по умолчанию 24, /srvset
#define DATABASE_FILE       "rp.db"          // лежит в папке scriptfiles

// Требовать ник формата Имя_Фамилия (true / false)
#define REQUIRE_RP_NAME     true

// FPS игроков (fps.inc). 0 — без ограничений (по умолчанию).
// Например 165 — у кого FPS выше 165 дольше 30 секунд, того кикнет.
// Сам FPS игрок выбирает в лаунчере (Настройки > Лимит FPS).
#define FPS_MAX_ALLOWED     0

// Деньги и прогресс
#define START_MONEY_DEF     5000             // деньги новичка (меняется в /srvset)
#define START_MONEY         Cfg(CFG_START_MONEY)
#define REFERRAL_REWARD     Cfg(CFG_REFERRAL_REWARD) // пригласившему за новичка (referral.inc) (по умолчанию 5000, /srvset)
#define REFERRAL_LEVEL      Cfg(CFG_REFERRAL_LEVEL) // ...когда новичок дойдёт до этого уровня (по умолчанию 3, /srvset)
#define PAYDAY_SALARY_DEF   500              // зарплата каждый час (в :00), меняется в /srvset
#define PAYDAY_SALARY       Cfg(CFG_PAYDAY)
#define EXP_PER_LEVEL       Cfg(CFG_EXP_PER_LEVEL) // опыта на уровень = уровень * это число (по умолчанию 4, /srvset)

// Безопасность входа
#define MIN_PASSWORD_LEN    6
#define MAX_PASSWORD_LEN    32
#define MAX_LOGIN_TRIES     3
#define LOGIN_LOCK_SECONDS  300             // пауза на вход с IP после стольких неверных паролей, секунд
#define LOGIN_LOCK_SLOTS    64              // сколько IP на паузе помнить одновременно

// Радиусы чата
#define CHAT_RADIUS         20.0
#define SHOUT_RADIUS        40.0

// Топливо (fuel.inc)
#define FUEL_TANK           60               // литров в баке любой машины
#define FUEL_PRICE          Cfg(CFG_FUEL_PRICE) // цена литра на новых АЗС (у каждой АЗС можно своя: /addfuel) (по умолчанию 5, /srvset)
#define FUEL_CANISTER       Cfg(CFG_FUEL_CANISTER) // литров в канистре (по умолчанию 20, /srvset)

// Лицензии и автошкола (licenses.inc)
#define LIC_PRICE_CAR       Cfg(CFG_LIC_CAR) // по умолчанию 1500, /srvset
#define LIC_PRICE_MOTO      Cfg(CFG_LIC_MOTO) // по умолчанию 1000, /srvset
#define LIC_PRICE_BOAT      Cfg(CFG_LIC_BOAT) // по умолчанию 2000, /srvset
#define LIC_PRICE_GUN       Cfg(CFG_LIC_GUN) // по умолчанию 5000, /srvset
#define LIC_GUN_MIN_LEVEL   Cfg(CFG_LIC_GUN_LEVEL) // с какого уровня можно сдать на оружие (по умолчанию 2, /srvset)
#define LIC_TAKE_MIN_RANK   2                // с какого ранга полиция забирает лицензии (/takelic)
#define LIC_SCHOOL_X        1481.0           // автошкола — у мэрии Лос-Сантоса
#define LIC_SCHOOL_Y        -1771.3
#define LIC_SCHOOL_Z        18.8
#define LIC_CAR_SPAWN_X     1477.0           // где появляется учебная машина
#define LIC_CAR_SPAWN_Y     -1738.0
#define LIC_CAR_SPAWN_Z     13.4
#define LIC_CAR_SPAWN_A     270.0
#define LIC_CAR_MODEL       410              // Manana
#define LIC_MOTO_MODEL      461              // PCJ-600

// Фракции (factions.inc)
#define FACTION_SALARY_BASE Cfg(CFG_FSAL_BASE) // зарплата полиции и медиков на PayDay... (по умолчанию 300, /srvset)
#define FACTION_SALARY_RANK Cfg(CFG_FSAL_RANK) // ...плюс столько за каждый ранг (по умолчанию 100, /srvset)
#define HEAL_PRICE          Cfg(CFG_HEAL_PRICE) // цена лечения у медика (/heal) (по умолчанию 200, /srvset)
#define DEBT_TAX            0                // счёт долга: налоги (debts.inc)
#define DEBT_FINE           1                // счёт долга: штрафы
#define JAIL_MIN_PER_STAR   Cfg(CFG_JAIL_PER_STAR) // минут тюрьмы за одну звезду розыска (по умолчанию 5, /srvset)
#define ARREST_REWARD       Cfg(CFG_ARREST_REWARD) // премия полицейскому за одну звезду при аресте (по умолчанию 150, /srvset)

// Войны банд (turfs.inc)
#define TURF_CAPTURE_TIME   Cfg(CFG_TURF_TIME) // сколько секунд длится захват района (по умолчанию 300, /srvset)
#define TURF_COOLDOWN       Cfg(CFG_TURF_COOLDOWN) // через сколько секунд район можно атаковать снова (по умолчанию 1800, /srvset)
#define TURF_MIN_RANK       Cfg(CFG_TURF_RANK) // с какого ранга можно начать захват (/capture) (по умолчанию 3, /srvset)
#define TURF_INCOME         Cfg(CFG_TURF_INCOME) // доход на PayDay каждому члену банды онлайн — за каждый район (по умолчанию 150, /srvset)

// Банк (bank.inc)
#define BANK_MAX_TRANSFER   Cfg(CFG_BANK_MAX_TRANSFER) // максимум за один перевод (по умолчанию 1000000, /srvset)
#define BANK_MAX_BALANCE    2000000000       // максимум денег на одном счёте
#define BANK_MAX_EXTRA      5                // сколько дополнительных счетов можно открыть
#define BANK_OPEN_COST      Cfg(CFG_BANK_OPEN_COST) // цена открытия доп. счёта (списывается с основного) (по умолчанию 10000, /srvset)
#define BANK_ATM_FEE_PCT    Cfg(CFG_BANK_ATM_FEE) // комиссия за снятие в банкомате, % (0 — без комиссии) (по умолчанию 1, /srvset)
#define BANK_TRANSFER_FEE_PCT Cfg(CFG_BANK_TRANSFER_FEE) // комиссия за перевод, % (0 — без комиссии) (по умолчанию 1, /srvset)
#define BANK_INTEREST_PERMILLE Cfg(CFG_BANK_INTEREST) // проценты на остаток каждый PayDay, в десятых долях % (1 = 0.1%) (по умолчанию 1, /srvset)
#define BANK_INTEREST_MAX   Cfg(CFG_BANK_INTEREST_MAX) // максимум процентов за один PayDay (по умолчанию 100, /srvset)
#define PAYDAY_TO_BANK      true             // зарплата на счёт (true) или наличными (false)

// Телефон (phone.inc)
#define PHONE_PRICE         500              // цена телефона в магазине 24/7
#define SMS_PRICE           Cfg(CFG_SMS_PRICE) // цена одного SMS (по умолчанию 5, /srvset)

// Бизнесы (business.inc)
#define MAX_BUSINESSES      100
#define BIZ_PAYDAY_INCOME   200              // доход бизнеса в кассу каждый PayDay
#define BIZ_SALE_SHARE      50               // % от каждой покупки, который идёт в кассу владельцу

// ---------------------------------------------------------------------
//  Цвета (формат 0xRRGGBBAA)
// ---------------------------------------------------------------------
#define COLOR_WHITE         0xFFFFFFFF
#define COLOR_GREY          0xAFAFAFFF
#define COLOR_RED           0xFF6347FF
#define COLOR_GREEN         0x33AA33FF
#define COLOR_YELLOW        0xFFFF00FF
#define COLOR_ORANGE        0xFF9900FF
#define COLOR_PURPLE        0xC2A2DAFF   // /me /do
#define COLOR_BLUE          0x33CCFFFF
#define COLOR_ADMIN         0xFF66FFFF
#define COLOR_CIVIL         0xDDDDDDFF   // ник обычного жителя

// ---------------------------------------------------------------------
//  Точки на карте
// ---------------------------------------------------------------------
// Запасной спавн, пока админ не расставил вокзалы (/addspawn): вокзал Unity Station, Лос-Сантос
#define SPAWN_X             1743.0
#define SPAWN_Y             -1862.0
#define SPAWN_Z             13.58
#define SPAWN_A             0.0

// Больница, где появляешься после смерти (All Saints, Лос-Сантос)
#define HOSPITAL_X          1178.0
#define HOSPITAL_Y          -1323.5
#define HOSPITAL_Z          14.1
#define HOSPITAL_FEE        Cfg(CFG_HOSPITAL_FEE) // по умолчанию 100, /srvset

// Скины по умолчанию для регистрации
#define SKIN_MALE           60
#define SKIN_FEMALE         56

// Номера окон (диалогов) — в dialogs.inc
