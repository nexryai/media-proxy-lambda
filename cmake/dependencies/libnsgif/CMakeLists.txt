cmake_minimum_required(VERSION 4.2)

project(mediaproxy-libnsgif-adapter LANGUAGES C)

if(NOT DEFINED NSGIF_SOURCE_DIR)
    message(FATAL_ERROR "NSGIF_SOURCE_DIR is required")
endif()

foreach(required_source IN ITEMS
        "${NSGIF_SOURCE_DIR}/include/nsgif.h"
        "${NSGIF_SOURCE_DIR}/src/gif.c"
        "${NSGIF_SOURCE_DIR}/src/lzw.c"
        "${NSGIF_SOURCE_DIR}/src/lzw.h")
    if(NOT EXISTS "${required_source}")
        message(FATAL_ERROR "libnsgif source is absent: ${required_source}")
    endif()
endforeach()

add_library(nsgif STATIC
    "${NSGIF_SOURCE_DIR}/src/gif.c"
    "${NSGIF_SOURCE_DIR}/src/lzw.c"
)
set_target_properties(nsgif PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
target_compile_options(nsgif PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wpedantic
    -Wundef
    -Wpointer-arith
    -Wcast-align
    -Wwrite-strings
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wmissing-declarations
    -Wnested-externs
)
target_include_directories(nsgif PRIVATE
    "${NSGIF_SOURCE_DIR}/include"
    "${NSGIF_SOURCE_DIR}/src"
)
