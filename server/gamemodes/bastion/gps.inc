// =====================================================================
//  gps.inc — правка маркеров в игре (/markers) и GPS для игроков (/gps).
//
//  /markers (/mk), админ 4+:
//    у маркера — меню: перенести, вид (цилиндр, стрелка, значок),
//    выше/ниже, значок на карте, раздел и название в GPS, вернуть как было;
//    вдали от маркеров — поставить свой маркер, добавить место в GPS,
//    список всех маркеров с телепортом.
//    Перенос: «Перенести», дойти до нового места, снова /markers — «Поставить».
//    Для входа в здание встань лицом туда, куда игрок смотрит после входа.
//  /gps — разделы мест (берутся из маркеров), моя машина, мой дом.
//    Метка ставится на карту и радар, внизу экрана — расстояние.
// =====================================================================

#define MARKER_ADMIN        4
#define MARKER_NEAR         4.0
#define MARKER_STEP         0.25
#define MARKER_ICON_SLOTS   98      // значки маркеров: слоты 0..98
#define GPS_ICON_SLOT       99      // слот 99 — метка GPS
#define GPS_ICON            41      // значок «путевая точка»
#define GPS_ARRIVE          12.0
#define GPS_LIST_MAX        60
#define MK_LIST_PAGE        40
#define MK_MENU_MAX         12


static const gMkTypeNames[MK_TYPES][] =
{
	"Своё место", "Вход", "Дом", "Бизнес", "Банкомат", "АЗС", "СТО", "Фракция", "Работа", "Автосалон", "Автошкола", "Точка систем", "Прокат"
};

static const gGpsCatNames[GPS_CATS][] =
{
	"Не показывать", "Госучреждения", "Работы", "Магазины и бизнесы", "Транспорт",
	"Заправки", "Банкоматы", "Фракции", "Развлечения", "Другое"
};

// Значки на карте (номер значка SA-MP и название).
// Значки фракций и бизнесов на ПК заменены нашими (client-mods/tools/build_radar.py, hud.txd):
// фракции 61, 40, 8, 23; бизнесы 24, 13, 55, 49, 10, 6, 27, 25, 26, 34, 45, 52 (24, 13, 26, 34 — бывшие значки сюжетных героев);
// работы 12, 11, 47, 38, 46, 18, 42, 28, 44, 29 (бывшие сюжетные значки и пицца);
// имущество 31, 15, 32, 16, 43, 9, 21 (дом, квартира, особняк, гараж, транспорт, лодки, вертолёты).
enum E_MK_ICON
{
	miId,
	miName[24]
}
static const gMkIcons[][E_MK_ICON] =
{
	{ 30,"Полиция LSPD" },    { 22,"Больница" },        { 61,"ФБР" },
	{ 40,"Армия" },           { 8, "Мэрия" },           { 23,"СМИ" },
	{ 62,"Grove Street" },    { 59,"Ballas" },          { 58,"Aztecas" },
	{ 60,"Vagos" },           { 24,"Магазин 24/7" },    { 13,"Заправка" },
	{ 55,"Автосалон" },       { 49,"Бар" },             { 10,"Закусочная" },
	{ 6, "Оружейный" },       { 27,"СТО" },             { 25,"Казино" },
	{ 26,"Отель" },           { 34,"Аренда авто" },     { 45,"Магазин одежды" },
	{ 52,"Банк" },            { 12,"Работа: таксист" }, { 11,"Работа: дальнобойщик" },
	{ 47,"Работа: механик" }, { 38,"Работа: рыбак" },   { 46,"Работа: грузчик" },
	{ 18,"Работа: инкассатор" }, { 42,"Работа: фермер" }, { 28,"Работа: мусорщик" },
	{ 44,"Работа: автобус" }, { 29,"Работа: развоз пиццы" },
	{ 31,"Дом" },             { 15,"Квартира" },        { 32,"Особняк" },
	{ 16,"Гараж" },           { 43,"Личный транспорт" }, { 9, "Лодки" },
	{ 21,"Вертолёты" },
	{ 36,"Школа" },           { 63,"Покраска" },
	{ 51,"Грузовик" },        { 48,"Клуб" },            { 50,"Ресторан" },
	{ 14,"Курица" },          { 17,"Кафе" },
	{ 7, "Парикмахер" },      { 39,"Тату" },            { 54,"Спортзал" },
	{ 35,"Сохранение" },
	{ 33,"Флаг" },            { 5, "Самолёт" },
	{ 56,"Точка" }
};

// Пункты меню маркера
enum
{
	MA_CARRY,
	MA_STYLE,
	MA_UP,
	MA_DOWN,
	MA_ICON,
	MA_GPS,
	MA_NAME,
	MA_LABEL,
	MA_RESET,
	MA_DELETE,
	MA_FACTION
}

static gMkSel[MAX_PLAYERS] = { -1, ... };           // маркер в открытом меню
static gMkCarry[MAX_PLAYERS] = { -1, ... };         // маркер, который несём
static gMkActs[MAX_PLAYERS][MK_MENU_MAX];
static gMkActCount[MAX_PLAYERS];
static gMkListMap[MAX_PLAYERS][MK_LIST_PAGE + 1];
static gMkListCount[MAX_PLAYERS];
static gMkListPage[MAX_PLAYERS];

static gGpsMenu[MAX_PLAYERS][GPS_CATS + 3];
static gGpsMenuCount[MAX_PLAYERS];
static gGpsList[MAX_PLAYERS][GPS_LIST_MAX];
static gGpsListCount[MAX_PLAYERS];
static bool:gGpsOn[MAX_PLAYERS];
static Float:gGpsX[MAX_PLAYERS], Float:gGpsY[MAX_PLAYERS], Float:gGpsZ[MAX_PLAYERS];
static PlayerText:gGpsTD[MAX_PLAYERS] = { INVALID_PLAYER_TEXT_DRAW, ... };

forward Gps_OnSecond();

stock Gps_Init()
{
	SetTimer("Gps_OnSecond", 1000, true);
}

stock Gps_OnConnect(playerid)
{
	gMkSel[playerid] = -1;
	gMkCarry[playerid] = -1;
	gGpsOn[playerid] = false;
	gGpsTD[playerid] = INVALID_PLAYER_TEXT_DRAW;
	Markers_ShowIcons(playerid);
}

stock Gps_OnDisconnect(playerid)
{
	gMkSel[playerid] = -1;
	gMkCarry[playerid] = -1;
	gGpsOn[playerid] = false;
	gGpsTD[playerid] = INVALID_PLAYER_TEXT_DRAW;
}

// ---------------------------------------------------------------------
//  Значки на карте
// ---------------------------------------------------------------------
stock Markers_ShowIcons(playerid)
{
	for (new s = 0; s < MARKER_ICON_SLOTS; s++) RemovePlayerMapIcon(playerid, s);
	new slot = 0;
	for (new m = 0; m < MAX_MARKERS && slot < MARKER_ICON_SLOTS; m++)
	{
		if (!gMarker[m][mkUsed] || gMarker[m][mkIcon] < 0) continue;
		if (gMarker[m][mkWorld] != 0 || gMarker[m][mkInterior] != 0) continue;
		if (!Marker_VisibleTo(playerid, m)) continue;   // точка только для своей фракции
		SetPlayerMapIcon(playerid, slot++, gMarker[m][mkX], gMarker[m][mkY], gMarker[m][mkZ], gMarker[m][mkIcon], 0, MAPICON_LOCAL);
	}
}

stock Markers_RefreshIcons()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i)) Markers_ShowIcons(i);
	}
}

// ---------------------------------------------------------------------
//  Названия для меню
// ---------------------------------------------------------------------
static Mk_StyleName(model)
{
	new s[40];
	if (model == 0)
	{
		strcat(s, "без значка");
		return s;
	}
	for (new i = 0; i < sizeof gMkStyles; i++)
	{
		if (gMkStyles[i][msModel] == model)
		{
			strcat(s, gMkStyles[i][msName]);
			return s;
		}
	}
	format(s, sizeof s, "модель %d", model);
	return s;
}

static Mk_IconName(icon)
{
	new s[32];
	if (icon < 0)
	{
		strcat(s, "нет");
		return s;
	}
	for (new i = 0; i < sizeof gMkIcons; i++)
	{
		if (gMkIcons[i][miId] == icon)
		{
			strcat(s, gMkIcons[i][miName]);
			return s;
		}
	}
	format(s, sizeof s, "№%d", icon);
	return s;
}

static Gps_DistText(Float:d)
{
	new s[16];
	if (d < 1000.0) format(s, sizeof s, "%d м", floatround(d));
	else format(s, sizeof s, "%.1f км", d / 1000.0);
	return s;
}

// Скопировать ввод игрока: без %, не длиннее size-1; | — новая строка (если lines)
static Mk_CleanInput(dest[], size, const src[], bool:lines)
{
	new n = 0;
	for (new i = 0; src[i] != '\0' && n < size - 1; i++)
	{
		new c = src[i];
		if (c == '%') c = '#';
		if (lines && c == '|') c = '\n';
		dest[n++] = c;
	}
	dest[n] = '\0';
}

// ---------------------------------------------------------------------
//  /markers
// ---------------------------------------------------------------------
static Mk_Nearest(playerid, Float:range)
{
	new Float:px, Float:py, Float:pz;
	GetPlayerPos(playerid, px, py, pz);
	new world = GetPlayerVirtualWorld(playerid), interior = GetPlayerInterior(playerid);
	new best = -1;
	new Float:bestd = range;
	for (new m = 0; m < MAX_MARKERS; m++)
	{
		if (!gMarker[m][mkUsed] || gMarker[m][mkWorld] != world || gMarker[m][mkInterior] != interior) continue;
		new Float:d = VectorSize(px - gMarker[m][mkX], py - gMarker[m][mkY], pz - gMarker[m][mkZ]);
		if (d < bestd)
		{
			bestd = d;
			best = m;
		}
	}
	return best;
}

stock bool:Cmd_Markers(playerid)
{
	if (!Admin_Check(playerid, MARKER_ADMIN)) return true;
	new c = gMkCarry[playerid];
	if (Marker_IsValid(c))
	{
		ShowPlayerDialog(playerid, DIALOG_MK_PLACE, DIALOG_STYLE_MSGBOX, "Перенос маркера",
			"{FFFFFF}Поставить «%s» туда, где ты стоишь?\n\n{AAAAAA}Для входа в здание встань лицом туда, куда игрок будет смотреть после входа.",
			"Поставить", "Отмена", gMarker[c][mkName]);
		return true;
	}
	gMkCarry[playerid] = -1;
	new m = Mk_Nearest(playerid, MARKER_NEAR);
	if (m == -1)
	{
		ShowPlayerDialog(playerid, DIALOG_MK_NEW, DIALOG_STYLE_LIST, "Маркеры {AAAAAA}(рядом нет)",
			"Поставить свой маркер здесь (жёлтый цилиндр)\nДобавить место в GPS здесь (без значка)\nВсе маркеры (телепорт к ним)",
			"Выбрать", "Закрыть");
		return true;
	}
	Mk_ShowMenu(playerid, m);
	return true;
}

static Mk_AddAct(playerid, text[], act, const line[])
{
	new n = gMkActCount[playerid];
	if (n >= MK_MENU_MAX) return;
	if (n > 0) strcat(text, "\n", 1000);
	strcat(text, line, 1000);
	gMkActs[playerid][n] = act;
	gMkActCount[playerid] = n + 1;
}

static Mk_ShowMenu(playerid, m)
{
	gMkSel[playerid] = m;
	gMkActCount[playerid] = 0;
	new text[1000], line[128];
	new bool:custom = (gMarker[m][mkType] == MK_CUSTOM);

	Mk_AddAct(playerid, text, MA_CARRY, "Перенести в другое место");
	if (gMarker[m][mkModel] < 0)
		format(line, sizeof line, "Вид: {FFFF00}%s {AAAAAA}(как в коде)", Mk_StyleName(Marker_Model(m)));
	else
		format(line, sizeof line, "Вид: {FFFF00}%s", Mk_StyleName(Marker_Model(m)));
	Mk_AddAct(playerid, text, MA_STYLE, line);
	Mk_AddAct(playerid, text, MA_UP, "Поднять значок на 0.25 м");
	Mk_AddAct(playerid, text, MA_DOWN, "Опустить значок на 0.25 м");
	format(line, sizeof line, "Значок на карте: {FFFF00}%s", Mk_IconName(gMarker[m][mkIcon]));
	Mk_AddAct(playerid, text, MA_ICON, line);
	format(line, sizeof line, "GPS: {FFFF00}%s", gGpsCatNames[gMarker[m][mkGps]]);
	Mk_AddAct(playerid, text, MA_GPS, line);
	format(line, sizeof line, "Название: {FFFF00}%s", gMarker[m][mkName]);
	Mk_AddAct(playerid, text, MA_NAME, line);
	if (!Marker_IsSecret(m)) format(line, sizeof line, "Видят: {FFFF00}все");
	else
	{
		new cnt = 0, one = gMarker[m][mkFaction];
		for (new f = 1; f < MAX_FACTIONS; f++) if (f == gMarker[m][mkFaction] || Marker_AllowHas(m, f)) { cnt++; one = f; }
		if (cnt == 1) format(line, sizeof line, "Видят: {FFFF00}только %s {AAAAAA}(секретная)", Faction_GetName(one));
		else format(line, sizeof line, "Видят: {FFFF00}%d фракции {AAAAAA}(секретная)", cnt);
	}
	Mk_AddAct(playerid, text, MA_FACTION, line);
	if (custom)
	{
		Mk_AddAct(playerid, text, MA_LABEL, "Надпись над маркером");
		Mk_AddAct(playerid, text, MA_DELETE, "{FF6347}Удалить");
	}
	else Mk_AddAct(playerid, text, MA_RESET, "{AAAAAA}Вернуть как было в коде");

	new title[80];
	format(title, sizeof title, "%s: %s", gMkTypeNames[gMarker[m][mkType]], gMarker[m][mkName]);
	ShowPlayerDialog(playerid, DIALOG_MK_MENU, DIALOG_STYLE_LIST, title, "%s", "Выбрать", "Закрыть", text);
}

static Mk_OnAction(playerid, m, act)
{
	switch (act)
	{
		case MA_CARRY:
		{
			gMkCarry[playerid] = m;
			SendClientMessage(playerid, COLOR_ADMIN, "Маркер «%s» взят. Встань на новое место и снова введи /markers.", gMarker[m][mkName]);
		}
		case MA_STYLE:
		{
			new text[1200], line[64];
			format(text, sizeof text, "{AAAAAA}Как в коде (%s)", Mk_StyleName(gMarker[m][mkDefModel]));
			for (new i = 0; i < sizeof gMkStyles; i++)
			{
				format(line, sizeof line, "\n%s", gMkStyles[i][msName]);
				strcat(text, line);
			}
			strcat(text, "\nБез значка (только надпись)");
			ShowPlayerDialog(playerid, DIALOG_MK_STYLE, DIALOG_STYLE_LIST, "Вид маркера", "%s", "Выбрать", "Назад", text);
		}
		case MA_UP, MA_DOWN:
		{
			if (act == MA_UP) gMarker[m][mkZOff] += MARKER_STEP;
			else gMarker[m][mkZOff] -= MARKER_STEP;
			Marker_Build(m);
			Marker_Save(m);
			Mk_ShowMenu(playerid, m);
		}
		case MA_ICON:
		{
			new text[2600], line[64];
			strcat(text, "{AAAAAA}Без значка");
			for (new i = 0; i < sizeof gMkIcons; i++)
			{
				format(line, sizeof line, "\n%s {AAAAAA}(%d)", gMkIcons[i][miName], gMkIcons[i][miId]);
				strcat(text, line);
			}
			ShowPlayerDialog(playerid, DIALOG_MK_ICON, DIALOG_STYLE_LIST, "Значок на карте", "%s", "Выбрать", "Назад", text);
		}
		case MA_GPS:
		{
			new text[400];
			for (new i = 0; i < GPS_CATS; i++)
			{
				if (i > 0) strcat(text, "\n");
				strcat(text, gGpsCatNames[i]);
			}
			ShowPlayerDialog(playerid, DIALOG_MK_GPS, DIALOG_STYLE_LIST, "Раздел в GPS", "%s", "Выбрать", "Назад", text);
		}
		case MA_NAME:
		{
			ShowPlayerDialog(playerid, DIALOG_MK_NAME, DIALOG_STYLE_INPUT, "Название",
				"{FFFFFF}Название места для GPS и списков (до 31 буквы).\nСейчас: {FFFF00}%s\n{AAAAAA}Пусто — как в коде.",
				"Сохранить", "Назад", gMarker[m][mkName]);
		}
		case MA_LABEL:
		{
			ShowPlayerDialog(playerid, DIALOG_MK_LABEL, DIALOG_STYLE_INPUT, "Надпись над маркером",
				"{FFFFFF}Текст над маркером. Новая строка — символ |\n{AAAAAA}Пусто — без надписи.",
				"Сохранить", "Назад");
		}
		case MA_FACTION: Mk_ShowAllow(playerid, m);
		case MA_RESET:
		{
			Mk_Reset(m);
			SendClientMessage(playerid, COLOR_ADMIN, "Маркер «%s» вернулся к настройкам из кода.", gMarker[m][mkName]);
			Mk_ShowMenu(playerid, m);
		}
		case MA_DELETE:
		{
			ShowPlayerDialog(playerid, DIALOG_MK_DELETE, DIALOG_STYLE_MSGBOX, "Удалить маркер",
				"{FFFFFF}Удалить «%s»?", "Удалить", "Назад", gMarker[m][mkName]);
		}
	}
}

// Кто видит секретную точку: номера фракций через запятую
static Mk_ShowAllow(playerid, m)
{
	new cur[160], text[600];
	Marker_AllowString(m, cur, sizeof cur);
	if (cur[0] == '\0') strcat(cur, "все");
	format(text, sizeof text, "{FFFFFF}Номера фракций через запятую, например: {FFFF00}1,7,50\n{FFFFFF}Остальные не увидят ни значок, ни надпись и не смогут войти.\n\nСейчас: {FFFF00}%s\n{AAAAAA}0 или пусто — видят все. Номера: /flist",
		cur);
	if (gMarker[m][mkFaction] != 0)
	{
		new add[96];
		format(add, sizeof add, "\n{AAAAAA}Своя точка фракции %s: её участники видят её всегда.", Faction_GetName(gMarker[m][mkFaction]));
		strcat(text, add);
	}
	ShowPlayerDialog(playerid, DIALOG_MK_FACTION, DIALOG_STYLE_INPUT, "Кто видит точку", "%s", "Сохранить", "Назад", text);
}

static Mk_Reset(m)
{
	new bool:moved = gMarker[m][mkMoved];
	gMarker[m][mkX] = gMarker[m][mkDefX];
	gMarker[m][mkY] = gMarker[m][mkDefY];
	gMarker[m][mkZ] = gMarker[m][mkDefZ];
	gMarker[m][mkA] = gMarker[m][mkDefA];
	gMarker[m][mkWorld] = gMarker[m][mkDefWorld];
	gMarker[m][mkInterior] = gMarker[m][mkDefInterior];
	gMarker[m][mkMoved] = false;
	gMarker[m][mkModel] = -1;
	gMarker[m][mkZOff] = 0.0;
	gMarker[m][mkIcon] = -1;
	gMarker[m][mkGps] = gMarker[m][mkDefGps];
	gMarker[m][mkName][0] = '\0';
	strcat(gMarker[m][mkName], gMarker[m][mkDefName], 32);
	gMarker[m][mkNameSet] = false;
	for (new c = 0; c < MK_ALLOW_CELLS; c++) gMarker[m][mkAllow][c] = 0;
	Marker_Forget(m);
	Marker_Build(m);
	if (moved) Marker_OnMoved(m);
	Markers_RefreshIcons();
}

static Mk_PlaceHere(playerid, m)
{
	new Float:x, Float:y, Float:z, Float:a;
	GetPlayerPos(playerid, x, y, z);
	GetPlayerFacingAngle(playerid, a);
	gMarker[m][mkX] = x;
	gMarker[m][mkY] = y;
	gMarker[m][mkZ] = z;
	gMarker[m][mkA] = a;
	gMarker[m][mkWorld] = GetPlayerVirtualWorld(playerid);
	gMarker[m][mkInterior] = GetPlayerInterior(playerid);
	gMarker[m][mkMoved] = true;
	Marker_Build(m);
	Marker_Save(m);
	Marker_OnMoved(m);
	if (gMarker[m][mkIcon] >= 0) Markers_RefreshIcons();
	SendClientMessage(playerid, COLOR_ADMIN, "Маркер «%s» перенесён сюда и сохранён.", gMarker[m][mkName]);
}

static Mk_NextCustomRef()
{
	new ref = 0;
	for (new m = 0; m < MAX_MARKERS; m++)
	{
		if (gMarker[m][mkUsed] && gMarker[m][mkType] == MK_CUSTOM && gMarker[m][mkRef] >= ref) ref = gMarker[m][mkRef] + 1;
	}
	return ref;
}

static Mk_CreateHere(playerid, bool:withModel)
{
	new Float:x, Float:y, Float:z, Float:a;
	GetPlayerPos(playerid, x, y, z);
	GetPlayerFacingAngle(playerid, a);
	new m = Marker_Create(MK_CUSTOM, Mk_NextCustomRef(), "Новое место", 0, x, y, z,
		GetPlayerVirtualWorld(playerid), GetPlayerInterior(playerid), withModel ? GPS_NONE : GPS_OTHER, a);
	if (m == -1)
	{
		SendClientMessage(playerid, COLOR_GREY, "Слишком много маркеров (MAX_MARKERS).");
		return;
	}
	if (withModel) gMarker[m][mkModel] = 19197;
	gMarker[m][mkMoved] = true;
	Marker_Build(m);
	Marker_Save(m);
	gMkSel[playerid] = m;
	Mk_OnAction(playerid, m, MA_NAME);
}

static Mk_ShowList(playerid, page)
{
	new text[3000], line[80], n = 0, skip = page * MK_LIST_PAGE;
	for (new m = 0; m < MAX_MARKERS; m++)
	{
		if (!gMarker[m][mkUsed]) continue;
		if (skip > 0)
		{
			skip--;
			continue;
		}
		if (n == MK_LIST_PAGE)
		{
			strcat(text, "\n{FFFF00}Дальше >>");
			gMkListMap[playerid][n++] = -2;
			break;
		}
		if (n > 0) strcat(text, "\n");
		format(line, sizeof line, "{AAAAAA}%s:{FFFFFF} %s", gMkTypeNames[gMarker[m][mkType]], gMarker[m][mkName]);
		strcat(text, line);
		gMkListMap[playerid][n++] = m;
	}
	if (n == 0)
	{
		SendClientMessage(playerid, COLOR_GREY, "Маркеров больше нет.");
		return;
	}
	gMkListCount[playerid] = n;
	gMkListPage[playerid] = page;
	new title[48];
	format(title, sizeof title, "Все маркеры, стр. %d", page + 1);
	ShowPlayerDialog(playerid, DIALOG_MK_LIST, DIALOG_STYLE_LIST, title, "%s", "Телепорт", "Закрыть", text);
}

// ---------------------------------------------------------------------
//  GPS
// ---------------------------------------------------------------------
static Gps_AddMenu(playerid, text[], act, const line[])
{
	new n = gGpsMenuCount[playerid];
	if (n >= GPS_CATS + 3) return;
	if (n > 0) strcat(text, "\n", 800);
	strcat(text, line, 800);
	gGpsMenu[playerid][n] = act;
	gGpsMenuCount[playerid] = n + 1;
}

stock bool:Cmd_Gps(playerid)
{
	new count[GPS_CATS];
	for (new m = 0; m < MAX_MARKERS; m++)
	{
		if (gMarker[m][mkUsed] && gMarker[m][mkGps] > 0 && gMarker[m][mkGps] < GPS_CATS && Marker_VisibleTo(playerid, m)) count[gMarker[m][mkGps]]++;
	}
	new text[800], line[80];
	gGpsMenuCount[playerid] = 0;
	if (gGpsOn[playerid]) Gps_AddMenu(playerid, text, -1, "{FF6347}Убрать метку GPS");
	Gps_AddMenu(playerid, text, -2, "{FFFF00}Моя машина");
	Gps_AddMenu(playerid, text, -3, "{FFFF00}Мой дом");
	for (new c = 1; c < GPS_CATS; c++)
	{
		if (count[c] == 0) continue;
		format(line, sizeof line, "%s {AAAAAA}(%d)", gGpsCatNames[c], count[c]);
		Gps_AddMenu(playerid, text, c, line);
	}
	if (Player[playerid][pAdmin] >= MARKER_ADMIN)
		ShowPlayerDialog(playerid, DIALOG_GPS_MAIN, DIALOG_STYLE_LIST, "GPS {AAAAAA}(править места: /markers)", "%s", "Выбрать", "Закрыть", text);
	else
		ShowPlayerDialog(playerid, DIALOG_GPS_MAIN, DIALOG_STYLE_LIST, "GPS", "%s", "Выбрать", "Закрыть", text);
	return true;
}

// Места раздела cat по расстоянию -> gGpsList[playerid], возвращает количество
static Gps_FillCat(playerid, cat, Float:dist[GPS_LIST_MAX])
{
	new Float:px, Float:py, Float:pz;
	GetPlayerPos(playerid, px, py, pz);
	new n = 0;
	for (new m = 0; m < MAX_MARKERS && n < GPS_LIST_MAX; m++)
	{
		if (!gMarker[m][mkUsed] || gMarker[m][mkGps] != cat || !Marker_VisibleTo(playerid, m)) continue;
		new Float:d = VectorSize(px - gMarker[m][mkX], py - gMarker[m][mkY], pz - gMarker[m][mkZ]);
		// вставка по возрастанию расстояния
		new j = n;
		while (j > 0 && dist[j - 1] > d)
		{
			dist[j] = dist[j - 1];
			gGpsList[playerid][j] = gGpsList[playerid][j - 1];
			j--;
		}
		dist[j] = d;
		gGpsList[playerid][j] = m;
		n++;
	}
	gGpsListCount[playerid] = n;
	return n;
}

static Gps_ShowCat(playerid, cat)
{
	new Float:dist[GPS_LIST_MAX];
	new n = Gps_FillCat(playerid, cat, dist);
	if (n == 0)
	{
		SendClientMessage(playerid, COLOR_GREY, "В этом разделе пока нет мест.");
		return;
	}
	gGpsListCount[playerid] = n;
	new text[3600], line[80];
	for (new i = 0; i < n; i++)
	{
		if (i > 0) strcat(text, "\n");
		format(line, sizeof line, "%s {AAAAAA}— %s", gMarker[gGpsList[playerid][i]][mkName], Gps_DistText(dist[i]));
		strcat(text, line);
	}
	ShowPlayerDialog(playerid, DIALOG_GPS_LIST, DIALOG_STYLE_LIST, gGpsCatNames[cat], "%s", "Метка", "Назад", text);
}

static Gps_Set(playerid, Float:x, Float:y, Float:z, const name[])
{
	gGpsOn[playerid] = true;
	gGpsX[playerid] = x;
	gGpsY[playerid] = y;
	gGpsZ[playerid] = z;
	SetPlayerMapIcon(playerid, GPS_ICON_SLOT, x, y, z, GPS_ICON, 0, MAPICON_GLOBAL);
	if (gGpsTD[playerid] == INVALID_PLAYER_TEXT_DRAW)
	{
		new Float:ty = IsPlayerMobile[playerid] ? 30.0 : 418.0;
		gGpsTD[playerid] = CreatePlayerTextDraw(playerid, 320.0, ty, "_");
		PlayerTextDrawLetterSize(playerid, gGpsTD[playerid], 0.3, 1.2);
		PlayerTextDrawFont(playerid, gGpsTD[playerid], TEXT_DRAW_FONT_1);
		PlayerTextDrawAlignment(playerid, gGpsTD[playerid], TEXT_DRAW_ALIGN_CENTER);
		PlayerTextDrawColour(playerid, gGpsTD[playerid], 0xFFFFFFFF);
		PlayerTextDrawSetOutline(playerid, gGpsTD[playerid], 1);
		PlayerTextDrawShow(playerid, gGpsTD[playerid]);
	}
	Gps_Update(playerid);
	SendClientMessage(playerid, COLOR_YELLOW, "GPS: метка «%s» на карте и радаре. Убрать: /gps", name);
}

// Поставить метку GPS из других модулей
stock Gps_SetPoint(playerid, Float:x, Float:y, Float:z, const name[])
{
	Gps_Set(playerid, x, y, z, name);
}

stock Gps_Clear(playerid)
{
	if (!gGpsOn[playerid]) return;
	gGpsOn[playerid] = false;
	RemovePlayerMapIcon(playerid, GPS_ICON_SLOT);
	if (gGpsTD[playerid] != INVALID_PLAYER_TEXT_DRAW)
	{
		PlayerTextDrawDestroy(playerid, gGpsTD[playerid]);
		gGpsTD[playerid] = INVALID_PLAYER_TEXT_DRAW;
	}
}

static Gps_Update(playerid)
{
	new Float:d = GetPlayerDistanceFromPoint(playerid, gGpsX[playerid], gGpsY[playerid], gGpsZ[playerid]);
	if (d < GPS_ARRIVE)
	{
		Gps_Clear(playerid);
		SendClientMessage(playerid, COLOR_YELLOW, "GPS: ты на месте.");
		return;
	}
	if (gGpsTD[playerid] == INVALID_PLAYER_TEXT_DRAW) return;
	if (d < 1000.0) PlayerTextDrawSetString(playerid, gGpsTD[playerid], "GPS ~y~%d m", floatround(d));
	else PlayerTextDrawSetString(playerid, gGpsTD[playerid], "GPS ~y~%.1f km", d / 1000.0);
}

public Gps_OnSecond()
{
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (gGpsOn[i] && IsPlayerConnected(i)) Gps_Update(i);
	}
	return 1;
}

static Gps_OnMain(playerid, listitem)
{
	if (listitem < 0 || listitem >= gGpsMenuCount[playerid]) return;
	Gps_DoAct(playerid, gGpsMenu[playerid][listitem]);
}

// -1 убрать метку, -2 моя машина, -3 мой дом, 1.. раздел мест
static Gps_DoAct(playerid, act)
{
	switch (act)
	{
		case -1:
		{
			Gps_Clear(playerid);
			SendClientMessage(playerid, COLOR_YELLOW, "GPS: метка убрана.");
		}
		case -2:
		{
			new v = gPlayerVeh[playerid];
			if (v == INVALID_VEHICLE_ID)
			{
				SendClientMessage(playerid, COLOR_GREY, "У тебя нет машины.");
				return;
			}
			new Float:x, Float:y, Float:z;
			GetVehiclePos(v, x, y, z);
			Gps_Set(playerid, x, y, z, "Моя машина");
		}
		case -3:
		{
			new h = House_GetOwned(playerid);
			if (h == -1) h = House_GetRented(playerid);
			if (h == -1)
			{
				SendClientMessage(playerid, COLOR_GREY, "У тебя нет дома.");
				return;
			}
			Gps_Set(playerid, House[h][hX], House[h][hY], House[h][hZ], "Мой дом");
		}
		default: Gps_ShowCat(playerid, act);
	}
}

// ---------------------------------------------------------------------
//  Окна
// ---------------------------------------------------------------------
stock Markers_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid < DIALOG_MK_MENU || dialogid > DIALOG_GPS_LIST) return 0;

	switch (dialogid)
	{
		case DIALOG_GPS_MAIN:
		{
			if (response) Gps_OnMain(playerid, listitem);
			return 1;
		}
		case DIALOG_GPS_LIST:
		{
			if (!response)
			{
				Cmd_Gps(playerid);
				return 1;
			}
			if (listitem < 0 || listitem >= gGpsListCount[playerid]) return 1;
			new m = gGpsList[playerid][listitem];
			if (Marker_IsValid(m)) Gps_Set(playerid, gMarker[m][mkX], gMarker[m][mkY], gMarker[m][mkZ], gMarker[m][mkName]);
			return 1;
		}
	}

	if (Player[playerid][pAdmin] < MARKER_ADMIN) return 1;

	switch (dialogid)
	{
		case DIALOG_MK_NEW:
		{
			if (!response) return 1;
			if (listitem == 0) Mk_CreateHere(playerid, true);
			else if (listitem == 1) Mk_CreateHere(playerid, false);
			else Mk_ShowList(playerid, 0);
			return 1;
		}
		case DIALOG_MK_LIST:
		{
			if (!response || listitem < 0 || listitem >= gMkListCount[playerid]) return 1;
			new m = gMkListMap[playerid][listitem];
			if (m == -2)
			{
				Mk_ShowList(playerid, gMkListPage[playerid] + 1);
				return 1;
			}
			if (!Marker_IsValid(m)) return 1;
			Teleport_SaveBack(playerid);
			Teleport_Player(playerid, gMarker[m][mkX] + 1.0, gMarker[m][mkY], gMarker[m][mkZ], gMarker[m][mkA], gMarker[m][mkInterior], gMarker[m][mkWorld]);
			SendClientMessage(playerid, COLOR_ADMIN, "Ты у маркера «%s». Меню: /markers", gMarker[m][mkName]);
			return 1;
		}
		case DIALOG_MK_PLACE:
		{
			new c = gMkCarry[playerid];
			gMkCarry[playerid] = -1;
			if (!Marker_IsValid(c)) return 1;
			if (response) Mk_PlaceHere(playerid, c);
			else SendClientMessage(playerid, COLOR_GREY, "Перенос отменён, маркер остался на месте.");
			return 1;
		}
	}

	new m = gMkSel[playerid];
	if (!Marker_IsValid(m)) return 1;

	switch (dialogid)
	{
		case DIALOG_MK_MENU:
		{
			if (response && listitem >= 0 && listitem < gMkActCount[playerid]) Mk_OnAction(playerid, m, gMkActs[playerid][listitem]);
		}
		case DIALOG_MK_STYLE:
		{
			if (response)
			{
				if (listitem == 0) gMarker[m][mkModel] = -1;
				else if (listitem - 1 < sizeof gMkStyles) gMarker[m][mkModel] = gMkStyles[listitem - 1][msModel];
				else gMarker[m][mkModel] = 0;
				gMarker[m][mkZOff] = 0.0;
				Marker_Build(m);
				Marker_Save(m);
			}
			Mk_ShowMenu(playerid, m);
		}
		case DIALOG_MK_ICON:
		{
			if (response)
			{
				if (listitem == 0) gMarker[m][mkIcon] = -1;
				else if (listitem - 1 < sizeof gMkIcons) gMarker[m][mkIcon] = gMkIcons[listitem - 1][miId];
				Marker_Save(m);
				Markers_RefreshIcons();
			}
			Mk_ShowMenu(playerid, m);
		}
		case DIALOG_MK_GPS:
		{
			if (response && listitem >= 0 && listitem < GPS_CATS)
			{
				gMarker[m][mkGps] = listitem;
				Marker_Save(m);
			}
			Mk_ShowMenu(playerid, m);
		}
		case DIALOG_MK_NAME:
		{
			if (response)
			{
				gMarker[m][mkName][0] = '\0';
				if (inputtext[0] == '\0')
				{
					strcat(gMarker[m][mkName], gMarker[m][mkDefName], 32);
					gMarker[m][mkNameSet] = false;
				}
				else
				{
					Mk_CleanInput(gMarker[m][mkName], 32, inputtext, false);
					gMarker[m][mkNameSet] = true;
				}
				Marker_Save(m);
			}
			Mk_ShowMenu(playerid, m);
		}
		case DIALOG_MK_LABEL:
		{
			if (response)
			{
				new text[MK_TEXT_LEN];
				Mk_CleanInput(text, sizeof text, inputtext, true);
				Marker_SetLabel(m, text);
				Marker_Save(m);
			}
			Mk_ShowMenu(playerid, m);
		}
		case DIALOG_MK_FACTION:
		{
			if (response)
			{
				new bad = Marker_SetAllowList(m, inputtext);
				if (bad > 0) SendClientMessage(playerid, COLOR_GREY, "Номера, которых нет среди фракций, пропущены (%d). Номера: /flist", bad);
			}
			Mk_ShowMenu(playerid, m);
		}
		case DIALOG_MK_DELETE:
		{
			if (!response)
			{
				Mk_ShowMenu(playerid, m);
				return 1;
			}
			SendClientMessage(playerid, COLOR_ADMIN, "Маркер «%s» удалён.", gMarker[m][mkName]);
			Marker_Destroy(m, true);
			gMkSel[playerid] = -1;
		}
	}
	return 1;
}

// ---------------------------------------------------------------------
//  Для приложения «Карта» в телефоне (phoneapp.inc)
// ---------------------------------------------------------------------
stock Gps_IsOn(playerid)
{
	return gGpsOn[playerid] ? 1 : 0;
}

// Разделы с местами: [[номер,"название",мест],...]
stock Gps_PhoneCats(playerid, dest[], size)
{
	new count[GPS_CATS], part[64];
	for (new m = 0; m < MAX_MARKERS; m++)
	{
		if (gMarker[m][mkUsed] && gMarker[m][mkGps] > 0 && gMarker[m][mkGps] < GPS_CATS && Marker_VisibleTo(playerid, m)) count[gMarker[m][mkGps]]++;
	}
	dest[0] = '\0';
	strcat(dest, "[", size);
	new n = 0;
	for (new c = 1; c < GPS_CATS; c++)
	{
		if (count[c] == 0) continue;
		format(part, sizeof part, "%s[%d,\"%s\",%d]", n ? "," : "", c, gGpsCatNames[c], count[c]);
		strcat(dest, part, size);
		n++;
	}
	strcat(dest, "]", size);
}

// Места раздела по расстоянию: [["название","1.2 км"],...]; номер в списке потом в Gps_PhonePick
stock Gps_PhoneList(playerid, cat, dest[], size, max = 30)
{
	new Float:dist[GPS_LIST_MAX], part[112], name[80];
	dest[0] = '\0';
	strcat(dest, "[", size);
	new n = (cat > 0 && cat < GPS_CATS) ? Gps_FillCat(playerid, cat, dist) : 0;
	if (n > max) { n = max; gGpsListCount[playerid] = max; }
	for (new i = 0; i < n; i++)
	{
		UI_JsonEscape(name, gMarker[gGpsList[playerid][i]][mkName]);
		format(part, sizeof part, "%s[\"%s\",\"%s\"]", i ? "," : "", name, Gps_DistText(dist[i]));
		if (strlen(dest) + strlen(part) + 2 >= size) { gGpsListCount[playerid] = i; break; }
		strcat(dest, part, size);
	}
	strcat(dest, "]", size);
}

stock Gps_CatName(cat)
{
	new s[32];
	if (cat > 0 && cat < GPS_CATS) strcat(s, gGpsCatNames[cat]);
	return s;
}

// Метка на место из последнего Gps_PhoneList
stock Gps_PhonePick(playerid, idx)
{
	if (idx < 0 || idx >= gGpsListCount[playerid]) return 0;
	new m = gGpsList[playerid][idx];
	if (!Marker_IsValid(m)) return 0;
	Gps_Set(playerid, gMarker[m][mkX], gMarker[m][mkY], gMarker[m][mkZ], gMarker[m][mkName]);
	return 1;
}

// -1 убрать метку, -2 моя машина, -3 мой дом
stock Gps_PhoneAct(playerid, act)
{
	if (act < 0) Gps_DoAct(playerid, act);
}
