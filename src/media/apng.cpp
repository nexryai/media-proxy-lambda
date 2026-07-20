#include <mediaproxy/media/apng.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <zlib.h>

namespace mediaproxy::media {
namespace {

constexpr std::array<std::byte, 8> png_signature{
    std::byte{0x89}, std::byte{0x50}, std::byte{0x4e}, std::byte{0x47},
    std::byte{0x0d}, std::byte{0x0a}, std::byte{0x1a}, std::byte{0x0a}};
constexpr std::size_t maximum_chunk_length = 10U * 1024U * 1024U;

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

[[nodiscard]] std::uint16_t read_u16(
    std::span<const std::byte> body,
    std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(
        (std::to_integer<std::uint8_t>(body[offset]) << 8U)
        | std::to_integer<std::uint8_t>(body[offset + 1]));
}

[[nodiscard]] bool tag_at(
    std::span<const std::byte> body,
    std::size_t offset,
    std::string_view tag) noexcept
{
    if (offset > body.size() || tag.size() > body.size() - offset) {
        return false;
    }
    for (std::size_t index = 0; index < tag.size(); ++index) {
        if (std::to_integer<unsigned char>(body[offset + index])
            != static_cast<unsigned char>(tag[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] ApngDescription fail(ApngParseError error)
{
    ApngDescription result;
    result.error = error;
    return result;
}

[[nodiscard]] bool has_png_signature(
    std::span<const std::byte> body) noexcept
{
    return body.size() >= png_signature.size()
        && std::equal(png_signature.begin(), png_signature.end(), body.begin());
}

} // namespace

ApngClassification classify_apng(
    std::span<const std::byte> body) noexcept
{
    if (body.size() <= 41 || !has_png_signature(body)
        || !tag_at(body, 37, "acTL")) {
        return ApngClassification::not_apng;
    }
    return body.size() > 64 && tag_at(body, 57, "PLTE")
        ? ApngClassification::palette
        : ApngClassification::animated;
}

ApngDescription parse_apng(std::span<const std::byte> body)
{
    if (!has_png_signature(body)) {
        return fail(ApngParseError::signature);
    }

    ApngDescription result;
    bool have_ihdr = false;
    bool have_actl = false;
    std::size_t offset = png_signature.size();
    while (offset < body.size()) {
        constexpr std::size_t chunk_overhead = 12;
        constexpr std::size_t length_field_size = 4;
        if (body.size() - offset < length_field_size) {
            return fail(ApngParseError::truncated_chunk);
        }
        const std::size_t length = read_u32(body, offset);
        if (length > maximum_chunk_length) {
            return fail(ApngParseError::chunk_length);
        }
        if (body.size() - offset < chunk_overhead) {
            return fail(ApngParseError::truncated_chunk);
        }
        if (length > body.size() - offset - chunk_overhead) {
            return fail(ApngParseError::truncated_chunk);
        }
        const std::size_t type_offset = offset + 4;
        const std::size_t data_offset = offset + 8;
        const std::size_t crc_offset = data_offset + length;
        const auto* crc_bytes = reinterpret_cast<const Bytef*>(
            body.data() + type_offset);
        const auto computed_crc = static_cast<std::uint32_t>(
            crc32(0, crc_bytes, static_cast<uInt>(length + 4)));
        if (computed_crc != read_u32(body, crc_offset)) {
            return fail(ApngParseError::crc);
        }

        if (tag_at(body, type_offset, "IHDR")) {
            if (have_ihdr || length != 13) {
                return fail(ApngParseError::ihdr);
            }
            result.canvas_width = read_u32(body, data_offset);
            result.canvas_height = read_u32(body, data_offset + 4);
            if (result.canvas_width == 0 || result.canvas_height == 0) {
                return fail(ApngParseError::ihdr);
            }
            have_ihdr = true;
        } else if (tag_at(body, type_offset, "acTL")) {
            if (!have_ihdr || have_actl || length != 8) {
                return fail(ApngParseError::animation_control);
            }
            result.declared_frames = read_u32(body, data_offset);
            result.loop_count = read_u32(body, data_offset + 4);
            if (result.declared_frames == 0) {
                return fail(ApngParseError::animation_control);
            }
            have_actl = true;
        } else if (tag_at(body, type_offset, "fcTL")) {
            if (!have_ihdr || !have_actl || length != 26) {
                return fail(ApngParseError::frame_control);
            }
            ApngFrameControl frame{
                .sequence = read_u32(body, data_offset),
                .width = read_u32(body, data_offset + 4),
                .height = read_u32(body, data_offset + 8),
                .x_offset = read_u32(body, data_offset + 12),
                .y_offset = read_u32(body, data_offset + 16),
                .delay_numerator = read_u16(body, data_offset + 20),
                .delay_denominator = read_u16(body, data_offset + 22),
                .dispose = std::to_integer<std::uint8_t>(body[data_offset + 24]),
                .blend = std::to_integer<std::uint8_t>(body[data_offset + 25]),
            };
            if (frame.width == 0 || frame.height == 0
                || frame.x_offset > result.canvas_width
                || frame.width > result.canvas_width - frame.x_offset
                || frame.y_offset > result.canvas_height
                || frame.height > result.canvas_height - frame.y_offset) {
                return fail(ApngParseError::frame_rectangle);
            }
            if (frame.delay_denominator == 0) {
                return fail(ApngParseError::delay_denominator);
            }
            if (frame.dispose > 2) {
                return fail(ApngParseError::dispose_operation);
            }
            if (frame.blend > 1) {
                return fail(ApngParseError::blend_operation);
            }
            result.frames.push_back(frame);
        }

        offset = crc_offset + 4;
    }
    if (!have_ihdr || !have_actl
        || result.frames.size() != result.declared_frames) {
        return fail(ApngParseError::animation_control);
    }
    return result;
}

} // namespace mediaproxy::media
