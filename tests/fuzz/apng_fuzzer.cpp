#include <cstddef>
#include <cstdint>
#include <span>

#include <mediaproxy/media/apng.hpp>
#include <mediaproxy/media/apng_decoder.hpp>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size)
{
    const auto bytes = std::as_bytes(std::span{data, size});
    static_cast<void>(mediaproxy::media::classify_apng(bytes));
    static_cast<void>(mediaproxy::media::parse_apng(bytes));
    static_cast<void>(mediaproxy::media::decode_apng_frames(bytes));
    return 0;
}
