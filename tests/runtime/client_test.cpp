#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <mediaproxy/http/response.hpp>
#include <mediaproxy/runtime/client.hpp>

#include <sys/socket.h>
#include <unistd.h>

namespace {

using mediaproxy::http::HttpHeader;
using mediaproxy::http::HttpResponse;
using mediaproxy::runtime::SocketTransport;
using mediaproxy::runtime::poll_next_on;
using mediaproxy::runtime::send_response_on;

std::array<int, 2> SocketPair()
{
    std::array<int, 2> sockets{};
    EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0,
                  sockets.data()),
        0);
    return sockets;
}

void SendAll(int fd, std::string_view bytes)
{
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const ssize_t written = send(fd, bytes.data() + offset,
            bytes.size() - offset, MSG_NOSIGNAL);
        ASSERT_GT(written, 0);
        offset += static_cast<std::size_t>(written);
    }
}

std::string ReadToEnd(int fd)
{
    std::string output;
    std::array<char, 4096> buffer{};
    while (true) {
        const ssize_t received = recv(fd, buffer.data(), buffer.size(), 0);
        EXPECT_GE(received, 0);
        if (received <= 0) {
            return output;
        }
        output.append(buffer.data(), static_cast<std::size_t>(received));
    }
}

TEST(RuntimeClient, PollsInvocationAndPreservesRuntimeHeaders)
{
    auto sockets = SocketPair();
    SocketTransport transport{sockets[0]};
    SendAll(sockets[1],
        "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n"
        "Lambda-Runtime-Aws-Request-Id: invocation-1\r\n"
        "Lambda-Runtime-Deadline-Ms: 987654321\r\n"
        "Lambda-Runtime-Trace-Id: Root=trace-1\r\n\r\n{}");

    const auto invocation = poll_next_on(transport, "127.0.0.1:9001");
    ASSERT_TRUE(invocation.has_value());
    EXPECT_EQ(invocation->request_id, "invocation-1");
    EXPECT_EQ(invocation->deadline_ms, 987654321U);
    EXPECT_EQ(invocation->trace_id, "Root=trace-1");
    EXPECT_EQ(invocation->event, std::vector<std::byte>({
        std::byte{'{'}, std::byte{'}'}}));

    std::array<char, 256> request{};
    const ssize_t received =
        recv(sockets[1], request.data(), request.size(), 0);
    ASSERT_GT(received, 0);
    EXPECT_EQ(std::string_view(request.data(), static_cast<std::size_t>(received)),
        "GET /2018-06-01/runtime/invocation/next HTTP/1.1\r\n"
        "Host: 127.0.0.1:9001\r\nConnection: close\r\n\r\n");
    ::close(sockets[1]);
}

TEST(RuntimeClient, SendsStreamingResponseAndWaitsForAcknowledgement)
{
    auto sockets = SocketPair();
    SocketTransport transport{sockets[0]};
    SendAll(sockets[1],
        "HTTP/1.1 202 Accepted\r\nContent-Length: 0\r\n\r\n");
    const HttpResponse response{
        .status = 200,
        .headers = {HttpHeader{"Content-Type", "application/json"}},
        .body = "{}",
    };
    ASSERT_TRUE(send_response_on(
        transport, "127.0.0.1:9001", "invocation-1", response));
    const std::string request = ReadToEnd(sockets[1]);
    EXPECT_TRUE(request.starts_with(
        "POST /2018-06-01/runtime/invocation/invocation-1/response HTTP/1.1\r\n"));
    EXPECT_NE(request.find(
                  "Lambda-Runtime-Function-Response-Mode: streaming\r\n"),
        std::string::npos);
    EXPECT_TRUE(request.ends_with("2\r\n{}\r\n0\r\n\r\n"));
    ::close(sockets[1]);
}

TEST(RuntimeClient, RejectsNonAcceptedRuntimeResponse)
{
    auto sockets = SocketPair();
    SocketTransport transport{sockets[0]};
    SendAll(sockets[1],
        "HTTP/1.1 500 Error\r\nContent-Length: 0\r\n\r\n");
    EXPECT_FALSE(send_response_on(transport, "127.0.0.1:9001", "id",
        HttpResponse{.status = 500, .headers = {}, .body = "error"}));
    ::close(sockets[1]);
}

} // namespace
