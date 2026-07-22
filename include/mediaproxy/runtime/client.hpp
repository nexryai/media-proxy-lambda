#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <mediaproxy/http/response.hpp>
#include <mediaproxy/runtime/next_response.hpp>
#include <mediaproxy/runtime/socket_transport.hpp>

namespace mediaproxy::runtime {

[[nodiscard]] std::optional<Invocation> poll_next_on(
    SocketTransport& transport,
    std::string_view runtime_authority);

[[nodiscard]] bool send_response_on(
    SocketTransport& transport,
    std::string_view runtime_authority,
    std::string_view request_id,
    const http::HttpResponse& response);

class RuntimeClient {
public:
    explicit RuntimeClient(std::string authority);

    [[nodiscard]] std::optional<Invocation> poll_next() const;
    [[nodiscard]] bool send_response(
        std::string_view request_id,
        const http::HttpResponse& response) const;

private:
    std::string authority_;
};

} // namespace mediaproxy::runtime
