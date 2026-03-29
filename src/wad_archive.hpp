#pragma once
#include "config.hpp"
#include "image_processor.hpp"
#include <vector>

class WadArchive {
public:
    static ExitCode Build(const AppConfig& config);
    static ExitCode List(const AppConfig& config);
    static ExitCode Extract(const AppConfig& config);

private:
    static std::string NormalizeName(const std::string& raw);
};