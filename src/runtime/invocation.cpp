#include <mediaproxy/runtime/invocation.hpp>

#include <exception>
#include <new>
#include <stdexcept>

#include <mediaproxy/http/response.hpp>
#include <mediaproxy/runtime/next_response.hpp>

namespace mediaproxy::runtime {
namespace {

[[nodiscard]] InvocationExecutionResult report_error(
    const InvocationResponder& responder,
    std::string_view request_id,
    std::string_view error_type,
    std::string_view error_message,
    InvocationExecutionResult reported) noexcept
{
    try {
        return responder.send_invocation_error(
                   request_id, error_type, error_message)
            ? reported
            : InvocationExecutionResult::error_submission_failure;
    } catch (...) {
        return InvocationExecutionResult::error_submission_failure;
    }
}

} // namespace

InvocationExecutionResult execute_invocation(
    const InvocationResponder& responder,
    const Invocation& invocation,
    InvocationHandlerFunction handler,
    void* handler_context) noexcept
{
    http::HttpResponse response;
    try {
        if (handler == nullptr) {
            throw std::invalid_argument{"missing invocation handler"};
        }
        response = handler(invocation, handler_context);
    } catch (const std::bad_alloc&) {
        return report_error(responder, invocation.request_id,
            "RuntimeOutOfMemory",
            "Invocation allocation failed before response",
            InvocationExecutionResult::out_of_memory_reported);
    } catch (const std::exception&) {
        return report_error(responder, invocation.request_id,
            "UnhandledInvocationError", "Invocation failed before response",
            InvocationExecutionResult::unhandled_error_reported);
    } catch (...) {
        return report_error(responder, invocation.request_id,
            "UnknownInvocationError", "Invocation failed before response",
            InvocationExecutionResult::unknown_error_reported);
    }

    // Once response submission is attempted, the integration stream may have
    // started. A failure here must terminate the environment rather than open
    // a second pre-response /error request for the same invocation.
    try {
        return responder.send_response(invocation.request_id, response)
            ? InvocationExecutionResult::response_sent
            : InvocationExecutionResult::response_failure;
    } catch (...) {
        return InvocationExecutionResult::response_failure;
    }
}

} // namespace mediaproxy::runtime
