cmake_minimum_required(VERSION 4.2.3)

project(MediaProxyAdaIdna LANGUAGES CXX)

if(NOT DEFINED MEDIAPROXY_ADA_SOURCE_DIR)
    message(FATAL_ERROR "MEDIAPROXY_ADA_SOURCE_DIR is required")
endif()

foreach(required_source IN ITEMS
        "${MEDIAPROXY_ADA_SOURCE_DIR}/LICENSE-MIT"
        "${MEDIAPROXY_ADA_SOURCE_DIR}/include/ada/ada_idna.h"
        "${MEDIAPROXY_ADA_SOURCE_DIR}/src/ada_idna.cpp")
    if(NOT EXISTS "${required_source}")
        message(FATAL_ERROR "Ada IDNA source is absent: ${required_source}")
    endif()
endforeach()

add_library(ada_idna STATIC
    "${MEDIAPROXY_ADA_SOURCE_DIR}/src/ada_idna.cpp"
)
set_target_properties(ada_idna PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_include_directories(ada_idna PRIVATE
    "${MEDIAPROXY_ADA_SOURCE_DIR}/include"
)
target_compile_options(ada_idna PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wpedantic
)

install(TARGETS ada_idna ARCHIVE DESTINATION lib)
install(FILES
    "${MEDIAPROXY_ADA_SOURCE_DIR}/include/ada/ada_idna.h"
    DESTINATION include/ada
)
install(FILES
    "${MEDIAPROXY_ADA_SOURCE_DIR}/LICENSE-MIT"
    DESTINATION share/licenses/ada-idna
)
