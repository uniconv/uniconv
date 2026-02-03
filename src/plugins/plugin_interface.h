#pragma once

#include "core/types.h"
#include <memory>
#include <string>
#include <vector>

namespace uniconv::plugins {

// Abstract plugin interface
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Plugin identification
    virtual core::PluginInfo info() const = 0;

    // Check if this plugin can handle the given target
    virtual bool supports_target(const std::string& target) const = 0;

    // Check if this plugin can handle the given input format
    virtual bool supports_input(const std::string& format) const = 0;

    // Execute the ETL operation
    virtual core::Result execute(const core::Request& request) = 0;
};

using PluginPtr = std::unique_ptr<IPlugin>;

} // namespace uniconv::plugins
