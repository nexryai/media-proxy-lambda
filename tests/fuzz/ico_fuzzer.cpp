#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <mediaproxy/media/conversion.hpp>
#include <mediaproxy/media/mime.hpp>
#include <mediaproxy/media/resize.hpp>

namespace {

void append_u16(std::vector<std::byte>& output, std::uint16_t value)
{
    output.push_back(static_cast<std::byte>(value & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
}

void append_u32(std::vector<std::byte>& output, std::uint32_t value)
{
    output.push_back(static_cast<std::byte>(value & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 16U) & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 24U) & 0xffU));
}

void convert_ico(std::span<const std::byte> bytes)
{
    static_cast<void>(mediaproxy::media::convert_media(bytes,
        mediaproxy::media::MimeType::image_ico, false,
        mediaproxy::media::OutputFormat::webp,
        mediaproxy::media::ImageDimensions{.width = 64, .height = 64}));
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    std::size_t size)
{
    const auto bytes = std::as_bytes(std::span{data, size});
    convert_ico(bytes);

    if (size > std::numeric_limits<std::uint32_t>::max()) {
        return 0;
    }
    std::vector<std::byte> wrapped;
    wrapped.reserve(22U + size);
    append_u16(wrapped, 0);
    append_u16(wrapped, 1);
    append_u16(wrapped, 1);
    wrapped.insert(wrapped.end(), 8U, std::byte{0});
    append_u32(wrapped, static_cast<std::uint32_t>(size));
    append_u32(wrapped, 22);
    wrapped.insert(wrapped.end(), bytes.begin(), bytes.end());
    convert_ico(wrapped);
    return 0;
}
