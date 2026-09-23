foreach(required_variable IN ITEMS
        BOOTSTRAP
        FONT
        FONT_SHA256
        LINK_MAP
        MANIFEST
        MANIFEST_LOCK
        MANIFEST_LOCK_SHA256
        NM
        RESVG_ARCHIVE)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

foreach(required_file IN ITEMS
        "${BOOTSTRAP}"
        "${FONT}"
        "${LINK_MAP}"
        "${MANIFEST}"
        "${MANIFEST_LOCK}"
        "${RESVG_ARCHIVE}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required resvg artifact is absent: ${required_file}")
    endif()
endforeach()

file(SHA256 "${FONT}" actual_font_sha256)
if(NOT actual_font_sha256 STREQUAL FONT_SHA256)
    message(FATAL_ERROR "Embedded SVG font does not match its pin")
endif()
file(SHA256 "${MANIFEST_LOCK}" actual_lock_sha256)
if(NOT actual_lock_sha256 STREQUAL MANIFEST_LOCK_SHA256)
    message(FATAL_ERROR "resvg shim Cargo.lock does not match its pin")
endif()

file(READ "${MANIFEST}" manifest)
foreach(required_setting IN ITEMS
        "default-features = false"
        "features = [\"raster-images\", \"text\"]"
        "crate-type = [\"staticlib\"]"
        "panic = \"unwind\"")
    string(FIND "${manifest}" "${required_setting}" setting_offset)
    if(setting_offset EQUAL -1)
        message(FATAL_ERROR "resvg shim manifest omits: ${required_setting}")
    endif()
endforeach()
if(manifest MATCHES "system-fonts|memmap-fonts|svgz")
    message(FATAL_ERROR "resvg shim enables a forbidden loader feature")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${RESVG_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE symbols
    ERROR_VARIABLE nm_error)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect resvg shim: ${nm_error}")
endif()
foreach(required_symbol IN ITEMS
        mp_resvg_parse
        mp_resvg_render
        mp_resvg_tree_destroy)
    if(NOT symbols MATCHES "(^|\n)${required_symbol} ")
        message(FATAL_ERROR "resvg shim omits ${required_symbol}")
    endif()
endforeach()

file(READ "${LINK_MAP}" link_map)
if(NOT link_map MATCHES "libmediaproxy_resvg_shim\\.a")
    message(FATAL_ERROR "bootstrap link map omits the resvg shim")
endif()
