// Прячет полоски здоровья/брони/кислорода и деньги стандартного HUD GTA SA 1.0 US.
// Кислород отдаём странице (BastionUI.breath).
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
constexpr uintptr_t kRenderBreathBar = 0x589190;  // CHud::RenderBreathBar(int, int, int)
constexpr uintptr_t kPlayers = 0xB7CD98;          // CWorld::Players[], CPlayerInfo 0x190, m_pPed в начале
constexpr uintptr_t kPedPlayerData = 0x480;       // CPed::m_pPlayerData
constexpr uintptr_t kPlayerDataBreath = 0x44;     // CPlayerData::m_fBreath
constexpr uintptr_t kPrintString = 0x71A700;      // CFont::PrintString(float, float, const char*)

using VoidFn = void(__cdecl*)();
using BarFn = void(__cdecl*)(int, int, int);
using PrintFn = void(__cdecl*)(float, float, const char*);

VoidFn g_origInfo = nullptr;
BarFn g_origHealth = nullptr;
BarFn g_origArmor = nullptr;
BarFn g_origBreath = nullptr;
PrintFn g_origPrint = nullptr;

volatile bool g_replaced = false;
bool g_inInfo = false;
volatile DWORD g_lastInfo = 0;
bool g_installed = false;
volatile DWORD g_lastBreath = 0;
volatile int g_breathPct = -1;
float g_breathMax = 0.0f;   // запас воздуха растёт со статой лёгких: берём наибольший виденный

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

// Полоску рисуют, пока игрок под водой или воздух не восстановился
void __cdecl HookBreath(int id, int x, int y) {
    float breath = -1.0f;
    __try {
        uintptr_t ped = *reinterpret_cast<uintptr_t*>(kPlayers + 0x190 * id);
        uintptr_t data = ped ? *reinterpret_cast<uintptr_t*>(ped + kPedPlayerData) : 0;
        if (data) breath = *reinterpret_cast<float*>(data + kPlayerDataBreath);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        breath = -1.0f;
    }
    if (breath >= 0.0f && breath < 100000.0f) {
        if (breath > g_breathMax) g_breathMax = breath;
        int pct = g_breathMax > 0.0f ? static_cast<int>(breath * 100.0f / g_breathMax + 0.5f) : 100;
        g_breathPct = pct > 100 ? 100 : pct;
        g_lastBreath = GetTickCount();
        if (g_replaced) return;
    }
    g_origBreath(id, x, y);
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
    Hook(kRenderBreathBar, reinterpret_cast<void*>(&HookBreath), g_origBreath, "CHud::RenderBreathBar");
    Hook(kPrintString, reinterpret_cast<void*>(&HookPrint), g_origPrint, "CFont::PrintString");
    g_installed = true;
    return true;
}

void SetReplaced(bool on) { g_replaced = on && g_installed; }

bool Drawn() {
    if (!g_installed) return *reinterpret_cast<volatile bool*>(0xBA67A4) == false;  // FrontEndMenuManager.m_bMenuActive
    return GetTickCount() - g_lastInfo < 300;
}

int Breath() {
    if (!g_replaced || !g_origBreath || GetTickCount() - g_lastBreath > 300) return -1;
    return g_breathPct;
}

}  // namespace gtahud
