/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016-2026 Fabio Angeletti */
#ifndef DSP3D_H
#define DSP3D_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define DSP3D_VERSION "2.0.0"
#define DSP3D_MODEL_VERSION 1u
typedef struct {
    float x, y, z;
} dsp3d_vec3;
typedef struct {
    dsp3d_vec3 position, normal;
} dsp3d_vertex;
typedef struct {
    uint32_t index[3], color;
} dsp3d_triangle; /* 0x00RRGGBB */
typedef struct {
    const dsp3d_vertex *vertices;
    const dsp3d_triangle *triangles;
    uint32_t vertex_count, triangle_count;
} dsp3d_mesh;
typedef enum { DSP3D_FLAT, DSP3D_GOURAUD, DSP3D_WIREFRAME, DSP3D_POINTS } dsp3d_mode;
typedef enum {
    DSP3D_OK,
    DSP3D_INVALID_ARGUMENT,
    DSP3D_INVALID_MODEL,
    DSP3D_UNSUPPORTED_VERSION,
    DSP3D_CHECKSUM_ERROR,
    DSP3D_INSUFFICIENT_MEMORY
} dsp3d_result;
typedef struct {
    dsp3d_vec3 eye, target, up;
    float vertical_fov, near_plane, far_plane; /* radians; positive distances */
} dsp3d_camera;
typedef struct {
    dsp3d_vec3 rotation, position;
    float scale;
} dsp3d_transform;
typedef struct {
    float x, y, z, light;
} dsp3d_cached_vertex;
typedef struct {
    uint32_t transformed_vertices, submitted_triangles, rasterized_triangles, shaded_pixels;
} dsp3d_stats;
typedef struct {
    uint16_t *color; /* RGB565; width pixels per row */
    float *depth;    /* reciprocal camera depth; cleared to zero */
    uint32_t width, height;
    dsp3d_cached_vertex *scratch;
    uint32_t scratch_count;
    dsp3d_camera camera;
    dsp3d_vec3 light_direction; /* world-space direction toward the light */
    float ambient;
    uint8_t backface_culling;
    dsp3d_stats stats;
    uint16_t palette[256];
    uint32_t palette_color;
    uint8_t palette_valid;
    uint8_t depth_initialized;
    uint32_t dirty_x0, dirty_y0, dirty_x1, dirty_y1;
} dsp3d_context;
/* No allocation, global renderer state, HAL, or display dependency. */
dsp3d_result dsp3d_init(dsp3d_context *ctx, uint32_t width, uint32_t height, uint16_t *color,
                        float *depth, dsp3d_cached_vertex *scratch, uint32_t scratch_count);
void dsp3d_begin_frame(dsp3d_context *ctx, uint16_t background);
/* Reset depth/statistics when a platform clears color using DMA. */
void dsp3d_clear_depth(dsp3d_context *ctx);
dsp3d_result dsp3d_draw(dsp3d_context *ctx, const dsp3d_mesh *mesh,
                        const dsp3d_transform *transform, dsp3d_mode mode);
/* Validate once before drawing a caller-created mesh. Binary loading does this. */
dsp3d_result dsp3d_validate_mesh(const dsp3d_mesh *mesh);
/* Zero-copy v1 little-endian model; data must be 4-byte aligned and outlive mesh. */
dsp3d_result dsp3d_model_open(const void *data, size_t size, dsp3d_mesh *mesh);
uint32_t dsp3d_crc32(const void *data, size_t size);
uint16_t dsp3d_rgb565(uint32_t rgb);
const char *dsp3d_result_string(dsp3d_result result);
#ifdef __cplusplus
}
#endif
#endif
