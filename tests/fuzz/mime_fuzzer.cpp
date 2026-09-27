#include <cstddef>
#include <cstdint>
#include <span>

#include <mediaproxy/media/mime.hpp>

// NOLINTNEXTLINE(readability-identifier-naming): required by libFuzzer ABI.
extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) -> int {
    const auto bytes = std::as_bytes(std::span{data, size});
    static_cast<void>(mediaproxy::media::sniff_mime(bytes));

    return 0;
}
