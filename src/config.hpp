#pragma once
#include <string>
#include <vector>
#include <cstdint>

#define DEFAULT_CONFIG_FILE "fastwad.conf"

enum class ExitCode : int {
    Success = 0,
    PartialSuccess = 1, // Warnings or some files failed but archive created
    FatalError = 2,    // IO errors, invalid WAD, etc.
    CliError = 3       // Invalid arguments
};

struct AppConfig {
    std::string command;
    std::string input_path;
    std::string output_path;
    std::string config_file_to_save;

    // Build options
    bool wad2 = false;            // Default is WAD3
    bool allow_overwrite = false;
    bool disable_dither = false;
    int max_size = 256;           // 256, 512, 1024
    std::string align = "center"; // center, top, bottom, left, right
    uint8_t pad_r = 0;
    uint8_t pad_g = 0;
    uint8_t pad_b = 255;
    bool stretch = false;         // If true, stretch to fit max_size (not default)

    // Extract options
    bool extract_bmp = false;     // Default is PNG

    // General
    bool verbose = false;
    bool quiet = false;
};
