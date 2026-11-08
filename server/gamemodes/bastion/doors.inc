// =====================================================================
//  doors.inc — входы в здания со стандартными интерьерами GTA.
//
//  У двери снаружи и внутри — маркер (стрелка) и надпись.
//  Войти/выйти: ALT рядом с маркером или /enter, /exit.
//  Новый вход — строчка в таблице gDoors ниже (координаты: /pos).
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


stock Doors_Init()
{
	new label[64];
	for (new d = 0; d < sizeof gDoors; d++)
	{
		new Float:ox, Float:oy, Float:ix, Float:iy;
		Door_OuterMarker(d, ox, oy);
		Door_InnerMarker(d, ix, iy);
		CreatePickup(DOOR_PICKUP_MODEL, 1, ox, oy, gDoors[d][dOutZ], 0);
		CreatePickup(DOOR_PICKUP_MODEL, 1, ix, iy, gDoors[d][dInZ], gDoors[d][dWorld]);
		format(label, sizeof label, "{FFFF66}%s\n{FFFFFF}ALT — войти", gDoors[d][dName]);
		Create3DTextLabel(label, 0xFFFFFFFF, ox, oy, gDoors[d][dOutZ] + 0.6, 15.0, 0, true);
		Create3DTextLabel("{FFFFFF}ALT — выйти", 0xFFFFFFFF, ix, iy, gDoors[d][dInZ] + 0.6, 10.0, gDoors[d][dWorld], true);
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
stock bool:Doors_Try(playerid)
{
	if (GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return false;
	new interior = GetPlayerInterior(playerid), world = GetPlayerVirtualWorld(playerid);
	new Float:x, Float:y;
	for (new d = 0; d < sizeof gDoors; d++)
	{
		if (interior == 0 && world == 0)
		{
			Door_OuterMarker(d, x, y);
			if (IsPlayerInRangeOfPoint(playerid, DOOR_RANGE, x, y, gDoors[d][dOutZ]))
			{
				SetPlayerInterior(playerid, gDoors[d][dInterior]);
				SetPlayerVirtualWorld(playerid, gDoors[d][dWorld]);
				SetPlayerPos(playerid, gDoors[d][dInX], gDoors[d][dInY], gDoors[d][dInZ]);
				SetPlayerFacingAngle(playerid, gDoors[d][dInA]);
				SetCameraBehindPlayer(playerid);
				return true;
			}
		}
		else if (interior == gDoors[d][dInterior] && world == gDoors[d][dWorld])
		{
			Door_InnerMarker(d, x, y);
			if (IsPlayerInRangeOfPoint(playerid, DOOR_RANGE, x, y, gDoors[d][dInZ]))
			{
				SetPlayerInterior(playerid, 0);
				SetPlayerVirtualWorld(playerid, 0);
				SetPlayerPos(playerid, gDoors[d][dOutX], gDoors[d][dOutY], gDoors[d][dOutZ]);
				SetPlayerFacingAngle(playerid, gDoors[d][dOutA]);
				SetCameraBehindPlayer(playerid);
				return true;
			}
		}
	}
	return false;
}
