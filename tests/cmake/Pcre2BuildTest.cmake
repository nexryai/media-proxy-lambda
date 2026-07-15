foreach(required_variable
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        CONFIG_HEADER
        FORTIFY_INCLUDE_DIR
        LINK_MAP
        NM
        PCRE2_ARCHIVE
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
        "${LINK_MAP}"
        "${PCRE2_ARCHIVE}"
        "${PKGCONFIG}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required PCRE2 build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${PCRE2_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0 OR archive_members STREQUAL "")
    message(FATAL_ERROR "Cannot inspect ${PCRE2_ARCHIVE}: ${ar_error}")
endif()
string(REPLACE "\n" ";" archive_member_list "${archive_members}")
list(FILTER archive_member_list EXCLUDE REGEX "^$")
list(LENGTH archive_member_list archive_member_count)
if(NOT archive_member_count EQUAL 31)
    message(FATAL_ERROR
        "PCRE2 archive must contain exactly 31 objects, found "
        "${archive_member_count}: ${archive_members}")
endif()
foreach(required_member IN ITEMS
        pcre2_auto_possess.c.o
        pcre2_chartables.c.o
        pcre2_chkdint.c.o
        pcre2_compile.c.o
        pcre2_compile_cgroup.c.o
        pcre2_compile_class.c.o
        pcre2_config.c.o
        pcre2_context.c.o
        pcre2_convert.c.o
        pcre2_dfa_match.c.o
        pcre2_error.c.o
        pcre2_extuni.c.o
        pcre2_find_bracket.c.o
        pcre2_jit_compile.c.o
        pcre2_maketables.c.o
        pcre2_match.c.o
        pcre2_match_data.c.o
        pcre2_match_next.c.o
        pcre2_newline.c.o
        pcre2_ord2utf.c.o
        pcre2_pattern_info.c.o
        pcre2_script_run.c.o
        pcre2_serialize.c.o
        pcre2_string_utils.c.o
        pcre2_study.c.o
        pcre2_substitute.c.o
        pcre2_substring.c.o
        pcre2_tables.c.o
        pcre2_ucd.c.o
        pcre2_valid_utf.c.o
        pcre2_xclass.c.o)
    if(NOT required_member IN_LIST archive_member_list)
        message(FATAL_ERROR
            "PCRE2 archive is missing ${required_member}: ${archive_members}")
    endif()
endforeach()
foreach(forbidden_member IN ITEMS
        pcre2posix.c.o
        pcre2grep.c.o
        pcre2test.c.o
        sljitLir.c.o)
    if(forbidden_member IN_LIST archive_member_list)
        message(FATAL_ERROR
            "PCRE2 archive contains forbidden ${forbidden_member}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${PCRE2_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect ${PCRE2_ARCHIVE}: ${nm_error}")
endif()
foreach(required_symbol IN ITEMS
        pcre2_callout_enumerate_8
        pcre2_code_free_8
        pcre2_compile_8
        pcre2_config_8
        pcre2_match_8
        pcre2_match_data_create_from_pattern_8
        pcre2_next_match_8
        pcre2_set_depth_limit_8
        pcre2_set_heap_limit_8
        pcre2_set_match_limit_8)
    if(NOT archive_symbols MATCHES "${required_symbol}")
        message(FATAL_ERROR "PCRE2 does not define ${required_symbol}")
    endif()
endforeach()

file(READ "${CONFIG_HEADER}" config_header)
foreach(required_config IN ITEMS
        "#define SUPPORT_UNICODE 1"
        "#define LINK_SIZE               2"
        "#define HEAP_LIMIT              20000000"
        "#define MATCH_LIMIT             10000000"
        "#define MATCH_LIMIT_DEPTH       MATCH_LIMIT"
        "#define MAX_VARLOOKBEHIND       255"
        "#define PARENS_NEST_LIMIT       250")
    string(FIND "${config_header}" "${required_config}" config_offset)
    if(config_offset EQUAL -1)
        message(FATAL_ERROR
            "PCRE2 configuration is missing ${required_config}")
    endif()
endforeach()
foreach(forbidden_config IN ITEMS
        "#define SUPPORT_JIT"
        "#define SUPPORT_VALGRIND"
        "#define EBCDIC"
        "#define EBCDIC_NL25"
        "#define EBCDIC_IGNORING_COMPILER")
    string(FIND "${config_header}" "${forbidden_config}" config_offset)
    if(NOT config_offset EQUAL -1)
        message(FATAL_ERROR
            "PCRE2 configuration enables ${forbidden_config}")
    endif()
endforeach()

file(READ "${PKGCONFIG}" pkgconfig)
foreach(required_pc_line IN ITEMS
        "Name: libpcre2-8"
        "Version: 10.47"
        "License: BSD-3-Clause WITH PCRE2-exception"
        "Libs: -L\${libdir} -lpcre2-8"
        "Cflags: -I\${includedir} -DPCRE2_STATIC")
    string(FIND "${pkgconfig}" "${required_pc_line}" pc_offset)
    if(pc_offset EQUAL -1)
        message(FATAL_ERROR
            "libpcre2-8.pc is missing ${required_pc_line}: ${pkgconfig}")
    endif()
endforeach()
string(REGEX MATCH "Libs.private:[^\n]*-l" private_library "${pkgconfig}")
if(private_library)
    message(FATAL_ERROR
        "libpcre2-8.pc unexpectedly links a private library: ${pkgconfig}")
endif()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(NOT command_count EQUAL 32)
    message(FATAL_ERROR
        "PCRE2 must configure exactly 32 C compile commands, found "
        "${command_count}")
endif()
math(EXPR last_command "${command_count} - 1")
foreach(index RANGE 0 ${last_command})
    string(JSON source_file GET "${compile_commands_json}" ${index} file)
    string(JSON compile_command GET
        "${compile_commands_json}" ${index} command)
    if(NOT source_file MATCHES "/src/pcre2[a-z0-9_]*\\.c$")
        message(FATAL_ERROR
            "PCRE2 configured an unexpected source: ${source_file}")
    endif()

    set(required_flags
        "--target=${TARGET_TRIPLE}"
        -D_FORTIFY_SOURCE=3
        -DPCRE2_CODE_UNIT_WIDTH=8
        -DPCRE2_STATIC
        -fPIC
        -fstack-protector-strong
        -ftrivial-auto-var-init=zero
        -fvisibility=hidden
        -flto=thin
        -fsanitize=cfi
        -fsanitize-trap=cfi
        -fno-sanitize-recover=cfi
        -std=gnu99
        -Wall
        -Wextra
        -Werror
        -Wpedantic
        -Wno-overlength-strings
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
                "${source_file} is missing ${required_flag}: "
                "${compile_command}")
        endif()
    endforeach()
    foreach(forbidden_flag IN ITEMS
            -DPCRE2_CODE_UNIT_WIDTH=16
            -DPCRE2_CODE_UNIT_WIDTH=32
            -fno-lto
            -fno-sanitize=cfi
            -fno-sanitize-trap=cfi
            -fno-stack-protector
            -ffast-math)
        string(FIND "${compile_command}" "${forbidden_flag}" flag_offset)
        if(NOT flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${source_file} contains forbidden ${forbidden_flag}")
        endif()
    endforeach()
endforeach()

get_filename_component(library_dir "${PCRE2_ARCHIVE}" DIRECTORY)
get_filename_component(build_dir "${COMPILE_COMMANDS}" DIRECTORY)
file(GLOB forbidden_libraries
    "${library_dir}/libpcre2-16*"
    "${library_dir}/libpcre2-32*"
    "${library_dir}/libpcre2-posix*"
    "${library_dir}/libpcre2-8.so*"
    "${build_dir}/libpcre2-16*"
    "${build_dir}/libpcre2-32*"
    "${build_dir}/libpcre2-posix.a"
    "${build_dir}/libpcre2-8.so*")
if(forbidden_libraries)
    message(FATAL_ERROR
        "PCRE2 built forbidden libraries: ${forbidden_libraries}")
endif()
foreach(forbidden_program IN ITEMS pcre2grep pcre2test)
    if(EXISTS "${build_dir}/${forbidden_program}")
        message(FATAL_ERROR "PCRE2 built forbidden ${forbidden_program}")
    endif()
endforeach()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${PCRE2_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${PCRE2_ARCHIVE}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "pcre2_config_8")
    message(FATAL_ERROR
        "bootstrap does not contain PCRE2: ${bootstrap_nm_error}")
endif()

foreach(forbidden_dependency IN ITEMS
        libpcre2-8.so
        libpcre2-16
        libpcre2-32
        libpcre2-posix
        libstdc++)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden PCRE2 dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()
