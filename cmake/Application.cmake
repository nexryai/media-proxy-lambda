include(CTest)
include(FetchContent)
include(cmake/Hardening.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(bootstrap src/bootstrap.cpp)
target_link_libraries(bootstrap
    PRIVATE
        mediaproxy_hardening
        mediaproxy_warnings
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
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(mediaproxy_smoke_test DISCOVERY_MODE PRE_TEST)

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
endif()
