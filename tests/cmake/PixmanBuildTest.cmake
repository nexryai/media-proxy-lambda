foreach(required_variable IN ITEMS
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        CONFIG_HEADER
        FORTIFY_INCLUDE_DIR
        LICENSE_FILE
        LINK_MAP
        NM
        PIXMAN_ARCHIVE
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
        "${LICENSE_FILE}"
        "${LINK_MAP}"
        "${PIXMAN_ARCHIVE}"
        "${PKGCONFIG}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required pixman build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${PIXMAN_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0 OR archive_members STREQUAL "")
    message(FATAL_ERROR
        "Invalid pixman archive ${PIXMAN_ARCHIVE}: ${ar_error}")
endif()
foreach(forbidden_member IN ITEMS
        pixman-arm-neon
        pixman-arm-simd
        pixman-arma64-neon
        pixman-mips-dspr2
        pixman-mmx
        pixman-rvv
        pixman-sse2
        pixman-ssse3
        pixman-vmx)
    string(FIND "${archive_members}" "${forbidden_member}" member_offset)
    if(NOT member_offset EQUAL -1)
        message(FATAL_ERROR
            "pixman archive contains disabled SIMD object "
            "${forbidden_member}: ${archive_members}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${PIXMAN_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect pixman symbols: ${nm_error}")
endif()
foreach(required_symbol IN ITEMS
        pixman_image_composite32
        pixman_image_create_bits
        pixman_image_unref
        pixman_region32_init
        pixman_version)
    string(FIND "${archive_symbols}" "${required_symbol}" symbol_offset)
    if(symbol_offset EQUAL -1)
        message(FATAL_ERROR "pixman does not define ${required_symbol}")
    endif()
endforeach()

file(READ "${CONFIG_HEADER}" config_header)
foreach(forbidden_definition IN ITEMS
        USE_ARM_A64_NEON
        USE_ARM_NEON
        USE_ARM_SIMD
        USE_LOONGSON_MMI
        USE_MIPS_DSPR2
        USE_RVV
        USE_VMX
        USE_X86_MMX
        USE_SSE2
        USE_SSSE3)
    if(config_header MATCHES
            "(^|\n)#define[ \t]+${forbidden_definition}([ \t\n]|$)")
        message(FATAL_ERROR
            "pixman enabled forbidden SIMD path ${forbidden_definition}")
    endif()
endforeach()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(command_count LESS 30 OR command_count GREATER 40)
    message(FATAL_ERROR
        "Unexpected pixman core source count: ${command_count}")
endif()

foreach(required_source IN ITEMS /pixman.c /pixman-image.c /pixman-region32.c)
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
        -ffunction-sections
        -fdata-sections
        -flto=thin
        -fsplit-lto-unit
        -fsanitize=cfi
        -fsanitize-trap=cfi
        -fno-sanitize-recover=cfi
        -ffp-contract=off
        -Wall
        -Wextra
        -Werror
        -Wno-missing-field-initializers
        -Wno-sign-compare
        -Wno-unused-parameter
        "-isystem ${FORTIFY_INCLUDE_DIR}")
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
            -fno-stack-protector
            -Wno-strict-prototypes)
        string(FIND "${matching_command}" "${forbidden_flag}" flag_offset)
        if(NOT flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${required_source} contains forbidden ${forbidden_flag}: "
                "${matching_command}")
        endif()
    endforeach()
endforeach()

file(READ "${PKGCONFIG}" pkgconfig)
foreach(required_field IN ITEMS
        "prefix=/usr"
        "Name: Pixman"
        "Version: 0.46.4"
        [=[Libs: -L${libdir} -lpixman-1 -lm -pthread]=]
        [=[Cflags: -I${includedir}/pixman-1 -pthread]=])
    string(FIND "${pkgconfig}" "${required_field}" field_offset)
    if(field_offset EQUAL -1)
        message(FATAL_ERROR
            "pixman pkg-config metadata is missing ${required_field}: "
            "${PKGCONFIG}")
    endif()
endforeach()
foreach(forbidden_text IN ITEMS
        "${CMAKE_BINARY_DIR}"
        "${CMAKE_SOURCE_DIR}"
        ".so"
        "Requires:")
    string(FIND "${pkgconfig}" "${forbidden_text}" forbidden_offset)
    if(NOT forbidden_offset EQUAL -1)
        message(FATAL_ERROR
            "pixman pkg-config metadata contains forbidden text "
            "${forbidden_text}: ${PKGCONFIG}")
    endif()
endforeach()

file(READ "${LICENSE_FILE}" license_text)
if(NOT license_text MATCHES
        "Permission is hereby granted, free of charge")
    message(FATAL_ERROR "pixman MIT license notice is incomplete")
endif()

get_filename_component(library_dir "${PIXMAN_ARCHIVE}" DIRECTORY)
get_filename_component(build_dir "${COMPILE_COMMANDS}" DIRECTORY)
file(GLOB forbidden_shared_libraries
    "${library_dir}/libpixman-1.so*"
    "${build_dir}/pixman/libpixman-1.so*")
if(forbidden_shared_libraries)
    message(FATAL_ERROR
        "pixman built shared libraries: ${forbidden_shared_libraries}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${PIXMAN_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${PIXMAN_ARCHIVE}")
endif()
foreach(required_member IN ITEMS
        pixman.c.o
        pixman-image.c.o
        pixman-general.c.o
        pixman-combine32.c.o)
    string(FIND "${link_map}" "${required_member}" member_offset)
    if(member_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap does not contain pixman member ${required_member}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "pixman_version")
    message(FATAL_ERROR
        "bootstrap does not contain pixman: ${bootstrap_nm_error}")
endif()

foreach(forbidden_dependency IN ITEMS
        libpixman-1.so
        libstdc++
        libgcc)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden pixman dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()
