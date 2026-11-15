#pragma once
#include <string>

namespace text {
// Строки SA-MP приходят и уходят в кодировке Windows-1251
std::string Cp1251ToUtf8(const char* s);
std::string Utf8ToCp1251(const std::string& s);
std::wstring Utf8ToWide(const std::string& s);
std::string WideToUtf8(const std::wstring& s);
// Строка JSON в кавычках (для передачи в JavaScript)
std::string JsonQuote(const std::string& utf8);
}
