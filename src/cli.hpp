#pragma once

#include "config.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>

namespace fastwad {

class CommandLineParser {
public:
    static AppConfig Parse(int argc, char** argv) {
        AppConfig config;
        std::map<std::string, std::string> args;
        
        // Load default config file first if it exists
        LoadConfigFile(DEFAULT_CONFIG_FILE, args);

        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") {
                args["json"] = "true";
            } else if (arg == "--quiet" || arg == "-q") {
                args["quiet"] = "true";
            } else if (arg == "--verbose" || arg == "-v") {
                args["verbose"] = "true";
            } else if (arg.find("--save-config=") == 0) {
                config.config_file_to_save = arg.substr(14);
            } else if (arg.find("config=") == 0) {
                LoadConfigFile(arg.substr(7), args);
            } else if (arg.find('=') != std::string::npos) {
                size_t pos = arg.find('=');
                args[arg.substr(0, pos)] = arg.substr(pos + 1);
            } else if (arg.size() > 2 && arg.substr(0, 2) == "--") {
                std::string key = arg.substr(2);
                if (i + 1 < argc && argv[i+1][0] != '-') {
                    args[key] = argv[++i];
                } else {
                    args[key] = "true";
                }
            } else {
                positional.push_back(arg);
            }
        }

        if (!positional.empty()) {
            config.command = positional[0];
            if (positional.size() > 1) config.input_path = positional[1];
            if (positional.size() > 2) config.output_path = positional[2];
        }

        auto apply_args = [&](const std::map<std::string, std::string>& m) {
            if (m.count("wad2")) config.wad2 = (m.at("wad2") == "true");
            if (m.count("allow_overwrite")) config.allow_overwrite = (m.at("allow_overwrite") == "true");
            if (m.count("disable_dither")) config.disable_dither = (m.at("disable_dither") == "true");
            if (m.count("max_size")) config.max_size = std::stoi(m.at("max_size"));
            if (m.count("align")) config.align = m.at("align");
            if (m.count("format")) config.extract_bmp = (m.at("format") == "bmp");
            if (m.count("verbose")) config.verbose = (m.at("verbose") == "true");
            if (m.count("quiet")) config.quiet = (m.at("quiet") == "true");
            if (m.count("json")) config.json_output = (m.at("json") == "true");
            if (m.count("stretch")) config.stretch = (m.at("stretch") == "true");
            if (m.count("pad_r")) config.pad_r = (uint8_t)std::stoi(m.at("pad_r"));
            if (m.count("pad_g")) config.pad_g = (uint8_t)std::stoi(m.at("pad_g"));
            if (m.count("pad_b")) config.pad_b = (uint8_t)std::stoi(m.at("pad_b"));
        };

        apply_args(args);

        if (!config.config_file_to_save.empty()) {
            SaveConfigFile(config.config_file_to_save, config);
        }

        return config;
    }

    static void PrintHelp() {
        std::cout << "fastwad - GoldSrc & Quake WAD Archive Builder & SDK\n\n"
                  << "Usage:\n"
                  << "  fastwad build <input_dir> <output.wad> [options]\n"
                  << "  fastwad list <input.wad> [options]\n"
                  << "  fastwad extract <input.wad> <output_dir> [options]\n"
                  << "  fastwad save-config [options]\n"
                  << "  fastwad reset-config\n\n"
                  << "Options:\n"
                  << "  --json                  Output structured machine-readable JSON to stdout\n"
                  << "  wad2=true               Generate WAD2 (Quake 1) instead of WAD3 (Half-Life)\n"
                  << "  allow_overwrite=true    Overwrite existing output file (default: false)\n"
                  << "  disable_dither=true     Disable Floyd-Steinberg dithering\n"
                  << "  max_size=512            Max texture dimension: 256, 512, 1024 (default: 256)\n"
                  << "  align=center            Padding alignment: center, top, bottom, left, right\n"
                  << "  stretch=true            Stretch to max_size instead of aspect contain\n"
                  << "  pad_r=0 pad_g=0 pad_b=255 Padding/transparency key color (default: 0 0 255)\n"
                  << "  format=bmp              Extraction image format: png or bmp (default: png)\n"
                  << "  config=<path>           Load settings from a specific configuration file\n"
                  << "  --save-config=<path>    Export current CLI settings to a file\n"
                  << "  verbose=true, -v        Enable verbose diagnostic logging (on stderr)\n"
                  << "  quiet=true, -q          Suppress standard non-JSON output\n";
    }

    static void SaveConfigFile(const std::string& path, const AppConfig& config) {
        std::ofstream out(path);
        if (!out) return;
        out << "# fastwad configuration\n"
            << "wad2=" << (config.wad2 ? "true" : "false") << "\n"
            << "allow_overwrite=" << (config.allow_overwrite ? "true" : "false") << "\n"
            << "disable_dither=" << (config.disable_dither ? "true" : "false") << "\n"
            << "max_size=" << config.max_size << "\n"
            << "align=" << config.align << "\n"
            << "stretch=" << (config.stretch ? "true" : "false") << "\n"
            << "pad_r=" << (int)config.pad_r << "\n"
            << "pad_g=" << (int)config.pad_g << "\n"
            << "pad_b=" << (int)config.pad_b << "\n"
            << "format=" << (config.extract_bmp ? "bmp" : "png") << "\n"
            << "verbose=" << (config.verbose ? "true" : "false") << "\n"
            << "quiet=" << (config.quiet ? "true" : "false") << "\n";
    }

private:
    static void LoadConfigFile(const std::string& path, std::map<std::string, std::string>& args) {
        std::ifstream in(path);
        if (!in) return;
        std::string line;
        while (std::getline(in, line)) {
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            if (line.empty() || line[0] == '#') continue;
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                args[line.substr(0, pos)] = line.substr(pos + 1);
            }
        }
    }
};

} // namespace fastwad
