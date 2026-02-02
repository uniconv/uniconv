/**
 * uniconv Native Plugin Example: image-invert
 *
 * Demonstrates how to create a native C++ plugin for uniconv.
 * This plugin inverts the colors of an image.
 *
 * Build:
 *   g++ -shared -fPIC -o libimage_invert.so image_invert.cpp \
 *       -I/path/to/uniconv/include $(pkg-config --cflags --libs vips-cpp)
 *
 * Or on macOS:
 *   clang++ -shared -fPIC -o libimage_invert.dylib image_invert.cpp \
 *       -I/path/to/uniconv/include $(pkg-config --cflags --libs vips-cpp)
 */

#include <uniconv/plugin_api.h>
#include <cstdlib>
#include <cstring>
#include <string>

// Uncomment if using libvips for actual implementation
// #include <vips/vips8>

// Plugin info - returned by uniconv_plugin_info()
static const char* targets[] = {"invert", "negative", nullptr};
static const char* input_formats[] = {"jpg", "jpeg", "png", "bmp", nullptr};

static UniconvPluginInfo plugin_info = {
    .name = "image-invert",
    .group = "image-invert",
    .etl = UNICONV_ETL_TRANSFORM,
    .version = "1.0.0",
    .description = "Invert image colors",
    .targets = targets,
    .input_formats = input_formats,
    .api_version = UNICONV_API_VERSION
};

extern "C" {

/**
 * Return plugin information
 */
UNICONV_EXPORT UniconvPluginInfo* uniconv_plugin_info(void) {
    return &plugin_info;
}

/**
 * Execute the plugin operation
 */
UNICONV_EXPORT UniconvResult* uniconv_plugin_execute(const UniconvRequest* request) {
    UniconvResult* result = static_cast<UniconvResult*>(calloc(1, sizeof(UniconvResult)));
    if (!result) {
        return nullptr;
    }

    // Validate input
    if (!request || !request->source) {
        result->status = UNICONV_ERROR;
        result->error = strdup("Invalid request: missing source");
        return result;
    }

    // Determine output path
    std::string output_path;
    if (request->output) {
        output_path = request->output;
    } else {
        output_path = request->source;
        // Replace extension
        size_t dot = output_path.rfind('.');
        if (dot != std::string::npos) {
            output_path = output_path.substr(0, dot) + "_inverted" + output_path.substr(dot);
        } else {
            output_path += "_inverted.jpg";
        }
    }

    // Dry run - just return what would happen
    if (request->dry_run) {
        result->status = UNICONV_SUCCESS;
        result->output = strdup(output_path.c_str());
        result->extra_json = strdup("{\"dry_run\": true}");
        return result;
    }

    // NOTE: This is a simplified example. For a real implementation,
    // you would use an image library like libvips, stb_image, or OpenCV.
    //
    // Example with libvips (commented out):
    /*
    try {
        vips::VImage image = vips::VImage::new_from_file(request->source);
        vips::VImage inverted = image.invert();
        inverted.write_to_file(output_path.c_str());

        result->status = UNICONV_SUCCESS;
        result->output = strdup(output_path.c_str());

        // Get file size
        FILE* f = fopen(output_path.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            result->output_size = ftell(f);
            fclose(f);
        }

        return result;
    } catch (const std::exception& e) {
        result->status = UNICONV_ERROR;
        result->error = strdup(e.what());
        return result;
    }
    */

    // Placeholder implementation - just demonstrates the API
    result->status = UNICONV_ERROR;
    result->error = strdup("This is a demonstration plugin. "
                          "For actual image inversion, build with libvips support.");
    return result;
}

/**
 * Free a result structure
 */
UNICONV_EXPORT void uniconv_plugin_free_result(UniconvResult* result) {
    if (result) {
        free(result->output);
        free(result->error);
        free(result->extra_json);
        free(result);
    }
}

} // extern "C"
