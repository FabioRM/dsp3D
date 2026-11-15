/* SPDX-License-Identifier: BSD-3-Clause */
#include "dsp3D.h"
#include "dsp3D_geometry.h"
#include "dsp3D_math.h"
#include "dsp3D_material.h"
#include "dsp3D_scene.h"
#include "metamask.h"
#include "suzanne.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define W 96
#define H 64
static struct {
    uint32_t before;
    uint16_t data[W * H];
    uint32_t after;
} color;
static struct {
    uint32_t before;
    float data[W * H];
    uint32_t after;
} depth;
static dsp3d_cached_vertex scratch[4096];
static dsp3d_context ctx;
static dsp3d_transform identity = {{0, 0, 0}, {0, 0, 0}, 1};
static dsp3d_vertex vertices[] = {
    {{-1, -1, 0}, {0, 0, 1}}, {{1, -1, 0}, {0, 0, 1}}, {{0, 1, 0}, {0, 0, 1}}};
static dsp3d_triangle triangles[] = {{{0, 1, 2}, 0xff0000}};
static dsp3d_mesh triangle = {vertices, triangles, 3, 1};
static void guards(void) {
    assert(color.before == 0x12345678 && color.after == 0x12345678);
    assert(depth.before == 0x12345678 && depth.after == 0x12345678);
}
static void init(void) {
    color.before = color.after = depth.before = depth.after = 0x12345678;
    assert(dsp3d_init(&ctx, W, H, color.data, depth.data, scratch, 4096) == DSP3D_OK);
    ctx.ambient = 1;
    dsp3d_begin_frame(&ctx, 0);
}
static void put32(unsigned char *p, uint32_t n) {
    for (int i = 0; i < 4; i++)
        p[i] = (unsigned char)(n >> (i * 8));
}
static void test_models(void) {
    assert(dsp3d_validate_mesh(&metamask) == DSP3D_OK);
    assert(dsp3d_validate_mesh(&suzanne) == DSP3D_OK);
    const char *paths[] = {"sdcard/models/metamask.d3m", "sdcard/models/suzanne.d3m"};
    for (unsigned i = 0; i < 2; i++) {
        FILE *f = fopen(paths[i], "rb");
        assert(f);
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        rewind(f);
        unsigned char *data = malloc((size_t)n);
        assert(data);
        assert(fread(data, 1, (size_t)n, f) == (size_t)n);
        fclose(f);
        dsp3d_mesh m;
        assert(dsp3d_model_open(data, (size_t)n, &m) == DSP3D_OK);
        assert(m.triangle_count == (i ? 968u : 188u));
        for (size_t cut = 0; cut < 32; cut++)
            assert(dsp3d_model_open(data, cut, &m) == DSP3D_INVALID_MODEL);
        assert(dsp3d_model_open(data, (size_t)n - 1, &m) == DSP3D_INVALID_MODEL);
        assert(dsp3d_model_open(data + 1, (size_t)n - 1, &m) == DSP3D_INVALID_ARGUMENT);
        data[n - 1] ^= 1;
        assert(dsp3d_model_open(data, (size_t)n, &m) == DSP3D_CHECKSUM_ERROR);
        data[n - 1] ^= 1;
        put32(data + 4, 999);
        assert(dsp3d_model_open(data, (size_t)n, &m) == DSP3D_UNSUPPORTED_VERSION);
        put32(data + 4, 1);
        put32(data + 32, 0x7fc00000);
        put32(data + 20, dsp3d_crc32(data + 32, (size_t)n - 32));
        assert(dsp3d_model_open(data, (size_t)n, &m) == DSP3D_INVALID_MODEL);
        assert(!m.vertices);
        free(data);
    }
    assert(dsp3d_crc32("123456789", 9) == 0xcbf43926u);
}
static void test_depth_and_culling(void) {
    init();
    assert(dsp3d_draw(&ctx, &triangle, &identity, DSP3D_FLAT) == DSP3D_OK);
    assert(ctx.stats.shaded_pixels > 100);
    uint16_t snapshot[W * H];
    memcpy(snapshot, color.data, sizeof(snapshot));
    dsp3d_transform behind = identity;
    behind.position.z = -1;
    assert(dsp3d_draw(&ctx, &triangle, &behind, DSP3D_FLAT) == DSP3D_OK);
    assert(!memcmp(snapshot, color.data, sizeof(snapshot)));
    dsp3d_begin_frame(&ctx, 0);
    triangles[0].index[1] = 2;
    triangles[0].index[2] = 1;
    assert(dsp3d_draw(&ctx, &triangle, &identity, DSP3D_FLAT) == DSP3D_OK);
    assert(ctx.stats.shaded_pixels == 0);
    triangles[0].index[1] = 1;
    triangles[0].index[2] = 2;
    for (int mode = 0; mode < 4; mode++) {
        dsp3d_begin_frame(&ctx, 0);
        assert(dsp3d_draw(&ctx, &suzanne, &identity, (dsp3d_mode)mode) == DSP3D_OK);
        assert(ctx.stats.shaded_pixels > 0);
        assert(ctx.stats.transformed_vertices == suzanne.vertex_count);
        guards();
    }
}
static void test_clipping_and_normals(void) {
    init();
    ctx.backface_culling = 0;
    dsp3d_transform t = identity;
    t.position.z = 3.7f;
    assert(dsp3d_draw(&ctx, &triangle, &t, DSP3D_GOURAUD) == DSP3D_OK);
    assert(ctx.stats.shaded_pixels > 0);
    guards();
    dsp3d_begin_frame(&ctx, 0);
    t.position.z = 5;
    assert(dsp3d_draw(&ctx, &triangle, &t, DSP3D_FLAT) == DSP3D_OK);
    assert(ctx.stats.shaded_pixels == 0);
    t = identity;
    ctx.ambient = 0.2f;
    assert(dsp3d_draw(&ctx, &triangle, &t, DSP3D_GOURAUD) == DSP3D_OK);
    float lighting = scratch[0].light;
    t.position.x = 0.8f;
    t.position.z = -2;
    assert(dsp3d_draw(&ctx, &triangle, &t, DSP3D_GOURAUD) == DSP3D_OK);
    assert(fabsf(scratch[0].light - lighting) < 0.001f);
    /* Deterministic clipping stress, including vertices crossing the camera plane. */
    uint32_t state = 12345;
    for (unsigned i = 0; i < 1000; i++) {
        for (unsigned j = 0; j < 3; j++) {
            state = state * 1664525u + 1013904223u;
            vertices[j].position.x = (float)(state % 2000) / 100 - 10;
            state = state * 1664525u + 1013904223u;
            vertices[j].position.y = (float)(state % 2000) / 100 - 10;
            state = state * 1664525u + 1013904223u;
            vertices[j].position.z = (float)(state % 2000) / 100 - 10;
        }
        dsp3d_begin_frame(&ctx, 0);
        assert(dsp3d_draw(&ctx, &triangle, &identity, (dsp3d_mode)(i % 4)) == DSP3D_OK);
        guards();
        for (unsigned j = 0; j < W * H; j++)
            assert(isfinite(depth.data[j]) && depth.data[j] >= 0);
    }
    t = identity;
    t.scale = NAN;
    assert(dsp3d_draw(&ctx, &triangle, &t, DSP3D_FLAT) == DSP3D_INVALID_ARGUMENT);
    ctx.camera.target = ctx.camera.eye;
    assert(dsp3d_draw(&ctx, &triangle, &identity, DSP3D_FLAT) == DSP3D_INVALID_ARGUMENT);
    init();
    ctx.scratch_count = 2;
    assert(dsp3d_draw(&ctx, &triangle, &identity, DSP3D_FLAT) == DSP3D_INSUFFICIENT_MEMORY);
}
static void test_shared_edges_and_dirty_clear(void) {
    init();
    ctx.camera.eye = (dsp3d_vec3){0, 0, 0};
    ctx.camera.target = (dsp3d_vec3){0, 0, -1};
    ctx.camera.vertical_fov = 1.5707963268f;
    const dsp3d_vertex v[] = {{{-1, -0.5f, -1}, {0, 0, 1}},
                              {{1, -0.5f, -1}, {0, 0, 1}},
                              {{1, 0.5f, -1}, {0, 0, 1}},
                              {{-1, 0.5f, -1}, {0, 0, 1}}};
    const dsp3d_triangle f[] = {{{0, 1, 2}, 0xffffff}, {{0, 2, 3}, 0xffffff}};
    const dsp3d_mesh quad = {v, f, 4, 2};
    assert(dsp3d_draw(&ctx, &quad, &identity, DSP3D_FLAT) == DSP3D_OK);
    assert(ctx.stats.shaded_pixels == 64 * 32);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            assert((color.data[y * W + x] != 0) == (x >= 16 && x < 80 && y >= 16 && y < 48));
    dsp3d_begin_frame(&ctx, 0);
    for (unsigned i = 0; i < W * H; i++)
        assert(depth.data[i] == 0);
    dsp3d_begin_frame(&ctx, 0); /* Empty previous frame must remain safe. */
    for (unsigned i = 0; i < W * H; i++)
        assert(depth.data[i] == 0);
    guards();
}
static void test_viewport_stride_and_scale(void) {
    uint16_t pixels[15 * 10];
    float z[17 * 10];
    dsp3d_cached_vertex cache[3];
    dsp3d_context c;
    for (unsigned i = 0; i < 150; i++)
        pixels[i] = 0xdead;
    for (unsigned i = 0; i < 170; i++)
        z[i] = -7;
    assert(dsp3d_init(&c, 12, 10, pixels, z, cache, 3) == DSP3D_OK);
    assert(dsp3d_bind_target(&c, 12, 10, pixels, 15, z, 17) == DSP3D_OK);
    assert(dsp3d_set_viewport(&c, (dsp3d_rect){2, 2, 8, 6}) == DSP3D_OK);
    assert(dsp3d_set_scissor(&c, (dsp3d_rect){4, 3, 4, 3}) == DSP3D_OK);
    assert(dsp3d_set_viewport(&c, (dsp3d_rect){UINT32_MAX, 0, 1, 1}) == DSP3D_INVALID_ARGUMENT);
    dsp3d_begin_frame(&c, 0);
    c.ambient = 1;
    const dsp3d_vertex v[] = {
        {{-2, -2, 0}, {0, 0, 1}}, {{2, -2, 0}, {0, 0, 1}}, {{0, 2, 0}, {0, 0, 1}}};
    const dsp3d_triangle f[] = {{{0, 1, 2}, 0xffffff}};
    const dsp3d_mesh m = {v, f, 3, 1};
    assert(dsp3d_draw(&c, &m, &identity, DSP3D_FLAT) == DSP3D_OK);
    assert(c.stats.shaded_pixels > 0);
    for (unsigned y = 0; y < 10; y++) {
        for (unsigned x = 0; x < 15; x++)
            if (!(x >= 4 && x < 8 && y >= 3 && y < 6))
                assert(pixels[y * 15 + x] == 0xdead);
        for (unsigned x = 12; x < 17; x++)
            assert(z[y * 17 + x] == -7);
    }
    dsp3d_begin_frame(&c, 0);
    for (unsigned y = 0; y < 10; y++)
        for (unsigned x = 0; x < 12; x++)
            assert(z[y * 17 + x] == 0);
    uint16_t source[] = {1, 2, 99, 3, 4, 99}, output[24];
    for (unsigned i = 0; i < 24; i++)
        output[i] = 55;
    assert(dsp3d_scale_rgb565(source, 2, 2, 3, output, 4, 4, 6) == DSP3D_OK);
    for (unsigned y = 0; y < 4; y++)
        for (unsigned x = 0; x < 6; x++)
            assert(output[y * 6 + x] == (x < 4 ? source[(y / 2) * 3 + x / 2] : 55));
}
static void test_geometry_utilities(void) {
    dsp3d_bounds b;
    dsp3d_vec3 normals[968];
    dsp3d_compact_vertex packed[505];
    dsp3d_vertex decoded[505];
    assert(dsp3d_prepare_mesh(&suzanne, normals, &b) == DSP3D_OK);
    assert(b.radius > 0);
    assert(dsp3d_pack_vertices(&suzanne, packed, 505, &b) == DSP3D_OK);
    assert(dsp3d_unpack_vertices(packed, 505, &b, decoded, 505) == DSP3D_OK);
    for (unsigned i = 0; i < 505; i++) {
        assert(fabsf(decoded[i].position.x - suzanne.vertices[i].position.x) < 0.0001f);
        assert(fabsf(decoded[i].position.y - suzanne.vertices[i].position.y) < 0.0001f);
        assert(fabsf(decoded[i].position.z - suzanne.vertices[i].position.z) < 0.0001f);
    }
    dsp3d_lod levels[] = {{&suzanne, 0}, {&metamask, 5}};
    assert(dsp3d_select_lod(levels, 2, 3) == &suzanne);
    assert(dsp3d_select_lod(levels, 2, 8) == &metamask);
    dsp3d_quality q;
    dsp3d_quality_init(&q, 33333);
    for (unsigned i = 0; i < 40; i++)
        dsp3d_quality_update(&q, 60000);
    assert(q.divisor == 2);
    for (unsigned i = 0; i < 80; i++)
        dsp3d_quality_update(&q, 1000);
    assert(q.divisor == 1);
}
static void test_camera_and_matrices(void) {
    dsp3d_mat4 a = dsp3d_matrix_trs((dsp3d_vec3){1, 2, 3},
                                    dsp3d_quat_euler((dsp3d_vec3){0.2f, 0.3f, 0.4f}),
                                    (dsp3d_vec3){2, 3, 4}),
               inverse;
    assert(dsp3d_matrix_inverse(&a, &inverse) == DSP3D_OK);
    dsp3d_mat4 product = dsp3d_matrix_multiply(&a, &inverse);
    for (unsigned i = 0; i < 16; i++)
        assert(fabsf(product.m[i] - (i % 5 == 0 ? 1.0f : 0.0f)) < 0.00001f);
    init();
    const dsp3d_vertex v[] = {
        {{-1, -1, 0}, {0, 0, 1}}, {{1, -1, 0}, {0, 0, 1}}, {{0, 1, 0}, {0, 0, 1}}};
    const dsp3d_triangle t[] = {{{0, 1, 2}, 0xffffff}};
    const dsp3d_mesh mesh = {v, t, 3, 1};
    dsp3d_hit hit;
    dsp3d_mat4 world = dsp3d_identity();
    assert(dsp3d_pick_mesh(&ctx.camera, (dsp3d_rect){0, 0, W, H}, W / 2, H / 2, &mesh, &world,
                           &hit) == DSP3D_OK);
    assert(hit.triangle == 0 && fabsf(hit.distance - 4) < 0.0001f);
    dsp3d_orbit orbit = {{0, 0, 0}, 0, 0, 4};
    dsp3d_orbit_apply(&orbit, &ctx.camera);
    assert(ctx.camera.eye.z == 4);
    dsp3d_vec3 before = ctx.camera.eye;
    dsp3d_camera_pan(&ctx.camera, 1, 0);
    assert(ctx.camera.eye.x == before.x + 1 && ctx.camera.target.x == 1);
}
static void test_materials_and_bounds(void) {
    init();
    ctx.ambient = 1;
    const dsp3d_vertex v[] = {
        {{-1, -1, 0}, {0, 0, 1}}, {{1, -1, 0}, {0, 0, 1}}, {{0, 1, 0}, {0, 0, 1}}};
    const dsp3d_triangle f[] = {{{0, 1, 2}, 0xff0000}};
    const dsp3d_mesh mesh = {v, f, 3, 1};
    dsp3d_geometry g;
    dsp3d_vec3 normals[1];
    assert(dsp3d_geometry_init(&g, &mesh, normals) == DSP3D_OK);
    const uint16_t texels[] = {0x001f, 0x07e0, 0xf800, 0xffff};
    const dsp3d_texture tex = {texels, NULL, 2, 2, 2, 0};
    const dsp3d_uv uv[] = {{0, 0}, {0, 0}, {0, 0}};
    dsp3d_material material = {0xffffff, &tex, 0, 0, 255, 0};
    g.materials = &material;
    g.material_count = 1;
    g.uv = uv;
    assert(dsp3d_validate_geometry(&g) == DSP3D_OK);
    assert(dsp3d_draw_geometry(&ctx, &g, &identity, DSP3D_TEXTURED) == DSP3D_OK);
    assert(color.data[(H / 2) * W + W / 2] == 0x001f);
    material.texture = NULL;
    material.color = 0xff0000;
    material.opacity = 128;
    dsp3d_begin_frame(&ctx, 0x001f);
    assert(dsp3d_draw_geometry(&ctx, &g, &identity, DSP3D_FLAT) == DSP3D_OK);
    uint16_t mixed = color.data[(H / 2) * W + W / 2];
    assert((mixed >> 11) > 0 && (mixed & 31) > 0);
    assert(depth.data[(H / 2) * W + W / 2] == 0);
    dsp3d_begin_frame(&ctx, 0);
    dsp3d_transform away = identity;
    away.position.x = 100;
    assert(dsp3d_draw_geometry(&ctx, &g, &away, DSP3D_TEXTURED) == DSP3D_OK);
    assert(ctx.stats.rejected_objects == 1 && ctx.stats.transformed_vertices == 0);
    dsp3d_mat4 world =
        dsp3d_matrix_trs((dsp3d_vec3){0, 0, 0}, (dsp3d_quat){0, 0, 0, 1}, (dsp3d_vec3){1, 2, 1});
    assert(dsp3d_draw_matrix(&ctx, &g, &world, DSP3D_GOURAUD) == DSP3D_OK);
    dsp3d_vertex shadow[3];
    assert(dsp3d_planar_shadow(&mesh, &world, (dsp3d_vec3){1, 2, 1}, -2, shadow, 3) == DSP3D_OK);
    assert(fabsf(shadow[0].position.y + 1.998f) < 0.0001f);
}
static void test_scene_animation_and_skin(void) {
    dsp3d_node nodes[2] = {0};
    for (unsigned i = 0; i < 2; i++) {
        nodes[i].parent = i ? 0 : -1;
        nodes[i].mesh_id = UINT32_MAX;
        nodes[i].scale = (dsp3d_vec3){1, 1, 1};
        nodes[i].rotation.w = 1;
        nodes[i].visible = 1;
    }
    nodes[0].translation.x = 1;
    nodes[1].translation.y = 2;
    dsp3d_scene scene = {nodes, 2};
    assert(dsp3d_scene_update(&scene) == DSP3D_OK);
    assert(nodes[1].world.m[3] == 1 && nodes[1].world.m[7] == 2);
    nodes[0].parent = 1;
    assert(dsp3d_scene_update(&scene) == DSP3D_INVALID_MODEL);
    nodes[0].parent = -1;
    dsp3d_keyframe keys[2] = {{0, {0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}},
                              {2, {2, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}}};
    dsp3d_track track = {0, 0, 2, 0};
    dsp3d_animation animation = {&track, keys, 1, 2, 2, 1};
    assert(dsp3d_scene_animate(&scene, &animation, 1) == DSP3D_OK);
    assert(nodes[0].translation.x == 1);
    uint8_t bytes[512];
    size_t written;
    assert(dsp3d_scene_write(&scene, &animation, bytes, sizeof(bytes), &written) == DSP3D_OK);
    dsp3d_node loaded_nodes[2];
    dsp3d_track loaded_tracks[1];
    dsp3d_keyframe loaded_keys[2];
    dsp3d_scene loaded = {loaded_nodes, 0};
    dsp3d_animation anim = {loaded_tracks, loaded_keys, 0, 0, 0, 0};
    assert(dsp3d_scene_read(bytes, written, &loaded, 2, &anim, 1, 2, NULL, 0) == DSP3D_OK);
    assert(dsp3d_scene_animate(&loaded, &anim, 1) == DSP3D_OK);
    assert(loaded.nodes[1].world.m[3] == 1);
    bytes[written - 1] ^= 1;
    assert(dsp3d_scene_read(bytes, written, &loaded, 2, &anim, 1, 2, NULL, 0) ==
           DSP3D_CHECKSUM_ERROR);
    uint32_t joints[] = {0, 1};
    dsp3d_mat4 bind[2] = {dsp3d_identity(), dsp3d_identity()}, identity_matrix = dsp3d_identity();
    dsp3d_joint_palette palette[2];
    assert(dsp3d_skin_palette(&scene, joints, bind, 2, &identity_matrix, palette) == DSP3D_OK);
    dsp3d_vertex source = {{0, 0, 0}, {0, 0, 1}}, destination;
    dsp3d_skin_vertex weight = {{0, 1, 0, 0}, {0.25f, 0.75f, 0, 0}};
    assert(dsp3d_skin_vertices(&source, &weight, 1, palette, 2, &destination, 1) == DSP3D_OK);
    assert(fabsf(destination.position.x - 1) < 0.0001f &&
           fabsf(destination.position.y - 1.5f) < 0.0001f);
    weight.joints[1] = 3;
    assert(dsp3d_skin_vertices(&source, &weight, 1, palette, 2, &destination, 1) ==
           DSP3D_INVALID_MODEL);
}
int main(void) {
    test_viewport_stride_and_scale();
    test_geometry_utilities();
    test_camera_and_matrices();
    test_materials_and_bounds();
    test_scene_animation_and_skin();
    test_models();
    test_depth_and_culling();
    test_clipping_and_normals();
    test_shared_edges_and_dirty_clear();
    puts("PASS: models, CRC, malformed input, depth, culling, all modes, normal invariance, "
         "clipping stress and buffer guards");
    return 0;
}
