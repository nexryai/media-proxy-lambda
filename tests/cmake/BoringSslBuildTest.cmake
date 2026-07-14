foreach(required_variable
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        CRYPTO_ARCHIVE
        FORTIFY_INCLUDE_DIR
        LINK_MAP
        NM
        SSL_ARCHIVE
        TARGET_ARCH
        TARGET_TRIPLE)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

foreach(required_file IN ITEMS
        "${COMPILE_COMMANDS}"
        "${BOOTSTRAP}"
        "${CRYPTO_ARCHIVE}"
        "${LINK_MAP}"
        "${SSL_ARCHIVE}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required BoringSSL build artifact is absent: ${required_file}")
    endif()
endforeach()

foreach(archive IN ITEMS "${CRYPTO_ARCHIVE}" "${SSL_ARCHIVE}")
    execute_process(
        COMMAND "${AR}" t "${archive}"
        RESULT_VARIABLE ar_result
        OUTPUT_VARIABLE archive_members
        ERROR_VARIABLE ar_error
    )
    if(NOT ar_result EQUAL 0 OR archive_members STREQUAL "")
        message(FATAL_ERROR "Invalid BoringSSL archive ${archive}: ${ar_error}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${CRYPTO_ARCHIVE}"
    RESULT_VARIABLE crypto_nm_result
    OUTPUT_VARIABLE crypto_symbols
    ERROR_VARIABLE crypto_nm_error
)
if(NOT crypto_nm_result EQUAL 0 OR NOT crypto_symbols MATCHES "SHA256")
    message(FATAL_ERROR
        "BoringSSL libcrypto does not define SHA256: ${crypto_nm_error}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${SSL_ARCHIVE}"
    RESULT_VARIABLE ssl_nm_result
    OUTPUT_VARIABLE ssl_symbols
    ERROR_VARIABLE ssl_nm_error
)
if(NOT ssl_nm_result EQUAL 0 OR NOT ssl_symbols MATCHES "SSL_CTX_new")
    message(FATAL_ERROR
        "BoringSSL libssl does not define SSL_CTX_new: ${ssl_nm_error}")
endif()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(command_count EQUAL 0)
    message(FATAL_ERROR "BoringSSL compile command database is empty")
endif()

get_filename_component(sysroot_include_dir "${FORTIFY_INCLUDE_DIR}" DIRECTORY)
set(libcxx_include_dir "${sysroot_include_dir}/c++/v1")
set(required_sources
    /crypto/fipsmodule/bcm.cc
    /ssl/ssl_lib.cc
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
        -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST
        -DBORINGSSL_IMPLEMENTATION
        -DOPENSSL_NO_ASM
        -fPIC
        -fstack-protector-strong
        -ftrivial-auto-var-init=zero
        -fvisibility=hidden
        -fvisibility-inlines-hidden
        -flto=thin
        -fsanitize=cfi
        -fsanitize-trap=cfi
        -fno-sanitize-recover=cfi
        -Werror
        "-isystem ${FORTIFY_INCLUDE_DIR}"
        "-isystem ${libcxx_include_dir}"
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

    string(FIND "${matching_command}"
        "-isystem ${libcxx_include_dir}" libcxx_include_offset)
    string(FIND "${matching_command}"
        "-isystem ${FORTIFY_INCLUDE_DIR}" fortify_include_offset)
    if(libcxx_include_offset EQUAL -1
            OR fortify_include_offset EQUAL -1
            OR libcxx_include_offset GREATER_EQUAL fortify_include_offset)
        message(FATAL_ERROR
            "${required_source} must search libc++ before fortify headers: "
            "${matching_command}")
    endif()
endforeach()

get_filename_component(library_dir "${CRYPTO_ARCHIVE}" DIRECTORY)
file(GLOB forbidden_shared_libraries
    "${library_dir}/libcrypto.so*"
    "${library_dir}/libssl.so*"
)
if(forbidden_shared_libraries)
    message(FATAL_ERROR
        "BoringSSL installed shared libraries: ${forbidden_shared_libraries}")
endif()

file(READ "${LINK_MAP}" link_map)
foreach(required_archive IN ITEMS "${CRYPTO_ARCHIVE}" "${SSL_ARCHIVE}")
    get_filename_component(required_archive_name "${required_archive}" NAME)
    get_filename_component(required_archive_dir "${required_archive}" DIRECTORY)
    string(REPLACE "." "\\." required_archive_regex "${required_archive_name}")
    if(NOT link_map MATCHES
            "${required_archive_dir}/[^ ]*${required_archive_regex}")
        message(FATAL_ERROR "bootstrap does not contain ${required_archive}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "SSL_CTX_new")
    message(FATAL_ERROR
        "bootstrap does not expose the linked BoringSSL TLS implementation: "
        "${bootstrap_nm_error}")
endif()
foreach(forbidden_provider IN ITEMS
        libcrypto.so
        libssl.so
        libgnutls
        libmbedtls
        libwolfssl
        libstdc++
        libgcc)
    string(FIND "${link_map}" "${forbidden_provider}" provider_offset)
    if(NOT provider_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden provider/runtime: "
            "${forbidden_provider}")
    endif()
endforeach()
