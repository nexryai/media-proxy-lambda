cmake_minimum_required(VERSION 4.2.3)

foreach(required_variable IN ITEMS
        CARGO
        CARGO_HOME
        MANIFEST
        RUST_STD_MANIFEST
        SOURCE_CACHE
        VENDOR_DIRECTORY
        OFFLINE_CARGO_HOME)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${VENDOR_DIRECTORY}" "${OFFLINE_CARGO_HOME}")
file(MAKE_DIRECTORY
    "${VENDOR_DIRECTORY}" "${OFFLINE_CARGO_HOME}" "${SOURCE_CACHE}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "CARGO_HOME=${CARGO_HOME}"
        "${CARGO}" vendor
        --locked
        --versioned-dirs
        --manifest-path "${MANIFEST}"
        --sync "${RUST_STD_MANIFEST}"
        "${VENDOR_DIRECTORY}"
    RESULT_VARIABLE vendor_result
    OUTPUT_VARIABLE vendor_config
    ERROR_VARIABLE vendor_error)
if(NOT vendor_result EQUAL 0)
    message(FATAL_ERROR "Could not vendor pinned Cargo sources: ${vendor_error}")
endif()
file(WRITE "${OFFLINE_CARGO_HOME}/config.toml"
    "${vendor_config}\n[net]\noffline = true\n")

file(GLOB_RECURSE crate_archives LIST_DIRECTORIES FALSE
    "${CARGO_HOME}/registry/cache/*.crate")
if(NOT crate_archives)
    message(FATAL_ERROR "Cargo did not retain any pinned crate archives")
endif()
foreach(crate_archive IN LISTS crate_archives)
    file(COPY "${crate_archive}" DESTINATION "${SOURCE_CACHE}")
endforeach()
