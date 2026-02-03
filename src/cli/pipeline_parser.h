#pragma once

#include "core/pipeline.h"
#include "core/types.h"
#include <string>
#include <vector>
#include <optional>

namespace uniconv::cli {

class PipelineParser {
public:
    struct ParseResult {
        bool success = false;
        core::Pipeline pipeline;
        std::string error;

        static ParseResult ok(core::Pipeline&& p) {
            ParseResult r;
            r.success = true;
            r.pipeline = std::move(p);
            return r;
        }

        static ParseResult fail(const std::string& err) {
            ParseResult r;
            r.success = false;
            r.error = err;
            return r;
        }
    };

    // Parse a pipeline string (everything after the source)
    // Input: "jpg --quality 90 | gdrive"
    ParseResult parse(const std::string& pipeline_str,
                      const std::filesystem::path& source,
                      const core::CoreOptions& core_options = {});

    // Parse from raw command line args (detects if pipeline syntax is used)
    // Returns nullopt if not a pipeline (use traditional -t/-e/-l parsing)
    std::optional<ParseResult> parse_from_args(const std::vector<std::string>& args);

    // Check if args contain pipeline syntax
    static bool is_pipeline_syntax(const std::vector<std::string>& args);

private:
    // Split by pipe character, respecting quotes
    std::vector<std::string> split_stages(const std::string& input);

    // Split by comma, respecting quotes
    std::vector<std::string> split_elements(const std::string& stage);

    // Parse a single element: "jpg@vips --quality 90"
    core::StageElement parse_element(const std::string& element_str);

    // Parse target[@plugin] part
    std::pair<std::string, std::optional<std::string>> parse_target(const std::string& target);

    // Parse options from tokenized strings
    std::pair<std::map<std::string, std::string>, std::vector<std::string>>
        parse_options(const std::vector<std::string>& tokens);

    // Tokenize a string respecting quotes
    std::vector<std::string> tokenize(const std::string& input);

    // Split string by delimiter, respecting quotes
    std::vector<std::string> split_respecting_quotes(const std::string& input, char delimiter);
};

} // namespace uniconv::cli
