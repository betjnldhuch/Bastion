// =====================================================================
//  hud.inc — свой HUD на экране: панель статуса и спидометр.
//
//  Панель статуса (всегда на экране после входа):
//    логотип, ID, полоска сытости, деньги в банке, уровень.
//    На ПК — справа под стандартными деньгами GTA, на телефоне — сверху по центру.
//  Спидометр (только за рулём):
//    название машины, скорость, полоски бензина и целости кузова,
//    значки двигателя, фар и замков.
//    На ПК — справа внизу, на телефоне — сверху по центру под панелью.
//
//  Панель статуса рисует только клиент Bastion (APK и ПК с bastion_ui.asi новой
//  версии, UI_HasHud) в стиле окон (ui/index.html, BastionUI.hud): ХП, броня,
//  наличные, банк, сытость, уровень. Данные идут скрытым диалогом "[hud]"
//  (HUD_DIALOG_ID) только когда у игрока нет открытого окна; клиент его не
//  показывает, а стандартные полоски ХП/брони и деньги GTA прячет.
//  Старая панель из текстдравов убрана: с ней на экране было две панели.
//
//  Игрокам:
//    /hud     — спрятать / показать панель статуса
//    /speedo  — спрятать / показать спидометр (fuel.inc)
//
//  Без клиента Bastion стандартный HUD GTA (здоровье, броня, деньги) остаётся:
//  сервер SA-MP сам его скрыть не может.
//  Надписи по-английски: стандартный шрифт текстдравов не знает кириллицу.
// =====================================================================

#define HUD_TICK_MS         250     // как часто обновлять (скорость)
#define HUD_SLOW_TICKS      4       // остальное — раз в 4 тика (1 с)

#define HUD_COL_BG          0x0E1218D0
#define HUD_COL_ACCENT      0xFFCC66FF
#define HUD_COL_BAR_BG      0x2A3340FF
#define HUD_COL_LABEL       0x9AA4B2FF
#define HUD_COL_GOOD        0x6FCF5CFF
#define HUD_COL_WARN        0xF2C94CFF
#define HUD_COL_BAD         0xEB5757FF

// Спидометр
#define HUD_SPEEDO_W        120.0
#define HUD_SPEEDO_BAR_W    82.0
enum
{
	SP_BG,
	SP_ACCENT,
	SP_NAME,
	SP_SPEED,
	SP_KMH,
	SP_FUEL_LBL,
	SP_FUEL_BG,
	SP_FUEL_FILL,
	SP_HP_LBL,
	SP_HP_BG,
	SP_HP_FILL,
	SP_FLAGS,
	SP_COUNT
}

static PlayerText:gHudSpeedo[MAX_PLAYERS][SP_COUNT];
static bool:gHudWant[MAX_PLAYERS];        // игрок заспавнился — панель нужна
static bool:gHudSpeedoOn[MAX_PLAYERS];
static bool:gHudStatusHidden[MAX_PLAYERS];
static bool:gHudSpeedoHidden[MAX_PLAYERS];
static Float:gHudSpeedoX[MAX_PLAYERS];     // левый край спидометра (для полосок)

// Что уже нарисовано — чтобы не слать одно и то же каждый тик
static gHudLastSpeed[MAX_PLAYERS] = { -1, ... };
static gHudLastFuel[MAX_PLAYERS] = { -1, ... };
static gHudLastHp[MAX_PLAYERS] = { -1, ... };
static gHudLastFlags[MAX_PLAYERS] = { -1, ... };
static gHudLastModel[MAX_PLAYERS] = { -1, ... };
static gHudTick;

// HTML-панель (UI_Has): что уже отправлено клиенту
#define HUD_DIALOG_ID       32700
static gHudWebCash[MAX_PLAYERS] = { cellmin, ... };
static gHudWebHp[MAX_PLAYERS] = { -1, ... };
static gHudWebArmour[MAX_PLAYERS] = { -1, ... };
static gHudWebFood[MAX_PLAYERS] = { -1, ... };
static gHudWebBank[MAX_PLAYERS] = { cellmin, ... };
static gHudWebLevel[MAX_PLAYERS] = { -1, ... };
static bool:gHudWebOn[MAX_PLAYERS];

static const gHudVehNames[][] =
{
	"Landstalker", "Bravura", "Buffalo", "Linerunner", "Perennial", "Sentinel", "Dumper", "Firetruck", "Trashmaster", "Stretch",
	"Manana", "Infernus", "Voodoo", "Pony", "Mule", "Cheetah", "Ambulance", "Leviathan", "Moonbeam", "Esperanto",
	"Taxi", "Washington", "Bobcat", "Mr Whoopee", "BF Injection", "Hunter", "Premier", "Enforcer", "Securicar", "Banshee",
	"Predator", "Bus", "Rhino", "Barracks", "Hotknife", "Trailer", "Previon", "Coach", "Cabbie", "Stallion",
	"Rumpo", "RC Bandit", "Romero", "Packer", "Monster", "Admiral", "Squalo", "Seasparrow", "Pizzaboy", "Tram",
	"Trailer", "Turismo", "Speeder", "Reefer", "Tropic", "Flatbed", "Yankee", "Caddy", "Solair", "Berkley's RC Van",
	"Skimmer", "PCJ-600", "Faggio", "Freeway", "RC Baron", "RC Raider", "Glendale", "Oceanic", "Sanchez", "Sparrow",
	"Patriot", "Quad", "Coastguard", "Dinghy", "Hermes", "Sabre", "Rustler", "ZR-350", "Walton", "Regina",
	"Comet", "BMX", "Burrito", "Camper", "Marquis", "Baggage", "Dozer", "Maverick", "News Chopper", "Rancher",
	"FBI Rancher", "Virgo", "Greenwood", "Jetmax", "Hotring", "Sandking", "Blista Compact", "Police Maverick", "Boxville", "Benson",
	"Mesa", "RC Goblin", "Hotring Racer A", "Hotring Racer B", "Bloodring Banger", "Rancher", "Super GT", "Elegant", "Journey", "Bike",
	"Mountain Bike", "Beagle", "Cropduster", "Stunt", "Tanker", "Roadtrain", "Nebula", "Majestic", "Buccaneer", "Shamal",
	"Hydra", "FCR-900", "NRG-500", "HPV1000", "Cement Truck", "Tow Truck", "Fortune", "Cadrona", "FBI Truck", "Willard",
	"Forklift", "Tractor", "Combine", "Feltzer", "Remington", "Slamvan", "Blade", "Freight", "Streak", "Vortex",
	"Vincent", "Bullet", "Clover", "Sadler", "Firetruck LA", "Hustler", "Intruder", "Primo", "Cargobob", "Tampa",
	"Sunrise", "Merit", "Utility", "Nevada", "Yosemite", "Windsor", "Monster A", "Monster B", "Uranus", "Jester",
	"Sultan", "Stratum", "Elegy", "Raindance", "RC Tiger", "Flash", "Tahoma", "Savanna", "Bandito", "Freight Flat",
	"Streak Carriage", "Kart", "Mower", "Duneride", "Sweeper", "Broadway", "Tornado", "AT-400", "DFT-30", "Huntley",
	"Stafford", "BF-400", "Newsvan", "Tug", "Trailer", "Emperor", "Wayfarer", "Euros", "Hotdog", "Club",
	"Freight Box", "Trailer", "Andromada", "Dodo", "RC Cam", "Launch", "Police LS", "Police SF", "Police LV", "Police Ranger",
	"Picador", "S.W.A.T.", "Alpha", "Phoenix", "Glendale", "Sadler", "Luggage", "Luggage", "Stairs", "Boxville",
	"Tiller", "Utility Trailer"
};

stock Hud_GetVehicleName(model, dest[], size = sizeof (dest))
{
	dest[0] = '\0';
	if (model >= 400 && model < 400 + sizeof (gHudVehNames)) strcat(dest, gHudVehNames[model - 400], size);
	else strcat(dest, "Vehicle", size);
}

// ---------------------------------------------------------------------
//  Помощники для текстдравов
// ---------------------------------------------------------------------
static PlayerText:Hud_Box(playerid, Float:x, Float:y, Float:x2, Float:lines, colour)
{
	new PlayerText:td = CreatePlayerTextDraw(playerid, x, y, "_");
	PlayerTextDrawLetterSize(playerid, td, 0.0, lines);
	PlayerTextDrawTextSize(playerid, td, x2, 0.0);
	PlayerTextDrawUseBox(playerid, td, true);
	PlayerTextDrawBoxColour(playerid, td, colour);
	PlayerTextDrawSetShadow(playerid, td, 0);
	return td;
}

static PlayerText:Hud_Text(playerid, Float:x, Float:y, const text[], TEXT_DRAW_FONT:font, Float:sx, Float:sy, colour, TEXT_DRAW_ALIGN:align = TEXT_DRAW_ALIGN_LEFT)
{
	new PlayerText:td = CreatePlayerTextDraw(playerid, x, y, text);
	PlayerTextDrawFont(playerid, td, font);
	PlayerTextDrawLetterSize(playerid, td, sx, sy);
	PlayerTextDrawColour(playerid, td, colour);
	PlayerTextDrawAlignment(playerid, td, align);
	PlayerTextDrawSetOutline(playerid, td, 1);
	PlayerTextDrawSetShadow(playerid, td, 0);
	PlayerTextDrawBackgroundColour(playerid, td, 0x000000AA);
	PlayerTextDrawSetProportional(playerid, td, true);
	return td;
}

// Полоска: ширина от 0 до full по проценту
static Hud_SetBar(playerid, PlayerText:td, Float:x, Float:full, pct, colour)
{
	if (pct <= 0)
	{
		PlayerTextDrawHide(playerid, td);
		return;
	}
	if (pct > 100) pct = 100;
	PlayerTextDrawTextSize(playerid, td, x + full * float(pct) / 100.0, 0.0);
	PlayerTextDrawBoxColour(playerid, td, colour);
	PlayerTextDrawShow(playerid, td);
}

static Hud_PctColour(pct)
{
	if (pct <= 20) return HUD_COL_BAD;
	if (pct <= 50) return HUD_COL_WARN;
	return HUD_COL_GOOD;
}

// Клиент сам рисует панель HUD: телефон с "-bui" или ПК-плагин новой версии.
// Старому ПК-плагину один раз говорим, что окна нужно обновить через лаунчер.
stock bool:UI_HasHud(playerid)
{
	if (!gUiSupported[playerid]) return false;
	if (IsPlayerMobile[playerid] || gUiPcVersion[playerid] >= UI_PC_HUD_VERSION) return true;
	new version[32];
	GetPlayerVersion(playerid, version, sizeof version);
	if (strfind(version, UI_CLIENT_TAG) != -1) return true;
	if (!gUiPcOldWarned[playerid] && Player[playerid][pLogged])
	{
		gUiPcOldWarned[playerid] = true;
		SendClientMessage(playerid, 0xFF9900FF, "Окна Bastion у тебя старые, поэтому нет панели HUD. Выйди из игры и зайди через BastionLauncher.exe: он обновит их сам.");
	}
	return false;
}

// ---------------------------------------------------------------------
//  HTML-панель клиента Bastion
// ---------------------------------------------------------------------
static Hud_WebReset(playerid)
{
	gHudWebOn[playerid] = true;
	gHudWebCash[playerid] = cellmin;
	gHudWebBank[playerid] = cellmin;
	gHudWebHp[playerid] = -1;
	gHudWebArmour[playerid] = -1;
	gHudWebFood[playerid] = -1;
	gHudWebLevel[playerid] = -1;
}

static Hud_WebSend(playerid, const json[])
{
	// Только когда окна нет: иначе сервер забудет id открытого окна и не примет ответ
	new cur = GetPlayerDialogID(playerid);
	if (cur != -1 && cur != HUD_DIALOG_ID) return 0;
	ShowPlayerDialog(playerid, HUD_DIALOG_ID, DIALOG_STYLE_MSGBOX, "[hud]", "%s", "_", "", json);
	return 1;
}

static Hud_WebUpdate(playerid)
{
	if (!gHudWebOn[playerid]) return;
	new Float:health, Float:armour;
	GetPlayerHealth(playerid, health);
	GetPlayerArmour(playerid, armour);
	new hp = floatround(health), arm = floatround(armour);
	if (hp < 0) hp = 0;
	if (arm < 0) arm = 0;
	new cash = GetMoney(playerid), bank = Player[playerid][pBank];
	new food = Needs_GetHunger(playerid), level = Player[playerid][pLevel];
	if (hp == gHudWebHp[playerid] && arm == gHudWebArmour[playerid] && cash == gHudWebCash[playerid]
		&& bank == gHudWebBank[playerid] && food == gHudWebFood[playerid] && level == gHudWebLevel[playerid]) return;

	new json[144];
	format(json, sizeof json, "{\"id\":%d,\"lvl\":%d,\"hp\":%d,\"arm\":%d,\"food\":%d,\"cash\":%d,\"bank\":%d}",
		playerid, level, hp, arm, food, cash, bank);
	if (!Hud_WebSend(playerid, json)) return;    // окно открыто — отправим, когда закроется
	gHudWebHp[playerid] = hp;
	gHudWebArmour[playerid] = arm;
	gHudWebCash[playerid] = cash;
	gHudWebBank[playerid] = bank;
	gHudWebFood[playerid] = food;
	gHudWebLevel[playerid] = level;
}

static Hud_WebOff(playerid)
{
	if (!gHudWebOn[playerid]) return;
	gHudWebOn[playerid] = false;
	Hud_WebSend(playerid, "{\"off\":1}");
}

// ---------------------------------------------------------------------
//  Панель статуса
// ---------------------------------------------------------------------
static Hud_CreateStatus(playerid)
{
	gHudWant[playerid] = true;
	if (UI_HasHud(playerid)) Hud_WebReset(playerid);   // панель рисует клиент
}

static Hud_DestroyStatus(playerid)
{
	gHudWant[playerid] = false;
}

// ---------------------------------------------------------------------
//  Спидометр
// ---------------------------------------------------------------------
static Hud_CreateSpeedo(playerid)
{
	if (gHudSpeedoOn[playerid]) return;
	new Float:bx, Float:by;
	if (IsPlayerMobile[playerid]) { bx = 320.0 - HUD_SPEEDO_W / 2.0; by = 42.0; }
	else { bx = 505.0; by = 346.0; }
	gHudSpeedoX[playerid] = bx;

	new Float:r = bx + HUD_SPEEDO_W;
	new Float:barX = r - 5.0 - HUD_SPEEDO_BAR_W;
	gHudSpeedo[playerid][SP_BG]       = Hud_Box(playerid, bx, by, r, 8.4, HUD_COL_BG);
	gHudSpeedo[playerid][SP_ACCENT]   = Hud_Box(playerid, bx, by, r, -0.25, HUD_COL_ACCENT);
	gHudSpeedo[playerid][SP_NAME]     = Hud_Text(playerid, bx + 5.0, by + 3.0, "_", TEXT_DRAW_FONT_2, 0.17, 0.8, HUD_COL_ACCENT);
	gHudSpeedo[playerid][SP_SPEED]    = Hud_Text(playerid, bx + 72.0, by + 12.0, "0", TEXT_DRAW_FONT_3, 0.6, 2.4, 0xFFFFFFFF, TEXT_DRAW_ALIGN_RIGHT);
	gHudSpeedo[playerid][SP_KMH]      = Hud_Text(playerid, bx + 76.0, by + 25.0, "KM/H", TEXT_DRAW_FONT_2, 0.17, 0.8, HUD_COL_LABEL);
	gHudSpeedo[playerid][SP_FUEL_LBL] = Hud_Text(playerid, bx + 5.0, by + 42.0, "FUEL", TEXT_DRAW_FONT_2, 0.14, 0.7, HUD_COL_LABEL);
	gHudSpeedo[playerid][SP_FUEL_BG]  = Hud_Box(playerid, barX, by + 44.0, r - 5.0, 0.3, HUD_COL_BAR_BG);
	gHudSpeedo[playerid][SP_FUEL_FILL]= Hud_Box(playerid, barX, by + 44.0, r - 5.0, 0.3, HUD_COL_GOOD);
	gHudSpeedo[playerid][SP_HP_LBL]   = Hud_Text(playerid, bx + 5.0, by + 52.0, "BODY", TEXT_DRAW_FONT_2, 0.14, 0.7, HUD_COL_LABEL);
	gHudSpeedo[playerid][SP_HP_BG]    = Hud_Box(playerid, barX, by + 54.0, r - 5.0, 0.3, HUD_COL_BAR_BG);
	gHudSpeedo[playerid][SP_HP_FILL]  = Hud_Box(playerid, barX, by + 54.0, r - 5.0, 0.3, HUD_COL_GOOD);
	gHudSpeedo[playerid][SP_FLAGS]    = Hud_Text(playerid, bx + 5.0, by + 63.0, "_", TEXT_DRAW_FONT_2, 0.15, 0.75, 0xFFFFFFFF);

	for (new i = 0; i < SP_COUNT; i++)
	{
		if (i != SP_FUEL_FILL && i != SP_HP_FILL) PlayerTextDrawShow(playerid, gHudSpeedo[playerid][i]);
	}
	gHudSpeedoOn[playerid] = true;
	gHudLastSpeed[playerid] = -1;
	gHudLastFuel[playerid] = -1;
	gHudLastHp[playerid] = -1;
	gHudLastFlags[playerid] = -1;
	gHudLastModel[playerid] = -1;
}

stock Hud_HideSpeedo(playerid)
{
	if (!gHudSpeedoOn[playerid]) return;
	for (new i = 0; i < SP_COUNT; i++) PlayerTextDrawDestroy(playerid, gHudSpeedo[playerid][i]);
	gHudSpeedoOn[playerid] = false;
}

static Hud_UpdateSpeedo(playerid, bool:full)
{
	new v = GetPlayerVehicleID(playerid);
	if (v == 0) return;
	if (!gHudSpeedoOn[playerid]) { Hud_CreateSpeedo(playerid); full = true; }

	new text[48];
	new speed = Fuel_GetSpeed(v);
	if (speed != gHudLastSpeed[playerid])
	{
		gHudLastSpeed[playerid] = speed;
		format(text, sizeof text, "%d", speed);
		PlayerTextDrawSetString(playerid, gHudSpeedo[playerid][SP_SPEED], text);
	}
	if (!full) return;

	new model = GetVehicleModel(v);
	if (model != gHudLastModel[playerid])
	{
		gHudLastModel[playerid] = model;
		Hud_GetVehicleName(model, text);
		PlayerTextDrawSetString(playerid, gHudSpeedo[playerid][SP_NAME], text);
	}

	new Float:barX = gHudSpeedoX[playerid] + HUD_SPEEDO_W - 5.0 - HUD_SPEEDO_BAR_W;
	new bool:usesFuel = Fuel_VehicleUsesFuel(v);
	new fuel = usesFuel ? floatround(Fuel_Get(v) * 100.0 / float(FUEL_TANK)) : 100;
	if (fuel != gHudLastFuel[playerid])
	{
		gHudLastFuel[playerid] = fuel;
		Hud_SetBar(playerid, gHudSpeedo[playerid][SP_FUEL_FILL], barX, HUD_SPEEDO_BAR_W, fuel, Hud_PctColour(fuel));
	}

	new Float:health;
	GetVehicleHealth(v, health);
	new hp = floatround((health - 250.0) * 100.0 / 750.0);    // 250 — машина горит
	if (hp < 0) hp = 0;
	if (hp > 100) hp = 100;
	if (hp != gHudLastHp[playerid])
	{
		gHudLastHp[playerid] = hp;
		Hud_SetBar(playerid, gHudSpeedo[playerid][SP_HP_FILL], barX, HUD_SPEEDO_BAR_W, hp, Hud_PctColour(hp));
	}

	new engine, lights, alarm, doors, bonnet, boot, objective;
	GetVehicleParamsEx(v, engine, lights, alarm, doors, bonnet, boot, objective);
	new flags = (engine == 1 ? 1 : 0) | (lights == 1 ? 2 : 0) | (doors == 1 ? 4 : 0);
	if (flags != gHudLastFlags[playerid])
	{
		gHudLastFlags[playerid] = flags;
		format(text, sizeof text, "%sENGINE  %sLIGHTS  %sLOCK",
			(flags & 1) ? ("~g~") : ("~r~"),
			(flags & 2) ? ("~y~") : ("~l~"),
			(flags & 4) ? ("~g~") : ("~l~"));
		PlayerTextDrawSetString(playerid, gHudSpeedo[playerid][SP_FLAGS], text);
	}
}

// ---------------------------------------------------------------------
//  Подключение к моду
// ---------------------------------------------------------------------
forward Hud_OnTick();

stock Hud_Init()
{
	SetTimer("Hud_OnTick", HUD_TICK_MS, true);
}

// Из OnPlayerSpawn: показать панель после входа
stock Hud_OnSpawn(playerid)
{
	if (!gHudStatusHidden[playerid]) Hud_CreateStatus(playerid);
}

// Из OnPlayerDisconnect: текстдравы удаляет сервер, только сбросить флаги
stock Hud_OnDisconnect(playerid)
{
	gHudWant[playerid] = false;
	gHudSpeedoOn[playerid] = false;
	gHudStatusHidden[playerid] = false;
	gHudSpeedoHidden[playerid] = false;
	gHudWebOn[playerid] = false;
}

// Из OnDialogResponse: ответ на скрытый диалог панели ничего не делает
stock bool:Hud_IsDialog(dialogid)
{
	return dialogid == HUD_DIALOG_ID;
}

public Hud_OnTick()
{
	new bool:slow = (++gHudTick % HUD_SLOW_TICKS) == 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged]) continue;
		if (gHudWant[i] && !gHudWebOn[i] && !gHudStatusHidden[i] && UI_HasHud(i)) Hud_WebReset(i);  // клиент включил окна после спавна
		Hud_WebUpdate(i);
		if (gHudSpeedoHidden[i] || GetPlayerState(i) != PLAYER_STATE_DRIVER) Hud_HideSpeedo(i);
		else Hud_UpdateSpeedo(i, slow || !gHudSpeedoOn[i]);
	}
	return 1;
}

stock bool:Hud_ToggleSpeedo(playerid)
{
	gHudSpeedoHidden[playerid] = !gHudSpeedoHidden[playerid];
	if (gHudSpeedoHidden[playerid]) Hud_HideSpeedo(playerid);
	return gHudSpeedoHidden[playerid];
}

stock bool:Cmd_Hud(playerid)
{
	gHudStatusHidden[playerid] = !gHudStatusHidden[playerid];
	if (gHudStatusHidden[playerid])
	{
		Hud_DestroyStatus(playerid);
		Hud_WebOff(playerid);
		SendClientMessage(playerid, COLOR_WHITE, "Панель статуса спрятана. Вернуть: /hud");
	}
	else
	{
		Hud_CreateStatus(playerid);
		SendClientMessage(playerid, COLOR_WHITE, "Панель статуса включена.");
	}
	return true;
}
