/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef DSP3D_SCENE_H
#define DSP3D_SCENE_H
#include "dsp3D_material.h"
#ifdef __cplusplus
extern "C" {
#endif
#define DSP3D_SCENE_MAX_NODES 128u
#define DSP3D_SKIN_MAX_JOINTS 64u
typedef struct {
    int32_t parent;
    uint32_t mesh_id;
    const dsp3d_geometry *geometry;
    dsp3d_vec3 translation, scale;
    dsp3d_quat rotation;
    dsp3d_mat4 world;
    uint8_t visible;
} dsp3d_node;
typedef struct {
    dsp3d_node *nodes;
    uint32_t count;
} dsp3d_scene;
typedef struct {
    float time;
    dsp3d_vec3 translation;
    dsp3d_quat rotation;
    dsp3d_vec3 scale;
} dsp3d_keyframe;
typedef struct {
    uint32_t node, first_key, key_count, step;
} dsp3d_track;
typedef struct {
    dsp3d_track *tracks;
    dsp3d_keyframe *keys;
    uint32_t track_count, key_count;
    float duration;
    uint8_t loop;
} dsp3d_animation;
typedef struct {
    uint16_t joints[4];
    float weights[4];
} dsp3d_skin_vertex;
typedef struct {
    dsp3d_mat4 position, normal;
} dsp3d_joint_palette;
dsp3d_result dsp3d_scene_update(dsp3d_scene *scene);
dsp3d_result dsp3d_scene_render(dsp3d_context *ctx, dsp3d_scene *scene, dsp3d_mode mode);
dsp3d_result dsp3d_scene_animate(dsp3d_scene *scene, const dsp3d_animation *animation,
                                 float seconds);
dsp3d_result dsp3d_scene_pick(const dsp3d_context *ctx, const dsp3d_scene *scene, float x, float y,
                              uint32_t *node, dsp3d_hit *hit);
dsp3d_result dsp3d_scene_write(const dsp3d_scene *scene, const dsp3d_animation *animation,
                               void *data, size_t capacity, size_t *written);
dsp3d_result dsp3d_scene_read(const void *data, size_t size, dsp3d_scene *scene,
                              uint32_t node_capacity, dsp3d_animation *animation,
                              uint32_t track_capacity, uint32_t key_capacity,
                              const dsp3d_geometry *const *catalog, uint32_t catalog_count);
dsp3d_result dsp3d_skin_palette(const dsp3d_scene *scene, const uint32_t *joints,
                                const dsp3d_mat4 *inverse_bind, uint32_t count,
                                const dsp3d_mat4 *mesh_inverse, dsp3d_joint_palette *palette);
dsp3d_result dsp3d_skin_vertices(const dsp3d_vertex *source, const dsp3d_skin_vertex *skin,
                                 uint32_t count, const dsp3d_joint_palette *palette,
                                 uint32_t joint_count, dsp3d_vertex *destination,
                                 uint32_t capacity);
#ifdef __cplusplus
}
#endif
#endif
