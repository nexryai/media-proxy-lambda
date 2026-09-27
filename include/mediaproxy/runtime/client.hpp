#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <mediaproxy/http/response.hpp>
#include <mediaproxy/runtime/invocation.hpp>
#include <mediaproxy/runtime/next_response.hpp>
#include <mediaproxy/runtime/socket_transport.hpp>

namespace mediaproxy::runtime {

[[nodiscard]] auto poll_next_on(SocketTransport &transport, std::string_view runtime_authority) -> std::optional<Invocation>;

[[nodiscard]] auto send_response_on(SocketTransport &transport, std::string_view runtime_authority, std::string_view request_id, const http::HttpResponse &response) -> bool;

[[nodiscard]] auto send_invocation_error_on(SocketTransport &transport, std::string_view runtime_authority, std::string_view request_id, std::string_view error_type, std::string_view error_message) -> bool;

class RuntimeClient final : public InvocationResponder {
  public:
    explicit RuntimeClient(std::string authority);

    [[nodiscard]] auto poll_next() const -> std::optional<Invocation>;
    [[nodiscard]] auto send_response(std::string_view request_id, const http::HttpResponse &response) const -> bool override;
    [[nodiscard]] auto send_invocation_error(std::string_view request_id, std::string_view error_type, std::string_view error_message) const -> bool override;

  private:
    std::string authority_;
};

} // namespace mediaproxy::runtime
