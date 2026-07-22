#include <array>
#include <cstddef>
#include <span>
#include <string>

#include <gtest/gtest.h>
#include <mediaproxy/runtime/socket_transport.hpp>

#include <sys/socket.h>
#include <unistd.h>

namespace {

using mediaproxy::runtime::SocketTransport;
using mediaproxy::runtime::parse_runtime_authority;

TEST(RuntimeSocketTransport, ParsesHostPortAndBracketedIpv6)
{
    const auto ipv4 = parse_runtime_authority("127.0.0.1:9001");
    ASSERT_TRUE(ipv4.has_value());
    EXPECT_EQ(ipv4->host, "127.0.0.1");
    EXPECT_EQ(ipv4->service, "9001");

    const auto ipv6 = parse_runtime_authority("[::1]:9001");
    ASSERT_TRUE(ipv6.has_value());
    EXPECT_EQ(ipv6->host, "::1");
    EXPECT_EQ(ipv6->service, "9001");

    EXPECT_FALSE(parse_runtime_authority("missing-port").has_value());
    EXPECT_FALSE(parse_runtime_authority("host:0").has_value());
    EXPECT_FALSE(parse_runtime_authority("host:65536").has_value());
    EXPECT_FALSE(parse_runtime_authority("bad host:9001").has_value());
}

TEST(RuntimeSocketTransport, WritesAllBytesAndReadsFragments)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0,
                  sockets.data()),
        0);
    SocketTransport transport{sockets[0]};
    const std::string outbound = "request-bytes";
    ASSERT_TRUE(transport.write(std::as_bytes(std::span{outbound})));

    std::array<char, 32> peer_buffer{};
    const ssize_t peer_read =
        recv(sockets[1], peer_buffer.data(), peer_buffer.size(), 0);
    ASSERT_EQ(peer_read, static_cast<ssize_t>(outbound.size()));
    EXPECT_EQ(std::string_view(peer_buffer.data(), outbound.size()), outbound);

    const std::string inbound = "fragmented-response";
    ASSERT_EQ(send(sockets[1], inbound.data(), inbound.size(), MSG_NOSIGNAL),
        static_cast<ssize_t>(inbound.size()));
    std::array<std::byte, 4> fragment{};
    EXPECT_EQ(transport.read_some(fragment), 4);
    EXPECT_EQ(std::string_view(
                  reinterpret_cast<const char*>(fragment.data()), 4),
        "frag");
    ::close(sockets[1]);
}

TEST(RuntimeSocketTransport, ReportsPeerClosureWithoutSigpipe)
{
    std::array<int, 2> sockets{};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0,
                  sockets.data()),
        0);
    SocketTransport transport{sockets[0]};
    ::close(sockets[1]);
    const std::string bytes = "response";
    EXPECT_FALSE(transport.write(std::as_bytes(std::span{bytes})));
}

} // namespace
