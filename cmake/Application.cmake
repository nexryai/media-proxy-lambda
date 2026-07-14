include(CTest)
include(FetchContent)
include(cmake/Hardening.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

foreach(required_yyjson_artifact IN ITEMS
        "${MEDIAPROXY_YYJSON_INCLUDE_DIR}/yyjson.h"
        "${MEDIAPROXY_YYJSON_LIBRARY}")
    if(NOT EXISTS "${required_yyjson_artifact}")
        message(FATAL_ERROR "Pinned yyjson artifact is absent: ${required_yyjson_artifact}")
    endif()
endforeach()

add_library(mediaproxy_yyjson STATIC IMPORTED GLOBAL)
set_target_properties(mediaproxy_yyjson PROPERTIES
    IMPORTED_LOCATION "${MEDIAPROXY_YYJSON_LIBRARY}"
)

add_executable(bootstrap src/bootstrap.cpp)
target_link_libraries(bootstrap
    PRIVATE
        mediaproxy_hardening
        mediaproxy_warnings
        mediaproxy_yyjson
)
target_link_options(bootstrap PRIVATE
    "LINKER:-Map,${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
)

add_custom_command(
    TARGET bootstrap
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
        "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
        "-DREADELF=${MEDIAPROXY_READELF}"
        "-DNM=${MEDIAPROXY_NM}"
        "-DUNDEFINED_SYMBOLS_FILE=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.undefined-symbols.txt"
        -P "${CMAKE_SOURCE_DIR}/cmake/VerifyStaticElf.cmake"
    VERBATIM
)

if(BUILD_TESTING)
    FetchContent_Declare(googletest
        URL "${MEDIAPROXY_GOOGLETEST_URL}"
        URL_HASH "SHA256=${MEDIAPROXY_GOOGLETEST_SHA256}"
        DOWNLOAD_DIR "${MEDIAPROXY_SOURCE_CACHE}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    target_link_libraries(gtest PRIVATE mediaproxy_hardening)
    target_link_libraries(gtest_main PRIVATE mediaproxy_hardening)

    add_executable(mediaproxy_smoke_test tests/smoke_test.cpp)
    target_link_libraries(mediaproxy_smoke_test
        PRIVATE
            mediaproxy_hardening
            mediaproxy_warnings
            mediaproxy_yyjson
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(mediaproxy_smoke_test DISCOVERY_MODE PRE_TEST)

    add_executable(mediaproxy_stack_smash_probe
        tests/hardening/stack_smash.cpp
    )
    add_executable(mediaproxy_cfi_violation_probe
        tests/hardening/cfi_violation.cpp
    )
    add_executable(mediaproxy_fortify_probe
        tests/hardening/fortify.cpp
    )
    foreach(hardening_probe IN ITEMS
            mediaproxy_stack_smash_probe
            mediaproxy_cfi_violation_probe
            mediaproxy_fortify_probe)
        target_link_libraries(${hardening_probe}
            PRIVATE
                mediaproxy_hardening
                mediaproxy_warnings
        )
    endforeach()

    add_test(
        NAME yyjson-build-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DARCHIVE=${MEDIAPROXY_YYJSON_LIBRARY}"
            "-DAR=${MEDIAPROXY_AR}"
            "-DCOMPILE_COMMANDS=${MEDIAPROXY_YYJSON_COMPILE_COMMANDS}"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DNM=${MEDIAPROXY_NM}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            "-DTARGET_TRIPLE=${MEDIAPROXY_TARGET_TRIPLE}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/YyjsonBuildTest.cmake"
    )
    add_test(
        NAME hardening-flags
        COMMAND "${CMAKE_COMMAND}"
            "-DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DCOMPILE_COMMANDS=${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json"
            "-DFORTIFY_INCLUDE_DIR=${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
            "-DNINJA=${CMAKE_MAKE_PROGRAM}"
            "-DTARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/HardeningFlagsTest.cmake"
    )
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL CMAKE_SYSTEM_PROCESSOR)
        add_test(
            NAME hardening-fortify-safe
            COMMAND mediaproxy_fortify_probe
        )
        add_test(
            NAME hardening-fortify-overflow
            COMMAND "${CMAKE_COMMAND}"
                "-DPROGRAM=$<TARGET_FILE:mediaproxy_fortify_probe>"
                "-DPROGRAM_ARGUMENTS=overflow"
                "-DEXPECTED_RESULT=Illegal instruction"
                -P "${CMAKE_SOURCE_DIR}/tests/cmake/ExpectSignalFailure.cmake"
        )
        add_test(
            NAME hardening-stack-smash
            COMMAND "${CMAKE_COMMAND}"
                "-DPROGRAM=$<TARGET_FILE:mediaproxy_stack_smash_probe>"
                "-DEXPECTED_RESULT=Segmentation fault"
                -P "${CMAKE_SOURCE_DIR}/tests/cmake/ExpectSignalFailure.cmake"
        )
        add_test(
            NAME hardening-cfi-violation
            COMMAND "${CMAKE_COMMAND}"
                "-DPROGRAM=$<TARGET_FILE:mediaproxy_cfi_violation_probe>"
                "-DEXPECTED_RESULT=Illegal instruction"
                -P "${CMAKE_SOURCE_DIR}/tests/cmake/ExpectSignalFailure.cmake"
        )
    endif()

    add_test(
        NAME static-elf-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/StaticElfPolicyTest.cmake"
    )
    add_test(
        NAME bootstrap-static-elf
        COMMAND "${CMAKE_COMMAND}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DREADELF=${MEDIAPROXY_READELF}"
            "-DNM=${MEDIAPROXY_NM}"
            "-DUNDEFINED_SYMBOLS_FILE=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.undefined-symbols.txt"
            -P "${CMAKE_SOURCE_DIR}/cmake/VerifyStaticElf.cmake"
    )
    add_test(
        NAME release-binary-policy
        COMMAND "${CMAKE_COMMAND}"
            "-DBOOTSTRAP=$<TARGET_FILE:bootstrap>"
            "-DLINK_MAP=${CMAKE_CURRENT_BINARY_DIR}/bootstrap.map"
            "-DNM=${MEDIAPROXY_NM}"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/ReleaseBinaryPolicyTest.cmake"
    )
endif()
