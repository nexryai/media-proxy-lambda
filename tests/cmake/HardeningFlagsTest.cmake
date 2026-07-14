foreach(required_variable
        BUILD_DIR
        COMPILE_COMMANDS
        FORTIFY_INCLUDE_DIR
        NINJA
        TARGET_ARCH)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

set(required_link_flags
    -fuse-ld=lld
    -static-pie
    -flto=thin
    -fsanitize=cfi
    -fsanitize-trap=cfi
    -fno-sanitize-recover=cfi
    "-Xlinker --gc-sections"
    "-Xlinker --fatal-warnings"
    "-Xlinker -z -Xlinker relro"
    "-Xlinker -z -Xlinker now"
    "-Xlinker -z -Xlinker noexecstack"
)

set(hardening_targets
    mediaproxy_stack_smash_probe
    mediaproxy_cfi_violation_probe
    mediaproxy_fortify_probe
)
foreach(hardening_target IN LISTS hardening_targets)
    execute_process(
        COMMAND "${NINJA}" -C "${BUILD_DIR}" -t commands "${hardening_target}"
        RESULT_VARIABLE ninja_result
        OUTPUT_VARIABLE target_commands
        ERROR_VARIABLE ninja_error
    )
    if(NOT ninja_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to inspect ${hardening_target} link command: ${ninja_error}")
    endif()

    string(REPLACE "\n" ";" command_lines "${target_commands}")
    set(link_command "")
    foreach(command_line IN LISTS command_lines)
        if(command_line MATCHES " -o ${hardening_target}( |$)")
            set(link_command "${command_line}")
        endif()
    endforeach()
    if(link_command STREQUAL "")
        message(FATAL_ERROR "No link command found for ${hardening_target}")
    endif()

    foreach(required_flag IN LISTS required_link_flags)
        string(FIND "${link_command}" "${required_flag}" flag_offset)
        if(flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${hardening_target} is missing link flag ${required_flag}: "
                "${link_command}")
        endif()
    endforeach()
endforeach()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if(command_count EQUAL 0)
    message(FATAL_ERROR "Compile command database is empty")
endif()

set(required_flags
    -D_FORTIFY_SOURCE=3
    -fstack-protector-strong
    -ftrivial-auto-var-init=zero
    -fvisibility=hidden
    -flto=thin
    -fsanitize=cfi
    -fsanitize-trap=cfi
    -fno-sanitize-recover=cfi
    -Werror
    "-isystem ${FORTIFY_INCLUDE_DIR}"
)
if(TARGET_ARCH STREQUAL "x86_64")
    list(APPEND required_flags -fstack-clash-protection -fcf-protection=full)
elseif(TARGET_ARCH STREQUAL "arm64")
    list(APPEND required_flags -mbranch-protection=standard)
else()
    message(FATAL_ERROR "Unsupported TARGET_ARCH: ${TARGET_ARCH}")
endif()

set(required_sources
    tests/hardening/stack_smash.cpp
    tests/hardening/cfi_violation.cpp
    tests/hardening/fortify.cpp
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

    foreach(required_flag IN LISTS required_flags)
        string(FIND "${matching_command}" "${required_flag}" flag_offset)
        if(flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${required_source} is missing hardening flag ${required_flag}: "
                "${matching_command}")
        endif()
    endforeach()
endforeach()
