#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <mediaproxy/runtime/next_response.hpp>

namespace {

using mediaproxy::runtime::NextParseStatus;
using mediaproxy::runtime::NextResponseParser;
using mediaproxy::runtime::make_next_request_head;

std::span<const std::byte> Bytes(std::string_view value)
{
    return std::as_bytes(std::span{value});
}

TEST(RuntimeNextResponse, BuildsStrictPollRequest)
{
    EXPECT_EQ(make_next_request_head("127.0.0.1:9001"),
        "GET /2018-06-01/runtime/invocation/next HTTP/1.1\r\n"
        "Host: 127.0.0.1:9001\r\nConnection: close\r\n\r\n");
    EXPECT_TRUE(make_next_request_head("bad host").empty());
}

TEST(RuntimeNextResponse, ParsesOneByteFragments)
{
    const std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 7\r\n"
        "Lambda-Runtime-Aws-Request-Id: request-1\r\n"
        "Lambda-Runtime-Deadline-Ms: 123456789\r\n"
        "Lambda-Runtime-Trace-Id: Root=trace\r\n"
        "Content-Type: application/json\r\n\r\n"
        "{\"x\":1}";
    NextResponseParser parser;
    for (std::size_t index = 0; index < response.size(); ++index) {
        const auto status = parser.feed(Bytes(
            std::string_view{response}.substr(index, 1)));
        EXPECT_EQ(status, index + 1 == response.size()
                ? NextParseStatus::complete
                : NextParseStatus::incomplete);
    }
    EXPECT_EQ(parser.invocation().request_id, "request-1");
    EXPECT_EQ(parser.invocation().deadline_ms, 123456789U);
    EXPECT_EQ(parser.invocation().trace_id, "Root=trace");
    const std::string event(
        reinterpret_cast<const char*>(parser.invocation().event.data()),
        parser.invocation().event.size());
    EXPECT_EQ(event, "{\"x\":1}");
}

TEST(RuntimeNextResponse, MovesCompletedInvocationWithoutBodyCopy)
{
    const std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Lambda-Runtime-Aws-Request-Id: request-move\r\n"
        "Lambda-Runtime-Deadline-Ms: 123456789\r\n"
        "Lambda-Runtime-Trace-Id: Root=trace\r\n"
        "Content-Length: 4\r\n\r\nbody";
    NextResponseParser parser;
    ASSERT_EQ(parser.feed(Bytes(response)), NextParseStatus::complete);
    const std::byte* const owned_body = parser.invocation().event.data();

    const auto invocation = parser.take_invocation();
    EXPECT_EQ(invocation.event.data(), owned_body);
    EXPECT_EQ(std::string_view(
                  reinterpret_cast<const char*>(invocation.event.data()),
                  invocation.event.size()),
        "body");
}

TEST(RuntimeNextResponse, RejectsDuplicateAndInvalidFraming)
{
    for (const std::string response : {
             "HTTP/1.1 500 Error\r\nContent-Length: 0\r\n\r\n",
             "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nContent-Length: 0\r\n"
             "Lambda-Runtime-Aws-Request-Id: id\r\n"
             "Lambda-Runtime-Deadline-Ms: 1\r\n\r\n",
             "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
             "Lambda-Runtime-Aws-Request-Id: id\r\n"
             "Lambda-Runtime-Deadline-Ms: 1\r\n\r\n"}) {
        NextResponseParser parser;
        EXPECT_EQ(parser.feed(Bytes(response)), NextParseStatus::error);
    }
}

TEST(RuntimeNextResponse, RejectsBodyBeyondDeclaredLength)
{
    const std::string response =
        "HTTP/1.1 200 OK\r\nContent-Length: 1\r\n"
        "Lambda-Runtime-Aws-Request-Id: id\r\n"
        "Lambda-Runtime-Deadline-Ms: 1\r\n\r\nxx";
    NextResponseParser parser;
    EXPECT_EQ(parser.feed(Bytes(response)), NextParseStatus::error);
}

} // namespace
