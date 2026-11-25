// =====================================================================
//  menumem.inc — меню помнят последний выбор.
//
//  Во всех списочных окнах (DIALOG_STYLE_LIST / TABLIST) пункт, который
//  игрок выбрал в этом окне в прошлый раз, повторяется первой строкой
//  «» Банк». Первая строка в окне выделена сразу, так что повторный
//  выбор — это просто Enter (ПК) или одно нажатие (телефон).
//  Остальные пункты стоят на своих местах, модули ничего не замечают:
//  номер строки (listitem) и текст (inputtext) подменяются обратно на
//  исходные до того, как ответ попадёт в модуль.
//
//  Подключается сразу после base.inc: подменяет ShowPlayerDialog на
//  Dlg_Show (как anticheat.inc подменяет SetPlayerPos). Работает на
//  сервере, поэтому одинаково для ПК и APK, обновлять клиент не надо.
//
//  Пункт узнаётся по тексту до первого цветового кода, табуляции или
//  скобки: «Банк {AAAAAA}— 1200 м» = «Банк» при любом расстоянии.
//  Память своя для каждого окна и заголовка (без цифр): в GPS у
//  каждого раздела свой последний пункт. Хранится до выхода с сервера.
//
//  Не повторяются: окна меньше чем из 4 пунктов, «Назад/Далее», опасные
//  действия (удалить, продать, выбросить...), окна из Dlg_IsExcluded.
// =====================================================================

#define DLG_BODY_MAX     4096
#define DLG_MAX_ITEMS    128     // пунктов в окне, которые запоминаются
#define DLG_MEM_SLOTS    24      // сколько разных окон помнит игрок
#define DLG_MIN_ITEMS    4       // в коротких окнах повтор не нужен
#define DLG_MARK         "{66CCFF}» "
#define DLG_MARK_LEN     10

static gDlgBuf[DLG_BODY_MAX];
static gDlgOut[DLG_BODY_MAX + 64];

// Что показано сейчас
static gDlgKey[MAX_PLAYERS];                     // окно + заголовок
static gDlgId[MAX_PLAYERS] = { -1, ... };
static gDlgDup[MAX_PLAYERS] = { -1, ... };       // какой пункт повторён сверху (-1 — нет)
static gDlgCount[MAX_PLAYERS];
static gDlgHash[MAX_PLAYERS][DLG_MAX_ITEMS];     // ключи пунктов (0 — не запоминать)

// Что помнит игрок: окно -> пункт
static gDlgMemKey[MAX_PLAYERS][DLG_MEM_SLOTS];
static gDlgMemItem[MAX_PLAYERS][DLG_MEM_SLOTS];
static gDlgMemNext[MAX_PLAYERS];

// Окна, где повтор не нужен или мешает (добавлять сюда)
static stock bool:Dlg_IsExcluded(dialogid)
{
	switch (dialogid)
	{
		case DIALOG_HELP_RESULTS, DIALOG_GENDER: return true;
	}
	return false;
}

// cp1251: строчная буква, ё -> е
stock Dlg_Lower(c)
{
	c &= 0xFF;
	if (c >= 'A' && c <= 'Z') return c + 32;
	if (c >= 0xC0 && c <= 0xDF) return c + 32;
	if (c == 0xA8 || c == 0xB8) return 0xE5;
	return c;
}

// {RRGGBB} с позиции i? (длина 8)
static stock bool:Dlg_IsColor(const s[], i)
{
	if (s[i] != '{') return false;
	for (new k = 1; k <= 6; k++)
	{
		new c = s[i + k];
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
	}
	return (s[i + 7] == '}');
}

// Ключ пункта: текст [start, end) до цвета/таба/скобки, без регистра.
// out — этот текст (для проверки на «Назад» и опасные слова)
static stock Dlg_ItemKey(const s[], start, end, out[], outSize)
{
	new i = start, n = 0;
	while (i < end)
	{
		if (Dlg_IsColor(s, i)) { i += 8; continue; }
		if (s[i] == ' ') { i++; continue; }
		break;
	}
	while (i < end && n < outSize - 1)
	{
		new c = s[i];
		if (c == '\t' || c == '(' || c == '{' || c == '\r') break;
		out[n++] = Dlg_Lower(c);
		i++;
	}
	while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '-' || out[n - 1] == ':' || out[n - 1] == 0x97)) n--;
	out[n] = '\0';
	if (n == 0) return 0;

	// Навигация и опасные действия не повторяем
	static const skip[][] =
	{
		"назад", "далее", "дальше", "<<", ">>", "<", ">", "следующ", "предыдущ",
		"удал", "выброс", "продать", "уволить", "распуст", "сброс", "сдать", "выйти из", "покинуть"
	};
	new w[16];
	for (new k = 0; k < sizeof skip; k++)
	{
		w[0] = '\0';
		strcat(w, skip[k]);
		for (new j = 0; w[j] != '\0'; j++) w[j] = Dlg_Lower(w[j]);
		if (strfind(out, w) == 0) return 0;
	}

	new h = 5381;
	for (new k = 0; k < n; k++) h = h * 33 + out[k];
	if (h == 0) h = 1;
	return h;
}

// Ключ окна: номер + заголовок без цифр и цветов
static stock Dlg_WindowKey(dialogid, const title[])
{
	new h = 5381 + dialogid * 7919;
	for (new i = 0; title[i] != '\0'; i++)
	{
		if (Dlg_IsColor(title, i)) { i += 7; continue; }
		new c = title[i];
		if ((c >= '0' && c <= '9') || c == ' ') continue;
		h = h * 33 + Dlg_Lower(c);
	}
	if (h == 0) h = 1;
	return h;
}

static stock Dlg_MemGet(playerid, key)
{
	for (new s = 0; s < DLG_MEM_SLOTS; s++)
	{
		if (gDlgMemKey[playerid][s] == key) return gDlgMemItem[playerid][s];
	}
	return 0;
}

static stock Dlg_MemSet(playerid, key, item)
{
	for (new s = 0; s < DLG_MEM_SLOTS; s++)
	{
		if (gDlgMemKey[playerid][s] == key)
		{
			gDlgMemItem[playerid][s] = item;
			return;
		}
	}
	new s = gDlgMemNext[playerid];
	gDlgMemKey[playerid][s] = key;
	gDlgMemItem[playerid][s] = item;
	gDlgMemNext[playerid] = (s + 1) % DLG_MEM_SLOTS;
}

stock Dlg_OnConnect(playerid)
{
	for (new s = 0; s < DLG_MEM_SLOTS; s++) gDlgMemKey[playerid][s] = 0;
	gDlgMemNext[playerid] = 0;
	gDlgId[playerid] = -1;
	gDlgDup[playerid] = -1;
	gDlgCount[playerid] = 0;
}

// Список: разобрать пункты, повторить запомненный сверху. Результат в gDlgOut.
static stock bool:Dlg_Prepare(playerid, dialogid, DIALOG_STYLE:style, const title[])
{
	gDlgId[playerid] = dialogid;
	gDlgDup[playerid] = -1;
	gDlgCount[playerid] = 0;
	if (style != DIALOG_STYLE_LIST && style != DIALOG_STYLE_TABLIST && style != DIALOG_STYLE_TABLIST_HEADERS) return false;
	if (Dlg_IsExcluded(dialogid)) return false;

	new key = Dlg_WindowKey(dialogid, title);
	gDlgKey[playerid] = key;
	new want = Dlg_MemGet(playerid, key);

	// Строки: у TABLIST_HEADERS первая — заголовки колонок
	new len = strlen(gDlgBuf), start = 0, line = 0, item = 0;
	new found = -1, foundStart = 0, foundEnd = 0, firstItemStart = -1;
	new txt[64];
	if (style == DIALOG_STYLE_TABLIST_HEADERS)
	{
		while (start < len && gDlgBuf[start] != '\n') start++;
		if (start < len) start++;
		line = 1;
	}
	while (start <= len && item < DLG_MAX_ITEMS)
	{
		new end = start;
		while (end < len && gDlgBuf[end] != '\n') end++;
		if (firstItemStart < 0) firstItemStart = start;
		new h = Dlg_ItemKey(gDlgBuf, start, end, txt, sizeof txt);
		gDlgHash[playerid][item] = h;
		if (want != 0 && h == want && found < 0)
		{
			found = item;
			foundStart = start;
			foundEnd = end;
		}
		item++;
		line++;
		if (end >= len) break;
		start = end + 1;
	}
	gDlgCount[playerid] = item;

	if (found <= 0 || item < DLG_MIN_ITEMS || firstItemStart < 0) return false;
	new dupLen = foundEnd - foundStart;
	if (len + dupLen + DLG_MARK_LEN + 2 >= sizeof gDlgOut) return false;

	// Собираем: [заголовки колонок] + «» пункт» + весь исходный список
	new o = 0;
	for (new i = 0; i < firstItemStart; i++) gDlgOut[o++] = gDlgBuf[i];
	static const mark[] = DLG_MARK;
	for (new i = 0; i < DLG_MARK_LEN; i++) gDlgOut[o++] = mark[i];
	for (new i = foundStart; i < foundEnd; i++) gDlgOut[o++] = gDlgBuf[i];
	gDlgOut[o++] = '\n';
	for (new i = firstItemStart; i < len; i++) gDlgOut[o++] = gDlgBuf[i];
	gDlgOut[o] = '\0';
	gDlgDup[playerid] = found;
	return true;
}

// Замена ShowPlayerDialog (ниже #define): форматирует текст как обычно,
// добавляет повтор последнего выбора и показывает окно.
stock bool:Dlg_Show(playerid, dialogid, DIALOG_STYLE:style, const title[], const body[], const button1[], const button2[], OPEN_MP_TAGS:...)
{
	new n = numargs();
	if (n <= 7)
	{
		gDlgBuf[0] = '\0';
		strcat(gDlgBuf, body);
	}
	else
	{
		// format(gDlgBuf, sizeof gDlgBuf, body, <доп. аргументы>) — аргументы передаём дальше как есть
		new bytes = (n - 7 + 3) * 4, i = n - 1;
		while (i >= 7)
		{
			#emit LCTRL 5
			#emit LOAD.S.alt i
			#emit SHL.C.alt 2
			#emit ADD
			#emit ADD.C 12
			#emit LOAD.I
			#emit PUSH.pri
			i--;
		}
		#emit PUSH.S body
		#emit PUSH.C 4096
		#emit PUSH.C gDlgBuf
		#emit PUSH.S bytes
		#emit SYSREQ.C format
		#emit LCTRL 4
		#emit LOAD.S.alt bytes
		#emit ADD
		#emit ADD.C 4
		#emit SCTRL 4
	}
	if (playerid >= 0 && playerid < MAX_PLAYERS && Dlg_Prepare(playerid, dialogid, style, title))
		return ShowPlayerDialog(playerid, dialogid, style, title, "%s", button1, button2, gDlgOut);
	return ShowPlayerDialog(playerid, dialogid, style, title, "%s", button1, button2, gDlgBuf);
}

// Первой строкой в OnDialogResponse: номер строки -> исходный пункт, запомнить выбор
stock Dlg_OnResponse(playerid, dialogid, response, &listitem, inputtext[])
{
	if (playerid < 0 || playerid >= MAX_PLAYERS || dialogid != gDlgId[playerid]) return;
	new dup = gDlgDup[playerid];
	gDlgDup[playerid] = -1;
	gDlgId[playerid] = -1;
	if (dup >= 0 && listitem >= 0)
	{
		if (listitem == 0)
		{
			listitem = dup;
			// убрать «» » из текста строки, если клиент его прислал
			if (!strcmp(inputtext, "{66CCFF}", false, 8)) strdel(inputtext, 0, 8);
			if ((inputtext[0] & 0xFF) == 0xBB && inputtext[1] == ' ') strdel(inputtext, 0, 2);
		}
		else listitem--;
	}
	if (!response || listitem < 0 || listitem >= gDlgCount[playerid]) return;
	new h = gDlgHash[playerid][listitem];
	if (h != 0) Dlg_MemSet(playerid, gDlgKey[playerid], h);
}

#define ShowPlayerDialog Dlg_Show
