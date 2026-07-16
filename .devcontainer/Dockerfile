FROM ubuntu:resolute

RUN apt-get update && \
    apt-get install -y git xz-utils curl python3 clang-22 clangd-22 lld-22 llvm-22-linker-tools make ninja-build meson pkgconf cargo rustc rsync file perl

RUN curl -L https://github.com/Kitware/CMake/releases/download/v4.2.3/cmake-4.2.3-linux-x86_64.sh -o install_cmake.sh && bash install_cmake.sh --skip-license --prefix=/usr/local --exclude-subdir && rm install_cmake.sh

RUN curl -fsSL https://deb.nodesource.com/setup_24.x | bash -
RUN apt-get install -y nodejs
RUN npm install -g pnpm @openai/codex
