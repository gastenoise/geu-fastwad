#include "utils.hpp"
#include <cstring>
#include <algorithm>
#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace utils {

std::string Deaccent(const std::string& str) {
    struct Mapping { const char* from; const char* to; };
    static const Mapping mappings[] = {
        {"á", "a"}, {"à", "a"}, {"â", "a"}, {"ä", "a"}, {"ã", "a"}, {"å", "a"},
        {"é", "e"}, {"è", "e"}, {"ê", "e"}, {"ë", "e"},
        {"í", "i"}, {"ì", "i"}, {"î", "i"}, {"ï", "i"},
        {"ó", "o"}, {"ò", "o"}, {"ô", "o"}, {"ö", "o"}, {"õ", "o"},
        {"ú", "u"}, {"ù", "u"}, {"û", "u"}, {"ü", "u"},
        {"ñ", "n"}, {"ç", "c"}, {"ÿ", "y"},
        {"Á", "a"}, {"À", "a"}, {"Â", "a"}, {"Ä", "a"}, {"Ã", "a"}, {"Å", "a"},
        {"É", "e"}, {"È", "e"}, {"Ê", "e"}, {"Ë", "e"},
        {"Í", "i"}, {"Ì", "i"}, {"Î", "i"}, {"Ï", "i"},
        {"Ó", "o"}, {"Ò", "o"}, {"Ô", "o"}, {"Ö", "o"}, {"Õ", "o"},
        {"Ú", "u"}, {"Ù", "u"}, {"Û", "u"}, {"Ü", "u"},
        {"Ñ", "n"}, {"Ç", "c"}
    };

    std::string result;
    for (size_t i = 0; i < str.length(); ) {
        bool found = false;
        for (const auto& m : mappings) {
            size_t len = std::strlen(m.from);
            if (str.compare(i, len, m.from) == 0) {
                result += m.to;
                i += len;
                found = true;
                break;
            }
        }
        if (!found) {
            result += str[i];
            i++;
        }
    }
    return result;
}

uint32_t Fnv1aHash(const std::string& str) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : str) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

FILE* OpenFilePortable(const std::string& path, const char* mode) {
#if defined(_WIN32)
    std::wstring wpath = fs::path(path).wstring();
    std::wstring wmode;
    while (*mode) wmode += (wchar_t)*mode++;
    return _wfopen(wpath.c_str(), wmode.c_str());
#else
    return fopen(path.c_str(), mode);
#endif
}

} // namespace utils
