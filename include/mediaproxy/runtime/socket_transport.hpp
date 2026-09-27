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

[[nodiscard]] auto parse_runtime_authority(std::string_view authority) -> std::optional<RuntimeAuthority>;

class SocketTransport final : public ByteSink {
  public:
    explicit SocketTransport(int fd) noexcept;
    ~SocketTransport() override;

    SocketTransport(const SocketTransport &) = delete;
    auto operator=(const SocketTransport &) -> SocketTransport & = delete;
    SocketTransport(SocketTransport &&other) noexcept;
    auto operator=(SocketTransport &&other) noexcept -> SocketTransport &;

    [[nodiscard]] static auto connect(std::string_view authority) -> std::optional<SocketTransport>;

    [[nodiscard]] auto write(std::span<const std::byte> bytes) -> bool override;
    [[nodiscard]] auto read_some(std::span<std::byte> output) const noexcept -> std::ptrdiff_t;
    [[nodiscard]] auto shutdown_write() const noexcept -> bool;
    [[nodiscard]] explicit operator bool() const noexcept {

        return fd_ >= 0;
    }

  private:
    void close() noexcept;

    int fd_ = -1;
};

} // namespace mediaproxy::runtime
