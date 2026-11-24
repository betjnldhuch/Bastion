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
//  Кислород под водой, значок оружия и патроны тоже рисует страница
//  (BastionUI.breath, BastionUI.weapon, gtahud.cpp).
//  Диалог "[bastion:rec]" — команда /rec (accounts.inc): после кика сервером
//  переводим SA-MP в «ждёт подключения», и он сам заходит заново.
//  Диалог "[bastion:photo]" (текст — номер скина) — съёмка на паспорт в мэрии
//  (docs.inc): со следующего кадра вырезаем лицо по центру экрана и сохраняем
//  bastion_ui\ui\doc\skins\<скин>.bmp, его показывает паспорт.
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
int g_dialogStyle = 0;
int g_photoSkin = -1;    // "[bastion:photo]": снять кадр для паспорта на следующем Present    // стиль открытого диалога: у не-списков пункт в ответе всегда -1
bool g_helloSent = false;
bool g_hudOn = false;    // сервер прислал свою панель HUD
int g_breathSent = -1;   // что последним ушло в BastionUI.breath
std::string g_weaponSent = "-1";   // что последним ушло в BastionUI.weapon ("-1" — значка нет)
bool g_recArmed = false; // /rec: ждём кика, чтобы подключиться заново
DWORD g_recSince = 0;
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
    if (serverside && caption && strcmp(caption, "[bastion:rec]") == 0) {
        // Проверяем смещение: сейчас игрок точно в игре. Иначе /rec не трогаем.
        int st = samp::GetGameState();
        if (st == samp::kStateConnected) {
            g_recArmed = true;
            g_recSince = GetTickCount();
            Log("rec: ждём кика");
        } else {
            Log("rec: неожиданное состояние %d, переподключение выключено", st);
        }
        return;
    }
    if (serverside && caption && strcmp(caption, "[bastion:photo]") == 0) {
        int skin = txt ? atoi(txt) : -1;
        if (skin >= 0 && skin < 20000) g_photoSkin = skin;
        return;
    }
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
        samp::SendCommand("/__bui 4");  // сервер включит HTML-окна (ui.inc) и перешлёт окно входа;
                                          // версия: 2 — скрытая панель "[hud]" (hud.inc), 3 — /rec, 4 — фото на паспорт
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
    g_dialogStyle = type;
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

// ---------------- фото на паспорт ----------------
// Кадр игры (до наших окон) -> центр экрана 3:4 -> 300x400 -> BMP 24 бит.
// Камеру сервер ставит прямо перед лицом сидящего игрока (docs.inc).
bool SavePhoto(IDirect3DDevice9* dev, int skin) {
    const int W = 300, H = 400;
    IDirect3DSurface9 *bb = nullptr, *rt = nullptr, *sys = nullptr;
    bool ok = false;
    do {
        if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) break;
        D3DSURFACE_DESC d;
        bb->GetDesc(&d);
        if (FAILED(dev->CreateRenderTarget(W, H, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, nullptr))) break;
        LONG h = static_cast<LONG>(d.Height * 0.62), w = h * 3 / 4;
        RECT src = {static_cast<LONG>(d.Width / 2) - w / 2, static_cast<LONG>(d.Height * 0.16), 0, 0};
        src.right = src.left + w;
        src.bottom = src.top + h;
        if (FAILED(dev->StretchRect(bb, &src, rt, nullptr, D3DTEXF_LINEAR))) break;
        if (FAILED(dev->CreateOffscreenPlainSurface(W, H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, nullptr))) break;
        if (FAILED(dev->GetRenderTargetData(rt, sys))) break;
        D3DLOCKED_RECT lr;
        if (FAILED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) break;
        std::wstring dir = g_dir + L"\\ui\\doc\\skins";
        CreateDirectoryW((g_dir + L"\\ui\\doc").c_str(), nullptr);
        CreateDirectoryW(dir.c_str(), nullptr);
        std::wstring path = dir + L"\\" + std::to_wstring(skin) + L".bmp";
        FILE* f = _wfopen(path.c_str(), L"wb");
        if (f) {
            const int row = W * 3, size = row * H;   // 900 байт — кратно 4, без выравнивания
            BITMAPFILEHEADER fh = {};
            BITMAPINFOHEADER ih = {};
            fh.bfType = 0x4D42;
            fh.bfOffBits = sizeof fh + sizeof ih;
            fh.bfSize = fh.bfOffBits + size;
            ih.biSize = sizeof ih;
            ih.biWidth = W;
            ih.biHeight = H;   // снизу вверх
            ih.biPlanes = 1;
            ih.biBitCount = 24;
            ih.biCompression = BI_RGB;
            ih.biSizeImage = size;
            fwrite(&fh, sizeof fh, 1, f);
            fwrite(&ih, sizeof ih, 1, f);
            std::string line(row, '\0');
            for (int y = H - 1; y >= 0; --y) {
                const unsigned char* px = static_cast<const unsigned char*>(lr.pBits) + y * lr.Pitch;
                for (int x = 0; x < W; ++x) memcpy(&line[x * 3], px + x * 4, 3);   // BGRX -> BGR
                fwrite(line.data(), 1, row, f);
            }
            fclose(f);
            ok = true;
        }
        sys->UnlockRect();
    } while (false);
    if (sys) sys->Release();
    if (rt) rt->Release();
    if (bb) bb->Release();
    Log(ok ? "фото на паспорт: скин %d сохранён" : "фото на паспорт: скин %d не снялся", skin);
    return ok;
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
        // у INPUT/PASSWORD/MSGBOX обычный клиент SA-MP шлёт пункт -1; open.mp сверяет ответ, другой пункт мог не пройти
        bool list = g_dialogStyle == 2 || g_dialogStyle == 4 || g_dialogStyle == 5;
        int item = list ? resp.listItem : -1;
        CloseWindowUi(false);
        if (!samp::SendDialogResponse(id, resp.button, item, text::Utf8ToCp1251(resp.text)))
            Log("DialogResponse %d не отправлен", id);
        else
            Log("ответ окну %d: кнопка %d, пункт %d, текст %u байт", id, resp.button, item, static_cast<unsigned>(resp.text.size()));
    }
    if (g_recArmed) {
        int st = samp::GetGameState();
        if (st != samp::kStateConnected && st != samp::kStateWaitConnect && st != samp::kStateConnecting &&
            st != samp::kStateAwaitJoin && st != -1) {
            g_recArmed = false;
            g_hudOn = false;
            gtahud::SetReplaced(false);
            CloseWindowUi(true);
            samp::SetGameState(samp::kStateWaitConnect);  // SA-MP сам подключится заново через ~3 с
            Log("rec: связь закрыта (состояние %d), подключаемся заново", st);
        } else if (GetTickCount() - g_recSince > 30000) {
            g_recArmed = false;
            Log("rec: кика не было, отбой");
        }
    }
    int breath = g_hudOn ? gtahud::Breath() : -1;
    if (breath != g_breathSent) {
        g_breathSent = breath;
        browser::ExecJs("window.BastionUI && BastionUI.breath && BastionUI.breath(" + std::to_string(breath) + ")");
    }
    int wt = -1, wc = 0, wa = 0;
    if (!g_hudOn || !gtahud::Weapon(wt, wc, wa)) wt = -1;
    std::string weapon = wt < 0 ? "-1" : std::to_string(wt) + "," + std::to_string(wc) + "," + std::to_string(wa);
    if (weapon != g_weaponSent) {
        g_weaponSent = weapon;
        browser::ExecJs("window.BastionUI && BastionUI.weapon && BastionUI.weapon(" + weapon + ")");
    }
    if (g_photoSkin >= 0) {
        SavePhoto(dev, g_photoSkin);   // до overlay: на фото только игра
        g_photoSkin = -1;
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
