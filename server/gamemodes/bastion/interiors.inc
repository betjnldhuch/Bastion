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
//  Объекты видны во всех мирах, поэтому один зал банды служит всем бандам:
//  у каждой свой виртуальный мир (/gotoint ставит его сам).
//
//    Больница   центр 2850, -2650   зал 19 x 29 м: приёмная, 6 палат
//    Армия      центр 2650, -2850   зал 19 x 19 м: казарма, оружейная, штаб
//    ФБР        центр 2650, -2650   зал 19 x 19 м: рабочий зал, переговорная
//    Банда      центр 2450, -2850   зал 10 x 19 м: притон
// =====================================================================

#define ORGINT_INTERIOR     1
#define ORGINT_Z            (1500.0)

enum E_ORG_INT { Float:oiX, Float:oiY, Float:oiHalfY }
// Центр пола и половина длины зала (вход всегда у южной стены, лицом на север)
static const Float:gOrgInt[][E_ORG_INT] =
{
	{ 2850.0, -2650.0, 14.4 },      // больница
	{ 2650.0, -2850.0,  9.6 },      // армия
	{ 2650.0, -2650.0,  9.6 },      // ФБР
	{ 2450.0, -2850.0,  9.6 }       // банда
};

static Float:gIntOX, Float:gIntOY, Float:gIntOZ;    // начало координат зала, который сейчас строится

static IO(model, Float:x, Float:y, Float:z, Float:rx = 0.0, Float:ry = 0.0, Float:rz = 0.0)
{
	return CreateObject(model, gIntOX + x, gIntOY + y, gIntOZ + z, rx, ry, rz, INT_DRAW_DISTANCE);
}

static IPaint(objectid, color)
{
	SetObjectMaterial(objectid, 0, 10765, "airportgnd_sfse", "white", color);
}

// Коробка зала: wide — ширина в плитах (1 = 9.6 м, 2 = 19.2 м), len — длина в плитах по 9.6 м.
// Пол: floorColor = 0 — мрамор, иначе заливка цветом.
static Int_Shell(index, wide, len, Float:h, wallColor, ceilColor, floorColor)
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
			if (floorColor == 0) SetObjectMaterial(o, 0, 3922, "bistro", "marblekb_256128", 0);
			else IPaint(o, floorColor);
			o = IO(INT_PANEL_BIG, x, y, h + 0.086, 0.0, 90.0, 0.0);
			IPaint(o, ceilColor);
		}
		o = IO(INT_PANEL_BIG, -(wide * 4.8 + 0.086), y, h - 5.25);
		IPaint(o, wallColor);
		o = IO(INT_PANEL_BIG, wide * 4.8 + 0.086, y, h - 5.25);
		IPaint(o, wallColor);
	}
	for (new ix = 0; ix < wide; ix++)
	{
		new Float:x = (wide == 1) ? 0.0 : ((ix == 0) ? -4.8 : 4.8);
		o = IO(INT_PANEL_BIG, x, -(len * 4.8 + 0.086), h - 5.25, 0.0, 0.0, 90.0);
		IPaint(o, wallColor);
		o = IO(INT_PANEL_BIG, x, len * 4.8 + 0.086, h - 5.25, 0.0, 0.0, 90.0);
		IPaint(o, wallColor);
	}
	// дверь-декорация за спиной у входящего
	IO(1569, -0.75, -(len * 4.8) + 0.04, 0.0);
}

// Перегородка 3.2 x 3.5 м: along = 0 — вдоль Y, 90 — вдоль X
static Int_Wall(Float:x, Float:y, Float:along, color)
{
	new o = IO(INT_PANEL_SMALL, x, y, 1.75, 0.0, 0.0, along);
	IPaint(o, color);
}

// Надпись на панели 3.2 x 3.5 м у торцевой стены
static Int_Sign(Float:x, Float:y, Float:z, const text[], fontsize, color, back)
{
	new o = IO(INT_PANEL_SMALL, x, y, z, 0.0, 0.0, 90.0);
	SetObjectMaterialText(o, text, 0, OBJECT_MATERIAL_SIZE_256x256, "Arial", fontsize, true, color, back, OBJECT_MATERIAL_TEXT_ALIGN_CENTER);
}

// --- Больница: приёмная у входа, дальше коридор и по три палаты с каждой стороны ---
static Hospital_Build()
{
	Int_Shell(0, 2, 3, 3.5, 0xFFF2F4F5, 0xFFFFFFFF, 0);
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
	IO(2146, 1.6, -9.5, 0.5);                           // носилки на колёсах
	IO(2146, 1.6, 12.0, 0.5);
	for (new i = 0; i < 4; i++) IO(1893, 0.0, -10.5 + i * 7.0, 3.45);
	Int_Sign(0.0, 14.30, 1.75, "BASTION\nMEDICAL", 50, 0xFFC0202A, 0xFFFFFFFF);
}

// --- Армия: слева казарма, справа оружейная, в центре стол штаба ---
static Army_Build()
{
	Int_Shell(1, 2, 2, 5.0, 0xFF5D6B4A, 0xFF2B2F27, 0xFF4A4D48);
	for (new i = 0; i < 4; i++)
	{
		IO(1771, -8.4, -6.6 + i * 4.2, 0.6);                        // двухъярусные койки
		IO(1893, (i % 2 == 0) ? -4.8 : 4.8, (i < 2) ? -4.8 : 4.8, 4.95);
	}
	for (new i = 0; i < 3; i++)
	{
		IO(2977, 8.3, -6.0 + i * 2.2, 0.0);                         // армейские ящики
		IO(3014, 8.6, 2.0 + i * 1.2, 0.25);                         // ящики с оружием
		IO(2358, 6.8, 2.0 + i * 1.2, 0.12);                         // патроны
	}
	IO(2063, 9.1, 7.6, 0.9, 0.0, 0.0, 270.0);                       // стеллаж
	// стол штаба и стулья
	IO(2357, 0.0, 3.5, 0.4);
	IO(1810, -1.5, 2.2, 0.0);
	IO(1810, 1.5, 2.2, 0.0);
	IO(1810, -1.5, 4.8, 0.0, 0.0, 0.0, 180.0);
	IO(1810, 1.5, 4.8, 0.0, 0.0, 0.0, 180.0);
	Int_Sign(0.0, 9.50, 2.6, "BASTION\nARMY", 60, 0xFFE8E0B0, 0xFF2B2F27);
}

// --- ФБР: рабочий зал на шесть столов, за перегородкой переговорная ---
static Fbi_Build()
{
	Int_Shell(2, 2, 2, 3.5, 0xFFDCE0E6, 0xFFF4F4F4, 0xFF2C3440);
	for (new side = -1; side <= 1; side += 2)
	{
		for (new i = 0; i < 3; i++)
		{
			IO(2165, side * 6.0, -5.5 + i * 3.6, 0.0, 0.0, 0.0, 90.0 + side * 90.0);     // стол с компьютером
			IO(1714, side * 6.0 + 0.5, -4.4 + i * 3.6, 0.0);
		}
		Int_Wall(side * 8.0, 6.0, 90.0, 0xFF1F2A3A);
		Int_Wall(side * 4.8, 6.0, 90.0, 0xFF1F2A3A);
		IO(2010, side * 9.0, -9.0, 0.0);
		IO(2011, side * 9.0, 5.3, 0.0);
		IO(1893, side * 5.0, -4.0, 3.45);
		IO(1893, side * 5.0, 2.5, 3.45);
	}
	Int_Wall(1.6, 6.0, 90.0, 0xFF1F2A3A);                           // проход в переговорную слева от неё
	IO(2357, 4.5, 7.9, 0.4, 0.0, 0.0, 90.0);                        // длинный стол
	IO(1714, 3.0, 9.0, 0.0);
	IO(1714, 6.0, 9.0, 0.0);
	IO(1808, -9.2, 0.0, 0.0, 0.0, 0.0, 90.0);
	Int_Sign(0.0, 5.90, 1.75, "FBI", 90, 0xFFE8C860, 0xFF1F2A3A);
}

// --- Банда: диваны и телевизор у входа, бильярд, в глубине койки, сейф и ящики ---
static Gang_Build()
{
	Int_Shell(3, 1, 2, 3.5, 0xFF8A7F6E, 0xFF3A3632, 0xFF5A4630);
	IO(1726, -4.2, -4.0, 0.0, 0.0, 0.0, 90.0);
	IO(1726, 4.2, -6.0, 0.0, 0.0, 0.0, 270.0);
	IO(2315, -0.7, -5.0, 0.0);
	IO(1810, -1.5, -2.2, 0.0);
	IO(1810, 1.5, -2.2, 0.0);
	IO(2964, 0.0, 2.2, 0.0);                                        // бильярдный стол
	IO(1771, -3.6, 8.0, 0.6);
	IO(1771, -3.6, 5.0, 0.6);
	IO(2332, 4.2, 9.0, 0.45, 0.0, 0.0, 270.0);                      // сейф
	IO(2977, 3.6, 6.6, 0.0);
	IO(2977, 3.6, 4.8, 0.0);
	IO(3014, 2.2, 8.9, 0.25);
	IO(1893, 0.0, -4.8, 3.45);
	IO(1893, 0.0, 4.8, 3.45);
	Int_Sign(0.0, 9.50, 1.75, "NO\nSNITCHES", 50, 0xFFD8D0C0, 0xFF3A3632);
}

stock Interiors_Init()
{
	Salon_Build();
	Hospital_Build();
	Army_Build();
	Fbi_Build();
	Gang_Build();
	SetTimer("Interiors_FallCheck", 1000, true);
	print("[Interiors] Showroom and 4 organisation interiors built.");
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
			if (Interiors_Rescue(i, x, y, z, gOrgInt[k][oiX], gOrgInt[k][oiY], gOrgInt[k][oiY] - gOrgInt[k][oiHalfY] + 1.8)) break;
		}
	}
}
