/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef DSP3D_GEOMETRY_H
#define DSP3D_GEOMETRY_H
#include "dsp3D.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    dsp3d_vec3 min, max, center;
    float radius;
} dsp3d_bounds;
typedef struct {
    int16_t position[3], normal[3];
} dsp3d_compact_vertex;
typedef struct {
    const dsp3d_mesh *mesh;
    float minimum_distance;
} dsp3d_lod;
typedef struct {
    uint32_t target_us, filtered_us, stable_frames;
    uint8_t divisor;
} dsp3d_quality;
/* Caller supplies triangle_count normals. Pass NULL to compute bounds only. */
dsp3d_result dsp3d_prepare_mesh(const dsp3d_mesh *mesh, dsp3d_vec3 *face_normals,
                                dsp3d_bounds *bounds);
dsp3d_result dsp3d_pack_vertices(const dsp3d_mesh *mesh, dsp3d_compact_vertex *out,
                                 uint32_t capacity, dsp3d_bounds *bounds);
dsp3d_result dsp3d_unpack_vertices(const dsp3d_compact_vertex *packed, uint32_t count,
                                   const dsp3d_bounds *bounds, dsp3d_vertex *out,
                                   uint32_t capacity);
const dsp3d_mesh *dsp3d_select_lod(const dsp3d_lod *levels, uint32_t count, float distance);
void dsp3d_quality_init(dsp3d_quality *quality, uint32_t target_us);
/* Hysteresis and 30-frame settling prevent rapid quality oscillation. */
uint8_t dsp3d_quality_update(dsp3d_quality *quality, uint32_t measured_us);
#ifdef __cplusplus
}
#endif
#endif
