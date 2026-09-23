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
        mediaproxy::media::MimeType::image_svg_xml,
        mediaproxy::media::OutputFormat::webp,
        mediaproxy::media::ImageDimensions{.width = 64, .height = 64}));
    return 0;
}
