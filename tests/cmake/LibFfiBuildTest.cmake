foreach(required_variable IN ITEMS
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        CONFIG_HEADER
        FORTIFY_INCLUDE_DIR
        LIBFFI_ARCHIVE
        LINK_MAP
        NM
        PKGCONFIG
        READELF
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
        "${LIBFFI_ARCHIVE}"
        "${LINK_MAP}"
        "${PKGCONFIG}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Required libffi build artifact is absent: ${required_file}")
    endif()
endforeach()

set(required_members
    closures.c.o
    prep_cif.c.o
    tramp.c.o
    types.c.o)
set(required_c_sources
    /src/closures.c
    /src/prep_cif.c
    /src/tramp.c
    /src/types.c)
if(TARGET_ARCH STREQUAL "x86_64")
    list(APPEND required_members
        ffi64.c.o
        ffiw64.c.o
        unix64.S.o
        win64.S.o)
    list(APPEND required_c_sources
        /src/x86/ffi64.c
        /src/x86/ffiw64.c)
    set(required_assembly_sources
        /src/x86/unix64.S
        /src/x86/win64.S)
elseif(TARGET_ARCH STREQUAL "arm64")
    list(APPEND required_members ffi.c.o sysv.S.o)
    list(APPEND required_c_sources /src/aarch64/ffi.c)
    set(required_assembly_sources /src/aarch64/sysv.S)
else()
    message(FATAL_ERROR "Unsupported TARGET_ARCH: ${TARGET_ARCH}")
endif()

execute_process(
    COMMAND "${AR}" t "${LIBFFI_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect ${LIBFFI_ARCHIVE}: ${ar_error}")
endif()
string(REGEX MATCHALL "[^\n]+" archive_member_list "${archive_members}")
list(LENGTH archive_member_list archive_member_count)
list(LENGTH required_members required_member_count)
if(NOT archive_member_count EQUAL required_member_count)
    message(FATAL_ERROR
        "libffi archive contains ${archive_member_count} objects instead of "
        "${required_member_count}: ${archive_members}")
endif()
foreach(required_member IN LISTS required_members)
    if(NOT required_member IN_LIST archive_member_list)
        message(FATAL_ERROR
            "libffi archive is missing ${required_member}: ${archive_members}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${LIBFFI_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect ${LIBFFI_ARCHIVE}: ${nm_error}")
endif()
foreach(required_symbol IN ITEMS
        ffi_call
        ffi_closure_alloc
        ffi_closure_free
        ffi_get_struct_offsets
        ffi_prep_cif
        ffi_prep_closure_loc
        ffi_tramp_arch
        ffi_type_sint64
        ffi_type_uint64)
    if(NOT archive_symbols MATCHES "(^|\n)${required_symbol} ")
        message(FATAL_ERROR "libffi does not define ${required_symbol}")
    endif()
endforeach()

file(READ "${CONFIG_HEADER}" config_header)
foreach(required_config IN ITEMS
        "#define FFI_EXEC_STATIC_TRAMP 1"
        "#define FFI_NO_RAW_API 1"
        "#define HAVE_AS_CFI_PSEUDO_OP 1"
        "#define HAVE_INT128 1"
        "#define HAVE_MEMCPY 1"
        "#define SIZEOF_DOUBLE 8"
        "#define SIZEOF_LONG_DOUBLE 16"
        "#define SIZEOF_SIZE_T 8")
    string(FIND "${config_header}" "${required_config}" config_offset)
    if(config_offset EQUAL -1)
        message(FATAL_ERROR
            "libffi configuration is missing ${required_config}")
    endif()
endforeach()
foreach(forbidden_config IN ITEMS
        "#define FFI_DEBUG 1"
        "#define FFI_MMAP_EXEC_EMUTRAMP_PAX 1"
        "#define FFI_MMAP_EXEC_WRIT 1")
    string(FIND "${config_header}" "${forbidden_config}" config_offset)
    if(NOT config_offset EQUAL -1)
        message(FATAL_ERROR
            "libffi configuration enables ${forbidden_config}")
    endif()
endforeach()

file(READ "${PKGCONFIG}" pkgconfig)
foreach(required_pc_line IN ITEMS
        "Name: libffi"
        "Version: 3.7.1"
        "Libs: -L\${toolexeclibdir} -lffi"
        "Cflags: -I\${includedir}")
    string(FIND "${pkgconfig}" "${required_pc_line}" pc_offset)
    if(pc_offset EQUAL -1)
        message(FATAL_ERROR
            "libffi.pc is missing ${required_pc_line}: ${pkgconfig}")
    endif()
endforeach()
if(pkgconfig MATCHES "Libs.private:" OR pkgconfig MATCHES "Requires")
    message(FATAL_ERROR
        "libffi.pc unexpectedly declares another dependency: ${pkgconfig}")
endif()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
list(LENGTH required_members expected_command_count)
if(NOT command_count EQUAL expected_command_count)
    message(FATAL_ERROR
        "libffi must compile exactly ${expected_command_count} curated "
        "sources, found ${command_count}")
endif()

foreach(required_source IN LISTS required_c_sources required_assembly_sources)
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
        "--sysroot="
        -fPIC)
    if(matching_command MATCHES "\\.c( |$)")
        list(APPEND required_flags
            "--target=${TARGET_TRIPLE}"
            -D_FORTIFY_SOURCE=3
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
            "-isystem ${FORTIFY_INCLUDE_DIR}")
    else()
        list(APPEND required_flags "-target ${TARGET_TRIPLE}")
    endif()
    if(TARGET_ARCH STREQUAL "x86_64")
        list(APPEND required_flags
            -fstack-clash-protection
            -fcf-protection=full)
    else()
        list(APPEND required_flags -mbranch-protection=standard)
    endif()
    foreach(required_flag IN LISTS required_flags)
        string(FIND "${matching_command}" "${required_flag}" flag_offset)
        if(flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${required_source} is missing ${required_flag}: "
                "${matching_command}")
        endif()
    endforeach()

    if(required_source STREQUAL "/src/closures.c")
        set(permitted_warning_exception
            -Wno-gnu-null-pointer-arithmetic)
    elseif(required_source STREQUAL "/src/prep_cif.c"
            OR required_source STREQUAL "/src/aarch64/ffi.c")
        set(permitted_warning_exception -Wno-unused-parameter)
    else()
        set(permitted_warning_exception "")
    endif()
    string(REGEX MATCHALL "-Wno-[A-Za-z0-9_-]+"
        warning_exceptions "${matching_command}")
    if(NOT "${warning_exceptions}" STREQUAL
            "${permitted_warning_exception}")
        message(FATAL_ERROR
            "${required_source} has unexpected warning exceptions: "
            "${warning_exceptions}")
    endif()
    foreach(forbidden_flag IN ITEMS
            -fno-lto
            -fno-sanitize=cfi
            -fno-sanitize-trap=cfi
            -fno-stack-protector)
        string(FIND "${matching_command}" "${forbidden_flag}" flag_offset)
        if(NOT flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${required_source} contains forbidden ${forbidden_flag}")
        endif()
    endforeach()
endforeach()

execute_process(
    COMMAND "${READELF}" --sections --notes "${LIBFFI_ARCHIVE}"
    RESULT_VARIABLE readelf_result
    OUTPUT_VARIABLE archive_elf
    ERROR_VARIABLE readelf_error
)
if(NOT readelf_result EQUAL 0)
    message(FATAL_ERROR
        "Cannot inspect libffi assembly objects: ${readelf_error}")
endif()
string(REGEX MATCHALL "\\.note\\.GNU-stack" stack_notes "${archive_elf}")
list(LENGTH stack_notes stack_note_count)
list(LENGTH required_assembly_sources assembly_source_count)
if(NOT stack_note_count EQUAL assembly_source_count)
    message(FATAL_ERROR
        "Every libffi assembly object must declare a non-executable stack: "
        "${archive_elf}")
endif()
if(TARGET_ARCH STREQUAL "x86_64")
    string(REGEX MATCHALL "x86 feature: IBT, SHSTK"
        branch_notes "${archive_elf}")
else()
    string(REGEX MATCHALL "aarch64 feature: BTI, PAC"
        branch_notes "${archive_elf}")
endif()
list(LENGTH branch_notes branch_note_count)
if(NOT branch_note_count EQUAL assembly_source_count)
    message(FATAL_ERROR
        "Every libffi assembly object must advertise branch protection: "
        "${archive_elf}")
endif()

get_filename_component(library_dir "${LIBFFI_ARCHIVE}" DIRECTORY)
get_filename_component(build_dir "${COMPILE_COMMANDS}" DIRECTORY)
file(GLOB forbidden_shared_libraries "${library_dir}/libffi.so*")
file(GLOB forbidden_outputs
    "${build_dir}/libffi.so*"
    "${build_dir}/testsuite"
    "${build_dir}/man"
    "${build_dir}/doc")
if(forbidden_shared_libraries OR forbidden_outputs)
    message(FATAL_ERROR
        "libffi built forbidden outputs: ${forbidden_shared_libraries};"
        "${forbidden_outputs}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${LIBFFI_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${LIBFFI_ARCHIVE}")
endif()
execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_nm_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_nm_error
)
if(NOT bootstrap_nm_result EQUAL 0
        OR NOT bootstrap_symbols MATCHES "(^|\n)ffi_prep_cif ")
    message(FATAL_ERROR
        "bootstrap does not contain libffi: ${bootstrap_nm_error}")
endif()
foreach(forbidden_dependency IN ITEMS libffi.so libgcc libstdc++)
    string(FIND "${link_map}" "${forbidden_dependency}" dependency_offset)
    if(NOT dependency_offset EQUAL -1)
        message(FATAL_ERROR
            "bootstrap link map contains forbidden libffi dependency: "
            "${forbidden_dependency}")
    endif()
endforeach()
