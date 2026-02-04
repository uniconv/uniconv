#pragma once

#include "cli/parser.h"

namespace uniconv::cli::commands {

class UpdateCommand {
public:
    int execute(const ParsedArgs& args);

private:
    // Fetch the latest release tag from GitHub
    std::string fetch_latest_version();

    // Download and install the new binary
    int perform_update(const std::string& latest_version);

    // Get the path of the currently running executable
    std::filesystem::path get_self_path();

    // Extract a .tar.gz archive into a directory (Unix)
    // or .zip archive (Windows)
    bool extract_archive(const std::filesystem::path& archive,
                         const std::filesystem::path& dest_dir);
};

} // namespace uniconv::cli::commands
