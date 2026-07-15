if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|x86_64)$")
    message(FATAL_ERROR
        "Unsupported target processor: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# CMake's compiler ABI probe cannot link while the musl sysroot is still being
# assembled. Supply the ABI facts shared by the two supported Lambda targets
# after project() enables C, where they remain visible to feature checks.
set(CMAKE_C_BYTE_ORDER LITTLE_ENDIAN)
set(CMAKE_SIZEOF_VOID_P 8)
