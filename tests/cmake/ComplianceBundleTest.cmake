foreach(required_variable IN ITEMS
        COMPLIANCE_DIRECTORY
        LOCK_FILE
        BOOTSTRAP
        BOOTSTRAP_ARTIFACT
        READELF
        NM)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

foreach(required_path IN ITEMS
        "${COMPLIANCE_DIRECTORY}/sbom.spdx"
        "${COMPLIANCE_DIRECTORY}/THIRD_PARTY_NOTICES.md"
        "${COMPLIANCE_DIRECTORY}/corresponding-source/README.md"
        "${COMPLIANCE_DIRECTORY}/relink/README.md"
        "${COMPLIANCE_DIRECTORY}/relink/relink.sh")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Compliance artifact is absent: ${required_path}")
    endif()
endforeach()

file(READ "${LOCK_FILE}" dependency_lock)
file(READ "${COMPLIANCE_DIRECTORY}/sbom.spdx" spdx)
file(SHA256 "${BOOTSTRAP_ARTIFACT}" bootstrap_artifact_sha256)
string(FIND "${spdx}"
    "PackageChecksum: SHA256: ${bootstrap_artifact_sha256}"
    bootstrap_checksum_position)
if(bootstrap_checksum_position EQUAL -1)
    message(FATAL_ERROR "SBOM does not identify the stripped bootstrap")
endif()
string(JSON dependency_count LENGTH "${dependency_lock}" dependencies)
math(EXPR dependency_last "${dependency_count} - 1")
foreach(dependency_index RANGE 0 ${dependency_last})
    string(JSON dependency_scope ERROR_VARIABLE scope_error
        GET "${dependency_lock}" dependencies ${dependency_index} scope)
    if(scope_error)
        set(dependency_scope runtime)
    endif()
    if(dependency_scope STREQUAL "test")
        continue()
    endif()
    string(JSON dependency_name GET "${dependency_lock}"
        dependencies ${dependency_index} name)
    string(JSON dependency_sha256 GET "${dependency_lock}"
        dependencies ${dependency_index} sha256)
    string(FIND "${spdx}" "PackageName: ${dependency_name}\n"
        package_position)
    if(package_position EQUAL -1)
        message(FATAL_ERROR
            "SBOM does not describe dependency ${dependency_name}")
    endif()
    string(FIND "${spdx}"
        "PackageChecksum: SHA256: ${dependency_sha256}"
        checksum_position)
    if(checksum_position EQUAL -1)
        message(FATAL_ERROR
            "SBOM hash is absent for dependency ${dependency_name}")
    endif()
endforeach()

foreach(lgpl_archive IN ITEMS
        glib-2.88.2.tar.xz
        libheif-1.22.2.tar.gz
        vips-8.18.2.tar.xz
        libexif-0.6.26.tar.xz)
    if(NOT EXISTS
            "${COMPLIANCE_DIRECTORY}/corresponding-source/archives/${lgpl_archive}")
        message(FATAL_ERROR
            "Corresponding-source archive is absent: ${lgpl_archive}")
    endif()
endforeach()

execute_process(
    COMMAND "${COMPLIANCE_DIRECTORY}/relink/relink.sh"
    RESULT_VARIABLE relink_result
    OUTPUT_VARIABLE relink_stdout
    ERROR_VARIABLE relink_stderr
    TIMEOUT 300)
if(NOT relink_result EQUAL 0)
    message(FATAL_ERROR
        "Relink command failed:\n${relink_stdout}\n${relink_stderr}")
endif()
set(relinked_bootstrap
    "${COMPLIANCE_DIRECTORY}/relink/application/bootstrap")
file(SHA256 "${BOOTSTRAP}" bootstrap_sha256)
file(SHA256 "${relinked_bootstrap}" relinked_sha256)
if(NOT bootstrap_sha256 STREQUAL relinked_sha256)
    message(FATAL_ERROR
        "Relinked bootstrap is not byte-identical to the release input")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DBOOTSTRAP=${relinked_bootstrap}"
        "-DREADELF=${READELF}"
        "-DNM=${NM}"
        "-DUNDEFINED_SYMBOLS_FILE=${COMPLIANCE_DIRECTORY}/relink/bootstrap.undefined-symbols.txt"
        -P "${CMAKE_CURRENT_LIST_DIR}/../../cmake/VerifyStaticElf.cmake"
    RESULT_VARIABLE verify_result
    OUTPUT_VARIABLE verify_stdout
    ERROR_VARIABLE verify_stderr)
if(NOT verify_result EQUAL 0)
    message(FATAL_ERROR
        "Relinked bootstrap failed ELF policy:\n${verify_stdout}\n${verify_stderr}")
endif()
