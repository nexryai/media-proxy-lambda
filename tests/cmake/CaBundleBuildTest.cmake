foreach(required_variable IN ITEMS
        BOOTSTRAP
        BUNDLE
        BUNDLE_OBJECT
        EXPECTED_SHA256
        READELF
        TARGET_ARCH)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

foreach(required_file IN ITEMS "${BOOTSTRAP}" "${BUNDLE}" "${BUNDLE_OBJECT}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required CA bundle artifact is absent: ${required_file}")
    endif()
endforeach()

file(SHA256 "${BUNDLE}" actual_sha256)
if(NOT actual_sha256 STREQUAL EXPECTED_SHA256)
    message(FATAL_ERROR "Embedded CA input does not match the dependency lock")
endif()
file(SIZE "${BUNDLE}" bundle_size)
if(NOT bundle_size EQUAL 186446)
    message(FATAL_ERROR "Unexpected CA bundle size: ${bundle_size}")
endif()
file(READ "${BUNDLE}" bundle_text)
string(REGEX MATCHALL "-----BEGIN CERTIFICATE-----" certificates "${bundle_text}")
list(LENGTH certificates certificate_count)
if(NOT certificate_count EQUAL 119)
    message(FATAL_ERROR "Unexpected CA certificate count: ${certificate_count}")
endif()

execute_process(
    COMMAND "${READELF}" --file-header --sections --symbols --wide
        "${BUNDLE_OBJECT}"
    RESULT_VARIABLE object_result
    OUTPUT_VARIABLE object_elf
    ERROR_VARIABLE object_error)
if(NOT object_result EQUAL 0)
    message(FATAL_ERROR "Invalid CA bundle object: ${object_error}")
endif()
if(TARGET_ARCH STREQUAL "x86_64")
    set(expected_machine "Advanced Micro Devices X86-64")
elseif(TARGET_ARCH STREQUAL "arm64")
    set(expected_machine "AArch64")
else()
    message(FATAL_ERROR "Unsupported target architecture: ${TARGET_ARCH}")
endif()
if(NOT object_elf MATCHES "Machine:[ ]+${expected_machine}")
    message(FATAL_ERROR "CA bundle object has the wrong target architecture")
endif()
if(NOT object_elf MATCHES
        "\\.rodata\\.mediaproxy_ca[ ]+PROGBITS[^\n]+[ ]A[ ]")
    message(FATAL_ERROR "CA bundle object is not in an allocatable read-only section")
endif()
foreach(symbol IN ITEMS _binary_cacert_pem_start _binary_cacert_pem_end)
    if(NOT object_elf MATCHES "GLOBAL[ ]+HIDDEN[^\n]+${symbol}")
        message(FATAL_ERROR "CA bundle object does not hide ${symbol}")
    endif()
endforeach()

execute_process(
    COMMAND "${READELF}" --symbols --wide "${BOOTSTRAP}"
    RESULT_VARIABLE bootstrap_result
    OUTPUT_VARIABLE bootstrap_symbols
    ERROR_VARIABLE bootstrap_error)
if(NOT bootstrap_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect bootstrap symbols: ${bootstrap_error}")
endif()
foreach(symbol IN ITEMS _binary_cacert_pem_start _binary_cacert_pem_end)
    if(NOT bootstrap_symbols MATCHES "LOCAL[ ]+HIDDEN[^\n]+${symbol}")
        message(FATAL_ERROR "bootstrap does not contain hidden embedded symbol ${symbol}")
    endif()
endforeach()
