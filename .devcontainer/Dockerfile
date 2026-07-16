FROM ubuntu:resolute AS toolchain

RUN apt-get update && \
    apt-get install -y --no-install-recommends git xz-utils curl python3 clang-22 clangd-22 lld-22 llvm-22-linker-tools make ninja-build meson pkgconf cargo rustc rsync file perl && \
    rm -rf /var/lib/apt/lists/*

RUN container_arch="$(dpkg --print-architecture)" && \
    case "${container_arch}" in \
        amd64) \
            cmake_arch=x86_64; \
            cmake_sha256=b760514fde7fc510fcd16e51a81a4d2687b1f051b263d40b6806789d3d9fd62c \
            ;; \
        arm64) \
            cmake_arch=aarch64; \
            cmake_sha256=8e65cb924c41e3f6c5c98d1900aeeab3cfe3494f18ed8e5e63b91022df710401 \
            ;; \
        *) \
            echo "Unsupported container architecture: ${container_arch}" >&2; \
            exit 1 \
            ;; \
    esac && \
    curl -fsSLo /tmp/install_cmake.sh "https://github.com/Kitware/CMake/releases/download/v4.2.3/cmake-4.2.3-linux-${cmake_arch}.sh" && \
    echo "${cmake_sha256}  /tmp/install_cmake.sh" | sha256sum -c - && \
    bash /tmp/install_cmake.sh --skip-license --prefix=/usr/local --exclude-subdir && \
    rm /tmp/install_cmake.sh

FROM toolchain AS ci

FROM toolchain AS development

RUN curl -fsSL https://deb.nodesource.com/setup_24.x | bash -
RUN apt-get install -y --no-install-recommends nodejs && \
    rm -rf /var/lib/apt/lists/*
RUN npm install -g pnpm @openai/codex
