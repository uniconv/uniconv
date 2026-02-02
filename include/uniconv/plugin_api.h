/**
 * uniconv Plugin C API
 *
 * This header defines the C ABI for native uniconv plugins.
 * Plugins must implement all required functions to be loaded.
 *
 * Example usage in a plugin:
 *
 *   #include <uniconv/plugin_api.h>
 *
 *   UNICONV_EXPORT UniconvPluginInfo* uniconv_plugin_info(void) {
 *       static UniconvPluginInfo info = {
 *           .name = "my-plugin",
 *           .group = "my-plugin",
 *           .etl = UNICONV_ETL_TRANSFORM,
 *           .version = "1.0.0",
 *           .description = "My awesome plugin",
 *           .targets = (const char*[]){"target1", "target2", NULL},
 *           .input_formats = (const char*[]){"jpg", "png", NULL}
 *       };
 *       return &info;
 *   }
 *
 *   UNICONV_EXPORT UniconvResult* uniconv_plugin_execute(const UniconvRequest* req) {
 *       // ... implementation
 *   }
 */

#ifndef UNICONV_PLUGIN_API_H
#define UNICONV_PLUGIN_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Export macro for plugin functions */
#if defined(_WIN32) || defined(_WIN64)
    #define UNICONV_EXPORT __declspec(dllexport)
#else
    #define UNICONV_EXPORT __attribute__((visibility("default")))
#endif

/* API version for compatibility checking */
#define UNICONV_API_VERSION 1

/**
 * ETL operation types
 */
typedef enum {
    UNICONV_ETL_TRANSFORM = 0,
    UNICONV_ETL_EXTRACT = 1,
    UNICONV_ETL_LOAD = 2
} UniconvETLType;

/**
 * Result status codes
 */
typedef enum {
    UNICONV_SUCCESS = 0,
    UNICONV_ERROR = 1,
    UNICONV_SKIPPED = 2
} UniconvStatus;

/**
 * Plugin information structure
 * Returned by uniconv_plugin_info()
 */
typedef struct {
    const char* name;           /* Plugin name (e.g., "my-plugin") */
    const char* group;          /* Plugin group (e.g., "my-plugin") */
    UniconvETLType etl;         /* ETL type this plugin handles */
    const char* version;        /* Version string (e.g., "1.0.0") */
    const char* description;    /* Human-readable description */
    const char** targets;       /* NULL-terminated array of supported targets */
    const char** input_formats; /* NULL-terminated array of supported input formats */
    int api_version;            /* Should be UNICONV_API_VERSION */
} UniconvPluginInfo;

/**
 * Callback type for option lookup
 * Returns the value for the given key, or NULL if not found
 */
typedef const char* (*UniconvOptionGetter)(const char* key, void* ctx);

/**
 * Request structure passed to plugin execute function
 */
typedef struct {
    UniconvETLType etl;              /* ETL operation type */
    const char* source;              /* Input file path */
    const char* target;              /* Target format/type (e.g., "jpg", "faces") */
    const char* output;              /* Output path (may be NULL) */
    int force;                       /* Overwrite existing files */
    int dry_run;                     /* Don't actually execute */

    /* Option accessors - use these to get option values */
    UniconvOptionGetter get_core_option;    /* Get core option (quality, width, etc.) */
    UniconvOptionGetter get_plugin_option;  /* Get plugin-specific option */
    void* options_ctx;                      /* Context for option getters */
} UniconvRequest;

/**
 * Result structure returned by plugin execute function
 * Plugin allocates this; core frees it via uniconv_plugin_free_result()
 */
typedef struct {
    UniconvStatus status;        /* Result status */
    char* output;                /* Output file path (allocated by plugin) */
    size_t output_size;          /* Output file size in bytes */
    char* error;                 /* Error message if status != SUCCESS (allocated by plugin) */
    char* extra_json;            /* Optional JSON string with extra data (allocated by plugin) */
} UniconvResult;

/**
 * Required plugin functions
 * All native plugins must implement these
 */

/**
 * Get plugin information
 * Called once when plugin is loaded
 * Returns: Pointer to static PluginInfo (do not free)
 */
typedef UniconvPluginInfo* (*UniconvPluginInfoFunc)(void);

/**
 * Execute the plugin operation
 * Called for each file to process
 * Returns: Allocated result structure (freed by core via free_result)
 */
typedef UniconvResult* (*UniconvPluginExecuteFunc)(const UniconvRequest* request);

/**
 * Free a result structure
 * Called by core after processing the result
 */
typedef void (*UniconvPluginFreeResultFunc)(UniconvResult* result);

/* Function names that plugins must export */
#define UNICONV_PLUGIN_INFO_FUNC "uniconv_plugin_info"
#define UNICONV_PLUGIN_EXECUTE_FUNC "uniconv_plugin_execute"
#define UNICONV_PLUGIN_FREE_RESULT_FUNC "uniconv_plugin_free_result"

/**
 * Helper macros for plugin implementation
 */

/* Allocate a result structure */
#define UNICONV_RESULT_ALLOC() \
    ((UniconvResult*)calloc(1, sizeof(UniconvResult)))

/* Create a success result */
#define UNICONV_RESULT_SUCCESS(out_path, out_size) \
    do { \
        UniconvResult* _r = UNICONV_RESULT_ALLOC(); \
        _r->status = UNICONV_SUCCESS; \
        _r->output = strdup(out_path); \
        _r->output_size = out_size; \
        return _r; \
    } while(0)

/* Create an error result */
#define UNICONV_RESULT_ERROR(msg) \
    do { \
        UniconvResult* _r = UNICONV_RESULT_ALLOC(); \
        _r->status = UNICONV_ERROR; \
        _r->error = strdup(msg); \
        return _r; \
    } while(0)

/* Default free_result implementation */
#define UNICONV_DEFAULT_FREE_RESULT(result) \
    do { \
        if (result) { \
            free(result->output); \
            free(result->error); \
            free(result->extra_json); \
            free(result); \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* UNICONV_PLUGIN_API_H */
