#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include <mediaproxy/http/response.hpp>
#include <mediaproxy/runtime/next_response.hpp>
#include <mediaproxy/runtime/streaming.hpp>

namespace {

class DiscardSink final : public mediaproxy::runtime::ByteSink {
public:
    [[nodiscard]] bool write(
        std::span<const std::byte> bytes) override
    {
        retained_bytes_ += bytes.size();
        return retained_bytes_ <= 128U * 1024U;
    }

private:
    std::size_t retained_bytes_ = 0;
};

} // namespace

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size)
{
    const auto bytes = std::as_bytes(std::span{data, size});
    mediaproxy::runtime::NextResponseParser raw_parser;
    static_cast<void>(raw_parser.feed(bytes));

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Lambda-Runtime-Aws-Request-Id: fuzz-request\r\n"
        "Lambda-Runtime-Deadline-Ms: 9999999999999\r\n"
        "Lambda-Runtime-Trace-Id: fuzz-trace\r\n"
        "Content-Length: ";
    response += std::to_string(size);
    response += "\r\n\r\n";
    if (size != 0U) {
        response.append(reinterpret_cast<const char*>(data), size);
    }
    mediaproxy::runtime::NextResponseParser framed_parser;
    static_cast<void>(framed_parser.feed(
        std::as_bytes(std::span{response})));

    mediaproxy::http::HttpResponse integration{
        .status = 200,
        .headers = {{.name = "X-Fuzz", .value = {}}},
        .body = {bytes.begin(), bytes.end()},
    };
    if (size != 0U) {
        integration.headers.front().value.assign(
            reinterpret_cast<const char*>(data), size);
    }
    DiscardSink sink;
    static_cast<void>(
        mediaproxy::runtime::write_streaming_response(sink, integration));
    return 0;
}
