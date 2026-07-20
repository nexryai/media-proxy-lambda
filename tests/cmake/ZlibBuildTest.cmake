foreach(required_variable
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        FORTIFY_INCLUDE_DIR
        LINK_MAP
        NM
        TARGET_ARCH
        TARGET_TRIPLE
        ZLIB_ARCHIVE
        ZLIB_PKGCONFIG)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

foreach(required_file IN ITEMS
        "${BOOTSTRAP}"
        "${COMPILE_COMMANDS}"
        "${LINK_MAP}"
        "${ZLIB_ARCHIVE}"
        "${ZLIB_PKGCONFIG}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required zlib build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${ZLIB_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0 OR archive_members STREQUAL "")
    message(FATAL_ERROR "Invalid zlib archive ${ZLIB_ARCHIVE}: ${ar_error}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${ZLIB_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0
        OR NOT archive_symbols MATCHES "compress2"
        OR NOT archive_symbols MATCHES "uncompress")
    message(FATAL_ERROR
        "zlib archive does not define compression APIs: ${nm_error}")
endif()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(command_count EQUAL 0)
    message(FATAL_ERROR "zlib compile command database is empty")
endif()

set(required_sources
    /deflate.c
    /inflate.c
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

file(READ "${ZLIB_PKGCONFIG}" pkgconfig)
foreach(required_field IN ITEMS
        "prefix=/usr"
        "Name: zlib"
        "Version: 1.3.2"
        [=[Libs: -L${libdir} -L${sharedlibdir} -lz]=]
        [=[Cflags: -I${includedir}]=])
    string(FIND "${pkgconfig}" "${required_field}" field_offset)
    if(field_offset EQUAL -1)
        message(FATAL_ERROR
            "zlib pkg-config metadata is missing ${required_field}: "
            "${ZLIB_PKGCONFIG}")
    endif()
endforeach()
foreach(forbidden_text IN ITEMS
        "${CMAKE_BINARY_DIR}"
        "${CMAKE_SOURCE_DIR}"
        ".so")
    string(FIND "${pkgconfig}" "${forbidden_text}" forbidden_offset)
    if(NOT forbidden_offset EQUAL -1)
        message(FATAL_ERROR
            "zlib pkg-config metadata contains forbidden text "
            "${forbidden_text}: ${ZLIB_PKGCONFIG}")
    endif()
endforeach()

get_filename_component(library_dir "${ZLIB_ARCHIVE}" DIRECTORY)
file(GLOB forbidden_shared_libraries "${library_dir}/libz.so*")
if(forbidden_shared_libraries)
    message(FATAL_ERROR
        "zlib installed shared libraries: ${forbidden_shared_libraries}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${ZLIB_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${ZLIB_ARCHIVE}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "zlibVersion")
    message(FATAL_ERROR
        "bootstrap does not contain the linked zlib implementation: "
        "${bootstrap_nm_error}")
endif()
string(FIND "${link_map}" "libz.so" shared_library_offset)
if(NOT shared_library_offset EQUAL -1)
    message(FATAL_ERROR "bootstrap link map contains a shared zlib")
endif()
