function(mediaproxy_lock_get dependency field output)
    file(READ "${CMAKE_SOURCE_DIR}/dependencies.lock.json" lock_json)
    string(JSON dependency_count LENGTH "${lock_json}" dependencies)

    if(dependency_count EQUAL 0)
        message(FATAL_ERROR "Dependency lock contains no entries")
    endif()

    math(EXPR last_dependency "${dependency_count} - 1")
    foreach(index RANGE 0 ${last_dependency})
        string(JSON candidate GET "${lock_json}" dependencies ${index} name)
        if(candidate STREQUAL dependency)
            string(JSON value GET "${lock_json}" dependencies ${index} ${field})
            set(${output} "${value}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    message(FATAL_ERROR "Dependency '${dependency}' is absent from dependencies.lock.json")
endfunction()

function(mediaproxy_lock_get_patch dependency patch_index field output)
    file(READ "${CMAKE_SOURCE_DIR}/dependencies.lock.json" lock_json)
    string(JSON dependency_count LENGTH "${lock_json}" dependencies)
    math(EXPR last_dependency "${dependency_count} - 1")

    foreach(index RANGE 0 ${last_dependency})
        string(JSON candidate GET "${lock_json}" dependencies ${index} name)
        if(candidate STREQUAL dependency)
            string(JSON value GET
                "${lock_json}" dependencies ${index} patches ${patch_index} ${field})
            set(${output} "${value}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    message(FATAL_ERROR "Dependency '${dependency}' is absent from dependencies.lock.json")
endfunction()
