#pragma once
#include <string>

// Журнал bastion_ui\bastion_ui.log (перезаписывается при каждом запуске игры)
void LogOpen(const std::wstring& path);
void Log(const char* fmt, ...);
