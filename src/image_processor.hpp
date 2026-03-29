#pragma once
#include "config.hpp"
#include <vector>
#include <string>

struct ColorRGB { uint8_t r, g, b; };

struct MipTexData {
    std::string name;
    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> mip[4]; // Indices
    std::vector<ColorRGB> palette;
};

class ImageProcessor {
public:
    static bool ProcessFile(const std::string& filepath, const std::string& internal_name, 
                            const AppConfig& config, MipTexData& out_data);
    static bool ExportImage(const std::string& filepath, const MipTexData& data, bool as_bmp);

private:
    static void QuantizeAndDither(const std::vector<ColorRGB>& image_rgb, int w, int h, 
                                  bool use_dither, MipTexData& out_data, bool has_transparency,
                                  const std::vector<bool>& is_transparent, const AppConfig& config);
    static void GenerateMipmaps(MipTexData& data);
};
