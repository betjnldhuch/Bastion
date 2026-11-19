// =====================================================================
//  interiors.inc — свои интерьеры, собранные из объектов (маппинг).
//
//  Интерьеры стоят высоко в небе, в своём виртуальном мире. Объекты
//  создаются обычным CreateObject (плагинов нет), поэтому их немного.
//
//  Автосалон «Bastion Motors»: зал 19 x 29 м, 6 подиумов под машины,
//  стойка менеджера, зона ожидания. Вход — строка «Автосалон» в doors.inc
//  (стрелку у здания можно подвинуть: /markers).
//
//  Для других систем (автосалон, продажа техники):
//    SALON_INTERIOR, SALON_WORLD        — интерьер и мир зала
//    Salon_GetSlot(i, x, y, z, a)       — место машины на подиуме, i = 0..SALON_SLOTS-1
//    Salon_GetBuyPoint(x, y, z)         — точка у стойки менеджера (покупка)
//    Salon_IsInside(playerid)           — игрок в зале
//  Машину на подиум: CreateVehicle по Salon_GetSlot, затем
//    LinkVehicleToInterior(v, SALON_INTERIOR) и SetVehicleVirtualWorld(v, SALON_WORLD).
//
//  Расстановка сделана по размерам моделей, вживую не проверялась:
//  если что-то стоит криво — поправить числа здесь.
// =====================================================================

#define SALON_INTERIOR      1
#define SALON_WORLD         120

// Центр пола зала (верх пола = SALON_Z)
#define SALON_X             (2850.0)
#define SALON_Y             (-2850.0)
#define SALON_Z             (1500.0)

// Где игрок появляется внутри (у двери, лицом в зал)
#define SALON_IN_X          (2850.0)
#define SALON_IN_Y          (-2862.2)
#define SALON_IN_Z          (1501.0)
#define SALON_IN_A          (0.0)

#define SALON_SLOTS         6
#define SALON_HEIGHT        (6.0)       // высота потолка
#define INT_DRAW_DISTANCE   (120.0)

// Модели: панель 10.5 x 9.6 м и панель 3.5 x 3.2 м (толщина 0.17)
#define INT_PANEL_BIG       19377
#define INT_PANEL_SMALL     19353

#define INT_COL_WALL        0xFFE9E9EC
#define INT_COL_CEILING     0xFF2A2C31
#define INT_COL_PODIUM      0xFF8E1B1B
#define INT_COL_SIGN        0xFF15161A

// Места машин: x, y от центра зала, угол. Слева смотрят вправо, справа — влево (носом в проход).
static const Float:gSalonSlot[SALON_SLOTS][3] =
{
	{ -5.9, -7.0, 270.0 },
	{ -5.9,  0.0, 270.0 },
	{ -5.9,  7.0, 270.0 },
	{  5.9, -7.0,  90.0 },
	{  5.9,  0.0,  90.0 },
	{  5.9,  7.0,  90.0 }
};

static gSalonActor = INVALID_ACTOR_ID;

static Salon_Obj(model, Float:x, Float:y, Float:z, Float:rx = 0.0, Float:ry = 0.0, Float:rz = 0.0)
{
	return CreateObject(model, SALON_X + x, SALON_Y + y, SALON_Z + z, rx, ry, rz, INT_DRAW_DISTANCE);
}

static Salon_Paint(objectid, color)
{
	SetObjectMaterial(objectid, 0, 10765, "airportgnd_sfse", "white", color);
}

static Salon_Build()
{
	new o;

	// Пол и потолок: 2 x 3 плиты, лежат плашмя
	for (new ix = 0; ix < 2; ix++) for (new iy = 0; iy < 3; iy++)
	{
		new Float:x = (ix == 0) ? -5.25 : 5.25, Float:y = (iy - 1) * 9.6;
		o = Salon_Obj(INT_PANEL_BIG, x, y, -0.086, 0.0, 90.0, 0.0);
		SetObjectMaterial(o, 0, 3922, "bistro", "marblekb_256128", 0);
		o = Salon_Obj(INT_PANEL_BIG, x, y, SALON_HEIGHT + 0.086, 0.0, 90.0, 0.0);
		Salon_Paint(o, INT_COL_CEILING);
	}

	// Боковые стены (вдоль Y) и торцевые (вдоль X). Панель выше потолка, низ уходит под пол.
	for (new iy = 0; iy < 3; iy++)
	{
		o = Salon_Obj(INT_PANEL_BIG, -9.686, (iy - 1) * 9.6, SALON_HEIGHT - 5.25);
		Salon_Paint(o, INT_COL_WALL);
		o = Salon_Obj(INT_PANEL_BIG, 9.686, (iy - 1) * 9.6, SALON_HEIGHT - 5.25);
		Salon_Paint(o, INT_COL_WALL);
	}
	for (new ix = 0; ix < 2; ix++)
	{
		new Float:x = (ix == 0) ? -4.8 : 4.8;
		o = Salon_Obj(INT_PANEL_BIG, x, -14.486, SALON_HEIGHT - 5.25, 0.0, 0.0, 90.0);
		Salon_Paint(o, INT_COL_WALL);
		o = Salon_Obj(INT_PANEL_BIG, x, 14.486, SALON_HEIGHT - 5.25, 0.0, 0.0, 90.0);
		Salon_Paint(o, INT_COL_WALL);
	}

	// Подиумы под машины (7 x 3.2 м, на 6 см выше пола), по столбику света у углов, лампа над каждым
	for (new s = 0; s < SALON_SLOTS; s++)
	{
		new Float:sx = gSalonSlot[s][0], Float:sy = gSalonSlot[s][1];
		new Float:side = (sx < 0.0) ? -1.0 : 1.0;
		o = Salon_Obj(INT_PANEL_SMALL, sx - 1.75, sy, -0.025, 0.0, 90.0, 0.0);
		Salon_Paint(o, INT_COL_PODIUM);
		o = Salon_Obj(INT_PANEL_SMALL, sx + 1.75, sy, -0.025, 0.0, 90.0, 0.0);
		Salon_Paint(o, INT_COL_PODIUM);
		Salon_Obj(19121, side * 2.2, sy - 1.8, 0.5);
		Salon_Obj(19121, side * 2.2, sy + 1.8, 0.5);
		Salon_Obj(1893, sx, sy, SALON_HEIGHT - 0.05);
	}

	// Вывеска на дальней стене
	o = Salon_Obj(INT_PANEL_SMALL, 0.0, 14.30, 3.9, 0.0, 0.0, 90.0);
	SetObjectMaterialText(o, "BASTION\nMOTORS", 0, OBJECT_MATERIAL_SIZE_256x256, "Arial", 56, true, 0xFFFFFFFF, INT_COL_SIGN, OBJECT_MATERIAL_TEXT_ALIGN_CENTER);

	// Стойка менеджера у дальней стены
	Salon_Obj(2008, -2.0, 11.6, 0.0, 0.0, 0.0, 180.0);
	Salon_Obj(2008, 2.0, 11.6, 0.0, 0.0, 0.0, 180.0);
	Salon_Obj(1714, -1.0, 13.0, 0.0, 0.0, 0.0, 180.0);
	Salon_Obj(1714, 3.0, 13.0, 0.0, 0.0, 0.0, 180.0);

	// Зона ожидания у входа: диваны вдоль боковых стен, столик
	Salon_Obj(1726, -9.0, -11.0, 0.0, 0.0, 0.0, 90.0);
	Salon_Obj(1726, 9.0, -13.0, 0.0, 0.0, 0.0, 270.0);
	Salon_Obj(2315, -7.4, -12.6, 0.0, 0.0, 0.0, 90.0);
	Salon_Obj(2315, 7.4, -11.4, 0.0, 0.0, 0.0, 90.0);

	// Растения по углам и у стойки
	Salon_Obj(2010, -9.0, -13.8, 0.0);
	Salon_Obj(2010, 9.0, -13.8, 0.0);
	Salon_Obj(2010, -9.0, 13.8, 0.0);
	Salon_Obj(2010, 9.0, 13.8, 0.0);
	Salon_Obj(2011, -4.6, 13.6, 0.0);
	Salon_Obj(2011, 4.6, 13.6, 0.0);

	// Дверь (декорация) за спиной у входящего
	Salon_Obj(1569, -0.75, -14.36, 0.0);

	// Менеджер за стойкой
	gSalonActor = CreateActor(17, SALON_X, SALON_Y + 12.9, SALON_Z + 1.0, 180.0);
	if (gSalonActor != INVALID_ACTOR_ID)
	{
		SetActorVirtualWorld(gSalonActor, SALON_WORLD);
		SetActorInvulnerable(gSalonActor, true);
	}
}

// =====================================================================
//  Интерьеры организаций. Все в интерьере ORGINT_INTERIOR, попасть: /gotoint.
//  Привязать к фракции: /gotoint [номер], встать у двери и /setfint [фракция]
//  (вход с улицы ставится той же командой снаружи), спавн — как обычно.
//  Объекты видны во всех мирах: у каждой банды свой зал, а общий «Притон»
//  для созданных бандами (/createfaction) делят по виртуальным мирам
//  (/gotoint 25 [мир], мир — любой свободный, например 140 + номер фракции).
//
//    Больница   центр 2850, -2650   19 x 29 м: приёмная, 6 палат (белая плитка)
//    Армия      центр 2650, -2850   19 x 19 м: казарма, оружейная, тир (камуфляж, бетон)
//    ФБР        центр 2650, -2650   19 x 19 м: рабочий зал, архив, переговорная (паркет)
//    Grove      центр 2450, -2850   19 x 19 м: гостиная, кухня, качалка (кирпич, зелёный)
//    Ballas     центр 2450, -2650   19 x 19 м: клуб, бар, покер, VIP-диваны (фиолетовый)
//    Aztecas    центр 2450, -2450   19 x 19 м: мастерская лоурайдеров (штукатурка, бирюза)
//    Притон     центр 2650, -2450   10 x 19 м: заброшка (доски, матрасы, бочки)
//
//  Вход всегда у южной стены, лицом на север. Точка появления — в 3.5 м
//  от двери (ORGINT_IN_OFFSET), чтобы камера не упиралась в стену за спиной.
//  Возле входа (x от -2.5 до 2.5, первые 5 м) ничего не ставить.
// =====================================================================

#define ORGINT_INTERIOR     1
#define ORGINT_Z            (1500.0)
#define ORGINT_IN_OFFSET    (3.5)

enum E_ORG_INT { Float:oiX, Float:oiY, Float:oiHalfY }
// Центр пола и половина длины зала
static const Float:gOrgInt[][E_ORG_INT] =
{
	{ 2850.0, -2650.0, 14.4 },      // 0 больница
	{ 2650.0, -2850.0,  9.6 },      // 1 армия
	{ 2650.0, -2650.0,  9.6 },      // 2 ФБР
	{ 2450.0, -2850.0,  9.6 },      // 3 Grove
	{ 2450.0, -2650.0,  9.6 },      // 4 Ballas
	{ 2450.0, -2450.0,  9.6 },      // 5 Aztecas
	{ 2650.0, -2450.0,  9.6 }       // 6 притон остальных банд
};

// Текстуры стен, пола и потолка. color у SetObjectMaterial подкрашивает текстуру (0 — как есть).
enum { TX_PAINT, TX_MARBLE, TX_PARQUET, TX_WALLPAPER, TX_BRICK, TX_PAVE, TX_PLASTER, TX_PLANKS, TX_GYM, TX_PLYWOOD, TX_CAMO }
enum E_INT_TEX { itModel, itTxd[24], itTex[24] }
static const gIntTex[][E_INT_TEX] =
{
	{ 10765, "airportgnd_sfse", "white" },
	{ 3922,  "bistro",          "marblekb_256128" },
	{ 14581, "ab_mafiasuitea",  "cof_wood2" },
	{ 14581, "ab_mafiasuitea",  "walp45S" },
	{ 9514,  "711_sfw",         "brick" },
	{ 10101, "2notherbuildsfe", "Bow_Abpave_Gen" },
	{ 4552,  "ammu_lan2",       "sl_lavicdtwall1" },
	{ 14650, "ab_trukstpc",     "sa_wood08_128" },
	{ 14789, "ab_sfgymmain",    "gym_floor6" },
	{ 18065, "ab_sfammumain",   "plywood_gym" },
	{ 3095,  "a51jdrx",         "sam_camo" }
};

static Float:gIntOX, Float:gIntOY, Float:gIntOZ;    // начало координат зала, который сейчас строится

static IO(model, Float:x, Float:y, Float:z, Float:rx = 0.0, Float:ry = 0.0, Float:rz = 0.0)
{
	return CreateObject(model, gIntOX + x, gIntOY + y, gIntOZ + z, rx, ry, rz, INT_DRAW_DISTANCE);
}

static ISkin(objectid, tex, color)
{
	SetObjectMaterial(objectid, 0, gIntTex[tex][itModel], gIntTex[tex][itTxd], gIntTex[tex][itTex], color);
}

// Коробка зала: wide — ширина в плитах (1 = 9.6 м, 2 = 19.2 м), len — длина в плитах по 9.6 м.
// Для стен, пола и потолка — текстура (TX_*) и подкраска.
static Int_Shell(index, wide, len, Float:h, wallTex, wallColor, floorTex, floorColor, ceilTex, ceilColor)
{
	gIntOX = gOrgInt[index][oiX];
	gIntOY = gOrgInt[index][oiY];
	gIntOZ = ORGINT_Z;
	new o, Float:y;
	for (new iy = 0; iy < len; iy++)
	{
		y = (float(iy) - float(len - 1) / 2.0) * 9.6;
		for (new ix = 0; ix < wide; ix++)
		{
			new Float:x = (wide == 1) ? 0.0 : ((ix == 0) ? -5.25 : 5.25);
			o = IO(INT_PANEL_BIG, x, y, -0.086, 0.0, 90.0, 0.0);
			ISkin(o, floorTex, floorColor);
			o = IO(INT_PANEL_BIG, x, y, h + 0.086, 0.0, 90.0, 0.0);
			ISkin(o, ceilTex, ceilColor);
		}
		o = IO(INT_PANEL_BIG, -(wide * 4.8 + 0.086), y, h - 5.25);
		ISkin(o, wallTex, wallColor);
		o = IO(INT_PANEL_BIG, wide * 4.8 + 0.086, y, h - 5.25);
		ISkin(o, wallTex, wallColor);
	}
	for (new ix = 0; ix < wide; ix++)
	{
		new Float:x = (wide == 1) ? 0.0 : ((ix == 0) ? -4.8 : 4.8);
		o = IO(INT_PANEL_BIG, x, -(len * 4.8 + 0.086), h - 5.25, 0.0, 0.0, 90.0);
		ISkin(o, wallTex, wallColor);
		o = IO(INT_PANEL_BIG, x, len * 4.8 + 0.086, h - 5.25, 0.0, 0.0, 90.0);
		ISkin(o, wallTex, wallColor);
	}
	// дверь-декорация за спиной у входящего
	IO(1569, -0.75, -(len * 4.8) + 0.04, 0.0);
}

// Перегородка 3.2 x 3.5 м: along = 0 — вдоль Y, 90 — вдоль X
static Int_Wall(Float:x, Float:y, Float:along, color, tex = TX_PAINT)
{
	new o = IO(INT_PANEL_SMALL, x, y, 1.75, 0.0, 0.0, along);
	ISkin(o, tex, color);
	return o;
}

// Нижняя панель стены (обшивка) высотой 1.2 м вдоль северной/южной (along 90) или боковой (along 0) стены
static Int_Wainscot(Float:x, Float:y, Float:along, tex, color)
{
	new o = IO(INT_PANEL_SMALL, x, y, -0.55, 0.0, 0.0, along);
	ISkin(o, tex, color);
}

// Надпись на панели 3.2 x 3.5 м. along = 90 — на северной стене, 0 — на боковой. back = 0 — без фона (граффити)
static Int_Sign(Float:x, Float:y, Float:z, const text[], fontsize, color, back, Float:along = 90.0)
{
	new o = IO(INT_PANEL_SMALL, x, y, z, 0.0, 0.0, along);
	SetObjectMaterialText(o, text, 0, OBJECT_MATERIAL_SIZE_256x256, "Arial", fontsize, true, color, back, OBJECT_MATERIAL_TEXT_ALIGN_CENTER);
}

// --- Больница: приёмная у входа, дальше коридор и по три палаты с каждой стороны ---
static Hospital_Build()
{
	Int_Shell(0, 2, 3, 3.5, TX_PAINT, 0xFFF2F4F5, TX_MARBLE, 0, TX_PAINT, 0xFFFFFFFF);
	for (new side = -1; side <= 1; side += 2)
	{
		for (new w = 0; w < 3; w++)
		{
			new Float:py = -4.0 + w * 6.0;              // перегородка палаты
			Int_Wall(side * 4.8, py, 90.0, 0xFFBFE3E0);
			Int_Wall(side * 8.0, py, 90.0, 0xFFBFE3E0);
			IO(1997, side * 7.6, py + 3.0, 0.0, 0.0, 0.0, 90.0);     // койка-каталка
			IO(2011, side * 9.0, py + 0.7, 0.0);
		}
		IO(1726, side * 9.0, -9.0 - side * 1.0, 0.0, 0.0, 0.0, 180.0 - side * 90.0);   // диван в приёмной
		IO(2010, side * 9.0, -13.8, 0.0);
	}
	// стойка регистратуры
	IO(2008, -5.0, -7.0, 0.0, 0.0, 0.0, 180.0);
	IO(2008, -3.0, -7.0, 0.0, 0.0, 0.0, 180.0);
	IO(1714, -4.0, -5.8, 0.0, 0.0, 0.0, 180.0);
	IO(1808, 9.2, -6.0, 0.0, 0.0, 0.0, 270.0);          // кулер
	IO(2146, 3.0, -8.0, 0.5);                           // носилки на колёсах
	IO(2146, 1.6, 12.0, 0.5);
	for (new i = 0; i < 4; i++) IO(1893, 0.0, -10.5 + i * 7.0, 3.45);
	Int_Sign(0.0, 14.30, 1.75, "BASTION\nMEDICAL", 50, 0xFFC0202A, 0xFFFFFFFF);
}

// --- Армия: стены в камуфляже, бетонный пол. Слева казарма, справа оружейная, в глубине тир ---
static Army_Build()
{
	Int_Shell(1, 2, 2, 5.0, TX_CAMO, 0, TX_PAVE, 0xFF8C8C84, TX_PAINT, 0xFF2B2F27);
	// казарма: койки вдоль левой стены, тумбочки
	for (new i = 0; i < 4; i++)
	{
		IO(1771, -8.4, -5.0 + i * 3.6, 0.6);
		IO(1271, -6.9, -3.6 + i * 3.6, 0.35);                       // ящик-тумбочка
	}
	// оружейная справа: перегородка, ящики, стойка с автоматами, мешки
	Int_Wall(5.0, -1.0, 0.0, 0xFF3E4634);
	for (new i = 0; i < 3; i++)
	{
		IO(964, 8.3, -6.5 + i * 1.6, 0.0);                          // железные ящики
		IO(3014, 8.6, 1.5 + i * 1.2, 0.25);                         // ящики с оружием
		IO(2358, 7.0, 1.5 + i * 1.2, 0.12);                         // патроны
	}
	IO(356, 9.3, -1.6, 1.4, 0.0, 90.0, 90.0);                       // M4 на стене
	IO(355, 9.3, -0.4, 1.4, 0.0, 90.0, 90.0);                       // AK на стене
	// тир у северной стены: мешки с песком и мишени
	for (new i = 0; i < 5; i++) IO(2060, -4.0 + i * 2.0, 4.6, 0.0);
	IO(1585, -3.0, 9.0, 0.0, 0.0, 0.0, 180.0);
	IO(1586, 0.0, 9.0, 0.0, 0.0, 0.0, 180.0);
	IO(1587, 3.0, 9.0, 0.0, 0.0, 0.0, 180.0);
	// стол штаба у правой стены за перегородкой
	IO(2357, 6.6, -5.0, 0.4, 0.0, 0.0, 90.0);
	IO(1810, 5.5, -6.0, 0.0, 0.0, 0.0, 270.0);
	IO(1810, 5.5, -4.0, 0.0, 0.0, 0.0, 270.0);
	// свет: лампы по углам, у входа не висят
	IO(1893, -4.8, -1.0, 4.95);
	IO(1893, 4.8, -1.0, 4.95);
	IO(1893, -4.8, 6.0, 4.95);
	IO(1893, 4.8, 6.0, 4.95);
	Int_Sign(0.0, 9.50, 3.2, "BASTION\nARMY", 60, 0xFFE8E0B0, 0xFF2B2F27);
}

// --- ФБР: паркет и обшивка, рабочий зал на шесть столов, архив, за перегородкой переговорная ---
static Fbi_Build()
{
	Int_Shell(2, 2, 2, 3.5, TX_WALLPAPER, 0xFFB8C4D6, TX_PARQUET, 0xFF6A5040, TX_PAINT, 0xFFF4F4F4);
	for (new side = -1; side <= 1; side += 2)
	{
		for (new i = 0; i < 3; i++)
		{
			IO(2165, side * 6.0, -4.5 + i * 3.4, 0.0, 0.0, 0.0, 90.0 + side * 90.0);     // стол с компьютером
			IO(1714, side * 6.0 + 0.5, -3.4 + i * 3.4, 0.0);
		}
		Int_Wall(side * 8.0, 6.0, 90.0, 0xFF1F2A3A);
		Int_Wall(side * 4.8, 6.0, 90.0, 0xFF1F2A3A);
		Int_Wainscot(side * 9.5, -4.8, 0.0, TX_PARQUET, 0xFF3A2A20);
		Int_Wainscot(side * 9.5, -1.6, 0.0, TX_PARQUET, 0xFF3A2A20);
		IO(1893, side * 5.0, -3.0, 3.45);
		IO(1893, side * 5.0, 2.5, 3.45);
	}
	// архив у правой стены за переговорной: шкафы
	for (new i = 0; i < 4; i++) IO(2007, -9.0, 7.0 + i * 0.6, 0.0, 0.0, 0.0, 90.0);
	IO(2202, -6.0, 9.0, 0.0);                                       // ксерокс
	// доска с ориентировками
	IO(2611, -9.5, 2.0, 1.6, 0.0, 0.0, 90.0);
	IO(2612, 9.5, 2.0, 1.6, 0.0, 0.0, 270.0);
	Int_Wall(1.6, 6.0, 90.0, 0xFF1F2A3A);                           // проход в переговорную слева от неё
	IO(2357, 4.5, 7.9, 0.4, 0.0, 0.0, 90.0);                        // длинный стол
	IO(1714, 3.0, 9.0, 0.0);
	IO(1714, 6.0, 9.0, 0.0);
	IO(1808, 9.2, -6.0, 0.0, 0.0, 0.0, 270.0);
	IO(2001, -9.0, -8.6, 0.0);
	Int_Sign(0.0, 5.90, 1.75, "FBI", 90, 0xFFE8C860, 0xFF1F2A3A);
}

// --- Grove: красный кирпич, зелёные акценты. Гостиная слева, кухня справа, в глубине качалка ---
static Grove_Build()
{
	Int_Shell(3, 2, 2, 3.6, TX_BRICK, 0, TX_PARQUET, 0, TX_PAINT, 0xFFE8E2D0);
	// гостиная: диваны буквой П вокруг телевизора, ковёр
	IO(2836, -6.0, -3.0, 0.01);
	IO(1702, -8.8, -3.0, 0.0, 0.0, 0.0, 90.0);
	IO(1702, -6.0, -0.4, 0.0, 0.0, 0.0, 180.0);
	IO(1817, -6.0, -3.0, 0.0);
	IO(2315, -3.4, -3.2, 0.0, 0.0, 0.0, 90.0);
	IO(1786, -3.4, -3.2, 0.5, 0.0, 0.0, 270.0);
	IO(2226, -8.8, -6.6, 0.0);                                      // магнитофон
	// кухня справа: гарнитур вдоль стены, холодильник, стол
	IO(2131, 9.0, -7.0, 0.0, 0.0, 0.0, 270.0);
	IO(2134, 9.0, -5.5, 0.0, 0.0, 0.0, 270.0);
	IO(2141, 9.0, -4.5, 0.0, 0.0, 0.0, 270.0);
	IO(2149, 9.0, -4.5, 1.05, 0.0, 0.0, 270.0);
	IO(2115, 6.0, -3.0, 0.0);
	IO(1811, 6.0, -4.2, 0.0);
	IO(1811, 6.0, -1.8, 0.0, 0.0, 0.0, 180.0);
	// качалка в глубине: жим, гантели, коврик
	IO(2629, -6.0, 6.0, 0.0);
	IO(2628, -3.0, 7.5, 0.0);
	IO(2913, -8.5, 8.5, 0.0);
	IO(2632, -6.0, 8.5, 0.01);
	// мангал и пиво
	IO(1481, 6.5, 7.5, 0.0);
	IO(1486, 6.0, -3.0, 0.85);
	IO(1543, 6.3, -2.8, 0.85);
	// зелёный неон под потолком и граффити
	IO(18649, -9.4, 0.0, 3.3, 0.0, 0.0, 0.0);
	IO(18649, 9.4, 0.0, 3.3, 0.0, 0.0, 0.0);
	for (new i = 0; i < 2; i++) IO(1893, (i == 0) ? -5.0 : 5.0, 0.5, 3.55);
	Int_Sign(0.0, 9.50, 1.9, "GROVE\nSTREET\n4 LIFE", 46, 0xFF2EC83C, 0);
	Int_Sign(-9.5, 3.5, 1.9, "GSF", 110, 0xFF1E9A2A, 0, 0.0);
}

// --- Ballas: клуб. Фиолетовые обои, тёмный пол, барная стойка, покер, розовый неон ---
static Ballas_Build()
{
	Int_Shell(4, 2, 2, 4.2, TX_WALLPAPER, 0xFF8A4AB8, TX_GYM, 0xFF2A1A34, TX_PAINT, 0xFF140A1C);
	// бар у правой стены: стойка из панелей и стулья
	new o = IO(INT_PANEL_SMALL, 7.0, 2.0, -0.6, 0.0, 0.0, 0.0);
	ISkin(o, TX_MARBLE, 0xFF2A1A34);
	o = IO(INT_PANEL_SMALL, 8.6, 3.7, -0.6, 0.0, 0.0, 90.0);
	ISkin(o, TX_MARBLE, 0xFF2A1A34);
	for (new i = 0; i < 4; i++) IO(2350, 6.2, 0.8 + i * 0.9, 0.0);
	IO(1209, 9.2, 8.6, 0.0, 0.0, 0.0, 270.0);                       // автомат с газировкой
	// покерный стол и рулетка-блэкджек
	IO(2188, -4.0, 4.0, 0.0);
	IO(1671, -4.0, 5.4, 0.0, 0.0, 0.0, 0.0);
	IO(1671, -4.0, 2.6, 0.0, 0.0, 0.0, 180.0);
	// VIP-диваны у левой стены
	IO(1723, -8.9, -4.0, 0.0, 0.0, 0.0, 90.0);
	IO(1723, -8.9, -0.5, 0.0, 0.0, 0.0, 90.0);
	IO(1817, -7.4, -2.2, 0.0);
	IO(2847, -7.0, -2.2, 0.01);
	// колонки и игровые автоматы у северной стены
	IO(2229, -2.5, 9.0, 0.0);
	IO(2229, 2.5, 9.0, 0.0);
	IO(2779, -8.6, 8.6, 0.0);
	IO(2778, -7.4, 8.6, 0.0);
	// розовый неон по периметру
	IO(18651, -9.4, -4.0, 4.0);
	IO(18651, -9.4, 4.0, 4.0);
	IO(18651, 9.4, -4.0, 4.0);
	IO(18651, 9.4, 4.0, 4.0);
	IO(18651, 0.0, 9.4, 4.0, 0.0, 0.0, 90.0);
	Int_Sign(0.0, 9.50, 2.3, "BALLAS", 70, 0xFFC860FF, 0);
	Int_Sign(9.5, -4.5, 1.9, "BALLAS\nCLUB", 50, 0xFFFF78E6, 0, 0.0);
}

// --- Aztecas: мастерская лоурайдеров. Штукатурка цвета бирюзы, плитка, инструменты, колёса ---
static Aztecas_Build()
{
	Int_Shell(5, 2, 2, 4.6, TX_PLASTER, 0xFF40B8C8, TX_PAVE, 0xFFB0A080, TX_PAINT, 0xFFE6D2AA);
	// мастерская справа: стенд, шкафы, двигатель, колёса
	IO(19815, 9.5, 0.0, 1.6, 0.0, 0.0, 270.0);
	IO(19899, 9.1, -3.0, 0.0, 0.0, 0.0, 270.0);
	IO(19900, 9.1, 3.0, 0.0, 0.0, 0.0, 270.0);
	IO(19917, 6.0, 0.0, 0.0);
	IO(19921, 6.0, 1.6, 0.0);
	IO(1080, 8.8, 6.0, 0.4, 0.0, 0.0, 90.0);
	IO(1080, 8.8, 7.0, 0.4, 0.0, 0.0, 90.0);
	IO(1079, 8.8, 8.0, 0.4, 0.0, 0.0, 90.0);
	IO(1010, 7.0, 8.6, 0.0);
	IO(1218, 4.5, 8.6, 0.0);
	// уголок отдыха слева: диван, кресла, телевизор, алтарь со свечами
	IO(1726, -8.9, -2.0, 0.0, 0.0, 0.0, 90.0);
	IO(1704, -6.0, 0.6, 0.0, 0.0, 0.0, 180.0);
	IO(2311, -6.5, -2.0, 0.0);
	IO(2103, -6.5, -2.0, 0.5);
	IO(2835, -7.0, -1.0, 0.01);
	IO(2001, -9.0, 8.8, 0.0);
	IO(2001, -9.0, -8.8, 0.0);
	// голубой неон
	IO(18648, -9.4, 0.0, 4.4);
	IO(18648, 9.4, 0.0, 4.4);
	IO(18648, 0.0, 9.4, 4.4, 0.0, 0.0, 90.0);
	for (new i = 0; i < 3; i++) IO(1893, -4.0 + i * 4.0, 3.5, 4.55);
	Int_Sign(-3.0, 9.50, 2.4, "VARRIOS\nLOS\nAZTECAS", 40, 0xFF00C8F0, 0);
	Int_Sign(-9.5, 4.5, 2.0, "VLA", 110, 0xFFF0F0F0, 0, 0.0);
}

// --- Притон для остальных банд: заброшка из досок, матрасы, бочки, ящики ---
static Squat_Build()
{
	Int_Shell(6, 1, 2, 3.3, TX_PLANKS, 0, TX_PLYWOOD, 0xFF8C7A60, TX_PLANKS, 0xFF5A5040);
	IO(1812, -3.6, 1.0, 0.0);                                       // матрасы-кровати
	IO(1812, -3.6, 4.0, 0.0);
	IO(1812, -3.6, 7.0, 0.0);
	IO(1218, 3.8, -2.0, 0.0);                                       // бочки
	IO(1218, 4.0, -1.0, 0.0);
	IO(2912, 3.8, 3.0, 0.0);                                        // ящики
	IO(2912, 3.8, 4.0, 0.0);
	IO(2912, 3.8, 3.5, 0.95);
	IO(1271, 3.8, 6.0, 0.35);
	IO(2103, -1.0, 8.8, 0.0);                                       // магнитофон
	IO(1704, 1.6, 7.6, 0.0, 0.0, 0.0, 200.0);                       // старое кресло
	IO(1550, 4.0, 8.8, 0.3);                                        // мешок с деньгами
	IO(1893, 0.0, 0.0, 3.25);
	IO(1893, 0.0, 6.0, 3.25);
	Int_Sign(0.0, 9.50, 1.9, "OUR\nSTREETS", 60, 0xFFE0A030, 0);
	Int_Sign(4.7, 0.0, 1.9, "NO\nSNITCHES", 46, 0xFFD02020, 0, 0.0);
}

stock Interiors_Init()
{
	Salon_Build();
	Hospital_Build();
	Army_Build();
	Fbi_Build();
	Grove_Build();
	Ballas_Build();
	Aztecas_Build();
	Squat_Build();
	SetTimer("Interiors_FallCheck", 1000, true);
	print("[Interiors] Showroom and 7 organisation interiors built.");
}

stock bool:Salon_IsInside(playerid)
{
	return GetPlayerVirtualWorld(playerid) == SALON_WORLD && GetPlayerInterior(playerid) == SALON_INTERIOR;
}

stock Salon_GetSlot(slot, &Float:x, &Float:y, &Float:z, &Float:a)
{
	if (slot < 0 || slot >= SALON_SLOTS) return 0;
	x = SALON_X + gSalonSlot[slot][0];
	y = SALON_Y + gSalonSlot[slot][1];
	z = SALON_Z + 1.0;
	a = gSalonSlot[slot][2];
	return 1;
}

stock Salon_GetBuyPoint(&Float:x, &Float:y, &Float:z)
{
	x = SALON_X;
	y = SALON_Y + 10.2;
	z = SALON_Z + 1.0;
}

// Пол — объект: если он не успел прогрузиться и игрок провалился, вернуть ко входу
static Interiors_Rescue(playerid, Float:x, Float:y, Float:z, Float:cx, Float:cy, Float:inY)
{
	if (z > ORGINT_Z - 3.0 || z < ORGINT_Z - 400.0 || floatabs(x - cx) > 60.0 || floatabs(y - cy) > 60.0) return 0;
	SetPlayerPos(playerid, cx, inY, ORGINT_Z + 1.3);
	SetPlayerFacingAngle(playerid, 0.0);
	return 1;
}

forward Interiors_FallCheck();
public Interiors_FallCheck()
{
	new Float:x, Float:y, Float:z;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || GetPlayerInterior(i) != ORGINT_INTERIOR) continue;
		GetPlayerPos(i, x, y, z);
		if (z > ORGINT_Z - 3.0) continue;
		if (Interiors_Rescue(i, x, y, z, SALON_X, SALON_Y, SALON_IN_Y)) continue;
		for (new k = 0; k < sizeof gOrgInt; k++)
		{
			if (Interiors_Rescue(i, x, y, z, gOrgInt[k][oiX], gOrgInt[k][oiY], gOrgInt[k][oiY] - gOrgInt[k][oiHalfY] + ORGINT_IN_OFFSET)) break;
		}
	}
}
