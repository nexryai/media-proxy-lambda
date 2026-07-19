#pragma once

#include <cstddef>

namespace mediaproxy::media {

inline constexpr int vips_worker_concurrency = 1;
inline constexpr std::size_t vips_cache_maximum_memory = 8U * 1024U * 1024U;
inline constexpr int vips_cache_maximum_entries = 32;
inline constexpr int vips_cache_maximum_files = 32;

[[nodiscard]] bool initialize_vips() noexcept;

} // namespace mediaproxy::media
