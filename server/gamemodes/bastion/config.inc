// =====================================================================
//  config.inc — все настройки сервера в одном месте.
//  Меняй цифры здесь, а не по всему коду.
// =====================================================================

#define SERVER_NAME         "Bastion Online"
#define MODE_TEXT           "Bastion Online v1.1"
// Пока Peace и War работают на одном и том же моде без различий.
// Название сервера в списке задаётся в config.json ("name").

// Захват районов банд разрешён с CAPTURE_HOUR_FROM:00 до CAPTURE_HOUR_TO:00
#define CAPTURE_HOUR_FROM   16
#define CAPTURE_HOUR_TO     24
#define DATABASE_FILE       "rp.db"          // лежит в папке scriptfiles

// Требовать ник формата Имя_Фамилия (true / false)
#define REQUIRE_RP_NAME     true

// FPS игроков (fps.inc). 0 — без ограничений (по умолчанию).
// Например 165 — у кого FPS выше 165 дольше 30 секунд, того кикнет.
// Сам FPS игрок выбирает в лаунчере (Настройки > Лимит FPS).
#define FPS_MAX_ALLOWED     0

// Деньги и прогресс
#define START_MONEY         5000             // деньги новичка
#define PAYDAY_SALARY       500              // зарплата каждый час (в :00)
#define EXP_PER_LEVEL       4                // опыта на уровень = уровень * это число

// Безопасность входа
#define MIN_PASSWORD_LEN    6
#define MAX_PASSWORD_LEN    32
#define MAX_LOGIN_TRIES     3

// Радиусы чата
#define CHAT_RADIUS         20.0
#define SHOUT_RADIUS        40.0

// Топливо (fuel.inc)
#define FUEL_TANK           60               // литров в баке любой машины
#define FUEL_PRICE          5                // цена литра на новых АЗС (у каждой АЗС можно своя: /addfuel)
#define FUEL_CANISTER       20               // литров в канистре

// Лицензии и автошкола (licenses.inc)
#define LIC_PRICE_CAR       1500
#define LIC_PRICE_MOTO      1000
#define LIC_PRICE_BOAT      2000
#define LIC_PRICE_GUN       5000
#define LIC_GUN_MIN_LEVEL   2                // с какого уровня можно сдать на оружие
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
#define FACTION_SALARY_BASE 300              // зарплата полиции и медиков на PayDay...
#define FACTION_SALARY_RANK 100              // ...плюс столько за каждый ранг
#define HEAL_PRICE          200              // цена лечения у медика (/heal)
#define JAIL_MIN_PER_STAR   5                // минут тюрьмы за одну звезду розыска
#define ARREST_REWARD       150              // премия полицейскому за одну звезду при аресте

// Войны банд (turfs.inc)
#define TURF_CAPTURE_TIME   300              // сколько секунд длится захват района
#define TURF_COOLDOWN       1800             // через сколько секунд район можно атаковать снова
#define TURF_MIN_RANK       3                // с какого ранга можно начать захват (/capture)
#define TURF_INCOME         150              // доход на PayDay каждому члену банды онлайн — за каждый район

// Банк (bank.inc)
#define BANK_MAX_TRANSFER   1000000          // максимум за один перевод

// Телефон (phone.inc)
#define PHONE_PRICE         500              // цена телефона в магазине 24/7
#define SMS_PRICE           5                // цена одного SMS

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
// Спавн новичка (Гантон, Лос-Сантос)
#define SPAWN_X             2495.3547
#define SPAWN_Y             -1688.2319
#define SPAWN_Z             13.6774
#define SPAWN_A             351.1646

// Больница, где появляешься после смерти (All Saints, Лос-Сантос)
#define HOSPITAL_X          1178.0
#define HOSPITAL_Y          -1323.5
#define HOSPITAL_Z          14.1
#define HOSPITAL_FEE        100

// Скины по умолчанию для регистрации
#define SKIN_MALE           60
#define SKIN_FEMALE         56

// ---------------------------------------------------------------------
//  ID диалогов. Каждое окно должно иметь свой номер.
// ---------------------------------------------------------------------
enum
{
	DIALOG_NONE,
	DIALOG_REGISTER,
	DIALOG_LOGIN,
	DIALOG_GENDER,
	DIALOG_BUYCAR,
	DIALOG_HELP,
	DIALOG_BANK,
	DIALOG_BANK_DEPOSIT,
	DIALOG_BANK_WITHDRAW,
	DIALOG_BANK_TRANSFER,
	DIALOG_BIZ_BUY,
	DIALOG_BIZ_MENU,
	DIALOG_CONFIRM_HOUSE,
	DIALOG_INTERACT,
	DIALOG_INTERACT_PAY
}
