// =====================================================================
//  bastion_ui.asi — HTML-окна Bastion на ПК.
//
//  Сервер шлёт обычный диалог SA-MP. Если заголовок "[ui:ИМЯ]", а текст —
//  JSON (ui.inc), открываем окно ИМЯ из bastion_ui\ui\index.html (та же
//  страница, что в Android-клиенте). Остальные диалоги рисуются той же
//  страницей в стиле Bastion (окно "samp"). Ответ уходит серверу обычным
//  RPC DialogResponse, мод получает его в OnDialogResponse.
//
//  Сервер узнаёт, что окна есть, по скрытой команде /__bui (ui.inc).
//  Диалог "[hud]" — данные панели HUD (hud.inc): не показываем, передаём
//  странице (BastionUI.hud), стандартные полоски ХП/брони и деньги прячем.
//  Если CEF не запустился или версия SA-MP неизвестна — обычные диалоги.
// =====================================================================
#include <windows.h>
#include <d3d9.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "MinHook.h"
#include "browser.h"
#include "log.h"
#include "gtahud.h"
#include "overlay.h"
#include "samp.h"
#include "text.h"

// ---------------- журнал ----------------
static FILE* g_log = nullptr;

void LogOpen(const std::wstring& path) { g_log = _wfopen(path.c_str(), L"w"); }

void Log(const char* fmt, ...) {
    if (!g_log) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(g_log, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

namespace {

// GTA SA 1.0 US
IDirect3DDevice9** const kDevice = reinterpret_cast<IDirect3DDevice9**>(0xC97C28);
HWND* const kWindow = reinterpret_cast<HWND*>(0xC97C1C);

std::wstring g_dir;     // ...\GTA\bastion_ui
HWND g_hwnd = nullptr;
WNDPROC g_origWndProc = nullptr;
bool g_cefTried = false;
bool g_visible = false;  // окно открыто: перехватываем мышь и клавиатуру
int g_dialogId = -1;
bool g_helloSent = false;
bool g_hudOn = false;    // сервер прислал свою панель HUD
DWORD g_lastClick = 0;
int g_lastClickX = 0, g_lastClickY = 0, g_clicks = 0;

using ShowFn = void(__thiscall*)(void*, int, int, const char*, const char*, const char*, const char*, BOOL);
using HideFn = void(__thiscall*)(void*);
using PresentFn = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using ResetFn = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
ShowFn g_origShow = nullptr;
HideFn g_origHide = nullptr;
PresentFn g_origPresent = nullptr;
ResetFn g_origReset = nullptr;

void CloseWindowUi(bool tellPage) {
    if (!g_visible) return;
    g_visible = false;
    g_dialogId = -1;
    if (tellPage) browser::ExecJs("window.BastionUI && BastionUI.hide && BastionUI.hide()");
    browser::Focus(false);
    samp::SetCursor(false);
}

// ---------------- диалоги ----------------
void __fastcall HookShow(void* self, void*, int id, int type, const char* caption, const char* txt, const char* b1,
                         const char* b2, BOOL serverside) {
    if (serverside && caption && strncmp(caption, "[hud]", 5) == 0) {
        // панель HUD: окна нет, ответа нет; без страницы просто молчим
        if (!browser::PageLoaded()) return;
        std::string body = text::Cp1251ToUtf8(txt);
        g_hudOn = body.find("\"off\"") == std::string::npos;
        gtahud::SetReplaced(g_hudOn);
        browser::ExecJs("window.BastionUI && BastionUI.hud && BastionUI.hud(" + text::JsonQuote(body) + ")");
        return;
    }
    if (!serverside || !browser::PageLoaded()) {
        g_origShow(self, id, type, caption, txt, b1, b2, serverside);
        return;
    }
    if (!g_helloSent) {
        g_helloSent = true;
        samp::SendCommand("/__bui");  // сервер включит HTML-окна (ui.inc) и перешлёт окно входа
    }

    std::string cap = text::Cp1251ToUtf8(caption);
    std::string body = text::Cp1251ToUtf8(txt);
    std::string window = "samp", data;
    size_t close = cap.find(']');
    if (cap.rfind("[ui:", 0) == 0 && close != std::string::npos && close > 4) {
        window = cap.substr(4, close - 4);
        data = text::JsonQuote(body);  // страница сама сделает JSON.parse
    } else {
        data = "{\"style\":" + std::to_string(type) + ",\"caption\":" + text::JsonQuote(cap) +
               ",\"text\":" + text::JsonQuote(body) + "}";
    }
    std::string msg = "{\"window\":" + text::JsonQuote(window) + ",\"button1\":" +
                      text::JsonQuote(text::Cp1251ToUtf8(b1)) + ",\"button2\":" +
                      text::JsonQuote(text::Cp1251ToUtf8(b2)) + ",\"data\":" + data + "}";
    browser::ExecJs("window.BastionUI && BastionUI.open(" + msg + ")");

    g_dialogId = id;
    if (!g_visible) {
        g_visible = true;
        samp::SetCursor(true);
    }
    browser::Focus(true);
}

void __fastcall HookHide(void* self, void*) {
    CloseWindowUi(true);  // сервер закрыл диалог (ShowPlayerDialog с -1)
    g_origHide(self);
}

// ---------------- кадр ----------------
HRESULT __stdcall HookPresent(IDirect3DDevice9* dev, const RECT* a, const RECT* b, HWND w, const RGNDATA* r) {
    if (!g_cefTried) {
        g_cefTried = true;  // CefInitialize — в главном потоке игры
        if (!browser::Init(g_hwnd, g_dir)) Log("CEF не запустился: HTML-окна выключены");
    }
    browser::Response resp;
    while (browser::PopResponse(resp)) {
        if (g_dialogId < 0) continue;
        int id = g_dialogId;
        CloseWindowUi(false);
        if (!samp::SendDialogResponse(id, resp.button, resp.listItem, text::Utf8ToCp1251(resp.text)))
            Log("DialogResponse %d не отправлен", id);
    }
    overlay::Present(dev, g_visible || (g_hudOn && gtahud::Drawn()));
    return g_origPresent(dev, a, b, w, r);
}

HRESULT __stdcall HookReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {
    overlay::OnLost();
    return g_origReset(dev, pp);
}

// ---------------- мышь и клавиатура ----------------
void ToView(LPARAM lp, int& x, int& y) {
    x = static_cast<short>(LOWORD(lp));
    y = static_cast<short>(HIWORD(lp));
    RECT rc;
    int bw, bh;
    overlay::BackBufferSize(bw, bh);
    if (GetClientRect(g_hwnd, &rc) && rc.right > 0 && rc.bottom > 0 && bw > 0 && bh > 0) {
        x = x * bw / rc.right;
        y = y * bh / rc.bottom;
    }
}

LRESULT CALLBACK HookWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_visible) {
        int x, y;
        switch (msg) {
        case WM_MOUSEMOVE:
            ToView(lp, x, y);
            browser::MouseMove(x, y);
            return 0;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: {
            ToView(lp, x, y);
            DWORD now = GetTickCount();
            bool same = now - g_lastClick < GetDoubleClickTime() && abs(x - g_lastClickX) < 4 && abs(y - g_lastClickY) < 4;
            g_clicks = same ? g_clicks + 1 : 1;
            g_lastClick = now;
            g_lastClickX = x;
            g_lastClickY = y;
            browser::MouseButton(msg == WM_RBUTTONDOWN ? 1 : 0, false, x, y, g_clicks);
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
            ToView(lp, x, y);
            browser::MouseButton(msg == WM_RBUTTONUP ? 1 : 0, true, x, y, g_clicks);
            return 0;
        case WM_MOUSEWHEEL: {
            POINT p = {static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};  // экранные координаты
            ScreenToClient(hwnd, &p);
            ToView(MAKELPARAM(p.x, p.y), x, y);
            browser::MouseWheel(x, y, GET_WHEEL_DELTA_WPARAM(wp));
            return 0;
        }
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_CHAR:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_SYSCHAR:
            if (wp == VK_F4 && (msg == WM_SYSKEYDOWN)) break;  // Alt+F4 — игре
            browser::KeyMessage(msg, wp, lp, !IsWindowUnicode(hwnd));
            return 0;
        }
    }
    return CallWindowProcA(g_origWndProc, hwnd, msg, wp, lp);
}

// ---------------- запуск ----------------
template <typename T>
bool Hook(uintptr_t addr, void* detour, T& orig, const char* name) {
    void* target = reinterpret_cast<void*>(addr);
    if (MH_CreateHook(target, detour, reinterpret_cast<void**>(&orig)) != MH_OK || MH_EnableHook(target) != MH_OK) {
        Log("hook %s failed", name);
        return false;
    }
    return true;
}

DWORD WINAPI Start(LPVOID) {
    // папка игры = папка gta_sa.exe
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring gta = exe;
    gta = gta.substr(0, gta.find_last_of(L"\\/"));
    g_dir = gta + L"\\bastion_ui";
    LogOpen(g_dir + L"\\bastion_ui.log");
    Log("Bastion UI start");

    // libcef.dll и chrome_elf.dll — из папки bastion_ui (libcef подключён с отложенной загрузкой)
    if (!LoadLibraryExW((g_dir + L"\\libcef.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)) {
        Log("нет bastion_ui\\libcef.dll (ошибка %lu): HTML-окна выключены", GetLastError());
        return 0;
    }

    for (int i = 0; i < 1200 && !GetModuleHandleW(L"samp.dll"); i++) Sleep(100);
    if (!samp::Detect()) {
        Log("samp.dll не найден или версия не поддерживается (entry 0x%X)", samp::EntryPoint());
        return 0;
    }
    Log("SA-MP %s", samp::Get().name);

    for (int i = 0; i < 1200 && (!*kDevice || !*kWindow); i++) Sleep(100);
    if (!*kDevice || !*kWindow) {
        Log("нет устройства D3D9");
        return 0;
    }
    g_hwnd = *kWindow;

    if (MH_Initialize() != MH_OK) {
        Log("MH_Initialize failed");
        return 0;
    }
    void** vt = *reinterpret_cast<void***>(*kDevice);
    bool ok = Hook(reinterpret_cast<uintptr_t>(vt[17]), reinterpret_cast<void*>(&HookPresent), g_origPresent, "Present") &&
              Hook(reinterpret_cast<uintptr_t>(vt[16]), reinterpret_cast<void*>(&HookReset), g_origReset, "Reset");
    if (!ok) return 0;
    // диалоги перехватываем только когда есть, чем рисовать
    Hook(samp::Base() + samp::Get().dialogShow, reinterpret_cast<void*>(&HookShow), g_origShow, "CDialog::Show");
    Hook(samp::Base() + samp::Get().dialogHide, reinterpret_cast<void*>(&HookHide), g_origHide, "CDialog::Hide");
    g_origWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&HookWndProc)));
    if (!gtahud::Install()) Log("стандартный HUD GTA не прячем");
    Log("hooks ready");
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(mod);
        HANDLE t = CreateThread(nullptr, 0, Start, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
