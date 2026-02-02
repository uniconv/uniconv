#pragma once

#include "core/types.h"
#include "core/plugin_manager.h"
#include <functional>
#include <memory>
#include <vector>

namespace uniconv::core {

// Progress callback type
using ProgressCallback = std::function<void(const std::string& file, size_t current, size_t total)>;

// Batch result
struct BatchResult {
    std::vector<ETLResult> results;
    size_t succeeded = 0;
    size_t failed = 0;
    size_t skipped = 0;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["succeeded"] = succeeded;
        j["failed"] = failed;
        j["skipped"] = skipped;
        j["total"] = results.size();
        j["results"] = nlohmann::json::array();
        for (const auto& r : results) {
            j["results"].push_back(r.to_json());
        }
        return j;
    }
};

class Engine {
public:
    Engine();
    explicit Engine(std::shared_ptr<PluginManager> plugin_manager);
    ~Engine() = default;

    // Execute a single ETL request
    ETLResult execute(const ETLRequest& request);

    // Execute batch of ETL requests
    BatchResult execute_batch(
        const std::vector<ETLRequest>& requests,
        const ProgressCallback& progress = nullptr
    );

    // Create ETL request from sources with common options
    std::vector<ETLRequest> create_requests(
        ETLType etl,
        const std::vector<std::filesystem::path>& sources,
        const std::string& target,
        const std::optional<std::string>& plugin,
        const CoreOptions& options,
        const std::vector<std::string>& plugin_options
    );

    // Get file information
    FileInfo get_file_info(const std::filesystem::path& path) const;

    // Get plugin manager
    PluginManager& plugin_manager() { return *plugin_manager_; }
    const PluginManager& plugin_manager() const { return *plugin_manager_; }

private:
    std::shared_ptr<PluginManager> plugin_manager_;

    // Resolve output path for a request
    std::filesystem::path resolve_output_path(
        const ETLRequest& request,
        const std::string& target_format
    ) const;

    // Check if output file exists and handle force option
    bool should_skip_existing(
        const std::filesystem::path& output,
        const CoreOptions& options
    ) const;
};

} // namespace uniconv::core
