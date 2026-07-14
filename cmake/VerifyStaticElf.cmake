foreach(required_variable BOOTSTRAP READELF NM)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/StaticElfPolicy.cmake")

execute_process(
    COMMAND "${READELF}" --file-header "${BOOTSTRAP}"
    RESULT_VARIABLE header_result
    OUTPUT_VARIABLE header_output
    ERROR_VARIABLE header_error
)
if(NOT header_result EQUAL 0)
    message(FATAL_ERROR "llvm-readelf failed: ${header_error}")
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
execute_process(
    COMMAND "${READELF}" --dynamic-table "${BOOTSTRAP}"
    RESULT_VARIABLE dynamic_result
    OUTPUT_VARIABLE dynamic_output
    ERROR_VARIABLE dynamic_error
)
if(NOT dynamic_result EQUAL 0)
    message(FATAL_ERROR "llvm-readelf failed: ${dynamic_error}")
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
mediaproxy_validate_static_elf_outputs(
    "${header_output}"
    "${program_output}"
    "${dynamic_output}"
    "${nm_output}"
    validation_error
)
if(NOT validation_error STREQUAL "")
    message(FATAL_ERROR "${validation_error}")
endif()
