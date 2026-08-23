#pragma once

#include "config.hpp"
#include "fastwad/fastwad.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace fastwad {

using MipTexData = MipTex;

class ImageProcessor {
public:
    static bool ProcessFile(const std::string& filepath, const std::string& internal_name, 
                            const AppConfig& config, MipTexData& out_data);
    static bool ProcessBuffer(const uint8_t* buffer, size_t size, const std::string& internal_name,
                             const AppConfig& config, MipTexData& out_data);
    static bool ExportImage(const std::string& filepath, const MipTexData& data, bool as_bmp);
    static bool ExportImageToMemory(const MipTexData& data, bool as_bmp, std::vector<uint8_t>& out_bytes);

    static void QuantizeAndDither(const std::vector<ColorRGB>& image_rgb, int w, int h, 
                                  bool use_dither, MipTexData& out_data, bool has_transparency,
                                  const std::vector<bool>& is_transparent, const AppConfig& config);
    static void GenerateMipmaps(MipTexData& data);
};

} // namespace fastwad
