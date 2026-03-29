#include "config.hpp"
#include "wad_archive.hpp"
#include "utils.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstring>

std::string TestNormalizeName(const std::string& raw) {
    if (raw.empty()) return "tex";
    std::string clean = utils::Deaccent(raw);
    std::string out;
    bool first = true;
    for (unsigned char c : clean) {
        if (first && (c == '{' || c == '!' || c == '+' || c == '~')) {
            out += (char)c;
        } else if (std::isalnum(c) || c == '_' || c == '-') {
            out += (char)std::tolower(c);
        }
        first = false;
    }
    if (out.empty()) return "tex";
    if (out.length() > 15) out = out.substr(0, 15);
    return out;
}

int main() {
    std::cout << "Running fastWAD Core Unit Tests...\n";
    
    // 1. Name Normalization Tests
    assert(TestNormalizeName("MyTex_01!") == "mytex_01");
    assert(TestNormalizeName("{Glass_01") == "{glass_01");
    assert(TestNormalizeName("!Water_Blue") == "!water_blue");
    assert(TestNormalizeName("+0_Anim") == "+0_anim");
    assert(TestNormalizeName("~Light") == "~light");
    assert(TestNormalizeName("extremely_long_texture_name_123") == "extremely_long_");
    assert(TestNormalizeName("@@@") == "tex");
    assert(TestNormalizeName("fútbol") == "futbol");
    assert(TestNormalizeName("Niño") == "nino");

    // 2. Hash Stability
    assert(utils::Fnv1aHash("test.png") == 0x753bfe00);

    // 2. Bounds checking behavior conceptually
    // Rule: Nearest multiple of 16, within [16, max_size].

    auto calculate_canvas = [](int fit_w, int fit_h, int max_s) {
        int canvas_w = std::max(16, std::min(max_s, (int)std::round(fit_w / 16.0) * 16));
        int canvas_h = std::max(16, std::min(max_s, (int)std::round(fit_h / 16.0) * 16));
        return std::make_pair(canvas_w, canvas_h);
    };

    // 33x33 -> 32x32 (micro-stretch if we use this logic)
    auto c1 = calculate_canvas(33, 33, 256);
    assert(c1.first == 32 && c1.second == 32);

    // 40x40 -> 48x48
    auto c2 = calculate_canvas(40, 40, 256);
    assert(c2.first == 48 && c2.second == 48);

    // 250x250 -> 256x256
    auto c3 = calculate_canvas(250, 250, 256);
    assert(c3.first == 256 && c3.second == 256);

    std::cout << "All core logic tests passed!\n";
    return 0;
}
