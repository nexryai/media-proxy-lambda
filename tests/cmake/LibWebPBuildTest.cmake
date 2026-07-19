foreach(required_variable
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        CONFIG_HEADER
        FORTIFY_INCLUDE_DIR
        LINK_MAP
        NM
        SHARPYUV_ARCHIVE
        WEBP_ARCHIVE
        WEBPDEMUX_ARCHIVE
        WEBPMUX_ARCHIVE
        TARGET_ARCH
        TARGET_TRIPLE)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

foreach(required_file IN ITEMS
        "${BOOTSTRAP}"
        "${COMPILE_COMMANDS}"
        "${CONFIG_HEADER}"
        "${LINK_MAP}"
        "${SHARPYUV_ARCHIVE}"
        "${WEBP_ARCHIVE}"
        "${WEBPDEMUX_ARCHIVE}"
        "${WEBPMUX_ARCHIVE}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required libwebp build artifact is absent: ${required_file}")
    endif()
endforeach()

set(archives
    "${SHARPYUV_ARCHIVE}"
    "${WEBP_ARCHIVE}"
    "${WEBPDEMUX_ARCHIVE}"
    "${WEBPMUX_ARCHIVE}"
)
foreach(archive IN LISTS archives)
    execute_process(
        COMMAND "${AR}" t "${archive}"
        RESULT_VARIABLE ar_result
        OUTPUT_VARIABLE archive_members
        ERROR_VARIABLE ar_error
    )
    if(NOT ar_result EQUAL 0 OR archive_members STREQUAL "")
        message(FATAL_ERROR "Invalid libwebp archive ${archive}: ${ar_error}")
    endif()
endforeach()

function(require_archive_symbols archive)
    execute_process(
        COMMAND "${NM}" --defined-only --format=posix "${archive}"
        RESULT_VARIABLE nm_result
        OUTPUT_VARIABLE archive_symbols
        ERROR_VARIABLE nm_error
    )
    if(NOT nm_result EQUAL 0)
        message(FATAL_ERROR "Cannot inspect ${archive}: ${nm_error}")
    endif()
    foreach(required_symbol IN LISTS ARGN)
        if(NOT archive_symbols MATCHES "${required_symbol}")
            message(FATAL_ERROR
                "${archive} does not define required symbol ${required_symbol}")
        endif()
    endforeach()
endfunction()

require_archive_symbols("${SHARPYUV_ARCHIVE}"
    SharpYuvGetVersion)
require_archive_symbols("${WEBP_ARCHIVE}"
    WebPDecodeRGBA
    WebPEncodeLosslessRGBA
    WebPGetDecoderVersion
    WebPGetEncoderVersion)
require_archive_symbols("${WEBPDEMUX_ARCHIVE}"
    WebPDemuxInternal
    WebPGetDemuxVersion)
require_archive_symbols("${WEBPMUX_ARCHIVE}"
    WebPAnimEncoderAssemble
    WebPAnimEncoderNewInternal
    WebPGetMuxVersion
    WebPMuxAssemble
    WebPMuxPushFrame)

file(READ "${CONFIG_HEADER}" config_header)
foreach(forbidden_feature IN ITEMS
        WEBP_HAVE_GIF
        WEBP_HAVE_GL
        WEBP_HAVE_JPEG
        WEBP_HAVE_PNG
        WEBP_HAVE_SDL
        WEBP_HAVE_TIFF
        WEBP_NEAR_LOSSLESS)
    if(config_header MATCHES
            "(^|\n)#define[ \t]+${forbidden_feature}([ \t\n]|$)")
        message(FATAL_ERROR
            "libwebp unexpectedly enables ${forbidden_feature}")
    endif()
endforeach()
if(NOT config_header MATCHES
        "(^|\n)#define[ \t]+WEBP_USE_THREAD[ \t]+1([ \t\n]|$)")
    message(FATAL_ERROR "libwebp does not enable musl thread support")
endif()
if(TARGET_ARCH STREQUAL "x86_64")
    foreach(required_feature IN ITEMS
            WEBP_HAVE_AVX2
            WEBP_HAVE_SSE2
            WEBP_HAVE_SSE41)
        if(NOT config_header MATCHES
                "(^|\n)#define[ \t]+${required_feature}[ \t]+1([ \t\n]|$)")
            message(FATAL_ERROR
                "x86_64 libwebp does not enable ${required_feature}")
        endif()
    endforeach()
elseif(TARGET_ARCH STREQUAL "arm64")
    if(NOT config_header MATCHES
            "(^|\n)#define[ \t]+WEBP_HAVE_NEON([ \t\n]|$)")
        message(FATAL_ERROR "arm64 libwebp does not enable NEON")
    endif()
endif()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(command_count EQUAL 0)
    message(FATAL_ERROR "libwebp compile command database is empty")
endif()

set(required_sources
    /sharpyuv/sharpyuv.c
    /src/dec/vp8_dec.c
    /src/enc/webp_enc.c
    /src/demux/demux.c
    /src/mux/anim_encode.c
)
if(TARGET_ARCH STREQUAL "x86_64")
    list(APPEND required_sources /src/dsp/lossless_avx2.c)
elseif(TARGET_ARCH STREQUAL "arm64")
    list(APPEND required_sources /src/dsp/lossless_neon.c)
else()
    message(FATAL_ERROR "Unsupported TARGET_ARCH: ${TARGET_ARCH}")
endif()

foreach(required_source IN LISTS required_sources)
    set(matching_command "")
    math(EXPR last_command "${command_count} - 1")
    foreach(index RANGE 0 ${last_command})
        string(JSON source_file GET "${compile_commands_json}" ${index} file)
        if(source_file MATCHES "${required_source}$")
            string(JSON matching_command GET
                "${compile_commands_json}" ${index} command)
            break()
        endif()
    endforeach()
    if(matching_command STREQUAL "")
        message(FATAL_ERROR "No compile command found for ${required_source}")
    endif()

    set(required_flags
        "--target=${TARGET_TRIPLE}"
        -D_FORTIFY_SOURCE=3
        -fPIC
        -fstack-protector-strong
        -ftrivial-auto-var-init=zero
        -fvisibility=hidden
        -flto=thin
        -fsanitize=cfi
        -fno-sanitize=cfi-icall
        -fsanitize-trap=cfi
        -fno-sanitize-recover=cfi
        -Wall
        -Wextra
        -Werror
        "-isystem ${FORTIFY_INCLUDE_DIR}"
    )
    if(TARGET_ARCH STREQUAL "x86_64")
        list(APPEND required_flags
            -fstack-clash-protection
            -fcf-protection=full)
        if(required_source MATCHES "_avx2\\.c$")
            list(APPEND required_flags -mavx2)
        endif()
    else()
        list(APPEND required_flags -mbranch-protection=standard)
    endif()

    foreach(required_flag IN LISTS required_flags)
        string(FIND "${matching_command}" "${required_flag}" flag_offset)
        if(flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${required_source} is missing build flag ${required_flag}: "
                "${matching_command}")
        endif()
    endforeach()
    string(REPLACE "-fno-sanitize=cfi-icall" ""
        command_without_icall_exception "${matching_command}")
    foreach(forbidden_flag IN ITEMS
            -fno-lto
            -fno-sanitize=cfi
            -fno-sanitize-trap=cfi
            -fno-stack-protector)
        string(FIND "${command_without_icall_exception}"
            "${forbidden_flag}" flag_offset)
        if(NOT flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${required_source} contains forbidden build flag "
                "${forbidden_flag}: ${matching_command}")
        endif()
    endforeach()
endforeach()

get_filename_component(library_dir "${WEBP_ARCHIVE}" DIRECTORY)
get_filename_component(build_dir "${COMPILE_COMMANDS}" DIRECTORY)
file(GLOB forbidden_shared_libraries
    "${library_dir}/libsharpyuv.so*"
    "${library_dir}/libwebp.so*"
    "${library_dir}/libwebpdemux.so*"
    "${library_dir}/libwebpmux.so*"
)
if(forbidden_shared_libraries)
    message(FATAL_ERROR
        "libwebp installed shared libraries: ${forbidden_shared_libraries}")
endif()
file(GLOB forbidden_tools
    "${build_dir}/anim_diff"
    "${build_dir}/anim_dump"
    "${build_dir}/cwebp"
    "${build_dir}/dwebp"
    "${build_dir}/gif2webp"
    "${build_dir}/img2webp"
    "${build_dir}/vwebp"
    "${build_dir}/webpinfo"
    "${build_dir}/webpmux"
)
if(forbidden_tools)
    message(FATAL_ERROR "libwebp built forbidden tools: ${forbidden_tools}")
endif()

file(READ "${LINK_MAP}" link_map)
foreach(archive IN LISTS archives)
    get_filename_component(archive_name "${archive}" NAME)
    string(REPLACE "." "\\." archive_regex "${archive_name}")
    if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
        message(FATAL_ERROR "bootstrap does not contain ${archive}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "SharpYuvGetVersion"
        OR NOT bootstrap_symbols MATCHES "WebPGetDecoderVersion"
        OR NOT bootstrap_symbols MATCHES "WebPGetDemuxVersion"
        OR NOT bootstrap_symbols MATCHES "WebPGetEncoderVersion"
        OR NOT bootstrap_symbols MATCHES "WebPGetMuxVersion")
    message(FATAL_ERROR
        "bootstrap does not contain the linked libwebp implementation: "
        "${bootstrap_nm_error}")
endif()

foreach(forbidden_dependency IN ITEMS
        libsharpyuv.so
        libwebp.so
        libwebpdemux.so
        libwebpmux.so)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden libwebp dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()
