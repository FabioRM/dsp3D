/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef DSP3D_MATERIAL_H
#define DSP3D_MATERIAL_H
#include "dsp3D_math.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float u, v;
} dsp3d_uv;
typedef struct {
    const uint16_t *pixels;
    const uint8_t *alpha; /* optional, tightly packed width*height */
    uint32_t width, height, stride;
    uint8_t repeat;
} dsp3d_texture;
enum { DSP3D_MATERIAL_UNLIT = 1u, DSP3D_MATERIAL_FACE_COLOR = 2u };
typedef struct {
    uint32_t color;
    const dsp3d_texture *texture;
    float specular, emissive;
    uint8_t opacity, flags;
} dsp3d_material;
typedef struct {
    dsp3d_mesh mesh;
    const dsp3d_uv *uv;
    const dsp3d_vec3 *face_normals;
    const uint32_t *material_indices;
    const dsp3d_material *materials;
    uint32_t material_count;
    dsp3d_bounds bounds;
    uint8_t bounds_valid;
} dsp3d_geometry;
dsp3d_result dsp3d_geometry_init(dsp3d_geometry *geometry, const dsp3d_mesh *mesh,
                                 dsp3d_vec3 *normal_storage);
dsp3d_result dsp3d_validate_geometry(const dsp3d_geometry *geometry);
dsp3d_result dsp3d_draw_geometry(dsp3d_context *ctx, const dsp3d_geometry *geometry,
                                 const dsp3d_transform *transform, dsp3d_mode mode);
dsp3d_result dsp3d_draw_matrix(dsp3d_context *ctx, const dsp3d_geometry *geometry,
                               const dsp3d_mat4 *world, dsp3d_mode mode);
/* Directional-light planar shadow, in world coordinates; output is caller-owned. */
dsp3d_result dsp3d_planar_shadow(const dsp3d_mesh *mesh, const dsp3d_mat4 *world,
                                 dsp3d_vec3 light_direction, float plane_y, dsp3d_vertex *out,
                                 uint32_t capacity);
#ifdef __cplusplus
}
#endif
#endif
