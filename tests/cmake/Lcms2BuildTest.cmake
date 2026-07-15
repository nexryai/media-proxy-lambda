foreach(required_variable
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        FORTIFY_INCLUDE_DIR
        LCMS2_ARCHIVE
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
        "${LCMS2_ARCHIVE}"
        "${LINK_MAP}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required lcms2 build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${LCMS2_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0 OR archive_members STREQUAL "")
    message(FATAL_ERROR "Cannot inspect ${LCMS2_ARCHIVE}: ${ar_error}")
endif()
foreach(required_member IN ITEMS
        cmsalpha.c.o
        cmscgats.c.o
        cmsio0.c.o
        cmsplugin.c.o
        cmsxform.c.o)
    string(FIND "\n${archive_members}" "\n${required_member}\n" member_offset)
    if(member_offset EQUAL -1)
        message(FATAL_ERROR
            "lcms2 archive is missing ${required_member}: ${archive_members}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${LCMS2_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect ${LCMS2_ARCHIVE}: ${nm_error}")
endif()
foreach(required_symbol IN ITEMS
        cmsCloseProfile
        cmsCreate_sRGBProfile
        cmsCreateTransform
        cmsDeleteTransform
        cmsDoTransform
        cmsGetEncodedCMMversion
        cmsOpenProfileFromMem
        cmsSaveProfileToMem)
    if(NOT archive_symbols MATCHES "${required_symbol}")
        message(FATAL_ERROR "lcms2 does not define ${required_symbol}")
    endif()
endforeach()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(NOT command_count EQUAL 26)
    message(FATAL_ERROR
        "lcms2 must compile exactly 26 core C sources, found ${command_count}")
endif()

set(required_sources
    /src/cmscgats.c
    /src/cmsio0.c
    /src/cmsplugin.c
    /src/cmsxform.c
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
        -D_POSIX_C_SOURCE=200809L
        -DHasTHREADS=1
        -fPIC
        -fstack-protector-strong
        -ftrivial-auto-var-init=zero
        -fvisibility=hidden
        -flto=thin
        -fsanitize=cfi
        -fsanitize-trap=cfi
        -fno-sanitize-recover=cfi
        -ffp-contract=off
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
        set(forbidden_target_flag -DCMS_DONT_USE_SSE2=1)
    elseif(TARGET_ARCH STREQUAL "arm64")
        list(APPEND required_flags
            -DCMS_DONT_USE_SSE2=1
            -mbranch-protection=standard)
        set(forbidden_target_flag -fcf-protection=full)
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
            -ffast-math
            -DWORDS_BIGENDIAN
            "${forbidden_target_flag}")
        string(FIND "${matching_command}" "${forbidden_flag}" flag_offset)
        if(NOT flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${required_source} contains forbidden ${forbidden_flag}")
        endif()
    endforeach()
endforeach()

math(EXPR last_command "${command_count} - 1")
foreach(index RANGE 0 ${last_command})
    string(JSON source_file GET "${compile_commands_json}" ${index} file)
    if(NOT source_file MATCHES "/src/[^/]+\\.c$")
        message(FATAL_ERROR "lcms2 compiled non-core source: ${source_file}")
    endif()
endforeach()

get_filename_component(library_dir "${LCMS2_ARCHIVE}" DIRECTORY)
get_filename_component(build_dir "${COMPILE_COMMANDS}" DIRECTORY)
file(GLOB forbidden_shared_libraries "${library_dir}/liblcms2.so*")
if(forbidden_shared_libraries)
    message(FATAL_ERROR
        "lcms2 installed shared libraries: ${forbidden_shared_libraries}")
endif()
file(GLOB forbidden_outputs
    "${build_dir}/jpgicc*"
    "${build_dir}/linkicc*"
    "${build_dir}/psicc*"
    "${build_dir}/testbed*"
    "${build_dir}/tificc*"
    "${build_dir}/tiffdiff*"
    "${build_dir}/transicc*"
    "${build_dir}/liblcms2_fast_float*"
    "${build_dir}/liblcms2_threaded*"
)
if(forbidden_outputs)
    message(FATAL_ERROR "lcms2 built forbidden outputs: ${forbidden_outputs}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${LCMS2_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${LCMS2_ARCHIVE}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "cmsGetEncodedCMMversion")
    message(FATAL_ERROR
        "bootstrap does not contain lcms2: ${bootstrap_nm_error}")
endif()

foreach(forbidden_dependency IN ITEMS
        liblcms2.so
        lcms2_fast_float
        lcms2_threaded
        libstdc++)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden color dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()
