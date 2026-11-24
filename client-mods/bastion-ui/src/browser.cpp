#include "browser.h"

#include <atomic>
#include <cctype>
#include <cstring>
#include <deque>
#include <mutex>

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"

#include "log.h"

namespace browser {
namespace {

std::mutex g_mx;                 // кадр, очереди
std::vector<uint8_t> g_frame;
int g_frameW = 0, g_frameH = 0;
bool g_frameDirty = false;
std::deque<Response> g_responses;
std::deque<std::string> g_pendingJs;

std::atomic<int> g_viewW{1280}, g_viewH{720};
std::atomic<bool> g_started{false}, g_loaded{false};
CefRefPtr<CefBrowser> g_browser;  // под g_mx

CefRefPtr<CefBrowserHost> Host() {
    std::lock_guard<std::mutex> lk(g_mx);
    return g_browser ? g_browser->GetHost() : nullptr;
}

// Игра запущена через Wine (Mac / Linux): у Chromium там нет песочницы и части API Windows
bool UnderWine() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

class App : public CefApp, public CefBrowserProcessHandler {
public:
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
    void OnBeforeCommandLineProcessing(const CefString& processType, CefRefPtr<CefCommandLine> cmd) override {
        if (!processType.empty()) return;
        // Рисуем программно в текстуру: GPU-процесс не нужен, меньше памяти в 32-битной игре
        cmd->AppendSwitch("disable-gpu");
        cmd->AppendSwitch("disable-gpu-compositing");
        cmd->AppendSwitch("disable-extensions");
        cmd->AppendSwitch("disable-spell-checking");
        cmd->AppendSwitch("no-proxy-server");
        cmd->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");
        if (UnderWine()) {
            cmd->AppendSwitch("no-sandbox");
            cmd->AppendSwitch("in-process-gpu");
            cmd->AppendSwitch("disable-gpu-sandbox");
            cmd->AppendSwitch("disable-direct-composition");
            cmd->AppendSwitchWithValue("disable-features",
                "RendererCodeIntegrity,WinUseBrowserSpellChecker,CalculateNativeWinOcclusion");
        }
    }

private:
    IMPLEMENT_REFCOUNTING(App);
};

class Client : public CefClient, public CefRenderHandler, public CefLoadHandler, public CefLifeSpanHandler {
public:
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }

    // --- окно ---
    void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
        rect = CefRect(0, 0, (std::max)(1, g_viewW.load()), (std::max)(1, g_viewH.load()));
    }

    void OnPaint(CefRefPtr<CefBrowser>, PaintElementType type, const RectList&, const void* buffer, int width,
                 int height) override {
        if (type != PET_VIEW) return;
        std::lock_guard<std::mutex> lk(g_mx);
        const auto* p = static_cast<const uint8_t*>(buffer);
        g_frame.assign(p, p + static_cast<size_t>(width) * height * 4);
        g_frameW = width;
        g_frameH = height;
        g_frameDirty = true;
    }

    // --- жизнь браузера ---
    void OnAfterCreated(CefRefPtr<CefBrowser> b) override {
        {
            std::lock_guard<std::mutex> lk(g_mx);
            g_browser = b;
        }
        Log("browser created");
    }

    void OnBeforeClose(CefRefPtr<CefBrowser>) override {
        std::lock_guard<std::mutex> lk(g_mx);
        g_browser = nullptr;
    }

    void OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, int status) override {
        if (!frame->IsMain()) return;
        Log("page loaded, status %d", status);
        std::deque<std::string> js;
        {
            std::lock_guard<std::mutex> lk(g_mx);
            js.swap(g_pendingJs);
            g_loaded = true;
        }
        for (auto& s : js) frame->ExecuteJavaScript(s, frame->GetURL(), 0);
    }

    void OnLoadError(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, ErrorCode code, const CefString& text,
                     const CefString& url) override {
        if (frame->IsMain())
            Log("page load error %d: %s (%s)", static_cast<int>(code), text.ToString().c_str(), url.ToString().c_str());
    }

    // --- ответ окна: BastionNative.respond(button, listitem, text) из bastion_cef.exe ---
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefProcessId,
                                  CefRefPtr<CefProcessMessage> msg) override {
        if (msg->GetName() != "bastion_respond") return false;
        CefRefPtr<CefListValue> a = msg->GetArgumentList();
        Response r;
        r.button = a->GetInt(0);
        r.listItem = a->GetInt(1);
        r.text = a->GetString(2).ToString();
        std::lock_guard<std::mutex> lk(g_mx);
        g_responses.push_back(r);
        return true;
    }

private:
    IMPLEMENT_REFCOUNTING(Client);
};

CefRefPtr<Client> g_client;

std::string FileUrl(const std::wstring& path) {
    std::string utf8;
    {
        int n = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
        utf8.resize(n > 0 ? n - 1 : 0);
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, utf8.data(), n, nullptr, nullptr);
    }
    std::string url = "file:///";
    for (unsigned char c : utf8) {
        if (c == '\\') url += '/';
        else if (isalnum(c) || strchr("/:._-~", c)) url += static_cast<char>(c);
        else {
            char b[4];
            wsprintfA(b, "%%%02X", c);
            url += b;
        }
    }
    return url;
}

int Modifiers() {
    int m = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) m |= EVENTFLAG_SHIFT_DOWN;
    if (GetKeyState(VK_CONTROL) & 0x8000) m |= EVENTFLAG_CONTROL_DOWN;
    if (GetKeyState(VK_MENU) & 0x8000) m |= EVENTFLAG_ALT_DOWN;
    if (GetKeyState(VK_LBUTTON) & 0x8000) m |= EVENTFLAG_LEFT_MOUSE_BUTTON;
    if (GetKeyState(VK_RBUTTON) & 0x8000) m |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
    return m;
}

}  // namespace

bool Init(HWND gameWindow, const std::wstring& dir) {
    if (g_started) return true;
    if (UnderWine()) Log("Wine detected: CEF in compatibility mode");
    CefMainArgs args(GetModuleHandleW(nullptr));
    CefSettings s;
    s.no_sandbox = true;
    s.multi_threaded_message_loop = true;      // свой UI-поток CEF, игре не нужно крутить цикл
    s.windowless_rendering_enabled = true;
    s.log_severity = LOGSEVERITY_WARNING;
    s.background_color = 0;
    CefString(&s.browser_subprocess_path) = dir + L"\\bastion_cef.exe";
    CefString(&s.resources_dir_path) = dir;
    CefString(&s.locales_dir_path) = dir + L"\\locales";
    CefString(&s.root_cache_path) = dir + L"\\cache";
    CefString(&s.cache_path) = dir + L"\\cache\\default";
    CefString(&s.log_file) = dir + L"\\cef.log";
    CefString(&s.locale) = "ru";

    if (!CefInitialize(args, s, new App(), nullptr)) {
        Log("CefInitialize failed (code %d)", CefGetExitCode());
        return false;
    }
    g_started = true;

    CefWindowInfo wi;
    wi.SetAsWindowless(gameWindow);
    CefBrowserSettings bs;
    bs.windowless_frame_rate = 60;
    bs.background_color = CefColorSetARGB(0, 0, 0, 0);
    g_client = new Client();
    std::string url = FileUrl(dir + L"\\ui\\index.html");
    Log("loading %s", url.c_str());
    if (!CefBrowserHost::CreateBrowser(wi, g_client, url, bs, nullptr, nullptr)) {
        Log("CreateBrowser failed");
        return false;
    }
    return true;
}

bool Started() { return g_started; }
bool PageLoaded() { return g_loaded; }

void Resize(int w, int h) {
    if (w == g_viewW && h == g_viewH) return;
    g_viewW = w;
    g_viewH = h;
    if (auto host = Host()) host->WasResized();
}

void ExecJs(const std::string& js) {
    CefRefPtr<CefBrowser> b;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        if (!g_loaded || !g_browser) {
            g_pendingJs.push_back(js);
            return;
        }
        b = g_browser;
    }
    CefRefPtr<CefFrame> f = b->GetMainFrame();
    f->ExecuteJavaScript(js, f->GetURL(), 0);
}

void Focus(bool on) {
    if (auto host = Host()) host->SetFocus(on);
}

void MouseMove(int x, int y) {
    auto host = Host();
    if (!host) return;
    CefMouseEvent e;
    e.x = x;
    e.y = y;
    e.modifiers = Modifiers();
    host->SendMouseMoveEvent(e, false);
}

void MouseButton(int button, bool up, int x, int y, int clicks) {
    auto host = Host();
    if (!host) return;
    CefMouseEvent e;
    e.x = x;
    e.y = y;
    e.modifiers = Modifiers();
    host->SendMouseClickEvent(e, button == 1 ? MBT_RIGHT : MBT_LEFT, up, clicks);
}

void MouseWheel(int x, int y, int delta) {
    auto host = Host();
    if (!host) return;
    CefMouseEvent e;
    e.x = x;
    e.y = y;
    e.modifiers = Modifiers();
    host->SendMouseWheelEvent(e, 0, delta);
}

void KeyMessage(UINT msg, WPARAM wp, LPARAM lp, bool ansiWindow) {
    auto host = Host();
    if (!host) return;
    CefKeyEvent e;
    e.windows_key_code = static_cast<int>(wp);
    e.native_key_code = static_cast<int>(lp);
    e.is_system_key = msg == WM_SYSCHAR || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP;
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) e.type = KEYEVENT_RAWKEYDOWN;
    else if (msg == WM_KEYUP || msg == WM_SYSKEYUP) e.type = KEYEVENT_KEYUP;
    else {
        e.type = KEYEVENT_CHAR;
        // окно GTA ANSI: русская буква приходит байтом Windows-1251
        if (ansiWindow && wp >= 0x80 && wp <= 0xFF) {
            char c = static_cast<char>(wp);
            wchar_t w = 0;
            MultiByteToWideChar(CP_ACP, 0, &c, 1, &w, 1);
            e.windows_key_code = w;
        }
        e.character = static_cast<char16_t>(e.windows_key_code);
        e.unmodified_character = e.character;
    }
    e.modifiers = Modifiers();
    host->SendKeyEvent(e);
}

bool TakeFrame(std::vector<uint8_t>& out, int& w, int& h) {
    std::lock_guard<std::mutex> lk(g_mx);
    if (!g_frameDirty) return false;
    out.swap(g_frame);
    w = g_frameW;
    h = g_frameH;
    g_frameDirty = false;
    return true;
}

bool PopResponse(Response& r) {
    std::lock_guard<std::mutex> lk(g_mx);
    if (g_responses.empty()) return false;
    r = g_responses.front();
    g_responses.pop_front();
    return true;
}

}  // namespace browser
