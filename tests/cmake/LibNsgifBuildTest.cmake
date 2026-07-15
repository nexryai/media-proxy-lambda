foreach(required_variable
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        FORTIFY_INCLUDE_DIR
        LIBNSGIF_ARCHIVE
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
        "${LIBNSGIF_ARCHIVE}"
        "${LINK_MAP}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required libnsgif build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${LIBNSGIF_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect ${LIBNSGIF_ARCHIVE}: ${ar_error}")
endif()
foreach(required_member IN ITEMS gif.c.o lzw.c.o)
    string(FIND "\n${archive_members}" "\n${required_member}\n" member_offset)
    if(member_offset EQUAL -1)
        message(FATAL_ERROR
            "libnsgif archive is missing ${required_member}: ${archive_members}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${LIBNSGIF_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect ${LIBNSGIF_ARCHIVE}: ${nm_error}")
endif()
foreach(required_symbol IN ITEMS
        lzw_context_create
        nsgif_create
        nsgif_data_scan
        nsgif_destroy
        nsgif_frame_decode
        nsgif_frame_prepare
        nsgif_get_info
        nsgif_strerror)
    if(NOT archive_symbols MATCHES "${required_symbol}")
        message(FATAL_ERROR "libnsgif does not define ${required_symbol}")
    endif()
endforeach()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(NOT command_count EQUAL 2)
    message(FATAL_ERROR
        "libnsgif must compile exactly gif.c and lzw.c, found ${command_count}")
endif()

set(required_sources /src/gif.c /src/lzw.c)
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
        -std=c99
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
                "${required_source} is missing ${required_flag}: "
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
                "${required_source} contains forbidden ${forbidden_flag}")
        endif()
    endforeach()
endforeach()

get_filename_component(library_dir "${LIBNSGIF_ARCHIVE}" DIRECTORY)
get_filename_component(build_dir "${COMPILE_COMMANDS}" DIRECTORY)
file(GLOB forbidden_shared_libraries "${library_dir}/libnsgif.so*")
if(forbidden_shared_libraries)
    message(FATAL_ERROR
        "libnsgif installed shared libraries: ${forbidden_shared_libraries}")
endif()
file(GLOB forbidden_outputs
    "${build_dir}/nsgif"
    "${build_dir}/test/*"
)
if(forbidden_outputs)
    message(FATAL_ERROR "libnsgif built forbidden tools: ${forbidden_outputs}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${LIBNSGIF_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${LIBNSGIF_ARCHIVE}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "nsgif_strerror")
    message(FATAL_ERROR
        "bootstrap does not contain libnsgif: ${bootstrap_nm_error}")
endif()

foreach(forbidden_dependency IN ITEMS libnsgif.so libstdc++)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden GIF dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()
