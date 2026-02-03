#include "pipeline_parser.h"
#include "utils/string_utils.h"
#include <cctype>

namespace uniconv::cli {

using namespace uniconv::utils;

// Check if args contain pipeline syntax (has '|' delimiter)
bool PipelineParser::is_pipeline_syntax(const std::vector<std::string>& args) {
    for (const auto& arg : args) {
        if (arg == "|") {
            return true;
        }
        // Check if any arg contains '|' (might be in quoted string or escaped)
        if (arg.find('|') != std::string::npos) {
            return true;
        }
    }
    return false;
}

// Parse from raw command line args
std::optional<PipelineParser::ParseResult> PipelineParser::parse_from_args(
    const std::vector<std::string>& args) {

    if (!is_pipeline_syntax(args)) {
        return std::nullopt;
    }

    // Find source file (first non-option argument)
    std::filesystem::path source;
    core::CoreOptions core_options;
    std::vector<std::string> pipeline_parts;

    bool found_source = false;
    bool after_pipe = false;

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];

        // Handle pipe delimiter
        if (arg == "|") {
            after_pipe = true;
            continue;
        }

        // Everything after '|' is part of pipeline
        if (after_pipe) {
            pipeline_parts.push_back(arg);
            continue;
        }

        // Core options before source
        if (starts_with(arg, "-")) {
            // Parse core options
            if (arg == "-o" || arg == "--output") {
                if (i + 1 < args.size()) {
                    core_options.output = args[++i];
                }
            } else if (arg == "-f" || arg == "--force") {
                core_options.force = true;
            } else if (arg == "--json") {
                core_options.json_output = true;
            } else if (arg == "--quiet") {
                core_options.quiet = true;
            } else if (arg == "--verbose") {
                core_options.verbose = true;
            } else if (arg == "--dry-run") {
                core_options.dry_run = true;
            }
            // Skip other options for now
            continue;
        }

        // First non-option arg is source
        if (!found_source) {
            source = arg;
            found_source = true;
        }
    }

    if (!found_source) {
        return ParseResult::fail("No source file specified");
    }

    if (pipeline_parts.empty()) {
        return ParseResult::fail("Pipeline syntax detected but no pipeline specified after '|'");
    }

    // Join pipeline parts back together
    std::string pipeline_str = join(pipeline_parts, " ");

    return parse(pipeline_str, source, core_options);
}

// Parse a pipeline string
PipelineParser::ParseResult PipelineParser::parse(
    const std::string& pipeline_str,
    const std::filesystem::path& source,
    const core::CoreOptions& core_options) {

    core::Pipeline pipeline;
    pipeline.source = source;
    pipeline.core_options = core_options;

    // Split by pipe to get stages
    auto stage_strings = split_stages(pipeline_str);

    if (stage_strings.empty()) {
        return ParseResult::fail("Empty pipeline");
    }

    // Parse each stage
    for (const auto& stage_str : stage_strings) {
        if (trim(stage_str).empty()) {
            return ParseResult::fail("Empty stage in pipeline");
        }

        // Split by comma to get parallel elements
        auto element_strings = split_elements(stage_str);

        core::PipelineStage stage;
        for (const auto& elem_str : element_strings) {
            if (trim(elem_str).empty()) {
                continue;
            }

            auto element = parse_element(elem_str);
            stage.elements.push_back(std::move(element));
        }

        if (stage.elements.empty()) {
            return ParseResult::fail("Stage has no elements");
        }

        pipeline.stages.push_back(std::move(stage));
    }

    // Validate pipeline structure
    auto validation = pipeline.validate();
    if (!validation.valid) {
        return ParseResult::fail(validation.error);
    }

    return ParseResult::ok(std::move(pipeline));
}

// Split by pipe, respecting quotes
std::vector<std::string> PipelineParser::split_stages(const std::string& input) {
    return split_respecting_quotes(input, '|');
}

// Split by comma, respecting quotes
std::vector<std::string> PipelineParser::split_elements(const std::string& stage) {
    return split_respecting_quotes(stage, ',');
}

// Split string by delimiter, respecting quotes
std::vector<std::string> PipelineParser::split_respecting_quotes(
    const std::string& input, char delimiter) {

    std::vector<std::string> result;
    std::string current;
    bool in_quotes = false;
    bool escaped = false;

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];

        if (escaped) {
            current += c;
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (c == '"' || c == '\'') {
            in_quotes = !in_quotes;
            current += c;
            continue;
        }

        if (c == delimiter && !in_quotes) {
            result.push_back(current);
            current.clear();
            continue;
        }

        current += c;
    }

    if (!current.empty()) {
        result.push_back(current);
    }

    return result;
}

// Parse a single element: "jpg@vips --quality 90"
core::StageElement PipelineParser::parse_element(const std::string& element_str) {
    core::StageElement element;

    // Tokenize the element string
    auto tokens = tokenize(trim(element_str));

    if (tokens.empty()) {
        return element;
    }

    // First token is target[@plugin]
    auto [target, plugin] = parse_target(tokens[0]);
    element.target = target;
    element.plugin = plugin;

    // Remaining tokens are options
    if (tokens.size() > 1) {
        std::vector<std::string> option_tokens(tokens.begin() + 1, tokens.end());
        auto [parsed_opts, raw_opts] = parse_options(option_tokens);
        element.options = std::move(parsed_opts);
        element.raw_options = std::move(raw_opts);
    }

    return element;
}

// Parse target[@plugin]
std::pair<std::string, std::optional<std::string>> PipelineParser::parse_target(
    const std::string& target_str) {

    auto pos = target_str.find('@');
    if (pos == std::string::npos) {
        return {target_str, std::nullopt};
    }

    std::string target = target_str.substr(0, pos);
    std::string plugin = target_str.substr(pos + 1);

    return {target, plugin};
}

// Parse options from tokens
std::pair<std::map<std::string, std::string>, std::vector<std::string>>
PipelineParser::parse_options(const std::vector<std::string>& tokens) {

    std::map<std::string, std::string> parsed;
    std::vector<std::string> raw;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];

        // Skip non-option tokens
        if (!starts_with(token, "-")) {
            raw.push_back(token);
            continue;
        }

        // Handle --key=value format
        auto eq_pos = token.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = token.substr(0, eq_pos);
            std::string value = token.substr(eq_pos + 1);

            // Remove quotes from value if present
            if (value.size() >= 2 &&
                ((value.front() == '"' && value.back() == '"') ||
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.size() - 2);
            }

            // Remove leading dashes from key
            while (!key.empty() && key[0] == '-') {
                key = key.substr(1);
            }

            parsed[key] = value;
            raw.push_back(token);
            continue;
        }

        // Handle --key value format
        std::string key = token;
        while (!key.empty() && key[0] == '-') {
            key = key.substr(1);
        }

        // Check if next token is a value (not another option)
        if (i + 1 < tokens.size() && !starts_with(tokens[i + 1], "-")) {
            std::string value = tokens[i + 1];

            // Remove quotes from value if present
            if (value.size() >= 2 &&
                ((value.front() == '"' && value.back() == '"') ||
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.size() - 2);
            }

            parsed[key] = value;
            raw.push_back(token);
            raw.push_back(tokens[i + 1]);
            ++i;  // Skip the value token
        } else {
            // Boolean flag (no value)
            parsed[key] = "true";
            raw.push_back(token);
        }
    }

    return {parsed, raw};
}

// Tokenize a string respecting quotes
std::vector<std::string> PipelineParser::tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;
    char quote_char = '\0';
    bool escaped = false;

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];

        if (escaped) {
            current += c;
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if ((c == '"' || c == '\'') && !in_quotes) {
            in_quotes = true;
            quote_char = c;
            current += c;
            continue;
        }

        if (c == quote_char && in_quotes) {
            in_quotes = false;
            current += c;
            quote_char = '\0';
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(c)) && !in_quotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current += c;
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

} // namespace uniconv::cli
