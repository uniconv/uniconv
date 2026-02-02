#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #ifdef UNICONV_BUILDING_DLL
        #define UNICONV_API __declspec(dllexport)
    #else
        #define UNICONV_API __declspec(dllimport)
    #endif
#else
    #if __GNUC__ >= 4
        #define UNICONV_API __attribute__((visibility("default")))
    #else
        #define UNICONV_API
    #endif
#endif

#ifdef __cplusplus
    #define UNICONV_EXTERN_C extern "C"
    #define UNICONV_EXTERN_C_BEGIN extern "C" {
    #define UNICONV_EXTERN_C_END }
#else
    #define UNICONV_EXTERN_C
    #define UNICONV_EXTERN_C_BEGIN
    #define UNICONV_EXTERN_C_END
#endif
