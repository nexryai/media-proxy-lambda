#include <cstddef>
#include <cstdint>
#include <span>

#include <mediaproxy/media/mime.hpp>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size)
{
    const auto bytes = std::as_bytes(std::span{data, size});
    static_cast<void>(mediaproxy::media::sniff_mime(bytes));
    return 0;
}
