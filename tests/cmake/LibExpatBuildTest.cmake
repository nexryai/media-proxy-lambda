foreach(required_variable
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        CONFIG_HEADER
        FORTIFY_INCLUDE_DIR
        LIBEXPAT_ARCHIVE
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
        "${LIBEXPAT_ARCHIVE}"
        "${LINK_MAP}"
        "${PKGCONFIG}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required libexpat build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${LIBEXPAT_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0 OR archive_members STREQUAL "")
    message(FATAL_ERROR "Cannot inspect ${LIBEXPAT_ARCHIVE}: ${ar_error}")
endif()
foreach(required_member IN ITEMS
        random_getrandom.c.o
        xmlparse.c.o
        xmlrole.c.o
        xmltok.c.o)
    string(FIND "\n${archive_members}" "\n${required_member}\n" member_offset)
    if(member_offset EQUAL -1)
        message(FATAL_ERROR
            "libexpat archive is missing ${required_member}: "
            "${archive_members}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${LIBEXPAT_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect ${LIBEXPAT_ARCHIVE}: ${nm_error}")
endif()
foreach(required_symbol IN ITEMS
        XML_ExpatVersion
        XML_Parse
        XML_ParserCreateNS
        XML_ParserFree
        XML_SetAllocTrackerActivationThreshold
        XML_SetBillionLaughsAttackProtectionMaximumAmplification
        XML_SetHashSalt16Bytes
        XML_SetReparseDeferralEnabled)
    if(NOT archive_symbols MATCHES "${required_symbol}")
        message(FATAL_ERROR "libexpat does not define ${required_symbol}")
    endif()
endforeach()

file(READ "${CONFIG_HEADER}" config_header)
foreach(required_config IN ITEMS
        "#define BYTEORDER 1234"
        "#define HAVE_GETRANDOM"
        "#  define XML_CONTEXT_BYTES 1024"
        "#define XML_DTD"
        "#  define XML_GE 1"
        "#define XML_NS")
    string(FIND "${config_header}" "${required_config}" config_offset)
    if(config_offset EQUAL -1)
        message(FATAL_ERROR
            "libexpat configuration is missing ${required_config}")
    endif()
endforeach()
foreach(forbidden_config IN ITEMS
        "#define HAVE_ARC4RANDOM"
        "#define HAVE_ARC4RANDOM_BUF"
        "#define HAVE_GETENTROPY"
        "#define HAVE_SYSCALL_GETRANDOM"
        "#define WORDS_BIGENDIAN"
        "#define XML_ATTR_INFO"
        "#define XML_DEV_URANDOM")
    string(FIND "${config_header}" "${forbidden_config}" config_offset)
    if(NOT config_offset EQUAL -1)
        message(FATAL_ERROR
            "libexpat configuration enables ${forbidden_config}")
    endif()
endforeach()

file(READ "${PKGCONFIG}" pkgconfig)
foreach(required_pc_line IN ITEMS
        "Version: 2.8.2"
        "Libs: -L\${libdir} -lexpat"
        "Cflags: -I\${includedir}"
        "Cflags.private: -DXML_STATIC")
    string(FIND "${pkgconfig}" "${required_pc_line}" pc_offset)
    if(pc_offset EQUAL -1)
        message(FATAL_ERROR
            "expat.pc is missing ${required_pc_line}: ${pkgconfig}")
    endif()
endforeach()
string(FIND "${pkgconfig}" "Libs.private: -l" private_library_offset)
if(NOT private_library_offset EQUAL -1)
    message(FATAL_ERROR
        "expat.pc unexpectedly links a private library: ${pkgconfig}")
endif()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(NOT command_count EQUAL 4)
    message(FATAL_ERROR
        "libexpat must compile exactly four library C sources, found "
        "${command_count}")
endif()

set(required_sources
    /lib/random_getrandom.c
    /lib/xmlparse.c
    /lib/xmlrole.c
    /lib/xmltok.c
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
        -std=c99
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
        string(FIND "${matching_command}" "${required_flag}" flag_offset)
        if(flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${required_source} is missing ${required_flag}: "
                "${matching_command}")
        endif()
    endforeach()
    string(REPLACE "-fno-sanitize=cfi-icall" ""
        command_without_icall_exception "${matching_command}")
    foreach(forbidden_flag IN ITEMS
            -fno-lto
            -fno-sanitize=cfi
            -fno-sanitize-trap=cfi
            -fno-stack-protector
            -ffast-math)
        string(FIND "${command_without_icall_exception}"
            "${forbidden_flag}" flag_offset)
        if(NOT flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${required_source} contains forbidden ${forbidden_flag}")
        endif()
    endforeach()
endforeach()

math(EXPR last_command "${command_count} - 1")
foreach(index RANGE 0 ${last_command})
    string(JSON source_file GET "${compile_commands_json}" ${index} file)
    if(NOT source_file MATCHES
            "/lib/(random_getrandom|xmlparse|xmlrole|xmltok)\\.c$")
        message(FATAL_ERROR
            "libexpat compiled non-library source: ${source_file}")
    endif()
endforeach()

get_filename_component(library_dir "${LIBEXPAT_ARCHIVE}" DIRECTORY)
get_filename_component(build_dir "${COMPILE_COMMANDS}" DIRECTORY)
file(GLOB forbidden_shared_libraries "${library_dir}/libexpat.so*")
if(forbidden_shared_libraries)
    message(FATAL_ERROR
        "libexpat installed shared libraries: ${forbidden_shared_libraries}")
endif()
file(GLOB_RECURSE forbidden_outputs
    "${build_dir}/examples/*"
    "${build_dir}/fuzz/*"
    "${build_dir}/tests/*"
    "${build_dir}/xmlwf/*"
    "${build_dir}/libexpat.so*"
)
if(forbidden_outputs)
    message(FATAL_ERROR
        "libexpat built forbidden outputs: ${forbidden_outputs}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${LIBEXPAT_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${LIBEXPAT_ARCHIVE}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "XML_ExpatVersion")
    message(FATAL_ERROR
        "bootstrap does not contain libexpat: ${bootstrap_nm_error}")
endif()

foreach(forbidden_dependency IN ITEMS
        libexpat.so
        libstdc++)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden XML dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()
