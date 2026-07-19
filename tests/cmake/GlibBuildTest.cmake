foreach(required_variable IN ITEMS
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        FORTIFY_INCLUDE_DIR
        GIO_ARCHIVE
        GIO_PKGCONFIG
        GLIB_ARCHIVE
        GLIB_CONFIG_HEADER
        GLIB_PKGCONFIG
        GMODULE_ARCHIVE
        GMODULE_CONFIG_HEADER
        GMODULE_PKGCONFIG
        GOBJECT_ARCHIVE
        GOBJECT_PKGCONFIG
        GTHREAD_ARCHIVE
        GTHREAD_PKGCONFIG
        LINK_MAP
        NM
        TARGET_ARCH
        TARGET_TRIPLE)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

set(glib_archives
    "${GLIB_ARCHIVE}"
    "${GOBJECT_ARCHIVE}"
    "${GTHREAD_ARCHIVE}"
    "${GMODULE_ARCHIVE}"
    "${GIO_ARCHIVE}")
if(NOT TARGET_ARCH STREQUAL "x86_64" AND NOT TARGET_ARCH STREQUAL "arm64")
    message(FATAL_ERROR "Unsupported TARGET_ARCH: ${TARGET_ARCH}")
endif()
set(expected_member_counts 0 21 1 2 244)
set(archive_index 0)
foreach(archive IN LISTS glib_archives)
    if(NOT EXISTS "${archive}")
        message(FATAL_ERROR "Required GLib archive is absent: ${archive}")
    endif()
    execute_process(
        COMMAND "${AR}" t "${archive}"
        RESULT_VARIABLE ar_result
        OUTPUT_VARIABLE archive_members
        ERROR_VARIABLE ar_error)
    if(NOT ar_result EQUAL 0)
        message(FATAL_ERROR "Cannot inspect ${archive}: ${ar_error}")
    endif()
    string(REGEX MATCHALL "[^\n]+" archive_member_list "${archive_members}")
    list(LENGTH archive_member_list archive_member_count)
    if(archive_index EQUAL 0)
        # A native runner can execute the musl printf probes. A cross-only
        # builder cannot, so GLib adds eleven gnulib compatibility objects.
        if(archive_member_count EQUAL 97)
            set(expected_hardened_command_count 351)
        elseif(archive_member_count EQUAL 108)
            set(expected_hardened_command_count 362)
        else()
            message(FATAL_ERROR
                "${archive} contains unexpected ${archive_member_count} objects")
        endif()
    else()
        list(GET expected_member_counts ${archive_index} expected_member_count)
        if(NOT archive_member_count EQUAL expected_member_count)
            message(FATAL_ERROR
                "${archive} contains ${archive_member_count} objects instead of "
                "${expected_member_count}")
        endif()
    endif()
    math(EXPR archive_index "${archive_index} + 1")
endforeach()

foreach(symbol_check IN ITEMS
        "${GLIB_ARCHIVE}|glib_major_version|g_regex_new|g_utf8_normalize"
        "${GOBJECT_ARCHIVE}|g_object_new|g_object_unref"
        "${GTHREAD_ARCHIVE}|g_thread_init"
        "${GMODULE_ARCHIVE}|g_module_open|g_module_supported"
        "${GIO_ARCHIVE}|g_input_stream_read|g_memory_input_stream_new_from_data")
    string(REPLACE "|" ";" symbol_check_fields "${symbol_check}")
    list(POP_FRONT symbol_check_fields archive)
    execute_process(
        COMMAND "${NM}" --defined-only --format=posix "${archive}"
        RESULT_VARIABLE nm_result
        OUTPUT_VARIABLE archive_symbols
        ERROR_VARIABLE nm_error)
    if(NOT nm_result EQUAL 0)
        message(FATAL_ERROR "Cannot inspect ${archive}: ${nm_error}")
    endif()
    foreach(required_symbol IN LISTS symbol_check_fields)
        if(NOT archive_symbols MATCHES "(^|\n)${required_symbol} ")
            message(FATAL_ERROR "${archive} does not define ${required_symbol}")
        endif()
    endforeach()
endforeach()

execute_process(
    COMMAND "${NM}" --undefined-only --format=posix "${GMODULE_ARCHIVE}"
    RESULT_VARIABLE gmodule_nm_result
    OUTPUT_VARIABLE gmodule_undefined
    ERROR_VARIABLE gmodule_nm_error)
if(NOT gmodule_nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect GModule: ${gmodule_nm_error}")
endif()
foreach(forbidden_symbol IN ITEMS dlclose dlerror dlopen dlsym)
    if(gmodule_undefined MATCHES "(^|\n)${forbidden_symbol} ")
        message(FATAL_ERROR
            "Static GModule retains dynamic-loader hook ${forbidden_symbol}")
    endif()
endforeach()

foreach(required_file IN ITEMS
        "${COMPILE_COMMANDS}"
        "${GLIB_CONFIG_HEADER}"
        "${GMODULE_CONFIG_HEADER}"
        "${GLIB_PKGCONFIG}"
        "${GOBJECT_PKGCONFIG}"
        "${GTHREAD_PKGCONFIG}"
        "${GMODULE_PKGCONFIG}"
        "${GIO_PKGCONFIG}"
        "${BOOTSTRAP}"
        "${LINK_MAP}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required GLib build artifact is absent: ${required_file}")
    endif()
endforeach()

file(READ "${GMODULE_CONFIG_HEADER}" gmodule_config)
if(NOT gmodule_config MATCHES "#define[ \t]+G_MODULE_IMPL[ \t]+0")
    message(FATAL_ERROR "GModule is not configured as the unsupported stub")
endif()
# Meson emits disabled feature macros inside literal `#if (0)` blocks, so their
# textual presence does not mean that the generated configuration enables them.
# G_MODULE_IMPL_NONE above and the archive's undefined-symbol checks are the
# authoritative build-time and link-time guards against a loader implementation.

file(READ "${GLIB_CONFIG_HEADER}" glib_config)
foreach(required_config IN ITEMS
        "#define GLIB_STATIC_COMPILATION 1"
        "#define G_THREADS_ENABLED"
        "#define G_ATOMIC_LOCK_FREE")
    string(FIND "${glib_config}" "${required_config}" config_offset)
    if(config_offset EQUAL -1)
        message(FATAL_ERROR "glibconfig.h is missing ${required_config}")
    endif()
endforeach()

foreach(pkgconfig_check IN ITEMS
        "${GLIB_PKGCONFIG}|Name: GLib|Version: 2.88.2|Requires: libpcre2-8 >= 10.32|Libs: -L\${libdir} -lglib-2.0 -lm -pthread"
        "${GOBJECT_PKGCONFIG}|Name: GObject|Version: 2.88.2|Requires: glib-2.0, libffi >=  3.0.0"
        "${GTHREAD_PKGCONFIG}|Name: GThread|Version: 2.88.2|Requires: glib-2.0"
        "${GMODULE_PKGCONFIG}|Name: GModule|Version: 2.88.2|gmodule_supported=false"
        "${GIO_PKGCONFIG}|Name: GIO|Version: 2.88.2|Requires: glib-2.0, gobject-2.0, gmodule-no-export-2.0, zlib|giomoduledir=\${prefix}/lib/mediaproxy-gio-modules-disabled")
    string(REPLACE "|" ";" pkgconfig_fields "${pkgconfig_check}")
    list(POP_FRONT pkgconfig_fields pkgconfig_file)
    file(READ "${pkgconfig_file}" pkgconfig)
    foreach(required_line IN LISTS pkgconfig_fields)
        string(FIND "${pkgconfig}" "${required_line}" pc_offset)
        if(pc_offset EQUAL -1)
            message(FATAL_ERROR
                "${pkgconfig_file} is missing ${required_line}: ${pkgconfig}")
        endif()
    endforeach()
endforeach()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
math(EXPR last_command "${command_count} - 1")
set(glib_library_command_count 0)
foreach(index RANGE 0 ${last_command})
    string(JSON output GET "${compile_commands_json}" ${index} output)
    if(NOT output MATCHES
            "^(glib/(libglib-2\\.0|libcharset/libcharset|gnulib/libgnulib)|gobject/libgobject-2\\.0|gthread/libgthread-2\\.0|gmodule/libgmodule-2\\.0|gio/libgio-2\\.0)\\.a\\.p/")
        continue()
    endif()
    math(EXPR glib_library_command_count "${glib_library_command_count} + 1")
    string(JSON compile_command GET "${compile_commands_json}" ${index} command)
    foreach(required_flag IN ITEMS
            "--target=${TARGET_TRIPLE}"
            --sysroot=
            -D_FORTIFY_SOURCE=3
            -fPIC
            -fstack-protector-strong
            -ftrivial-auto-var-init=zero
            -fvisibility=hidden
            -flto=thin
            -fsplit-lto-unit
            -std=gnu99
            -Wall
            -Wextra
            -Werror
            "-isystem ${FORTIFY_INCLUDE_DIR}")
        string(FIND "${compile_command}" "${required_flag}" flag_offset)
        if(flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${output} is missing ${required_flag}: ${compile_command}")
        endif()
    endforeach()
    if(TARGET_ARCH STREQUAL "x86_64")
        set(required_arch_flags -fstack-clash-protection -fcf-protection=full)
    elseif(TARGET_ARCH STREQUAL "arm64")
        set(required_arch_flags -mbranch-protection=standard)
    else()
        message(FATAL_ERROR "Unsupported TARGET_ARCH: ${TARGET_ARCH}")
    endif()
    foreach(required_flag IN LISTS required_arch_flags)
        string(FIND "${compile_command}" "${required_flag}" flag_offset)
        if(flag_offset EQUAL -1)
            message(FATAL_ERROR "${output} is missing ${required_flag}")
        endif()
    endforeach()
    foreach(forbidden_flag IN ITEMS
            -fno-lto
            -fno-stack-protector
            -ffast-math
            -fsanitize=cfi
            -fsanitize-cfi-icall-generalize-pointers
            -fno-sanitize-recover=cfi
            -fsanitize-trap=cfi)
        string(FIND "${compile_command}" "${forbidden_flag}" flag_offset)
        if(NOT flag_offset EQUAL -1)
            message(FATAL_ERROR "${output} contains forbidden ${forbidden_flag}")
        endif()
    endforeach()
endforeach()
if(NOT glib_library_command_count EQUAL expected_hardened_command_count)
    message(FATAL_ERROR
        "GLib configured ${glib_library_command_count} hardened library compile "
        "commands instead of ${expected_hardened_command_count}")
endif()

get_filename_component(library_dir "${GLIB_ARCHIVE}" DIRECTORY)
get_filename_component(build_dir "${COMPILE_COMMANDS}" DIRECTORY)
file(GLOB forbidden_libraries
    "${library_dir}/libglib-2.0.so*"
    "${library_dir}/libgobject-2.0.so*"
    "${library_dir}/libgthread-2.0.so*"
    "${library_dir}/libgmodule-2.0.so*"
    "${library_dir}/libgio-2.0.so*"
    "${library_dir}/libgirepository*"
    "${build_dir}/girepository/libgirepository*")
if(forbidden_libraries)
    message(FATAL_ERROR "GLib built forbidden libraries: ${forbidden_libraries}")
endif()

file(READ "${LINK_MAP}" link_map)
# libgthread only exports obsolete initialization shims. Modern GLib initializes
# threading internally, so a demand-loaded static link correctly extracts no
# object from that archive. Its archive and symbol are validated above.
set(glib_bootstrap_archives
    "${GLIB_ARCHIVE}"
    "${GOBJECT_ARCHIVE}"
    "${GMODULE_ARCHIVE}"
    "${GIO_ARCHIVE}")
foreach(archive IN LISTS glib_bootstrap_archives)
    get_filename_component(archive_name "${archive}" NAME)
    string(REPLACE "." "\\." archive_regex "${archive_name}")
    if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
        message(FATAL_ERROR "bootstrap does not contain ${archive}")
    endif()
endforeach()
foreach(forbidden_dependency IN ITEMS
        libglib-2.0.so
        libgobject-2.0.so
        libgmodule-2.0.so
        libgio-2.0.so
        libgirepository
        libdl.so)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden GLib dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error)
if(NOT bootstrap_nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect bootstrap: ${bootstrap_nm_error}")
endif()
foreach(required_symbol IN ITEMS
        g_input_stream_read
        g_memory_input_stream_new_from_data
        g_module_supported
        g_object_unref)
    if(NOT bootstrap_symbols MATCHES "(^|\n)${required_symbol} ")
        message(FATAL_ERROR "bootstrap does not contain ${required_symbol}")
    endif()
endforeach()
