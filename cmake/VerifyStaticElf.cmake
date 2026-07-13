foreach(required_variable BOOTSTRAP READELF NM)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

execute_process(
    COMMAND "${READELF}" --file-header "${BOOTSTRAP}"
    RESULT_VARIABLE header_result
    OUTPUT_VARIABLE header_output
    ERROR_VARIABLE header_error
)
if(NOT header_result EQUAL 0)
    message(FATAL_ERROR "llvm-readelf failed: ${header_error}")
endif()
if(NOT header_output MATCHES "Type:[ ]+DYN")
    message(FATAL_ERROR "bootstrap is not a position-independent executable")
endif()

execute_process(
    COMMAND "${READELF}" --program-headers "${BOOTSTRAP}"
    RESULT_VARIABLE program_result
    OUTPUT_VARIABLE program_output
    ERROR_VARIABLE program_error
)
if(NOT program_result EQUAL 0)
    message(FATAL_ERROR "llvm-readelf failed: ${program_error}")
endif()
if(program_output MATCHES "INTERP")
    message(FATAL_ERROR "bootstrap contains an ELF interpreter")
endif()
if(program_output MATCHES "GNU_STACK[^\n]*RWE")
    message(FATAL_ERROR "bootstrap requests an executable stack")
endif()

execute_process(
    COMMAND "${READELF}" --sections "${BOOTSTRAP}"
    RESULT_VARIABLE sections_result
    OUTPUT_VARIABLE sections_output
    ERROR_VARIABLE sections_error
)
if(NOT sections_result EQUAL 0)
    message(FATAL_ERROR "llvm-readelf failed: ${sections_error}")
endif()
if(sections_output MATCHES "[.]dynamic")
    message(FATAL_ERROR "bootstrap contains a dynamic section")
endif()

execute_process(
    COMMAND "${READELF}" --dynamic-table "${BOOTSTRAP}"
    RESULT_VARIABLE dynamic_result
    OUTPUT_VARIABLE dynamic_output
    ERROR_VARIABLE dynamic_error
)
if(NOT dynamic_result EQUAL 0)
    message(FATAL_ERROR "llvm-readelf failed: ${dynamic_error}")
endif()
if(dynamic_output MATCHES "NEEDED")
    message(FATAL_ERROR "bootstrap contains a DT_NEEDED entry")
endif()

execute_process(
    COMMAND "${NM}" --undefined-only "${BOOTSTRAP}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE nm_output
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "llvm-nm failed: ${nm_error}")
endif()
if(NOT nm_output STREQUAL "")
    message(FATAL_ERROR "bootstrap contains unresolved symbols: ${nm_output}")
endif()
