add_library(mediaproxy_hardening INTERFACE)
add_library(mediaproxy_warnings INTERFACE)

if(NOT DEFINED MEDIAPROXY_FORTIFY_INCLUDE_DIR
        OR NOT EXISTS "${MEDIAPROXY_FORTIFY_INCLUDE_DIR}/fortify-headers.h")
    message(FATAL_ERROR
        "Pinned fortify headers are required at MEDIAPROXY_FORTIFY_INCLUDE_DIR")
endif()

target_compile_features(mediaproxy_hardening INTERFACE cxx_std_20)
target_compile_definitions(mediaproxy_hardening INTERFACE
    _FORTIFY_SOURCE=3
    _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST
)
target_compile_options(mediaproxy_hardening INTERFACE
    "SHELL:-isystem ${MEDIAPROXY_FORTIFY_INCLUDE_DIR}"
    -fstack-protector-strong
    -ftrivial-auto-var-init=zero
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -ffunction-sections
    -fdata-sections
)

target_compile_options(mediaproxy_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)
if(MEDIAPROXY_SANITIZER_BUILD)
    target_compile_options(mediaproxy_hardening INTERFACE
        -O1
        -g
        -fno-omit-frame-pointer
        -fsanitize=address,undefined
        -fno-sanitize-recover=all
    )
    target_link_options(mediaproxy_hardening INTERFACE
        -fuse-ld=lld
        -fsanitize=address,undefined
        -fno-sanitize-link-runtime
        -fno-sanitize-recover=all
        LINKER:--gc-sections
        LINKER:--fatal-warnings
        LINKER:-z,relro
        LINKER:-z,now
        LINKER:-z,noexecstack
    )
else()
    target_compile_options(mediaproxy_hardening INTERFACE
        -O2
        -flto=thin
        -fsanitize=cfi
        -fsanitize-trap=cfi
        -fno-sanitize-recover=cfi
    )
    target_link_options(mediaproxy_hardening INTERFACE
        -fuse-ld=lld
        -static-pie
        -flto=thin
        -fsanitize=cfi
        -fsanitize-trap=cfi
        -fno-sanitize-recover=cfi
        LINKER:--gc-sections
        LINKER:--fatal-warnings
        LINKER:-z,relro
        LINKER:-z,now
        LINKER:-z,noexecstack
    )
endif()

if(MEDIAPROXY_TARGET_ARCH STREQUAL "x86_64")
    target_compile_options(mediaproxy_hardening INTERFACE
        -fstack-clash-protection
        -fcf-protection=full
    )
elseif(MEDIAPROXY_TARGET_ARCH STREQUAL "arm64")
    target_compile_options(mediaproxy_hardening INTERFACE
        -mbranch-protection=standard
    )
else()
    message(FATAL_ERROR "Unsupported hardening architecture")
endif()
