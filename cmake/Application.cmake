include(CTest)
include(FetchContent)
include(cmake/Hardening.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

foreach(required_ada_idna_artifact IN ITEMS
        "${MEDIAPROXY_ADA_IDNA_INCLUDE_DIR}/ada/ada_idna.h"
        "${MEDIAPROXY_ADA_IDNA_LIBRARY}"
        "${MEDIAPROXY_ADA_IDNA_LICENSE}")
    if(NOT EXISTS "${required_ada_idna_artifact}")
        message(FATAL_ERROR
            "Pinned Ada IDNA artifact is absent: "
            "${required_ada_idna_artifact}")
    endif()
endforeach()

foreach(required_yyjson_artifact IN ITEMS
        "${MEDIAPROXY_YYJSON_INCLUDE_DIR}/yyjson.h"
        "${MEDIAPROXY_YYJSON_LIBRARY}")
    if(NOT EXISTS "${required_yyjson_artifact}")
        message(FATAL_ERROR "Pinned yyjson artifact is absent: ${required_yyjson_artifact}")
    endif()
endforeach()

foreach(required_boringssl_artifact IN ITEMS
        "${MEDIAPROXY_BORINGSSL_INCLUDE_DIR}/openssl/ssl.h"
        "${MEDIAPROXY_BORINGSSL_CRYPTO_LIBRARY}"
        "${MEDIAPROXY_BORINGSSL_SSL_LIBRARY}")
    if(NOT EXISTS "${required_boringssl_artifact}")
        message(FATAL_ERROR
            "Pinned BoringSSL artifact is absent: ${required_boringssl_artifact}")
    endif()
endforeach()

foreach(required_curl_artifact IN ITEMS
        "${MEDIAPROXY_CURL_INCLUDE_DIR}/curl/curl.h"
        "${MEDIAPROXY_CURL_LIBRARY}")
    if(NOT EXISTS "${required_curl_artifact}")
        message(FATAL_ERROR
            "Pinned curl artifact is absent: ${required_curl_artifact}")
    endif()
endforeach()

foreach(required_nghttp2_artifact IN ITEMS
        "${MEDIAPROXY_NGHTTP2_INCLUDE_DIR}/nghttp2/nghttp2.h"
        "${MEDIAPROXY_NGHTTP2_LIBRARY}")
    if(NOT EXISTS "${required_nghttp2_artifact}")
        message(FATAL_ERROR
            "Pinned nghttp2 artifact is absent: ${required_nghttp2_artifact}")
    endif()
endforeach()

foreach(required_libpng_artifact IN ITEMS
        "${MEDIAPROXY_LIBPNG_INCLUDE_DIR}/png.h"
        "${MEDIAPROXY_LIBPNG_INCLUDE_DIR}/pnglibconf.h"
        "${MEDIAPROXY_LIBPNG_LIBRARY}")
    if(NOT EXISTS "${required_libpng_artifact}")
        message(FATAL_ERROR
            "Pinned libpng artifact is absent: ${required_libpng_artifact}")
    endif()
endforeach()

foreach(required_libjpeg_turbo_artifact IN ITEMS
        "${MEDIAPROXY_LIBJPEG_TURBO_INCLUDE_DIR}/jconfig.h"
        "${MEDIAPROXY_LIBJPEG_TURBO_INCLUDE_DIR}/jerror.h"
        "${MEDIAPROXY_LIBJPEG_TURBO_INCLUDE_DIR}/jmorecfg.h"
        "${MEDIAPROXY_LIBJPEG_TURBO_INCLUDE_DIR}/jpeglib.h"
        "${MEDIAPROXY_LIBJPEG_TURBO_LIBRARY}")
    if(NOT EXISTS "${required_libjpeg_turbo_artifact}")
        message(FATAL_ERROR
            "Pinned libjpeg-turbo artifact is absent: "
            "${required_libjpeg_turbo_artifact}")
    endif()
endforeach()

foreach(required_libnsgif_artifact IN ITEMS
        "${MEDIAPROXY_LIBNSGIF_INCLUDE_DIR}/nsgif.h"
        "${MEDIAPROXY_LIBNSGIF_LIBRARY}")
    if(NOT EXISTS "${required_libnsgif_artifact}")
        message(FATAL_ERROR
            "Pinned libnsgif artifact is absent: ${required_libnsgif_artifact}")
    endif()
endforeach()

foreach(required_libexif_artifact IN ITEMS
        "${MEDIAPROXY_LIBEXIF_INCLUDE_DIR}/libexif/exif-data.h"
        "${MEDIAPROXY_LIBEXIF_INCLUDE_DIR}/libexif/exif-tag.h"
        "${MEDIAPROXY_LIBEXIF_LIBRARY}"
        "${MEDIAPROXY_LIBEXIF_PKGCONFIG}")
    if(NOT EXISTS "${required_libexif_artifact}")
        message(FATAL_ERROR
            "Pinned libexif artifact is absent: ${required_libexif_artifact}")
    endif()
endforeach()

foreach(required_libexpat_artifact IN ITEMS
        "${MEDIAPROXY_LIBEXPAT_INCLUDE_DIR}/expat.h"
        "${MEDIAPROXY_LIBEXPAT_INCLUDE_DIR}/expat_config.h"
        "${MEDIAPROXY_LIBEXPAT_INCLUDE_DIR}/expat_external.h"
        "${MEDIAPROXY_LIBEXPAT_LIBRARY}"
        "${MEDIAPROXY_LIBEXPAT_PKGCONFIG}")
    if(NOT EXISTS "${required_libexpat_artifact}")
        message(FATAL_ERROR
            "Pinned libexpat artifact is absent: "
            "${required_libexpat_artifact}")
    endif()
endforeach()

foreach(required_libffi_artifact IN ITEMS
        "${MEDIAPROXY_LIBFFI_INCLUDE_DIR}/ffi.h"
        "${MEDIAPROXY_LIBFFI_INCLUDE_DIR}/ffitarget.h"
        "${MEDIAPROXY_LIBFFI_LIBRARY}"
        "${MEDIAPROXY_LIBFFI_PKGCONFIG}")
    if(NOT EXISTS "${required_libffi_artifact}")
        message(FATAL_ERROR
            "Pinned libffi artifact is absent: ${required_libffi_artifact}")
    endif()
endforeach()

foreach(required_pcre2_artifact IN ITEMS
        "${MEDIAPROXY_PCRE2_INCLUDE_DIR}/pcre2.h"
        "${MEDIAPROXY_PCRE2_LIBRARY}"
        "${MEDIAPROXY_PCRE2_PKGCONFIG}")
    if(NOT EXISTS "${required_pcre2_artifact}")
        message(FATAL_ERROR
            "Pinned PCRE2 artifact is absent: ${required_pcre2_artifact}")
    endif()
endforeach()

foreach(required_glib_artifact IN ITEMS
        "${MEDIAPROXY_GLIB_INCLUDE_DIR}/glib.h"
        "${MEDIAPROXY_GLIB_INCLUDE_DIR}/glib-object.h"
        "${MEDIAPROXY_GLIB_INCLUDE_DIR}/gmodule.h"
        "${MEDIAPROXY_GLIB_INCLUDE_DIR}/gio/gio.h"
        "${MEDIAPROXY_GLIB_CONFIG_INCLUDE_DIR}/glibconfig.h"
        "${MEDIAPROXY_GLIB_LIBRARY}"
        "${MEDIAPROXY_GOBJECT_LIBRARY}"
        "${MEDIAPROXY_GTHREAD_LIBRARY}"
        "${MEDIAPROXY_GMODULE_LIBRARY}"
        "${MEDIAPROXY_GIO_LIBRARY}")
    if(NOT EXISTS "${required_glib_artifact}")
        message(FATAL_ERROR
            "Pinned GLib artifact is absent: ${required_glib_artifact}")
    endif()
endforeach()

foreach(required_libaom_artifact IN ITEMS
        "${MEDIAPROXY_LIBAOM_INCLUDE_DIR}/aom/aom.h"
        "${MEDIAPROXY_LIBAOM_INCLUDE_DIR}/aom/aomcx.h"
        "${MEDIAPROXY_LIBAOM_INCLUDE_DIR}/aom/aomdx.h"
        "${MEDIAPROXY_LIBAOM_LIBRARY}"
        "${MEDIAPROXY_LIBAOM_PKGCONFIG}")
    if(NOT EXISTS "${required_libaom_artifact}")
        message(FATAL_ERROR
            "Pinned libaom artifact is absent: ${required_libaom_artifact}")
    endif()
endforeach()

foreach(required_libheif_artifact IN ITEMS
        "${MEDIAPROXY_LIBHEIF_INCLUDE_DIR}/libheif/heif.h"
        "${MEDIAPROXY_LIBHEIF_INCLUDE_DIR}/libheif/heif_version.h"
        "${MEDIAPROXY_LIBHEIF_LIBRARY}"
        "${MEDIAPROXY_LIBHEIF_PKGCONFIG}")
    if(NOT EXISTS "${required_libheif_artifact}")
        message(FATAL_ERROR
            "Pinned libheif artifact is absent: ${required_libheif_artifact}")
    endif()
endforeach()

foreach(required_libvips_artifact IN ITEMS
        "${MEDIAPROXY_LIBVIPS_INCLUDE_DIR}/vips/vips.h"
        "${MEDIAPROXY_LIBVIPS_INCLUDE_DIR}/vips/version.h"
        "${MEDIAPROXY_LIBVIPS_INCLUDE_DIR}/vips/enumtypes.h"
        "${MEDIAPROXY_LIBVIPS_LIBRARY}"
        "${MEDIAPROXY_LIBVIPS_PKGCONFIG}")
    if(NOT EXISTS "${required_libvips_artifact}")
        message(FATAL_ERROR
            "Pinned libvips artifact is absent: ${required_libvips_artifact}")
    endif()
endforeach()

foreach(required_lcms2_artifact IN ITEMS
        "${MEDIAPROXY_LCMS2_INCLUDE_DIR}/lcms2.h"
        "${MEDIAPROXY_LCMS2_LIBRARY}")
    if(NOT EXISTS "${required_lcms2_artifact}")
        message(FATAL_ERROR
            "Pinned lcms2 artifact is absent: ${required_lcms2_artifact}")
    endif()
endforeach()

foreach(required_libwebp_artifact IN ITEMS
        "${MEDIAPROXY_LIBWEBP_INCLUDE_DIR}/webp/decode.h"
        "${MEDIAPROXY_LIBWEBP_INCLUDE_DIR}/webp/encode.h"
        "${MEDIAPROXY_LIBWEBP_INCLUDE_DIR}/webp/demux.h"
        "${MEDIAPROXY_LIBWEBP_INCLUDE_DIR}/webp/mux.h"
        "${MEDIAPROXY_LIBWEBP_INCLUDE_DIR}/webp/sharpyuv/sharpyuv.h"
        "${MEDIAPROXY_LIBWEBP_SHARPYUV_LIBRARY}"
        "${MEDIAPROXY_LIBWEBP_LIBRARY}"
        "${MEDIAPROXY_LIBWEBP_DEMUX_LIBRARY}"
        "${MEDIAPROXY_LIBWEBP_MUX_LIBRARY}")
    if(NOT EXISTS "${required_libwebp_artifact}")
        message(FATAL_ERROR
            "Pinned libwebp artifact is absent: ${required_libwebp_artifact}")
    endif()
endforeach()

foreach(required_zlib_artifact IN ITEMS
        "${MEDIAPROXY_ZLIB_INCLUDE_DIR}/zlib.h"
        "${MEDIAPROXY_ZLIB_LIBRARY}"
        "${MEDIAPROXY_ZLIB_PKGCONFIG}")
    if(NOT EXISTS "${required_zlib_artifact}")
        message(FATAL_ERROR
            "Pinned zlib artifact is absent: ${required_zlib_artifact}")
    endif()
endforeach()

add_library(mediaproxy_ada_idna STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_ada_idna PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_ADA_IDNA_LIBRARY}"
)

add_library(mediaproxy_yyjson STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_yyjson PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_YYJSON_LIBRARY}"
)

add_library(mediaproxy_boringssl_crypto STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_boringssl_crypto PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_BORINGSSL_CRYPTO_LIBRARY}"
)
add_library(mediaproxy_boringssl_ssl STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_boringssl_ssl PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_BORINGSSL_SSL_LIBRARY}"
    INTERFACE_LINK_LIBRARIES mediaproxy_boringssl_crypto
)

add_library(mediaproxy_nghttp2 STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_nghttp2 PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_NGHTTP2_LIBRARY}"
    INTERFACE_COMPILE_DEFINITIONS NGHTTP2_STATICLIB
)

add_library(mediaproxy_zlib STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_zlib PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_ZLIB_LIBRARY}"
)

add_library(mediaproxy_libpng STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_libpng PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LIBPNG_LIBRARY}"
    INTERFACE_LINK_LIBRARIES mediaproxy_zlib
)

add_library(mediaproxy_libjpeg_turbo STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_libjpeg_turbo PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LIBJPEG_TURBO_LIBRARY}"
)

add_library(mediaproxy_libnsgif STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_libnsgif PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LIBNSGIF_LIBRARY}"
)

add_library(mediaproxy_libexif STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_libexif PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LIBEXIF_LIBRARY}"
    INTERFACE_LINK_LIBRARIES m
)

add_library(mediaproxy_libexpat STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_libexpat PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LIBEXPAT_LIBRARY}"
    INTERFACE_COMPILE_DEFINITIONS XML_STATIC
)

add_library(mediaproxy_libffi STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_libffi PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LIBFFI_LIBRARY}"
)

add_library(mediaproxy_pcre2 STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_pcre2 PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_PCRE2_LIBRARY}"
    INTERFACE_COMPILE_DEFINITIONS "PCRE2_CODE_UNIT_WIDTH=8;PCRE2_STATIC"
)

add_library(mediaproxy_glib STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_glib PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_GLIB_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES
        "${MEDIAPROXY_GLIB_INCLUDE_DIR};${MEDIAPROXY_GLIB_CONFIG_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "mediaproxy_pcre2;m"
)
add_library(mediaproxy_gobject STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_gobject PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_GOBJECT_LIBRARY}"
    INTERFACE_LINK_LIBRARIES "mediaproxy_libffi;mediaproxy_glib"
)
add_library(mediaproxy_gthread STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_gthread PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_GTHREAD_LIBRARY}"
    INTERFACE_LINK_LIBRARIES mediaproxy_glib
)
add_library(mediaproxy_gmodule STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_gmodule PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_GMODULE_LIBRARY}"
    INTERFACE_LINK_LIBRARIES mediaproxy_glib
)
add_library(mediaproxy_gio STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_gio PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_GIO_LIBRARY}"
    INTERFACE_LINK_LIBRARIES
        "mediaproxy_gmodule;mediaproxy_gobject;mediaproxy_glib;mediaproxy_zlib"
)

add_library(mediaproxy_libaom STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_libaom PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LIBAOM_LIBRARY}"
)

add_library(mediaproxy_lcms2 STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_lcms2 PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LCMS2_LIBRARY}"
)

add_library(mediaproxy_libwebp_sharpyuv STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_libwebp_sharpyuv PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LIBWEBP_SHARPYUV_LIBRARY}"
)
add_library(mediaproxy_libheif STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_libheif PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LIBHEIF_LIBRARY}"
    INTERFACE_LINK_LIBRARIES
        "mediaproxy_libaom;mediaproxy_libwebp_sharpyuv"
)
add_library(mediaproxy_libwebp STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_libwebp PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LIBWEBP_LIBRARY}"
    INTERFACE_LINK_LIBRARIES mediaproxy_libwebp_sharpyuv
)
add_library(mediaproxy_libwebp_demux STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_libwebp_demux PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LIBWEBP_DEMUX_LIBRARY}"
    INTERFACE_LINK_LIBRARIES mediaproxy_libwebp
)
add_library(mediaproxy_libwebp_mux STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_libwebp_mux PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LIBWEBP_MUX_LIBRARY}"
    INTERFACE_LINK_LIBRARIES mediaproxy_libwebp
)

add_library(mediaproxy_libvips STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_libvips PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_LIBVIPS_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES
        "${MEDIAPROXY_GLIB_INCLUDE_DIR};${MEDIAPROXY_GLIB_CONFIG_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES
        "mediaproxy_libheif;mediaproxy_libaom;mediaproxy_libwebp_mux;mediaproxy_libwebp_demux;mediaproxy_libpng;mediaproxy_libjpeg_turbo;mediaproxy_lcms2;mediaproxy_libexif;mediaproxy_libexpat;mediaproxy_gio;mediaproxy_gobject;mediaproxy_gthread;mediaproxy_glib;mediaproxy_zlib;m"
)

add_library(mediaproxy_curl STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_curl PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_CURL_LIBRARY}"
    INTERFACE_COMPILE_DEFINITIONS CURL_STATICLIB
    INTERFACE_LINK_LIBRARIES
        "mediaproxy_boringssl_ssl;mediaproxy_nghttp2;mediaproxy_zlib"
)

add_library(mediaproxy_http STATIC
    src/http/address_policy.cpp
    src/http/curl_resolve_pin.cpp
    src/http/dns_policy.cpp
    src/http/event.cpp
    src/http/idna.cpp
    src/http/query.cpp
    src/http/request_plan.cpp
    src/http/response.cpp
    src/http/url_policy.cpp
)
target_include_directories(mediaproxy_http PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)
target_link_libraries(mediaproxy_http
    PRIVATE
        mediaproxy_hardening
        mediaproxy_warnings
        mediaproxy_ada_idna
        mediaproxy_curl
        mediaproxy_yyjson
)

add_executable(bootstrap src/bootstrap.cpp)
target_link_libraries(bootstrap
    PRIVATE
        mediaproxy_hardening
        mediaproxy_warnings
        mediaproxy_http
        mediaproxy_libvips
        mediaproxy_curl
        mediaproxy_boringssl_ssl
        mediaproxy_lcms2
        mediaproxy_libexif
        mediaproxy_libexpat
        mediaproxy_libffi
        mediaproxy_gio
        mediaproxy_gthread
        mediaproxy_libaom
        mediaproxy_libheif
        mediaproxy_pcre2
        mediaproxy_libjpeg_turbo
        mediaproxy_libnsgif
        mediaproxy_libpng
        mediaproxy_libwebp_demux
        mediaproxy_libwebp_mux
        mediaproxy_nghttp2
        mediaproxy_yyjson
        mediaproxy_zlib
)
target_link_options(bootstrap PRIVATE
    "LINKER:-Map,${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
)

add_custom_command(
    TARGET bootstrap
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
        "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
        "-DREADELF=${MEDIAPROXY_READELF}"
        "-DNM=${MEDIAPROXY_NM}"
        "-DUNDEFINED_SYMBOLS_FILE=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.undefined-symbols.txt"
        -P "${CMAKE_SOURCE_DIR}/cmake/VerifyStaticElf.cmake"
    VERBATIM
)

if(BUILD_TESTING)
    FetchContent_Declare(googletest
        URL "${MEDIAPROXY_GOOGLETEST_URL}"
        URL_HASH "SHA256=${MEDIAPROXY_GOOGLETEST_SHA256}"
        DOWNLOAD_DIR "${MEDIAPROXY_SOURCE_CACHE}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    target_link_libraries(gtest PRIVATE mediaproxy_hardening)
    target_link_libraries(gtest_main PRIVATE mediaproxy_hardening)

    add_executable(mediaproxy_smoke_test
        tests/smoke/build_test.cpp
        tests/smoke/boringssl_test.cpp
        tests/smoke/curl_test.cpp
        tests/smoke/lcms2_test.cpp
        tests/smoke/libexif_test.cpp
        tests/smoke/libexpat_test.cpp
        tests/smoke/libffi_test.cpp
        tests/smoke/glib_test.cpp
        tests/smoke/idna_test.cpp
        tests/smoke/libaom_test.cpp
        tests/smoke/libheif_test.cpp
        tests/smoke/libvips_test.cpp
        tests/smoke/pcre2_test.cpp
        tests/smoke/libjpeg_turbo_test.cpp
        tests/smoke/libnsgif_test.cpp
        tests/smoke/libpng_test.cpp
        tests/smoke/libwebp_test.cpp
        tests/smoke/nghttp2_test.cpp
        tests/smoke/yyjson_test.cpp
        tests/smoke/zlib_test.cpp
    )
    target_link_libraries(mediaproxy_smoke_test
        PRIVATE
            mediaproxy_hardening
            mediaproxy_warnings
            mediaproxy_http
            mediaproxy_libvips
            mediaproxy_curl
            mediaproxy_boringssl_ssl
            mediaproxy_lcms2
            mediaproxy_libexif
            mediaproxy_libexpat
            mediaproxy_libffi
            mediaproxy_gio
            mediaproxy_gthread
            mediaproxy_libaom
            mediaproxy_libheif
            mediaproxy_pcre2
            mediaproxy_libjpeg_turbo
            mediaproxy_libnsgif
            mediaproxy_libpng
            mediaproxy_libwebp_demux
            mediaproxy_libwebp_mux
            mediaproxy_nghttp2
            mediaproxy_yyjson
            mediaproxy_zlib
            GTest::gtest_main
    )
    target_compile_definitions(mediaproxy_smoke_test PRIVATE
        "MEDIAPROXY_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\""
    )

    include(GoogleTest)
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL CMAKE_SYSTEM_PROCESSOR)
        gtest_discover_tests(mediaproxy_smoke_test DISCOVERY_MODE PRE_TEST)
    endif()

    add_executable(mediaproxy_http_test
        tests/http/address_policy_test.cpp
        tests/http/curl_resolve_pin_test.cpp
        tests/http/dns_policy_test.cpp
        tests/http/event_test.cpp
        tests/http/query_test.cpp
        tests/http/response_test.cpp
        tests/http/selectors_test.cpp
        tests/http/url_policy_test.cpp
    )
    target_link_libraries(mediaproxy_http_test
        PRIVATE
            mediaproxy_hardening
            mediaproxy_warnings
            mediaproxy_http
            mediaproxy_yyjson
            GTest::gtest_main
    )
    target_compile_definitions(mediaproxy_http_test PRIVATE
        "MEDIAPROXY_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\""
    )
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL CMAKE_SYSTEM_PROCESSOR)
        gtest_discover_tests(
            mediaproxy_http_test
            DISCOVERY_MODE PRE_TEST
            NO_PRETTY_VALUES)
    endif()

    add_executable(mediaproxy_stack_smash_probe
        tests/hardening/stack_smash.cpp
    )
    add_executable(mediaproxy_cfi_violation_probe
        tests/hardening/cfi_violation.cpp
    )
    add_executable(mediaproxy_fortify_probe
        tests/hardening/fortify.cpp
    )
    foreach(hardening_probe IN ITEMS
            mediaproxy_stack_smash_probe
            mediaproxy_cfi_violation_probe
            mediaproxy_fortify_probe)
        target_link_libraries(${hardening_probe}
            PRIVATE
                mediaproxy_hardening
                mediaproxy_warnings
        )
    endforeach()

    add_test(
        NAME ada-idna-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DADA_IDNA_ARCHIVE=${MEDIAPROXY_ADA_IDNA_LIBRARY}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_ADA_IDNA_COMPILE_COMMANDS}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLICENSE_FILE=${MEDIAPROXY_ADA_IDNA_LICENSE}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/AdaIdnaBuildTest.cmake"
    )
    add_test(
        NAME boringssl-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_BORINGSSL_COMPILE_COMMANDS}"
            "-DCRYPTO_ARCHIVE=${MEDIAPROXY_BORINGSSL_CRYPTO_LIBRARY}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DSSL_ARCHIVE=${MEDIAPROXY_BORINGSSL_SSL_LIBRARY}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/BoringSslBuildTest.cmake"
    )
    add_test(
        NAME curl-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_CURL_COMPILE_COMMANDS}"
            "-DCONFIG_HEADER=${MEDIAPROXY_CURL_CONFIG_HEADER}"
            "-DCURL_ARCHIVE=${MEDIAPROXY_CURL_LIBRARY}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/CurlBuildTest.cmake"
    )
    add_test(
        NAME yyjson-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DARCHIVE=${MEDIAPROXY_YYJSON_LIBRARY}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_YYJSON_COMPILE_COMMANDS}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DNM=${MEDIAPROXY_NM}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/YyjsonBuildTest.cmake"
    )
    add_test(
        NAME nghttp2-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_NGHTTP2_COMPILE_COMMANDS}"
            "-DCONFIG_HEADER=${MEDIAPROXY_NGHTTP2_CONFIG_HEADER}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNGHTTP2_ARCHIVE=${MEDIAPROXY_NGHTTP2_LIBRARY}"
            "-DNM=${MEDIAPROXY_NM}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/Nghttp2BuildTest.cmake"
    )
    add_test(
        NAME libpng-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_LIBPNG_COMPILE_COMMANDS}"
            "-DCONFIG_HEADER=${MEDIAPROXY_LIBPNG_CONFIG_HEADER}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLIBPNG_ARCHIVE=${MEDIAPROXY_LIBPNG_LIBRARY}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/LibPngBuildTest.cmake"
    )
    add_test(
        NAME libjpeg-turbo-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_LIBJPEG_TURBO_COMPILE_COMMANDS}"
            "-DCONFIG_HEADER=${MEDIAPROXY_LIBJPEG_TURBO_CONFIG_HEADER}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DINTERNAL_CONFIG_HEADER=${MEDIAPROXY_LIBJPEG_TURBO_INTERNAL_CONFIG_HEADER}"
            "-DJPEG_ARCHIVE=${MEDIAPROXY_LIBJPEG_TURBO_LIBRARY}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/LibJpegTurboBuildTest.cmake"
    )
    add_test(
        NAME libnsgif-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_LIBNSGIF_COMPILE_COMMANDS}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLIBNSGIF_ARCHIVE=${MEDIAPROXY_LIBNSGIF_LIBRARY}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/LibNsgifBuildTest.cmake"
    )
    add_test(
        NAME libexif-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_LIBEXIF_COMPILE_COMMANDS}"
            "-DCONFIG_HEADER=${MEDIAPROXY_LIBEXIF_CONFIG_HEADER}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLIBEXIF_ARCHIVE=${MEDIAPROXY_LIBEXIF_LIBRARY}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DPKGCONFIG=${MEDIAPROXY_LIBEXIF_PKGCONFIG}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/LibExifBuildTest.cmake"
    )
    add_test(
        NAME libexpat-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_LIBEXPAT_COMPILE_COMMANDS}"
            "-DCONFIG_HEADER=${MEDIAPROXY_LIBEXPAT_CONFIG_HEADER}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLIBEXPAT_ARCHIVE=${MEDIAPROXY_LIBEXPAT_LIBRARY}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DPKGCONFIG=${MEDIAPROXY_LIBEXPAT_PKGCONFIG}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/LibExpatBuildTest.cmake"
    )
    add_test(
        NAME libffi-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_LIBFFI_COMPILE_COMMANDS}"
            "-DCONFIG_HEADER=${MEDIAPROXY_LIBFFI_CONFIG_HEADER}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLIBFFI_ARCHIVE=${MEDIAPROXY_LIBFFI_LIBRARY}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DPKGCONFIG=${MEDIAPROXY_LIBFFI_PKGCONFIG}"
            "-DREADELF=${MEDIAPROXY_READELF}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/LibFfiBuildTest.cmake"
    )
    add_test(
        NAME pcre2-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_PCRE2_COMPILE_COMMANDS}"
            "-DCONFIG_HEADER=${MEDIAPROXY_PCRE2_CONFIG_HEADER}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DPCRE2_ARCHIVE=${MEDIAPROXY_PCRE2_LIBRARY}"
            "-DPKGCONFIG=${MEDIAPROXY_PCRE2_PKGCONFIG}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/Pcre2BuildTest.cmake"
    )
    add_test(
        NAME glib-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_GLIB_COMPILE_COMMANDS}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DGIO_ARCHIVE=${MEDIAPROXY_GIO_LIBRARY}"
            "-DGIO_PKGCONFIG=${MEDIAPROXY_GIO_PKGCONFIG}"
            "-DGLIB_ARCHIVE=${MEDIAPROXY_GLIB_LIBRARY}"
            "-DGLIB_CONFIG_HEADER=${MEDIAPROXY_GLIB_CONFIG_HEADER}"
            "-DGLIB_PKGCONFIG=${MEDIAPROXY_GLIB_PKGCONFIG}"
            "-DGMODULE_ARCHIVE=${MEDIAPROXY_GMODULE_LIBRARY}"
            "-DGMODULE_CONFIG_HEADER=${MEDIAPROXY_GMODULE_CONFIG_HEADER}"
            "-DGMODULE_PKGCONFIG=${MEDIAPROXY_GMODULE_PKGCONFIG}"
            "-DGOBJECT_ARCHIVE=${MEDIAPROXY_GOBJECT_LIBRARY}"
            "-DGOBJECT_PKGCONFIG=${MEDIAPROXY_GOBJECT_PKGCONFIG}"
            "-DGTHREAD_ARCHIVE=${MEDIAPROXY_GTHREAD_LIBRARY}"
            "-DGTHREAD_PKGCONFIG=${MEDIAPROXY_GTHREAD_PKGCONFIG}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/GlibBuildTest.cmake"
    )
    add_test(
        NAME libaom-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_LIBAOM_COMPILE_COMMANDS}"
            "-DCONFIG_HEADER=${MEDIAPROXY_LIBAOM_CONFIG_HEADER}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLIBAOM_ARCHIVE=${MEDIAPROXY_LIBAOM_LIBRARY}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DPKGCONFIG=${MEDIAPROXY_LIBAOM_PKGCONFIG}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/LibaomBuildTest.cmake"
    )
    add_test(
        NAME libheif-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCMAKE_CACHE=${MEDIAPROXY_LIBHEIF_CMAKE_CACHE}"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_LIBHEIF_COMPILE_COMMANDS}"
            "-DCONFIG_HEADER=${MEDIAPROXY_LIBHEIF_CONFIG_HEADER}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLIBAOM_ARCHIVE=${MEDIAPROXY_LIBAOM_LIBRARY}"
            "-DLIBHEIF_ARCHIVE=${MEDIAPROXY_LIBHEIF_LIBRARY}"
            "-DLIBSHARPYUV_ARCHIVE=${MEDIAPROXY_LIBWEBP_SHARPYUV_LIBRARY}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DPKGCONFIG=${MEDIAPROXY_LIBHEIF_PKGCONFIG}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/LibheifBuildTest.cmake"
    )
    add_test(
        NAME libvips-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DBUILD_DIRECTORY=${MEDIAPROXY_LIBVIPS_BUILD_DIRECTORY}"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_LIBVIPS_COMPILE_COMMANDS}"
            "-DCONFIG_HEADER=${MEDIAPROXY_LIBVIPS_CONFIG_HEADER}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLIBVIPS_ARCHIVE=${MEDIAPROXY_LIBVIPS_LIBRARY}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DPKGCONFIG=${MEDIAPROXY_LIBVIPS_PKGCONFIG}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/LibvipsBuildTest.cmake"
    )
    add_test(
        NAME lcms2-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_LCMS2_COMPILE_COMMANDS}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLCMS2_ARCHIVE=${MEDIAPROXY_LCMS2_LIBRARY}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/Lcms2BuildTest.cmake"
    )
    add_test(
        NAME zlib-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_ZLIB_COMPILE_COMMANDS}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DZLIB_ARCHIVE=${MEDIAPROXY_ZLIB_LIBRARY}"
            "-DZLIB_PKGCONFIG=${MEDIAPROXY_ZLIB_PKGCONFIG}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/ZlibBuildTest.cmake"
    )
    add_test(
        NAME libwebp-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_LIBWEBP_COMPILE_COMMANDS}"
            "-DCONFIG_HEADER=${MEDIAPROXY_LIBWEBP_CONFIG_HEADER}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            "-DSHARPYUV_ARCHIVE=${MEDIAPROXY_LIBWEBP_SHARPYUV_LIBRARY}"
            "-DWEBP_ARCHIVE=${MEDIAPROXY_LIBWEBP_LIBRARY}"
            "-DWEBPDEMUX_ARCHIVE=${MEDIAPROXY_LIBWEBP_DEMUX_LIBRARY}"
            "-DWEBPMUX_ARCHIVE=${MEDIAPROXY_LIBWEBP_MUX_LIBRARY}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/LibWebPBuildTest.cmake"
    )
    add_test(
        NAME hardening-flags
        COMMAND "${CMAKE_COMMAND}"
            "-DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DCOMPILE_COMMANDS=${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DNINJA=${CMAKE_MAKE_PROGRAM}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/HardeningFlagsTest.cmake"
    )
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL CMAKE_SYSTEM_PROCESSOR)
        add_test(
            NAME hardening-fortify-safe
            COMMAND mediaproxy_fortify_probe
        )
        add_test(
            NAME hardening-fortify-overflow
            COMMAND "${CMAKE_COMMAND}"
                "-DPROGRAM=$<TARGET_FILE:mediaproxy_fortify_probe>"
                "-DPROGRAM_ARGUMENTS=overflow"
                "-DEXPECTED_RESULT=Illegal instruction"
                -P "${CMAKE_SOURCE_DIR}/tests/cmake/ExpectSignalFailure.cmake"
        )
        add_test(
            NAME hardening-stack-smash
            COMMAND "${CMAKE_COMMAND}"
                "-DPROGRAM=$<TARGET_FILE:mediaproxy_stack_smash_probe>"
                "-DEXPECTED_RESULT=Segmentation fault"
                -P "${CMAKE_SOURCE_DIR}/tests/cmake/ExpectSignalFailure.cmake"
        )
        add_test(
            NAME hardening-cfi-violation
            COMMAND "${CMAKE_COMMAND}"
                "-DPROGRAM=$<TARGET_FILE:mediaproxy_cfi_violation_probe>"
                "-DEXPECTED_RESULT=Illegal instruction"
                -P "${CMAKE_SOURCE_DIR}/tests/cmake/ExpectSignalFailure.cmake"
        )
    endif()

    add_test(
        NAME static-elf-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/StaticElfPolicyTest.cmake"
    )
    add_test(
        NAME bootstrap-static-elf
        COMMAND "${CMAKE_COMMAND}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DREADELF=${MEDIAPROXY_READELF}"
            "-DNM=${MEDIAPROXY_NM}"
            "-DUNDEFINED_SYMBOLS_FILE=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.undefined-symbols.txt"
            -P "${CMAKE_SOURCE_DIR}/cmake/VerifyStaticElf.cmake"
    )
    add_test(
        NAME release-binary-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/ReleaseBinaryPolicyTest.cmake"
    )
endif()
