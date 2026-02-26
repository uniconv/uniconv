#pragma once

#include "plugin_manifest.h"
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace uniconv::core
{

    struct DependencyCheckResult
    {
        bool satisfied = false;
        std::string message;      // human-readable status
        std::string install_hint; // e.g., "pip install Pillow>=9.0"
    };

    class DependencyChecker
    {
    public:
        // Check a single dependency
        DependencyCheckResult check(const Dependency &dep) const;

        // Check all dependencies, return results
        std::vector<std::pair<Dependency, DependencyCheckResult>>
        check_all(const std::vector<Dependency> &deps) const;

        // Check all dependencies using a plugin's virtualenv for Python checks
        std::vector<std::pair<Dependency, DependencyCheckResult>>
        check_all(const std::vector<Dependency> &deps,
                  const std::filesystem::path &python_bin) const;

        // Print warnings for unsatisfied dependencies to stderr
        static void print_warnings(
            const std::vector<std::pair<Dependency, DependencyCheckResult>> &results);

    private:
        DependencyCheckResult check_custom(const Dependency &dep) const;
        DependencyCheckResult check_system(const Dependency &dep) const;
        DependencyCheckResult check_python(const Dependency &dep,
#ifdef _WIN32
                                           const std::string &python_cmd = "python") const;
#else
                                           const std::string &python_cmd = "python3") const;
#endif
        DependencyCheckResult check_node(const Dependency &dep) const;

        // Run a command and return exit code
        static int run_check_command(const std::string &command,
                                     const std::vector<std::string> &args);

        // Run a command and capture stdout
        static std::string run_capture_command(const std::string &command,
                                               const std::vector<std::string> &args);
    };

} // namespace uniconv::core
