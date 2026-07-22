#include <mediaproxy/runtime/socket_transport.hpp>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

namespace mediaproxy::runtime {
namespace {

[[nodiscard]] bool valid_service(std::string_view service) noexcept
{
    if (service.empty()) {
        return false;
    }
    std::uint32_t port = 0;
    for (const char character : service) {
        if (character < '0' || character > '9') {
            return false;
        }
        port = port * 10U + static_cast<std::uint32_t>(character - '0');
        if (port > 65535U) {
            return false;
        }
    }
    return port != 0;
}

} // namespace

std::optional<RuntimeAuthority> parse_runtime_authority(
    std::string_view authority)
{
    std::string_view host;
    std::string_view service;
    if (authority.starts_with('[')) {
        const std::size_t closing = authority.find(']');
        if (closing == std::string_view::npos || closing == 1
            || closing + 1 >= authority.size()
            || authority[closing + 1] != ':') {
            return std::nullopt;
        }
        host = authority.substr(1, closing - 1);
        service = authority.substr(closing + 2);
    } else {
        const std::size_t colon = authority.rfind(':');
        if (colon == std::string_view::npos || colon == 0
            || authority.find(':') != colon) {
            return std::nullopt;
        }
        host = authority.substr(0, colon);
        service = authority.substr(colon + 1);
    }
    if (host.find_first_of("\r\n \t/\\") != std::string_view::npos
        || !valid_service(service)) {
        return std::nullopt;
    }
    return RuntimeAuthority{
        .host = std::string{host},
        .service = std::string{service},
    };
}

SocketTransport::SocketTransport(int fd) noexcept
    : fd_(fd)
{
}

SocketTransport::~SocketTransport()
{
    close();
}

SocketTransport::SocketTransport(SocketTransport&& other) noexcept
    : fd_(std::exchange(other.fd_, -1))
{
}

SocketTransport& SocketTransport::operator=(SocketTransport&& other) noexcept
{
    if (this != &other) {
        close();
        fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
}

std::optional<SocketTransport> SocketTransport::connect(
    std::string_view authority)
{
    const auto parsed = parse_runtime_authority(authority);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_NUMERICSERV;
    addrinfo* raw_addresses = nullptr;
    if (getaddrinfo(parsed->host.c_str(), parsed->service.c_str(),
            &hints, &raw_addresses)
        != 0) {
        return std::nullopt;
    }
    struct AddressCleanup {
        addrinfo* addresses;
        ~AddressCleanup() { freeaddrinfo(addresses); }
    } cleanup{raw_addresses};

    for (const addrinfo* address = raw_addresses; address != nullptr;
        address = address->ai_next) {
        const int fd = socket(address->ai_family,
            address->ai_socktype | SOCK_CLOEXEC, address->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (::connect(fd, address->ai_addr, address->ai_addrlen) == 0) {
            return SocketTransport{fd};
        }
        ::close(fd);
    }
    return std::nullopt;
}

bool SocketTransport::write(std::span<const std::byte> bytes)
{
    if (fd_ < 0) {
        return false;
    }
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const std::size_t remaining = bytes.size() - offset;
        const std::size_t request = std::min(remaining,
            static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
        const ssize_t written = send(fd_, bytes.data() + offset, request,
            MSG_NOSIGNAL);
        if (written > 0) {
            offset += static_cast<std::size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

std::ptrdiff_t SocketTransport::read_some(
    std::span<std::byte> output) noexcept
{
    if (fd_ < 0 || output.empty()) {
        return -1;
    }
    while (true) {
        const std::size_t request = std::min(output.size(),
            static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
        const ssize_t received = recv(fd_, output.data(), request, 0);
        if (received >= 0) {
            return static_cast<std::ptrdiff_t>(received);
        }
        if (errno != EINTR) {
            return -1;
        }
    }
}

bool SocketTransport::shutdown_write() noexcept
{
    return fd_ >= 0 && shutdown(fd_, SHUT_WR) == 0;
}

void SocketTransport::close() noexcept
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace mediaproxy::runtime
