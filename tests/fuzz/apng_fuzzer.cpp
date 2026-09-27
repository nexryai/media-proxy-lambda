#include <cstddef>
#include <cstdint>
#include <span>

#include <mediaproxy/media/apng.hpp>
#include <mediaproxy/media/apng_decoder.hpp>

// NOLINTNEXTLINE(readability-identifier-naming): required by libFuzzer ABI.
extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) -> int {
    const auto bytes = std::as_bytes(std::span{data, size});
    static_cast<void>(mediaproxy::media::classify_apng(bytes));
    static_cast<void>(mediaproxy::media::parse_apng(bytes));
    static_cast<void>(mediaproxy::media::decode_apng_frames(bytes));

    return 0;
}
