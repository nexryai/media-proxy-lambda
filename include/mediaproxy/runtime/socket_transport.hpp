#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <mediaproxy/runtime/streaming.hpp>

namespace mediaproxy::runtime {

struct RuntimeAuthority {
    std::string host;
    std::string service;
};

[[nodiscard]] std::optional<RuntimeAuthority> parse_runtime_authority(
    std::string_view authority);

class SocketTransport final : public ByteSink {
public:
    explicit SocketTransport(int fd) noexcept;
    ~SocketTransport() override;

    SocketTransport(const SocketTransport&) = delete;
    SocketTransport& operator=(const SocketTransport&) = delete;
    SocketTransport(SocketTransport&& other) noexcept;
    SocketTransport& operator=(SocketTransport&& other) noexcept;

    [[nodiscard]] static std::optional<SocketTransport> connect(
        std::string_view authority);

    [[nodiscard]] bool write(std::span<const std::byte> bytes) override;
    [[nodiscard]] std::ptrdiff_t read_some(
        std::span<std::byte> output) noexcept;
    [[nodiscard]] bool shutdown_write() noexcept;
    [[nodiscard]] explicit operator bool() const noexcept { return fd_ >= 0; }

private:
    void close() noexcept;

    int fd_ = -1;
};

} // namespace mediaproxy::runtime
