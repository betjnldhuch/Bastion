// =====================================================================
//  config.inc — все настройки сервера в одном месте.
//  Меняй цифры здесь, а не по всему коду.
// =====================================================================

#define SERVER_NAME         "Bastion RP"
#define MODE_TEXT           "Bastion RP v1.0"
#define DATABASE_FILE       "rp.db"          // лежит в папке scriptfiles

// Требовать ник формата Имя_Фамилия (true / false)
#define REQUIRE_RP_NAME     true

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

// Фракции (factions.inc)
#define FACTION_SALARY_BASE 300              // зарплата полиции и медиков на PayDay...
#define FACTION_SALARY_RANK 100              // ...плюс столько за каждый ранг
#define HEAL_PRICE          200              // цена лечения у медика (/heal)
#define JAIL_MIN_PER_STAR   5                // минут тюрьмы за одну звезду розыска
#define ARREST_REWARD       150              // премия полицейскому за одну звезду при аресте

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
	DIALOG_HELP
}
