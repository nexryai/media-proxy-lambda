#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace mediaproxy::media {

enum class ApngConversionError {
    none,
    decode,
    base_frame,
    dimensions,
    composition,
    picture,
    encoder,
};

struct ApngConversionResult {
    ApngConversionError error = ApngConversionError::none;
    std::vector<std::byte> body;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == ApngConversionError::none;
    }
};

[[nodiscard]] ApngConversionResult convert_apng_to_webp(
    std::span<const std::byte> body,
    std::uint32_t target_width,
    std::uint32_t target_height);

} // namespace mediaproxy::media
