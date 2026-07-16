foreach(required_variable IN ITEMS
        ADA_IDNA_ARCHIVE
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        FORTIFY_INCLUDE_DIR
        LICENSE_FILE
        LINK_MAP
        NM
        TARGET_ARCH
        TARGET_TRIPLE)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

foreach(required_file IN ITEMS
        "${ADA_IDNA_ARCHIVE}"
        "${BOOTSTRAP}"
        "${COMPILE_COMMANDS}"
        "${LICENSE_FILE}"
        "${LINK_MAP}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required Ada IDNA artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${ADA_IDNA_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect Ada IDNA archive: ${ar_error}")
endif()
string(REGEX MATCHALL "[^\n]+" archive_member_list "${archive_members}")
list(LENGTH archive_member_list archive_member_count)
if(NOT archive_member_count EQUAL 1
        OR NOT archive_members MATCHES "ada_idna\\.cpp\\.o")
    message(FATAL_ERROR
        "Ada IDNA archive must contain only ada_idna.cpp.o: "
        "${archive_members}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --demangle --format=posix
        "${ADA_IDNA_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect Ada IDNA symbols: ${nm_error}")
endif()
foreach(required_symbol IN ITEMS
        "ada::idna::contains_forbidden_domain_code_point"
        "ada::idna::is_label_valid"
        "ada::idna::to_ascii")
    string(FIND "${archive_symbols}" "${required_symbol}" symbol_offset)
    if(symbol_offset EQUAL -1)
        message(FATAL_ERROR "Ada IDNA does not define ${required_symbol}")
    endif()
endforeach()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(NOT command_count EQUAL 1)
    message(FATAL_ERROR
        "Ada IDNA must compile exactly one source, found ${command_count}")
endif()
string(JSON source_file GET "${compile_commands_json}" 0 file)
string(JSON compile_command GET "${compile_commands_json}" 0 command)
if(NOT source_file MATCHES "/src/ada_idna\\.cpp$")
    message(FATAL_ERROR "Unexpected Ada IDNA source: ${source_file}")
endif()

set(required_flags
    "--target=${TARGET_TRIPLE}"
    -D_FORTIFY_SOURCE=3
    -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST
    -fPIC
    -fstack-protector-strong
    -ftrivial-auto-var-init=zero
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -ffunction-sections
    -fdata-sections
    -flto=thin
    -fsanitize=cfi
    -fsanitize-trap=cfi
    -fno-sanitize-recover=cfi
    -std=c++20
    -Wall
    -Wextra
    -Werror
    -Wpedantic
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
    string(FIND "${compile_command}" "${required_flag}" flag_offset)
    if(flag_offset EQUAL -1)
        message(FATAL_ERROR
            "Ada IDNA is missing ${required_flag}: ${compile_command}")
    endif()
endforeach()
foreach(forbidden_flag IN ITEMS
        ADA_USE_SIMDUTF
        -fno-lto
        -fno-sanitize=cfi
        -fno-stack-protector)
    string(FIND "${compile_command}" "${forbidden_flag}" flag_offset)
    if(NOT flag_offset EQUAL -1)
        message(FATAL_ERROR "Ada IDNA contains forbidden ${forbidden_flag}")
    endif()
endforeach()

file(READ "${LICENSE_FILE}" license_text)
if(NOT license_text MATCHES
        "Permission is hereby granted, free of charge")
    message(FATAL_ERROR "Ada IDNA MIT license notice is incomplete")
endif()

get_filename_component(library_dir "${ADA_IDNA_ARCHIVE}" DIRECTORY)
get_filename_component(build_dir "${COMPILE_COMMANDS}" DIRECTORY)
file(GLOB forbidden_shared_libraries
    "${library_dir}/libada_idna.so*"
    "${build_dir}/libada_idna.so*")
if(forbidden_shared_libraries)
    message(FATAL_ERROR
        "Ada IDNA built shared libraries: ${forbidden_shared_libraries}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${ADA_IDNA_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${ADA_IDNA_ARCHIVE}")
endif()
foreach(forbidden_dependency IN ITEMS
        libidn
        libidn2
        libunistring
        libicu
        simdutf)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap contains forbidden IDNA dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()
