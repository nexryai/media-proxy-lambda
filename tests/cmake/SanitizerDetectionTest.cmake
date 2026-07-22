foreach(required_variable IN ITEMS LOADER EXECUTABLE MODE EXPECTED)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:fast_unwind_on_malloc=0"
        "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1"
        "${LOADER}" "${EXECUTABLE}" "${MODE}"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_stdout
    ERROR_VARIABLE probe_stderr
    TIMEOUT 10)

if(probe_result EQUAL 0)
    message(FATAL_ERROR "Sanitizer probe unexpectedly succeeded")
endif()
set(probe_output "${probe_stdout}${probe_stderr}")
if(NOT probe_output MATCHES "${EXPECTED}")
    message(FATAL_ERROR
        "Sanitizer probe did not report ${EXPECTED}:\n${probe_output}")
endif()
