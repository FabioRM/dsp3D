/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef DSP3D_ASSET_H
#define DSP3D_ASSET_H
#include "dsp3D_scene.h"
#ifdef __cplusplus
extern "C" {
#endif
#define DSP3D_ASSET_VERSION 2u
typedef struct {
    dsp3d_geometry geometry;
    const dsp3d_skin_vertex *skin;
    const dsp3d_mat4 *inverse_bind;
    const uint32_t *joint_nodes;
    uint32_t joint_count, texture_count;
    const char (*material_names)[32];
    const uint16_t *thumbnail;
    uint32_t thumbnail_width, thumbnail_height;
} dsp3d_asset;
/* data/workspace remain caller-owned and must outlive the resulting asset.
 * v1 models need no workspace; v2 compact vertices and pointer-bearing material
 * descriptors are decoded into the supplied arena. No heap is used. */
dsp3d_result dsp3d_asset_open(const void *data, size_t size, void *workspace, size_t capacity,
                              dsp3d_asset *asset);
#ifdef __cplusplus
}
#endif
#endif
