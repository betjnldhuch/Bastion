#pragma once

// Стандартный HUD GTA SA 1.0 US: прячем полоски здоровья/брони и деньги,
// когда сервер прислал свою панель (BastionUI.hud в index.html).
namespace gtahud {
bool Install();             // после MH_Initialize; false — адреса не похожи на 1.0 US
void SetReplaced(bool on);  // true — рисуем свою панель, стандартные полоски и деньги не нужны
bool Drawn();               // игра сейчас рисует HUD игрока (не меню, не катсцена, не смерть)
int Breath();               // кислород под водой 0..100 (вместо стандартной полоски), -1 — полоски нет
}
