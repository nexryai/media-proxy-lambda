foreach(required_variable
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        CONFIG_HEADER
        FORTIFY_INCLUDE_DIR
        LIBPNG_ARCHIVE
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
        "${LIBPNG_ARCHIVE}"
        "${LINK_MAP}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required libpng build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${LIBPNG_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0 OR archive_members STREQUAL "")
    message(FATAL_ERROR
        "Invalid libpng archive ${LIBPNG_ARCHIVE}: ${ar_error}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${LIBPNG_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0
        OR NOT archive_symbols MATCHES "png_create_read_struct"
        OR NOT archive_symbols MATCHES "png_image_begin_read_from_memory"
        OR NOT archive_symbols MATCHES "png_image_write_to_memory")
    message(FATAL_ERROR
        "libpng archive does not define required bounded APIs: ${nm_error}")
endif()

file(READ "${CONFIG_HEADER}" config_header)
foreach(required_definition IN ITEMS
        PNG_READ_SUPPORTED
        PNG_SETJMP_SUPPORTED
        PNG_SIMPLIFIED_READ_SUPPORTED
        PNG_SIMPLIFIED_WRITE_SUPPORTED
        PNG_USER_LIMITS_SUPPORTED
        PNG_WRITE_SUPPORTED)
    if(NOT config_header MATCHES
            "(^|\n)#define[ \t]+${required_definition}([ \t\n]|$)")
        message(FATAL_ERROR
            "libpng is missing required configuration definition: "
            "${required_definition}")
    endif()
endforeach()
if(NOT config_header MATCHES
        "(^|\n)#define[ \t]+PNG_ZLIB_VERNUM[ \t]+0x1320([ \t\n]|$)")
    message(FATAL_ERROR
        "libpng configuration does not pin zlib 1.3.2")
endif()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(command_count EQUAL 0)
    message(FATAL_ERROR "libpng compile command database is empty")
endif()

set(required_sources
    /pngread.c
    /pngrutil.c
    /pngwrite.c
)
if(TARGET_ARCH STREQUAL "x86_64")
    list(APPEND required_sources /intel/filter_sse2_intrinsics.c)
    set(required_arch_definition -DPNG_INTEL_SSE_OPT=1)
elseif(TARGET_ARCH STREQUAL "arm64")
    list(APPEND required_sources
        /arm/filter_neon_intrinsics.c
        /arm/palette_neon_intrinsics.c)
    set(required_arch_definition -DPNG_ARM_NEON_OPT=2)
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
        ${required_arch_definition}
        -D_FORTIFY_SOURCE=3
        -fPIC
        -fstack-protector-strong
        -ftrivial-auto-var-init=zero
        -fvisibility=hidden
        -flto=thin
        -fsanitize=cfi
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

get_filename_component(library_dir "${LIBPNG_ARCHIVE}" DIRECTORY)
file(GLOB forbidden_shared_libraries
    "${library_dir}/libpng.so*"
    "${library_dir}/libpng16.so*"
)
if(forbidden_shared_libraries)
    message(FATAL_ERROR
        "libpng installed shared libraries: ${forbidden_shared_libraries}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${LIBPNG_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${LIBPNG_ARCHIVE}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "png_access_version_number")
    message(FATAL_ERROR
        "bootstrap does not contain the linked libpng implementation: "
        "${bootstrap_nm_error}")
endif()

foreach(forbidden_dependency IN ITEMS
        libpng.so
        libpng16.so
        libz.so)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden libpng dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()
