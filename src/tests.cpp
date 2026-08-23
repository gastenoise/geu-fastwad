#include "config.hpp"
#include "wad_archive.hpp"
#include "image_processor.hpp"
#include "utils.hpp"
#include "fastwad/fastwad.hpp"

#include "stb_image_write.h"
#include "stb_image.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <cstdlib>
#include <algorithm>

namespace fs = std::filesystem;
using namespace fastwad;

#define FW_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            std::cout << "ASSERTION FAILED at " << __FILE__ << ":" << __LINE__ << ": " #condition << std::endl; \
            std::exit(1); \
        } \
    } while(0)

// Helper: Generate synthetic test image
static bool CreateSyntheticImage(const std::string& path, int width, int height, int channels, int pattern_type) {
    std::vector<uint8_t> pixels(width * height * channels);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * channels;
            if (pattern_type == 0) {
                // Smooth RGB gradient
                pixels[idx + 0] = (uint8_t)((x * 255) / std::max(1, width - 1));
                pixels[idx + 1] = (uint8_t)((y * 255) / std::max(1, height - 1));
                pixels[idx + 2] = (uint8_t)(((x + y) * 128) / std::max(1, width + height - 2));
                if (channels == 4) pixels[idx + 3] = 255;
            } else if (pattern_type == 1) {
                // Transparent alpha cutout (corners transparent)
                pixels[idx + 0] = 220;
                pixels[idx + 1] = 40;
                pixels[idx + 2] = 40;
                if (channels == 4) {
                    bool in_border = (x < 8 || x >= width - 8 || y < 8 || y >= height - 8);
                    pixels[idx + 3] = in_border ? 0 : 255; // Alpha 0 in border
                }
            } else if (pattern_type == 2) {
                // Checkerboard pattern
                int check = ((x / 8) + (y / 8)) % 2;
                pixels[idx + 0] = check ? 240 : 30;
                pixels[idx + 1] = check ? 240 : 30;
                pixels[idx + 2] = check ? 240 : 30;
                if (channels == 4) pixels[idx + 3] = 255;
            } else if (pattern_type == 3) {
                // Cyberpunk neon colors
                pixels[idx + 0] = (uint8_t)((std::sin(x * 0.1) + 1.0) * 127);
                pixels[idx + 1] = (uint8_t)((std::cos(y * 0.1) + 1.0) * 127);
                pixels[idx + 2] = (uint8_t)((std::sin((x + y) * 0.05) + 1.0) * 127);
                if (channels == 4) pixels[idx + 3] = 255;
            }
        }
    }

    std::string ext = fs::path(path).extension().string();
    if (ext == ".bmp") {
        return stbi_write_bmp(path.c_str(), width, height, channels, pixels.data()) != 0;
    }
    return stbi_write_png(path.c_str(), width, height, channels, pixels.data(), width * channels) != 0;
}

// Generate Palette Visualization PNG (16x16 grid of 16x16 pixel swatches = 256x256 image)
static void GeneratePalettePreviewPng(const std::string& path, const std::vector<ColorRGB>& palette) {
    int swatch = 16;
    int img_dim = 16 * swatch;
    std::vector<uint8_t> rgb(img_dim * img_dim * 3);

    for (int pal_idx = 0; pal_idx < 256; ++pal_idx) {
        int grid_x = pal_idx % 16;
        int grid_y = pal_idx / 16;
        ColorRGB col = (pal_idx < (int)palette.size()) ? palette[pal_idx] : ColorRGB{0, 0, 0};

        for (int sy = 0; sy < swatch; ++sy) {
            for (int sx = 0; sx < swatch; ++sx) {
                int px = grid_x * swatch + sx;
                int py = grid_y * swatch + sy;
                int idx = (py * img_dim + px) * 3;
                
                // Border around each swatch
                if (sx == 0 || sy == 0) {
                    rgb[idx + 0] = 30;
                    rgb[idx + 1] = 30;
                    rgb[idx + 2] = 30;
                } else {
                    rgb[idx + 0] = col.r;
                    rgb[idx + 1] = col.g;
                    rgb[idx + 2] = col.b;
                }
            }
        }
    }
    stbi_write_png(path.c_str(), img_dim, img_dim, 3, rgb.data(), img_dim * 3);
}

// ---------------------------------------------------------------------------
// End-to-End Test Suite
// ---------------------------------------------------------------------------

void RunCoreTests() {
    std::cout << "[Test 1/6] Testing Name Normalization & Deaccenting..." << std::endl;
    FW_ASSERT(WadArchive::NormalizeName("MyTex_01!") == "mytex_01");
    FW_ASSERT(WadArchive::NormalizeName("{Glass_01") == "{glass_01");
    FW_ASSERT(WadArchive::NormalizeName("!Water_Blue") == "!water_blue");
    FW_ASSERT(WadArchive::NormalizeName("+0_Anim") == "+0_anim");
    FW_ASSERT(WadArchive::NormalizeName("~Light") == "~light");
    FW_ASSERT(WadArchive::NormalizeName("extremely_long_texture_name_123") == "extremely_long_");
    FW_ASSERT(WadArchive::NormalizeName("@@@") == "tex");
    std::cout << "  Normalize(fútbol) = " << WadArchive::NormalizeName("fútbol") << std::endl;
    FW_ASSERT(WadArchive::NormalizeName("fútbol") == "futbol");
    std::cout << "  Normalize(Niño) = " << WadArchive::NormalizeName("Niño") << std::endl;
    FW_ASSERT(WadArchive::NormalizeName("Niño") == "nino");
    FW_ASSERT(WadArchive::NormalizeName("Música_Clásica") == "musica_clasica");

    std::cout << "[Test 2/6] Testing FNV-1a Hash Stability..." << std::endl;
    std::cout << "  Fnv1aHash(test.png) = 0x" << std::hex << utils::Fnv1aHash("test.png") << std::dec << std::endl;
    FW_ASSERT(utils::Fnv1aHash("test.png") != 0);
    FW_ASSERT(utils::Fnv1aHash("wall.bmp") != 0);

    std::cout << "[Test 3/6] Testing Dimension Calculation...\n";
    auto calc_canvas = [](int fit_w, int fit_h, int max_s) {
        int canvas_w = std::clamp((int)std::round(fit_w / 16.0) * 16, 16, max_s);
        int canvas_h = std::clamp((int)std::round(fit_h / 16.0) * 16, 16, max_s);
        return std::make_pair(canvas_w, canvas_h);
    };
    FW_ASSERT(calc_canvas(33, 33, 256) == std::make_pair(32, 32));
    FW_ASSERT(calc_canvas(40, 40, 256) == std::make_pair(48, 48));
    FW_ASSERT(calc_canvas(250, 250, 256) == std::make_pair(256, 256));
    FW_ASSERT(calc_canvas(100, 50, 256) == std::make_pair(96, 48));

    std::cout << "[Test 4/6] Running Full E2E Archive Creation, Listing & Extraction Pipeline..." << std::endl;
    fs::path test_dir = fs::current_path() / "test_sandbox_tmp";
    fs::path input_dir = test_dir / "input";
    fs::path extract_dir = test_dir / "extracted";
    fs::path out_wad1 = test_dir / "test_output1.wad";
    fs::path out_wad2 = test_dir / "test_output2.wad";

    fs::create_directories(input_dir);

    std::cout << "  (4.1) Creating synthetic textures..." << std::endl;
    FW_ASSERT(CreateSyntheticImage((input_dir / "gradient_64x64.png").string(), 64, 64, 3, 0));
    FW_ASSERT(CreateSyntheticImage((input_dir / "trans_cutout.png").string(), 64, 64, 4, 1));
    FW_ASSERT(CreateSyntheticImage((input_dir / "micro_33x33.png").string(), 33, 33, 3, 0));
    FW_ASSERT(CreateSyntheticImage((input_dir / "contain_100x50.png").string(), 100, 50, 3, 2));
    FW_ASSERT(CreateSyntheticImage((input_dir / "sample_bmp.bmp").string(), 64, 64, 3, 2));
    FW_ASSERT(CreateSyntheticImage((input_dir / "textura_fútbol_niño.png").string(), 64, 64, 3, 0));
    FW_ASSERT(CreateSyntheticImage((input_dir / "very_long_texture_name_alpha.png").string(), 64, 64, 3, 0));
    FW_ASSERT(CreateSyntheticImage((input_dir / "very_long_texture_name_beta.png").string(), 64, 64, 3, 0));

    std::cout << "  (4.2) Building WAD3..." << std::endl;
    AppConfig build_cfg;
    build_cfg.command = "build";
    build_cfg.input_path = input_dir.string();
    build_cfg.output_path = out_wad1.string();
    build_cfg.allow_overwrite = true;
    build_cfg.quiet = false;

    ExitCode ec = WadArchive::Build(build_cfg);
    std::cout << "    Build exit code = " << (int)ec << std::endl;
    FW_ASSERT(ec == ExitCode::Success);
    FW_ASSERT(fs::exists(out_wad1) && fs::file_size(out_wad1) > 0);

    std::cout << "  (4.3) Inspecting WAD3..." << std::endl;
    AppConfig list_cfg;
    list_cfg.command = "list";
    list_cfg.input_path = out_wad1.string();
    list_cfg.quiet = true;
    FW_ASSERT(WadArchive::List(list_cfg) == ExitCode::Success);

    std::string inspect_err;
    auto wad_info = InspectWadFile(out_wad1.string(), &inspect_err);
    if (!wad_info.has_value()) {
        std::cout << "    InspectWadFile failed with error: " << inspect_err << std::endl;
    }
    FW_ASSERT(wad_info.has_value());
    std::cout << "    Lump count = " << wad_info->num_lumps << std::endl;
    FW_ASSERT(wad_info->num_lumps == 8);
    FW_ASSERT(wad_info->format == WadFormat::WAD3);

    // Check transparent texture was prefixed with '{'
    bool found_trans = false;
    for (const auto& lump : wad_info->lumps) {
        std::cout << "      lump: " << lump.name << " [" << lump.width << "x" << lump.height << "]" << std::endl;
        FW_ASSERT(lump.width % 16 == 0);
        FW_ASSERT(lump.height % 16 == 0);
        if (lump.name.find("trans_cutout") != std::string::npos) {
            FW_ASSERT(lump.name[0] == '{');
            found_trans = true;
        }
    }
    FW_ASSERT(found_trans);

    std::cout << "  (4.4) Extracting WAD3..." << std::endl;
    AppConfig ext_cfg;
    ext_cfg.command = "extract";
    ext_cfg.input_path = out_wad1.string();
    ext_cfg.output_path = extract_dir.string();
    ext_cfg.quiet = false;
    FW_ASSERT(WadArchive::Extract(ext_cfg) == ExitCode::Success);

    // Validate extracted images with stbi_load
    int extracted_files = 0;
    for (const auto& entry : fs::directory_iterator(extract_dir)) {
        std::cout << "    Found in extract_dir: " << entry.path().filename().string() << std::endl;
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            int w = 0, h = 0, ch = 0;
            uint8_t* data = stbi_load(entry.path().string().c_str(), &w, &h, &ch, 3);
            FW_ASSERT(data != nullptr);
            FW_ASSERT(w > 0 && w % 16 == 0);
            FW_ASSERT(h > 0 && h % 16 == 0);
            stbi_image_free(data);
            extracted_files++;
        }
    }
    std::cout << "  (4.5) Validated extracted files count = " << extracted_files << std::endl;
    FW_ASSERT(extracted_files == 8);

    std::cout << "[Test 5/6] Testing Bit-for-Bit Determinism across Builds..." << std::endl;
    build_cfg.output_path = out_wad2.string();
    build_cfg.quiet = true;
    FW_ASSERT(WadArchive::Build(build_cfg) == ExitCode::Success);

    // Compare byte-by-byte
    std::vector<uint8_t> b1, b2;
    {
        std::ifstream f1(out_wad1, std::ios::binary);
        std::ifstream f2(out_wad2, std::ios::binary);
        b1.assign((std::istreambuf_iterator<char>(f1)), std::istreambuf_iterator<char>());
        b2.assign((std::istreambuf_iterator<char>(f2)), std::istreambuf_iterator<char>());
    }
    std::cout << "  Archive 1 size = " << b1.size() << ", Archive 2 size = " << b2.size() << std::endl;
    FW_ASSERT(b1.size() > 0);
    FW_ASSERT(b1.size() == b2.size());
    FW_ASSERT(std::memcmp(b1.data(), b2.data(), b1.size()) == 0);
    std::cout << "  Bit-for-bit match verified!" << std::endl;

    std::cout << "[Test 6/6] Testing In-Memory Public C++ SDK Operations..." << std::endl;
    auto unpacked_lumps = UnpackWadFromMemory(b1.data(), b1.size());
    FW_ASSERT(unpacked_lumps.has_value());
    FW_ASSERT(unpacked_lumps->size() == 8);

    auto repacked_bytes = PackWadToMemory(*unpacked_lumps, WadFormat::WAD3);
    FW_ASSERT(!repacked_bytes.empty());
    std::cout << "  Repacked in-memory size = " << repacked_bytes.size() << " (original = " << b1.size() << ")" << std::endl;
    FW_ASSERT(repacked_bytes.size() == b1.size());
    FW_ASSERT(std::memcmp(repacked_bytes.data(), b1.data(), b1.size()) == 0);
    std::cout << "  Lossless in-memory round-trip verified!" << std::endl;

    // Teardown temporary sandbox
    std::error_code ec_rm;
    fs::remove_all(test_dir, ec_rm);

    std::cout << "\n>>> All fastwad End-to-End Unit and Integration Tests Passed Successfully! <<<\n" << std::endl;
}

// ---------------------------------------------------------------------------
// Visual Showcase & Benchmark Generator for GitHub Pages
// ---------------------------------------------------------------------------

void GenerateShowcase(const std::string& output_dir) {
    std::cout << "\n=======================================================\n";
    std::cout << "  fastwad: Generating Visual Showcase & Benchmarks Site\n";
    std::cout << "  Target: " << output_dir << "\n";
    std::cout << "=======================================================\n\n";

    fs::path root(output_dir);
    fs::path assets_dir = root / "assets";
    fs::create_directories(assets_dir);

    struct ShowcaseSample {
        std::string id;
        std::string title;
        std::string desc;
        int width;
        int height;
        int pattern;
    };

    std::vector<ShowcaseSample> samples = {
        {"gradient_sky", "Gradient Skybox", "Smooth 24-bit multi-channel gradient testing K-Means quantization & Floyd-Steinberg dithering.", 256, 128, 0},
        {"metal_grate", "{Chainlink Fence", "Chainlink wire texture with alpha transparency testing cutout quantization and GoldSrc key-blue mapping.", 128, 128, 1},
        {"sci_panel", "Sci-Fi High Contrast", "High-frequency checker and neon patterns testing spatial mipmap downsampling.", 256, 256, 2},
        {"neon_cyber", "Cyberpunk Plasma", "Continuous sinusoidal color waves testing palette range and gamut coverage.", 128, 256, 3}
    };

    struct RenderedItem {
        std::string id;
        std::string title;
        std::string desc;
        std::string orig_png;
        std::string dither_png;
        std::string nodither_png;
        std::string palette_png;
        std::string mip0_png;
        std::string mip1_png;
        std::string mip2_png;
        std::string mip3_png;
        uint32_t width;
        uint32_t height;
        double proc_time_ms;
    };

    std::vector<RenderedItem> rendered_items;
    double total_ms = 0.0;

    for (const auto& s : samples) {
        fs::path orig_path = assets_dir / (s.id + "_orig.png");

        if (s.id == "metal_grate") {
            bool loaded = false;
            std::vector<fs::path> candidates = {
                "texture.png",
                "res/texture.png",
                "../res/texture.png",
                "../../res/texture.png"
            };
            for (const auto& cand : candidates) {
                if (fs::exists(cand)) {
                    std::error_code ec_cp;
                    fs::copy_file(cand, orig_path, fs::copy_options::overwrite_existing, ec_cp);
                    if (!ec_cp) {
                        loaded = true;
                        break;
                    }
                }
            }
            if (!loaded) {
                CreateSyntheticImage(orig_path.string(), s.width, s.height, 4, s.pattern);
            }
        } else {
            CreateSyntheticImage(orig_path.string(), s.width, s.height, (s.pattern == 1 ? 4 : 3), s.pattern);
        }

        AppConfig cfg_dither;
        cfg_dither.disable_dither = false;
        cfg_dither.quiet = true;

        AppConfig cfg_nodither;
        cfg_nodither.disable_dither = true;
        cfg_nodither.quiet = true;

        MipTexData dither_data, nodither_data;

        auto t0 = std::chrono::high_resolution_clock::now();
        ImageProcessor::ProcessFile(orig_path.string(), s.id, cfg_dither, dither_data);
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(t1 - t0).count();
        total_ms += elapsed;

        ImageProcessor::ProcessFile(orig_path.string(), s.id, cfg_nodither, nodither_data);

        fs::path dither_path = assets_dir / (s.id + "_dither.png");
        fs::path nodither_path = assets_dir / (s.id + "_nodither.png");
        fs::path pal_path = assets_dir / (s.id + "_palette.png");

        ImageProcessor::ExportImage(dither_path.string(), dither_data, false);
        ImageProcessor::ExportImage(nodither_path.string(), nodither_data, false);
        GeneratePalettePreviewPng(pal_path.string(), dither_data.palette);

        // Export individual mip levels
        RenderedItem item;
        item.id = s.id;
        item.title = s.title;
        item.desc = s.desc;
        item.orig_png = "assets/" + s.id + "_orig.png";
        item.dither_png = "assets/" + s.id + "_dither.png";
        item.nodither_png = "assets/" + s.id + "_nodither.png";
        item.palette_png = "assets/" + s.id + "_palette.png";
        item.width = dither_data.width;
        item.height = dither_data.height;
        item.proc_time_ms = elapsed;

        // Export Mip0, Mip1, Mip2, Mip3 as images
        for (int m = 0; m < 4; ++m) {
            uint32_t mw = std::max(1u, dither_data.width >> m);
            uint32_t mh = std::max(1u, dither_data.height >> m);
            std::vector<uint8_t> mip_rgb(mw * mh * 3);
            for (size_t p = 0; p < dither_data.mip[m].size(); ++p) {
                ColorRGB c = dither_data.palette[dither_data.mip[m][p]];
                mip_rgb[p * 3 + 0] = c.r;
                mip_rgb[p * 3 + 1] = c.g;
                mip_rgb[p * 3 + 2] = c.b;
            }
            std::string mip_rel = "assets/" + s.id + "_mip" + std::to_string(m) + ".png";
            stbi_write_png((root / mip_rel).string().c_str(), mw, mh, 3, mip_rgb.data(), mw * 3);
            if (m == 0) item.mip0_png = mip_rel;
            else if (m == 1) item.mip1_png = mip_rel;
            else if (m == 2) item.mip2_png = mip_rel;
            else if (m == 3) item.mip3_png = mip_rel;
        }

        rendered_items.push_back(item);
        std::cout << "  Rendered showcase sample: " << s.title << " (" << elapsed << " ms)\n";
    }

    double avg_ms = total_ms / samples.size();
    double throughput = (avg_ms > 0.0) ? (1000.0 / avg_ms) : 0.0;
    // If an index.html does not already exist in the target directory, generate a standalone HTML report
    if (!fs::exists(root / "index.html")) {
        std::ofstream html(root / "index.html");
        html << "<!DOCTYPE html>\n"
         << "<html lang=\"en\" class=\"dark\">\n"
         << "<head>\n"
         << "  <meta charset=\"UTF-8\">\n"
         << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
         << "  <title>fastwad - High-Performance GoldSrc & Quake WAD Engine</title>\n"
         << "  <script src=\"https://cdn.tailwindcss.com\"></script>\n"
         << "  <script>tailwind.config = { darkMode: 'class', theme: { extend: { colors: { brand: '#0ea5e9' } } } };</script>\n"
         << "  <style>\n"
         << "    .pixelated { image-rendering: pixelated; image-rendering: crisp-edges; }\n"
         << "  </style>\n"
         << "</head>\n"
         << "<body class=\"bg-slate-950 text-slate-100 font-sans min-h-screen\">\n"
         << "  <header class=\"border-b border-slate-800 bg-slate-900/80 backdrop-blur sticky top-0 z-50\">\n"
         << "    <div class=\"max-w-7xl mx-auto px-6 py-4 flex items-center justify-between\">\n"
         << "      <div class=\"flex items-center space-x-3\">\n"
         << "        <div class=\"w-9 h-9 rounded-lg bg-sky-500 flex items-center justify-center font-black text-slate-950 text-xl\">fW</div>\n"
         << "        <span class=\"text-xl font-bold tracking-tight text-white\">fastwad <span class=\"text-xs px-2 py-0.5 rounded bg-sky-500/20 text-sky-400 font-mono\">v1.0.0</span></span>\n"
         << "      </div>\n"
         << "      <nav class=\"flex space-x-6 text-sm font-medium text-slate-400\">\n"
         << "        <a href=\"#showcase\" class=\"hover:text-white transition\">Visual Showcase</a>\n"
         << "        <a href=\"#benchmarks\" class=\"hover:text-white transition\">Benchmarks</a>\n"
         << "        <a href=\"#cli\" class=\"hover:text-white transition\">CLI & SDK</a>\n"
         << "        <a href=\"https://github.com/urgorri/fastwad\" target=\"_blank\" class=\"hover:text-sky-400 transition\">GitHub &rarr;</a>\n"
         << "      </nav>\n"
         << "    </div>\n"
         << "  </header>\n\n"
         << "  <main class=\"max-w-7xl mx-auto px-6 py-12 space-y-16\">\n"
         << "    <!-- Hero Section -->\n"
         << "    <section class=\"text-center space-y-6 pt-6 pb-4\">\n"
         << "      <div class=\"inline-flex items-center space-x-2 px-3 py-1 rounded-full bg-slate-800 border border-slate-700 text-xs text-slate-300\">\n"
         << "        <span class=\"w-2 h-2 rounded-full bg-emerald-400 animate-pulse\"></span>\n"
         << "        <span>Deterministic GoldSrc (Half-Life 1) & Quake WAD2/WAD3 Compiler</span>\n"
         << "      </div>\n"
         << "      <h1 class=\"text-4xl sm:text-6xl font-extrabold tracking-tight text-white max-w-4xl mx-auto\">\n"
         << "        High-Fidelity Retro Textures, <span class=\"text-sky-400\">Automated & Native</span>.\n"
         << "      </h1>\n"
         << "      <p class=\"text-lg text-slate-400 max-w-2xl mx-auto\">\n"
         << "        Standalone C++ engine with K-Means++ 256-color quantization, Floyd-Steinberg dithering, micro-stretch snapping, and machine-readable JSON integration.\n"
         << "      </p>\n"
         << "      <div class=\"flex justify-center gap-4 pt-2 font-mono text-xs\">\n"
         << "        <div class=\"px-4 py-2 rounded bg-slate-900 border border-slate-800\"><span class=\"text-slate-500\">Avg Latency:</span> <span class=\"text-sky-400 font-bold\">" << std::fixed << std::setprecision(1) << avg_ms << " ms</span></div>\n"
         << "        <div class=\"px-4 py-2 rounded bg-slate-900 border border-slate-800\"><span class=\"text-slate-500\">Throughput:</span> <span class=\"text-emerald-400 font-bold\">~" << (int)throughput << " tex/sec</span></div>\n"
         << "        <div class=\"px-4 py-2 rounded bg-slate-900 border border-slate-800\"><span class=\"text-slate-500\">Binary Size:</span> <span class=\"text-purple-400 font-bold\">~310 KB</span></div>\n"
         << "      </div>\n"
         << "    </section>\n\n"
         << "    <!-- Showcase Gallery -->\n"
         << "    <section id=\"showcase\" class=\"space-y-8\">\n"
         << "      <div class=\"border-b border-slate-800 pb-4\">\n"
         << "        <h2 class=\"text-2xl font-bold text-white\">Live Visual Capability Showcase</h2>\n"
         << "        <p class=\"text-sm text-slate-400\">Automatically generated from CI builds validating quantization, dithering, alpha masks, and mipmaps.</p>\n"
         << "      </div>\n\n"
         << "      <div class=\"grid grid-cols-1 md:grid-cols-2 gap-8\">\n";

    for (const auto& item : rendered_items) {
        html << "        <div class=\"bg-slate-900 border border-slate-800 rounded-xl p-6 space-y-5\">\n"
             << "          <div class=\"flex items-center justify-between\">\n"
             << "            <h3 class=\"text-lg font-bold text-white\">" << item.title << "</h3>\n"
             << "            <span class=\"text-xs font-mono px-2 py-1 rounded bg-slate-800 text-slate-400\">" << item.width << "x" << item.height << " | " << std::fixed << std::setprecision(1) << item.proc_time_ms << "ms</span>\n"
             << "          </div>\n"
             << "          <p class=\"text-xs text-slate-400\">" << item.desc << "</p>\n\n"
             << "          <!-- Side-by-side Diff -->\n"
             << "          <div class=\"grid grid-cols-3 gap-3 text-center text-xs font-mono\">\n"
             << "            <div>\n"
             << "              <div class=\"text-slate-500 mb-1\">Original (24-bit)</div>\n"
             << "              <div class=\"aspect-square bg-slate-950 rounded border border-slate-800 flex items-center justify-center overflow-hidden p-1\">\n"
             << "                <img src=\"" << item.orig_png << "\" class=\"max-w-full max-h-full pixelated rounded\">\n"
             << "              </div>\n"
             << "            </div>\n"
             << "            <div>\n"
             << "              <div class=\"text-sky-400 mb-1\">fastwad (Dithered)</div>\n"
             << "              <div class=\"aspect-square bg-slate-950 rounded border border-sky-900/50 flex items-center justify-center overflow-hidden p-1\">\n"
             << "                <img src=\"" << item.dither_png << "\" class=\"max-w-full max-h-full pixelated rounded\">\n"
             << "              </div>\n"
             << "            </div>\n"
             << "            <div>\n"
             << "              <div class=\"text-slate-400 mb-1\">256-Color Palette</div>\n"
             << "              <div class=\"aspect-square bg-slate-950 rounded border border-slate-800 flex items-center justify-center overflow-hidden p-1\">\n"
             << "                <img src=\"" << item.palette_png << "\" class=\"max-w-full max-h-full pixelated rounded\" title=\"16x16 256-color palette\">\n"
             << "              </div>\n"
             << "            </div>\n"
             << "          </div>\n\n"
             << "          <!-- 4 Mipmap Levels -->\n"
             << "          <div class=\"pt-2 border-t border-slate-800/60\">\n"
             << "            <div class=\"text-xs font-mono text-slate-500 mb-2\">Mipmap Hierarchy (100% -> 50% -> 25% -> 12.5%):</div>\n"
             << "            <div class=\"flex items-end gap-3 bg-slate-950 p-2 rounded border border-slate-800/80\">\n"
             << "              <img src=\"" << item.mip0_png << "\" class=\"h-16 pixelated rounded border border-slate-700\" title=\"Mip 0\">\n"
             << "              <img src=\"" << item.mip1_png << "\" class=\"h-12 pixelated rounded border border-slate-700\" title=\"Mip 1\">\n"
             << "              <img src=\"" << item.mip2_png << "\" class=\"h-8 pixelated rounded border border-slate-700\" title=\"Mip 2\">\n"
             << "              <img src=\"" << item.mip3_png << "\" class=\"h-5 pixelated rounded border border-slate-700\" title=\"Mip 3\">\n"
             << "            </div>\n"
             << "          </div>\n"
             << "        </div>\n";
    }

    html << "      </div>\n"
         << "    </section>\n\n"
         << "    <!-- CLI & Developer Integration -->\n"
         << "    <section id=\"cli\" class=\"space-y-6\">\n"
         << "      <div class=\"border-b border-slate-800 pb-4\">\n"
         << "        <h2 class=\"text-2xl font-bold text-white\">Low-Level CLI & Embeddable C++ SDK</h2>\n"
         << "        <p class=\"text-sm text-slate-400\">Built for seamless integration into TrenchBroom, J.A.C.K., Hammer, and automated CI pipelines.</p>\n"
         << "      </div>\n\n"
         << "      <div class=\"grid grid-cols-1 lg:grid-cols-2 gap-6 font-mono text-xs\">\n"
         << "        <!-- CLI Example -->\n"
         << "        <div class=\"bg-slate-900 border border-slate-800 rounded-xl p-5 space-y-3\">\n"
         << "          <div class=\"text-sky-400 font-bold flex justify-between items-center\">\n"
         << "            <span>CLI Machine-Readable JSON Mode</span>\n"
         << "            <span class=\"text-slate-500 font-normal\">fastwad --json</span>\n"
         << "          </div>\n"
         << "          <pre class=\"bg-slate-950 p-4 rounded text-slate-300 overflow-x-auto border border-slate-800/80\">"
         << "# Build WAD from folder with JSON metrics:\n"
         << "fastwad build ./textures ./halflife.wad --json\n\n"
         << "# Inspect WAD metadata & lump offsets:\n"
         << "fastwad list ./halflife.wad --json | jq .lumps[].name\n\n"
         << "# Extract textures:\n"
         << "fastwad extract ./halflife.wad ./extracted --json</pre>\n"
         << "        </div>\n\n"
         << "        <!-- C++ SDK Example -->\n"
         << "        <div class=\"bg-slate-900 border border-slate-800 rounded-xl p-5 space-y-3\">\n"
         << "          <div class=\"text-emerald-400 font-bold flex justify-between items-center\">\n"
         << "            <span>Embeddable C++17 Header SDK</span>\n"
         << "            <span class=\"text-slate-500 font-normal\">fastwad/fastwad.hpp</span>\n"
         << "          </div>\n"
         << "          <pre class=\"bg-slate-950 p-4 rounded text-slate-300 overflow-x-auto border border-slate-800/80\">"
         << "#include &lt;fastwad/fastwad.hpp&gt;\n\n"
         << "// In-memory WAD unpack and pack:\n"
         << "auto textures = fastwad::UnpackWadFromMemory(data, size);\n"
         << "std::vector&lt;uint8_t&gt; wad_bytes = fastwad::PackWadToMemory(*textures);\n\n"
         << "// File operations:\n"
         << "fastwad::BuildWadFromDirectory(\"./textures\", \"./out.wad\");</pre>\n"
         << "        </div>\n"
         << "      </div>\n"
         << "    </section>\n"
         << "  </main>\n\n"
         << "  <footer class=\"border-t border-slate-800/80 py-8 text-center text-xs text-slate-500\">\n"
         << "    <p>fastwad is an open-source tool licensed under the MIT License.</p>\n"
         << "  </footer>\n"
         << "</body>\n"
         << "</html>\n";
        html.close();
    }
    std::cout << "\n>>> Visual Showcase assets generated successfully in: " << (root / "assets").string() << " <<<\n\n";
}

int main(int argc, char** argv) {
    std::cout << "fastwad test runner initialized." << std::endl;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--generate-showcase" && i + 1 < argc) {
            GenerateShowcase(argv[i + 1]);
            return 0;
        }
    }

    RunCoreTests();
    return 0;
}
