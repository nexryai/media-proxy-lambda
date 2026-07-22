cmake_minimum_required(VERSION 4.2.3)

foreach(required_variable IN ITEMS
        LOCK_FILE
        SOURCE_CACHE
        PROJECT_SOURCE_DIRECTORY
        APPLICATION_BUILD_DIRECTORY
        SYSROOT
        NINJA
        CLANGXX
        BOOTSTRAP
        BOOTSTRAP_ARTIFACT
        LINK_MAP
        UNDEFINED_SYMBOLS_FILE
        ARTIFACT_DIRECTORY
        OUTPUT_DIRECTORY
        TARGET_ARCH)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

set(expected_output_directory "${ARTIFACT_DIRECTORY}/compliance")
cmake_path(NORMAL_PATH expected_output_directory)
cmake_path(NORMAL_PATH OUTPUT_DIRECTORY)
if(NOT OUTPUT_DIRECTORY STREQUAL expected_output_directory)
    message(FATAL_ERROR
        "Compliance output must be the artifact compliance directory")
endif()
foreach(required_file IN ITEMS
        "${LOCK_FILE}"
        "${BOOTSTRAP}"
        "${BOOTSTRAP_ARTIFACT}"
        "${LINK_MAP}"
        "${UNDEFINED_SYMBOLS_FILE}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Compliance input is absent: ${required_file}")
    endif()
endforeach()

file(READ "${LOCK_FILE}" dependency_lock)
string(JSON lock_schema ERROR_VARIABLE lock_error
    GET "${dependency_lock}" schemaVersion)
if(lock_error OR NOT lock_schema EQUAL 1)
    message(FATAL_ERROR "Unsupported dependency lock schema")
endif()
string(JSON dependency_count LENGTH "${dependency_lock}" dependencies)
if(dependency_count LESS 1)
    message(FATAL_ERROR "Dependency lock is empty")
endif()

file(GLOB source_candidates LIST_DIRECTORIES FALSE "${SOURCE_CACHE}/*")
if(NOT source_candidates)
    message(FATAL_ERROR "Pinned dependency source cache is empty")
endif()
foreach(source_candidate IN LISTS source_candidates)
    file(SHA256 "${source_candidate}" source_candidate_sha256)
    list(APPEND source_candidate_hashes "${source_candidate_sha256}")
endforeach()

file(SHA256 "${LOCK_FILE}" lock_sha256)
file(SHA256 "${BOOTSTRAP}" bootstrap_sha256)
file(SHA256 "${BOOTSTRAP_ARTIFACT}" bootstrap_artifact_sha256)
string(CONCAT spdx
    "SPDXVersion: SPDX-2.3\n"
    "DataLicense: CC0-1.0\n"
    "SPDXID: SPDXRef-DOCUMENT\n"
    "DocumentName: MediaProxy-${TARGET_ARCH}\n"
    "DocumentNamespace: https://mediaproxy.invalid/spdx/${lock_sha256}\n"
    "Creator: Tool: MediaProxy CMake compliance generator\n"
    "Created: 1970-01-01T00:00:00Z\n"
    "CreatorComment: <text>The fixed epoch makes release evidence reproducible; the dependency lock hash identifies its exact inputs.</text>\n"
    "\nPackageName: MediaProxy-bootstrap\n"
    "SPDXID: SPDXRef-Package-MediaProxy-bootstrap\n"
    "PackageVersion: NOASSERTION\n"
    "PackageDownloadLocation: NOASSERTION\n"
    "FilesAnalyzed: false\n"
    "PackageChecksum: SHA256: ${bootstrap_artifact_sha256}\n"
    "PackageLicenseConcluded: MIT\n"
    "PackageLicenseDeclared: MIT\n"
    "PackageCopyrightText: NOASSERTION\n"
    "Relationship: SPDXRef-DOCUMENT DESCRIBES SPDXRef-Package-MediaProxy-bootstrap\n")
string(CONCAT notices
    "# Third-party notices\n\n"
    "This index was generated from `dependencies.lock.json`. Upstream license "
    "and notice files are retained under `licenses/`; corresponding source for "
    "the statically linked LGPL components is under `corresponding-source/`.\n\n"
    "| Package | Version | Declared license | Source | SHA-256 |\n"
    "| --- | --- | --- | --- | --- |\n")
set(lgpl_packages glib libheif libvips libexif)
math(EXPR dependency_last "${dependency_count} - 1")

file(REMOVE_RECURSE "${OUTPUT_DIRECTORY}")
file(MAKE_DIRECTORY
    "${OUTPUT_DIRECTORY}/licenses"
    "${OUTPUT_DIRECTORY}/corresponding-source/archives"
    "${OUTPUT_DIRECTORY}/corresponding-source/patches"
    "${OUTPUT_DIRECTORY}/relink/application/CMakeFiles"
    "${OUTPUT_DIRECTORY}/relink/sysroot/usr")

foreach(dependency_index RANGE 0 ${dependency_last})
    string(JSON dependency_scope ERROR_VARIABLE scope_error
        GET "${dependency_lock}" dependencies ${dependency_index} scope)
    if(scope_error)
        set(dependency_scope runtime)
    endif()
    if(dependency_scope STREQUAL "test")
        continue()
    elseif(NOT dependency_scope STREQUAL "runtime")
        message(FATAL_ERROR
            "Dependency ${dependency_index} has an invalid scope")
    endif()
    foreach(field IN ITEMS name version revision url sha256 license reason)
        string(JSON dependency_${field} ERROR_VARIABLE field_error
            GET "${dependency_lock}" dependencies ${dependency_index} ${field})
        if(field_error OR dependency_${field} STREQUAL "")
            message(FATAL_ERROR
                "Dependency ${dependency_index} has no valid ${field}")
        endif()
    endforeach()
    string(LENGTH "${dependency_sha256}" dependency_sha256_length)
    if(NOT dependency_sha256 MATCHES "^[0-9a-f]+$"
            OR NOT dependency_sha256_length EQUAL 64)
        message(FATAL_ERROR
            "Dependency ${dependency_name} has an invalid SHA-256")
    endif()

    list(FIND source_candidate_hashes "${dependency_sha256}" source_index)
    if(source_index EQUAL -1)
        message(FATAL_ERROR
            "No source-cache file matches ${dependency_name} ${dependency_sha256}")
    endif()
    list(GET source_candidates ${source_index} dependency_source)
    get_filename_component(dependency_source_name "${dependency_source}" NAME)

    string(REGEX REPLACE "[^A-Za-z0-9.-]" "-" package_spdx_id
        "${dependency_name}")
    string(APPEND spdx
        "\nPackageName: ${dependency_name}\n"
        "SPDXID: SPDXRef-Package-${package_spdx_id}\n"
        "PackageVersion: ${dependency_version}\n"
        "PackageDownloadLocation: ${dependency_url}\n"
        "FilesAnalyzed: false\n"
        "PackageChecksum: SHA256: ${dependency_sha256}\n"
        "PackageLicenseConcluded: ${dependency_license}\n"
        "PackageLicenseDeclared: ${dependency_license}\n"
        "PackageCopyrightText: NOASSERTION\n"
        "PackageComment: <text>Revision: ${dependency_revision}. ${dependency_reason}</text>\n"
        "Relationship: SPDXRef-Package-MediaProxy-bootstrap DEPENDS_ON SPDXRef-Package-${package_spdx_id}\n")
    string(REPLACE "|" "\\|" notice_license "${dependency_license}")
    string(APPEND notices
        "| ${dependency_name} | ${dependency_version} | ${notice_license} | "
        "[upstream](${dependency_url}) | `${dependency_sha256}` |\n")

    set(license_destination
        "${OUTPUT_DIRECTORY}/licenses/${dependency_name}")
    file(MAKE_DIRECTORY "${license_destination}")
    if(dependency_source_name MATCHES "\\.(tar\\..*|tgz|zip)$")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E tar tf "${dependency_source}"
            RESULT_VARIABLE archive_list_result
            OUTPUT_VARIABLE archive_listing
            ERROR_VARIABLE archive_list_error)
        if(NOT archive_list_result EQUAL 0)
            message(FATAL_ERROR
                "Could not list ${dependency_source_name}: ${archive_list_error}")
        endif()
        string(REPLACE "\n" ";" archive_entries "${archive_listing}")
        set(license_entries "")
        foreach(archive_entry IN LISTS archive_entries)
            get_filename_component(archive_basename "${archive_entry}" NAME)
            if(archive_basename MATCHES
                    "^(LICENSE([.-].*)?|COPYING([.-].*)?|COPYRIGHT([.-].*)?|NOTICE([.-].*)?|PATENTS([.-].*)?)$"
                    OR archive_entry MATCHES "/LICENSES/[^/]+$")
                list(APPEND license_entries "${archive_entry}")
            endif()
        endforeach()
        if(NOT license_entries)
            message(FATAL_ERROR
                "No license or notice file was found in ${dependency_source_name}")
        endif()
        file(ARCHIVE_EXTRACT
            INPUT "${dependency_source}"
            DESTINATION "${license_destination}"
            PATTERNS ${license_entries})
    else()
        file(COPY "${dependency_source}"
            DESTINATION "${license_destination}")
    endif()

    if(dependency_name IN_LIST lgpl_packages)
        file(COPY "${dependency_source}"
            DESTINATION
                "${OUTPUT_DIRECTORY}/corresponding-source/archives")
        string(JSON patch_type ERROR_VARIABLE patch_error
            TYPE "${dependency_lock}" dependencies ${dependency_index} patches)
        if(NOT patch_error AND patch_type STREQUAL "ARRAY")
            string(JSON patch_count LENGTH "${dependency_lock}"
                dependencies ${dependency_index} patches)
            if(patch_count GREATER 0)
                math(EXPR patch_last "${patch_count} - 1")
                foreach(patch_index RANGE 0 ${patch_last})
                    string(JSON patch_path GET "${dependency_lock}"
                        dependencies ${dependency_index} patches
                        ${patch_index} path)
                    set(patch_source
                        "${PROJECT_SOURCE_DIRECTORY}/${patch_path}")
                    if(NOT EXISTS "${patch_source}")
                        message(FATAL_ERROR
                            "Locked patch is absent: ${patch_path}")
                    endif()
                    file(COPY "${patch_source}"
                        DESTINATION
                            "${OUTPUT_DIRECTORY}/corresponding-source/patches/${dependency_name}")
                endforeach()
            endif()
        endif()
    endif()
endforeach()

string(APPEND spdx
    "\nLicenseID: LicenseRef-AOM-Patent-License-1.0\n"
    "ExtractedText: <text>See the libaom PATENTS file retained in the license bundle.</text>\n"
    "LicenseName: Alliance for Open Media Patent License 1.0\n")
file(WRITE "${OUTPUT_DIRECTORY}/sbom.spdx" "${spdx}")
file(WRITE "${OUTPUT_DIRECTORY}/THIRD_PARTY_NOTICES.md" "${notices}")
file(COPY "${PROJECT_SOURCE_DIRECTORY}/LICENSE"
    DESTINATION "${OUTPUT_DIRECTORY}")
file(COPY "${LOCK_FILE}" DESTINATION "${OUTPUT_DIRECTORY}")

set(corresponding_build_directory
    "${OUTPUT_DIRECTORY}/corresponding-source/mediaproxy-build")
file(MAKE_DIRECTORY "${corresponding_build_directory}")
foreach(build_input IN ITEMS
        .devcontainer
        .editorconfig
        .gitattributes
        .github
        .gitignore
        AGENTS.md
        CMakeLists.txt
        CMakePresets.json
        dependencies.lock.json
        LICENSE
        README.md
        SPECIFICATION.md
        PLANS.md
        cmake
        include
        src
        tests)
    file(COPY "${PROJECT_SOURCE_DIRECTORY}/${build_input}"
        DESTINATION "${corresponding_build_directory}")
endforeach()
file(WRITE "${OUTPUT_DIRECTORY}/corresponding-source/README.md"
    "# Corresponding source\n\n"
    "The `archives` directory contains the exact hash-verified upstream source "
    "archives for GLib, libheif, libvips, and libexif. Local changes are in "
    "`patches`; `mediaproxy-build` contains the CMake superbuild, toolchain, "
    "dependency lock, and application source needed to rebuild them.\n")

file(COPY "${SYSROOT}/usr/lib"
    DESTINATION "${OUTPUT_DIRECTORY}/relink/sysroot/usr")
foreach(first_party_library IN ITEMS
        libmediaproxy_http.a
        libmediaproxy_handler.a
        libmediaproxy_media.a
        libmediaproxy_runtime.a)
    set(library_path
        "${APPLICATION_BUILD_DIRECTORY}/${first_party_library}")
    if(NOT EXISTS "${library_path}")
        message(FATAL_ERROR "Relink library is absent: ${library_path}")
    endif()
    file(COPY "${library_path}"
        DESTINATION "${OUTPUT_DIRECTORY}/relink/application")
endforeach()
file(COPY
    "${APPLICATION_BUILD_DIRECTORY}/CMakeFiles/bootstrap.dir"
    DESTINATION "${OUTPUT_DIRECTORY}/relink/application/CMakeFiles")
file(COPY "${LINK_MAP}" "${UNDEFINED_SYMBOLS_FILE}"
    DESTINATION "${OUTPUT_DIRECTORY}/relink")

execute_process(
    COMMAND "${NINJA}" -C "${APPLICATION_BUILD_DIRECTORY}"
        -t commands bootstrap
    RESULT_VARIABLE commands_result
    OUTPUT_VARIABLE bootstrap_commands
    ERROR_VARIABLE commands_error)
if(NOT commands_result EQUAL 0)
    message(FATAL_ERROR
        "Could not obtain the bootstrap link command: ${commands_error}")
endif()
string(REPLACE "\n" ";" bootstrap_command_lines "${bootstrap_commands}")
set(bootstrap_link_command "")
foreach(command_line IN LISTS bootstrap_command_lines)
    if(command_line MATCHES
            "^: && .*clang\\+\\+[^ ]* .* -o bootstrap  ")
        set(bootstrap_link_command "${command_line}")
    endif()
endforeach()
if(bootstrap_link_command STREQUAL "")
    message(FATAL_ERROR "Could not identify the bootstrap link command")
endif()
string(REGEX REPLACE "^: && " "" bootstrap_link_command
    "${bootstrap_link_command}")
string(REGEX REPLACE " && cd .*" "" bootstrap_link_command
    "${bootstrap_link_command}")
set(shell_dollar "$")
get_filename_component(clangxx_name "${CLANGXX}" NAME)
string(REPLACE "${CLANGXX}"
    "\"${shell_dollar}{CXX:-${clangxx_name}}\""
    bootstrap_link_command "${bootstrap_link_command}")
string(REPLACE "${SYSROOT}"
    "\"${shell_dollar}{relink_root}/sysroot\""
    bootstrap_link_command "${bootstrap_link_command}")
string(REPLACE "${APPLICATION_BUILD_DIRECTORY}"
    "\"${shell_dollar}{relink_root}/application\""
    bootstrap_link_command "${bootstrap_link_command}")

set(relink_script "${OUTPUT_DIRECTORY}/relink/relink.sh")
file(WRITE "${relink_script}"
    "#!/bin/sh\n"
    "set -eu\n\n"
    "relink_root=$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd -P)\n"
    "cd \"$relink_root/application\"\n"
    "${bootstrap_link_command}\n")
file(CHMOD "${relink_script}"
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)
file(WRITE "${OUTPUT_DIRECTORY}/relink/README.md"
    "# Static relinking materials\n\n"
    "This directory contains the first-party ${TARGET_ARCH} ThinLTO objects and archives, "
    "the complete static link-time sysroot, the original link evidence, and a "
    "portable reconstruction of the exact bootstrap link command. Replace an "
    "LGPL archive in `sysroot/usr/lib` with an API/ABI-compatible ${TARGET_ARCH} archive "
    "built by LLVM 22, then run `./relink.sh`. Set `CXX` when `clang++-22` is "
    "not on `PATH`. The unstripped result is `application/bootstrap`. The "
    "corresponding-source build files document the required hardening options.\n")

file(SHA256 "${OUTPUT_DIRECTORY}/sbom.spdx" spdx_sha256)
file(SHA256 "${OUTPUT_DIRECTORY}/THIRD_PARTY_NOTICES.md" notices_sha256)
file(WRITE "${OUTPUT_DIRECTORY}/MANIFEST.sha256"
    "${bootstrap_artifact_sha256}  ../bootstrap\n"
    "${lock_sha256}  dependencies.lock.json\n"
    "${spdx_sha256}  sbom.spdx\n"
    "${notices_sha256}  THIRD_PARTY_NOTICES.md\n")
file(WRITE "${OUTPUT_DIRECTORY}/BOOTSTRAP.sha256"
    "${bootstrap_sha256}  unstripped-bootstrap\n")
file(WRITE "${OUTPUT_DIRECTORY}/.stamp" "${lock_sha256}\n")
