#include <array>
#include <charconv>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <netinet/in.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
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

[[nodiscard]] std::optional<std::string> DecodeChunkedEntity(
    std::string_view encoded)
{
    std::string decoded;
    std::size_t cursor = 0;
    while (cursor < encoded.size()) {
        const std::size_t line_end = encoded.find("\r\n", cursor);
        if (line_end == std::string_view::npos || line_end == cursor) {
            return std::nullopt;
        }
        std::size_t chunk_size = 0;
        const char* const first = encoded.data() + cursor;
        const char* const last = encoded.data() + line_end;
        const auto [end, error] =
            std::from_chars(first, last, chunk_size, 16);
        if (error != std::errc{} || end != last) {
            return std::nullopt;
        }
        cursor = line_end + 2;
        if (chunk_size == 0U) {
            if (encoded.substr(cursor) != "\r\n") {
                return std::nullopt;
            }
            return decoded;
        }
        if (chunk_size > encoded.size() - cursor
            || encoded.size() - cursor - chunk_size < 2U
            || encoded.substr(cursor + chunk_size, 2) != "\r\n") {
            return std::nullopt;
        }
        decoded.append(encoded.substr(cursor, chunk_size));
        cursor += chunk_size + 2;
    }
    return std::nullopt;
}

[[nodiscard]] bool InstallNoFileOpenFilter() noexcept
{
    const sock_filter filter[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
            offsetof(seccomp_data, nr)),
#ifdef __NR_open
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_open, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_openat, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#ifdef __NR_openat2
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_openat2, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };
    const sock_fprog program{
        .len = static_cast<unsigned short>(std::size(filter)),
        .filter = const_cast<sock_filter*>(filter),
    };
    return ::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == 0
        && ::prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program) == 0;
}

[[nodiscard]] std::string ServeInvocation(
    int listener,
    std::string_view authority,
    std::string_view request_id,
    std::string_view trace_id,
    std::string_view event)
{
    FileDescriptor poll = Accept(listener);
    EXPECT_GE(poll.get(), 0);
    const std::string poll_request = ReadHeaders(poll.get());
    EXPECT_EQ(poll_request,
        "GET /2018-06-01/runtime/invocation/next HTTP/1.1\r\nHost: "
            + std::string{authority}
            + "\r\nConnection: close\r\n\r\n");
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
    std::string_view authority,
    std::string_view request_id,
    unsigned int status,
    std::string_view content_type,
    std::string_view body)
{
    const std::string expected_head =
        "POST /2018-06-01/runtime/invocation/" + std::string{request_id}
        + "/response HTTP/1.1\r\nHost: " + std::string{authority}
        + "\r\nLambda-Runtime-Function-Response-Mode: streaming"
          "\r\nTransfer-Encoding: chunked"
          "\r\nContent-Type: "
          "application/vnd.awslambda.http-integration-response"
          "\r\nTrailer: Lambda-Runtime-Function-Error-Type, "
          "Lambda-Runtime-Function-Error-Body"
          "\r\nConnection: close\r\n\r\n";
    const std::size_t head_end = request.find("\r\n\r\n");
    ASSERT_NE(head_end, std::string::npos);
    EXPECT_EQ(request.substr(0, head_end + 4), expected_head);

    const auto decoded = DecodeChunkedEntity(
        std::string_view{request}.substr(head_end + 4));
    ASSERT_TRUE(decoded.has_value());
    std::string expected = "{\"statusCode\":" + std::to_string(status)
        + ",\"headers\":{\"Content-Type\":\""
        + std::string{content_type} + "\"}}";
    expected.append(8, '\0');
    expected.append(body);
    EXPECT_EQ(*decoded, expected);
}

TEST(BootstrapRuntime, ProcessesConsecutiveInvocationsWithoutOpeningFiles)
{
    std::uint16_t port = 0;
    FileDescriptor listener = CreateListener(port);
    ASSERT_GE(listener.get(), 0);
    ASSERT_NE(port, 0);
    const std::string authority = "127.0.0.1:" + std::to_string(port);

    const pid_t pid = ::fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        ::close(listener.get());
        if (::setenv("AWS_LAMBDA_RUNTIME_API", authority.c_str(), 1) != 0) {
            _exit(126);
        }
        if (!InstallNoFileOpenFilter()) {
            _exit(125);
        }
        ::execl(MEDIAPROXY_BOOTSTRAP_PATH,
            MEDIAPROXY_BOOTSTRAP_PATH, static_cast<char*>(nullptr));
        _exit(127);
    }
    ChildProcess child{pid};

    const std::string status_event =
        R"({"version":"2.0","rawPath":"/status","rawQueryString":"","requestContext":{"http":{"method":"GET"}}})";
    const std::string status_response = ServeInvocation(
        listener.get(), authority, "request-1", "Root=trace-1", status_event);
    ExpectStreamingResponse(
        status_response, authority, "request-1", 200, "application/json",
        R"({"status":"OK"})");

    const std::string bad_response = ServeInvocation(
        listener.get(), authority, "request-2", "Root=trace-2", "{}");
    ExpectStreamingResponse(bad_response, authority, "request-2", 400,
        "text/plain; charset=utf-8", "Bad request\n");

    {
        FileDescriptor final_poll = Accept(listener.get());
        ASSERT_GE(final_poll.get(), 0);
        EXPECT_EQ(ReadHeaders(final_poll.get()),
            "GET /2018-06-01/runtime/invocation/next HTTP/1.1\r\nHost: "
                + authority + "\r\nConnection: close\r\n\r\n");
    }

    const int status = child.wait();
    ASSERT_NE(status, -1);
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 1);
}

} // namespace
