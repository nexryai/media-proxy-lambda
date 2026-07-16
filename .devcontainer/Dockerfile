FROM ubuntu:resolute AS toolchain

RUN apt-get update && \
    apt-get install -y --no-install-recommends git xz-utils curl python3 perl cmake clang-22 clangd-22 lld-22 llvm-22-linker-tools make ninja-build meson pkgconf cargo rustc rsync file && \
    rm -rf /var/lib/apt/lists/*

FROM toolchain AS ci

FROM toolchain AS development

RUN curl -fsSL https://deb.nodesource.com/setup_24.x | bash -
RUN apt-get install -y --no-install-recommends nodejs && \
    rm -rf /var/lib/apt/lists/*
RUN npm install -g pnpm @openai/codex
