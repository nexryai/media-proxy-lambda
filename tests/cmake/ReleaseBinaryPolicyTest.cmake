foreach(required_variable BOOTSTRAP LINK_MAP NM)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

foreach(required_file IN ITEMS "${BOOTSTRAP}" "${LINK_MAP}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required release artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" --format=posix "${BOOTSTRAP}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE symbol_output
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "llvm-nm failed: ${nm_error}")
endif()

set(forbidden_symbol_patterns
    __asan
    __hwasan
    __msan
    __tsan
    __ubsan
    __sanitizer
    LLVMFuzzer
)
foreach(forbidden_pattern IN LISTS forbidden_symbol_patterns)
    if(symbol_output MATCHES "${forbidden_pattern}")
        message(FATAL_ERROR
            "bootstrap contains forbidden diagnostic runtime symbol: "
            "${forbidden_pattern}")
    endif()
endforeach()

file(READ "${LINK_MAP}" link_map)
# Avoid matching GLib's production gtestutils.c implementation. These patterns
# identify GoogleTest/GoogleMock archives, source trees, and C++ symbols.
set(forbidden_test_patterns
    libgtest
    libgmock
    googletest-src
    googlemock-src
    "testing::")
foreach(forbidden_pattern IN LISTS forbidden_test_patterns)
    if(link_map MATCHES "${forbidden_pattern}")
        message(FATAL_ERROR
            "bootstrap link map contains test-only code: ${forbidden_pattern}")
    endif()
endforeach()
