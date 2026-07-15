cmake_minimum_required(VERSION 4.2.3)

project(MediaProxyLibExif LANGUAGES C)

foreach(required_variable IN ITEMS
        MEDIAPROXY_BUILD_DEFINITION_SHA256
        MEDIAPROXY_LIBEXIF_SOURCE_DIR
        MEDIAPROXY_LIBEXIF_VERSION)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

message(STATUS
    "MediaProxy libexif build definition: "
    "${MEDIAPROXY_BUILD_DEFINITION_SHA256}")

set(libexif_sources
    canon/exif-mnote-data-canon.c
    canon/mnote-canon-entry.c
    canon/mnote-canon-tag.c
    exif-byte-order.c
    exif-content.c
    exif-data.c
    exif-entry.c
    exif-format.c
    exif-gps-ifd.c
    exif-ifd.c
    exif-loader.c
    exif-log.c
    exif-mem.c
    exif-mnote-data.c
    exif-tag.c
    exif-utils.c
    fuji/exif-mnote-data-fuji.c
    fuji/mnote-fuji-entry.c
    fuji/mnote-fuji-tag.c
    olympus/exif-mnote-data-olympus.c
    olympus/mnote-olympus-entry.c
    olympus/mnote-olympus-tag.c
    pentax/exif-mnote-data-pentax.c
    pentax/mnote-pentax-entry.c
    pentax/mnote-pentax-tag.c
)
list(TRANSFORM libexif_sources
    PREPEND "${MEDIAPROXY_LIBEXIF_SOURCE_DIR}/libexif/")

foreach(required_source IN LISTS libexif_sources)
    if(NOT EXISTS "${required_source}")
        message(FATAL_ERROR "libexif source is absent: ${required_source}")
    endif()
endforeach()

configure_file(config.h.in config.h @ONLY)

add_library(exif STATIC ${libexif_sources})
set_target_properties(exif PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
target_include_directories(exif PRIVATE
    "${MEDIAPROXY_LIBEXIF_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_options(exif PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wpedantic
    -Wno-switch
)

set(libexif_public_headers
    exif-byte-order.h
    exif-content.h
    exif-data.h
    exif-data-type.h
    exif-entry.h
    exif-format.h
    exif-ifd.h
    exif-loader.h
    exif-log.h
    exif-mem.h
    exif-mnote-data.h
    exif-tag.h
    exif-utils.h
    _stdint.h
)
list(TRANSFORM libexif_public_headers
    PREPEND "${MEDIAPROXY_LIBEXIF_SOURCE_DIR}/libexif/")

set(prefix /usr)
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
set(VERSION "${MEDIAPROXY_LIBEXIF_VERSION}")
configure_file(
    "${MEDIAPROXY_LIBEXIF_SOURCE_DIR}/libexif.pc.in"
    libexif.pc
    @ONLY
)

install(TARGETS exif ARCHIVE DESTINATION lib)
install(FILES ${libexif_public_headers} DESTINATION include/libexif)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libexif.pc"
    DESTINATION lib/pkgconfig)
