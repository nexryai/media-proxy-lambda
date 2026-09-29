#pragma once

#include <libheif/heif.h>
#include <libheif/heif_sequences.h>

namespace mediaproxy::media {

// Returns false for ambiguous or invalid alpha-track references.
[[nodiscard]] auto find_avif_alpha_track(heif_context *context, heif_track *visual, heif_track **alpha) -> bool;

// Decode the matching auxiliary sample and place its luma in an RGBA frame.
[[nodiscard]] auto apply_avif_alpha_frame(heif_track *alpha, heif_image *rgba, const heif_decoding_options *options) -> bool;

} // namespace mediaproxy::media
