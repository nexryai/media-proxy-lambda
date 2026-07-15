cmake_minimum_required(VERSION 4.2.3)

project(MediaProxyLibPng LANGUAGES C)

foreach(required_variable IN ITEMS
        MEDIAPROXY_BUILD_DEFINITION_SHA256
        MEDIAPROXY_LIBPNG_SOURCE_DIR
        MEDIAPROXY_TARGET_ARCH
        MEDIAPROXY_ZLIB_INCLUDE_DIR
        MEDIAPROXY_ZLIB_VERNUM)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

message(STATUS
    "MediaProxy libpng build definition: "
    "${MEDIAPROXY_BUILD_DEFINITION_SHA256}")

set(libpng_sources
    png.c
    pngerror.c
    pngget.c
    pngmem.c
    pngpread.c
    pngread.c
    pngrio.c
    pngrtran.c
    pngrutil.c
    pngset.c
    pngtrans.c
    pngwio.c
    pngwrite.c
    pngwtran.c
    pngwutil.c
)

if(MEDIAPROXY_TARGET_ARCH STREQUAL "x86_64")
    list(APPEND libpng_sources
        intel/intel_init.c
        intel/filter_sse2_intrinsics.c
    )
    set(libpng_arch_definition PNG_INTEL_SSE_OPT=1)
elseif(MEDIAPROXY_TARGET_ARCH STREQUAL "arm64")
    list(APPEND libpng_sources
        arm/arm_init.c
        arm/filter_neon_intrinsics.c
        arm/palette_neon_intrinsics.c
    )
    set(libpng_arch_definition PNG_ARM_NEON_OPT=2)
else()
    message(FATAL_ERROR
        "MEDIAPROXY_TARGET_ARCH must be x86_64 or arm64")
endif()

list(TRANSFORM libpng_sources
    PREPEND "${MEDIAPROXY_LIBPNG_SOURCE_DIR}/")

file(READ
    "${MEDIAPROXY_LIBPNG_SOURCE_DIR}/scripts/pnglibconf.h.prebuilt"
    libpng_configuration)
string(REPLACE
    "#define PNG_ZLIB_VERNUM 0 /* unknown */"
    "#define PNG_ZLIB_VERNUM ${MEDIAPROXY_ZLIB_VERNUM}"
    libpng_configuration
    "${libpng_configuration}")
file(WRITE
    "${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.h"
    "${libpng_configuration}")

add_library(png_static STATIC ${libpng_sources})
set_target_properties(png_static PROPERTIES OUTPUT_NAME png16)
target_compile_definitions(png_static PRIVATE ${libpng_arch_definition})
target_include_directories(png_static
    PRIVATE
        "${MEDIAPROXY_LIBPNG_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${MEDIAPROXY_ZLIB_INCLUDE_DIR}"
)
