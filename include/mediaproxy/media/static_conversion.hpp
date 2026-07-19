#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <mediaproxy/media/classification.hpp>
#include <mediaproxy/media/resize.hpp>

namespace mediaproxy::media {

enum class StaticConversionError {
    none,
    initialization,
    decode,
    dimensions,
    resize,
    encode,
};

struct StaticConversionResult {
    StaticConversionError error = StaticConversionError::none;
    std::vector<std::byte> body;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == StaticConversionError::none;
    }
};

[[nodiscard]] StaticConversionResult convert_static_image(
    std::span<const std::byte> body,
    OutputFormat output,
    ImageDimensions limits);

} // namespace mediaproxy::media
