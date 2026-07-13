include(ExternalProject)
include(cmake/DependencyLock.cmake)

set(MEDIAPROXY_TARGET_ARCH "x86_64" CACHE STRING "Lambda target architecture")
set_property(CACHE MEDIAPROXY_TARGET_ARCH PROPERTY STRINGS x86_64 arm64)

if(MEDIAPROXY_TARGET_ARCH STREQUAL "x86_64")
    set(target_triple x86_64-linux-musl)
    set(target_processor x86_64)
    set(compiler_rt_arch x86_64)
    set(kernel_arch x86)
elseif(MEDIAPROXY_TARGET_ARCH STREQUAL "arm64")
    set(target_triple aarch64-linux-musl)
    set(target_processor aarch64)
    set(compiler_rt_arch aarch64)
    set(kernel_arch arm64)
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

set(source_cache "${CMAKE_SOURCE_DIR}/.cache/sources")
set(sysroot "${CMAKE_BINARY_DIR}/sysroot")

mediaproxy_lock_get(linux-uapi-headers url linux_url)
mediaproxy_lock_get(linux-uapi-headers sha256 linux_sha256)
mediaproxy_lock_get(musl url musl_url)
mediaproxy_lock_get(musl sha256 musl_sha256)
mediaproxy_lock_get_patch(musl 0 path musl_patch_relative)
mediaproxy_lock_get_patch(musl 0 sha256 musl_patch_sha256)
mediaproxy_lock_get(llvm-runtimes url llvm_url)
mediaproxy_lock_get(llvm-runtimes sha256 llvm_sha256)
mediaproxy_lock_get(googletest url googletest_url)
mediaproxy_lock_get(googletest sha256 googletest_sha256)

set(musl_patch "${CMAKE_SOURCE_DIR}/${musl_patch_relative}")
file(SHA256 "${musl_patch}" actual_musl_patch_sha256)
if(NOT actual_musl_patch_sha256 STREQUAL musl_patch_sha256)
    message(FATAL_ERROR "musl security patch does not match dependencies.lock.json")
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
        "-DCMAKE_C_COMPILER_TARGET=${target_triple}"
        "-DCMAKE_CXX_COMPILER_TARGET=${target_triple}"
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

set(application_binary_directory "${CMAKE_BINARY_DIR}/application")
ExternalProject_Add(application
    DEPENDS llvm_runtimes
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
        "-DMEDIAPROXY_GOOGLETEST_URL=${googletest_url}"
        "-DMEDIAPROXY_GOOGLETEST_SHA256=${googletest_sha256}"
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/toolchains/llvm-musl.cmake"
        "-DCMAKE_BUILD_TYPE=Release"
        "-DBUILD_TESTING=${BUILD_TESTING}"
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS
        "${application_binary_directory}/bootstrap"
        "${application_binary_directory}/bootstrap.map"
)

if(BUILD_TESTING)
    add_test(
        NAME application-tests
        COMMAND "${CMAKE_CTEST_COMMAND}"
            --test-dir "${application_binary_directory}"
            --output-on-failure
    )
endif()
