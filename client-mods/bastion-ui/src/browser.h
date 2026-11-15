#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

// Браузер CEF без окна (off-screen rendering) внутри gta_sa.exe.
namespace browser {

struct Response {
    int button = 0;
    int listItem = -1;
    std::string text;  // UTF-8
};

// Вызывать из главного потока игры (из Present). dir — папка bastion_ui.
bool Init(HWND gameWindow, const std::wstring& dir);
bool Started();
bool PageLoaded();

void Resize(int width, int height);
void ExecJs(const std::string& utf8);  // до загрузки страницы копится в очереди
void Focus(bool on);

void MouseMove(int x, int y);
void MouseButton(int button, bool up, int x, int y, int clicks);  // 0 — левая, 1 — правая
void MouseWheel(int x, int y, int delta);
void KeyMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool ansiWindow);

// Новый кадр (BGRA, сверху вниз). false — с прошлого раза не менялся.
bool TakeFrame(std::vector<uint8_t>& out, int& width, int& height);
bool PopResponse(Response& r);

}  // namespace browser
