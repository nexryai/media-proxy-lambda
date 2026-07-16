foreach(required_variable
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        CONFIG_HEADER
        FORTIFY_INCLUDE_DIR
        LIBEXIF_ARCHIVE
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
        "${LIBEXIF_ARCHIVE}"
        "${LINK_MAP}"
        "${PKGCONFIG}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required libexif build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${LIBEXIF_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0 OR archive_members STREQUAL "")
    message(FATAL_ERROR "Cannot inspect ${LIBEXIF_ARCHIVE}: ${ar_error}")
endif()
foreach(required_member IN ITEMS
        exif-data.c.o
        exif-entry.c.o
        exif-loader.c.o
        exif-mnote-data-fuji.c.o
        exif-mnote-data-olympus.c.o
        exif-tag.c.o)
    string(FIND "\n${archive_members}" "\n${required_member}\n" member_offset)
    if(member_offset EQUAL -1)
        message(FATAL_ERROR
            "libexif archive is missing ${required_member}: "
            "${archive_members}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${LIBEXIF_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect ${LIBEXIF_ARCHIVE}: ${nm_error}")
endif()
foreach(required_symbol IN ITEMS
        exif_content_get_entry
        exif_data_new_from_data
        exif_data_save_data
        exif_data_unref
        exif_entry_initialize
        exif_get_short
        exif_loader_write
        exif_set_short
        exif_tag_get_name)
    if(NOT archive_symbols MATCHES "${required_symbol}")
        message(FATAL_ERROR "libexif does not define ${required_symbol}")
    endif()
endforeach()

file(READ "${CONFIG_HEADER}" config_header)
foreach(required_config IN ITEMS
        "#define GETTEXT_PACKAGE \"libexif-12\""
        "#define HAVE_LOCALTIME_R 1"
        "#define PACKAGE_VERSION \"0.6.26\"")
    string(FIND "${config_header}" "${required_config}" config_offset)
    if(config_offset EQUAL -1)
        message(FATAL_ERROR
            "libexif configuration is missing ${required_config}")
    endif()
endforeach()
foreach(forbidden_config IN ITEMS
        "#define ENABLE_NLS"
        "#define HAVE_GETTEXT"
        "#define HAVE_ICONV")
    string(FIND "${config_header}" "${forbidden_config}" config_offset)
    if(NOT config_offset EQUAL -1)
        message(FATAL_ERROR
            "libexif configuration enables ${forbidden_config}")
    endif()
endforeach()

file(READ "${PKGCONFIG}" pkgconfig)
foreach(required_pc_line IN ITEMS
        "Version: 0.6.26"
        "Libs: -L\${libdir} -lexif"
        "Libs.private: -lm"
        "Cflags: -I\${includedir}")
    string(FIND "${pkgconfig}" "${required_pc_line}" pc_offset)
    if(pc_offset EQUAL -1)
        message(FATAL_ERROR
            "libexif.pc is missing ${required_pc_line}: ${pkgconfig}")
    endif()
endforeach()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(NOT command_count EQUAL 25)
    message(FATAL_ERROR
        "libexif must compile exactly 25 library C sources, found "
        "${command_count}")
endif()

set(required_sources
    /libexif/exif-data.c
    /libexif/exif-entry.c
    /libexif/exif-loader.c
    /libexif/fuji/exif-mnote-data-fuji.c
    /libexif/olympus/exif-mnote-data-olympus.c
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
        -fPIC
        -fstack-protector-strong
        -ftrivial-auto-var-init=zero
        -fvisibility=hidden
        -flto=thin
        -fsanitize=cfi
        -fno-sanitize=cfi-icall
        -fsanitize-trap=cfi
        -fno-sanitize-recover=cfi
        -ffp-contract=off
        -std=c99
        -Wall
        -Wextra
        -Werror
        -Wpedantic
        -Wno-switch
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
            -DENABLE_NLS
            -fno-lto
            -fno-sanitize-trap=cfi
            -fno-stack-protector
            -ffast-math)
        string(FIND "${matching_command}" "${forbidden_flag}" flag_offset)
        if(NOT flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${required_source} contains forbidden ${forbidden_flag}")
        endif()
    endforeach()
    if(matching_command MATCHES
            "(^|[ ])-fno-sanitize=cfi($|[ ])")
        message(FATAL_ERROR
            "${required_source} contains forbidden -fno-sanitize=cfi")
    endif()
endforeach()

math(EXPR last_command "${command_count} - 1")
foreach(index RANGE 0 ${last_command})
    string(JSON source_file GET "${compile_commands_json}" ${index} file)
    if(NOT source_file MATCHES
            "/libexif/(canon/|fuji/|olympus/|pentax/)?[^/]+\\.c$")
        message(FATAL_ERROR
            "libexif compiled non-library source: ${source_file}")
    endif()
endforeach()

get_filename_component(library_dir "${LIBEXIF_ARCHIVE}" DIRECTORY)
get_filename_component(build_dir "${COMPILE_COMMANDS}" DIRECTORY)
file(GLOB forbidden_shared_libraries "${library_dir}/libexif.so*")
if(forbidden_shared_libraries)
    message(FATAL_ERROR
        "libexif installed shared libraries: ${forbidden_shared_libraries}")
endif()
file(GLOB_RECURSE forbidden_outputs
    "${build_dir}/test/*"
    "${build_dir}/contrib/*"
    "${build_dir}/libexif*.so*"
)
if(forbidden_outputs)
    message(FATAL_ERROR "libexif built forbidden outputs: ${forbidden_outputs}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${LIBEXIF_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${LIBEXIF_ARCHIVE}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "exif_tag_get_name")
    message(FATAL_ERROR
        "bootstrap does not contain libexif: ${bootstrap_nm_error}")
endif()

foreach(forbidden_dependency IN ITEMS
        libexif.so
        libiconv
        libintl
        libstdc++)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden EXIF dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()
