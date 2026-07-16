foreach(required_variable IN ITEMS
        AR
        BOOTSTRAP
        BUILD_DIRECTORY
        COMPILE_COMMANDS
        CONFIG_HEADER
        FORTIFY_INCLUDE_DIR
        LIBVIPS_ARCHIVE
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
        "${COMPILE_COMMANDS}"
        "${CONFIG_HEADER}"
        "${LIBVIPS_ARCHIVE}"
        "${LINK_MAP}"
        "${PKGCONFIG}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required libvips build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${LIBVIPS_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error)
if(NOT ar_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect libvips: ${ar_error}")
endif()
string(REGEX MATCHALL "[^\n]+" archive_member_list "${archive_members}")
list(LENGTH archive_member_list archive_member_count)
if(NOT archive_member_count EQUAL 359)
    message(FATAL_ERROR
        "libvips contains ${archive_member_count} objects instead of 359")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${LIBVIPS_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect libvips symbols: ${nm_error}")
endif()
foreach(required_symbol IN ITEMS
        vips_init
        vips_shutdown
        vips_black
        vips_resize
        vips_heifload_buffer
        vips_heifsave_buffer
        vips_jpegload_buffer
        vips_pngload_buffer
        vips_webpload_buffer)
    if(NOT archive_symbols MATCHES "(^|\n)${required_symbol} ")
        message(FATAL_ERROR "libvips does not define ${required_symbol}")
    endif()
endforeach()
if(archive_symbols MATCHES "(^|\n)[^ ]*(libde265|x265)[^ ]* ")
    message(FATAL_ERROR "libvips contains an HEVC codec backend symbol")
endif()

execute_process(
    COMMAND "${NM}" --undefined-only --format=posix "${LIBVIPS_ARCHIVE}"
    RESULT_VARIABLE undefined_nm_result
    OUTPUT_VARIABLE undefined_symbols
    ERROR_VARIABLE undefined_nm_error)
if(NOT undefined_nm_result EQUAL 0)
    message(FATAL_ERROR
        "Cannot inspect undefined libvips symbols: ${undefined_nm_error}")
endif()
if(undefined_symbols MATCHES
        "(^|\n)(dlopen|dlsym|dlclose|dlerror|de265_[^ ]*|x265_[^ ]*) ")
    message(FATAL_ERROR
        "libvips retains a plugin loader or HEVC backend reference")
endif()

file(READ "${CONFIG_HEADER}" config_header)
foreach(required_definition IN ITEMS
        "#undef ENABLE_MODULES"
        "#define HAVE_HEIF"
        "#define HAVE_JPEG"
        "#define HAVE_LCMS2"
        "#define HAVE_LIBWEBP"
        "#define HAVE_NSGIF"
        "#define HAVE_PNG"
        "#define HAVE_ZLIB"
        "#undef HAVE_ANALYZE"
        "#undef HAVE_PPM"
        "#undef HAVE_RADIANCE")
    string(FIND "${config_header}" "${required_definition}" definition_offset)
    if(definition_offset EQUAL -1)
        message(FATAL_ERROR
            "libvips config.h is missing ${required_definition}")
    endif()
endforeach()
foreach(forbidden_definition IN ITEMS
        HAVE_CAIRO
        HAVE_CFITSIO
        HAVE_FFTW
        HAVE_JXL
        HAVE_LIBARCHIVE
        HAVE_MAGICK
        HAVE_OPENEXR
        HAVE_OPENJPEG
        HAVE_OPENSLIDE
        HAVE_ORC
        HAVE_PANGOFT2
        HAVE_PDFIUM
        HAVE_POPPLER
        HAVE_RSVG
        HAVE_TIFF)
    if(config_header MATCHES "#define ${forbidden_definition}([ \n]|$)")
        message(FATAL_ERROR
            "libvips unexpectedly enables ${forbidden_definition}")
    endif()
endforeach()

file(READ "${PKGCONFIG}" pkgconfig)
foreach(required_line IN ITEMS
        "Name: vips"
        "Version: 8.18.2"
        "Libs: -L\${libdir} -lvips"
        "libheif >= 1.7.0"
        "libjpeg"
        "libpng >= 1.2.9"
        "libwebp >= 0.6")
    string(FIND "${pkgconfig}" "${required_line}" pc_offset)
    if(pc_offset EQUAL -1)
        message(FATAL_ERROR
            "vips.pc is missing ${required_line}: ${pkgconfig}")
    endif()
endforeach()
if(pkgconfig MATCHES
        "(libde265|x265|Magick|libtiff|libjxl|openjp|poppler|rsvg)")
    message(FATAL_ERROR "vips.pc advertises a forbidden dependency")
endif()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
math(EXPR last_command "${command_count} - 1")
set(libvips_library_command_count 0)
foreach(index RANGE 0 ${last_command})
    string(JSON output GET "${compile_commands_json}" ${index} output)
    if(NOT output MATCHES "^libvips/")
        continue()
    endif()
    math(EXPR libvips_library_command_count
        "${libvips_library_command_count} + 1")
    string(JSON source_file GET "${compile_commands_json}" ${index} file)
    string(JSON compile_command GET "${compile_commands_json}" ${index} command)
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
            -Werror
            "-isystem ${FORTIFY_INCLUDE_DIR}")
        string(FIND "${compile_command}" "${required_flag}" flag_offset)
        if(flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${output} is missing ${required_flag}: ${compile_command}")
        endif()
    endforeach()
    if(source_file MATCHES "\\.(cc|cpp|cxx)$")
        string(FIND "${compile_command}" "-std=c++20" standard_offset)
        if(standard_offset EQUAL -1)
            message(FATAL_ERROR "${output} is not compiled as C++20")
        endif()
    endif()
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
if(NOT libvips_library_command_count EQUAL 359)
    message(FATAL_ERROR
        "libvips configured ${libvips_library_command_count} hardened "
        "archive compile commands instead of 359")
endif()

file(GLOB_RECURSE forbidden_outputs
    "${BUILD_DIRECTORY}/*.so"
    "${BUILD_DIRECTORY}/*.so.*")
foreach(forbidden_executable IN ITEMS
        vips
        vipsedit
        vipsheader
        vipsthumbnail)
    if(EXISTS "${BUILD_DIRECTORY}/tools/${forbidden_executable}")
        list(APPEND forbidden_outputs
            "${BUILD_DIRECTORY}/tools/${forbidden_executable}")
    endif()
endforeach()
if(forbidden_outputs)
    message(FATAL_ERROR "libvips built forbidden outputs: ${forbidden_outputs}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${LIBVIPS_ARCHIVE}" NAME)
if(NOT link_map MATCHES "${archive_name}")
    message(FATAL_ERROR "bootstrap does not contain ${LIBVIPS_ARCHIVE}")
endif()
foreach(required_symbol IN ITEMS
        vips_init
        vips_type_find
        vips_shutdown)
    if(NOT link_map MATCHES "${required_symbol}")
        message(FATAL_ERROR "bootstrap link map omits ${required_symbol}")
    endif()
endforeach()
