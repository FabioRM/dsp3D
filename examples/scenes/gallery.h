/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef DSP3D_GALLERY_H
#define DSP3D_GALLERY_H
#include "dsp3D_asset.h"
#define GALLERY_COUNT 10u
typedef struct {
    dsp3d_geometry fox, monkey, cube, textured, glass, shiny, ribbon, shadow, floor;
    dsp3d_vec3 fox_normals[188], monkey_normals[968];
    dsp3d_asset compact[3];
    unsigned char workspace[65536];
    dsp3d_uv uv[24];
    uint16_t checker[256];
    dsp3d_texture texture;
    dsp3d_material paint, transparent, polished, dark, ground;
    dsp3d_node nodes[8];
    dsp3d_scene scene;
    dsp3d_keyframe keys[3];
    dsp3d_track track;
    dsp3d_animation animation;
    dsp3d_vertex rest[18], deformed[18], projected[24], floor_vertices[4];
    dsp3d_triangle ribbon_faces[16], floor_faces[2];
    dsp3d_skin_vertex weights[18];
    dsp3d_joint_palette palette[2];
    dsp3d_mat4 inverse_bind[2];
    uint32_t joints[2];
    uint32_t selected, active_lod;
    float time;
} gallery;
extern const char *const gallery_names[GALLERY_COUNT];
extern const char *const gallery_descriptions[GALLERY_COUNT];
dsp3d_result gallery_init(gallery *state);
void gallery_select(gallery *state, unsigned index);
void gallery_step(gallery *state, unsigned index, float seconds);
dsp3d_result gallery_draw(gallery *state, dsp3d_context *context, unsigned index,
                         const dsp3d_transform *pose, dsp3d_mode mode);
dsp3d_result gallery_pick(gallery *state, const dsp3d_context *context, float x, float y,
                         uint32_t *node, dsp3d_hit *hit);
#endif
