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

stock Interiors_Init()
{
	Salon_Build();
	SetTimer("Interiors_FallCheck", 1000, true);
	print("[Interiors] Showroom built.");
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
forward Interiors_FallCheck();
public Interiors_FallCheck()
{
	new Float:x, Float:y, Float:z;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || GetPlayerVirtualWorld(i) != SALON_WORLD) continue;
		GetPlayerPos(i, x, y, z);
		if (z < SALON_Z - 3.0 && floatabs(x - SALON_X) < 60.0 && floatabs(y - SALON_Y) < 60.0)
		{
			SetPlayerPos(i, SALON_IN_X, SALON_IN_Y, SALON_IN_Z + 0.3);
			SetPlayerFacingAngle(i, SALON_IN_A);
		}
	}
}
