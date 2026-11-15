// =====================================================================
//  dialogs.inc — номера всех окон (диалогов) сервера в одном месте.
//
//  У каждого окна должен быть свой номер, иначе ответ игрока попадёт
//  в чужой модуль (так ломался /help). Раньше номера были разбросаны
//  по модулям, и их легко было задать одинаковыми. Теперь они идут
//  подряд в одном списке и совпасть не могут.
//
//  Новое окно — новая строчка В КОНЕЦ своей группы. Группы не
//  перемешивай: модули проверяют «от первого окна группы до последнего».
// =====================================================================

enum
{
	DIALOG_NONE,

	// --- /orgs (orgs.inc): от DIALOG_ORGS до DIALOG_ORGS_FAMDIS
	DIALOG_ORGS,
	DIALOG_ORGS_F,
	DIALOG_ORGS_INFO,
	DIALOG_ORGS_FSPEC,
	DIALOG_ORGS_FSRANK,
	DIALOG_ORGS_FDEPT,
	DIALOG_ORGS_FDIS,
	DIALOG_ORGS_FAMS,
	DIALOG_ORGS_FAMDIS,

	// --- Аккаунт, банк, бизнес, действия с игроком
	DIALOG_REGISTER,
	DIALOG_LOGIN,
	DIALOG_GENDER,
	DIALOG_RULES,
	DIALOG_REFERRAL,
	DIALOG_BUYCAR,
	DIALOG_HELP,
	DIALOG_BANK,
	DIALOG_BANK_DEPOSIT,
	DIALOG_BANK_WITHDRAW,
	DIALOG_BANK_TRANSFER,
	DIALOG_BANK_TRANSFER_ACC,
	DIALOG_BANK_HISTORY,
	DIALOG_BANK_ACCOUNTS,
	DIALOG_BANK_ACC_MENU,
	DIALOG_BANK_ACC_IN,
	DIALOG_BANK_ACC_OUT,
	DIALOG_BIZ_BUY,
	DIALOG_BIZ_MENU,
	DIALOG_CONFIRM_HOUSE,
	DIALOG_INTERACT,
	DIALOG_INTERACT_PAY,

	// --- Фракции (factions.inc): от DIALOG_LMENU до DIALOG_FACTION_LAST
	DIALOG_LMENU,
	DIALOG_FBANK,
	DIALOG_FBANK_DEPOSIT,
	DIALOG_FBANK_WITHDRAW,
	DIALOG_FSTORE,
	DIALOG_FSTORE_BUY,
	DIALOG_FSTORE_BUY_QTY,
	DIALOG_FSTORE_RANK,
	DIALOG_LMEMBERS,
	DIALOG_LMEMBER_ACT,
	DIALOG_LMEMBER_RANK,
	DIALOG_LMEMBER_WARN,
	DIALOG_LRANKS,
	DIALOG_LRANK_ACT,
	DIALOG_LRANK_NAME,
	DIALOG_LRANK_SALARY,
	DIALOG_LRANK_SKIN,
	DIALOG_LLOGS,
	DIALOG_FM_INFO,         // «Управление» (fmanage.inc)
	DIALOG_FM_RANKS,
	DIALOG_FM_RANKPERMS,
	DIALOG_FM_PERSONAL,
	DIALOG_FM_SQUAD,
	DIALOG_FM_NOTE,
	DIALOG_FM_LOGMENU,
	DIALOG_FM_LOGNAME,
	DIALOG_FACTION_LAST,

	// --- Топливо (fuel.inc)
	DIALOG_FUEL_FILL,
	DIALOG_FUEL_CAN,

	// --- Автошкола (licenses.inc)
	DIALOG_LIC_MENU,
	DIALOG_LIC_THEORY,

	// --- Работы (jobs.inc)
	DIALOG_JOB_OFFER,           // предложение механика игроку

	// --- Сумка (needs.inc): от DIALOG_INVENTORY до DIALOG_INV_UI
	DIALOG_INVENTORY,
	DIALOG_INV_ACTION,
	DIALOG_INV_GIVE,
	DIALOG_INV_DROP,
	DIALOG_INV_UI,               // HTML-окно сумки (ui.inc)

	// --- Дом (houses.inc): от DIALOG_HOUSE_MENU до DIALOG_HOUSE_RENT
	DIALOG_HOUSE_MENU,
	DIALOG_HOUSE_TAKEGUN,
	DIALOG_HOUSE_SAFE_IN,
	DIALOG_HOUSE_SAFE_OUT,
	DIALOG_HOUSE_PAY,
	DIALOG_HOUSE_WITHDRAW,
	DIALOG_HOUSE_RENT,

	// --- СТО и штрафстоянка (garage.inc): от DIALOG_TUNE до DIALOG_UNIMPOUND
	DIALOG_TUNE,
	DIALOG_TUNE_PAINT,
	DIALOG_TUNE_WHEELS,
	DIALOG_UNIMPOUND,

	// --- Разработка (dev.inc)
	DIALOG_DEV_POINTS,
	DIALOG_DEV_HELP,

	// --- Перекраска объектов (textures.inc): от DIALOG_TEX_SLOTS до DIALOG_TEX_TEXT, затем справка
	DIALOG_TEX_SLOTS,
	DIALOG_TEX_PICK,
	DIALOG_TEX_COLOR,
	DIALOG_TEX_TEXT,
	DIALOG_TEX_HELP,

	// --- Маркеры и GPS (gps.inc): от DIALOG_MK_MENU до DIALOG_GPS_LIST
	DIALOG_MK_MENU,
	DIALOG_MK_STYLE,
	DIALOG_MK_ICON,
	DIALOG_MK_GPS,
	DIALOG_MK_NAME,
	DIALOG_MK_LABEL,
	DIALOG_MK_NEW,
	DIALOG_MK_LIST,
	DIALOG_MK_PLACE,
	DIALOG_MK_DELETE,
	DIALOG_MK_FACTION,
	DIALOG_GPS_MAIN,
	DIALOG_GPS_LIST,

	// --- Администрация (admin.inc): от DIALOG_ADMIN_INFO до DIALOG_REPORT_NEW
	DIALOG_ADMIN_INFO,          // просто сообщение (бан, кик, справка)
	DIALOG_AMENU,               // список игроков
	DIALOG_AMENU_ACT,           // действия с игроком
	DIALOG_AMENU_INPUT,         // ввод срока и причины
	DIALOG_REPORTS,             // список репортов
	DIALOG_REPORT_ACT,          // действия с репортом
	DIALOG_REPORT_ANSWER,       // ввод ответа
	DIALOG_REPORT_NEW,          // игрок пишет репорт

	// --- Вход в админку (admin_auth.inc)
	DIALOG_ALOGIN,              // ввод админ-пароля
	DIALOG_ALOGIN_NEW,          // первый вход: придумать админ-пароль
	DIALOG_APASS_OLD,           // /apass: текущий пароль
	DIALOG_APASS_NEW,           // /apass: новый пароль

	// --- Инструменты админов (admin_tools.inc, admin.inc)
	DIALOG_TPLIST,
	DIALOG_VEH_CAT,
	DIALOG_VEH_LIST,

	// --- Ранения (wounded.inc)
	DIALOG_WOUNDS,

	// --- Помощь (help.inc)
	DIALOG_HELP_TOPICS,
	DIALOG_HELP_TOPIC,

	// --- Выбор места появления (spawns.inc)
	DIALOG_SETSPAWN,

	// --- Цвет фракции (/fcolor, factions.inc)
	DIALOG_FCOLOR,
	DIALOG_FCOLOR_HEX,

	// --- Чёрный рынок (/bm, factions.inc)
	DIALOG_BM,

	// --- Заводы, материалы, крафт (factions.inc)
	DIALOG_CRAFT,
	DIALOG_CRAFT_QTY,
	DIALOG_FSTOREPT,
	DIALOG_FSTOREPT_PUT,
	DIALOG_FCLOSE,
	DIALOG_FMANAGE,
	DIALOG_FTRANSFER,
	DIALOG_LOCKER,

	// --- Новые системы: наркотики, казино, лотерея, больница
	DIALOG_DRUG_DEALER,
	DIALOG_CASINO,
	DIALOG_CASINO_PICK,
	DIALOG_CASINO_BET,
	DIALOG_LOTTO,
	DIALOG_HOSPITAL,

	// --- Донат (donate.inc)
	DIALOG_DONATE,
	DIALOG_DONATE_MONEY,
	DIALOG_DONATE_INPUT,
	DIALOG_DONATE_UI,            // HTML-окно доната (ui.inc)

	// --- Аренда мопедов (rental.inc)
	DIALOG_RENTAL,

	// --- Окно семьи (/family, families.inc): от DIALOG_FAM_MENU до DIALOG_FAM_LEAVE
	DIALOG_FAM_MENU,
	DIALOG_FAM_BACK,
	DIALOG_FAM_ONLINE,
	DIALOG_FAM_ALL,
	DIALOG_FAM_ACT,
	DIALOG_FAM_RANKSET,
	DIALOG_FAM_BANK,
	DIALOG_FAM_BANK_IN,
	DIALOG_FAM_BANK_OUT,
	DIALOG_FAM_INVITE,
	DIALOG_FAM_RANKS,
	DIALOG_FAM_RANKNAME,
	DIALOG_FAM_MANAGE,      // «Управление» семьёй
	DIALOG_FAM_INFO,
	DIALOG_FAM_PRANKS,
	DIALOG_FAM_PRANK,
	DIALOG_FAM_PPERMS,
	DIALOG_FAM_WARN,
	DIALOG_FAM_NOTE,
	DIALOG_FAM_LOGMENU,
	DIALOG_FAM_LOGNAME,
	DIALOG_FAM_LOG,
	DIALOG_FAM_LEAVE,

	// --- Окно фракции (/fr, /members, factions.inc): от DIALOG_FR_MENU до DIALOG_FR_LEAVE
	DIALOG_FR_MENU,
	DIALOG_FR_BACK,
	DIALOG_FR_ONLINE,
	DIALOG_FR_LEAVE,

	// --- /setspawn > Организация и /fspawnaccess (spawns.inc)
	DIALOG_SETSPAWN_F,
	DIALOG_FSPAWN_ACCESS
}
