/* SPDX-License-Identifier: BSD-3-Clause */
#include "scenes.h"
static const dsp3d_vertex triangle_vertices[] = {
    {{-1, -1, 0}, {0, 0, 1}}, {{1, -1, 0}, {0, 0, 1}}, {{0, 1, 0}, {0, 0, 1}}};
static const dsp3d_triangle triangle_faces[] = {{{0, 1, 2}, 0xf2a24a}};
const dsp3d_mesh dsp3d_example_triangle = {triangle_vertices, triangle_faces, 3, 1};
/* Separate vertices at hard edges keep normals independent for each face. */
static const dsp3d_vertex cube_vertices[] = {
    {{-1, -1, 1}, {0, 0, 1}},   {{1, -1, 1}, {0, 0, 1}},   {{1, 1, 1}, {0, 0, 1}},
    {{-1, 1, 1}, {0, 0, 1}},    {{1, -1, -1}, {0, 0, -1}}, {{-1, -1, -1}, {0, 0, -1}},
    {{-1, 1, -1}, {0, 0, -1}},  {{1, 1, -1}, {0, 0, -1}},  {{1, -1, 1}, {1, 0, 0}},
    {{1, -1, -1}, {1, 0, 0}},   {{1, 1, -1}, {1, 0, 0}},   {{1, 1, 1}, {1, 0, 0}},
    {{-1, -1, -1}, {-1, 0, 0}}, {{-1, -1, 1}, {-1, 0, 0}}, {{-1, 1, 1}, {-1, 0, 0}},
    {{-1, 1, -1}, {-1, 0, 0}},  {{-1, 1, 1}, {0, 1, 0}},   {{1, 1, 1}, {0, 1, 0}},
    {{1, 1, -1}, {0, 1, 0}},    {{-1, 1, -1}, {0, 1, 0}},  {{-1, -1, -1}, {0, -1, 0}},
    {{1, -1, -1}, {0, -1, 0}},  {{1, -1, 1}, {0, -1, 0}},  {{-1, -1, 1}, {0, -1, 0}}};
static const dsp3d_triangle cube_faces[] = {
    {{0, 1, 2}, 0xe98d3a},    {{0, 2, 3}, 0xe98d3a},    {{4, 5, 6}, 0x8b67d7},
    {{4, 6, 7}, 0x8b67d7},    {{8, 9, 10}, 0x5aa9e6},   {{8, 10, 11}, 0x5aa9e6},
    {{12, 13, 14}, 0x5bc4a0}, {{12, 14, 15}, 0x5bc4a0}, {{16, 17, 18}, 0xe9c75f},
    {{16, 18, 19}, 0xe9c75f}, {{20, 21, 22}, 0xd86179}, {{20, 22, 23}, 0xd86179}};
const dsp3d_mesh dsp3d_example_cube = {cube_vertices, cube_faces, 24, 12};
dsp3d_result dsp3d_example_scene(dsp3d_context *ctx, float angle, dsp3d_mode mode) {
    dsp3d_transform a = {{angle * 0.6f, angle, 0}, {-0.65f, 0, 0}, 0.5f};
    dsp3d_transform b = {{0, -angle, angle * 0.3f}, {0.65f, 0, -0.35f}, 0.4f};
    dsp3d_result r = dsp3d_draw(ctx, &dsp3d_example_cube, &a, mode);
    if (r != DSP3D_OK)
        return r;
    return dsp3d_draw(ctx, &dsp3d_example_triangle, &b, mode);
}
