#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace fastwad {

enum class WadFormat {
    WAD2, // Quake 1
    WAD3  // Half-Life 1 / GoldSrc
};

struct ColorRGB {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

struct MipTex {
    std::string name;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> mip[4];    // 4 mipmap levels of indexed color bytes
    std::vector<ColorRGB> palette;  // 256 RGB colors
    bool has_transparency = false;
};

struct LumpInfo {
    std::string name;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t offset = 0;
    uint32_t disk_size = 0;
    uint32_t size = 0;
    uint8_t type = 0x43;
    bool has_transparency = false;
    uint32_t mip_offsets[4] = {0, 0, 0, 0};
    uint32_t palette_offset = 0;
};

struct WadInfo {
    WadFormat format = WadFormat::WAD3;
    uint32_t num_lumps = 0;
    uint32_t info_table_offset = 0;
    std::vector<LumpInfo> lumps;
};

struct TextureOptions {
    int max_size = 256;
    bool disable_dither = false;
    bool stretch = false;
    std::string align = "center";
    ColorRGB key_color = ColorRGB{0, 0, 255};
};

struct BuildResult {
    bool success = false;
    int total_processed = 0;
    int skipped = 0;
    int failed = 0;
    std::string error_message;
    std::vector<std::string> warnings;
};

// High-Level File Operations
BuildResult BuildWadFromDirectory(const std::string& input_dir, const std::string& output_wad,
                                  WadFormat format = WadFormat::WAD3,
                                  const TextureOptions& opts = TextureOptions{},
                                  bool allow_overwrite = false);

bool ExtractWadToDirectory(const std::string& input_wad, const std::string& output_dir,
                           bool as_bmp = false,
                           std::string* error_msg = nullptr,
                           std::vector<std::string>* extracted_files = nullptr);

std::optional<WadInfo> InspectWadFile(const std::string& input_wad,
                                     std::string* error_msg = nullptr);

// Low-Level In-Memory Stream Operations
std::vector<uint8_t> PackWadToMemory(const std::vector<MipTex>& textures,
                                     WadFormat format = WadFormat::WAD3);

std::optional<std::vector<MipTex>> UnpackWadFromMemory(const uint8_t* wad_data,
                                                      size_t size,
                                                      std::string* error_msg = nullptr);

std::optional<WadInfo> InspectWadFromMemory(const uint8_t* wad_data,
                                           size_t size,
                                           std::string* error_msg = nullptr);

std::optional<MipTex> ProcessImageBuffer(const uint8_t* img_bytes, size_t size,
                                        const std::string& lump_name,
                                        const TextureOptions& opts);

} // namespace fastwad
