if(NOT DEFINED GIO_PKGCONFIG)
    message(FATAL_ERROR "GIO_PKGCONFIG is required")
endif()
if(NOT EXISTS "${GIO_PKGCONFIG}")
    message(FATAL_ERROR "GIO pkg-config metadata is absent: ${GIO_PKGCONFIG}")
endif()

file(READ "${GIO_PKGCONFIG}" contents)
set(original [=[Requires: glib-2.0, gobject-2.0, gmodule-no-export-2.0
Libs: -L${libdir} -lgio-2.0 -lz]=])
set(replacement [=[Requires: glib-2.0, gobject-2.0, gmodule-no-export-2.0, zlib
Libs: -L${libdir} -lgio-2.0]=])
string(REPLACE "${original}" "${replacement}" normalized "${contents}")
if(normalized STREQUAL contents)
    string(FIND "${contents}" "${replacement}" replacement_offset)
    if(replacement_offset EQUAL -1)
        message(FATAL_ERROR
            "GIO pkg-config dependency layout changed upstream: "
            "${GIO_PKGCONFIG}")
    endif()
endif()
file(WRITE "${GIO_PKGCONFIG}" "${normalized}")
