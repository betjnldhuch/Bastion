// =====================================================================
//  textures.inc — перекраска объектов маппинга (админ DEV_LEVEL+).
//
//  Объект из /mo можно обтянуть любой текстурой GTA, залить цветом или
//  сделать на нём надпись (вывески, таблички, номера домов). Это работает
//  у всех: ПК-клиент 0.3.7 и телефон (с APK, где включён SetObjectMaterial).
//  Текстуры GTA берутся из игры, наши (bst_*) приходят с лаунчером/APK. Всё хранится в таблице map_materials и
//  восстанавливается после рестарта, попадает в /mo export.
//
//    /mt               — окно перекраски объекта, который двигаешь (или ближайшего)
//    /mt [№]           — то же для объекта №
//    /mt set [слот] [модель] [txd] [текстура] — любая текстура GTA
//    /mt color [слот] [цвет]  — залить цветом, цвет FF0000 или AARRGGBB
//    /mt text [слот] [размер шрифта] [цвет] [текст]  — надпись, | — новая строка
//    /mt clear [слот|all]     — убрать перекраску
//    /mt info          — что надето на объект
//    /mt save [название] [слот] — сохранить текстуру слота в список
//    /mt unsave [№]    — убрать из списка
//
//  Слот (0–15) — часть модели: у одних объектов 0 — вся модель, у других
//  стены/пол/рамка — разные слоты. Проще всего пробовать по очереди.
//  Названия текстур: dev.prineside.com/ru/gtasa_samp_model_id/ (вкладка
//  текстур у модели) или textures.xyin.ws.
// =====================================================================

#define TEX_SLOTS               16
#define TEX_LIST_MAX            60

#define TEX_KIND_TEXTURE        (0)
#define TEX_KIND_TEXT           (1)

static gTexSlot[MAX_PLAYERS] = { -1, ... };      // объект маппинга (слот в dev.inc)
static gTexIndex[MAX_PLAYERS];                   // слот материала 0–15
static gTexList[MAX_PLAYERS][TEX_LIST_MAX];      // строка окна -> id готовой текстуры

// Готовые текстуры при первом запуске. Название, модель, txd, текстура.
// Проверены не все: если какая-то выглядит не так — /mt unsave и добавь свою.
enum E_TEX_SEED { tsName[32], tsModel, tsTxd[32], tsTex[32] }
static const gTexSeed[][E_TEX_SEED] =
{
	{ "Белый (для заливки)",  10765, "airportgnd_sfse",  "white" },
	{ "Кирпич красный",       9514,  "711_sfw",          "brick" },
	{ "Мрамор",               3922,  "bistro",           "marblekb_256128" },
	{ "Паркет",               14581, "ab_mafiasuitea",   "cof_wood2" },
	{ "Обои",                 14581, "ab_mafiasuitea",   "walp45S" },
	{ "Тротуарная плитка",    10101, "2notherbuildsfe",  "Bow_Abpave_Gen" },
	{ "Штукатурка",           4552,  "ammu_lan2",        "sl_lavicdtwall1" },
	{ "Стекло",               18065, "ab_sfammumain",    "shelf_glas" },
	{ "Золото",               8463,  "vgseland",         "tiadbuddhagold" },
	{ "Доски",                14650, "ab_trukstpc",      "sa_wood08_128" }
};

// Наши текстуры Bastion (client-mods/tools/mapping_textures.py), модель 1240, txd icons5.
// Видны у игроков с обновлённым лаунчером/APK. Добавляются в список при каждом запуске, если их там нет.
enum E_TEX_OWN { toName[32], toTex[32] }
static const gTexOwn[][E_TEX_OWN] =
{
	{ "Больница: стена", "bst_hosp_wall" },
	{ "Банк: дверь хранилища", "bst_vault_door" },
	{ "Банк: стойка кассы", "bst_bank_counter" },
	{ "Банк: мрамор светлый", "bst_marble_beige" },
	{ "Банк: мрамор тёмный", "bst_marble_dark" },
	{ "24/7: стеллаж", "bst_shop_shelves" },
	{ "24/7: холодильник", "bst_shop_fridge" },
	{ "24/7: плитка пола", "bst_shop_floor" },
	{ "24/7: вывеска", "bst_shop_sign" },
	{ "Бар: полки с бутылками", "bst_bar_shelves" },
	{ "Бар: стойка", "bst_bar_counter" },
	{ "Вывеска автосалона", "bst_dealer_sign" },
	{ "Граффити Ballas", "bst_ballas_tag" },
	{ "Флаг Grove", "bst_grove_flag" },
	{ "Кирпич жёлтый облезлый", "bst_yellow_brick" },
	{ "Асфальт", "bst_asphalt" },
	{ "Кирпич красный (наш)", "bst_brick_red" },
	{ "Ковролин синий", "bst_carpet_blue" },
	{ "Бетонная плитка", "bst_concrete" },
	{ "Профлист", "bst_corrugated" },
	{ "Пол гаража", "bst_garage_floor" },
	{ "Паркет (наш)", "bst_parquet" },
	{ "Штукатурка бежевая", "bst_plaster" },
	{ "Кожа чёрная", "bst_leather" },
	{ "Ткань синяя", "bst_fabric_blue" },
	{ "Доски (наши)", "bst_planks" },
	{ "Полиция: ростомер", "bst_height_chart" },
	{ "Табличка «Камеры»", "bst_sign_cells" },
	{ "Табличка «Допросная»", "bst_sign_interrog" },
	{ "Табличка «Раздевалка»", "bst_sign_lockers" },
	{ "Табличка «Оружейная»", "bst_sign_armory" }
};

enum E_TEX_COLOR { tcName[24], tcColor }
static const gTexColors[][E_TEX_COLOR] =
{
	{ "Белый",       0xFFFFFFFF },
	{ "Чёрный",      0xFF111111 },
	{ "Серый",       0xFF808080 },
	{ "Красный",     0xFFCC2222 },
	{ "Бордовый",    0xFF6E1414 },
	{ "Оранжевый",   0xFFFF8800 },
	{ "Жёлтый",      0xFFFFD21E },
	{ "Зелёный",     0xFF2E8B3A },
	{ "Синий",       0xFF1E3C96 },
	{ "Голубой",     0xFF64B4FF },
	{ "Фиолетовый",  0xFF7832A0 },
	{ "Розовый",     0xFFFF78B4 },
	{ "Коричневый",  0xFF5A3C1E },
	{ "Бежевый",     0xFFE6D2AA }
};

// ---------------------------------------------------------------------
//  Запуск (из Dev_Init, после загрузки объектов)
// ---------------------------------------------------------------------
stock Tex_Init()
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS map_materials (id INTEGER PRIMARY KEY AUTOINCREMENT, obj INTEGER, idx INTEGER, kind INTEGER, model INTEGER, txd TEXT, tex TEXT, color INTEGER, text TEXT, fsize INTEGER, UNIQUE(obj, idx))"));
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "CREATE TABLE IF NOT EXISTS texture_presets (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, model INTEGER, txd TEXT, tex TEXT, author TEXT)"));

	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT COUNT(*) FROM texture_presets");
	new count = 0;
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0) count = DB_GetFieldInt(r, 0);
		DB_FreeResultSet(r);
	}
	if (count == 0)
	{
		for (new i = 0; i < sizeof gTexSeed; i++)
		{
			DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO texture_presets (name, model, txd, tex, author) VALUES ('%q', %d, '%q', '%q', 'server')",
				gTexSeed[i][tsName], gTexSeed[i][tsModel], gTexSeed[i][tsTxd], gTexSeed[i][tsTex]));
		}
	}

	for (new i = 0; i < sizeof gTexOwn; i++)
	{
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO texture_presets (name, model, txd, tex, author) SELECT '%q', 1240, 'icons5', '%q', 'bastion' WHERE NOT EXISTS (SELECT 1 FROM texture_presets WHERE tex = '%q')",
			gTexOwn[i][toName], gTexOwn[i][toTex], gTexOwn[i][toTex]));
	}

	new applied = 0;
	r = DB_ExecuteQuery(gDB, "SELECT obj, idx, kind, model, txd, tex, color, text, fsize FROM map_materials ORDER BY obj");
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new o = Map_ObjectByDbId(DB_GetFieldInt(r, 0));
			if (o != INVALID_OBJECT_ID)
			{
				Tex_ApplyRow(o, r);
				applied++;
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	printf("[Tex] Map materials: %d.", applied);
}

stock Tex_OnConnect(playerid)
{
	gTexSlot[playerid] = -1;
}

// Строка результата: obj, idx, kind, model, txd, tex, color, text, fsize
static Tex_ApplyRow(objectid, DBResult:r)
{
	new idx = DB_GetFieldInt(r, 1), color = DB_GetFieldInt(r, 6);
	if (DB_GetFieldInt(r, 2) == TEX_KIND_TEXT)
	{
		new text[144];
		DB_GetFieldString(r, 7, text, sizeof text);
		Tex_ApplyText(objectid, idx, text, DB_GetFieldInt(r, 8), color);
	}
	else
	{
		new txd[32], tex[32];
		DB_GetFieldString(r, 4, txd, sizeof txd);
		DB_GetFieldString(r, 5, tex, sizeof tex);
		SetObjectMaterial(objectid, idx, DB_GetFieldInt(r, 3), txd, tex, color);
	}
}

// | в тексте -> перенос строки
static Tex_ApplyText(objectid, idx, const text[], fsize, color)
{
	new buf[144];
	strcat(buf, text);
	for (new i = 0; buf[i] != '\0'; i++)
	{
		if (buf[i] == '|') buf[i] = '\n';
	}
	SetObjectMaterialText(objectid, "%s", idx, OBJECT_MATERIAL_SIZE_256x128, "Arial", fsize, true, color, 0, OBJECT_MATERIAL_TEXT_ALIGN_CENT, buf);
}

// Надеть на объект всё сохранённое для него (после копирования / пересоздания)
stock Tex_ApplyFor(dbid, objectid)
{
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT obj, idx, kind, model, txd, tex, color, text, fsize FROM map_materials WHERE obj = %d", dbid);
	if (r == DBResult:0) return 0;
	new rows = DB_GetRowCount(r);
	for (new i = 0; i < rows; i++)
	{
		Tex_ApplyRow(objectid, r);
		DB_SelectNextRow(r);
	}
	DB_FreeResultSet(r);
	return rows;
}

// Из /mo copy: перенести перекраску на копию
stock Tex_OnMapCopy(srcDb, dstDb, dstObject)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO map_materials (obj, idx, kind, model, txd, tex, color, text, fsize) SELECT %d, idx, kind, model, txd, tex, color, text, fsize FROM map_materials WHERE obj = %d",
		dstDb, srcDb));
	Tex_ApplyFor(dstDb, dstObject);
}

// Из /mo del
stock Tex_OnMapDelete(dbid)
{
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM map_materials WHERE obj = %d", dbid));
}

// Из /mo export: строки SetObjectMaterial(Text) для объекта в переменной tmpobjid.
// Возвращает число строк. countOnly — только посчитать.
stock Tex_Export(File:f, dbid, bool:countOnly = false)
{
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT obj, idx, kind, model, txd, tex, color, text, fsize FROM map_materials WHERE obj = %d ORDER BY idx", dbid);
	if (r == DBResult:0) return 0;
	new rows = DB_GetRowCount(r);
	if (!countOnly)
	{
		new line[256], txd[32], tex[32], text[144];
		for (new i = 0; i < rows; i++)
		{
			if (DB_GetFieldInt(r, 2) == TEX_KIND_TEXT)
			{
				DB_GetFieldString(r, 7, text, sizeof text);
				for (new c = 0; text[c] != '\0'; c++)
				{
					if (text[c] == '"') text[c] = '\'';
				}
				format(line, sizeof line, "SetObjectMaterialText(tmpobjid, \"%s\", %d, OBJECT_MATERIAL_SIZE_256x128, \"Arial\", %d, true, 0x%08x, 0, OBJECT_MATERIAL_TEXT_ALIGN_CENTER);\r\n",
					text, DB_GetFieldInt(r, 1), DB_GetFieldInt(r, 8), DB_GetFieldInt(r, 6));
			}
			else
			{
				DB_GetFieldString(r, 4, txd, sizeof txd);
				DB_GetFieldString(r, 5, tex, sizeof tex);
				format(line, sizeof line, "SetObjectMaterial(tmpobjid, %d, %d, \"%s\", \"%s\", 0x%08x);\r\n",
					DB_GetFieldInt(r, 1), DB_GetFieldInt(r, 3), txd, tex, DB_GetFieldInt(r, 6));
			}
			fwrite(f, line);
			DB_SelectNextRow(r);
		}
	}
	DB_FreeResultSet(r);
	return rows;
}

// ---------------------------------------------------------------------
//  Изменение
// ---------------------------------------------------------------------
static Tex_SetTexture(playerid, s, idx, model, const txd[], const tex[], color)
{
	new dbid = Map_DbIdOf(s);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO map_materials (obj, idx, kind, model, txd, tex, color, text, fsize) VALUES (%d, %d, %d, %d, '%q', '%q', %d, '', 0)",
		dbid, idx, TEX_KIND_TEXTURE, model, txd, tex, color));
	SetObjectMaterial(Map_ObjectOf(s), idx, model, txd, tex, color);
	Admin_Log(playerid, "mt set", INVALID_PLAYER_ID, tex, model);
	SendClientMessage(playerid, COLOR_GREEN, "Объект №%d, слот %d: %d %s %s. Не то место — попробуй другой слот.", s + 1, idx, model, txd, tex);
}

static Tex_SetText(playerid, s, idx, const text[], fsize, color)
{
	new dbid = Map_DbIdOf(s);
	DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT OR REPLACE INTO map_materials (obj, idx, kind, model, txd, tex, color, text, fsize) VALUES (%d, %d, %d, 0, '', '', %d, '%q', %d)",
		dbid, idx, TEX_KIND_TEXT, color, text, fsize));
	Tex_ApplyText(Map_ObjectOf(s), idx, text, fsize, color);
	Admin_Log(playerid, "mt text", INVALID_PLAYER_ID, text, idx);
	SendClientMessage(playerid, COLOR_GREEN, "Объект №%d, слот %d: надпись «%s».", s + 1, idx, text);
}

// Цвет: залить цветом. Если в слоте уже текстура — покрасить её, иначе белая + цвет.
static Tex_SetColor(playerid, s, idx, color)
{
	new dbid = Map_DbIdOf(s);
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT model, txd, tex FROM map_materials WHERE obj = %d AND idx = %d AND kind = %d", dbid, idx, TEX_KIND_TEXTURE);
	new model = gTexSeed[0][tsModel], txd[32], tex[32];
	strcat(txd, gTexSeed[0][tsTxd]);
	strcat(tex, gTexSeed[0][tsTex]);
	if (r != DBResult:0)
	{
		if (DB_GetRowCount(r) > 0)
		{
			model = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 1, txd, sizeof txd);
			DB_GetFieldString(r, 2, tex, sizeof tex);
		}
		DB_FreeResultSet(r);
	}
	Tex_SetTexture(playerid, s, idx, model, txd, tex, color);
}

// Убрать перекраску: в SA-MP её нельзя снять, объект пересоздаётся
static Tex_Clear(playerid, s, idx)
{
	new dbid = Map_DbIdOf(s);
	if (idx < 0) DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM map_materials WHERE obj = %d", dbid));
	else DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM map_materials WHERE obj = %d AND idx = %d", dbid, idx));
	Map_Rebuild(s);
	Admin_Log(playerid, "mt clear", INVALID_PLAYER_ID, "", idx);
	if (idx < 0) SendClientMessage(playerid, COLOR_WHITE, "С объекта №%d снята вся перекраска.", s + 1);
	else SendClientMessage(playerid, COLOR_WHITE, "Объект №%d, слот %d: перекраска снята.", s + 1, idx);
}

// "FF0000" -> 0xFFFF0000, "80FF0000" -> 0x80FF0000. Возвращает false, если не цвет.
static bool:Tex_ParseColor(const str[], &color)
{
	new len = strlen(str), start = 0;
	if (len >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) start = 2;
	else if (len >= 1 && str[0] == '#') start = 1;
	new digits = len - start;
	if (digits != 6 && digits != 8) return false;
	new value = 0;
	for (new i = start; i < len; i++)
	{
		new c = str[i], d;
		if (c >= '0' && c <= '9') d = c - '0';
		else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
		else return false;
		value = (value << 4) | d;
	}
	if (digits == 6) value |= 0xFF000000;
	color = value;
	return true;
}

// Имя txd/текстуры: только латиница, цифры, _ - . и пробел не нужен
static bool:Tex_ValidName(const str[])
{
	if (str[0] == '\0') return false;
	for (new i = 0; str[i] != '\0'; i++)
	{
		new c = str[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
	}
	return true;
}

static bool:Tex_NextIndex(const params[], &pos, &idx)
{
	return (NextIntParam(params, pos, idx) && idx >= 0 && idx < TEX_SLOTS);
}

// ---------------------------------------------------------------------
//  Окна
// ---------------------------------------------------------------------
static Tex_ShowSlots(playerid)
{
	new s = gTexSlot[playerid];
	new desc[TEX_SLOTS][48];
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT idx, kind, tex, text, color FROM map_materials WHERE obj = %d", Map_DbIdOf(s));
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r);
		for (new i = 0; i < rows; i++)
		{
			new idx = DB_GetFieldInt(r, 0);
			if (idx >= 0 && idx < TEX_SLOTS)
			{
				new name[40];
				if (DB_GetFieldInt(r, 1) == TEX_KIND_TEXT)
				{
					DB_GetFieldString(r, 3, name, sizeof name);
					format(desc[idx], sizeof desc[], "{FFFF66}надпись: %s", name);
				}
				else
				{
					DB_GetFieldString(r, 2, name, sizeof name);
					format(desc[idx], sizeof desc[], "{%06x}## {FFFFFF}%s", DB_GetFieldInt(r, 4) & 0xFFFFFF, name);
				}
			}
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	new text[TEX_SLOTS * 64], line[64];
	strcat(text, "Слот\tСейчас\n");
	for (new i = 0; i < TEX_SLOTS; i++)
	{
		if (desc[i][0] != '\0') format(line, sizeof line, "%d\t%s\n", i, desc[i]);
		else format(line, sizeof line, "%d\t{808080}как в игре\n", i);
		strcat(text, line);
	}
	new title[64];
	format(title, sizeof title, "Перекраска объекта №%d (модель %d)", s + 1, GetObjectModel(Map_ObjectOf(s)));
	ShowPlayerDialog(playerid, DIALOG_TEX_SLOTS, DIALOG_STYLE_TABLIST_HEADERS, title, "%s", "Выбрать", "Закрыть", text);
}

static Tex_ShowPick(playerid)
{
	new text[4000], line[96], n = 0;
	strcat(text, "{FF6666}Убрать перекраску слота\n{66FF66}Залить цветом...\n{FFFF66}Надпись...\n");
	new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id, name, model, tex FROM texture_presets ORDER BY id LIMIT %d", TEX_LIST_MAX);
	if (r != DBResult:0)
	{
		new rows = DB_GetRowCount(r), name[32], tex[32];
		for (new i = 0; i < rows && n < TEX_LIST_MAX; i++)
		{
			gTexList[playerid][n++] = DB_GetFieldInt(r, 0);
			DB_GetFieldString(r, 1, name, sizeof name);
			DB_GetFieldString(r, 3, tex, sizeof tex);
			format(line, sizeof line, "{FFFFFF}%d. %s {808080}(%d %s)\n", n, name, DB_GetFieldInt(r, 2), tex);
			strcat(text, line);
			DB_SelectNextRow(r);
		}
		DB_FreeResultSet(r);
	}
	for (new i = n; i < TEX_LIST_MAX; i++) gTexList[playerid][i] = 0;
	new title[48];
	format(title, sizeof title, "Объект №%d, слот %d", gTexSlot[playerid] + 1, gTexIndex[playerid]);
	ShowPlayerDialog(playerid, DIALOG_TEX_PICK, DIALOG_STYLE_LIST, title, "%s", "Надеть", "Назад", text);
}

static Tex_ShowColors(playerid)
{
	new text[800], line[48];
	for (new i = 0; i < sizeof gTexColors; i++)
	{
		format(line, sizeof line, "{%06x}##### {FFFFFF}%s\n", gTexColors[i][tcColor] & 0xFFFFFF, gTexColors[i][tcName]);
		strcat(text, line);
	}
	strcat(text, "{AFAFAF}Свой цвет: /mt color [слот] FF8800");
	ShowPlayerDialog(playerid, DIALOG_TEX_COLOR, DIALOG_STYLE_LIST, "Цвет", "%s", "Залить", "Назад", text);
}

static Tex_Help(playerid)
{
	new text[1200];
	strcat(text, "{FFFF00}Перекраска объектов маппинга (видно и на ПК, и на телефоне)\n{FFFFFF}");
	strcat(text, "/mt — окно для объекта, который двигаешь, или ближайшего   /mt [№] — для объекта №\n");
	strcat(text, "/mt set [слот] [модель] [txd] [текстура] — любая текстура GTA\n");
	strcat(text, "/mt color [слот] [цвет] — залить цветом: FF8800 или с прозрачностью 80FF8800\n");
	strcat(text, "/mt text [слот] [размер] [цвет] [текст] — надпись, | — новая строка\n");
	strcat(text, "/mt clear [слот|all] — снять   /mt info — что надето\n");
	strcat(text, "/mt save [название] [слот] — запомнить текстуру слота в общий список   /mt unsave [№]\n\n");
	strcat(text, "{AFAFAF}Слот 0–15 — часть модели, пробуй по очереди.\nТекстуры: dev.prineside.com/ru/gtasa_samp_model_id/ (у модели вкладка Textures)");
	ShowPlayerDialog(playerid, DIALOG_TEX_HELP, DIALOG_STYLE_MSGBOX, "Перекраска /mt", "%s", "Закрыть", "", text);
}

// ---------------------------------------------------------------------
//  Команда /mt
// ---------------------------------------------------------------------
stock bool:Cmd_Mt(playerid, const params[])
{
	if (!Admin_Check(playerid, DEV_LEVEL)) return true;
	new pos = 0, sub[16];
	if (!NextParam(params, pos, sub) || IsNumeric(sub))
	{
		new s = -1;
		if (sub[0] != '\0')
		{
			new num = strval(sub);
			if (num >= 1 && num <= Map_Count()) s = num - 1;
		}
		else s = Map_SelectedSlot(playerid);
		if (s == -1) return SendClientMessage(playerid, COLOR_GREY, "Объект не найден. Подойди к объекту из /mo или укажи номер: /mt [№] (номера — /mo near). Справка: /mt help");
		gTexSlot[playerid] = s;
		Tex_ShowSlots(playerid);
		return true;
	}
	if (!strcmp(sub, "help", true))
	{
		Tex_Help(playerid);
		return true;
	}
	if (!strcmp(sub, "unsave", true))
	{
		new num;
		if (!NextIntParam(params, pos, num) || num < 1) return SendClientMessage(playerid, COLOR_GREY, "Использование: /mt unsave [№ из списка в окне /mt]");
		new DBResult:r = DB_ExecuteQuery(gDB, "SELECT id FROM texture_presets ORDER BY id LIMIT 1 OFFSET %d", num - 1);
		new id = 0;
		if (r != DBResult:0)
		{
			if (DB_GetRowCount(r) > 0) id = DB_GetFieldInt(r, 0);
			DB_FreeResultSet(r);
		}
		if (id == 0) return SendClientMessage(playerid, COLOR_GREY, "В списке нет №%d.", num);
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "DELETE FROM texture_presets WHERE id = %d", id));
		return SendClientMessage(playerid, COLOR_WHITE, "Текстура №%d убрана из списка.", num);
	}

	new s = Map_SelectedSlot(playerid);
	if (s == -1) return SendClientMessage(playerid, COLOR_GREY, "Сначала выбери объект: /mo edit [№] или подойди к нему ближе 30 м.");
	gTexSlot[playerid] = s;

	if (!strcmp(sub, "info", true))
	{
		new DBResult:r = DB_ExecuteQuery(gDB, "SELECT idx, kind, model, txd, tex, color, text FROM map_materials WHERE obj = %d ORDER BY idx", Map_DbIdOf(s));
		new rows = 0;
		SendClientMessage(playerid, COLOR_YELLOW, "Объект №%d (модель %d):", s + 1, GetObjectModel(Map_ObjectOf(s)));
		if (r != DBResult:0)
		{
			rows = DB_GetRowCount(r);
			new txd[32], tex[32], text[144];
			for (new i = 0; i < rows; i++)
			{
				if (DB_GetFieldInt(r, 1) == TEX_KIND_TEXT)
				{
					DB_GetFieldString(r, 6, text, sizeof text);
					SendClientMessage(playerid, COLOR_WHITE, " слот %d: надпись «%s», цвет %08x", DB_GetFieldInt(r, 0), text, DB_GetFieldInt(r, 5));
				}
				else
				{
					DB_GetFieldString(r, 3, txd, sizeof txd);
					DB_GetFieldString(r, 4, tex, sizeof tex);
					SendClientMessage(playerid, COLOR_WHITE, " слот %d: %d %s %s, цвет %08x", DB_GetFieldInt(r, 0), DB_GetFieldInt(r, 2), txd, tex, DB_GetFieldInt(r, 5));
				}
				DB_SelectNextRow(r);
			}
			DB_FreeResultSet(r);
		}
		if (rows == 0) SendClientMessage(playerid, COLOR_GREY, " перекраски нет.");
		return true;
	}
	if (!strcmp(sub, "set", true))
	{
		new idx, model, txd[32], tex[32], ctext[12], color = 0;
		if (!Tex_NextIndex(params, pos, idx) || !NextIntParam(params, pos, model) || !NextParam(params, pos, txd) || !NextParam(params, pos, tex)
			|| !Tex_ValidName(txd) || !Tex_ValidName(tex))
			return SendClientMessage(playerid, COLOR_GREY, "Использование: /mt set [слот 0-15] [модель] [txd] [текстура] [цвет], например /mt set 0 9514 711_sfw brick");
		if (NextParam(params, pos, ctext) && !Tex_ParseColor(ctext, color))
			return SendClientMessage(playerid, COLOR_GREY, "Цвет пишется так: FF8800 (или 80FF8800 с прозрачностью).");
		Tex_SetTexture(playerid, s, idx, model, txd, tex, color);
		return true;
	}
	if (!strcmp(sub, "color", true))
	{
		new idx, ctext[12], color;
		if (!Tex_NextIndex(params, pos, idx) || !NextParam(params, pos, ctext) || !Tex_ParseColor(ctext, color))
			return SendClientMessage(playerid, COLOR_GREY, "Использование: /mt color [слот 0-15] [цвет], например /mt color 0 FF8800");
		Tex_SetColor(playerid, s, idx, color);
		return true;
	}
	if (!strcmp(sub, "text", true))
	{
		new idx, fsize, ctext[12], color, text[100];
		if (!Tex_NextIndex(params, pos, idx) || !NextIntParam(params, pos, fsize) || !NextParam(params, pos, ctext) || !Tex_ParseColor(ctext, color)
			|| !RestParam(params, pos, text))
			return SendClientMessage(playerid, COLOR_GREY, "Использование: /mt text [слот] [размер 10-60] [цвет] [текст], например /mt text 0 30 FFFFFF Мэрия|Лос-Сантос");
		if (fsize < 10 || fsize > 60) return SendClientMessage(playerid, COLOR_GREY, "Размер шрифта от 10 до 60.");
		Tex_SetText(playerid, s, idx, text, fsize, color);
		return true;
	}
	if (!strcmp(sub, "clear", true))
	{
		new arg[8], idx = -1;
		if (!NextParam(params, pos, arg)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /mt clear [слот 0-15] или /mt clear all");
		if (strcmp(arg, "all", true))
		{
			if (!IsNumeric(arg)) return SendClientMessage(playerid, COLOR_GREY, "Использование: /mt clear [слот 0-15] или /mt clear all");
			idx = strval(arg);
			if (idx < 0 || idx >= TEX_SLOTS) return SendClientMessage(playerid, COLOR_GREY, "Слот от 0 до 15.");
		}
		Tex_Clear(playerid, s, idx);
		return true;
	}
	if (!strcmp(sub, "save", true))
	{
		new name[32], idx;
		if (!NextParam(params, pos, name) || !Tex_NextIndex(params, pos, idx))
			return SendClientMessage(playerid, COLOR_GREY, "Использование: /mt save [название_без_пробелов] [слот], например /mt save плитка_кухня 0");
		new DBResult:r = DB_ExecuteQuery(gDB, "SELECT model, txd, tex FROM map_materials WHERE obj = %d AND idx = %d AND kind = %d", Map_DbIdOf(s), idx, TEX_KIND_TEXTURE);
		new model = -1, txd[32], tex[32];
		if (r != DBResult:0)
		{
			if (DB_GetRowCount(r) > 0)
			{
				model = DB_GetFieldInt(r, 0);
				DB_GetFieldString(r, 1, txd, sizeof txd);
				DB_GetFieldString(r, 2, tex, sizeof tex);
			}
			DB_FreeResultSet(r);
		}
		if (model == -1) return SendClientMessage(playerid, COLOR_GREY, "В слоте %d объекта №%d нет текстуры. Сначала надень её: /mt set", idx, s + 1);
		new author[MAX_PLAYER_NAME];
		GetPlayerName(playerid, author, sizeof author);
		DB_FreeResultSet(DB_ExecuteQuery(gDB, "INSERT INTO texture_presets (name, model, txd, tex, author) VALUES ('%q', %d, '%q', '%q', '%q')", name, model, txd, tex, author));
		return SendClientMessage(playerid, COLOR_GREEN, "Текстура %s сохранена в список как «%s».", tex, name);
	}
	Tex_Help(playerid);
	return true;
}

// ---------------------------------------------------------------------
//  Ответы на окна (из OnDialogResponse)
// ---------------------------------------------------------------------
stock Tex_OnDialog(playerid, dialogid, response, listitem, const inputtext[])
{
	if (dialogid == DIALOG_TEX_HELP) return 1;
	if (dialogid < DIALOG_TEX_SLOTS || dialogid > DIALOG_TEX_TEXT) return 0;
	new s = gTexSlot[playerid];
	if (s < 0 || s >= Map_Count()) return 1;

	switch (dialogid)
	{
		case DIALOG_TEX_SLOTS:
		{
			if (!response || listitem < 0 || listitem >= TEX_SLOTS) return 1;
			gTexIndex[playerid] = listitem;
			Tex_ShowPick(playerid);
		}
		case DIALOG_TEX_PICK:
		{
			if (!response)
			{
				Tex_ShowSlots(playerid);
				return 1;
			}
			new idx = gTexIndex[playerid];
			if (listitem == 0)
			{
				Tex_Clear(playerid, s, idx);
				Tex_ShowSlots(playerid);
			}
			else if (listitem == 1) Tex_ShowColors(playerid);
			else if (listitem == 2)
			{
				ShowPlayerDialog(playerid, DIALOG_TEX_TEXT, DIALOG_STYLE_INPUT, "Надпись",
					"Текст надписи (белый, размер 30). | — новая строка.\nЦвет и размер: /mt text [слот] [размер] [цвет] [текст]", "Готово", "Назад");
			}
			else
			{
				new n = listitem - 3;
				if (n >= TEX_LIST_MAX || gTexList[playerid][n] == 0) return 1;
				new DBResult:r = DB_ExecuteQuery(gDB, "SELECT model, txd, tex FROM texture_presets WHERE id = %d", gTexList[playerid][n]);
				if (r == DBResult:0) return 1;
				if (DB_GetRowCount(r) > 0)
				{
					new txd[32], tex[32];
					DB_GetFieldString(r, 1, txd, sizeof txd);
					DB_GetFieldString(r, 2, tex, sizeof tex);
					Tex_SetTexture(playerid, s, idx, DB_GetFieldInt(r, 0), txd, tex, 0);
				}
				DB_FreeResultSet(r);
				Tex_ShowSlots(playerid);
			}
		}
		case DIALOG_TEX_COLOR:
		{
			if (!response || listitem < 0 || listitem >= sizeof gTexColors)
			{
				Tex_ShowPick(playerid);
				return 1;
			}
			Tex_SetColor(playerid, s, gTexIndex[playerid], gTexColors[listitem][tcColor]);
			Tex_ShowSlots(playerid);
		}
		case DIALOG_TEX_TEXT:
		{
			if (!response || inputtext[0] == '\0')
			{
				Tex_ShowPick(playerid);
				return 1;
			}
			new text[100];
			strcat(text, inputtext);
			Tex_SetText(playerid, s, gTexIndex[playerid], text, 30, 0xFFFFFFFF);
			Tex_ShowSlots(playerid);
		}
	}
	return 1;
}
