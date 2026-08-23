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
#include <sstream>

namespace fs = std::filesystem;

namespace fastwad {

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
        if (!config.quiet) std::cerr << "Fatal: Input is not a valid directory: " << config.input_path << "\n";
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
    std::sort(files.begin(), files.end()); // Deterministic sorting across all platforms

    std::vector<MipTexData> textures;
    std::vector<std::string> texture_sources;
    std::set<std::string> used_names;
    int skipped = 0, failed = 0;

    for (const auto& path : files) {
        MipTexData tex;
        std::string raw_name = path.stem().string();
        std::string norm = NormalizeName(raw_name);

        if (!ImageProcessor::ProcessFile(path.string(), norm, config, tex)) {
            if (config.verbose && !config.json_output) {
                std::cerr << "Warn: Skipping non-image or unreadable " << path.filename().string() << "\n";
            }
            skipped++;
            continue;
        }
        if (tex.width == 0) { failed++; continue; }

        // If transparent, force name to start with '{'
        if (tex.has_transparency) {
            if (tex.name.empty() || tex.name[0] != '{') {
                tex.name = "{" + tex.name;
            }
            if (tex.name.length() > 15) tex.name = tex.name.substr(0, 15);
        }
        norm = tex.name;

        // Resolve Collisions deterministically
        if (used_names.count(norm)) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%08x", utils::Fnv1aHash(path.filename().string()));
            std::string hash_hex = buf;
            
            size_t suffix_len = hash_hex.length() + 1; // +1 for the underscore
            size_t take = (15 > suffix_len) ? (15 - suffix_len) : 0;
            norm = norm.substr(0, take) + "_" + hash_hex;

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
        tex.name = norm;

        textures.push_back(tex);
        texture_sources.push_back(path.string());
        if (config.verbose && !config.json_output) {
            std::cerr << "Processed: " << path.filename().string() << " -> " << norm << "\n";
        }
    }

    if (textures.empty()) {
        if (!config.quiet) std::cerr << "Fatal: No valid textures generated. Archive not created.\n";
        return ExitCode::FatalError;
    }

    std::ofstream out(fs::path(config.output_path), std::ios::binary);
    if (!out) {
        if (!config.quiet) std::cerr << "Fatal: Could not open output file for writing: " << config.output_path << "\n";
        return ExitCode::FatalError;
    }

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
    out.close();

    ExitCode result = failed > 0 ? ExitCode::PartialSuccess : ExitCode::Success;

    if (config.json_output) {
        std::cout << "{\n"
                  << "  \"status\": \"" << (result == ExitCode::Success ? "success" : "partial_success") << "\",\n"
                  << "  \"exit_code\": " << (int)result << ",\n"
                  << "  \"archive\": \"" << config.output_path << "\",\n"
                  << "  \"format\": \"" << (config.wad2 ? "WAD2" : "WAD3") << "\",\n"
                  << "  \"total_processed\": " << textures.size() << ",\n"
                  << "  \"skipped\": " << skipped << ",\n"
                  << "  \"failed\": " << failed << ",\n"
                  << "  \"textures\": [\n";
        for (size_t i = 0; i < textures.size(); ++i) {
            std::cout << "    {\n"
                      << "      \"input\": \"" << texture_sources[i] << "\",\n"
                      << "      \"lump_name\": \"" << textures[i].name << "\",\n"
                      << "      \"width\": " << textures[i].width << ",\n"
                      << "      \"height\": " << textures[i].height << ",\n"
                      << "      \"transparent\": " << (textures[i].has_transparency ? "true" : "false") << ",\n"
                      << "      \"disk_size\": " << directory[i].diskSize << "\n"
                      << "    }" << (i + 1 < textures.size() ? "," : "") << "\n";
        }
        std::cout << "  ]\n}\n";
    } else if (!config.quiet) {
        std::cout << "Build summary:\n"
                  << "  Total Processed: " << textures.size() << "\n"
                  << "  Skipped: " << skipped << "\n"
                  << "  Failed: " << failed << "\n"
                  << "  Archive: " << config.output_path << " (" << (config.wad2 ? "WAD2" : "WAD3") << ")\n";
    }

    return result;
}

ExitCode WadArchive::List(const AppConfig& config) {
    std::ifstream in(fs::path(config.input_path), std::ios::binary);
    if (!in) { 
        if (!config.quiet) std::cerr << "Fatal: Cannot open " << config.input_path << "\n"; 
        return ExitCode::FatalError; 
    }

    WadHeader header;
    in.read((char*)&header, sizeof(WadHeader));
    if (std::strncmp(header.signature, "WAD2", 4) != 0 && std::strncmp(header.signature, "WAD3", 4) != 0) {
        if (!config.quiet) std::cerr << "Fatal: Invalid WAD signature.\n"; 
        return ExitCode::FatalError;
    }

    in.seekg(header.infoTableOffset);
    std::vector<WadDirEntry> directory(header.numLumps);
    in.read((char*)directory.data(), header.numLumps * sizeof(WadDirEntry));

    struct ParsedLump {
        WadDirEntry dir;
        MipTexHeader mhead;
        bool is_miptex = false;
    };

    std::vector<ParsedLump> lumps;
    for (const auto& dir : directory) {
        ParsedLump pl;
        pl.dir = dir;
        if (dir.type == 0x43) {
            pl.is_miptex = true;
            in.seekg(dir.offset);
            in.read((char*)&pl.mhead, sizeof(MipTexHeader));
        }
        lumps.push_back(pl);
    }

    if (config.json_output) {
        std::cout << "{\n"
                  << "  \"format\": \"" << std::string(header.signature, 4) << "\",\n"
                  << "  \"archive\": \"" << config.input_path << "\",\n"
                  << "  \"num_lumps\": " << header.numLumps << ",\n"
                  << "  \"info_table_offset\": " << header.infoTableOffset << ",\n"
                  << "  \"lumps\": [\n";
        for (size_t i = 0; i < lumps.size(); ++i) {
            const auto& pl = lumps[i];
            std::string lump_name = pl.dir.name;
            bool is_trans = (!lump_name.empty() && lump_name[0] == '{');

            std::cout << "    {\n"
                      << "      \"name\": \"" << lump_name << "\",\n"
                      << "      \"type\": " << (int)(uint8_t)pl.dir.type << ",\n"
                      << "      \"type_name\": \"" << (pl.is_miptex ? "MipTex" : "Unknown") << "\",\n"
                      << "      \"offset\": " << pl.dir.offset << ",\n"
                      << "      \"disk_size\": " << pl.dir.diskSize << ",\n"
                      << "      \"size\": " << pl.dir.size << ",\n"
                      << "      \"width\": " << (pl.is_miptex ? pl.mhead.width : 0) << ",\n"
                      << "      \"height\": " << (pl.is_miptex ? pl.mhead.height : 0) << ",\n"
                      << "      \"has_transparency\": " << (is_trans ? "true" : "false");

            if (pl.is_miptex) {
                std::cout << ",\n      \"mip_offsets\": ["
                          << pl.mhead.offsets[0] << ", "
                          << pl.mhead.offsets[1] << ", "
                          << pl.mhead.offsets[2] << ", "
                          << pl.mhead.offsets[3] << "],\n"
                          << "      \"palette_offset\": " << (pl.mhead.offsets[3] + (pl.mhead.width/8)*(pl.mhead.height/8) + 2) << "\n";
            } else {
                std::cout << "\n";
            }

            std::cout << "    }" << (i + 1 < lumps.size() ? "," : "") << "\n";
        }
        std::cout << "  ]\n}\n";
    } else if (!config.quiet) {
        std::cout << "Archive: " << config.input_path << "\n";
        std::cout << "Format:  " << std::string(header.signature, 4) << "\n";
        std::cout << "Lumps:   " << header.numLumps << "\n\n";
        std::cout << std::left << std::setw(16) << "Name" << std::setw(12) << "Dimensions" << "Size (bytes)\n";
        std::cout << std::string(45, '-') << "\n";

        for (const auto& pl : lumps) {
            if (!pl.is_miptex) continue;
            std::cout << std::left << std::setw(16) << pl.dir.name 
                      << pl.mhead.width << "x" << std::setw(10) << pl.mhead.height 
                      << pl.dir.diskSize << "\n";
        }
    }
    return ExitCode::Success;
}

ExitCode WadArchive::Extract(const AppConfig& config) {
    std::ifstream in(fs::path(config.input_path), std::ios::binary);
    if (!in) { 
        if (!config.quiet) std::cerr << "Fatal: Cannot open WAD: " << config.input_path << "\n"; 
        return ExitCode::FatalError; 
    }

    if (!fs::exists(config.output_path)) {
        fs::create_directories(config.output_path);
    }

    WadHeader header;
    in.read((char*)&header, sizeof(WadHeader));
    if (std::strncmp(header.signature, "WAD2", 4) != 0 && std::strncmp(header.signature, "WAD3", 4) != 0) {
        if (!config.quiet) std::cerr << "Fatal: Invalid WAD signature.\n"; 
        return ExitCode::FatalError;
    }

    in.seekg(header.infoTableOffset);
    std::vector<WadDirEntry> directory(header.numLumps);
    in.read((char*)directory.data(), header.numLumps * sizeof(WadDirEntry));

    struct ExtractedRecord {
        std::string name;
        std::string file_path;
        uint32_t width;
        uint32_t height;
        std::string format;
    };

    std::vector<ExtractedRecord> extracted;
    int ext_count = 0;

    for (const auto& dir : directory) {
        if (dir.type != 0x43) continue;
        in.seekg(dir.offset);
        MipTexHeader mhead;
        in.read((char*)&mhead, sizeof(MipTexHeader));

        MipTexData tex;
        tex.width = mhead.width;
        tex.height = mhead.height;
        char clean_name[17] = {0};
        std::memcpy(clean_name, mhead.name, 16);
        tex.name = clean_name;
        
        in.seekg(dir.offset + mhead.offsets[0]);
        tex.mip[0].resize(tex.width * tex.height);
        in.read((char*)tex.mip[0].data(), tex.mip[0].size());

        uint32_t last_mip_size = (tex.width / 8) * (tex.height / 8);
        in.seekg(dir.offset + mhead.offsets[3] + last_mip_size);

        uint16_t palSize = 0;
        in.read((char*)&palSize, sizeof(uint16_t));
        if (palSize == 0) palSize = 256;

        tex.palette.resize(256);
        in.read((char*)tex.palette.data(), std::min((uint16_t)256, palSize) * sizeof(ColorRGB));

        std::string ext = config.extract_bmp ? ".bmp" : ".png";
        std::string norm_name = NormalizeName(tex.name);
        fs::path out_file = fs::path(config.output_path) / (norm_name + ext);
        if (ImageProcessor::ExportImage(out_file.string(), tex, config.extract_bmp)) {
            ext_count++;
            extracted.push_back({tex.name, out_file.string(), tex.width, tex.height, config.extract_bmp ? "bmp" : "png"});
            if (config.verbose && !config.json_output) {
                std::cerr << "Extracted: " << out_file.string() << "\n";
            }
        }
    }

    if (config.json_output) {
        std::cout << "{\n"
                  << "  \"status\": \"success\",\n"
                  << "  \"exit_code\": 0,\n"
                  << "  \"archive\": \"" << config.input_path << "\",\n"
                  << "  \"output_directory\": \"" << config.output_path << "\",\n"
                  << "  \"total_extracted\": " << ext_count << ",\n"
                  << "  \"textures\": [\n";
        for (size_t i = 0; i < extracted.size(); ++i) {
            std::cout << "    {\n"
                      << "      \"name\": \"" << extracted[i].name << "\",\n"
                      << "      \"file\": \"" << extracted[i].file_path << "\",\n"
                      << "      \"width\": " << extracted[i].width << ",\n"
                      << "      \"height\": " << extracted[i].height << ",\n"
                      << "      \"format\": \"" << extracted[i].format << "\"\n"
                      << "    }" << (i + 1 < extracted.size() ? "," : "") << "\n";
        }
        std::cout << "  ]\n}\n";
    } else if (!config.quiet) {
        std::cout << "Successfully extracted " << ext_count << " textures.\n";
    }

    return ExitCode::Success;
}

// ---------------------------------------------------------------------------
// Public C++ SDK Implementation
// ---------------------------------------------------------------------------

std::vector<uint8_t> PackWadToMemory(const std::vector<MipTex>& textures, WadFormat format) {
    std::vector<uint8_t> buffer;
    if (textures.empty()) return buffer;

    WadHeader header;
    std::memcpy(header.signature, (format == WadFormat::WAD2 ? "WAD2" : "WAD3"), 4);
    header.numLumps = (uint32_t)textures.size();
    header.infoTableOffset = 0;

    auto append_bytes = [&](const void* ptr, size_t size) {
        const auto* b = static_cast<const uint8_t*>(ptr);
        buffer.insert(buffer.end(), b, b + size);
    };

    append_bytes(&header, sizeof(WadHeader));

    std::vector<WadDirEntry> directory;
    for (const auto& tex : textures) {
        WadDirEntry dir{};
        dir.offset = (uint32_t)buffer.size();
        dir.type = 0x43;
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

        append_bytes(&mhead, sizeof(MipTexHeader));
        for (int i = 0; i < 4; ++i) {
            append_bytes(tex.mip[i].data(), tex.mip[i].size());
        }

        uint16_t palSize = 256;
        append_bytes(&palSize, sizeof(uint16_t));
        for (int i = 0; i < 256; ++i) {
            ColorRGB col = (i < (int)tex.palette.size()) ? tex.palette[i] : ColorRGB{0, 0, 0};
            append_bytes(&col, sizeof(ColorRGB));
        }

        dir.size = dir.diskSize = (uint32_t)buffer.size() - dir.offset;
        directory.push_back(dir);
    }

    uint32_t table_offset = (uint32_t)buffer.size();
    for (const auto& dir : directory) {
        append_bytes(&dir, sizeof(WadDirEntry));
    }

    // Fix header table offset
    std::memcpy(buffer.data() + 8, &table_offset, sizeof(uint32_t));
    return buffer;
}

std::optional<std::vector<MipTex>> UnpackWadFromMemory(const uint8_t* wad_data, size_t size, std::string* error_msg) {
    if (!wad_data || size < sizeof(WadHeader)) {
        if (error_msg) *error_msg = "Buffer is too small for WAD header.";
        return std::nullopt;
    }

    WadHeader header;
    std::memcpy(&header, wad_data, sizeof(WadHeader));
    if (std::strncmp(header.signature, "WAD2", 4) != 0 && std::strncmp(header.signature, "WAD3", 4) != 0) {
        if (error_msg) *error_msg = "Invalid WAD signature.";
        return std::nullopt;
    }

    if (header.infoTableOffset + header.numLumps * sizeof(WadDirEntry) > size) {
        if (error_msg) *error_msg = "Corrupted lump directory offset.";
        return std::nullopt;
    }

    std::vector<WadDirEntry> directory(header.numLumps);
    std::memcpy(directory.data(), wad_data + header.infoTableOffset, header.numLumps * sizeof(WadDirEntry));

    std::vector<MipTex> textures;
    for (const auto& dir : directory) {
        if (dir.type != 0x43) continue;
        if (dir.offset + sizeof(MipTexHeader) > size) continue;

        MipTexHeader mhead;
        std::memcpy(&mhead, wad_data + dir.offset, sizeof(MipTexHeader));

        MipTex tex;
        char name_buf[17] = {0};
        std::memcpy(name_buf, mhead.name, 16);
        tex.name = name_buf;
        tex.width = mhead.width;
        tex.height = mhead.height;
        tex.has_transparency = (!tex.name.empty() && tex.name[0] == '{');

        for (int i = 0; i < 4; ++i) {
            uint32_t mip_w = std::max(1u, tex.width >> i);
            uint32_t mip_h = std::max(1u, tex.height >> i);
            size_t mip_len = mip_w * mip_h;
            if (dir.offset + mhead.offsets[i] + mip_len <= size) {
                tex.mip[i].resize(mip_len);
                std::memcpy(tex.mip[i].data(), wad_data + dir.offset + mhead.offsets[i], mip_len);
            }
        }

        uint32_t last_mip_size = (tex.width / 8) * (tex.height / 8);
        size_t pal_start = dir.offset + mhead.offsets[3] + last_mip_size;
        if (pal_start + 2 + 768 <= size) {
            uint16_t palSize = 256;
            std::memcpy(&palSize, wad_data + pal_start, sizeof(uint16_t));
            tex.palette.resize(256);
            std::memcpy(tex.palette.data(), wad_data + pal_start + 2, std::min((size_t)palSize, (size_t)256) * sizeof(ColorRGB));
        }

        textures.push_back(tex);
    }
    return textures;
}

std::optional<WadInfo> InspectWadFromMemory(const uint8_t* wad_data, size_t size, std::string* error_msg) {
    if (!wad_data || size < sizeof(WadHeader)) {
        if (error_msg) *error_msg = "Buffer is too small for WAD header.";
        return std::nullopt;
    }

    WadHeader header;
    std::memcpy(&header, wad_data, sizeof(WadHeader));
    if (std::strncmp(header.signature, "WAD2", 4) != 0 && std::strncmp(header.signature, "WAD3", 4) != 0) {
        if (error_msg) *error_msg = "Invalid WAD signature.";
        return std::nullopt;
    }

    if (header.infoTableOffset + header.numLumps * sizeof(WadDirEntry) > size) {
        if (error_msg) *error_msg = "Corrupted lump directory offset.";
        return std::nullopt;
    }

    WadInfo info;
    info.format = (std::strncmp(header.signature, "WAD2", 4) == 0 ? WadFormat::WAD2 : WadFormat::WAD3);
    info.num_lumps = header.numLumps;
    info.info_table_offset = header.infoTableOffset;

    std::vector<WadDirEntry> directory(header.numLumps);
    std::memcpy(directory.data(), wad_data + header.infoTableOffset, header.numLumps * sizeof(WadDirEntry));

    for (const auto& dir : directory) {
        LumpInfo li;
        char name_buf[17] = {0};
        std::memcpy(name_buf, dir.name, 16);
        li.name = name_buf;
        li.offset = dir.offset;
        li.disk_size = dir.diskSize;
        li.size = dir.size;
        li.type = (uint8_t)dir.type;
        li.has_transparency = (!li.name.empty() && li.name[0] == '{');

        if (dir.type == 0x43 && dir.offset + sizeof(MipTexHeader) <= size) {
            MipTexHeader mhead;
            std::memcpy(&mhead, wad_data + dir.offset, sizeof(MipTexHeader));
            li.width = mhead.width;
            li.height = mhead.height;
            for (int i = 0; i < 4; ++i) li.mip_offsets[i] = mhead.offsets[i];
            li.palette_offset = mhead.offsets[3] + (mhead.width/8)*(mhead.height/8) + 2;
        }
        info.lumps.push_back(li);
    }
    return info;
}

std::optional<WadInfo> InspectWadFile(const std::string& input_wad, std::string* error_msg) {
    FILE* f = utils::OpenFilePortable(input_wad, "rb");
    if (!f) {
        if (error_msg) *error_msg = "Could not open file: " + input_wad;
        return std::nullopt;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        if (error_msg) *error_msg = "Empty file: " + input_wad;
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(size);
    if (fread(buffer.data(), 1, size, f) != (size_t)size) {
        fclose(f);
        if (error_msg) *error_msg = "Failed reading file data.";
        return std::nullopt;
    }
    fclose(f);

    return InspectWadFromMemory(buffer.data(), buffer.size(), error_msg);
}

BuildResult BuildWadFromDirectory(const std::string& input_dir, const std::string& output_wad,
                                  WadFormat format, const TextureOptions& opts, bool allow_overwrite) {
    AppConfig config;
    config.command = "build";
    config.input_path = input_dir;
    config.output_path = output_wad;
    config.wad2 = (format == WadFormat::WAD2);
    config.allow_overwrite = allow_overwrite;
    config.disable_dither = opts.disable_dither;
    config.max_size = opts.max_size;
    config.align = opts.align;
    config.stretch = opts.stretch;
    config.pad_r = opts.key_color.r;
    config.pad_g = opts.key_color.g;
    config.pad_b = opts.key_color.b;
    config.quiet = true;

    ExitCode ec = WadArchive::Build(config);
    BuildResult res;
    res.success = (ec == ExitCode::Success || ec == ExitCode::PartialSuccess);
    return res;
}

bool ExtractWadToDirectory(const std::string& input_wad, const std::string& output_dir,
                           bool as_bmp, std::string* /*error_msg*/, std::vector<std::string>* /*extracted_files*/) {
    AppConfig config;
    config.command = "extract";
    config.input_path = input_wad;
    config.output_path = output_dir;
    config.extract_bmp = as_bmp;
    config.quiet = true;

    ExitCode ec = WadArchive::Extract(config);
    return ec == ExitCode::Success;
}

std::optional<MipTex> ProcessImageBuffer(const uint8_t* img_bytes, size_t size,
                                        const std::string& lump_name, const TextureOptions& opts) {
    AppConfig config;
    config.max_size = opts.max_size;
    config.disable_dither = opts.disable_dither;
    config.stretch = opts.stretch;
    config.align = opts.align;
    config.pad_r = opts.key_color.r;
    config.pad_g = opts.key_color.g;
    config.pad_b = opts.key_color.b;
    config.quiet = true;

    MipTexData data;
    if (!ImageProcessor::ProcessBuffer(img_bytes, size, lump_name, config, data)) {
        return std::nullopt;
    }

    MipTex tex;
    tex.name = data.name;
    tex.width = data.width;
    tex.height = data.height;
    for (int i = 0; i < 4; ++i) tex.mip[i] = std::move(data.mip[i]);
    tex.palette = std::move(data.palette);
    tex.has_transparency = data.has_transparency;
    return tex;
}

} // namespace fastwad