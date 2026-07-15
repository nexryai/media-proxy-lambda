cmake_minimum_required(VERSION 4.2.3)

project(MediaProxyLibFfi LANGUAGES C ASM)

foreach(required_variable IN ITEMS
        MEDIAPROXY_BUILD_DEFINITION_SHA256
        MEDIAPROXY_LIBFFI_CONFIG_DIR
        MEDIAPROXY_LIBFFI_SOURCE_DIR
        MEDIAPROXY_LIBFFI_TARGET_ARCH
        MEDIAPROXY_LIBFFI_VERSION)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

message(STATUS
    "MediaProxy libffi ${MEDIAPROXY_LIBFFI_VERSION} build definition: "
    "${MEDIAPROXY_BUILD_DEFINITION_SHA256}")

if(NOT EXISTS "${MEDIAPROXY_LIBFFI_CONFIG_DIR}/fficonfig.h"
        OR NOT EXISTS "${MEDIAPROXY_LIBFFI_CONFIG_DIR}/include/ffi.h"
        OR NOT EXISTS "${MEDIAPROXY_LIBFFI_CONFIG_DIR}/include/ffitarget.h"
        OR NOT EXISTS "${MEDIAPROXY_LIBFFI_CONFIG_DIR}/libffi.pc")
    message(FATAL_ERROR "Configured libffi headers or metadata are absent")
endif()

set(libffi_sources
    "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src/closures.c"
    "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src/prep_cif.c"
    "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src/tramp.c"
    "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src/types.c"
)

if(MEDIAPROXY_LIBFFI_TARGET_ARCH STREQUAL "x86_64")
    list(APPEND libffi_sources
        "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src/x86/ffi64.c"
        "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src/x86/ffiw64.c"
        "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src/x86/unix64.S"
        "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src/x86/win64.S"
    )
elseif(MEDIAPROXY_LIBFFI_TARGET_ARCH STREQUAL "arm64")
    list(APPEND libffi_sources
        "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src/aarch64/ffi.c"
        "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src/aarch64/sysv.S"
    )
    set_source_files_properties(
        "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src/aarch64/ffi.c"
        PROPERTIES COMPILE_OPTIONS -Wno-unused-parameter
    )
else()
    message(FATAL_ERROR
        "Unsupported libffi target: ${MEDIAPROXY_LIBFFI_TARGET_ARCH}")
endif()

add_library(ffi STATIC ${libffi_sources})
set_target_properties(ffi PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME ffi
)
target_compile_definitions(ffi PRIVATE HAVE_CONFIG_H)
target_compile_options(ffi PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-Werror>"
)
set_source_files_properties(
    "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src/prep_cif.c"
    PROPERTIES COMPILE_OPTIONS -Wno-unused-parameter
)
set_source_files_properties(
    "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src/closures.c"
    PROPERTIES COMPILE_OPTIONS -Wno-gnu-null-pointer-arithmetic
)
target_include_directories(ffi PRIVATE
    "${MEDIAPROXY_LIBFFI_CONFIG_DIR}"
    "${MEDIAPROXY_LIBFFI_CONFIG_DIR}/include"
    "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/include"
    "${MEDIAPROXY_LIBFFI_SOURCE_DIR}/src"
)

install(TARGETS ffi ARCHIVE DESTINATION lib)
install(FILES
    "${MEDIAPROXY_LIBFFI_CONFIG_DIR}/include/ffi.h"
    "${MEDIAPROXY_LIBFFI_CONFIG_DIR}/include/ffitarget.h"
    DESTINATION include
)
install(FILES "${MEDIAPROXY_LIBFFI_CONFIG_DIR}/libffi.pc"
    DESTINATION lib/pkgconfig)
