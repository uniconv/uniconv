#pragma once

#include "core/types.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace uniconv::utils {

// Output JSON to stream (with optional pretty printing)
void output_json(std::ostream& os, const nlohmann::json& j, bool pretty = false);

// Output ETL result
void output_result(std::ostream& os, const core::Result& result, bool json_mode, bool verbose = false);

// Output multiple ETL results
void output_results(std::ostream& os, const std::vector<core::Result>& results, bool json_mode, bool verbose = false);

// Output file info
void output_file_info(std::ostream& os, const core::FileInfo& info, bool json_mode);

// Output plugin info
void output_plugin_info(std::ostream& os, const core::PluginInfo& info, bool json_mode);

// Output multiple plugin infos
void output_plugins(std::ostream& os, const std::vector<core::PluginInfo>& plugins, bool json_mode);

// Output preset
void output_preset(std::ostream& os, const core::Preset& preset, bool json_mode);

// Output multiple presets
void output_presets(std::ostream& os, const std::vector<core::Preset>& presets, bool json_mode);

// Output error
void output_error(std::ostream& os, const std::string& message, bool json_mode);

// Output success message
void output_success(std::ostream& os, const std::string& message, bool json_mode);

} // namespace uniconv::utils
