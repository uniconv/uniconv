#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace uniconv::core
{

    // File event types
    enum class FileEvent
    {
        Created,
        Modified
    };

    // Callback signature: (path, event) -> void
    using WatchCallback = std::function<void(const std::filesystem::path &, FileEvent)>;

    // Simple polling-based directory watcher
    class Watcher
    {
    public:
        explicit Watcher(std::chrono::milliseconds poll_interval = std::chrono::milliseconds(1000));
        ~Watcher();

        // Set the callback for file events
        void set_callback(WatchCallback callback);

        // Start watching a directory (blocking until stop() is called)
        // Returns false if directory doesn't exist or isn't a directory
        bool watch(const std::filesystem::path &directory, bool recursive = false);

        // Stop watching (call from signal handler or another thread)
        void stop();

        // Check if currently watching
        bool is_running() const;

        // Get list of supported extensions to filter (empty = all files)
        void set_extensions(const std::set<std::string> &extensions);

    private:
        // Scan directory and return files with their modification times
        std::map<std::filesystem::path, std::filesystem::file_time_type>
        scan_directory(const std::filesystem::path &directory, bool recursive);

        // Check if file matches extension filter
        bool matches_filter(const std::filesystem::path &path) const;

        std::chrono::milliseconds poll_interval_;
        std::atomic<bool> running_{false};
        WatchCallback callback_;
        std::set<std::string> extensions_;
    };

} // namespace uniconv::core
