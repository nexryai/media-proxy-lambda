foreach(required_variable
        ARCHIVE
        AR
        COMPILE_COMMANDS
        FORTIFY_INCLUDE_DIR
        NM
        TARGET_ARCH
        TARGET_TRIPLE)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

foreach(required_file IN ITEMS "${ARCHIVE}" "${COMPILE_COMMANDS}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required yyjson build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error
)
if(NOT ar_result EQUAL 0)
    message(FATAL_ERROR "llvm-ar failed for yyjson: ${ar_error}")
endif()
if(NOT archive_members MATCHES "yyjson\\.c\\.o")
    message(FATAL_ERROR "yyjson archive does not contain yyjson.c.o: ${archive_members}")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE defined_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "llvm-nm failed for yyjson: ${nm_error}")
endif()
foreach(required_symbol yyjson_read_opts yyjson_mut_write_opts)
    if(NOT defined_symbols MATCHES "${required_symbol}")
        message(FATAL_ERROR "yyjson archive is missing ${required_symbol}")
    endif()
endforeach()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(command_count EQUAL 0)
    message(FATAL_ERROR "yyjson compile command database is empty")
endif()

set(yyjson_compile_command "")
math(EXPR last_command "${command_count} - 1")
foreach(index RANGE 0 ${last_command})
    string(JSON source_file GET "${compile_commands_json}" ${index} file)
    if(source_file MATCHES "/src/yyjson\\.c$")
        string(JSON yyjson_compile_command GET
            "${compile_commands_json}" ${index} command)
        break()
    endif()
endforeach()
if(yyjson_compile_command STREQUAL "")
    message(FATAL_ERROR "No compile command found for yyjson.c")
endif()

set(required_flags
    "--target=${TARGET_TRIPLE}"
    -D_FORTIFY_SOURCE=3
    -DYYJSON_DISABLE_INCR_READER
    -DYYJSON_DISABLE_NON_STANDARD
    -DYYJSON_DISABLE_UTILS
    -fPIC
    -fstack-protector-strong
    -ftrivial-auto-var-init=zero
    -fvisibility=hidden
    -flto=thin
    -fsanitize=cfi
    -fsanitize-trap=cfi
    -fno-sanitize-recover=cfi
    "-isystem ${FORTIFY_INCLUDE_DIR}"
)
if(TARGET_ARCH STREQUAL "x86_64")
    list(APPEND required_flags -fstack-clash-protection -fcf-protection=full)
elseif(TARGET_ARCH STREQUAL "arm64")
    list(APPEND required_flags -mbranch-protection=standard)
else()
    message(FATAL_ERROR "Unsupported TARGET_ARCH: ${TARGET_ARCH}")
endif()

foreach(required_flag IN LISTS required_flags)
    string(FIND "${yyjson_compile_command}" "${required_flag}" flag_offset)
    if(flag_offset EQUAL -1)
        message(FATAL_ERROR
            "yyjson.c is missing build flag ${required_flag}: "
            "${yyjson_compile_command}")
    endif()
endforeach()
