#include "image_transform.h"

#ifdef UNICONV_HAS_VIPS
#include <vips/vips8>
#endif

#include <algorithm>
#include <stdexcept>

namespace uniconv::plugins {

namespace {

// Helper to lowercase a string
std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Get file extension without dot, lowercase
std::string get_extension(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    return to_lower(ext);
}

} // anonymous namespace

ImageTransformPlugin::ImageTransformPlugin() {
#ifdef UNICONV_HAS_VIPS
    // Initialize vips if not already done
    if (VIPS_INIT("uniconv")) {
        throw std::runtime_error("Failed to initialize libvips");
    }
#endif
}

core::PluginInfo ImageTransformPlugin::info() const {
    core::PluginInfo info;
    info.id = std::string(kGroupName) + ".transform";
    info.group = std::string(kGroupName);
    info.etl = kETLType;
    info.version = "0.1.0";
#ifdef UNICONV_HAS_VIPS
    info.description = "Image format transformation using libvips";
#else
    info.description = "Image format transformation (libvips not available)";
#endif
    info.builtin = true;

    for (const auto& fmt : kOutputFormats) {
        info.targets.emplace_back(fmt);
    }
    for (const auto& fmt : kInputFormats) {
        info.input_formats.emplace_back(fmt);
    }

    return info;
}

bool ImageTransformPlugin::supports_target(const std::string& target) const {
#ifndef UNICONV_HAS_VIPS
    return false;
#else
    auto lower = to_lower(target);
    return std::find(kOutputFormats.begin(), kOutputFormats.end(), lower) != kOutputFormats.end();
#endif
}

bool ImageTransformPlugin::supports_input(const std::string& format) const {
#ifndef UNICONV_HAS_VIPS
    return false;
#else
    auto lower = to_lower(format);
    return std::find(kInputFormats.begin(), kInputFormats.end(), lower) != kInputFormats.end();
#endif
}

core::ETLResult ImageTransformPlugin::execute(const core::ETLRequest& request) {
#ifndef UNICONV_HAS_VIPS
    return core::ETLResult::failure(
        request.etl, request.target, request.source,
        "libvips not available - image transform disabled. Install libvips and rebuild."
    );
#else
    // Validate input file exists
    if (!std::filesystem::exists(request.source)) {
        return core::ETLResult::failure(
            request.etl, request.target, request.source,
            "Input file does not exist: " + request.source.string()
        );
    }

    // Check input format
    auto input_format = get_extension(request.source);
    if (!supports_input(input_format)) {
        return core::ETLResult::failure(
            request.etl, request.target, request.source,
            "Unsupported input format: " + input_format
        );
    }

    // Check target format
    if (!supports_target(request.target)) {
        return core::ETLResult::failure(
            request.etl, request.target, request.source,
            "Unsupported target format: " + request.target
        );
    }

    // Determine output path
    auto output_path = determine_output_path(
        request.source, request.target, request.core_options.output
    );

    // Check if output exists and force flag
    if (std::filesystem::exists(output_path) && !request.core_options.force) {
        return core::ETLResult::failure(
            request.etl, request.target, request.source,
            "Output file already exists (use --force to overwrite): " + output_path.string()
        );
    }

    // Dry run - don't actually convert
    if (request.core_options.dry_run) {
        auto result = core::ETLResult::success(
            request.etl, request.target, std::string(kGroupName),
            request.source, output_path,
            std::filesystem::file_size(request.source), 0
        );
        result.extra["dry_run"] = true;
        return result;
    }

    // Perform conversion
    return convert_image(request.source, output_path, request.target, request.core_options);
#endif
}

core::ETLResult ImageTransformPlugin::convert_image(
    const std::filesystem::path& input,
    const std::filesystem::path& output,
    const std::string& format,
    const core::CoreOptions& options
) {
#ifndef UNICONV_HAS_VIPS
    return core::ETLResult::failure(
        core::ETLType::Transform, format, input,
        "libvips not available"
    );
#else
    try {
        // Load image
        vips::VImage image = vips::VImage::new_from_file(input.string().c_str());

        size_t input_size = std::filesystem::file_size(input);

        // Apply resize if width or height specified
        if (options.width || options.height) {
            int target_width = options.width.value_or(0);
            int target_height = options.height.value_or(0);

            if (target_width > 0 && target_height > 0) {
                // Resize to exact dimensions
                image = image.thumbnail_image(target_width,
                    vips::VImage::option()
                        ->set("height", target_height)
                        ->set("size", VIPS_SIZE_FORCE));
            } else if (target_width > 0) {
                // Resize by width, maintain aspect ratio
                image = image.thumbnail_image(target_width);
            } else if (target_height > 0) {
                // Resize by height, maintain aspect ratio
                double scale = static_cast<double>(target_height) / image.height();
                int new_width = static_cast<int>(image.width() * scale);
                image = image.thumbnail_image(new_width,
                    vips::VImage::option()->set("height", target_height));
            }
        }

        // Create output directory if needed
        auto output_dir = output.parent_path();
        if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
            std::filesystem::create_directories(output_dir);
        }

        // Get quality setting
        int quality = get_quality_for_format(format, options.quality);

        // Save based on format
        std::string lower_format = to_lower(format);

        if (lower_format == "jpg" || lower_format == "jpeg") {
            image.jpegsave(output.string().c_str(),
                vips::VImage::option()->set("Q", quality));
        } else if (lower_format == "png") {
            // PNG compression level (0-9), we map quality 0-100 to 9-0
            int compression = 9 - (quality * 9 / 100);
            image.pngsave(output.string().c_str(),
                vips::VImage::option()->set("compression", compression));
        } else if (lower_format == "webp") {
            image.webpsave(output.string().c_str(),
                vips::VImage::option()->set("Q", quality));
        } else if (lower_format == "pdf") {
            // For PDF, we save as a single-page PDF
            image.magicksave(output.string().c_str());
        } else {
            return core::ETLResult::failure(
                core::ETLType::Transform, format, input,
                "Unsupported output format: " + format
            );
        }

        size_t output_size = std::filesystem::file_size(output);

        auto result = core::ETLResult::success(
            core::ETLType::Transform, format, std::string(kGroupName),
            input, output, input_size, output_size
        );

        // Add image dimensions to extra info
        result.extra["input_dimensions"] = {
            {"width", vips::VImage::new_from_file(input.string().c_str()).width()},
            {"height", vips::VImage::new_from_file(input.string().c_str()).height()}
        };
        result.extra["output_dimensions"] = {
            {"width", image.width()},
            {"height", image.height()}
        };

        return result;

    } catch (const vips::VError& e) {
        return core::ETLResult::failure(
            core::ETLType::Transform, format, input,
            std::string("libvips error: ") + e.what()
        );
    } catch (const std::exception& e) {
        return core::ETLResult::failure(
            core::ETLType::Transform, format, input,
            std::string("Error: ") + e.what()
        );
    }
#endif
}

std::filesystem::path ImageTransformPlugin::determine_output_path(
    const std::filesystem::path& input,
    const std::string& target_format,
    const std::optional<std::filesystem::path>& output_option
) const {
    if (output_option) {
        auto& out = *output_option;
        // If output is a directory, use input filename with new extension
        if (std::filesystem::is_directory(out) || out.string().back() == '/') {
            return out / (input.stem().string() + "." + target_format);
        }
        // Otherwise use as-is
        return out;
    }

    // Default: same directory, same name, new extension
    return input.parent_path() / (input.stem().string() + "." + target_format);
}

int ImageTransformPlugin::get_quality_for_format(
    const std::string& format,
    std::optional<int> quality
) const {
    // Default qualities by format
    constexpr int kDefaultJpegQuality = 85;
    constexpr int kDefaultWebpQuality = 80;
    constexpr int kDefaultPngQuality = 90;  // Maps to compression level

    if (quality) {
        // Clamp to valid range
        return std::clamp(*quality, 1, 100);
    }

    std::string lower = to_lower(format);
    if (lower == "jpg" || lower == "jpeg") return kDefaultJpegQuality;
    if (lower == "webp") return kDefaultWebpQuality;
    if (lower == "png") return kDefaultPngQuality;

    return 85;  // Generic default
}

} // namespace uniconv::plugins
