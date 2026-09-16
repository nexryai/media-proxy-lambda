foreach(required_variable IN ITEMS
        AR
        BOOTSTRAP
        FORTIFY_INCLUDE_DIR
        HIGHWAY_ARCHIVE
        HIGHWAY_COMPILE_COMMANDS
        LIBJXL_ARCHIVE
        LIBJXL_COMPILE_COMMANDS
        LINK_MAP
        NM
        TARGET_TRIPLE)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

foreach(required_file IN ITEMS
        "${BOOTSTRAP}"
        "${HIGHWAY_ARCHIVE}"
        "${HIGHWAY_COMPILE_COMMANDS}"
        "${LIBJXL_ARCHIVE}"
        "${LIBJXL_COMPILE_COMMANDS}"
        "${LINK_MAP}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required JPEG XL artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${LIBJXL_ARCHIVE}"
    RESULT_VARIABLE libjxl_ar_result
    OUTPUT_VARIABLE libjxl_members
    ERROR_VARIABLE libjxl_ar_error)
if(NOT libjxl_ar_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect libjxl: ${libjxl_ar_error}")
endif()
string(REGEX MATCHALL "[^\n]+" libjxl_member_list "${libjxl_members}")
list(LENGTH libjxl_member_list libjxl_member_count)
if(NOT libjxl_member_count EQUAL 75)
    message(FATAL_ERROR
        "libjxl decoder contains ${libjxl_member_count} objects instead of 75")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${LIBJXL_ARCHIVE}"
    RESULT_VARIABLE libjxl_nm_result
    OUTPUT_VARIABLE libjxl_symbols
    ERROR_VARIABLE libjxl_nm_error)
if(NOT libjxl_nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect libjxl symbols: ${libjxl_nm_error}")
endif()
foreach(required_symbol IN ITEMS
        JxlDecoderCreate
        JxlDecoderDestroy
        JxlDecoderProcessInput
        JxlDecoderSetImageOutBuffer)
    if(NOT libjxl_symbols MATCHES "(^|\n)${required_symbol} ")
        message(FATAL_ERROR "libjxl does not define ${required_symbol}")
    endif()
endforeach()
if(libjxl_symbols MATCHES "(^|\n)JxlEncoder[^ ]* ")
    message(FATAL_ERROR "libjxl decoder archive contains an encoder API")
endif()

execute_process(
    COMMAND "${NM}" --undefined-only --format=posix "${LIBJXL_ARCHIVE}"
    RESULT_VARIABLE undefined_nm_result
    OUTPUT_VARIABLE undefined_symbols
    ERROR_VARIABLE undefined_nm_error)
if(NOT undefined_nm_result EQUAL 0)
    message(FATAL_ERROR
        "Cannot inspect undefined libjxl symbols: ${undefined_nm_error}")
endif()
if(undefined_symbols MATCHES
        "(^|\n)(Brotli[^ ]*|JxlEncoder[^ ]*|JxlThreadParallelRunner[^ ]*) ")
    message(FATAL_ERROR
        "libjxl decoder retains Brotli, encoder, or thread-runner references")
endif()

execute_process(
    COMMAND "${AR}" t "${HIGHWAY_ARCHIVE}"
    RESULT_VARIABLE highway_ar_result
    OUTPUT_VARIABLE highway_members
    ERROR_VARIABLE highway_ar_error)
if(NOT highway_ar_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect Highway: ${highway_ar_error}")
endif()
string(REGEX MATCHALL "[^\n]+" highway_member_list "${highway_members}")
list(LENGTH highway_member_list highway_member_count)
if(NOT highway_member_count EQUAL 7)
    message(FATAL_ERROR
        "Highway contains ${highway_member_count} objects instead of 7")
endif()

function(check_compile_commands path output_pattern expected_count)
    file(READ "${path}" compile_commands_json)
    string(JSON command_count LENGTH "${compile_commands_json}")
    math(EXPR last_command "${command_count} - 1")
    set(matched_count 0)
    foreach(index RANGE 0 ${last_command})
        string(JSON output GET "${compile_commands_json}" ${index} output)
        if(NOT output MATCHES "${output_pattern}")
            continue()
        endif()
        math(EXPR matched_count "${matched_count} + 1")
        string(JSON compile_command GET
            "${compile_commands_json}" ${index} command)
        foreach(required_flag IN ITEMS
                "--target=${TARGET_TRIPLE}"
                --sysroot=
                -D_FORTIFY_SOURCE=3
                -DHWY_COMPILE_ONLY_SCALAR=1
                -fPIC
                -fstack-protector-strong
                -ftrivial-auto-var-init=zero
                -fvisibility=hidden
                -flto=thin
                -fsanitize=cfi
                -fsanitize-trap=cfi
                -fno-sanitize-recover=cfi
                -std=c++20
                -Werror
                "-isystem ${FORTIFY_INCLUDE_DIR}")
            string(FIND "${compile_command}" "${required_flag}" flag_offset)
            if(flag_offset EQUAL -1)
                message(FATAL_ERROR
                    "${output} is missing ${required_flag}: ${compile_command}")
            endif()
        endforeach()
    endforeach()
    if(NOT matched_count EQUAL expected_count)
        message(FATAL_ERROR
            "${path} contains ${matched_count} matching commands instead of ${expected_count}")
    endif()
endfunction()

check_compile_commands("${HIGHWAY_COMPILE_COMMANDS}" "CMakeFiles/hwy.dir/" 7)
check_compile_commands("${LIBJXL_COMPILE_COMMANDS}" "jxl_dec-obj.dir/" 75)

file(READ "${LINK_MAP}" link_map)
foreach(required_link_input IN ITEMS libjxl_dec.a)
    if(NOT link_map MATCHES "${required_link_input}")
        message(FATAL_ERROR "bootstrap link map omits ${required_link_input}")
    endif()
endforeach()
if(link_map MATCHES
        "libjxl(_cms|_threads)?\\.a|libbrotli|JxlEncoder")
    message(FATAL_ERROR
        "bootstrap link map contains a JXL encoder/CMS/thread or Brotli archive")
endif()
