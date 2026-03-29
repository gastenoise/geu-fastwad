#include "wad_archive.hpp"
#include "utils.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <iomanip>
#include <set>
#include <cstring>
#include <map>
#include <algorithm>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct WadHeader {
    char signature[4];
    uint32_t numLumps;
    uint32_t infoTableOffset;
};

struct WadDirEntry {
    uint32_t offset;
    uint32_t diskSize;
    uint32_t size;
    char type;
    char compression;
    char pad[2];
    char name[16];
};

struct MipTexHeader {
    char name[16];
    uint32_t width;
    uint32_t height;
    uint32_t offsets[4];
};
#pragma pack(pop)

std::string WadArchive::NormalizeName(const std::string& raw) {
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

ExitCode WadArchive::Build(const AppConfig& config) {
    if (!fs::exists(config.input_path) || !fs::is_directory(config.input_path)) {
        if (!config.quiet) std::cerr << "Fatal: Input is not a valid directory.\n";
        return ExitCode::FatalError;
    }

    if (fs::exists(config.output_path) && !config.allow_overwrite) {
        if (!config.quiet) std::cerr << "Fatal: Output exists. Use allow_overwrite=true to bypass.\n";
        return ExitCode::FatalError;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(config.input_path)) {
        if (entry.is_regular_file()) files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end()); // Deterministic sorting

    std::vector<MipTexData> textures;
    std::set<std::string> used_names;
    int skipped = 0, failed = 0;

    for (const auto& path : files) {
        std::string raw_name = path.stem().string();
        std::string norm = NormalizeName(raw_name);
        
        // Resolve Collisions deterministically
        if (used_names.count(norm)) {
            std::string hash_hex;
            char buf[16];
            // Hash based on filename only for stability across different parent paths
            snprintf(buf, sizeof(buf), "%08x", utils::Fnv1aHash(path.filename().string()));
            hash_hex = buf;
            
            size_t suffix_len = hash_hex.length() + 1; // +1 for the underscore
            size_t take = (15 > suffix_len) ? (15 - suffix_len) : 0;
            norm = norm.substr(0, take) + "_" + hash_hex;

            // Second pass safety: if still colliding (unlikely), append a counter
            int counter = 1;
            std::string base_norm = norm;
            while (used_names.count(norm)) {
                std::string counter_str = std::to_string(counter++);
                size_t c_len = counter_str.length();
                size_t b_take = (15 > c_len) ? (15 - c_len) : 0;
                norm = base_norm.substr(0, b_take) + counter_str;
            }
        }
        used_names.insert(norm);

        MipTexData tex;
        if (!ImageProcessor::ProcessFile(path.string(), norm, config, tex)) {
            if (config.verbose) std::cerr << "Warn: Skipping non-image or unreadable " << path.filename() << "\n";
            skipped++;
            continue;
        }
        if (tex.width == 0) { failed++; continue; }
        textures.push_back(tex);
        if (config.verbose) std::cout << "Processed: " << path.filename() << " -> " << norm << "\n";
    }

    if (textures.empty()) {
        if (!config.quiet) std::cerr << "Fatal: No valid textures generated. Archive not created.\n";
        return ExitCode::FatalError;
    }

    std::ofstream out(config.output_path, std::ios::binary);
    if (!out) return ExitCode::FatalError;

    WadHeader header;
    std::memcpy(header.signature, config.wad2 ? "WAD2" : "WAD3", 4);
    header.numLumps = (uint32_t)textures.size();
    header.infoTableOffset = 0; // Written later
    out.write((char*)&header, sizeof(WadHeader));

    std::vector<WadDirEntry> directory;
    for (auto& tex : textures) {
        WadDirEntry dir{};
        dir.offset = (uint32_t)out.tellp();
        dir.type = 0x43; // MipTex
        dir.compression = 0;
        std::strncpy(dir.name, tex.name.c_str(), 15);
        
        MipTexHeader mhead{};
        std::strncpy(mhead.name, tex.name.c_str(), 15);
        mhead.width = tex.width;
        mhead.height = tex.height;
        
        uint32_t cursor = sizeof(MipTexHeader);
        for (int i = 0; i < 4; ++i) {
            mhead.offsets[i] = cursor;
            cursor += (uint32_t)tex.mip[i].size();
        }
        
        out.write((char*)&mhead, sizeof(MipTexHeader));
        for (int i = 0; i < 4; ++i) {
            out.write((char*)tex.mip[i].data(), tex.mip[i].size());
        }
        
        uint16_t palSize = 256;
        out.write((char*)&palSize, sizeof(uint16_t));
        for (int i = 0; i < 256; ++i) {
            out.write((char*)&tex.palette[i], sizeof(ColorRGB));
        }
        
        dir.size = dir.diskSize = (uint32_t)out.tellp() - dir.offset;
        directory.push_back(dir);
    }

    header.infoTableOffset = (uint32_t)out.tellp();
    for (const auto& dir : directory) {
        out.write((char*)&dir, sizeof(WadDirEntry));
    }
    out.seekp(0);
    out.write((char*)&header, sizeof(WadHeader));
    
    if (!config.quiet) {
        std::cout << "Build summary:\n"
                  << "  Total Processed: " << textures.size() << "\n"
                  << "  Skipped: " << skipped << "\n"
                  << "  Failed: " << failed << "\n"
                  << "  Archive: " << config.output_path << " (" << (config.wad2 ? "WAD2" : "WAD3") << ")\n";
    }

    return failed > 0 ? ExitCode::PartialSuccess : ExitCode::Success;
}

ExitCode WadArchive::List(const AppConfig& config) {
    std::ifstream in(config.input_path, std::ios::binary);
    if (!in) { std::cerr << "Fatal: Cannot open " << config.input_path << "\n"; return ExitCode::FatalError; }

    WadHeader header;
    in.read((char*)&header, sizeof(WadHeader));
    if (std::strncmp(header.signature, "WAD2", 4) != 0 && std::strncmp(header.signature, "WAD3", 4) != 0) {
        std::cerr << "Fatal: Invalid WAD signature.\n"; return ExitCode::FatalError;
    }

    if (!config.quiet) {
        std::cout << "Archive: " << config.input_path << "\n";
        std::cout << "Format:  " << std::string(header.signature, 4) << "\n";
        std::cout << "Lumps:   " << header.numLumps << "\n\n";
        std::cout << std::left << std::setw(16) << "Name" << std::setw(12) << "Dimensions" << "Size (bytes)\n";
        std::cout << std::string(45, '-') << "\n";
    }

    in.seekg(header.infoTableOffset);
    std::vector<WadDirEntry> directory(header.numLumps);
    in.read((char*)directory.data(), header.numLumps * sizeof(WadDirEntry));

    for (const auto& dir : directory) {
        if (dir.type != 0x43) continue;
        in.seekg(dir.offset);
        MipTexHeader mhead;
        in.read((char*)&mhead, sizeof(MipTexHeader));
        if (!config.quiet) {
            std::cout << std::left << std::setw(16) << dir.name 
                      << mhead.width << "x" << std::setw(10) << mhead.height 
                      << dir.diskSize << "\n";
        }
    }
    return ExitCode::Success;
}

ExitCode WadArchive::Extract(const AppConfig& config) {
    std::ifstream in(config.input_path, std::ios::binary);
    if (!in) { std::cerr << "Fatal: Cannot open WAD.\n"; return ExitCode::FatalError; }

    if (!fs::exists(config.output_path)) fs::create_directories(config.output_path);

    WadHeader header;
    in.read((char*)&header, sizeof(WadHeader));
    in.seekg(header.infoTableOffset);
    std::vector<WadDirEntry> directory(header.numLumps);
    in.read((char*)directory.data(), header.numLumps * sizeof(WadDirEntry));

    int ext_count = 0;
    for (const auto& dir : directory) {
        if (dir.type != 0x43) continue;
        in.seekg(dir.offset);
        MipTexHeader mhead;
        in.read((char*)&mhead, sizeof(MipTexHeader));

        MipTexData tex;
        tex.width = mhead.width;
        tex.height = mhead.height;
        tex.name = mhead.name;
        
        // Mip0 size is width*height
        in.seekg(dir.offset + mhead.offsets[0]);
        tex.mip[0].resize(tex.width * tex.height);
        in.read((char*)tex.mip[0].data(), tex.mip[0].size());

        // Palette is always at the end: 2 bytes for color count + 256*3 bytes for RGB
        // For MipTex, it is after the last mip (mip[3])
        // mip[3] is width/8 * height/8
        uint32_t last_mip_size = (tex.width / 8) * (tex.height / 8);
        in.seekg(dir.offset + mhead.offsets[3] + last_mip_size);

        uint16_t palSize = 0;
        in.read((char*)&palSize, sizeof(uint16_t));
        if (palSize == 0) palSize = 256; // Fallback

        tex.palette.resize(256, {0, 0, 0});
        in.read((char*)tex.palette.data(), std::min((uint16_t)256, palSize) * sizeof(ColorRGB));

        std::string ext = config.extract_bmp ? ".bmp" : ".png";
        fs::path out_file = fs::path(config.output_path) / (NormalizeName(tex.name) + ext);
        if (ImageProcessor::ExportImage(out_file.string(), tex, config.extract_bmp)) {
            ext_count++;
            if (config.verbose) std::cout << "Extracted: " << out_file.string() << "\n";
        }
    }
    if (!config.quiet) std::cout << "Successfully extracted " << ext_count << " textures.\n";
    return ExitCode::Success;
}