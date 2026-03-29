#include "cli.hpp"
#include "wad_archive.hpp"
#include <filesystem>

int main(int argc, char** argv) {
    if (argc < 2) {
        CommandLineParser::PrintHelp();
        return (int)ExitCode::CliError;
    }

    AppConfig config = CommandLineParser::Parse(argc, argv);

    if (config.command == "build" && !config.input_path.empty() && !config.output_path.empty()) {
        return (int)WadArchive::Build(config);
    } else if (config.command == "list" && !config.input_path.empty()) {
        return (int)WadArchive::List(config);
    } else if (config.command == "extract" && !config.input_path.empty() && !config.output_path.empty()) {
        return (int)WadArchive::Extract(config);
    } else if (config.command == "save-config") {
        CommandLineParser::SaveConfigFile(DEFAULT_CONFIG_FILE, config);
        std::cout << "Configuration saved to " << DEFAULT_CONFIG_FILE << "\n";
        return (int)ExitCode::Success;
    } else if (config.command == "reset-config") {
        if (std::filesystem::remove(DEFAULT_CONFIG_FILE)) {
            std::cout << "Configuration file " << DEFAULT_CONFIG_FILE << " deleted.\n";
        } else {
            std::cout << "No configuration file found to reset.\n";
        }
        return (int)ExitCode::Success;
    }

    std::cerr << "Error: Invalid command or missing arguments.\n\n";
    CommandLineParser::PrintHelp();
    return (int)ExitCode::CliError;
}