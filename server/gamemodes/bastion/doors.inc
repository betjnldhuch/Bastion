// =====================================================================
//  doors.inc — входы в здания со стандартными интерьерами GTA.
//
//  У двери снаружи и внутри — маркер (стрелка) и надпись.
//  Войти/выйти: ALT рядом с маркером или /enter, /exit.
//  Новый вход — строчка в конце таблицы gDoors ниже (координаты: /pos).
//  Маркеры можно двигать в игре: /markers (место выхода — шаг перед маркером, куда он «смотрит»).
// =====================================================================

#define DOOR_RANGE          1.5
#define DOOR_PICKUP_MODEL   1318    // белая стрелка
// Маркер стоит на 1.4 м позади точки появления (у самой двери), чтобы не войти/выйти дважды
#define Door_MarkerOffset   (1.4)

enum E_DOOR
{
	dName[32],
	Float:dOutX, Float:dOutY, Float:dOutZ, Float:dOutA,   // снаружи у двери (куда выходишь)
	Float:dInX, Float:dInY, Float:dInZ, Float:dInA,       // внутри у двери (куда входишь)
	dInterior,
	dWorld
}

static const gDoors[][E_DOOR] =
{
	// Полиция Лос-Сантоса: главный вход со стороны Pershing Square -> интерьер 6
	{ "Полиция LSPD", 1553.90, -1675.60, 16.19, 90.0,   246.78, 63.90, 1003.64, 0.0,   6, 0 }
};


static gDoorMk[sizeof gDoors][2];      // маркеры двери: [0] снаружи, [1] внутри

stock Doors_Init()
{
	new label[64], name[32];
	for (new d = 0; d < sizeof gDoors; d++)
	{
		new Float:x, Float:y, Float:z = gDoors[d][dOutZ];
		Door_OuterMarker(d, x, y);
		gDoorMk[d][0] = Marker_Create(MK_DOOR, d * 2, gDoors[d][dName], DOOR_PICKUP_MODEL, x, y, z, 0, 0, GPS_GOV, gDoors[d][dOutA]);
		format(label, sizeof label, "{FFFF66}%s\n{FFFFFF}ALT — войти", gDoors[d][dName]);
		Marker_SetLabel(gDoorMk[d][0], label);

		z = gDoors[d][dInZ];
		Door_InnerMarker(d, x, y);
		format(name, sizeof name, "%s, выход", gDoors[d][dName]);
		gDoorMk[d][1] = Marker_Create(MK_DOOR, d * 2 + 1, name, DOOR_PICKUP_MODEL, x, y, z,
			gDoors[d][dWorld], gDoors[d][dInterior], GPS_NONE, gDoors[d][dInA]);
		Marker_SetLabel(gDoorMk[d][1], "{FFFFFF}ALT — выйти", COLOR_WHITE, 10.0);
	}
	printf("[Doors] %d entrances.", sizeof gDoors);
}

// Маркер снаружи — на шаг ближе к двери, чем точка выхода (по направлению взгляда назад)
static Door_OuterMarker(d, &Float:x, &Float:y)
{
	x = gDoors[d][dOutX] + Door_MarkerOffset * floatsin(gDoors[d][dOutA], degrees);
	y = gDoors[d][dOutY] - Door_MarkerOffset * floatcos(gDoors[d][dOutA], degrees);
}

static Door_InnerMarker(d, &Float:x, &Float:y)
{
	x = gDoors[d][dInX] + Door_MarkerOffset * floatsin(gDoors[d][dInA], degrees);
	y = gDoors[d][dInY] - Door_MarkerOffset * floatcos(gDoors[d][dInA], degrees);
}

// Войти или выйти, если игрок у маркера. true — сработало.
forward bool:Law_CellGateUse(playerid);     // law.inc: полицейский у решётки камеры

stock bool:Doors_Try(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return false;
	if (Law_CellGateUse(playerid)) return true;
	for (new d = 0; d < sizeof gDoors; d++)
	{
		if (Marker_InRange(playerid, gDoorMk[d][0], DOOR_RANGE))
		{
			if (Law_TryLeave(playerid)) Door_SpawnAt(playerid, gDoorMk[d][1]);
			return true;
		}
		if (Marker_InRange(playerid, gDoorMk[d][1], DOOR_RANGE))
		{
			// камера в том же здании: заключённого дверь выпускает, только если её открыл
			// полицейский (/celldoor) или он договорился (/bribe) — law.inc
			if (Law_TryLeave(playerid)) Door_SpawnAt(playerid, gDoorMk[d][0]);
			return true;
		}
	}
	return false;
}

// Поставить игрока на шаг перед маркером двери, лицом по углу маркера
static Door_SpawnAt(playerid, m)
{
	new Float:x, Float:y, Float:z;
	if (!Marker_GetPos(m, x, y, z)) return;
	new Float:a = Marker_GetAngle(m);
	x -= Door_MarkerOffset * floatsin(a, degrees);
	y += Door_MarkerOffset * floatcos(a, degrees);
	SetPlayerInterior(playerid, Marker_GetInterior(m));
	SetPlayerVirtualWorld(playerid, Marker_GetWorld(m));
	SetPlayerPos(playerid, x, y, z);
	SetPlayerFacingAngle(playerid, a);
	SetCameraBehindPlayer(playerid);
}
