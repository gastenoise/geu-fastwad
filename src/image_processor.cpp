#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "image_processor.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <filesystem>
#include <cstring>

// Helper to calculate Euclidean distance squared
static inline int ColorDist(const ColorRGB& a, const ColorRGB& b) {
    int dr = (int)a.r - b.r, dg = (int)a.g - b.g, db = (int)a.b - b.b;
    return dr*dr + dg*dg + db*db;
}

static void BuildPalette(const std::vector<ColorRGB>& pixels, std::vector<ColorRGB>& palette, bool has_transparency, const AppConfig& config) {
    palette.assign(256, {0, 0, 0});
    int max_colors = has_transparency ? 255 : 256;
    if (has_transparency) {
        palette[255] = {config.pad_r, config.pad_g, config.pad_b};
    }

    // High quality: Use all pixels but deduplicate for initialization
    std::vector<ColorRGB> unique_colors;
    {
        // Simple hash-based deduplication for initialization speed
        std::vector<uint8_t> seen(1 << 15, 0); // 32k buckets
        for (const auto& p : pixels) {
            uint16_t h = ((p.r >> 3) << 10) | ((p.g >> 3) << 5) | (p.b >> 3);
            if (!seen[h]) {
                unique_colors.push_back(p);
                seen[h] = 1;
            }
            if (unique_colors.size() > 4096) break; // Enough for good seeding
        }
    }

    if (unique_colors.size() <= (size_t)max_colors) {
        for (size_t i = 0; i < unique_colors.size(); ++i) palette[i] = unique_colors[i];
        return;
    }

    // K-Means++ initialization (deterministic furthest-point seeding)
    palette[0] = unique_colors[0];
    std::vector<int> min_dist_sq(unique_colors.size(), 1e9);
    for (int i = 1; i < max_colors; ++i) {
        int best_p = 0;
        int max_d = -1;
        for (int p = 0; p < (int)unique_colors.size(); ++p) {
            int d = ColorDist(unique_colors[p], palette[i - 1]);
            if (d < min_dist_sq[p]) min_dist_sq[p] = d;
            if (min_dist_sq[p] > max_d) {
                max_d = min_dist_sq[p];
                best_p = p;
            }
        }
        palette[i] = unique_colors[best_p];
    }

    // K-Means iterations
    std::vector<uint64_t> sum_r(256), sum_g(256), sum_b(256), counts(256);
    for (int iter = 0; iter < 32; ++iter) { // More iterations for quality
        std::fill(sum_r.begin(), sum_r.end(), 0);
        std::fill(sum_g.begin(), sum_g.end(), 0);
        std::fill(sum_b.begin(), sum_b.end(), 0);
        std::fill(counts.begin(), counts.end(), 0);

        for (const auto& p : pixels) {
            int best_idx = 0;
            int best_dist = ColorDist(p, palette[0]);
            for (int i = 1; i < max_colors; ++i) {
                int d = ColorDist(p, palette[i]);
                if (d < best_dist) { best_dist = d; best_idx = i; }
            }
            sum_r[best_idx] += p.r;
            sum_g[best_idx] += p.g;
            sum_b[best_idx] += p.b;
            counts[best_idx]++;
        }

        bool changed = false;
        for (int i = 0; i < max_colors; ++i) {
            if (counts[i] > 0) {
                ColorRGB old = palette[i];
                palette[i].r = (uint8_t)(sum_r[i] / counts[i]);
                palette[i].g = (uint8_t)(sum_g[i] / counts[i]);
                palette[i].b = (uint8_t)(sum_b[i] / counts[i]);
                if (old.r != palette[i].r || old.g != palette[i].g || old.b != palette[i].b) changed = true;
            }
        }
        if (!changed) break; // Converged
    }
}

static int FindNearestPaletteIndex(const ColorRGB& c, const std::vector<ColorRGB>& pal, int max_idx) {
    int best_idx = 0;
    int best_dist = ColorDist(c, pal[0]);
    for (int i = 1; i <= max_idx; ++i) {
        int d = ColorDist(c, pal[i]);
        if (d < best_dist) { best_dist = d; best_idx = i; }
    }
    return best_idx;
}

void ImageProcessor::QuantizeAndDither(const std::vector<ColorRGB>& image_rgb, int w, int h, 
                                       bool use_dither, MipTexData& out_data, bool has_transparency,
                                       const std::vector<bool>& is_transparent, const AppConfig& config) {
    BuildPalette(image_rgb, out_data.palette, has_transparency, config);
    out_data.mip[0].resize(w * h);

    int max_pal_idx = has_transparency ? 254 : 255;

    if (!use_dither) {
        for (size_t i = 0; i < image_rgb.size(); ++i) {
            if (has_transparency && is_transparent[i]) {
                out_data.mip[0][i] = 255;
            } else {
                out_data.mip[0][i] = (uint8_t)FindNearestPaletteIndex(image_rgb[i], out_data.palette, max_pal_idx);
            }
        }
        return;
    }

    // Floyd-Steinberg Dithering
    std::vector<float> err_r(w * h, 0.0f), err_g(w * h, 0.0f), err_b(w * h, 0.0f);
    auto clamp = [](float v) { return std::max(0.0f, std::min(255.0f, v)); };

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            if (has_transparency && is_transparent[idx]) {
                out_data.mip[0][idx] = 255;
                continue;
            }

            ColorRGB p = image_rgb[idx];
            float nr = clamp(p.r + err_r[idx]);
            float ng = clamp(p.g + err_g[idx]);
            float nb = clamp(p.b + err_b[idx]);
            
            ColorRGB target = { (uint8_t)nr, (uint8_t)ng, (uint8_t)nb };
            int pal_idx = FindNearestPaletteIndex(target, out_data.palette, max_pal_idx);
            out_data.mip[0][idx] = (uint8_t)pal_idx;

            ColorRGB act = out_data.palette[pal_idx];
            float er = nr - act.r;
            float eg = ng - act.g;
            float eb = nb - act.b;

            auto add_err = [&](int ex, int ey, float factor) {
                if (ex >= 0 && ex < w && ey >= 0 && ey < h) {
                    err_r[ey * w + ex] += er * factor;
                    err_g[ey * w + ex] += eg * factor;
                    err_b[ey * w + ex] += eb * factor;
                }
            };
            add_err(x + 1, y, 7.0f / 16.0f);
            add_err(x - 1, y + 1, 3.0f / 16.0f);
            add_err(x, y + 1, 5.0f / 16.0f);
            add_err(x + 1, y + 1, 1.0f / 16.0f);
        }
    }
}

void ImageProcessor::GenerateMipmaps(MipTexData& data) {
    int w = data.width;
    int h = data.height;
    for (int mip = 1; mip < 4; ++mip) {
        int prev_w = std::max(1, w >> (mip - 1));
        int prev_h = std::max(1, h >> (mip - 1));
        int cur_w = std::max(1, w >> mip);
        int cur_h = std::max(1, h >> mip);
        
        data.mip[mip].resize(cur_w * cur_h);
        for (int y = 0; y < cur_h; ++y) {
            for (int x = 0; x < cur_w; ++x) {
                int px = x * 2, py = y * 2;
                int sum_r = 0, sum_g = 0, sum_b = 0, count = 0, trans_count = 0;
                for (int dy = 0; dy < 2; ++dy) {
                    for (int dx = 0; dx < 2; ++dx) {
                        if (px + dx < prev_w && py + dy < prev_h) {
                            uint8_t pal_idx = data.mip[mip-1][(py + dy) * prev_w + (px + dx)];
                            if (pal_idx == 255) {
                                trans_count++;
                            } else {
                                sum_r += data.palette[pal_idx].r;
                                sum_g += data.palette[pal_idx].g;
                                sum_b += data.palette[pal_idx].b;
                                count++;
                            }
                        }
                    }
                }
                if (trans_count >= 2) {
                    data.mip[mip][y * cur_w + x] = 255;
                } else if (count > 0) {
                    ColorRGB avg = { (uint8_t)(sum_r/count), (uint8_t)(sum_g/count), (uint8_t)(sum_b/count) };
                    data.mip[mip][y * cur_w + x] = (uint8_t)FindNearestPaletteIndex(avg, data.palette, 254);
                } else {
                    data.mip[mip][y * cur_w + x] = 255;
                }
            }
        }
    }
}

bool ImageProcessor::ProcessFile(const std::string& filepath, const std::string& internal_name, 
                                 const AppConfig& config, MipTexData& out_data) {
    int orig_w, orig_h, channels;

    // Portable file opening
    FILE* f = utils::OpenFilePortable(filepath, "rb");
    if (!f) return false;

    // Check for animation (GIF or WebP)
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buffer(size);
    if (fread(buffer.data(), 1, size, f) != (size_t)size) {
        fclose(f); return false;
    }

    // GIF check
    if (size > 10 && std::memcmp(buffer.data(), "GIF8", 4) == 0) {
        int z = 0, ch = 0, *delays = nullptr;
        uint8_t* gif_data = stbi_load_gif_from_memory(buffer.data(), (int)size, &delays, &orig_w, &orig_h, &z, &ch, 4);
        if (gif_data) {
            bool animated = (z > 1);
            stbi_image_free(gif_data);
            if (delays) stbi_image_free(delays);
            if (animated) {
                if (!config.quiet) std::cerr << "Warn: Skipping animated GIF: " << filepath << "\n";
                fclose(f); return false;
            }
        }
    }
    // WebP check for animation (RIFF ... WEBP ... ANIM)
    else if (size > 30 && std::memcmp(buffer.data(), "RIFF", 4) == 0 && std::memcmp(buffer.data() + 8, "WEBP", 4) == 0) {
        // Simple scan for "ANIM" chunk
        for (size_t i = 12; i < (size_t)size - 4; ++i) {
            if (std::memcmp(buffer.data() + i, "ANIM", 4) == 0) {
                if (!config.quiet) std::cerr << "Warn: Skipping animated WebP: " << filepath << "\n";
                fclose(f); return false;
            }
        }
    }

    fseek(f, 0, SEEK_SET);
    uint8_t* raw = stbi_load_from_file(f, &orig_w, &orig_h, &channels, 4);
    fclose(f);
    if (!raw) return false;

    int max_s = config.max_size;
    float aspect = (float)orig_w / orig_h;
    
    int fit_w = orig_w, fit_h = orig_h;
    if (fit_w > max_s || fit_h > max_s) {
        if (fit_w / (float)max_s > fit_h / (float)max_s) {
            fit_w = max_s; fit_h = std::max(1, (int)std::round(max_s / aspect));
        } else {
            fit_h = max_s; fit_w = std::max(1, (int)std::round(max_s * aspect));
        }
    }

    int canvas_w, canvas_h;
    if (config.stretch) {
        canvas_w = max_s;
        canvas_h = max_s;
    } else {
        canvas_w = std::clamp((int)std::round(fit_w / 16.0) * 16, 16, max_s);
        canvas_h = std::clamp((int)std::round(fit_h / 16.0) * 16, 16, max_s);
    }

    // Decision: use micro-stretch or padding?
    // User said: "contain primero, micro-stretch solo como remate técnico."
    // And: "The default fit behavior must be contain with padding and centered placement."

    // We'll scale to fit in canvas_w, canvas_h preserving aspect ratio.
    float canvas_aspect = (float)canvas_w / canvas_h;
    int img_w, img_h;
    if (aspect > canvas_aspect) {
        img_w = canvas_w;
        img_h = std::max(1, (int)std::round(canvas_w / aspect));
    } else {
        img_h = canvas_h;
        img_w = std::max(1, (int)std::round(canvas_h * aspect));
    }

    // Micro-stretch: if the difference is very small (e.g. <= 2px), just stretch to fill the canvas
    if (std::abs(img_w - canvas_w) <= 2 && std::abs(img_h - canvas_h) <= 2) {
        img_w = canvas_w;
        img_h = canvas_h;
    }

    std::vector<uint8_t> resized(img_w * img_h * 4);
    stbir_resize_uint8_linear(raw, orig_w, orig_h, 0, resized.data(), img_w, img_h, 0, STBIR_RGBA);
    stbi_image_free(raw);

    std::vector<ColorRGB> final_rgb(canvas_w * canvas_h, ColorRGB{config.pad_r, config.pad_g, config.pad_b});
    std::vector<bool> is_transparent(canvas_w * canvas_h, true);
    bool any_transparency = false;

    int offset_x = (canvas_w - img_w) / 2;
    int offset_y = (canvas_h - img_h) / 2;

    std::string align = config.align;
    if (align.find("top") != std::string::npos) offset_y = 0;
    if (align.find("bottom") != std::string::npos) offset_y = canvas_h - img_h;
    if (align.find("left") != std::string::npos) offset_x = 0;
    if (align.find("right") != std::string::npos) offset_x = canvas_w - img_w;

    for (int y = 0; y < img_h; ++y) {
        for (int x = 0; x < img_w; ++x) {
            int src_idx = (y * img_w + x) * 4;
            int dst_idx = ((y + offset_y) * canvas_w + (x + offset_x));
            uint8_t r = resized[src_idx], g = resized[src_idx + 1], b = resized[src_idx + 2], a = resized[src_idx + 3];
            
            if (a == 0) {
                final_rgb[dst_idx] = {config.pad_r, config.pad_g, config.pad_b};
                is_transparent[dst_idx] = true;
                any_transparency = true;
            } else if (a == 255) {
                final_rgb[dst_idx] = {r, g, b};
                is_transparent[dst_idx] = false;
            } else {
                float alpha = a / 255.0f;
                final_rgb[dst_idx].r = (uint8_t)(r * alpha + config.pad_r * (1.0f - alpha));
                final_rgb[dst_idx].g = (uint8_t)(g * alpha + config.pad_g * (1.0f - alpha));
                final_rgb[dst_idx].b = (uint8_t)(b * alpha + config.pad_b * (1.0f - alpha));
                is_transparent[dst_idx] = false; // Treat semi-transparent as opaque after blending
            }
        }
    }

    out_data.name = internal_name;
    out_data.width = canvas_w;
    out_data.height = canvas_h;

    QuantizeAndDither(final_rgb, canvas_w, canvas_h, !config.disable_dither, out_data, any_transparency, is_transparent, config);
    GenerateMipmaps(out_data);
    return true;
}

static void CustomStbiWrite(void* context, void* data, int size) {
    fwrite(data, 1, size, (FILE*)context);
}

bool ImageProcessor::ExportImage(const std::string& filepath, const MipTexData& data, bool as_bmp) {
    std::vector<uint8_t> rgb(data.width * data.height * 3);
    for (size_t i = 0; i < data.mip[0].size(); ++i) {
        ColorRGB c = data.palette[data.mip[0][i]];
        rgb[i * 3 + 0] = c.r;
        rgb[i * 3 + 1] = c.g;
        rgb[i * 3 + 2] = c.b;
    }

    FILE* f = utils::OpenFilePortable(filepath, "wb");
    if (!f) return false;

    int res = 0;
    if (as_bmp) res = stbi_write_bmp_to_func(CustomStbiWrite, f, data.width, data.height, 3, rgb.data());
    else res = stbi_write_png_to_func(CustomStbiWrite, f, data.width, data.height, 3, rgb.data(), data.width * 3);

    fclose(f);
    return res != 0;
}
