foreach(required_variable PROGRAM EXPECTED_RESULT)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

if(NOT EXISTS "${PROGRAM}")
    message(FATAL_ERROR "Program does not exist: ${PROGRAM}")
endif()

execute_process(
    COMMAND "${PROGRAM}" ${PROGRAM_ARGUMENTS}
    RESULT_VARIABLE process_result
    OUTPUT_VARIABLE process_output
    ERROR_VARIABLE process_error
)

if(process_result MATCHES "^[0-9]+$")
    message(FATAL_ERROR
        "Expected signal '${EXPECTED_RESULT}', but program exited ${process_result}: "
        "${process_output}${process_error}")
endif()

if(NOT process_result MATCHES "${EXPECTED_RESULT}")
    message(FATAL_ERROR
        "Expected signal '${EXPECTED_RESULT}', got '${process_result}': "
        "${process_output}${process_error}")
endif()
