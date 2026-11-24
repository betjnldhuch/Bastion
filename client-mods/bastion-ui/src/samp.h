#pragma once
#include <cstdint>
#include <string>

// Адреса внутри samp.dll для поддерживаемых версий клиента (из SAMP-API, MIT).
namespace samp {

struct Offsets {
    const char* name;
    uintptr_t dialogShow;     // CDialog::Show
    uintptr_t dialogHide;     // CDialog::Hide
    uintptr_t refDialog;      // CDialog* singleton
    uintptr_t refNetGame;     // CNetGame* singleton
    uintptr_t getRakClient;   // CNetGame::GetRakClient (0 — читать поле rakClientField)
    uintptr_t rakClientField; // смещение CNetGame::m_pRakClient
    uintptr_t refGame;        // CGame* singleton
    uintptr_t setCursorMode;  // CGame::SetCursorMode
    uintptr_t gameStateField; // смещение CNetGame::m_nGameState
};

// Ждёт samp.dll и определяет версию. false — версия не поддерживается.
bool Detect();
uintptr_t Base();
const Offsets& Get();
uint32_t EntryPoint();

void SetCursor(bool visible);
bool SendDialogResponse(int dialogId, int button, int listItem, const std::string& cp1251Text);
bool SendCommand(const std::string& cp1251Command);

// Состояние подключения CNetGame (0.3.7: 9 — ждёт подключения, 13 — подключается,
// 14 — в игре, 15 — ждёт входа). -1 — нет CNetGame.
enum { kStateWaitConnect = 9, kStateConnecting = 13, kStateConnected = 14, kStateAwaitJoin = 15 };
int GetGameState();
void SetGameState(int state);

}  // namespace samp
