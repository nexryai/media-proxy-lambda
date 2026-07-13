FROM ubuntu:resolute

RUN apt-get update && \
    apt-get install -y git xz-utils curl clang-22 clangd-22 lld-22 llvm-22-linker-tools make

RUN curl -L https://github.com/Kitware/CMake/releases/download/v4.2.3/cmake-4.2.3-linux-x86_64.sh -o install_cmake.sh && bash install_cmake.sh --skip-license --prefix=/usr/local --exclude-subdir && rm install_cmake.sh
