// =====================================================================
//  help.inc — /help: разделы со всеми командами сервера.
//
//  Окно со списком разделов (листается колесиком), в разделе — список
//  «команда — что делает». Пока видны все разделы, в том числе админские.
//  Новая команда — строчка в gHelp ниже: { раздел, "команда", "что делает" }.
// =====================================================================

#define DIALOG_HELP_TOPICS  1250    // 1230-1231 заняты меню /veh (admin.inc)
#define DIALOG_HELP_TOPIC   1251
#define HELP_PAGE_LINES     10

static gHelpTopic[MAX_PLAYERS], gHelpPage[MAX_PLAYERS], gHelpPages[MAX_PLAYERS];

enum
{
	HT_KEYS,
	HT_GENERAL,
	HT_CHAT,
	HT_BAG,
	HT_GPS,
	HT_JOBS,
	HT_CARS,
	HT_FUEL,
	HT_GARAGE,
	HT_HOUSE,
	HT_MONEY,
	HT_BIZ,
	HT_PHONE,
	HT_FACTION,
	HT_LEADER,
	HT_POLICE,
	HT_EMS,
	HT_GANGS,
	HT_ADMIN1,
	HT_ADMIN2,
	HT_ADMIN3,
	HT_ADMIN4,
	HT_ADMIN5,
	HT_DEV,
	HT_COUNT
}

static const gHelpTopics[HT_COUNT][] =
{
	"Кнопки",
	"Общее",
	"Чат и RP",
	"Сумка, еда, одежда",
	"GPS",
	"Работа",
	"Транспорт и права",
	"Топливо",
	"СТО и штрафстоянка",
	"Дом и аренда",
	"Деньги, банк, налоги",
	"Бизнес и магазины",
	"Телефон",
	"Фракции: все",
	"Фракции: лидерам",
	"Полиция LSPD",
	"Больница EMS",
	"Банды и районы",
	"Админ 1 — хелпер",
	"Админ 2 — модератор",
	"Админ 3 — администратор",
	"Админ 4 — ст. администратор",
	"Админ 5 — гл. администратор",
	"Разработка: точки, маппинг, маркеры"
};

enum E_HELP
{
	hpTopic,
	hpCmd[36],
	hpDesc[96]
}

static const gHelp[][E_HELP] =
{
	// --- Кнопки
	{ HT_KEYS, "ALT", "действие рядом: дом, банкомат, магазин, работа, вход, вещь на земле; иначе меню игрока" },
	{ HT_KEYS, "Y", "принять предложение, ответить на звонок; иначе открыть сумку" },
	{ HT_KEYS, "N", "отказаться, сбросить звонок" },
	{ HT_KEYS, "2 (в машине)", "завести / заглушить двигатель" },
	{ HT_KEYS, "H (в машине)", "на АЗС — заправка, на СТО — меню СТО, иначе шлагбаум / ворота" },
	{ HT_KEYS, "ESC", "закрыть окно сумки на экране" },

	// --- Общее
	{ HT_GENERAL, "/help", "эта справка" },
	{ HT_GENERAL, "/stats", "твоя статистика: уровень, деньги, работа, дом, машина" },
	{ HT_GENERAL, "/time", "время на сервере" },
	{ HT_GENERAL, "/aid [id]", "поднять раненого аптечкой из сумки" },
	{ HT_GENERAL, "/dead", "раненым: сдаться и уехать в больницу" },
	{ HT_GENERAL, "/carry [id]", "понести раненого (он соглашается на Y)" },
	{ HT_GENERAL, "/fps", "твой FPS" },
	{ HT_GENERAL, "/hud", "спрятать / показать панель статуса на экране" },
	{ HT_GENERAL, "/report [текст]", "вопрос или жалоба администрации" },
	{ HT_GENERAL, "/admins", "администрация онлайн" },
	{ HT_GENERAL, "/accept", "принять предложение (то же, что Y)" },
	{ HT_GENERAL, "/deny", "отказаться от предложения (то же, что N)" },
	{ HT_GENERAL, "/enter, /exit", "войти в здание / выйти (то же, что ALT у двери)" },

	// --- Чат
	{ HT_CHAT, "текст в чат", "тебя слышат игроки рядом" },
	{ HT_CHAT, "/me [действие]", "действие от третьего лица" },
	{ HT_CHAT, "/do [описание]", "описание обстановки" },
	{ HT_CHAT, "/try [действие]", "попытка: удачно или нет" },
	{ HT_CHAT, "/s [текст]", "крик, слышно дальше" },
	{ HT_CHAT, "/b [текст]", "OOC-чат рядом (не по роли)" },
	{ HT_CHAT, "/pm [id] [текст]", "личное сообщение" },

	// --- Сумка
	{ HT_BAG, "/inv, /i, Y", "сумка на экране: клик по вещи — использовать, передать, выбросить" },
	{ HT_BAG, "/invlist", "сумка списком" },
	{ HT_BAG, "/pickup, ALT", "поднять вещь с земли" },
	{ HT_BAG, "/hunger", "сытость; поесть — в баре или едой из 24/7" },
	{ HT_BAG, "/buy", "купить товар в магазине (то же, что ALT у кассы)" },

	// --- GPS
	{ HT_GPS, "/gps", "навигатор: места по разделам, моя машина, мой дом; метка на карте и радаре" },

	// --- Работа
	{ HT_JOBS, "/getjob", "устроиться (у значка работы)" },
	{ HT_JOBS, "/work", "начать / закончить смену" },
	{ HT_JOBS, "/quitjob", "уволиться" },
	{ HT_JOBS, "/taxi", "вызвать такси" },
	{ HT_JOBS, "/gotaxi [id]", "таксисту: принять вызов" },
	{ HT_JOBS, "/repair [id]", "механику: предложить ремонт" },
	{ HT_JOBS, "/refill [id]", "механику: предложить бензин" },

	// --- Транспорт
	{ HT_CARS, "/engine", "двигатель (то же, что 2)" },
	{ HT_CARS, "/buycar", "купить машину в автосалоне" },
	{ HT_CARS, "/sellcar", "продать свою машину в автосалоне" },
	{ HT_CARS, "/park", "поставить машину на парковку здесь" },
	{ HT_CARS, "/lock", "закрыть / открыть машину или дом" },
	{ HT_CARS, "/exam", "автошкола: сдать на права и лицензии" },
	{ HT_CARS, "/licenses", "мои лицензии" },
	{ HT_CARS, "/showlic [id]", "показать лицензии игроку" },

	// --- Топливо
	{ HT_FUEL, "/fill", "заправиться на АЗС (то же, что H)" },
	{ HT_FUEL, "/fuel", "сколько бензина в баке" },
	{ HT_FUEL, "/canister", "залить канистру в машину рядом" },
	{ HT_FUEL, "/speedo", "показать / спрятать спидометр" },

	// --- СТО
	{ HT_GARAGE, "/tune", "меню СТО: ремонт, покраска, нитро, гидравлика, диски (то же, что H)" },
	{ HT_GARAGE, "/unimpound", "забрать машину со штрафстоянки (заплатить штраф)" },

	// --- Дом
	{ HT_HOUSE, "/buyhouse", "купить дом" },
	{ HT_HOUSE, "/sellhouse", "продать дом" },
	{ HT_HOUSE, "/house", "меню дома: шкаф, сейф, счёт (или ALT внутри)" },
	{ HT_HOUSE, "/housepay", "оплатить налог на дом" },
	{ HT_HOUSE, "/rent", "снять комнату в доме" },
	{ HT_HOUSE, "/unrent", "съехать" },
	{ HT_HOUSE, "/setrent [сумма]", "владельцу: сдавать комнаты (0 — не сдавать)" },
	{ HT_HOUSE, "/tenants", "владельцу: список жильцов" },
	{ HT_HOUSE, "/evict [id]", "владельцу: выселить жильца" },
	{ HT_HOUSE, "/evictall", "владельцу: выселить всех" },

	// --- Деньги
	{ HT_MONEY, "/bank, /atm", "банкомат: счёт, снять, положить, перевод, история" },
	{ HT_MONEY, "/pay [id] [сумма]", "передать деньги игроку рядом" },
	{ HT_MONEY, "/taxes", "налоги и казна штата" },

	// --- Бизнес
	{ HT_BIZ, "/buybiz", "купить бизнес" },
	{ HT_BIZ, "/sellbiz", "продать бизнес" },
	{ HT_BIZ, "/bizinfo", "информация о бизнесе" },
	{ HT_BIZ, "/rob", "ограбить 24/7 или бар (оружие в руках, 2+ уровень)" },
	{ HT_BIZ, "/bizcash", "забрать деньги из кассы" },

	// --- Телефон
	{ HT_PHONE, "/number", "мой номер" },
	{ HT_PHONE, "/call [номер]", "позвонить" },
	{ HT_PHONE, "/p", "ответить на звонок (то же, что Y)" },
	{ HT_PHONE, "/h", "положить трубку (то же, что N)" },
	{ HT_PHONE, "/sms [номер] [текст]", "SMS" },
	{ HT_PHONE, "/911 [что и где]", "вызвать полицию и скорую" },

	// --- Фракции
	{ HT_FACTION, "/fhelp", "краткая справка по фракциям" },
	{ HT_FACTION, "/f [текст]", "рация фракции" },
	{ HT_FACTION, "/members", "кто из фракции онлайн" },
	{ HT_FACTION, "/duty", "начать / закончить смену (в раздевалке)" },
	{ HT_FACTION, "/fleave", "уйти из фракции" },
	{ HT_FACTION, "/fbank", "общак (на базе)" },
	{ HT_FACTION, "/fstorage, /fsklad", "склад: оружие, бронежилет, аптечка (на базе)" },

	// --- Лидерам
	{ HT_LEADER, "/lmenu", "панель лидера: состав, выговоры, ранги, зарплаты, склад, журнал" },
	{ HT_LEADER, "/invite [id]", "пригласить во фракцию" },
	{ HT_LEADER, "/uninvite [id]", "выгнать из фракции" },
	{ HT_LEADER, "/giverank [id] [ранг]", "сменить ранг" },

	// --- Полиция
	{ HT_POLICE, "/su [id] [звёзды] [причина]", "объявить в розыск" },
	{ HT_POLICE, "/clear [id]", "снять розыск" },
	{ HT_POLICE, "/wanted", "список разыскиваемых" },
	{ HT_POLICE, "/cuff [id], /uncuff [id]", "наручники" },
	{ HT_POLICE, "/frisk [id]", "обыскать" },
	{ HT_POLICE, "/putcar [id]", "посадить в машину (сидя за рулём)" },
	{ HT_POLICE, "/arrest [id]", "посадить в тюрьму (у участка)" },
	{ HT_POLICE, "/drag [id]", "вести задержанного или раненого (/drag — отпустить)" },
	{ HT_POLICE, "/m [текст]", "мегафон" },
	{ HT_POLICE, "/impound [штраф]", "эвакуировать личную машину на штрафстоянку" },
	{ HT_POLICE, "/checklic [id]", "проверить лицензии" },
	{ HT_POLICE, "/takelic [id] [вид]", "забрать лицензию (1 авто, 2 мото, 3 лодки, 4 оружие)" },

	// --- EMS
	{ HT_EMS, "/heal [id]", "предложить лечение" },
	{ HT_EMS, "/revive [id]", "поднять раненого (премия $250)" },
	{ HT_EMS, "/wounds", "список раненых, метка GPS" },

	// --- Банды
	{ HT_GANGS, "/turfs", "районы банд на карте" },
	{ HT_GANGS, "/capture", "начать захват района" },

	// --- Админ 1
	{ HT_ADMIN1, "/ahelp", "команды администрации по уровням" },
	{ HT_ADMIN1, "/amenu", "панель администратора (удобно с телефона)" },
	{ HT_ADMIN1, "/a [текст]", "админ-чат" },
	{ HT_ADMIN1, "/reports", "список репортов" },
	{ HT_ADMIN1, "/ans [№] [ответ]", "ответить на репорт" },
	{ HT_ADMIN1, "/check [id]", "информация об игроке" },
	{ HT_ADMIN1, "/spec [id], /specoff", "следить за игроком (листать: /specnext /specprev, ЛКМ / ПКМ)" },
	{ HT_ADMIN1, "/goto [id]", "телепорт к игроку" },
	{ HT_ADMIN1, "/kick [id] [причина]", "кикнуть" },
	{ HT_ADMIN1, "/mute [id] [мин] [причина]", "запретить чат" },
	{ HT_ADMIN1, "/unmute [id]", "снять мут" },
	{ HT_ADMIN1, "/aduty", "админ-дежурство" },
	{ HT_ADMIN1, "/tp", "телепорт к сохранённой точке (список)" },
	{ HT_ADMIN1, "/maptp", "телепорт по метке на карте: вкл / выкл" },
	{ HT_ADMIN1, "/back", "вернуться туда, где был до телепорта" },

	// --- Админ 2
	{ HT_ADMIN2, "/warn [id] [причина]", "предупреждение" },
	{ HT_ADMIN2, "/ajail [id] [мин] [причина]", "админ-тюрьма" },
	{ HT_ADMIN2, "/unajail [id]", "выпустить из админ-тюрьмы" },
	{ HT_ADMIN2, "/freeze [id], /unfreeze [id]", "заморозить / разморозить" },
	{ HT_ADMIN2, "/slap [id]", "подкинуть" },
	{ HT_ADMIN2, "/gethere [id]", "телепортировать игрока к себе" },
	{ HT_ADMIN2, "/sethp [id] [0-100]", "здоровье" },
	{ HT_ADMIN2, "/setskin [id] [скин]", "скин" },
	{ HT_ADMIN2, "/jail [id] [мин] [причина]", "тюрьма LSPD" },
	{ HT_ADMIN2, "/unjail [id]", "выпустить из тюрьмы" },
	{ HT_ADMIN2, "/sendto [id] [точка]", "отправить игрока к точке" },
	{ HT_ADMIN2, "/gotocar [id машины]", "телепорт к машине" },
	{ HT_ADMIN2, "/fixveh [id]", "починить машину" },
	{ HT_ADMIN2, "/flip [id]", "перевернуть машину" },

	// --- Админ 3
	{ HT_ADMIN3, "/ban [id] [дни] [причина]", "бан (0 дней — навсегда, с 4 уровня)" },
	{ HT_ADMIN3, "/offban [ник] [дни] [причина]", "бан игрока не в сети" },
	{ HT_ADMIN3, "/unwarn [id]", "снять предупреждение" },
	{ HT_ADMIN3, "/alog", "журнал действий администрации" },
	{ HT_ADMIN3, "/moneylog [id] [N]", "журнал денег игрока" },
	{ HT_ADMIN3, "/veh [название или модель]", "создать машину; без параметров — меню по категориям" },
	{ HT_ADMIN3, "/dveh", "удалить машину из /veh" },
	{ HT_ADMIN3, "/ao [текст]", "объявление от администрации" },
	{ HT_ADMIN3, "/gotocoord [x] [y] [z] [инт]", "телепорт по координатам" },
	{ HT_ADMIN3, "/respawncars", "вернуть пустые машины на места" },
	{ HT_ADMIN3, "/setweather [0-45]", "погода" },
	{ HT_ADMIN3, "/givelic [id] [вид]", "выдать лицензию" },
	{ HT_ADMIN3, "/setfuel", "заправить машину" },

	// --- Админ 4
	{ HT_ADMIN4, "/unban [ник]", "разбанить" },
	{ HT_ADMIN4, "/addtp [название], /deltp", "добавить / удалить точку телепорта" },
	{ HT_ADMIN4, "/tplist", "список точек телепорта" },
	{ HT_ADMIN4, "/createhouse [цена] [тип]", "создать дом здесь" },
	{ HT_ADMIN4, "/createbiz [тип] [цена] [имя]", "создать бизнес здесь" },
	{ HT_ADMIN4, "/delbiz", "удалить бизнес рядом" },
	{ HT_ADMIN4, "/addatm, /delatm", "поставить / удалить банкомат" },
	{ HT_ADMIN4, "/addfuel [цена], /delfuel", "поставить / удалить АЗС" },
	{ HT_ADMIN4, "/addgarage [1-2], /delgarage", "СТО (1) или штрафстоянка (2)" },
	{ HT_ADMIN4, "/addgate [фр] [тип], /delgate", "шлагбаум / ворота" },
	{ HT_ADMIN4, "/addturf [размер] [имя]", "создать район банд" },
	{ HT_ADMIN4, "/setturf [банда], /delturf", "владелец района / удалить район" },
	{ HT_ADMIN4, "/addexampoint, /clearexam", "маршрут экзамена в автошколе" },
	{ HT_ADMIN4, "/givefood [id]", "накормить" },
	{ HT_ADMIN4, "/giveitem [id] [вещь] [кол-во]", "выдать вещь (1-12)" },
	{ HT_ADMIN4, "/makeleader [id] [фракция]", "назначить лидера" },
	{ HT_ADMIN4, "/setfaction [id] [фр] [ранг]", "поставить во фракцию (0 — выгнать)" },
	{ HT_ADMIN4, "/setfspawn [фракция]", "спавн фракции здесь" },
	{ HT_ADMIN4, "/setfduty [фракция]", "раздевалка фракции здесь" },
	{ HT_ADMIN4, "/addfcar [фракция], /delfcar", "машина фракции (сидя в машине из /veh)" },
	{ HT_ADMIN4, "/fedit [фр] name|color|bank", "название, цвет, общак фракции" },
	{ HT_ADMIN4, "/fpslist", "FPS всех игроков" },

	// --- Админ 5
	{ HT_ADMIN5, "/makeadmin [id] [0-5]", "выдать админку" },
	{ HT_ADMIN5, "/givemoney [id] [сумма]", "выдать деньги" },
	{ HT_ADMIN5, "/settax income|car [знач.]", "налог с зарплаты (%) или на машину (‰)" },

	// --- Разработка
	{ HT_DEV, "/pos [название]", "координаты; с названием — сохранить точку (scriptfiles/bastion_points.txt)" },
	{ HT_DEV, "/points", "список точек, телепорт" },
	{ HT_DEV, "/delpoint [название]", "удалить точку" },
	{ HT_DEV, "/dev", "координаты на экране" },
	{ HT_DEV, "/markers, /mk", "маркеры: перенести, вид, значок на карте, раздел GPS; свои места" },
	{ HT_DEV, "/mo", "маппинг: справка по объектам" },
	{ HT_DEV, "/mo new [модель]", "поставить объект" },
	{ HT_DEV, "/mo sel, /mo edit [№]", "выбрать / двигать мышью" },
	{ HT_DEV, "/mo move x y z, /mo rot x y z", "сдвинуть / повернуть" },
	{ HT_DEV, "/mo here, /mo copy, /mo del", "к себе, копия, удалить" },
	{ HT_DEV, "/mo near", "объекты рядом" },
	{ HT_DEV, "/mo remove [радиус], /mo unremove", "убрать здания GTA / вернуть" },
	{ HT_DEV, "/mo export", "выгрузить карту в map_export.txt" },
	{ HT_DEV, "/mt", "перекраска объекта: текстуры, цвет, надписи (/mt help)" }
};

stock bool:Help_Show(playerid)
{
	new text[1400], line[80];
	for (new t = 0; t < HT_COUNT; t++)
	{
		new count = 0;
		for (new i = 0; i < sizeof gHelp; i++)
		{
			if (gHelp[i][hpTopic] == t) count++;
		}
		if (t > 0) strcat(text, "\n");
		if (t >= HT_ADMIN1 && t <= HT_DEV) format(line, sizeof line, "{FF66FF}%s{AAAAAA}  (%d)", gHelpTopics[t], count);
		else format(line, sizeof line, "%s{AAAAAA}  (%d)", gHelpTopics[t], count);
		strcat(text, line);
	}
	ShowPlayerDialog(playerid, DIALOG_HELP_TOPICS, DIALOG_STYLE_LIST, "Помощь — выбери раздел", "%s", "Открыть", "Закрыть", text);
	return true;
}

// Раздел по страницам: длинные окна старый клиент иногда не показывает
static Help_ShowTopic(playerid, t, page = 0)
{
	new total = 0;
	for (new i = 0; i < sizeof gHelp; i++)
	{
		if (gHelp[i][hpTopic] == t) total++;
	}
	new pages = (total + HELP_PAGE_LINES - 1) / HELP_PAGE_LINES;
	if (pages < 1) pages = 1;
	if (page >= pages) page = pages - 1;
	if (page < 0) page = 0;

	new text[1800], line[160], title[80], n = 0;
	for (new i = 0; i < sizeof gHelp; i++)
	{
		if (gHelp[i][hpTopic] != t) continue;
		if (n >= page * HELP_PAGE_LINES && n < (page + 1) * HELP_PAGE_LINES)
		{
			format(line, sizeof line, "{FFFF00}%s{FFFFFF} - %s\n", gHelp[i][hpCmd], gHelp[i][hpDesc]);
			strcat(text, line);
		}
		n++;
	}
	if (pages > 1) format(title, sizeof title, "%s (%d/%d)", gHelpTopics[t], page + 1, pages);
	else format(title, sizeof title, "%s", gHelpTopics[t]);

	gHelpTopic[playerid] = t;
	gHelpPage[playerid] = page;
	gHelpPages[playerid] = pages;
	if (page + 1 < pages) ShowPlayerDialog(playerid, DIALOG_HELP_TOPIC, DIALOG_STYLE_MSGBOX, title, "%s", "Дальше", "К списку", text);
	else ShowPlayerDialog(playerid, DIALOG_HELP_TOPIC, DIALOG_STYLE_MSGBOX, title, "%s", "К списку", "Закрыть", text);
}

stock Help_OnDialog(playerid, dialogid, response, listitem)
{
	if (dialogid == DIALOG_HELP_TOPICS)
	{
		if (response && listitem >= 0 && listitem < HT_COUNT) Help_ShowTopic(playerid, listitem);
		return 1;
	}
	if (dialogid == DIALOG_HELP_TOPIC)
	{
		if (gHelpPage[playerid] + 1 < gHelpPages[playerid])
		{
			// «Дальше» / «К списку»
			if (response) Help_ShowTopic(playerid, gHelpTopic[playerid], gHelpPage[playerid] + 1);
			else Help_Show(playerid);
		}
		else if (response) Help_Show(playerid);
		return 1;
	}
	return 0;
}
