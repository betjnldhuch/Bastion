#include "text.h"
#include <windows.h>

namespace text {

static std::wstring ToWide(const std::string& s, UINT cp) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(cp, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(cp, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

static std::string FromWide(const std::wstring& w, UINT cp) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(cp, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(cp, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::string Cp1251ToUtf8(const char* s) { return s ? FromWide(ToWide(s, 1251), CP_UTF8) : std::string(); }
std::string Utf8ToCp1251(const std::string& s) { return FromWide(ToWide(s, CP_UTF8), 1251); }
std::wstring Utf8ToWide(const std::string& s) { return ToWide(s, CP_UTF8); }
std::string WideToUtf8(const std::wstring& s) { return FromWide(s, CP_UTF8); }

std::string JsonQuote(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20) {
                char b[8];
                wsprintfA(b, "\\u%04x", c);
                o += b;
            } else {
                o += (char)c;
            }
        }
    }
    // U+2028/2029 ломают строковые литералы JavaScript
    for (size_t p; (p = o.find("\xE2\x80\xA8")) != std::string::npos;) o.replace(p, 3, "\\u2028");
    for (size_t p; (p = o.find("\xE2\x80\xA9")) != std::string::npos;) o.replace(p, 3, "\\u2029");
    return o + "\"";
}

}  // namespace text
