#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace uniconv::core
{

    // Data types for plugin input/output
    enum class DataType
    {
        File,   // Generic file (path-based)
        Image,  // Image data
        Video,  // Video data
        Audio,  // Audio data
        Text,   // Text data
        Json,   // Structured JSON data
        Binary, // Binary blob
        Stream  // Stream data
    };

    // Convert DataType to string
    inline std::string data_type_to_string(DataType type)
    {
        switch (type)
        {
        case DataType::File:
            return "file";
        case DataType::Image:
            return "image";
        case DataType::Video:
            return "video";
        case DataType::Audio:
            return "audio";
        case DataType::Text:
            return "text";
        case DataType::Json:
            return "json";
        case DataType::Binary:
            return "binary";
        case DataType::Stream:
            return "stream";
        }
        return "unknown";
    }

    // Parse DataType from string
    inline std::optional<DataType> data_type_from_string(const std::string &s)
    {
        if (s == "file")
            return DataType::File;
        if (s == "image")
            return DataType::Image;
        if (s == "video")
            return DataType::Video;
        if (s == "audio")
            return DataType::Audio;
        if (s == "text")
            return DataType::Text;
        if (s == "json")
            return DataType::Json;
        if (s == "binary")
            return DataType::Binary;
        if (s == "stream")
            return DataType::Stream;
        return std::nullopt;
    }

    // Result status
    enum class ResultStatus
    {
        Success,
        Error,
        Skipped
    };

    inline std::string result_status_to_string(ResultStatus status)
    {
        switch (status)
        {
        case ResultStatus::Success:
            return "success";
        case ResultStatus::Error:
            return "error";
        case ResultStatus::Skipped:
            return "skipped";
        }
        return "unknown";
    }

    // File type category
    enum class FileCategory
    {
        Image,
        Video,
        Audio,
        Document,
        Unknown
    };

    inline std::string file_category_to_string(FileCategory cat)
    {
        switch (cat)
        {
        case FileCategory::Image:
            return "image";
        case FileCategory::Video:
            return "video";
        case FileCategory::Audio:
            return "audio";
        case FileCategory::Document:
            return "document";
        case FileCategory::Unknown:
            return "unknown";
        }
        return "unknown";
    }

    // File information
    struct FileInfo
    {
        std::filesystem::path path;
        std::string format; // "heic", "jpg", "mp4", etc.
        std::string mime_type;
        FileCategory category = FileCategory::Unknown;
        size_t size = 0;
        std::optional<std::pair<int, int>> dimensions; // For images/videos (width, height)
        std::optional<double> duration;                // For audio/video in seconds

        // JSON serialization
        nlohmann::json to_json() const
        {
            nlohmann::json j;
            j["path"] = path.string();
            j["format"] = format;
            j["mime_type"] = mime_type;
            j["category"] = file_category_to_string(category);
            j["size"] = size;
            if (dimensions)
            {
                j["dimensions"] = {
                    {"width", dimensions->first},
                    {"height", dimensions->second}};
            }
            if (duration)
            {
                j["duration"] = *duration;
            }
            return j;
        }
    };

    // Core options (shared across plugins)
    struct CoreOptions
    {
        std::optional<std::filesystem::path> output;
        bool force = false;       // Overwrite existing
        bool json_output = false; // Output as JSON
        bool verbose = false;     // Verbose output
        bool quiet = false;       // Suppress output
        bool dry_run = false;     // Don't actually execute
        bool recursive = false;   // Process directories recursively

        nlohmann::json to_json() const
        {
            nlohmann::json j;
            if (output)
                j["output"] = output->string();
            j["force"] = force;
            j["json_output"] = json_output;
            j["verbose"] = verbose;
            j["quiet"] = quiet;
            j["dry_run"] = dry_run;
            j["recursive"] = recursive;
            return j;
        }
    };

    // Request structure for plugin execution
    struct Request
    {
        std::filesystem::path source;
        std::string target;                // "jpg", "faces", "gdrive", etc.
        std::optional<std::string> plugin; // Optional explicit plugin group
        CoreOptions core_options;
        std::vector<std::string> plugin_options; // Options after --

        nlohmann::json to_json() const
        {
            nlohmann::json j;
            j["source"] = source.string();
            j["target"] = target;
            if (plugin)
                j["plugin"] = *plugin;
            j["core_options"] = core_options.to_json();
            j["plugin_options"] = plugin_options;
            return j;
        }
    };

    // Result structure for plugin execution
    struct Result
    {
        ResultStatus status = ResultStatus::Error;
        std::string target;
        std::string plugin_used;
        std::filesystem::path input;
        std::optional<std::filesystem::path> output;
        size_t input_size = 0;
        std::optional<size_t> output_size;
        std::optional<std::string> error;
        nlohmann::json extra; // Plugin-specific result data

        nlohmann::json to_json() const
        {
            nlohmann::json j;
            j["success"] = (status == ResultStatus::Success);
            j["status"] = result_status_to_string(status);
            j["target"] = target;
            j["plugin"] = plugin_used;
            j["input"] = input.string();
            j["input_size"] = input_size;
            if (output)
            {
                j["output"] = output->string();
            }
            if (output_size)
            {
                j["output_size"] = *output_size;
                if (input_size > 0)
                {
                    j["size_ratio"] = static_cast<double>(*output_size) / static_cast<double>(input_size);
                }
            }
            if (error)
            {
                j["error"] = *error;
            }
            if (!extra.empty())
            {
                j["extra"] = extra;
            }
            return j;
        }

        // Helper to create success result
        static Result success(const std::string &target,
                              const std::string &plugin,
                              const std::filesystem::path &input,
                              const std::filesystem::path &output,
                              size_t in_size, size_t out_size)
        {
            Result r;
            r.status = ResultStatus::Success;
            r.target = target;
            r.plugin_used = plugin;
            r.input = input;
            r.output = output;
            r.input_size = in_size;
            r.output_size = out_size;
            return r;
        }

        // Helper to create error result
        static Result failure(const std::string &target,
                              const std::filesystem::path &input,
                              const std::string &error_msg)
        {
            Result r;
            r.status = ResultStatus::Error;
            r.target = target;
            r.input = input;
            r.error = error_msg;
            return r;
        }
    };

    // Plugin information
    struct PluginInfo
    {
        std::string id;                         // Full ID: "image-core"
        std::string group;                      // Group name: "image-core"
        std::vector<std::string> targets;       // Supported targets
        std::vector<std::string> input_formats; // Supported input formats
        std::string version;
        std::string description;
        bool builtin = false;

        // Data type information
        std::vector<DataType> input_types;  // Supported input data types
        std::vector<DataType> output_types; // Supported output data types

        nlohmann::json to_json() const
        {
            auto j = nlohmann::json{
                {"id", id},
                {"group", group},
                {"targets", targets},
                {"input_formats", input_formats},
                {"version", version},
                {"description", description},
                {"builtin", builtin}};

            // Add data type info
            if (!input_types.empty())
            {
                std::vector<std::string> input_strs;
                for (auto t : input_types)
                    input_strs.push_back(data_type_to_string(t));
                j["input_types"] = input_strs;
            }
            if (!output_types.empty())
            {
                std::vector<std::string> output_strs;
                for (auto t : output_types)
                    output_strs.push_back(data_type_to_string(t));
                j["output_types"] = output_strs;
            }

            return j;
        }
    };

    // Preset structure
    struct Preset
    {
        std::string name;
        std::string description;
        std::string target;
        std::optional<std::string> plugin;
        CoreOptions core_options;
        std::vector<std::string> plugin_options;

        nlohmann::json to_json() const
        {
            nlohmann::json j;
            j["name"] = name;
            j["description"] = description;
            j["target"] = target;
            if (plugin)
                j["plugin"] = *plugin;
            j["core_options"] = core_options.to_json();
            j["plugin_options"] = plugin_options;
            return j;
        }

        static Preset from_json(const nlohmann::json &j)
        {
            Preset p;
            p.name = j.at("name").get<std::string>();
            p.description = j.value("description", "");
            p.target = j.at("target").get<std::string>();

            if (j.contains("plugin"))
            {
                p.plugin = j.at("plugin").get<std::string>();
            }

            if (j.contains("core_options"))
            {
                auto &co = j.at("core_options");
                if (co.contains("output"))
                    p.core_options.output = co.at("output").get<std::string>();
                p.core_options.force = co.value("force", false);
                p.core_options.verbose = co.value("verbose", false);
                p.core_options.quiet = co.value("quiet", false);
                p.core_options.dry_run = co.value("dry_run", false);
                p.core_options.recursive = co.value("recursive", false);
            }

            if (j.contains("plugin_options"))
            {
                p.plugin_options = j.at("plugin_options").get<std::vector<std::string>>();
            }

            return p;
        }
    };

} // namespace uniconv::core
