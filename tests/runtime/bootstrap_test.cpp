#include <array>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

class FileDescriptor final {
public:
    explicit FileDescriptor(int value = -1) noexcept
        : value_(value)
    {
    }

    ~FileDescriptor()
    {
        if (value_ >= 0) {
            ::close(value_);
        }
    }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    FileDescriptor(FileDescriptor&& other) noexcept
        : value_(std::exchange(other.value_, -1))
    {
    }

    [[nodiscard]] int get() const noexcept { return value_; }

private:
    int value_;
};

class ChildProcess final {
public:
    explicit ChildProcess(pid_t pid) noexcept
        : pid_(pid)
    {
    }

    ~ChildProcess()
    {
        if (pid_ > 0) {
            ::kill(pid_, SIGKILL);
            int status = 0;
            while (::waitpid(pid_, &status, 0) < 0 && errno == EINTR) {
            }
        }
    }

    [[nodiscard]] int wait()
    {
        int status = 0;
        pid_t result = -1;
        do {
            result = ::waitpid(pid_, &status, 0);
        } while (result < 0 && errno == EINTR);
        if (result == pid_) {
            pid_ = -1;
            return status;
        }
        return -1;
    }

private:
    pid_t pid_;
};

[[nodiscard]] FileDescriptor CreateListener(std::uint16_t& port)
{
    FileDescriptor listener{::socket(AF_INET, SOCK_STREAM, 0)};
    if (listener.get() < 0) {
        return listener;
    }
    const sockaddr_in address{
        .sin_family = AF_INET,
        .sin_port = 0,
        .sin_addr = {.s_addr = htonl(INADDR_LOOPBACK)},
        .sin_zero = {},
    };
    if (::bind(listener.get(), reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)) != 0
        || ::listen(listener.get(), 4) != 0) {
        return FileDescriptor{};
    }
    sockaddr_in bound{};
    socklen_t bound_size = sizeof(bound);
    if (::getsockname(listener.get(), reinterpret_cast<sockaddr*>(&bound),
            &bound_size) != 0) {
        return FileDescriptor{};
    }
    port = ntohs(bound.sin_port);
    return listener;
}

[[nodiscard]] FileDescriptor Accept(int listener)
{
    int accepted = -1;
    do {
        accepted = ::accept(listener, nullptr, nullptr);
    } while (accepted < 0 && errno == EINTR);
    return FileDescriptor{accepted};
}

[[nodiscard]] bool SendAll(int socket, std::string_view bytes)
{
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const ssize_t sent = ::send(
            socket, bytes.data() + offset, bytes.size() - offset, 0);
        if (sent < 0 && errno == EINTR) {
            continue;
        }
        if (sent <= 0) {
            return false;
        }
        offset += static_cast<std::size_t>(sent);
    }
    return true;
}

[[nodiscard]] std::string ReadHeaders(int socket)
{
    std::string output;
    std::array<char, 1024> buffer{};
    while (output.size() <= 64U * 1024U
        && output.find("\r\n\r\n") == std::string::npos) {
        const ssize_t received = ::recv(socket, buffer.data(), buffer.size(), 0);
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received <= 0) {
            break;
        }
        output.append(buffer.data(), static_cast<std::size_t>(received));
    }
    return output;
}

[[nodiscard]] std::string ReadToEnd(int socket)
{
    std::string output;
    std::array<char, 4096> buffer{};
    while (output.size() <= 16U * 1024U * 1024U) {
        const ssize_t received = ::recv(socket, buffer.data(), buffer.size(), 0);
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received <= 0) {
            break;
        }
        output.append(buffer.data(), static_cast<std::size_t>(received));
    }
    return output;
}

[[nodiscard]] std::string InvocationResponse(
    std::string_view request_id,
    std::string_view trace_id,
    std::string_view event)
{
    std::string response =
        "HTTP/1.1 200 OK\r\nLambda-Runtime-Aws-Request-Id: ";
    response += request_id;
    response += "\r\nLambda-Runtime-Deadline-Ms: 9999999999999";
    response += "\r\nLambda-Runtime-Trace-Id: ";
    response += trace_id;
    response += "\r\nContent-Length: ";
    response += std::to_string(event.size());
    response += "\r\nConnection: close\r\n\r\n";
    response += event;
    return response;
}

[[nodiscard]] std::string ServeInvocation(
    int listener,
    std::string_view request_id,
    std::string_view trace_id,
    std::string_view event)
{
    FileDescriptor poll = Accept(listener);
    EXPECT_GE(poll.get(), 0);
    const std::string poll_request = ReadHeaders(poll.get());
    EXPECT_TRUE(poll_request.starts_with(
        "GET /2018-06-01/runtime/invocation/next HTTP/1.1\r\n"));
    EXPECT_TRUE(SendAll(
        poll.get(), InvocationResponse(request_id, trace_id, event)));

    FileDescriptor response = Accept(listener);
    EXPECT_GE(response.get(), 0);
    const std::string response_request = ReadToEnd(response.get());
    EXPECT_TRUE(SendAll(response.get(),
        "HTTP/1.1 202 Accepted\r\nContent-Length: 0\r\n\r\n"));
    return response_request;
}

void ExpectStreamingResponse(
    const std::string& request,
    std::string_view request_id,
    unsigned int status,
    std::string_view body)
{
    const std::string request_line =
        "POST /2018-06-01/runtime/invocation/" + std::string{request_id}
        + "/response HTTP/1.1\r\n";
    EXPECT_TRUE(request.starts_with(request_line));
    EXPECT_NE(request.find(
                  "Lambda-Runtime-Function-Response-Mode: streaming\r\n"),
        std::string::npos);
    EXPECT_NE(request.find("Transfer-Encoding: chunked\r\n"),
        std::string::npos);
    EXPECT_NE(request.find(
                  "{\"statusCode\":" + std::to_string(status)),
        std::string::npos);
    EXPECT_NE(request.find(std::string(8, '\0')), std::string::npos);
    EXPECT_NE(request.find(body), std::string::npos);
    EXPECT_TRUE(request.ends_with("0\r\n\r\n"));
}

TEST(BootstrapRuntime, ProcessesConsecutiveInvocationsUntilPollFailure)
{
    std::uint16_t port = 0;
    FileDescriptor listener = CreateListener(port);
    ASSERT_GE(listener.get(), 0);
    ASSERT_NE(port, 0);

    const pid_t pid = ::fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        ::close(listener.get());
        const std::string authority = "127.0.0.1:" + std::to_string(port);
        if (::setenv("AWS_LAMBDA_RUNTIME_API", authority.c_str(), 1) != 0) {
            _exit(126);
        }
        ::execl(MEDIAPROXY_BOOTSTRAP_PATH,
            MEDIAPROXY_BOOTSTRAP_PATH, static_cast<char*>(nullptr));
        _exit(127);
    }
    ChildProcess child{pid};

    const std::string status_event =
        R"({"version":"2.0","rawPath":"/status","rawQueryString":"","requestContext":{"http":{"method":"GET"}}})";
    const std::string status_response = ServeInvocation(
        listener.get(), "request-1", "Root=trace-1", status_event);
    ExpectStreamingResponse(
        status_response, "request-1", 200, R"({"status":"OK"})");

    const std::string bad_response = ServeInvocation(
        listener.get(), "request-2", "Root=trace-2", "{}");
    ExpectStreamingResponse(
        bad_response, "request-2", 400, "Bad request\n");

    {
        FileDescriptor final_poll = Accept(listener.get());
        ASSERT_GE(final_poll.get(), 0);
        EXPECT_TRUE(ReadHeaders(final_poll.get()).starts_with(
            "GET /2018-06-01/runtime/invocation/next HTTP/1.1\r\n"));
    }

    const int status = child.wait();
    ASSERT_NE(status, -1);
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 1);
}

} // namespace
