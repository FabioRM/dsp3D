/* SPDX-License-Identifier: BSD-3-Clause */
#include "dsp3D.h"
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
int main(void) {
    test_viewport_stride_and_scale();
    test_models();
    test_depth_and_culling();
    test_clipping_and_normals();
    test_shared_edges_and_dirty_clear();
    puts("PASS: models, CRC, malformed input, depth, culling, all modes, normal invariance, "
         "clipping stress and buffer guards");
    return 0;
}
