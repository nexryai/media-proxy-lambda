#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MpResvgTree MpResvgTree;

typedef struct MpResvgSize {
    uint32_t width;
    uint32_t height;
} MpResvgSize;

int32_t mp_resvg_parse(const uint8_t* data, size_t data_len,
    const uint8_t* font, size_t font_len, MpResvgTree** output_tree,
    MpResvgSize* output_size);
int32_t mp_resvg_render(const MpResvgTree* tree, uint32_t width,
    uint32_t height, uint8_t* pixels, size_t pixels_len);
void mp_resvg_tree_destroy(MpResvgTree* tree);

#ifdef __cplusplus
}
#endif
