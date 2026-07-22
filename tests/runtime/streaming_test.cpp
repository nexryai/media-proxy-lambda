#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <mediaproxy/http/response.hpp>
#include <mediaproxy/runtime/streaming.hpp>

namespace {

class CollectingSink final : public mediaproxy::runtime::ByteSink {
public:
    bool write(std::span<const std::byte> bytes) override
    {
        const auto* characters =
            reinterpret_cast<const char*>(bytes.data());
        output.append(characters, bytes.size());
        ++writes;
        return writes != fail_on_write;
    }

    std::string output;
    std::size_t writes = 0;
    std::size_t fail_on_write = 0;
};

class BodyTrackingSink final : public mediaproxy::runtime::ByteSink {
public:
    explicit BodyTrackingSink(std::span<const std::byte> body) noexcept
        : body_begin_(reinterpret_cast<std::uintptr_t>(body.data()))
        , body_end_(body_begin_ + body.size())
    {
    }

    bool write(std::span<const std::byte> bytes) override
    {
        const std::uintptr_t begin =
            reinterpret_cast<std::uintptr_t>(bytes.data());
        if (begin >= body_begin_ && begin <= body_end_
            && bytes.size() <= body_end_ - begin) {
            body_bytes += bytes.size();
            largest_body_write = std::max(largest_body_write, bytes.size());
        }
        ++writes;
        return true;
    }

    std::size_t body_bytes = 0;
    std::size_t largest_body_write = 0;
    std::size_t writes = 0;

private:
    std::uintptr_t body_begin_ = 0;
    std::uintptr_t body_end_ = 0;
};

using mediaproxy::http::HttpHeader;
using mediaproxy::http::HttpResponse;
using mediaproxy::runtime::make_streaming_request_head;
using mediaproxy::runtime::write_streaming_error;
using mediaproxy::runtime::write_streaming_response;

TEST(RuntimeStreaming, BuildsRequiredResponseRequestHeaders)
{
    EXPECT_EQ(make_streaming_request_head("127.0.0.1:9001", "request-123"),
        "POST /2018-06-01/runtime/invocation/request-123/response HTTP/1.1\r\n"
        "Host: 127.0.0.1:9001\r\n"
        "Lambda-Runtime-Function-Response-Mode: streaming\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Type: application/vnd.awslambda.http-integration-response\r\n"
        "Trailer: Lambda-Runtime-Function-Error-Type, "
        "Lambda-Runtime-Function-Error-Body\r\n"
        "Connection: close\r\n\r\n");
    EXPECT_TRUE(make_streaming_request_head("bad\r\nhost", "id").empty());
    EXPECT_TRUE(make_streaming_request_head("host", "bad/id").empty());
    EXPECT_TRUE(make_streaming_request_head("host", "bad?id").empty());
    EXPECT_TRUE(make_streaming_request_head("host", "bad id").empty());
}

TEST(RuntimeStreaming, WritesMetadataDelimiterAndRawBinaryBody)
{
    const HttpResponse response{
        .status = 201,
        .headers = {HttpHeader{"Content-Type", "image/webp"}},
        .body = {std::byte{0}, std::byte{0xff}},
    };
    CollectingSink sink;
    ASSERT_TRUE(write_streaming_response(sink, response));

    const std::string metadata =
        "{\"statusCode\":201,\"headers\":{\"Content-Type\":\"image/webp\"}}";
    const std::string prefix =
        [] (std::size_t size) {
            constexpr char hex[] = "0123456789abcdef";
            std::string result;
            do {
                result.push_back(hex[size & 0xfU]);
                size >>= 4U;
            } while (size != 0);
            std::reverse(result.begin(), result.end());
            return result;
        }(metadata.size() + 8);
    std::string expected = prefix + "\r\n" + metadata;
    expected.append(8, '\0');
    expected += "\r\n2\r\n";
    expected.append("\0\xff", 2);
    expected += "\r\n0\r\n\r\n";
    EXPECT_EQ(sink.output, expected);
}

TEST(RuntimeStreaming, BoundsBodyChunksAndStopsOnSinkFailure)
{
    HttpResponse response{.status = 200, .headers = {},
        .body = std::vector<std::byte>(
            mediaproxy::runtime::response_chunk_bytes + 1, std::byte{'x'})};
    CollectingSink sink;
    ASSERT_TRUE(write_streaming_response(sink, response));
    EXPECT_NE(sink.output.find("10000\r\n"), std::string::npos);

    CollectingSink failing;
    failing.fail_on_write = 2;
    EXPECT_FALSE(write_streaming_response(failing, response));
}

TEST(RuntimeStreaming, StreamsBodyAboveBufferedResponseLimitInRawChunks)
{
    constexpr std::size_t buffered_response_limit = 6U * 1024U * 1024U;
    HttpResponse response{
        .status = 200,
        .headers = {},
        .body = std::vector<std::byte>(
            buffered_response_limit + 1, std::byte{0xa5}),
    };
    BodyTrackingSink sink{response.body};
    ASSERT_TRUE(write_streaming_response(sink, response));
    EXPECT_EQ(sink.body_bytes, response.body.size());
    EXPECT_LE(sink.largest_body_write,
        mediaproxy::runtime::response_chunk_bytes);
    EXPECT_GT(sink.writes, 3U);
}

TEST(RuntimeStreaming, WritesBase64ErrorTrailers)
{
    CollectingSink sink;
    const std::string body{"bad\0body", 8};
    ASSERT_TRUE(write_streaming_error(sink, "MediaConversionError",
        std::as_bytes(std::span{body})));
    EXPECT_EQ(sink.output,
        "0\r\nLambda-Runtime-Function-Error-Type: MediaConversionError\r\n"
        "Lambda-Runtime-Function-Error-Body: YmFkAGJvZHk=\r\n\r\n");
    EXPECT_FALSE(write_streaming_error(
        sink, "bad\r\ntype", std::as_bytes(std::span{body})));
}

} // namespace
