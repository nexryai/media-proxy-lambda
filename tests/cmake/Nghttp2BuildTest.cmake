foreach(required_variable
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        CONFIG_HEADER
        FORTIFY_INCLUDE_DIR
        LINK_MAP
        NGHTTP2_ARCHIVE
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
        "${LINK_MAP}"
        "${NGHTTP2_ARCHIVE}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required nghttp2 build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${NGHTTP2_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0 OR archive_members STREQUAL "")
    message(FATAL_ERROR
        "Invalid nghttp2 archive ${NGHTTP2_ARCHIVE}: ${ar_error}")
endif()

file(READ "${CONFIG_HEADER}" config_header)
foreach(forbidden_definition IN ITEMS
        HAVE_LIBNGHTTP3
        HAVE_LIBNGTCP2
        HAVE_LIBXML2
        HAVE_OPENSSL
        HAVE_WOLFSSL
        ssize_t)
    if(config_header MATCHES
            "(^|\n)#define[ \t]+${forbidden_definition}([ \t]|$)")
        message(FATAL_ERROR
            "nghttp2 enabled forbidden configuration definition: "
            "${forbidden_definition}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${NGHTTP2_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0
        OR NOT archive_symbols MATCHES "nghttp2_session_client_new"
        OR NOT archive_symbols MATCHES "nghttp2_submit_settings")
    message(FATAL_ERROR
        "nghttp2 archive does not define required client APIs: ${nm_error}")
endif()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(command_count EQUAL 0)
    message(FATAL_ERROR "nghttp2 compile command database is empty")
endif()

set(required_sources
    /nghttp2_hd.c
    /nghttp2_session.c
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
        -DBUILDING_NGHTTP2
        -DNGHTTP2_STATICLIB
        -D_FORTIFY_SOURCE=3
        -fPIC
        -fstack-protector-strong
        -ftrivial-auto-var-init=zero
        -fvisibility=hidden
        -flto=thin
        -fsanitize=cfi
        -fsanitize-trap=cfi
        -fno-sanitize-recover=cfi
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

get_filename_component(library_dir "${NGHTTP2_ARCHIVE}" DIRECTORY)
file(GLOB forbidden_shared_libraries "${library_dir}/libnghttp2.so*")
if(forbidden_shared_libraries)
    message(FATAL_ERROR
        "nghttp2 installed shared libraries: ${forbidden_shared_libraries}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${NGHTTP2_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${NGHTTP2_ARCHIVE}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "nghttp2_version")
    message(FATAL_ERROR
        "bootstrap does not contain the linked nghttp2 implementation: "
        "${bootstrap_nm_error}")
endif()

foreach(forbidden_dependency IN ITEMS
        libnghttp2.so
        libnghttp3
        libngtcp2
        libssl.so
        libcrypto.so)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden nghttp2 dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()
