#pragma once

#include "config.hpp"
#include "image_processor.hpp"
#include "fastwad/fastwad.hpp"
#include <vector>
#include <string>
#include <optional>

namespace fastwad {

class WadArchive {
public:
    static ExitCode Build(const AppConfig& config);
    static ExitCode List(const AppConfig& config);
    static ExitCode Extract(const AppConfig& config);

    static std::string NormalizeName(const std::string& raw);
};

} // namespace fastwad