#pragma once
#include <string>
#include <cstdint>
#include <cstdio>

namespace utils {
    std::string Deaccent(const std::string& str);
    uint32_t Fnv1aHash(const std::string& str);
    FILE* OpenFilePortable(const std::string& path, const char* mode);
}
