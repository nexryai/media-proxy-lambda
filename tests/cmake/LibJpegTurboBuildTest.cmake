foreach(required_variable
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        CONFIG_HEADER
        FORTIFY_INCLUDE_DIR
        INTERNAL_CONFIG_HEADER
        JPEG_ARCHIVE
        LINK_MAP
        NM
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
        "${INTERNAL_CONFIG_HEADER}"
        "${JPEG_ARCHIVE}"
        "${LINK_MAP}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required libjpeg-turbo build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${JPEG_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0 OR archive_members STREQUAL "")
    message(FATAL_ERROR
        "Invalid libjpeg-turbo archive ${JPEG_ARCHIVE}: ${ar_error}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${JPEG_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect ${JPEG_ARCHIVE}: ${nm_error}")
endif()
foreach(required_symbol IN ITEMS
        jpeg_CreateCompress
        jpeg_CreateDecompress
        jpeg12_read_scanlines
        jpeg16_read_scanlines
        jpeg_mem_dest
        jpeg_mem_src
        jpeg_read_header
        jpeg_std_error
        jpeg_write_scanlines)
    if(NOT archive_symbols MATCHES "${required_symbol}")
        message(FATAL_ERROR
            "libjpeg-turbo does not define ${required_symbol}")
    endif()
endforeach()

file(READ "${CONFIG_HEADER}" config_header)
foreach(required_definition IN ITEMS
        "JPEG_LIB_VERSION[ \t]+62"
        "LIBJPEG_TURBO_VERSION[ \t]+3\\.1\\.4\\.1"
        "LIBJPEG_TURBO_VERSION_NUMBER[ \t]+3001004"
        "D_ARITH_CODING_SUPPORTED[ \t]+1"
        "MEM_SRCDST_SUPPORTED[ \t]+1"
        "BITS_IN_JSAMPLE[ \t]+8")
    if(NOT config_header MATCHES
            "(^|\n)#define[ \t]+${required_definition}([ \t\n]|$)")
        message(FATAL_ERROR
            "libjpeg-turbo configuration is missing ${required_definition}")
    endif()
endforeach()
foreach(forbidden_definition IN ITEMS
        C_ARITH_CODING_SUPPORTED
        WITH_SIMD)
    if(config_header MATCHES
            "(^|\n)#define[ \t]+${forbidden_definition}([ \t\n]|$)")
        message(FATAL_ERROR
            "libjpeg-turbo unexpectedly enables ${forbidden_definition}")
    endif()
endforeach()

file(READ "${INTERNAL_CONFIG_HEADER}" internal_config_header)
if(NOT internal_config_header MATCHES
        "(^|\n)#define[ \t]+SIZEOF_SIZE_T[ \t]+8([ \t\n]|$)")
    message(FATAL_ERROR
        "libjpeg-turbo internal configuration does not use 64-bit size_t")
endif()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(command_count EQUAL 0)
    message(FATAL_ERROR "libjpeg-turbo compile command database is empty")
endif()

set(required_sources
    /src/jcapimin.c
    /src/jdapimin.c
    /src/jdarith.c
    /src/jmemmgr.c
    /src/wrapper/jcapistd-8.c
)
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
        -fsanitize-trap=cfi
        -fno-sanitize-recover=cfi
        -ffp-contract=off
        -Wall
        -Wextra
        -Werror
        -Wno-unused-parameter
        "-isystem ${FORTIFY_INCLUDE_DIR}"
    )
    if(TARGET_ARCH STREQUAL "x86_64")
        list(APPEND required_flags
            -fstack-clash-protection
            -fcf-protection=full)
    elseif(TARGET_ARCH STREQUAL "arm64")
        list(APPEND required_flags -mbranch-protection=standard)
    else()
        message(FATAL_ERROR "Unsupported TARGET_ARCH: ${TARGET_ARCH}")
    endif()

    foreach(required_flag IN LISTS required_flags)
        string(FIND "${matching_command}" "${required_flag}" flag_offset)
        if(flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${required_source} is missing build flag ${required_flag}: "
                "${matching_command}")
        endif()
    endforeach()
    foreach(forbidden_flag IN ITEMS
            -fno-lto
            -fno-sanitize=cfi
            -fno-sanitize-trap=cfi
            -fno-stack-protector)
        string(FIND "${matching_command}" "${forbidden_flag}" flag_offset)
        if(NOT flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${required_source} contains forbidden build flag "
                "${forbidden_flag}: ${matching_command}")
        endif()
    endforeach()
endforeach()

math(EXPR last_command "${command_count} - 1")
foreach(index RANGE 0 ${last_command})
    string(JSON source_file GET "${compile_commands_json}" ${index} file)
    if(source_file MATCHES "/simd/" OR NOT source_file MATCHES "\\.c$")
        message(FATAL_ERROR
            "libjpeg-turbo compiled a non-C or SIMD source: ${source_file}")
    endif()
endforeach()

get_filename_component(library_dir "${JPEG_ARCHIVE}" DIRECTORY)
get_filename_component(build_dir "${COMPILE_COMMANDS}" DIRECTORY)
file(GLOB forbidden_shared_libraries
    "${library_dir}/libjpeg.so*"
    "${library_dir}/libturbojpeg.so*"
)
if(forbidden_shared_libraries)
    message(FATAL_ERROR
        "libjpeg-turbo installed shared libraries: ${forbidden_shared_libraries}")
endif()
file(GLOB forbidden_outputs
    "${build_dir}/cjpeg*"
    "${build_dir}/djpeg*"
    "${build_dir}/jpegtran*"
    "${build_dir}/libturbojpeg*"
    "${build_dir}/tjbench*"
)
if(forbidden_outputs)
    message(FATAL_ERROR
        "libjpeg-turbo built forbidden outputs: ${forbidden_outputs}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${JPEG_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${JPEG_ARCHIVE}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "jpeg_std_error")
    message(FATAL_ERROR
        "bootstrap does not contain the linked libjpeg-turbo implementation: "
        "${bootstrap_nm_error}")
endif()

foreach(forbidden_dependency IN ITEMS
        libjpeg.so
        libturbojpeg
        libstdc++)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden JPEG dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()
