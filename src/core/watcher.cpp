#include "watcher.h"
#include <thread>

namespace uniconv::core
{

    Watcher::Watcher(std::chrono::milliseconds poll_interval)
        : poll_interval_(poll_interval)
    {
    }

    Watcher::~Watcher()
    {
        stop();
    }

    void Watcher::set_callback(WatchCallback callback)
    {
        callback_ = std::move(callback);
    }

    void Watcher::set_extensions(const std::set<std::string> &extensions)
    {
        extensions_ = extensions;
    }

    bool Watcher::matches_filter(const std::filesystem::path &path) const
    {
        if (extensions_.empty())
        {
            return true; // No filter, accept all
        }

        std::string ext = path.extension().string();
        if (!ext.empty() && ext[0] == '.')
        {
            ext = ext.substr(1); // Remove leading dot
        }

        // Convert to lowercase for comparison
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        return extensions_.count(ext) > 0;
    }

    std::map<std::filesystem::path, std::filesystem::file_time_type>
    Watcher::scan_directory(const std::filesystem::path &directory, bool recursive)
    {
        std::map<std::filesystem::path, std::filesystem::file_time_type> files;

        try
        {
            if (recursive)
            {
                for (const auto &entry : std::filesystem::recursive_directory_iterator(directory))
                {
                    if (entry.is_regular_file() && matches_filter(entry.path()))
                    {
                        files[entry.path()] = entry.last_write_time();
                    }
                }
            }
            else
            {
                for (const auto &entry : std::filesystem::directory_iterator(directory))
                {
                    if (entry.is_regular_file() && matches_filter(entry.path()))
                    {
                        files[entry.path()] = entry.last_write_time();
                    }
                }
            }
        }
        catch (const std::filesystem::filesystem_error &)
        {
            // Ignore permission errors etc.
        }

        return files;
    }

    bool Watcher::watch(const std::filesystem::path &directory, bool recursive)
    {
        if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory))
        {
            return false;
        }

        running_ = true;

        // Initial scan to get baseline
        auto previous = scan_directory(directory, recursive);

        while (running_)
        {
            std::this_thread::sleep_for(poll_interval_);

            if (!running_)
                break;

            auto current = scan_directory(directory, recursive);

            // Check for new or modified files
            for (const auto &[path, mtime] : current)
            {
                auto it = previous.find(path);
                if (it == previous.end())
                {
                    // New file
                    if (callback_)
                    {
                        callback_(path, FileEvent::Created);
                    }
                }
                else if (it->second != mtime)
                {
                    // Modified file
                    if (callback_)
                    {
                        callback_(path, FileEvent::Modified);
                    }
                }
            }

            previous = std::move(current);
        }

        return true;
    }

    void Watcher::stop()
    {
        running_ = false;
    }

    bool Watcher::is_running() const
    {
        return running_;
    }

} // namespace uniconv::core
