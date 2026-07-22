#include <cstddef>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <mediaproxy/http/response.hpp>
#include <mediaproxy/runtime/invocation.hpp>
#include <mediaproxy/runtime/next_response.hpp>

namespace {

using mediaproxy::http::HttpResponse;
using mediaproxy::runtime::Invocation;
using mediaproxy::runtime::InvocationExecutionResult;
using mediaproxy::runtime::InvocationResponder;
using mediaproxy::runtime::execute_invocation;

class FakeResponder final : public InvocationResponder {
public:
    bool send_response(
        std::string_view request_id,
        const HttpResponse& response) const override
    {
        if (throw_response) {
            throw std::runtime_error{"response stream failed"};
        }
        response_request_id = request_id;
        response_status = response.status;
        return response_result;
    }

    bool send_invocation_error(
        std::string_view request_id,
        std::string_view error_type,
        std::string_view error_message) const override
    {
        if (throw_error) {
            throw std::runtime_error{"error endpoint failed"};
        }
        error_request_id = request_id;
        this->error_type = error_type;
        this->error_message = error_message;
        return error_result;
    }

    bool response_result = true;
    bool error_result = true;
    bool throw_response = false;
    bool throw_error = false;
    mutable std::string response_request_id;
    mutable unsigned int response_status = 0;
    mutable std::string error_request_id;
    mutable std::string error_type;
    mutable std::string error_message;
};

HttpResponse Success(const Invocation&, void*)
{
    return {.status = 204, .headers = {}, .body = {}};
}

HttpResponse OutOfMemory(const Invocation&, void*)
{
    throw std::bad_alloc{};
}

HttpResponse StandardFailure(const Invocation&, void*)
{
    throw std::runtime_error{"private detail must not be reported"};
}

HttpResponse UnknownFailure(const Invocation&, void*)
{
    throw 7;
}

Invocation TestInvocation()
{
    return {
        .request_id = "request-7",
        .deadline_ms = 123,
        .trace_id = "Root=trace",
        .event = {std::byte{0}},
    };
}

TEST(RuntimeInvocation, SendsSuccessfulResponseForMatchingRequest)
{
    FakeResponder responder;
    const Invocation invocation = TestInvocation();
    EXPECT_EQ(execute_invocation(responder, invocation, &Success),
        InvocationExecutionResult::response_sent);
    EXPECT_EQ(responder.response_request_id, invocation.request_id);
    EXPECT_EQ(responder.response_status, 204U);
    EXPECT_TRUE(responder.error_request_id.empty());

    responder.response_result = false;
    EXPECT_EQ(execute_invocation(responder, invocation, &Success),
        InvocationExecutionResult::response_failure);
}

struct FailureCase {
    const char* name;
    mediaproxy::runtime::InvocationHandlerFunction handler;
    const char* error_type;
    const char* error_message;
    InvocationExecutionResult result;
};

class RuntimeInvocationFailureTest :
    public testing::TestWithParam<FailureCase> {
};

TEST_P(RuntimeInvocationFailureTest, ReportsSanitizedPreResponseFailure)
{
    const FailureCase& expected = GetParam();
    FakeResponder responder;
    const Invocation invocation = TestInvocation();
    EXPECT_EQ(execute_invocation(responder, invocation, expected.handler),
        expected.result);
    EXPECT_EQ(responder.error_request_id, invocation.request_id);
    EXPECT_EQ(responder.error_type, expected.error_type);
    EXPECT_EQ(responder.error_message, expected.error_message);
    EXPECT_EQ(responder.error_message.find("private detail"),
        std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    RuntimeInvocation,
    RuntimeInvocationFailureTest,
    testing::Values(
        FailureCase{"out_of_memory", &OutOfMemory, "RuntimeOutOfMemory",
            "Invocation allocation failed before response",
            InvocationExecutionResult::out_of_memory_reported},
        FailureCase{"standard", &StandardFailure,
            "UnhandledInvocationError", "Invocation failed before response",
            InvocationExecutionResult::unhandled_error_reported},
        FailureCase{"unknown", &UnknownFailure, "UnknownInvocationError",
            "Invocation failed before response",
            InvocationExecutionResult::unknown_error_reported},
        FailureCase{"missing_handler", nullptr, "UnhandledInvocationError",
            "Invocation failed before response",
            InvocationExecutionResult::unhandled_error_reported}),
    [](const testing::TestParamInfo<FailureCase>& info) {
        return info.param.name;
    });

TEST(RuntimeInvocation, StopsWhenInvocationErrorSubmissionFails)
{
    FakeResponder responder;
    responder.error_result = false;
    EXPECT_EQ(execute_invocation(
                  responder, TestInvocation(), &StandardFailure),
        InvocationExecutionResult::error_submission_failure);

    responder.error_result = true;
    responder.throw_error = true;
    EXPECT_EQ(execute_invocation(
                  responder, TestInvocation(), &StandardFailure),
        InvocationExecutionResult::error_submission_failure);
}

TEST(RuntimeInvocation, DoesNotReportErrorAfterResponseSubmissionStarts)
{
    FakeResponder responder;
    responder.throw_response = true;
    EXPECT_EQ(execute_invocation(responder, TestInvocation(), &Success),
        InvocationExecutionResult::response_failure);
    EXPECT_TRUE(responder.error_request_id.empty());
}

} // namespace
