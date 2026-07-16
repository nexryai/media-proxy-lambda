foreach(required_variable IN ITEMS
        AR
        BOOTSTRAP
        CMAKE_CACHE
        COMPILE_COMMANDS
        CONFIG_HEADER
        FORTIFY_INCLUDE_DIR
        LIBAOM_ARCHIVE
        LIBHEIF_ARCHIVE
        LIBSHARPYUV_ARCHIVE
        LINK_MAP
        NM
        PKGCONFIG
        TARGET_ARCH
        TARGET_TRIPLE)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

foreach(required_file IN ITEMS
        "${BOOTSTRAP}"
        "${CMAKE_CACHE}"
        "${COMPILE_COMMANDS}"
        "${CONFIG_HEADER}"
        "${LIBAOM_ARCHIVE}"
        "${LIBHEIF_ARCHIVE}"
        "${LIBSHARPYUV_ARCHIVE}"
        "${LINK_MAP}"
        "${PKGCONFIG}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required libheif build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${LIBHEIF_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error)
if(NOT ar_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect libheif: ${ar_error}")
endif()
string(REGEX MATCHALL "[^\n]+" archive_member_list "${archive_members}")
list(LENGTH archive_member_list archive_member_count)
if(NOT archive_member_count EQUAL 98)
    message(FATAL_ERROR
        "libheif contains ${archive_member_count} objects instead of 98")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${LIBHEIF_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect libheif symbols: ${nm_error}")
endif()
foreach(required_symbol IN ITEMS
        heif_get_version
        heif_init
        heif_deinit
        heif_context_alloc
        heif_context_free
        heif_have_decoder_for_format
        heif_have_encoder_for_format
        heif_context_get_encoder_for_format)
    if(NOT archive_symbols MATCHES "(^|\n)${required_symbol} ")
        message(FATAL_ERROR "libheif does not define ${required_symbol}")
    endif()
endforeach()
if(archive_symbols MATCHES "(^|\n)[^ ]*(libde265|x265)[^ ]* ")
    message(FATAL_ERROR "libheif contains an HEVC codec backend symbol")
endif()

execute_process(
    COMMAND "${NM}" --undefined-only --format=posix "${LIBHEIF_ARCHIVE}"
    RESULT_VARIABLE undefined_nm_result
    OUTPUT_VARIABLE undefined_symbols
    ERROR_VARIABLE undefined_nm_error)
if(NOT undefined_nm_result EQUAL 0)
    message(FATAL_ERROR
        "Cannot inspect undefined libheif symbols: ${undefined_nm_error}")
endif()
if(undefined_symbols MATCHES "(^|\n)(dlopen|dlsym|dlclose|dlerror) ")
    message(FATAL_ERROR "libheif retains a runtime plugin-loading symbol")
endif()

file(READ "${CONFIG_HEADER}" version_header)
foreach(required_version IN ITEMS
        "#define LIBHEIF_VERSION \"1.22.2\""
        "#define LIBHEIF_NUMERIC_VERSION ((1<<24) | (22<<16) | (2<<8) | 0)")
    string(FIND "${version_header}" "${required_version}" version_offset)
    if(version_offset EQUAL -1)
        message(FATAL_ERROR
            "heif_version.h is missing ${required_version}")
    endif()
endforeach()

file(READ "${CMAKE_CACHE}" cmake_cache)
foreach(required_cache_entry IN ITEMS
        "BUILD_SHARED_LIBS:BOOL=OFF"
        "ENABLE_PLUGIN_LOADING:BOOL=OFF"
        "WITH_AOM_DECODER:BOOL=ON"
        "WITH_AOM_DECODER_PLUGIN:BOOL=OFF"
        "WITH_AOM_ENCODER:BOOL=ON"
        "WITH_AOM_ENCODER_PLUGIN:BOOL=OFF"
        "WITH_LIBSHARPYUV:BOOL=ON"
        "WITH_LIBSHARPYUV_INTERNAL:BOOL=OFF"
        "WITH_LIBDE265:BOOL=OFF"
        "WITH_X265:BOOL=OFF"
        "WITH_KVAZAAR:BOOL=OFF"
        "WITH_FFMPEG_DECODER:BOOL=OFF"
        "WITH_WEBCODECS:BOOL=OFF"
        "WITH_X264:BOOL=OFF"
        "WITH_OpenH264_DECODER:BOOL=OFF"
        "WITH_UVG266:BOOL=OFF"
        "WITH_VVDEC:BOOL=OFF"
        "WITH_VVENC:BOOL=OFF"
        "WITH_DAV1D:BOOL=OFF"
        "WITH_SvtEnc:BOOL=OFF"
        "WITH_RAV1E:BOOL=OFF"
        "WITH_JPEG_DECODER:BOOL=OFF"
        "WITH_JPEG_ENCODER:BOOL=OFF"
        "WITH_OpenJPEG_DECODER:BOOL=OFF"
        "WITH_OpenJPEG_ENCODER:BOOL=OFF"
        "WITH_OPENJPH_ENCODER:BOOL=OFF"
        "WITH_UNCOMPRESSED_CODEC:BOOL=OFF"
        "WITH_HEADER_COMPRESSION:BOOL=OFF"
        "ENABLE_EXPERIMENTAL_FEATURES:BOOL=OFF"
        "BUILD_TESTING:BOOL=OFF"
        "WITH_FUZZERS:BOOL=OFF")
    string(FIND "${cmake_cache}" "${required_cache_entry}" cache_offset)
    if(cache_offset EQUAL -1)
        message(FATAL_ERROR
            "libheif CMake cache is missing ${required_cache_entry}")
    endif()
endforeach()

file(READ "${PKGCONFIG}" pkgconfig)
foreach(required_line IN ITEMS
        "Name: libheif"
        "Version: 1.22.2"
        "Libs: -L\${libdir} -lheif"
        "Requires.private: aom libsharpyuv")
    string(FIND "${pkgconfig}" "${required_line}" pc_offset)
    if(pc_offset EQUAL -1)
        message(FATAL_ERROR
            "libheif.pc is missing ${required_line}: ${pkgconfig}")
    endif()
endforeach()
if(pkgconfig MATCHES "(libde265|x265|ffmpeg|openh264|x264|vvdec|vvenc)")
    message(FATAL_ERROR "libheif.pc advertises a forbidden codec dependency")
endif()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
math(EXPR last_command "${command_count} - 1")
set(libheif_library_command_count 0)
foreach(index RANGE 0 ${last_command})
    string(JSON output GET "${compile_commands_json}" ${index} output)
    if(NOT output MATCHES "/CMakeFiles/heif\.dir/")
        continue()
    endif()
    math(EXPR libheif_library_command_count
        "${libheif_library_command_count} + 1")
    string(JSON source_file GET "${compile_commands_json}" ${index} file)
    string(JSON compile_command GET "${compile_commands_json}" ${index} command)
    if(source_file MATCHES
            "(decoder_libde265|encoder_x265|encoder_kvazaar|decoder_ffmpeg|plugins_unix|plugins_windows)\\.cc$")
        message(FATAL_ERROR
            "libheif compiles a forbidden codec/plugin source: ${source_file}")
    endif()
    foreach(required_flag IN ITEMS
            "--target=${TARGET_TRIPLE}"
            --sysroot=
            -D_FORTIFY_SOURCE=3
            -fPIC
            -fstack-protector-strong
            -ftrivial-auto-var-init=zero
            -fvisibility=hidden
            -ffunction-sections
            -fdata-sections
            -flto=thin
            -fsanitize=cfi
            -fno-sanitize=cfi-icall
            -fsanitize-trap=cfi
            -fno-sanitize-recover=cfi
            -std=c++20
            -Wall
            -Werror
            "-isystem ${FORTIFY_INCLUDE_DIR}")
        string(FIND "${compile_command}" "${required_flag}" flag_offset)
        if(flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${output} is missing ${required_flag}: ${compile_command}")
        endif()
    endforeach()
    if(TARGET_ARCH STREQUAL "x86_64")
        set(required_arch_flags -fstack-clash-protection -fcf-protection=full)
    elseif(TARGET_ARCH STREQUAL "arm64")
        set(required_arch_flags -mbranch-protection=standard)
    else()
        message(FATAL_ERROR "Unsupported TARGET_ARCH: ${TARGET_ARCH}")
    endif()
    foreach(required_flag IN LISTS required_arch_flags)
        string(FIND "${compile_command}" "${required_flag}" flag_offset)
        if(flag_offset EQUAL -1)
            message(FATAL_ERROR "${output} is missing ${required_flag}")
        endif()
    endforeach()
    foreach(forbidden_flag IN ITEMS
            ENABLE_PLUGIN_LOADING
            HAVE_LIBDE265
            HAVE_X265
            -U_FORTIFY_SOURCE
            -D_FORTIFY_SOURCE=0
            -fno-lto
            -fno-stack-protector
            -ffast-math)
        string(FIND "${compile_command}" "${forbidden_flag}" flag_offset)
        if(NOT flag_offset EQUAL -1)
            message(FATAL_ERROR "${output} contains forbidden ${forbidden_flag}")
        endif()
    endforeach()
endforeach()
if(NOT libheif_library_command_count EQUAL 98)
    message(FATAL_ERROR
        "libheif configured ${libheif_library_command_count} hardened archive "
        "compile commands instead of 98")
endif()

get_filename_component(library_dir "${LIBHEIF_ARCHIVE}" DIRECTORY)
get_filename_component(build_dir "${COMPILE_COMMANDS}" DIRECTORY)
file(GLOB forbidden_outputs
    "${library_dir}/libheif.so*"
    "${library_dir}/libheif/*.so*"
    "${build_dir}/examples/*"
    "${build_dir}/gdk-pixbuf/*")
if(forbidden_outputs)
    message(FATAL_ERROR "libheif built forbidden outputs: ${forbidden_outputs}")
endif()

file(READ "${LINK_MAP}" link_map)
foreach(required_archive IN ITEMS
        "${LIBAOM_ARCHIVE}"
        "${LIBHEIF_ARCHIVE}"
        "${LIBSHARPYUV_ARCHIVE}")
    get_filename_component(archive_name "${required_archive}" NAME)
    string(REPLACE "." "\\." archive_regex "${archive_name}")
    if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}" AND
            NOT link_map MATCHES "${archive_regex}")
        message(FATAL_ERROR "bootstrap does not contain ${required_archive}")
    endif()
endforeach()
foreach(required_symbol IN ITEMS
        heif_get_version
        heif_init
        heif_have_decoder_for_format
        heif_have_encoder_for_format)
    if(NOT link_map MATCHES "${required_symbol}")
        message(FATAL_ERROR "bootstrap link map omits ${required_symbol}")
    endif()
endforeach()
