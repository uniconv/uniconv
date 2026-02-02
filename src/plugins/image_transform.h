#pragma once

#include "plugin_interface.h"
#include <array>
#include <string_view>

namespace uniconv::plugins {

class ImageTransformPlugin : public IPlugin {
public:
    static constexpr std::string_view kGroupName = "image-core";
    static constexpr core::ETLType kETLType = core::ETLType::Transform;

    // Supported formats
    static constexpr std::array<std::string_view, 9> kInputFormats = {
        "heic", "heif", "jpg", "jpeg", "png", "webp", "gif", "bmp", "tiff"
    };

    static constexpr std::array<std::string_view, 5> kOutputFormats = {
        "jpg", "jpeg", "png", "webp", "pdf"
    };

    ImageTransformPlugin();
    ~ImageTransformPlugin() override = default;

    // IPlugin interface
    core::PluginInfo info() const override;
    bool supports_target(const std::string& target) const override;
    bool supports_input(const std::string& format) const override;
    core::ETLResult execute(const core::ETLRequest& request) override;

private:
    // Internal conversion methods
    core::ETLResult convert_image(
        const std::filesystem::path& input,
        const std::filesystem::path& output,
        const std::string& format,
        const core::CoreOptions& options
    );

    // Determine output path
    std::filesystem::path determine_output_path(
        const std::filesystem::path& input,
        const std::string& target_format,
        const std::optional<std::filesystem::path>& output_option
    ) const;

    // Get quality setting for format
    int get_quality_for_format(const std::string& format, std::optional<int> quality) const;
};

} // namespace uniconv::plugins
