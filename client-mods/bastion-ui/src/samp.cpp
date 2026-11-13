#include "samp.h"

#include <windows.h>

#include "RakNet/BitStream.h"
#include "RakNet/RakClientInterface.h"

namespace samp {

namespace {
uintptr_t g_base = 0;
uint32_t g_entry = 0;
const Offsets* g_off = nullptr;

// Точка входа samp.dll (AddressOfEntryPoint) однозначно задаёт версию
struct Known { uint32_t entry; Offsets off; };
const Known kKnown[] = {
    {0x31DF13, {"0.3.7-R1", 0x6B9C0, 0x6B210, 0x21A0B8, 0x21A0F8, 0x1A40, 0, 0x21A10C, 0x9BD30}},
    {0xCC4D0,  {"0.3.7-R3", 0x6F8C0, 0x6F110, 0x26E898, 0x26E8DC, 0x1A40, 0, 0x26E8F4, 0x9FFE0}},
    {0xCBC90,  {"0.3.7-R5", 0x6FFB0, 0x6F860, 0x26EB50, 0x26EB94, 0xBBC0, 0, 0x26EBAC, 0xA06F0}},
    {0xFDB60,  {"0.3.DL",   0x6FA50, 0x6F2A0, 0x2AC9E0, 0x2ACA24, 0, 0x3C9, 0x2ACA3C, 0xA0530}},
};

enum { RPC_ServerCommand = 50, RPC_DialogResponse = 62 };

RakClientInterface* RakClient() {
    if (!g_off) return nullptr;
    void* netGame = *reinterpret_cast<void**>(g_base + g_off->refNetGame);
    if (!netGame) return nullptr;
    if (g_off->getRakClient) {
        using Fn = RakClientInterface*(__thiscall*)(void*);
        return reinterpret_cast<Fn>(g_base + g_off->getRakClient)(netGame);
    }
    return *reinterpret_cast<RakClientInterface**>(reinterpret_cast<uintptr_t>(netGame) + g_off->rakClientField);
}

bool SendRpc(int id, RakNet::BitStream& bs) {
    RakClientInterface* rak = RakClient();
    if (!rak) return false;
    return rak->RPC(&id, &bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false);
}
}  // namespace

bool Detect() {
    HMODULE mod = GetModuleHandleW(L"samp.dll");
    if (!mod) return false;
    g_base = reinterpret_cast<uintptr_t>(mod);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(g_base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(g_base + dos->e_lfanew);
    g_entry = nt->OptionalHeader.AddressOfEntryPoint;
    for (const Known& k : kKnown)
        if (k.entry == g_entry) { g_off = &k.off; return true; }
    return false;
}

uintptr_t Base() { return g_base; }
const Offsets& Get() { return *g_off; }
uint32_t EntryPoint() { return g_entry; }

void SetCursor(bool visible) {
    if (!g_off) return;
    void* game = *reinterpret_cast<void**>(g_base + g_off->refGame);
    if (!game) return;
    using Fn = void(__thiscall*)(void*, int, BOOL);
    // 2 = CURSOR_LOCKCAMANDCONTROL (как у обычного диалога), 0 = без курсора
    reinterpret_cast<Fn>(g_base + g_off->setCursorMode)(game, visible ? 2 : 0, visible ? FALSE : TRUE);
}

bool SendDialogResponse(int dialogId, int button, int listItem, const std::string& txt) {
    std::string t = txt.size() > 255 ? txt.substr(0, 255) : txt;
    RakNet::BitStream bs;
    bs.Write(static_cast<uint16_t>(dialogId));
    bs.Write(static_cast<uint8_t>(button));
    bs.Write(static_cast<int16_t>(listItem));
    bs.Write(static_cast<uint8_t>(t.size()));
    bs.Write(t.data(), static_cast<int>(t.size()));
    return SendRpc(RPC_DialogResponse, bs);
}

bool SendCommand(const std::string& cmd) {
    RakNet::BitStream bs;
    bs.Write(static_cast<uint32_t>(cmd.size()));
    bs.Write(cmd.data(), static_cast<int>(cmd.size()));
    return SendRpc(RPC_ServerCommand, bs);
}

}  // namespace samp
