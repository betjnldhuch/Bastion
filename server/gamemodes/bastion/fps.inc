// =====================================================================
//  fps.inc — FPS игроков.
//
//  Сервер не может «включить» игроку 120 или 165 FPS: это делает
//  лаунчер (пакет FPS, см. launcher/fps/README.txt). Здесь сервер:
//   • узнаёт настоящий FPS каждого игрока (через уровень опьянения:
//     клиент уменьшает его на 1 каждый кадр);
//   • показывает его: /fps — свой, /fps [id] — чужой, /fpslist — админам;
//   • по желанию ограничивает максимум (FPS_MAX_ALLOWED в config.inc).
// =====================================================================

#define FPS_DRUNK_START     2000   // с этого уровня считаем (2000 — ещё без эффекта опьянения)
#define FPS_DRUNK_RESET     100    // опустился ниже — снова ставим FPS_DRUNK_START

new gFpsDrunkLast[MAX_PLAYERS];
new gFps[MAX_PLAYERS];
#if FPS_MAX_ALLOWED > 0
new gFpsOver[MAX_PLAYERS];      // сколько секунд подряд FPS выше разрешённого
#endif

stock Fps_Init()
{
	SetTimer("Fps_Check", 1000, true);
}

stock Fps_Reset(playerid)
{
	gFpsDrunkLast[playerid] = 0;
	gFps[playerid] = 0;
#if FPS_MAX_ALLOWED > 0
	gFpsOver[playerid] = 0;
#endif
}

stock Fps_Get(playerid)
{
	return gFps[playerid];
}

// Вызывается из OnPlayerUpdate (много раз в секунду)
stock Fps_OnUpdate(playerid)
{
	new drunk = GetPlayerDrunkLevel(playerid);
	if (drunk < FPS_DRUNK_RESET)
	{
		SetPlayerDrunkLevel(playerid, FPS_DRUNK_START);
		gFpsDrunkLast[playerid] = FPS_DRUNK_START;
		return;
	}
	if (drunk != gFpsDrunkLast[playerid])
	{
		new frames = gFpsDrunkLast[playerid] - drunk;
		// клиент присылает уровень раз в секунду, так что разница = кадров за секунду
		if (frames > 0 && frames < 1000) gFps[playerid] = frames;
		gFpsDrunkLast[playerid] = drunk;
	}
}

// Раз в секунду: проверка лимита (если он включён)
forward Fps_Check();
public Fps_Check()
{
#if FPS_MAX_ALLOWED > 0
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i) || !Player[i][pLogged]) continue;
		// небольшой запас: FPS меряется неточно и «скачет» на пару кадров
		if (gFps[i] > FPS_MAX_ALLOWED + 10)
		{
			gFpsOver[i]++;
			if (gFpsOver[i] == 10)
				SendClientMessage(i, COLOR_RED, "У тебя %d FPS, а на сервере можно не больше %d. Поменяй лимит в лаунчере (Настройки), иначе через 20 секунд кикнет.",
					gFps[i], FPS_MAX_ALLOWED);
			else if (gFpsOver[i] >= 30)
			{
				SendClientMessage(i, COLOR_RED, "Кик: FPS выше %d. Поставь лимит в лаунчере и заходи снова.", FPS_MAX_ALLOWED);
				KickWithDelay(i);
				gFpsOver[i] = 0;
			}
		}
		else gFpsOver[i] = 0;
	}
#endif
	return 1;
}

// /fps или /fps [id]
stock bool:Cmd_Fps(playerid, const params[])
{
	new pos = 0;
	new target = NextPlayerParam(params, pos);
	if (target == INVALID_PLAYER_ID) target = playerid;
	if (gFps[target] == 0)
		return SendClientMessage(playerid, COLOR_GREY, "FPS ещё не измерен — подожди пару секунд (в паузе не меряется).");
	if (target == playerid)
		SendClientMessage(playerid, COLOR_WHITE, "Твой FPS: %d.", gFps[target]);
	else
		SendClientMessage(playerid, COLOR_WHITE, "FPS игрока %s: %d.", GetRPName(target), gFps[target]);
	return true;
}

// /fpslist — FPS всех игроков (для админов)
stock bool:Cmd_FpsList(playerid)
{
	if (!Admin_Check(playerid, 1)) return true;
	new text[2048], line[64], n = 0;
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (!IsPlayerConnected(i)) continue;
		format(line, sizeof line, "%d. %s — %d FPS\n", i, GetRPName(i), gFps[i]);
		if (strlen(text) + strlen(line) >= sizeof text) break;
		strcat(text, line);
		n++;
	}
	if (n == 0) strcat(text, "Никого нет.");
	ShowPlayerDialog(playerid, DIALOG_HELP, DIALOG_STYLE_MSGBOX, "FPS игроков", "%s", "Закрыть", "", text);
	return true;
}
