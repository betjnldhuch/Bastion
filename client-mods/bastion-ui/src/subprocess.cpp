// bastion_cef.exe — вспомогательные процессы Chromium (рендер, сеть, утилиты).
// В процессе рендера добавляет странице объект BastionNative.respond(button, listitem, text):
// ответ уходит в gta_sa.exe сообщением "bastion_respond".
#include <windows.h>

#include "include/cef_app.h"
#include "include/cef_v8.h"

namespace {

class Respond : public CefV8Handler {
public:
    bool Execute(const CefString& name, CefRefPtr<CefV8Value>, const CefV8ValueList& args,
                 CefRefPtr<CefV8Value>& retval, CefString& exception) override {
        if (name != "respond") return false;
        CefRefPtr<CefV8Context> ctx = CefV8Context::GetCurrentContext();
        CefRefPtr<CefFrame> frame = ctx ? ctx->GetFrame() : nullptr;
        if (!frame) return true;
        int button = args.size() > 0 && args[0]->IsInt() ? args[0]->GetIntValue()
                     : args.size() > 0 && args[0]->IsDouble() ? static_cast<int>(args[0]->GetDoubleValue()) : 0;
        int item = args.size() > 1 && args[1]->IsInt() ? args[1]->GetIntValue()
                   : args.size() > 1 && args[1]->IsDouble() ? static_cast<int>(args[1]->GetDoubleValue()) : -1;
        CefString txt = args.size() > 2 && args[2]->IsString() ? args[2]->GetStringValue() : CefString();
        CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("bastion_respond");
        CefRefPtr<CefListValue> a = msg->GetArgumentList();
        a->SetInt(0, button);
        a->SetInt(1, item);
        a->SetString(2, txt);
        frame->SendProcessMessage(PID_BROWSER, msg);
        retval = CefV8Value::CreateBool(true);
        return true;
    }

private:
    IMPLEMENT_REFCOUNTING(Respond);
};

class App : public CefApp, public CefRenderProcessHandler {
public:
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

    void OnContextCreated(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> ctx) override {
        if (!frame->IsMain()) return;
        CefRefPtr<CefV8Value> native = CefV8Value::CreateObject(nullptr, nullptr);
        native->SetValue("respond", CefV8Value::CreateFunction("respond", new Respond()), V8_PROPERTY_ATTRIBUTE_READONLY);
        native->SetValue("platform", CefV8Value::CreateString("pc"), V8_PROPERTY_ATTRIBUTE_READONLY);
        ctx->GetGlobal()->SetValue("BastionNative", native, V8_PROPERTY_ATTRIBUTE_READONLY);
    }

private:
    IMPLEMENT_REFCOUNTING(App);
};

}  // namespace

int APIENTRY wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
    CefMainArgs args(inst);
    return CefExecuteProcess(args, new App(), nullptr);
}
