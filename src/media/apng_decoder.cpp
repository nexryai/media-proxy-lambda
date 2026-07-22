#include <mediaproxy/media/apng_decoder.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

#include <png.h>
#include <zlib.h>

namespace mediaproxy::media {
namespace {

constexpr std::array<std::byte, 8> png_signature{
    std::byte{0x89}, std::byte{0x50}, std::byte{0x4e}, std::byte{0x47},
    std::byte{0x0d}, std::byte{0x0a}, std::byte{0x1a}, std::byte{0x0a}};

[[nodiscard]] std::uint32_t read_u32(
    std::span<const std::byte> body,
    std::size_t offset) noexcept
{
    const auto byte = [&body](std::size_t index) {
        return static_cast<std::uint32_t>(
            std::to_integer<std::uint8_t>(body[index]));
    };
    return (byte(offset) << 24U) | (byte(offset + 1) << 16U)
        | (byte(offset + 2) << 8U) | byte(offset + 3);
}

void append_u32(std::vector<std::byte>& output, std::uint32_t value)
{
    output.push_back(static_cast<std::byte>((value >> 24U) & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 16U) & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
    output.push_back(static_cast<std::byte>(value & 0xffU));
}

[[nodiscard]] bool tag_at(
    std::span<const std::byte> body,
    std::size_t offset,
    std::string_view tag) noexcept
{
    return offset <= body.size() && tag.size() <= body.size() - offset
        && std::equal(tag.begin(), tag.end(), body.begin() + offset,
            [](char expected, std::byte actual) {
                return static_cast<unsigned char>(expected)
                    == std::to_integer<unsigned char>(actual);
            });
}

void append_chunk(
    std::vector<std::byte>& output,
    std::string_view type,
    std::span<const std::byte> data)
{
    append_u32(output, static_cast<std::uint32_t>(data.size()));
    const std::size_t crc_begin = output.size();
    for (const char value : type) {
        output.push_back(static_cast<std::byte>(value));
    }
    output.insert(output.end(), data.begin(), data.end());
    const auto* bytes = reinterpret_cast<const Bytef*>(
        output.data() + crc_begin);
    const auto crc = static_cast<std::uint32_t>(
        crc32(0, bytes, static_cast<uInt>(type.size() + data.size())));
    append_u32(output, crc);
}

[[nodiscard]] bool rgba_size(
    std::uint32_t width,
    std::uint32_t height,
    std::size_t& output) noexcept
{
    if (width == 0 || height == 0
        || width > std::numeric_limits<std::size_t>::max() / height) {
        return false;
    }
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    if (pixels > std::numeric_limits<std::size_t>::max() / 4) {
        return false;
    }
    output = pixels * 4;
    return true;
}

[[nodiscard]] ApngDecodeError decode_png(
    std::span<const std::byte> png,
    const ApngFrameControl& control,
    std::vector<std::byte>& rgba)
{
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_memory(
            &image, png.data(), png.size()) == 0) {
        return ApngDecodeError::png_decode;
    }
    struct ImageCleanup {
        png_image* image;
        ~ImageCleanup() { png_image_free(image); }
    } cleanup{&image};

    if (image.width != control.width || image.height != control.height) {
        return ApngDecodeError::dimensions;
    }
    image.format = PNG_FORMAT_RGBA;
    std::size_t expected_size = 0;
    if (!rgba_size(control.width, control.height, expected_size)
        || PNG_IMAGE_SIZE(image) != expected_size) {
        return ApngDecodeError::dimensions;
    }
    rgba.resize(expected_size);
    if (png_image_finish_read(
            &image, nullptr, rgba.data(), 0, nullptr)
        == 0) {
        rgba.clear();
        return ApngDecodeError::png_decode;
    }
    return ApngDecodeError::none;
}

[[nodiscard]] ApngDecodedAnimation fail(ApngDecodeError error)
{
    ApngDecodedAnimation result;
    result.error = error;
    return result;
}

} // namespace

ApngDecodedAnimation decode_apng_frames(
    std::span<const std::byte> body)
{
    const auto description = parse_apng(body);
    if (!description) {
        return fail(ApngDecodeError::parse);
    }

    std::vector<std::byte> shared_chunks;
    std::vector<std::vector<std::byte>> compressed(description.frames.size());
    std::array<std::byte, 13> ihdr{};
    bool before_first_frame = true;
    std::size_t frame_index = 0;
    std::size_t offset = png_signature.size();
    while (offset < body.size()) {
        const std::size_t length = read_u32(body, offset);
        const std::size_t type_offset = offset + 4;
        const std::size_t data_offset = offset + 8;
        const std::size_t chunk_end = data_offset + length + 4;
        const auto data = body.subspan(data_offset, length);
        if (tag_at(body, type_offset, "IHDR")) {
            std::copy(data.begin(), data.end(), ihdr.begin());
        } else if (tag_at(body, type_offset, "fcTL")) {
            before_first_frame = false;
            if (frame_index < description.frames.size()
                && !compressed[frame_index].empty()) {
                ++frame_index;
            }
        } else if (tag_at(body, type_offset, "IDAT")) {
            if (frame_index >= compressed.size()) {
                return fail(ApngDecodeError::frame_stream);
            }
            compressed[frame_index].insert(
                compressed[frame_index].end(), data.begin(), data.end());
        } else if (tag_at(body, type_offset, "fdAT")) {
            if (frame_index >= compressed.size() || data.size() < 4) {
                return fail(ApngDecodeError::frame_stream);
            }
            const auto payload = data.subspan(4);
            compressed[frame_index].insert(compressed[frame_index].end(),
                payload.begin(), payload.end());
        } else if (before_first_frame
            && !tag_at(body, type_offset, "acTL")) {
            shared_chunks.insert(shared_chunks.end(), body.begin() + offset,
                body.begin() + static_cast<std::ptrdiff_t>(chunk_end));
        }
        offset = chunk_end;
    }

    ApngDecodedAnimation result{
        .error = ApngDecodeError::none,
        .canvas_width = description.canvas_width,
        .canvas_height = description.canvas_height,
        .frames = {},
    };
    result.frames.reserve(description.frames.size());
    for (std::size_t index = 0; index < description.frames.size(); ++index) {
        if (compressed[index].empty()) {
            return fail(ApngDecodeError::frame_stream);
        }
        auto frame_ihdr = ihdr;
        const auto& control = description.frames[index];
        for (std::size_t byte = 0; byte < 4; ++byte) {
            frame_ihdr[byte] = static_cast<std::byte>(
                control.width >> (24U - static_cast<unsigned>(byte) * 8U));
            frame_ihdr[byte + 4] = static_cast<std::byte>(
                control.height >> (24U - static_cast<unsigned>(byte) * 8U));
        }

        std::vector<std::byte> png(png_signature.begin(), png_signature.end());
        append_chunk(png, "IHDR", frame_ihdr);
        png.insert(png.end(), shared_chunks.begin(), shared_chunks.end());
        append_chunk(png, "IDAT", compressed[index]);
        append_chunk(png, "IEND", {});
        ApngDecodedFrame frame{.control = control, .rgba = {}};
        const auto error = decode_png(png, control, frame.rgba);
        if (error != ApngDecodeError::none) {
            return fail(error);
        }
        result.frames.push_back(std::move(frame));
    }
    return result;
}

} // namespace mediaproxy::media
