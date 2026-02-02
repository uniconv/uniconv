#include "string_utils.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>
#include <sstream>
#include <iomanip>

namespace uniconv::utils {

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string to_upper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::string trim(const std::string& s) {
    auto start = std::find_if_not(s.begin(), s.end(),
                                   [](unsigned char c) { return std::isspace(c); });
    auto end = std::find_if_not(s.rbegin(), s.rend(),
                                 [](unsigned char c) { return std::isspace(c); }).base();
    return (start < end) ? std::string(start, end) : std::string();
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> parts;
    std::istringstream stream(s);
    std::string part;
    while (std::getline(stream, part, delimiter)) {
        parts.push_back(part);
    }
    return parts;
}

std::string join(const std::vector<std::string>& parts, const std::string& delimiter) {
    if (parts.empty()) return "";

    std::ostringstream result;
    result << parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        result << delimiter << parts[i];
    }
    return result.str();
}

std::optional<size_t> parse_size(const std::string& s) {
    static const std::regex size_regex(
        R"(^\s*(\d+(?:\.\d+)?)\s*(B|KB|MB|GB|TB|K|M|G|T)?\s*$)",
        std::regex::icase
    );

    std::smatch match;
    if (!std::regex_match(s, match, size_regex)) {
        return std::nullopt;
    }

    double value = std::stod(match[1].str());
    std::string unit = to_upper(match[2].str());

    size_t multiplier = 1;
    if (unit.empty() || unit == "B") {
        multiplier = 1;
    } else if (unit == "K" || unit == "KB") {
        multiplier = 1024;
    } else if (unit == "M" || unit == "MB") {
        multiplier = 1024 * 1024;
    } else if (unit == "G" || unit == "GB") {
        multiplier = 1024ULL * 1024 * 1024;
    } else if (unit == "T" || unit == "TB") {
        multiplier = 1024ULL * 1024 * 1024 * 1024;
    }

    return static_cast<size_t>(value * multiplier);
}

std::string format_size(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }

    std::ostringstream ss;
    if (unit_index == 0) {
        ss << bytes << " " << units[unit_index];
    } else {
        ss << std::fixed << std::setprecision(size < 10 ? 2 : (size < 100 ? 1 : 0))
           << size << " " << units[unit_index];
    }
    return ss.str();
}

bool starts_with(const std::string& s, const std::string& prefix) {
    if (prefix.size() > s.size()) return false;
    return s.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(const std::string& s, const std::string& suffix) {
    if (suffix.size() > s.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string replace_all(const std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;

    std::string result = s;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}

} // namespace uniconv::utils
