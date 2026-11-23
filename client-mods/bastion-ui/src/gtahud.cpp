// Прячет полоски здоровья/брони и деньги стандартного HUD GTA SA 1.0 US.
// Адреса — gta-reversed / plugin-sdk (CHud, CFont). Перед перехватом проверяем,
// что перед адресом конец предыдущей функции (ret / int3 / nop): если exe
// другой версии, ничего не трогаем и стандартный HUD остаётся.
#include "gtahud.h"

#include <windows.h>

#include <cstdint>

#include "MinHook.h"
#include "log.h"

namespace gtahud {
namespace {

constexpr uintptr_t kDrawPlayerInfo = 0x58EAF0;   // CHud::DrawPlayerInfo()
constexpr uintptr_t kRenderHealthBar = 0x589270;  // CHud::RenderHealthBar(int, int, int)
constexpr uintptr_t kRenderArmorBar = 0x5890A0;   // CHud::RenderArmorBar(int, int, int)
constexpr uintptr_t kPrintString = 0x71A700;      // CFont::PrintString(float, float, const char*)

using VoidFn = void(__cdecl*)();
using BarFn = void(__cdecl*)(int, int, int);
using PrintFn = void(__cdecl*)(float, float, const char*);

VoidFn g_origInfo = nullptr;
BarFn g_origHealth = nullptr;
BarFn g_origArmor = nullptr;
PrintFn g_origPrint = nullptr;

volatile bool g_replaced = false;
bool g_inInfo = false;
volatile DWORD g_lastInfo = 0;
bool g_installed = false;

bool LooksLikeFunction(uintptr_t addr) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(reinterpret_cast<void*>(addr - 3), &mbi, sizeof mbi) || !(mbi.Protect & 0xF0)) return false;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(addr);
    uint8_t prev = p[-1];
    return prev == 0xC3 || prev == 0xCC || prev == 0x90 || p[-3] == 0xC2;
}

void __cdecl HookInfo() {
    g_lastInfo = GetTickCount();
    g_inInfo = true;
    g_origInfo();
    g_inInfo = false;
}

void __cdecl HookHealth(int id, int x, int y) {
    if (!g_replaced) g_origHealth(id, x, y);
}

void __cdecl HookArmor(int id, int x, int y) {
    if (!g_replaced) g_origArmor(id, x, y);
}

// Деньги — "$00001500" или "-$0001500" внутри DrawPlayerInfo
void __cdecl HookPrint(float x, float y, const char* s) {
    if (g_replaced && g_inInfo && s && (s[0] == '$' || (s[0] == '-' && s[1] == '$'))) return;
    g_origPrint(x, y, s);
}

template <typename T>
bool Hook(uintptr_t addr, void* detour, T& orig, const char* name) {
    if (!LooksLikeFunction(addr)) {
        Log("gtahud: %s (0x%X) не похож на GTA SA 1.0 US", name, static_cast<unsigned>(addr));
        return false;
    }
    void* target = reinterpret_cast<void*>(addr);
    if (MH_CreateHook(target, detour, reinterpret_cast<void**>(&orig)) != MH_OK || MH_EnableHook(target) != MH_OK) {
        Log("gtahud: hook %s failed", name);
        return false;
    }
    return true;
}

}  // namespace

bool Install() {
    // Без DrawPlayerInfo не узнать, где деньги, и когда HUD виден — тогда ничего не прячем
    if (!Hook(kDrawPlayerInfo, reinterpret_cast<void*>(&HookInfo), g_origInfo, "CHud::DrawPlayerInfo")) return false;
    Hook(kRenderHealthBar, reinterpret_cast<void*>(&HookHealth), g_origHealth, "CHud::RenderHealthBar");
    Hook(kRenderArmorBar, reinterpret_cast<void*>(&HookArmor), g_origArmor, "CHud::RenderArmorBar");
    Hook(kPrintString, reinterpret_cast<void*>(&HookPrint), g_origPrint, "CFont::PrintString");
    g_installed = true;
    return true;
}

void SetReplaced(bool on) { g_replaced = on && g_installed; }

bool Drawn() {
    if (!g_installed) return *reinterpret_cast<volatile bool*>(0xBA67A4) == false;  // FrontEndMenuManager.m_bMenuActive
    return GetTickCount() - g_lastInfo < 300;
}

}  // namespace gtahud
