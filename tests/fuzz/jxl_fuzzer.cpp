#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <mediaproxy/media/static_conversion.hpp>

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size)
{
    const auto bytes = std::as_bytes(std::span{data, size});
    static_cast<void>(mediaproxy::media::convert_static_image(bytes,
        mediaproxy::media::MimeType::image_jxl,
        mediaproxy::media::OutputFormat::webp,
        mediaproxy::media::ImageDimensions{.width = 64, .height = 64}));

    // Keep the decoder's successful path reachable even when the corpus input
    // is not itself a JXL codestream. This 8x8 codestream is also used by the
    // focused conversion tests.
    std::array<std::uint8_t, 56> mutated{
        0xff, 0x0a, 0x41, 0x06, 0x00, 0x12, 0x88, 0x02,
        0x00, 0xb4, 0x00, 0x55, 0x0f, 0x00, 0x00, 0xa8,
        0x50, 0x19, 0x65, 0xdc, 0xe0, 0xe5, 0x5c, 0xcf,
        0x97, 0x1f, 0x3a, 0x2c, 0xa6, 0x6d, 0x5c, 0x67,
        0x68, 0xab, 0x6d, 0x0b, 0x4b, 0x12, 0x45, 0xc6,
        0xb1, 0x49, 0x3a, 0x81, 0x43, 0x92, 0x48, 0xd2,
        0x60, 0x20, 0x99, 0xc1, 0x70, 0x5a, 0x25, 0x09,
    };
    for (std::size_t index = 0; index < size; ++index) {
        mutated[index % mutated.size()] ^= data[index];
    }
    static_cast<void>(mediaproxy::media::convert_static_image(
        std::as_bytes(std::span{mutated}),
        mediaproxy::media::MimeType::image_jxl,
        mediaproxy::media::OutputFormat::webp,
        mediaproxy::media::ImageDimensions{.width = 64, .height = 64}));
    return 0;
}
