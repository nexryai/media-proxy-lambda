foreach(required_variable
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        CONFIG_HEADER
        CURL_ARCHIVE
        FORTIFY_INCLUDE_DIR
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
        "${CURL_ARCHIVE}"
        "${LINK_MAP}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required curl build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${CURL_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0 OR archive_members STREQUAL "")
    message(FATAL_ERROR "Invalid curl archive ${CURL_ARCHIVE}: ${ar_error}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${CURL_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0
        OR NOT archive_symbols MATCHES "curl_easy_init"
        OR NOT archive_symbols MATCHES "curl_version_info")
    message(FATAL_ERROR
        "curl archive does not define required client APIs: ${nm_error}")
endif()

file(READ "${CONFIG_HEADER}" config_header)
foreach(required_definition IN ITEMS
        CURL_DISABLE_ALTSVC
        CURL_DISABLE_COOKIES
        CURL_DISABLE_DOH
        CURL_DISABLE_HSTS
        CURL_DISABLE_HTTP_AUTH
        CURL_DISABLE_MIME
        CURL_DISABLE_NETRC
        CURL_DISABLE_OPENSSL_AUTO_LOAD_CONFIG
        CURL_DISABLE_PROXY
        CURL_DISABLE_WEBSOCKETS
        HAVE_LIBZ
        USE_IPV6
        USE_NGHTTP2
        USE_OPENSSL)
    if(NOT config_header MATCHES
            "(^|\n)#define[ \t]+${required_definition}([ \t]|$)")
        message(FATAL_ERROR
            "curl is missing required configuration definition: "
            "${required_definition}")
    endif()
endforeach()
foreach(forbidden_definition IN ITEMS
        HAVE_BROTLI
        HAVE_LIBIDN2
        HAVE_ZSTD
        USE_ARES
        USE_GNUTLS
        USE_LIBPSL
        USE_MBEDTLS
        USE_NGTCP2
        USE_NTLM
        USE_QUICHE
        USE_RESOLV_THREADED
        USE_RUSTLS
        USE_THREADS_POSIX
        USE_WOLFSSL)
    if(config_header MATCHES
            "(^|\n)#define[ \t]+${forbidden_definition}([ \t]|$)")
        message(FATAL_ERROR
            "curl enabled forbidden configuration definition: "
            "${forbidden_definition}")
    endif()
endforeach()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(command_count EQUAL 0)
    message(FATAL_ERROR "curl compile command database is empty")
endif()

set(required_sources
    /lib/easy.c
    /lib/vtls/openssl.c
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
        -DBUILDING_LIBCURL
        -DCURL_HIDDEN_SYMBOLS
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

get_filename_component(library_dir "${CURL_ARCHIVE}" DIRECTORY)
file(GLOB forbidden_shared_libraries "${library_dir}/libcurl.so*")
if(forbidden_shared_libraries)
    message(FATAL_ERROR
        "curl installed shared libraries: ${forbidden_shared_libraries}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${CURL_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${CURL_ARCHIVE}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "curl_version_info")
    message(FATAL_ERROR
        "bootstrap does not contain the linked curl implementation: "
        "${bootstrap_nm_error}")
endif()

foreach(forbidden_dependency IN ITEMS
        libcurl.so
        libbrotli
        libgnutls
        libidn2
        libmbedtls
        libpsl
        libssh
        libwolfssl
        libzstd)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden curl dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()
