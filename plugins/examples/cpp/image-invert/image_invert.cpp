/**
 * uniconv Native Plugin Example: image-invert
 *
 * Demonstrates how to create a native C++ plugin for uniconv.
 * This plugin inverts the colors of an image using libvips.
 *
 * Build:
 *   mkdir build && cd build
 *   cmake ..
 *   cmake --build .
 */

#include <uniconv/plugin_api.h>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef HAS_VIPS
#include <vips/vips8>
#endif

// Plugin info - returned by uniconv_plugin_info()
static const char *targets[] = {"invert", "negative", nullptr};
static const char *input_formats[] = {"jpg", "jpeg", "png", "bmp", nullptr};

// Data type information
static UniconvDataType input_types[] = {UNICONV_DATA_IMAGE, UNICONV_DATA_FILE, (UniconvDataType)0};
static UniconvDataType output_types[] = {UNICONV_DATA_IMAGE, (UniconvDataType)0};

static UniconvPluginInfo plugin_info = {
    .name = "image-invert",
    .group = "image-invert",
    .version = "1.0.0",
    .description = "Invert image colors",
    .targets = targets,
    .input_formats = input_formats,
    .input_types = input_types,
    .output_types = output_types};

extern "C"
{

    /**
     * Return plugin information
     */
    UNICONV_EXPORT UniconvPluginInfo *uniconv_plugin_info(void)
    {
        return &plugin_info;
    }

    /**
     * Execute the plugin operation
     */
    UNICONV_EXPORT UniconvResult *uniconv_plugin_execute(const UniconvRequest *request)
    {
        UniconvResult *result = static_cast<UniconvResult *>(calloc(1, sizeof(UniconvResult)));
        if (!result)
        {
            return nullptr;
        }

        // Validate input
        if (!request || !request->source)
        {
            result->status = UNICONV_ERROR;
            result->error = strdup("Invalid request: missing source");
            return result;
        }

        // Get input file extension (preserve format)
        std::string source_path = request->source;
        std::string input_ext = ".jpg"; // Default fallback
        size_t src_dot = source_path.rfind('.');
        if (src_dot != std::string::npos)
        {
            input_ext = source_path.substr(src_dot);
        }

        // Get target name for suffix
        std::string target_suffix = request->target ? std::string("_") + request->target : "_invert";

        // Determine output path
        std::string output_path;
        if (request->output)
        {
            output_path = request->output;
            // Remove any extension, add suffix and input extension
            size_t dot = output_path.rfind('.');
            size_t slash = output_path.find_last_of("/\\");
            // Only treat as extension if dot is after last slash
            if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
            {
                output_path = output_path.substr(0, dot) + target_suffix + input_ext;
            }
            else
            {
                output_path += target_suffix + input_ext;
            }
        }
        else
        {
            output_path = source_path;
            // Replace extension with suffix + input extension
            size_t dot = output_path.rfind('.');
            if (dot != std::string::npos)
            {
                output_path = output_path.substr(0, dot) + target_suffix + input_ext;
            }
            else
            {
                output_path += target_suffix + input_ext;
            }
        }

        // Dry run - just return what would happen
        if (request->dry_run)
        {
            result->status = UNICONV_SUCCESS;
            result->output = strdup(output_path.c_str());
            result->extra_json = strdup("{\"dry_run\": true}");
            return result;
        }

#ifdef HAS_VIPS
        try
        {
            // Load image
            vips::VImage image = vips::VImage::new_from_file(request->source);

            // Invert colors
            vips::VImage inverted = image.invert();

            // Write output
            inverted.write_to_file(output_path.c_str());

            result->status = UNICONV_SUCCESS;
            result->output = strdup(output_path.c_str());

            // Get file size
            FILE *f = fopen(output_path.c_str(), "rb");
            if (f)
            {
                fseek(f, 0, SEEK_END);
                result->output_size = ftell(f);
                fclose(f);
            }

            return result;
        }
        catch (const vips::VError &e)
        {
            result->status = UNICONV_ERROR;
            result->error = strdup(e.what());
            return result;
        }
        catch (const std::exception &e)
        {
            result->status = UNICONV_ERROR;
            result->error = strdup(e.what());
            return result;
        }
#else
        // No libvips support
        result->status = UNICONV_ERROR;
        result->error = strdup("Plugin was built without libvips support. "
                               "Rebuild with libvips installed.");
        return result;
#endif
    }

    /**
     * Free a result structure
     */
    UNICONV_EXPORT void uniconv_plugin_free_result(UniconvResult *result)
    {
        if (result)
        {
            free(result->output);
            free(result->error);
            free(result->extra_json);
            free(result);
        }
    }

} // extern "C"
