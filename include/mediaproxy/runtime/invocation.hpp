#pragma once

#include <string_view>

#include <mediaproxy/http/response.hpp>
#include <mediaproxy/runtime/next_response.hpp>

namespace mediaproxy::runtime {

class InvocationResponder {
  public:
    virtual ~InvocationResponder() = default;

    [[nodiscard]] virtual auto send_response(std::string_view request_id, const http::HttpResponse &response) const -> bool = 0;
    [[nodiscard]] virtual auto send_invocation_error(std::string_view request_id, std::string_view error_type, std::string_view error_message) const -> bool = 0;
};

using InvocationHandlerFunction = http::HttpResponse (*)(const Invocation &invocation, void *context);

enum class InvocationExecutionResult {
    response_sent,
    response_failure,
    out_of_memory_reported,
    unhandled_error_reported,
    unknown_error_reported,
    error_submission_failure,
};

[[nodiscard]] auto execute_invocation(const InvocationResponder &responder, const Invocation &invocation, InvocationHandlerFunction handler, void *handler_context = nullptr) noexcept -> InvocationExecutionResult;

} // namespace mediaproxy::runtime
