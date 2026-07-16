include(ExternalProject)
include(cmake/DependencyLock.cmake)

set(MEDIAPROXY_TARGET_ARCH "x86_64" CACHE STRING "Lambda target architecture")
set_property(CACHE MEDIAPROXY_TARGET_ARCH PROPERTY STRINGS x86_64 arm64)

if(MEDIAPROXY_TARGET_ARCH STREQUAL "x86_64")
    set(target_triple x86_64-linux-musl)
    set(target_processor x86_64)
    set(compiler_rt_arch x86_64)
    set(kernel_arch x86)
    set(dependency_arch_hardening_flags
        -fstack-clash-protection
        -fcf-protection=full
    )
elseif(MEDIAPROXY_TARGET_ARCH STREQUAL "arm64")
    set(target_triple aarch64-linux-musl)
    set(target_processor aarch64)
    set(compiler_rt_arch aarch64)
    set(kernel_arch arm64)
    set(dependency_arch_hardening_flags -mbranch-protection=standard)
else()
    message(FATAL_ERROR "MEDIAPROXY_TARGET_ARCH must be x86_64 or arm64")
endif()

find_program(host_clang NAMES clang-22 REQUIRED)
find_program(host_clangxx NAMES clang++-22 REQUIRED)
find_program(host_lld NAMES ld.lld-22 REQUIRED)
find_program(host_ar NAMES llvm-ar-22 REQUIRED)
find_program(host_ranlib NAMES llvm-ranlib-22 REQUIRED)
find_program(host_nm NAMES llvm-nm-22 REQUIRED)
find_program(host_strip NAMES llvm-strip-22 REQUIRED)
find_program(host_readelf NAMES llvm-readelf-22 REQUIRED)
find_program(host_make NAMES make REQUIRED)
find_program(host_git NAMES git REQUIRED)
find_program(host_meson NAMES meson REQUIRED)
find_program(host_ninja NAMES ninja REQUIRED)
find_program(host_pkgconf NAMES pkgconf REQUIRED)

set(source_cache "${CMAKE_SOURCE_DIR}/.cache/sources")
set(sysroot "${CMAKE_BINARY_DIR}/sysroot")

mediaproxy_lock_get(linux-uapi-headers url linux_url)
mediaproxy_lock_get(linux-uapi-headers sha256 linux_sha256)
mediaproxy_lock_get(musl url musl_url)
mediaproxy_lock_get(musl sha256 musl_sha256)
mediaproxy_lock_get_patch(musl 0 path musl_patch_relative)
mediaproxy_lock_get_patch(musl 0 sha256 musl_patch_sha256)
mediaproxy_lock_get(fortify-headers url fortify_headers_url)
mediaproxy_lock_get(fortify-headers sha256 fortify_headers_sha256)
mediaproxy_lock_get(fortify-headers version fortify_headers_version)
mediaproxy_lock_get(llvm-runtimes url llvm_url)
mediaproxy_lock_get(llvm-runtimes sha256 llvm_sha256)
mediaproxy_lock_get(boringssl url boringssl_url)
mediaproxy_lock_get(boringssl sha256 boringssl_sha256)
mediaproxy_lock_get(boringssl version boringssl_version)
mediaproxy_lock_get(curl url curl_url)
mediaproxy_lock_get(curl sha256 curl_sha256)
mediaproxy_lock_get(curl version curl_version)
mediaproxy_lock_get(libexpat url libexpat_url)
mediaproxy_lock_get(libexpat sha256 libexpat_sha256)
mediaproxy_lock_get(libexpat version libexpat_version)
mediaproxy_lock_get(libffi url libffi_url)
mediaproxy_lock_get(libffi sha256 libffi_sha256)
mediaproxy_lock_get(libffi version libffi_version)
mediaproxy_lock_get(glib url glib_url)
mediaproxy_lock_get(glib sha256 glib_sha256)
mediaproxy_lock_get(glib version glib_version)
mediaproxy_lock_get_patch(glib 0 path glib_patch_relative)
mediaproxy_lock_get_patch(glib 0 sha256 glib_patch_sha256)
mediaproxy_lock_get(pcre2 url pcre2_url)
mediaproxy_lock_get(pcre2 sha256 pcre2_sha256)
mediaproxy_lock_get(pcre2 version pcre2_version)
mediaproxy_lock_get(nghttp2 url nghttp2_url)
mediaproxy_lock_get(nghttp2 sha256 nghttp2_sha256)
mediaproxy_lock_get(nghttp2 version nghttp2_version)
mediaproxy_lock_get(libpng url libpng_url)
mediaproxy_lock_get(libpng sha256 libpng_sha256)
mediaproxy_lock_get(libpng version libpng_version)
mediaproxy_lock_get(libjpeg-turbo url libjpeg_turbo_url)
mediaproxy_lock_get(libjpeg-turbo sha256 libjpeg_turbo_sha256)
mediaproxy_lock_get(libjpeg-turbo version libjpeg_turbo_version)
mediaproxy_lock_get(libwebp url libwebp_url)
mediaproxy_lock_get(libwebp sha256 libwebp_sha256)
mediaproxy_lock_get(libwebp version libwebp_version)
mediaproxy_lock_get(libnsgif url libnsgif_url)
mediaproxy_lock_get(libnsgif sha256 libnsgif_sha256)
mediaproxy_lock_get(libnsgif version libnsgif_version)
mediaproxy_lock_get(libexif url libexif_url)
mediaproxy_lock_get(libexif sha256 libexif_sha256)
mediaproxy_lock_get(libexif version libexif_version)
mediaproxy_lock_get(lcms2 url lcms2_url)
mediaproxy_lock_get(lcms2 sha256 lcms2_sha256)
mediaproxy_lock_get(lcms2 version lcms2_version)
mediaproxy_lock_get(zlib url zlib_url)
mediaproxy_lock_get(zlib sha256 zlib_sha256)
mediaproxy_lock_get(zlib version zlib_version)
mediaproxy_lock_get(yyjson url yyjson_url)
mediaproxy_lock_get(yyjson sha256 yyjson_sha256)
mediaproxy_lock_get(yyjson version yyjson_version)
mediaproxy_lock_get(googletest url googletest_url)
mediaproxy_lock_get(googletest sha256 googletest_sha256)

set(musl_patch "${CMAKE_SOURCE_DIR}/${musl_patch_relative}")
file(SHA256 "${musl_patch}" actual_musl_patch_sha256)
if(NOT actual_musl_patch_sha256 STREQUAL musl_patch_sha256)
    message(FATAL_ERROR "musl security patch does not match dependencies.lock.json")
endif()

set(glib_patch "${CMAKE_SOURCE_DIR}/${glib_patch_relative}")
file(SHA256 "${glib_patch}" actual_glib_patch_sha256)
if(NOT actual_glib_patch_sha256 STREQUAL glib_patch_sha256)
    message(FATAL_ERROR "GLib hardening patch does not match dependencies.lock.json")
endif()

ExternalProject_Add(linux_headers
    URL "${linux_url}"
    URL_HASH "SHA256=${linux_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND
        "${host_make}" -C <SOURCE_DIR>
        "ARCH=${kernel_arch}"
        "INSTALL_HDR_PATH=${sysroot}/usr"
        headers_install
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE TRUE
    BUILD_BYPRODUCTS "${sysroot}/usr/include/linux/futex.h"
)

set(compiler_rt_library
    "${sysroot}/usr/lib/linux/libclang_rt.builtins-${compiler_rt_arch}.a")
set(compiler_rt_crtbegin
    "${sysroot}/usr/lib/linux/clang_rt.crtbegin-${compiler_rt_arch}.o")
set(compiler_rt_crtend
    "${sysroot}/usr/lib/linux/clang_rt.crtend-${compiler_rt_arch}.o")

ExternalProject_Add(musl
    DEPENDS linux_headers
    URL "${musl_url}"
    URL_HASH "SHA256=${musl_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    PATCH_COMMAND "${host_git}" apply "${musl_patch}"
    CONFIGURE_COMMAND
        "${CMAKE_COMMAND}" -E env
        "CC=${host_clang} --target=${target_triple}"
        "AR=${host_ar}"
        "RANLIB=${host_ranlib}"
        "LIBCC=${compiler_rt_library}"
        "CFLAGS=-O2 -fPIC"
        "LDFLAGS=-fuse-ld=lld -nostdlib"
        <SOURCE_DIR>/configure
        --prefix=/usr
        --syslibdir=/lib
        --disable-shared
        "--target=${target_triple}"
    BUILD_COMMAND "${host_make}" -j2
    INSTALL_COMMAND "${host_make}" "DESTDIR=${sysroot}" install
    BUILD_BYPRODUCTS
        "${sysroot}/usr/lib/libc.a"
        "${sysroot}/usr/lib/rcrt1.o"
)

set(fortify_headers_include_dir "${sysroot}/usr/include/fortify")
ExternalProject_Add(fortify_headers
    DEPENDS musl
    URL "${fortify_headers_url}"
    URL_HASH "SHA256=${fortify_headers_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "fortify-headers-${fortify_headers_version}.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        "${host_make}" -C <SOURCE_DIR>
        "DESTDIR=${sysroot}"
        "PREFIX=/usr"
        install
    BUILD_IN_SOURCE TRUE
    BUILD_BYPRODUCTS
        "${fortify_headers_include_dir}/fortify-headers.h"
        "${fortify_headers_include_dir}/string.h"
)

set(dependency_hardening_base_flags
    -O2
    -fPIC
    -fstack-protector-strong
    -ftrivial-auto-var-init=zero
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
    -flto=thin
    -fsanitize=cfi
    -fsanitize-trap=cfi
    -fno-sanitize-recover=cfi
    ${dependency_arch_hardening_flags}
)
set(dependency_hardening_c_flags_list
    "-D_FORTIFY_SOURCE=3"
    "-isystem ${fortify_headers_include_dir}"
    ${dependency_hardening_base_flags}
)
set(dependency_hardening_cxx_flags_list
    -nostdinc++
    "-isystem ${sysroot}/usr/include/c++/v1"
    "-isystem ${fortify_headers_include_dir}"
    "-D_FORTIFY_SOURCE=3"
    "-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST"
    -fvisibility-inlines-hidden
    ${dependency_hardening_base_flags}
)
string(JOIN " " dependency_hardening_c_flags
    ${dependency_hardening_c_flags_list})
string(JOIN " " dependency_hardening_cxx_flags
    ${dependency_hardening_cxx_flags_list})
string(JOIN " " dependency_hardening_asm_flags
    -fPIC ${dependency_arch_hardening_flags})

if(MEDIAPROXY_TARGET_ARCH STREQUAL "x86_64")
    set(ARCH_HARDENING_FLAGS
        "'-fstack-clash-protection', '-fcf-protection=full'")
else()
    set(ARCH_HARDENING_FLAGS "'-mbranch-protection=standard'")
endif()
set(HOST_CLANG "${host_clang}")
set(HOST_AR "${host_ar}")
set(HOST_STRIP "${host_strip}")
set(HOST_PKGCONF "${host_pkgconf}")
set(TARGET_TRIPLE "${target_triple}")
set(TARGET_PROCESSOR "${target_processor}")
set(SYSROOT "${sysroot}")
set(FORTIFY_INCLUDE_DIR "${fortify_headers_include_dir}")
set(COMPILER_RT_LIBRARY "${compiler_rt_library}")
set(COMPILER_RT_CRTBEGIN "${compiler_rt_crtbegin}")
set(COMPILER_RT_CRTEND "${compiler_rt_crtend}")
set(glib_cross_template
    "${CMAKE_SOURCE_DIR}/cmake/dependencies/glib/meson-cross.ini.in")
file(SHA256 "${glib_cross_template}" glib_cross_template_sha256)
set(glib_cross_file "${CMAKE_BINARY_DIR}/glib-cross.ini")
configure_file(
    "${glib_cross_template}"
    "${glib_cross_file}"
    @ONLY
)

set(nghttp2_binary_directory "${CMAKE_BINARY_DIR}/nghttp2-build")
set(nghttp2_library "${sysroot}/usr/lib/libnghttp2.a")
set(nghttp2_include_dir "${sysroot}/usr/include")
ExternalProject_Add(nghttp2
    DEPENDS fortify_headers
    URL "${nghttp2_url}"
    URL_HASH "SHA256=${nghttp2_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "nghttp2-${nghttp2_version}.tar.xz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${nghttp2_binary_directory}"
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_SYSTEM_NAME=Linux"
        "-DCMAKE_SYSTEM_PROCESSOR=${target_processor}"
        "-DCMAKE_INSTALL_PREFIX=/usr"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=${dependency_hardening_c_flags} -Werror"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DCMAKE_DISABLE_FIND_PACKAGE_Jansson=TRUE"
        "-DCMAKE_DISABLE_FIND_PACKAGE_Jemalloc=TRUE"
        "-DCMAKE_DISABLE_FIND_PACKAGE_Libevent=TRUE"
        "-DCMAKE_DISABLE_FIND_PACKAGE_Libnghttp3=TRUE"
        "-DCMAKE_DISABLE_FIND_PACKAGE_Libngtcp2=TRUE"
        "-DCMAKE_DISABLE_FIND_PACKAGE_LibXml2=TRUE"
        "-DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=TRUE"
        "-DCMAKE_DISABLE_FIND_PACKAGE_Systemd=TRUE"
        "-DSIZEOF_SSIZE_T=8"
        "-DENABLE_LIB_ONLY=ON"
        "-DBUILD_SHARED_LIBS=OFF"
        "-DBUILD_STATIC_LIBS=ON"
        "-DBUILD_TESTING=OFF"
        "-DENABLE_WERROR=ON"
        "-DENABLE_THREADS=OFF"
        "-DENABLE_APP=OFF"
        "-DENABLE_DOC=OFF"
        "-DENABLE_EXAMPLES=OFF"
        "-DENABLE_FAILMALLOC=OFF"
        "-DENABLE_HPACK_TOOLS=OFF"
        "-DENABLE_HTTP3=OFF"
        "-DWITH_LIBBPF=OFF"
        "-DWITH_LIBXML2=OFF"
        "-DWITH_JEMALLOC=OFF"
        "-DWITH_MRUBY=OFF"
        "-DWITH_NEVERBLEED=OFF"
        "-DWITH_WOLFSSL=OFF"
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR>
        --parallel 2 --target nghttp2_static
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "${nghttp2_include_dir}/nghttp2" "${sysroot}/usr/lib"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/lib/libnghttp2.a "${nghttp2_library}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <SOURCE_DIR>/lib/includes/nghttp2/nghttp2.h
        "${nghttp2_include_dir}/nghttp2/nghttp2.h"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/lib/includes/nghttp2/nghttp2ver.h
        "${nghttp2_include_dir}/nghttp2/nghttp2ver.h"
    BUILD_BYPRODUCTS
        "${nghttp2_library}"
        "${nghttp2_include_dir}/nghttp2/nghttp2.h"
        "${nghttp2_include_dir}/nghttp2/nghttp2ver.h"
)

set(zlib_binary_directory "${CMAKE_BINARY_DIR}/zlib-build")
set(zlib_library "${sysroot}/usr/lib/libz.a")
set(zlib_include_dir "${sysroot}/usr/include")
set(zlib_vernum 0x1320)
ExternalProject_Add(zlib
    DEPENDS fortify_headers
    URL "${zlib_url}"
    URL_HASH "SHA256=${zlib_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "zlib-${zlib_version}.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${zlib_binary_directory}"
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_SYSTEM_NAME=Linux"
        "-DCMAKE_SYSTEM_PROCESSOR=${target_processor}"
        "-DCMAKE_INSTALL_PREFIX=/usr"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=${dependency_hardening_c_flags} -Wall -Wextra -Werror"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DZLIB_BUILD_SHARED=OFF"
        "-DZLIB_BUILD_STATIC=ON"
        "-DZLIB_BUILD_TESTING=OFF"
        "-DZLIB_INSTALL=OFF"
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "${zlib_include_dir}" "${sysroot}/usr/lib"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/libz.a "${zlib_library}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <SOURCE_DIR>/zlib.h "${zlib_include_dir}/zlib.h"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/zconf.h "${zlib_include_dir}/zconf.h"
    BUILD_BYPRODUCTS
        "${zlib_library}"
        "${zlib_include_dir}/zlib.h"
        "${zlib_include_dir}/zconf.h"
)

set(libpng_binary_directory "${CMAKE_BINARY_DIR}/libpng-static-build")
set(libpng_library "${sysroot}/usr/lib/libpng16.a")
set(libpng_include_dir "${sysroot}/usr/include")
file(SHA256
    "${CMAKE_SOURCE_DIR}/cmake/dependencies/libpng/CMakeLists.txt"
    libpng_build_definition_sha256)
ExternalProject_Add(libpng
    DEPENDS fortify_headers zlib
    URL "${libpng_url}"
    URL_HASH "SHA256=${libpng_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "libpng-${libpng_version}.tar.xz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${libpng_binary_directory}"
    CONFIGURE_COMMAND
        "${CMAKE_COMMAND}"
        -S "${CMAKE_SOURCE_DIR}/cmake/dependencies/libpng"
        -B <BINARY_DIR>
        -G Ninja
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_SYSTEM_NAME=Linux"
        "-DCMAKE_SYSTEM_PROCESSOR=${target_processor}"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=--target=${target_triple} ${dependency_hardening_c_flags} -Wall -Wextra -Werror"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DMEDIAPROXY_LIBPNG_SOURCE_DIR=<SOURCE_DIR>"
        "-DMEDIAPROXY_BUILD_DEFINITION_SHA256=${libpng_build_definition_sha256}"
        "-DMEDIAPROXY_TARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
        "-DMEDIAPROXY_ZLIB_INCLUDE_DIR=${zlib_include_dir}"
        "-DMEDIAPROXY_ZLIB_VERNUM=${zlib_vernum}"
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR>
        --parallel 2 --target png_static
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "${libpng_include_dir}" "${sysroot}/usr/lib"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <SOURCE_DIR>/png.h "${libpng_include_dir}/png.h"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <SOURCE_DIR>/pngconf.h "${libpng_include_dir}/pngconf.h"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/pnglibconf.h "${libpng_include_dir}/pnglibconf.h"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/libpng16.a "${libpng_library}"
    BUILD_BYPRODUCTS
        "${libpng_library}"
        "${libpng_include_dir}/png.h"
        "${libpng_include_dir}/pngconf.h"
        "${libpng_include_dir}/pnglibconf.h"
)

set(libjpeg_turbo_binary_directory
    "${CMAKE_BINARY_DIR}/libjpeg-turbo-static-build")
set(libjpeg_turbo_library "${sysroot}/usr/lib/libjpeg.a")
set(libjpeg_turbo_include_dir "${sysroot}/usr/include")
ExternalProject_Add(libjpeg_turbo
    DEPENDS fortify_headers
    URL "${libjpeg_turbo_url}"
    URL_HASH "SHA256=${libjpeg_turbo_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "libjpeg-turbo-${libjpeg_turbo_version}.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${libjpeg_turbo_binary_directory}"
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_SYSTEM_NAME=Linux"
        "-DCMAKE_SYSTEM_PROCESSOR=${target_processor}"
        "-DCMAKE_INSTALL_PREFIX=/usr"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_SIZEOF_VOID_P=8"
        "-DSIZE_T=8"
        "-DUNSIGNED_LONG=8"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=${dependency_hardening_c_flags} -Wall -Wextra -Werror -Wno-unused-parameter -ffp-contract=off"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DENABLE_SHARED=OFF"
        "-DENABLE_STATIC=ON"
        "-DREQUIRE_SIMD=OFF"
        "-DWITH_SIMD=OFF"
        "-DWITH_ARITH_DEC=ON"
        "-DWITH_ARITH_ENC=OFF"
        "-DWITH_JPEG7=OFF"
        "-DWITH_JPEG8=OFF"
        "-DWITH_TURBOJPEG=OFF"
        "-DWITH_JAVA=OFF"
        "-DWITH_TOOLS=OFF"
        "-DWITH_TESTS=OFF"
        "-DWITH_FUZZ=OFF"
        "-DFORCE_INLINE=ON"
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR>
        --parallel 2 --target jpeg-static
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "${libjpeg_turbo_include_dir}" "${sysroot}/usr/lib"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/libjpeg.a "${libjpeg_turbo_library}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/jconfig.h
        <SOURCE_DIR>/src/jerror.h
        <SOURCE_DIR>/src/jmorecfg.h
        <SOURCE_DIR>/src/jpeglib.h
        "${libjpeg_turbo_include_dir}"
    BUILD_BYPRODUCTS
        "${libjpeg_turbo_library}"
        "${libjpeg_turbo_include_dir}/jconfig.h"
        "${libjpeg_turbo_include_dir}/jerror.h"
        "${libjpeg_turbo_include_dir}/jmorecfg.h"
        "${libjpeg_turbo_include_dir}/jpeglib.h"
)

set(libwebp_binary_directory "${CMAKE_BINARY_DIR}/libwebp-static-build")
set(libwebp_sharpyuv_library "${sysroot}/usr/lib/libsharpyuv.a")
set(libwebp_library "${sysroot}/usr/lib/libwebp.a")
set(libwebp_demux_library "${sysroot}/usr/lib/libwebpdemux.a")
set(libwebp_mux_library "${sysroot}/usr/lib/libwebpmux.a")
set(libwebp_include_dir "${sysroot}/usr/include")
ExternalProject_Add(libwebp
    DEPENDS fortify_headers
    URL "${libwebp_url}"
    URL_HASH "SHA256=${libwebp_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "libwebp-${libwebp_version}.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${libwebp_binary_directory}"
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_SYSTEM_NAME=Linux"
        "-DCMAKE_SYSTEM_PROCESSOR=${target_processor}"
        "-DCMAKE_INSTALL_PREFIX=/usr"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=${dependency_hardening_c_flags} -Wall -Wextra -Werror"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DCMAKE_DISABLE_FIND_PACKAGE_OpenGL=TRUE"
        "-DBUILD_SHARED_LIBS=OFF"
        "-DWEBP_LINK_STATIC=ON"
        "-DWEBP_ENABLE_SIMD=ON"
        "-DWEBP_USE_THREAD=ON"
        "-DWEBP_NEAR_LOSSLESS=OFF"
        "-DWEBP_ENABLE_SWAP_16BIT_CSP=OFF"
        "-DWEBP_BITTRACE=0"
        "-DWEBP_ENABLE_WUNUSED_RESULT=OFF"
        "-DWEBP_BUILD_LIBWEBPMUX=ON"
        "-DWEBP_BUILD_ANIM_UTILS=OFF"
        "-DWEBP_BUILD_CWEBP=OFF"
        "-DWEBP_BUILD_DWEBP=OFF"
        "-DWEBP_BUILD_GIF2WEBP=OFF"
        "-DWEBP_BUILD_IMG2WEBP=OFF"
        "-DWEBP_BUILD_VWEBP=OFF"
        "-DWEBP_BUILD_WEBPINFO=OFF"
        "-DWEBP_BUILD_WEBPMUX=OFF"
        "-DWEBP_BUILD_EXTRAS=OFF"
        "-DWEBP_BUILD_WEBP_JS=OFF"
        "-DWEBP_BUILD_FUZZTEST=OFF"
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR>
        --parallel 2 --target sharpyuv webp webpdemux libwebpmux
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "${libwebp_include_dir}/webp/sharpyuv" "${sysroot}/usr/lib"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <SOURCE_DIR>/src/webp/decode.h
        <SOURCE_DIR>/src/webp/encode.h
        <SOURCE_DIR>/src/webp/types.h
        <SOURCE_DIR>/src/webp/demux.h
        <SOURCE_DIR>/src/webp/mux.h
        <SOURCE_DIR>/src/webp/mux_types.h
        "${libwebp_include_dir}/webp"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <SOURCE_DIR>/sharpyuv/sharpyuv.h
        <SOURCE_DIR>/sharpyuv/sharpyuv_csp.h
        "${libwebp_include_dir}/webp/sharpyuv"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/libsharpyuv.a "${libwebp_sharpyuv_library}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/libwebp.a "${libwebp_library}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/libwebpdemux.a "${libwebp_demux_library}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/libwebpmux.a "${libwebp_mux_library}"
    BUILD_BYPRODUCTS
        "${libwebp_sharpyuv_library}"
        "${libwebp_library}"
        "${libwebp_demux_library}"
        "${libwebp_mux_library}"
        "${libwebp_include_dir}/webp/decode.h"
        "${libwebp_include_dir}/webp/encode.h"
        "${libwebp_include_dir}/webp/demux.h"
        "${libwebp_include_dir}/webp/mux.h"
        "${libwebp_include_dir}/webp/sharpyuv/sharpyuv.h"
)

set(libnsgif_binary_directory "${CMAKE_BINARY_DIR}/libnsgif-build")
set(libnsgif_library "${sysroot}/usr/lib/libnsgif.a")
set(libnsgif_include_dir "${sysroot}/usr/include")
ExternalProject_Add(libnsgif
    DEPENDS fortify_headers
    URL "${libnsgif_url}"
    URL_HASH "SHA256=${libnsgif_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "libnsgif-${libnsgif_version}-src.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${libnsgif_binary_directory}"
    CONFIGURE_COMMAND
        "${CMAKE_COMMAND}"
        -S "${CMAKE_SOURCE_DIR}/cmake/dependencies/libnsgif"
        -B <BINARY_DIR>
        -G Ninja
        "-DNSGIF_SOURCE_DIR=<SOURCE_DIR>"
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_SYSTEM_NAME=Linux"
        "-DCMAKE_SYSTEM_PROCESSOR=${target_processor}"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=${dependency_hardening_c_flags}"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR>
        --parallel 2 --target nsgif
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "${libnsgif_include_dir}" "${sysroot}/usr/lib"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/libnsgif.a "${libnsgif_library}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <SOURCE_DIR>/include/nsgif.h "${libnsgif_include_dir}/nsgif.h"
    BUILD_BYPRODUCTS
        "${libnsgif_library}"
        "${libnsgif_include_dir}/nsgif.h"
)

set(libexif_binary_directory "${CMAKE_BINARY_DIR}/libexif-static-build")
set(libexif_library "${sysroot}/usr/lib/libexif.a")
set(libexif_include_dir "${sysroot}/usr/include")
set(libexif_config_header "${libexif_binary_directory}/config.h")
set(libexif_pkgconfig "${sysroot}/usr/lib/pkgconfig/libexif.pc")
file(SHA256
    "${CMAKE_SOURCE_DIR}/cmake/dependencies/libexif/CMakeLists.txt"
    libexif_cmake_sha256)
file(SHA256
    "${CMAKE_SOURCE_DIR}/cmake/dependencies/libexif/config.h.in"
    libexif_config_sha256)
string(SHA256 libexif_build_definition_sha256
    "${libexif_cmake_sha256};${libexif_config_sha256}")
ExternalProject_Add(libexif
    DEPENDS fortify_headers
    URL "${libexif_url}"
    URL_HASH "SHA256=${libexif_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "libexif-${libexif_version}.tar.xz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${libexif_binary_directory}"
    CONFIGURE_COMMAND
        "${CMAKE_COMMAND}"
        -S "${CMAKE_SOURCE_DIR}/cmake/dependencies/libexif"
        -B <BINARY_DIR>
        -G Ninja
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_SYSTEM_NAME=Linux"
        "-DCMAKE_SYSTEM_PROCESSOR=${target_processor}"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=${dependency_hardening_c_flags} -D_POSIX_C_SOURCE=200809L -ffp-contract=off"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DMEDIAPROXY_BUILD_DEFINITION_SHA256=${libexif_build_definition_sha256}"
        "-DMEDIAPROXY_LIBEXIF_SOURCE_DIR=<SOURCE_DIR>"
        "-DMEDIAPROXY_LIBEXIF_VERSION=${libexif_version}"
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR>
        --parallel 2 --target exif
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" --install <BINARY_DIR>
        --prefix "${sysroot}/usr"
    BUILD_BYPRODUCTS
        "${libexif_library}"
        "${libexif_include_dir}/libexif/exif-data.h"
        "${libexif_pkgconfig}"
)

set(lcms2_binary_directory "${CMAKE_BINARY_DIR}/lcms2-build")
set(lcms2_library "${sysroot}/usr/lib/liblcms2.a")
set(lcms2_include_dir "${sysroot}/usr/include")
ExternalProject_Add(lcms2
    DEPENDS fortify_headers
    URL "${lcms2_url}"
    URL_HASH "SHA256=${lcms2_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "lcms2-${lcms2_version}.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${lcms2_binary_directory}"
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_SYSTEM_NAME=Linux"
        "-DCMAKE_SYSTEM_PROCESSOR=${target_processor}"
        "-DCMAKE_INSTALL_PREFIX=/usr"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_PROJECT_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/LittleEndian64Cross.cmake"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=${dependency_hardening_c_flags} -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Wpedantic -std=c99 -ffp-contract=off"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DTHREADS_PREFER_PTHREAD_FLAG=ON"
        "-DLCMS2_BUILD_SHARED=OFF"
        "-DLCMS2_BUILD_STATIC=ON"
        "-DLCMS2_BUILD_TOOLS=OFF"
        "-DLCMS2_BUILD_TESTS=OFF"
        "-DLCMS2_WITH_JPEG=OFF"
        "-DLCMS2_WITH_TIFF=OFF"
        "-DLCMS2_WITH_ZLIB=OFF"
        "-DLCMS2_WITH_THREADS=ON"
        "-DLCMS2_WITH_FASTFLOAT=OFF"
        "-DLCMS2_WITH_THREADED_PLUGIN=OFF"
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR>
        --parallel 2 --target lcms2
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "${lcms2_include_dir}" "${sysroot}/usr/lib"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/liblcms2.a "${lcms2_library}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <SOURCE_DIR>/include/lcms2.h "${lcms2_include_dir}/lcms2.h"
    BUILD_BYPRODUCTS
        "${lcms2_library}"
        "${lcms2_include_dir}/lcms2.h"
)

set(libexpat_binary_directory "${CMAKE_BINARY_DIR}/libexpat-static-build")
set(libexpat_library "${sysroot}/usr/lib/libexpat.a")
set(libexpat_include_dir "${sysroot}/usr/include")
set(libexpat_config_header
    "${libexpat_binary_directory}/expat_config.h")
set(libexpat_pkgconfig "${sysroot}/usr/lib/pkgconfig/expat.pc")
ExternalProject_Add(libexpat
    DEPENDS fortify_headers
    URL "${libexpat_url}"
    URL_HASH "SHA256=${libexpat_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "expat-${libexpat_version}.tar.xz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${libexpat_binary_directory}"
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_SYSTEM_NAME=Linux"
        "-DCMAKE_SYSTEM_PROCESSOR=${target_processor}"
        "-DCMAKE_INSTALL_PREFIX=/usr"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_PROJECT_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/LittleEndian64Cross.cmake"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=${dependency_hardening_c_flags} -Wall -Wextra -Werror -Wpedantic"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DEXPAT_SHARED_LIBS=OFF"
        "-DEXPAT_BUILD_TOOLS=OFF"
        "-DEXPAT_BUILD_EXAMPLES=OFF"
        "-DEXPAT_BUILD_TESTS=OFF"
        "-DEXPAT_BUILD_DOCS=OFF"
        "-DEXPAT_BUILD_FUZZERS=OFF"
        "-DEXPAT_OSSFUZZ_BUILD=OFF"
        "-DEXPAT_BUILD_PKGCONFIG=ON"
        "-DEXPAT_ENABLE_INSTALL=OFF"
        "-DEXPAT_WARNINGS_AS_ERRORS=ON"
        "-DEXPAT_CONTEXT_BYTES=1024"
        "-DEXPAT_DTD=ON"
        "-DEXPAT_GE=ON"
        "-DEXPAT_NS=ON"
        "-DEXPAT_ATTR_INFO=OFF"
        "-DEXPAT_LARGE_SIZE=OFF"
        "-DEXPAT_MIN_SIZE=OFF"
        "-DEXPAT_CHAR_TYPE=char"
        "-DEXPAT_DEV_URANDOM=OFF"
        "-DEXPAT_WITH_ARC4RANDOM=OFF"
        "-DEXPAT_WITH_ARC4RANDOM_BUF=OFF"
        "-DEXPAT_WITH_GETENTROPY=OFF"
        "-DEXPAT_WITH_GETRANDOM=ON"
        "-DEXPAT_WITH_SYS_GETRANDOM=OFF"
        "-DEXPAT_SYMBOL_VERSIONING=OFF"
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR>
        --parallel 2 --target expat
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "${libexpat_include_dir}" "${sysroot}/usr/lib/pkgconfig"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/libexpat.a "${libexpat_library}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <SOURCE_DIR>/lib/expat.h "${libexpat_include_dir}/expat.h"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <SOURCE_DIR>/lib/expat_external.h
        "${libexpat_include_dir}/expat_external.h"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/expat_config.h
        "${libexpat_include_dir}/expat_config.h"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/Release/expat.pc "${libexpat_pkgconfig}"
    BUILD_BYPRODUCTS
        "${libexpat_library}"
        "${libexpat_include_dir}/expat.h"
        "${libexpat_include_dir}/expat_external.h"
        "${libexpat_include_dir}/expat_config.h"
        "${libexpat_pkgconfig}"
)

set(libffi_binary_directory "${CMAKE_BINARY_DIR}/libffi-static-build")
set(libffi_cmake_binary_directory
    "${libffi_binary_directory}/cmake-build")
set(libffi_library "${sysroot}/usr/lib/libffi.a")
set(libffi_include_dir "${sysroot}/usr/include")
set(libffi_config_header "${libffi_binary_directory}/fficonfig.h")
set(libffi_pkgconfig "${sysroot}/usr/lib/pkgconfig/libffi.pc")
set(libffi_configure_ldflags
    "--target=${target_triple} --sysroot=${sysroot} -fuse-ld=lld -static-pie -nostdlib ${sysroot}/usr/lib/rcrt1.o ${sysroot}/usr/lib/crti.o ${sysroot}/usr/lib/linux/clang_rt.crtbegin-${compiler_rt_arch}.o")
set(libffi_configure_libs
    "-Wl,--start-group -lc ${sysroot}/usr/lib/linux/libclang_rt.builtins-${compiler_rt_arch}.a -Wl,--end-group ${sysroot}/usr/lib/linux/clang_rt.crtend-${compiler_rt_arch}.o ${sysroot}/usr/lib/crtn.o")
file(SHA256
    "${CMAKE_SOURCE_DIR}/cmake/dependencies/libffi/CMakeLists.txt"
    libffi_build_definition_sha256)
ExternalProject_Add(libffi
    DEPENDS fortify_headers
    URL "${libffi_url}"
    URL_HASH "SHA256=${libffi_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "libffi-${libffi_version}.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${libffi_binary_directory}"
    CONFIGURE_COMMAND
        "${CMAKE_COMMAND}" -E env
        "CC=${host_clang} --target=${target_triple} --sysroot=${sysroot}"
        "CXX=${host_clangxx} --target=${target_triple} --sysroot=${sysroot}"
        "CCAS=${host_clang} --target=${target_triple} --sysroot=${sysroot}"
        "AR=${host_ar}"
        "RANLIB=${host_ranlib}"
        "NM=${host_nm}"
        "STRIP=${host_strip}"
        "ac_cv_func_memcpy=yes"
        "CFLAGS=${dependency_hardening_c_flags} -Wall -Wextra -Werror"
        "CCASFLAGS=${dependency_hardening_asm_flags}"
        "LDFLAGS=${libffi_configure_ldflags}"
        "LIBS=${libffi_configure_libs}"
        <SOURCE_DIR>/configure
        --build=x86_64-pc-linux-gnu
        "--host=${target_triple}"
        --prefix=/usr
        --libdir=/usr/lib
        --includedir=/usr/include
        --disable-shared
        --enable-static
        --with-pic
        --disable-docs
        --disable-dependency-tracking
        --disable-multi-os-directory
        --disable-raw-api
        --enable-exec-static-tramp
        --disable-symvers
        COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_SOURCE_DIR}/cmake/dependencies/libffi"
        -B "${libffi_cmake_binary_directory}"
        -G Ninja
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_SYSTEM_NAME=Linux"
        "-DCMAKE_SYSTEM_PROCESSOR=${target_processor}"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_ASM_COMPILER=${host_clang}"
        "-DCMAKE_ASM_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=${dependency_hardening_c_flags}"
        "-DCMAKE_ASM_FLAGS=${dependency_hardening_asm_flags}"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DMEDIAPROXY_BUILD_DEFINITION_SHA256=${libffi_build_definition_sha256}"
        "-DMEDIAPROXY_LIBFFI_CONFIG_DIR=${libffi_binary_directory}"
        "-DMEDIAPROXY_LIBFFI_SOURCE_DIR=<SOURCE_DIR>"
        "-DMEDIAPROXY_LIBFFI_TARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
        "-DMEDIAPROXY_LIBFFI_VERSION=${libffi_version}"
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build "${libffi_cmake_binary_directory}"
        --parallel 2 --target ffi
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" --install "${libffi_cmake_binary_directory}"
        --prefix "${sysroot}/usr"
    BUILD_BYPRODUCTS
        "${libffi_library}"
        "${libffi_include_dir}/ffi.h"
        "${libffi_include_dir}/ffitarget.h"
        "${libffi_pkgconfig}"
)

set(pcre2_binary_directory "${CMAKE_BINARY_DIR}/pcre2-static-build")
set(pcre2_library "${sysroot}/usr/lib/libpcre2-8.a")
set(pcre2_include_dir "${sysroot}/usr/include")
set(pcre2_config_header "${pcre2_binary_directory}/src/config.h")
set(pcre2_pkgconfig "${sysroot}/usr/lib/pkgconfig/libpcre2-8.pc")
ExternalProject_Add(pcre2
    DEPENDS fortify_headers
    URL "${pcre2_url}"
    URL_HASH "SHA256=${pcre2_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "pcre2-${pcre2_version}.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${pcre2_binary_directory}"
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_SYSTEM_NAME=Linux"
        "-DCMAKE_SYSTEM_PROCESSOR=${target_processor}"
        "-DCMAKE_INSTALL_PREFIX=/usr"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_PROJECT_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/LittleEndian64Cross.cmake"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=${dependency_hardening_c_flags} -Wall -Wextra -Werror -Wpedantic -Wno-overlength-strings"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DBUILD_SHARED_LIBS=OFF"
        "-DBUILD_STATIC_LIBS=ON"
        "-DPCRE2_BUILD_PCRE2_8=ON"
        "-DPCRE2_BUILD_PCRE2_16=OFF"
        "-DPCRE2_BUILD_PCRE2_32=OFF"
        "-DPCRE2_STATIC_PIC=ON"
        "-DPCRE2_BUILD_PCRE2GREP=OFF"
        "-DPCRE2_BUILD_TESTS=OFF"
        "-DPCRE2_SUPPORT_JIT=OFF"
        "-DPCRE2_SUPPORT_JIT_SEALLOC=OFF"
        "-DPCRE2GREP_SUPPORT_JIT=OFF"
        "-DPCRE2GREP_SUPPORT_CALLOUT=OFF"
        "-DPCRE2GREP_SUPPORT_CALLOUT_FORK=OFF"
        "-DPCRE2_SUPPORT_LIBZ=OFF"
        "-DPCRE2_SUPPORT_UNICODE=ON"
        "-DPCRE2_SUPPORT_VALGRIND=OFF"
        "-DPCRE2_REBUILD_CHARTABLES=OFF"
        "-DPCRE2_SHOW_REPORT=ON"
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR>
        --parallel 2 --target pcre2-8-static
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "${pcre2_include_dir}" "${sysroot}/usr/lib/pkgconfig"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/libpcre2-8.a "${pcre2_library}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/interface/pcre2.h "${pcre2_include_dir}/pcre2.h"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/libpcre2-8.pc "${pcre2_pkgconfig}"
    BUILD_BYPRODUCTS
        "${pcre2_library}"
        "${pcre2_include_dir}/pcre2.h"
        "${pcre2_pkgconfig}"
)

set(yyjson_binary_directory "${CMAKE_BINARY_DIR}/yyjson-build")
set(yyjson_library "${sysroot}/usr/lib/libyyjson.a")
set(yyjson_include_dir "${sysroot}/usr/include")
ExternalProject_Add(yyjson
    DEPENDS fortify_headers
    URL "${yyjson_url}"
    URL_HASH "SHA256=${yyjson_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "yyjson-${yyjson_version}.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${yyjson_binary_directory}"
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_SYSTEM_NAME=Linux"
        "-DCMAKE_SYSTEM_PROCESSOR=${target_processor}"
        "-DCMAKE_INSTALL_PREFIX=/usr"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=${dependency_hardening_c_flags}"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DBUILD_SHARED_LIBS=OFF"
        "-DYYJSON_BUILD_TESTS=OFF"
        "-DYYJSON_BUILD_FUZZER=OFF"
        "-DYYJSON_BUILD_MISC=OFF"
        "-DYYJSON_BUILD_DOC=OFF"
        "-DYYJSON_DISABLE_INCR_READER=ON"
        "-DYYJSON_DISABLE_UTILS=ON"
        "-DYYJSON_DISABLE_NON_STANDARD=ON"
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E env "DESTDIR=${sysroot}"
        "${CMAKE_COMMAND}" --install <BINARY_DIR>
    BUILD_BYPRODUCTS
        "${yyjson_library}"
        "${yyjson_include_dir}/yyjson.h"
)

ExternalProject_Add(llvm_source
    URL "${llvm_url}"
    URL_HASH "SHA256=${llvm_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)
ExternalProject_Get_Property(llvm_source SOURCE_DIR)
set(llvm_source_directory "${SOURCE_DIR}")

ExternalProject_Add(compiler_rt
    DEPENDS musl llvm_source
    SOURCE_DIR "${llvm_source_directory}/compiler-rt"
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_INSTALL_PREFIX=/usr"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_CXX_COMPILER=${host_clangxx}"
        "-DCMAKE_ASM_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_CXX_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_ASM_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=-fPIC"
        "-DCMAKE_CXX_FLAGS=-fPIC"
        "-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON"
        "-DCOMPILER_RT_BUILD_BUILTINS=ON"
        "-DCOMPILER_RT_BUILD_CRT=ON"
        "-DCOMPILER_RT_BUILD_SANITIZERS=OFF"
        "-DCOMPILER_RT_BUILD_XRAY=OFF"
        "-DCOMPILER_RT_BUILD_LIBFUZZER=OFF"
        "-DCOMPILER_RT_BUILD_PROFILE=OFF"
        "-DCOMPILER_RT_BUILD_ORC=OFF"
        "-DCOMPILER_RT_INCLUDE_TESTS=OFF"
        "-DLLVM_CONFIG_PATH="
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR> --target builtins crt
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E env "DESTDIR=${sysroot}"
        "${CMAKE_COMMAND}" --build <BINARY_DIR> --target install-builtins
        COMMAND
        "${CMAKE_COMMAND}" -E env "DESTDIR=${sysroot}"
        "${CMAKE_COMMAND}" --install <BINARY_DIR>
        --component "clang_rt.crtbegin-${compiler_rt_arch}"
        COMMAND
        "${CMAKE_COMMAND}" -E env "DESTDIR=${sysroot}"
        "${CMAKE_COMMAND}" --install <BINARY_DIR>
        --component "clang_rt.crtend-${compiler_rt_arch}"
    BUILD_BYPRODUCTS
        "${compiler_rt_library}"
        "${compiler_rt_crtbegin}"
        "${compiler_rt_crtend}"
)

ExternalProject_Add(llvm_runtimes
    DEPENDS compiler_rt
    SOURCE_DIR "${llvm_source_directory}/runtimes"
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_INSTALL_PREFIX=/usr"
        "-DCMAKE_C_COMPILER=${host_clang}"
        "-DCMAKE_CXX_COMPILER=${host_clangxx}"
        "-DCMAKE_ASM_COMPILER=${host_clang}"
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_CXX_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_ASM_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_SYSROOT=${sysroot}"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DCMAKE_AR=${host_ar}"
        "-DCMAKE_RANLIB=${host_ranlib}"
        "-DCMAKE_NM=${host_nm}"
        "-DCMAKE_LINKER=${host_lld}"
        "-DCMAKE_C_FLAGS=-fPIC"
        "-DCMAKE_CXX_FLAGS=-fPIC -nostdinc++"
        "-DLLVM_ENABLE_RUNTIMES=libunwind$<SEMICOLON>libcxxabi$<SEMICOLON>libcxx"
        "-DLLVM_INCLUDE_TESTS=OFF"
        "-DLIBUNWIND_ENABLE_SHARED=OFF"
        "-DLIBUNWIND_ENABLE_STATIC=ON"
        "-DLIBUNWIND_USE_COMPILER_RT=ON"
        "-DLIBUNWIND_INCLUDE_TESTS=OFF"
        "-DLIBCXXABI_ENABLE_SHARED=OFF"
        "-DLIBCXXABI_ENABLE_STATIC=ON"
        "-DLIBCXXABI_USE_COMPILER_RT=ON"
        "-DLIBCXXABI_USE_LLVM_UNWINDER=ON"
        "-DLIBCXXABI_INCLUDE_TESTS=OFF"
        "-DLIBCXX_ENABLE_SHARED=OFF"
        "-DLIBCXX_ENABLE_STATIC=ON"
        "-DLIBCXX_USE_COMPILER_RT=ON"
        "-DLIBCXX_CXX_ABI=libcxxabi"
        "-DLIBCXX_HAS_MUSL_LIBC=ON"
        "-DLIBCXX_INCLUDE_TESTS=OFF"
        "-DLIBCXX_INCLUDE_BENCHMARKS=OFF"
        "-DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=OFF"
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR> --target cxx cxxabi unwind
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E env "DESTDIR=${sysroot}"
        "${CMAKE_COMMAND}" --build <BINARY_DIR>
        --target install-cxx install-cxxabi install-unwind
    BUILD_BYPRODUCTS
        "${sysroot}/usr/lib/libc++.a"
        "${sysroot}/usr/lib/libc++abi.a"
        "${sysroot}/usr/lib/libunwind.a"
)

string(SHA256 glib_build_configuration_sha256
    "${glib_patch_sha256}:${glib_cross_template_sha256}")
string(SUBSTRING "${glib_build_configuration_sha256}" 0 12
    glib_patch_build_id)
set(glib_prefix_directory
    "${CMAKE_BINARY_DIR}/glib-${glib_version}-${glib_patch_build_id}-prefix")
set(glib_binary_directory
    "${CMAKE_BINARY_DIR}/glib-${glib_version}-${glib_patch_build_id}-static-build")
set(glib_include_dir "${sysroot}/usr/include/glib-2.0")
set(glib_config_include_dir "${sysroot}/usr/lib/glib-2.0/include")
set(glib_library "${sysroot}/usr/lib/libglib-2.0.a")
set(gobject_library "${sysroot}/usr/lib/libgobject-2.0.a")
set(gthread_library "${sysroot}/usr/lib/libgthread-2.0.a")
set(gmodule_library "${sysroot}/usr/lib/libgmodule-2.0.a")
set(gio_library "${sysroot}/usr/lib/libgio-2.0.a")
set(glib_pkgconfig "${sysroot}/usr/lib/pkgconfig/glib-2.0.pc")
set(gobject_pkgconfig "${sysroot}/usr/lib/pkgconfig/gobject-2.0.pc")
set(gthread_pkgconfig "${sysroot}/usr/lib/pkgconfig/gthread-2.0.pc")
set(gmodule_pkgconfig
    "${sysroot}/usr/lib/pkgconfig/gmodule-no-export-2.0.pc")
set(gio_pkgconfig "${sysroot}/usr/lib/pkgconfig/gio-2.0.pc")
ExternalProject_Add(glib
    DEPENDS compiler_rt fortify_headers libffi pcre2 zlib
    PREFIX "${glib_prefix_directory}"
    URL "${glib_url}"
    URL_HASH "SHA256=${glib_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "glib-${glib_version}.tar.xz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    PATCH_COMMAND
        "${CMAKE_COMMAND}" -E env
        "GIT_CEILING_DIRECTORIES=${CMAKE_BINARY_DIR}"
        "${host_git}" apply "${glib_patch}"
    BINARY_DIR "${glib_binary_directory}"
    CONFIGURE_COMMAND
        "${CMAKE_COMMAND}" -E env
        "PKG_CONFIG_LIBDIR=${sysroot}/usr/lib/pkgconfig"
        "PKG_CONFIG_SYSROOT_DIR=${sysroot}"
        "${host_meson}" setup <BINARY_DIR> <SOURCE_DIR>
        "--cross-file=${glib_cross_file}"
        --prefix=/usr
        --libdir=lib
        --buildtype=release
        --default-library=static
        --wrap-mode=nodownload
        --auto-features=disabled
        -Db_staticpic=true
        -Db_lto=false
        -Dwerror=true
        -Dtests=false
        -Dinstalled_tests=false
        -Dintrospection=disabled
        -Ddocumentation=false
        -Dman-pages=disabled
        -Dnls=disabled
        -Dselinux=disabled
        -Dlibmount=disabled
        -Dxattr=false
        -Dlibelf=disabled
        -Ddtrace=disabled
        -Dsystemtap=disabled
        -Dsysprof=disabled
        -Doss_fuzz=disabled
        -Dglib_debug=disabled
        -Dglib_assert=true
        -Dglib_checks=true
        -Dbsymbolic_functions=false
        -Dmultiarch=false
        -Dfile_monitor_backend=inotify
        -Dgio_module_dir=lib/mediaproxy-gio-modules-disabled
    BUILD_COMMAND
        "${host_ninja}" -C <BINARY_DIR>
        glib/libglib-2.0.a
        gobject/libgobject-2.0.a
        gthread/libgthread-2.0.a
        gmodule/libgmodule-2.0.a
        gio/libgio-2.0.a
    INSTALL_COMMAND
        "${host_meson}" install -C <BINARY_DIR>
        --no-rebuild
        "--destdir=${sysroot}"
        --tags=devel
    BUILD_BYPRODUCTS
        "${glib_library}"
        "${gobject_library}"
        "${gthread_library}"
        "${gmodule_library}"
        "${gio_library}"
        "${glib_include_dir}/glib.h"
        "${glib_include_dir}/glib-object.h"
        "${glib_include_dir}/gio/gio.h"
        "${glib_config_include_dir}/glibconfig.h"
        "${glib_pkgconfig}"
        "${gobject_pkgconfig}"
        "${gthread_pkgconfig}"
        "${gmodule_pkgconfig}"
        "${gio_pkgconfig}"
)

set(boringssl_binary_directory "${CMAKE_BINARY_DIR}/boringssl-build")
set(boringssl_crypto_library "${sysroot}/usr/lib/libcrypto.a")
set(boringssl_ssl_library "${sysroot}/usr/lib/libssl.a")
set(boringssl_include_dir "${sysroot}/usr/include")
ExternalProject_Add(boringssl
    DEPENDS fortify_headers llvm_runtimes
    URL "${boringssl_url}"
    URL_HASH "SHA256=${boringssl_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "boringssl-${boringssl_version}.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${boringssl_binary_directory}"
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_INSTALL_PREFIX=/usr"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DMEDIAPROXY_TARGET_TRIPLE=${target_triple}"
        "-DMEDIAPROXY_TARGET_PROCESSOR=${target_processor}"
        "-DMEDIAPROXY_COMPILER_RT_ARCH=${compiler_rt_arch}"
        "-DMEDIAPROXY_SYSROOT=${sysroot}"
        "-DMEDIAPROXY_CLANG=${host_clang}"
        "-DMEDIAPROXY_CLANGXX=${host_clangxx}"
        "-DMEDIAPROXY_LLD=${host_lld}"
        "-DMEDIAPROXY_AR=${host_ar}"
        "-DMEDIAPROXY_RANLIB=${host_ranlib}"
        "-DMEDIAPROXY_NM=${host_nm}"
        "-DMEDIAPROXY_STRIP=${host_strip}"
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/toolchains/llvm-musl.cmake"
        "-DCMAKE_C_FLAGS=${dependency_hardening_c_flags}"
        "-DCMAKE_CXX_FLAGS=${dependency_hardening_cxx_flags}"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DBUILD_SHARED_LIBS=OFF"
        "-DBUILD_TESTING=OFF"
        "-DFIPS=OFF"
        "-DOPENSSL_NO_ASM=ON"
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR>
        --parallel 2 --target crypto ssl
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "${boringssl_include_dir}/openssl"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
        <SOURCE_DIR>/include/openssl "${boringssl_include_dir}/openssl"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/libcrypto.a "${boringssl_crypto_library}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/libssl.a "${boringssl_ssl_library}"
    BUILD_BYPRODUCTS
        "${boringssl_crypto_library}"
        "${boringssl_ssl_library}"
        "${boringssl_include_dir}/openssl/ssl.h"
)

set(curl_binary_directory "${CMAKE_BINARY_DIR}/curl-static-build")
set(curl_library "${sysroot}/usr/lib/libcurl.a")
set(curl_include_dir "${sysroot}/usr/include")
ExternalProject_Add(curl
    DEPENDS boringssl fortify_headers nghttp2 zlib
    URL "${curl_url}"
    URL_HASH "SHA256=${curl_sha256}"
    DOWNLOAD_DIR "${source_cache}"
    DOWNLOAD_NAME "curl-${curl_version}.tar.xz"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    UPDATE_DISCONNECTED TRUE
    BINARY_DIR "${curl_binary_directory}"
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_INSTALL_PREFIX=/usr"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DMEDIAPROXY_TARGET_TRIPLE=${target_triple}"
        "-DMEDIAPROXY_TARGET_PROCESSOR=${target_processor}"
        "-DMEDIAPROXY_COMPILER_RT_ARCH=${compiler_rt_arch}"
        "-DMEDIAPROXY_SYSROOT=${sysroot}"
        "-DMEDIAPROXY_CLANG=${host_clang}"
        "-DMEDIAPROXY_CLANGXX=${host_clangxx}"
        "-DMEDIAPROXY_LLD=${host_lld}"
        "-DMEDIAPROXY_AR=${host_ar}"
        "-DMEDIAPROXY_RANLIB=${host_ranlib}"
        "-DMEDIAPROXY_NM=${host_nm}"
        "-DMEDIAPROXY_STRIP=${host_strip}"
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/toolchains/llvm-musl.cmake"
        "-DCMAKE_C_FLAGS=${dependency_hardening_c_flags}"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DCMAKE_DISABLE_FIND_PACKAGE_Perl=TRUE"
        "-DCURL_USE_PKGCONFIG=OFF"
        "-DCURL_USE_CMAKECONFIG=OFF"
        "-DBUILD_SHARED_LIBS=OFF"
        "-DBUILD_STATIC_LIBS=ON"
        "-DBUILD_CURL_EXE=OFF"
        "-DBUILD_TESTING=OFF"
        "-DBUILD_EXAMPLES=OFF"
        "-DBUILD_LIBCURL_DOCS=OFF"
        "-DBUILD_MISC_DOCS=OFF"
        "-DENABLE_CURL_MANUAL=OFF"
        "-DCURL_DISABLE_INSTALL=ON"
        "-DCURL_ENABLE_EXPORT_TARGET=OFF"
        "-DCURL_WERROR=ON"
        "-DPICKY_COMPILER=ON"
        "-DHTTP_ONLY=ON"
        "-DCURL_ENABLE_SSL=ON"
        "-DCURL_USE_OPENSSL=ON"
        "-DCURL_USE_MBEDTLS=OFF"
        "-DCURL_USE_WOLFSSL=OFF"
        "-DCURL_USE_GNUTLS=OFF"
        "-DCURL_USE_RUSTLS=OFF"
        "-DCURL_DISABLE_OPENSSL_AUTO_LOAD_CONFIG=ON"
        "-DOPENSSL_INCLUDE_DIR=${boringssl_include_dir}"
        "-DOPENSSL_SSL_LIBRARY=${boringssl_ssl_library}"
        "-DOPENSSL_CRYPTO_LIBRARY=${boringssl_crypto_library}"
        "-DHAVE_BORINGSSL=ON"
        "-DCURL_ZLIB=ON"
        "-DZLIB_INCLUDE_DIR=${zlib_include_dir}"
        "-DZLIB_LIBRARY=${zlib_library}"
        "-DCURL_BROTLI=OFF"
        "-DCURL_ZSTD=OFF"
        "-DUSE_NGHTTP2=ON"
        "-DNGHTTP2_USE_STATIC_LIBS=ON"
        "-DNGHTTP2_INCLUDE_DIR=${nghttp2_include_dir}"
        "-DNGHTTP2_LIBRARY=${nghttp2_library}"
        "-DUSE_NGTCP2=OFF"
        "-DUSE_QUICHE=OFF"
        "-DUSE_HTTPSRR=OFF"
        "-DUSE_ECH=OFF"
        "-DUSE_SSLS_EXPORT=OFF"
        "-DUSE_PROXY_HTTP3=OFF"
        "-DUSE_LIBIDN2=OFF"
        "-DCURL_USE_LIBPSL=OFF"
        "-DCURL_USE_LIBSSH2=OFF"
        "-DCURL_USE_LIBSSH=OFF"
        "-DCURL_USE_GSASL=OFF"
        "-DCURL_USE_GSSAPI=OFF"
        "-DCURL_USE_LIBBACKTRACE=OFF"
        "-DENABLE_ARES=OFF"
        "-DENABLE_THREADED_RESOLVER=OFF"
        "-DENABLE_UNIX_SOCKETS=OFF"
        "-DCURL_DISABLE_ALTSVC=ON"
        "-DCURL_DISABLE_SRP=ON"
        "-DCURL_DISABLE_COOKIES=ON"
        "-DCURL_DISABLE_HTTP_AUTH=ON"
        "-DCURL_DISABLE_DOH=ON"
        "-DCURL_DISABLE_GETOPTIONS=ON"
        "-DCURL_DISABLE_HEADERS_API=ON"
        "-DCURL_DISABLE_HSTS=ON"
        "-DCURL_DISABLE_MIME=ON"
        "-DCURL_DISABLE_BINDLOCAL=ON"
        "-DCURL_DISABLE_NETRC=ON"
        "-DCURL_DISABLE_PARSEDATE=ON"
        "-DCURL_DISABLE_PROGRESS_METER=ON"
        "-DCURL_DISABLE_PROXY=ON"
        "-DCURL_DISABLE_SHUFFLE_DNS=ON"
        "-DCURL_DISABLE_SOCKETPAIR=ON"
        "-DCURL_CA_NATIVE=OFF"
        "-DCURL_CA_FALLBACK=OFF"
        "-DCURL_CA_BUNDLE=none"
        "-DCURL_CA_PATH=none"
    BUILD_COMMAND
        "${CMAKE_COMMAND}" --build <BINARY_DIR>
        --parallel 2 --target libcurl_static
    INSTALL_COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "${curl_include_dir}/curl" "${sysroot}/usr/lib"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
        <SOURCE_DIR>/include/curl "${curl_include_dir}/curl"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        <BINARY_DIR>/lib/libcurl.a "${curl_library}"
    BUILD_BYPRODUCTS
        "${curl_library}"
        "${curl_include_dir}/curl/curl.h"
)

set(application_binary_directory "${CMAKE_BINARY_DIR}/application")
ExternalProject_Add(application
    DEPENDS boringssl curl fortify_headers glib lcms2 libexif libexpat libffi libjpeg_turbo libnsgif libpng libwebp llvm_runtimes nghttp2 pcre2 yyjson zlib
    BUILD_ALWAYS TRUE
    SOURCE_DIR "${CMAKE_SOURCE_DIR}"
    BINARY_DIR "${application_binary_directory}"
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
    CMAKE_GENERATOR Ninja
    CMAKE_ARGS
        "-DMEDIAPROXY_INNER_BUILD=ON"
        "-DMEDIAPROXY_TARGET_ARCH=${MEDIAPROXY_TARGET_ARCH}"
        "-DMEDIAPROXY_TARGET_TRIPLE=${target_triple}"
        "-DMEDIAPROXY_TARGET_PROCESSOR=${target_processor}"
        "-DMEDIAPROXY_COMPILER_RT_ARCH=${compiler_rt_arch}"
        "-DMEDIAPROXY_SYSROOT=${sysroot}"
        "-DMEDIAPROXY_CLANG=${host_clang}"
        "-DMEDIAPROXY_CLANGXX=${host_clangxx}"
        "-DMEDIAPROXY_LLD=${host_lld}"
        "-DMEDIAPROXY_AR=${host_ar}"
        "-DMEDIAPROXY_RANLIB=${host_ranlib}"
        "-DMEDIAPROXY_NM=${host_nm}"
        "-DMEDIAPROXY_STRIP=${host_strip}"
        "-DMEDIAPROXY_READELF=${host_readelf}"
        "-DMEDIAPROXY_SOURCE_CACHE=${source_cache}"
        "-DMEDIAPROXY_FORTIFY_INCLUDE_DIR=${fortify_headers_include_dir}"
        "-DMEDIAPROXY_YYJSON_INCLUDE_DIR=${yyjson_include_dir}"
        "-DMEDIAPROXY_YYJSON_LIBRARY=${yyjson_library}"
        "-DMEDIAPROXY_YYJSON_COMPILE_COMMANDS=${yyjson_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_BORINGSSL_INCLUDE_DIR=${boringssl_include_dir}"
        "-DMEDIAPROXY_BORINGSSL_CRYPTO_LIBRARY=${boringssl_crypto_library}"
        "-DMEDIAPROXY_BORINGSSL_SSL_LIBRARY=${boringssl_ssl_library}"
        "-DMEDIAPROXY_BORINGSSL_COMPILE_COMMANDS=${boringssl_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_CURL_INCLUDE_DIR=${curl_include_dir}"
        "-DMEDIAPROXY_CURL_LIBRARY=${curl_library}"
        "-DMEDIAPROXY_CURL_COMPILE_COMMANDS=${curl_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_CURL_CONFIG_HEADER=${curl_binary_directory}/lib/curl_config.h"
        "-DMEDIAPROXY_NGHTTP2_INCLUDE_DIR=${nghttp2_include_dir}"
        "-DMEDIAPROXY_NGHTTP2_LIBRARY=${nghttp2_library}"
        "-DMEDIAPROXY_NGHTTP2_COMPILE_COMMANDS=${nghttp2_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_NGHTTP2_CONFIG_HEADER=${nghttp2_binary_directory}/config.h"
        "-DMEDIAPROXY_LIBPNG_INCLUDE_DIR=${libpng_include_dir}"
        "-DMEDIAPROXY_LIBPNG_LIBRARY=${libpng_library}"
        "-DMEDIAPROXY_LIBPNG_COMPILE_COMMANDS=${libpng_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_LIBPNG_CONFIG_HEADER=${libpng_binary_directory}/pnglibconf.h"
        "-DMEDIAPROXY_LIBJPEG_TURBO_INCLUDE_DIR=${libjpeg_turbo_include_dir}"
        "-DMEDIAPROXY_LIBJPEG_TURBO_LIBRARY=${libjpeg_turbo_library}"
        "-DMEDIAPROXY_LIBJPEG_TURBO_COMPILE_COMMANDS=${libjpeg_turbo_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_LIBJPEG_TURBO_CONFIG_HEADER=${libjpeg_turbo_binary_directory}/jconfig.h"
        "-DMEDIAPROXY_LIBJPEG_TURBO_INTERNAL_CONFIG_HEADER=${libjpeg_turbo_binary_directory}/jconfigint.h"
        "-DMEDIAPROXY_LIBWEBP_INCLUDE_DIR=${libwebp_include_dir}"
        "-DMEDIAPROXY_LIBWEBP_SHARPYUV_LIBRARY=${libwebp_sharpyuv_library}"
        "-DMEDIAPROXY_LIBWEBP_LIBRARY=${libwebp_library}"
        "-DMEDIAPROXY_LIBWEBP_DEMUX_LIBRARY=${libwebp_demux_library}"
        "-DMEDIAPROXY_LIBWEBP_MUX_LIBRARY=${libwebp_mux_library}"
        "-DMEDIAPROXY_LIBWEBP_COMPILE_COMMANDS=${libwebp_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_LIBWEBP_CONFIG_HEADER=${libwebp_binary_directory}/src/webp/config.h"
        "-DMEDIAPROXY_LIBNSGIF_INCLUDE_DIR=${libnsgif_include_dir}"
        "-DMEDIAPROXY_LIBNSGIF_LIBRARY=${libnsgif_library}"
        "-DMEDIAPROXY_LIBNSGIF_COMPILE_COMMANDS=${libnsgif_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_LIBEXIF_INCLUDE_DIR=${libexif_include_dir}"
        "-DMEDIAPROXY_LIBEXIF_LIBRARY=${libexif_library}"
        "-DMEDIAPROXY_LIBEXIF_COMPILE_COMMANDS=${libexif_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_LIBEXIF_CONFIG_HEADER=${libexif_config_header}"
        "-DMEDIAPROXY_LIBEXIF_PKGCONFIG=${libexif_pkgconfig}"
        "-DMEDIAPROXY_LIBEXPAT_INCLUDE_DIR=${libexpat_include_dir}"
        "-DMEDIAPROXY_LIBEXPAT_LIBRARY=${libexpat_library}"
        "-DMEDIAPROXY_LIBEXPAT_COMPILE_COMMANDS=${libexpat_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_LIBEXPAT_CONFIG_HEADER=${libexpat_config_header}"
        "-DMEDIAPROXY_LIBEXPAT_PKGCONFIG=${libexpat_pkgconfig}"
        "-DMEDIAPROXY_LIBFFI_INCLUDE_DIR=${libffi_include_dir}"
        "-DMEDIAPROXY_LIBFFI_LIBRARY=${libffi_library}"
        "-DMEDIAPROXY_LIBFFI_COMPILE_COMMANDS=${libffi_cmake_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_LIBFFI_CONFIG_HEADER=${libffi_config_header}"
        "-DMEDIAPROXY_LIBFFI_PKGCONFIG=${libffi_pkgconfig}"
        "-DMEDIAPROXY_GLIB_INCLUDE_DIR=${glib_include_dir}"
        "-DMEDIAPROXY_GLIB_CONFIG_INCLUDE_DIR=${glib_config_include_dir}"
        "-DMEDIAPROXY_GLIB_LIBRARY=${glib_library}"
        "-DMEDIAPROXY_GOBJECT_LIBRARY=${gobject_library}"
        "-DMEDIAPROXY_GTHREAD_LIBRARY=${gthread_library}"
        "-DMEDIAPROXY_GMODULE_LIBRARY=${gmodule_library}"
        "-DMEDIAPROXY_GIO_LIBRARY=${gio_library}"
        "-DMEDIAPROXY_GLIB_COMPILE_COMMANDS=${glib_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_GLIB_CONFIG_HEADER=${glib_binary_directory}/glib/glibconfig.h"
        "-DMEDIAPROXY_GMODULE_CONFIG_HEADER=${glib_binary_directory}/gmodule/gmoduleconf.h"
        "-DMEDIAPROXY_GLIB_PKGCONFIG=${glib_pkgconfig}"
        "-DMEDIAPROXY_GOBJECT_PKGCONFIG=${gobject_pkgconfig}"
        "-DMEDIAPROXY_GTHREAD_PKGCONFIG=${gthread_pkgconfig}"
        "-DMEDIAPROXY_GMODULE_PKGCONFIG=${gmodule_pkgconfig}"
        "-DMEDIAPROXY_GIO_PKGCONFIG=${gio_pkgconfig}"
        "-DMEDIAPROXY_PCRE2_INCLUDE_DIR=${pcre2_include_dir}"
        "-DMEDIAPROXY_PCRE2_LIBRARY=${pcre2_library}"
        "-DMEDIAPROXY_PCRE2_COMPILE_COMMANDS=${pcre2_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_PCRE2_CONFIG_HEADER=${pcre2_config_header}"
        "-DMEDIAPROXY_PCRE2_PKGCONFIG=${pcre2_pkgconfig}"
        "-DMEDIAPROXY_LCMS2_INCLUDE_DIR=${lcms2_include_dir}"
        "-DMEDIAPROXY_LCMS2_LIBRARY=${lcms2_library}"
        "-DMEDIAPROXY_LCMS2_COMPILE_COMMANDS=${lcms2_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_ZLIB_INCLUDE_DIR=${zlib_include_dir}"
        "-DMEDIAPROXY_ZLIB_LIBRARY=${zlib_library}"
        "-DMEDIAPROXY_ZLIB_COMPILE_COMMANDS=${zlib_binary_directory}/compile_commands.json"
        "-DMEDIAPROXY_GOOGLETEST_URL=${googletest_url}"
        "-DMEDIAPROXY_GOOGLETEST_SHA256=${googletest_sha256}"
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/toolchains/llvm-musl.cmake"
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        "-DBUILD_TESTING=${BUILD_TESTING}"
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS
        "${application_binary_directory}/bootstrap"
        "${application_binary_directory}/bootstrap.map"
        "${application_binary_directory}/bootstrap.undefined-symbols.txt"
)

if(BUILD_TESTING)
    add_test(
        NAME application-tests
        COMMAND "${CMAKE_CTEST_COMMAND}"
            --test-dir "${application_binary_directory}"
            --output-on-failure
    )
endif()
