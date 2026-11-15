/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016-2026 Fabio Angeletti
 * The original 2016 implementation is preserved in legacy/. */
#include "dsp3D_material.h"
#include <math.h>
#include <string.h>
#define MAX_INT(a, b) ((a) > (b) ? (a) : (b))
#define MIN_INT(a, b) ((a) < (b) ? (a) : (b))
#ifdef DSP3D_CMSIS
#include "arm_math.h"
#define fast_sin arm_sin_f32
#define fast_cos arm_cos_f32
#else
#define fast_sin sinf
#define fast_cos cosf
#endif
static dsp3d_vec3 sub(dsp3d_vec3 a, dsp3d_vec3 b) {
    return (dsp3d_vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}
static float dot(dsp3d_vec3 a, dsp3d_vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static dsp3d_vec3 cross(dsp3d_vec3 a, dsp3d_vec3 b) {
    return (dsp3d_vec3){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static dsp3d_vec3 normalized(dsp3d_vec3 v) {
    float d = dot(v, v);
    if (!(d > 1.0e-20f) || !isfinite(d))
        return (dsp3d_vec3){0, 0, 0};
    float s = 1.0f / sqrtf(d);
    return (dsp3d_vec3){v.x * s, v.y * s, v.z * s};
}
static int finite3(dsp3d_vec3 v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}
static dsp3d_vec3 rotate(dsp3d_vec3 v, const float *r) {
    return (dsp3d_vec3){v.x * r[0] + v.y * r[1] + v.z * r[2], v.x * r[3] + v.y * r[4] + v.z * r[5],
                        v.x * r[6] + v.y * r[7] + v.z * r[8]};
}
static void rotation(dsp3d_vec3 a, float *r) {
    /* Bound the CMSIS lookup's integer range reduction even for large angles. */
    if (fabsf(a.x) > 6.2831853f)
        a.x = remainderf(a.x, 6.2831853f);
    if (fabsf(a.y) > 6.2831853f)
        a.y = remainderf(a.y, 6.2831853f);
    if (fabsf(a.z) > 6.2831853f)
        a.z = remainderf(a.z, 6.2831853f);
    float sx = fast_sin(a.x), cx = fast_cos(a.x), sy = fast_sin(a.y), cy = fast_cos(a.y),
          sz = fast_sin(a.z), cz = fast_cos(a.z);
    /* Rz * Ry * Rx; normals receive rotation only (uniform positive scale). */
    r[0] = cz * cy;
    r[1] = cz * sy * sx - sz * cx;
    r[2] = cz * sy * cx + sz * sx;
    r[3] = sz * cy;
    r[4] = sz * sy * sx + cz * cx;
    r[5] = sz * sy * cx - cz * sx;
    r[6] = -sy;
    r[7] = cy * sx;
    r[8] = cy * cx;
}
uint16_t dsp3d_rgb565(uint32_t c) {
    return (uint16_t)(((c >> 8) & 0xf800u) | ((c >> 5) & 0x07e0u) | ((c >> 3) & 31u));
}
static unsigned intensity(float x) {
#ifdef DSP3D_CMSIS
    return __USAT((int32_t)x, 8);
#else
    return x <= 0 ? 0u : x >= 255 ? 255u : (unsigned)x;
#endif
}
static const uint16_t *palette(dsp3d_context *c, uint32_t color) {
    for (unsigned i = 0; i < 4; i++)
        if ((c->palette_mask & (1u << i)) && c->palette_keys[i] == color)
            return i ? c->palette_cache[i - 1] : c->palette;
    unsigned slot = c->palette_next++ % 4;
    c->palette_keys[slot] = color;
    c->palette_mask |= (uint8_t)(1u << slot);
    uint16_t *p = slot ? c->palette_cache[slot - 1] : c->palette;
    unsigned r = (color >> 16) & 255u, g = (color >> 8) & 255u, b = color & 255u;
    for (unsigned i = 0; i < 256; i++)
        p[i] = dsp3d_rgb565(((r * i / 255u) << 16) | ((g * i / 255u) << 8) | (b * i / 255u));
    return p;
}
dsp3d_result dsp3d_init(dsp3d_context *c, uint32_t w, uint32_t h, uint16_t *color, float *depth,
                        dsp3d_cached_vertex *scratch, uint32_t n) {
    if (!c || !w || !h || w > 2047 || h > 2047 || !color || !depth || !scratch || !n ||
        ((uintptr_t)color & 1u) || ((uintptr_t)depth & 3u) || ((uintptr_t)scratch & 3u))
        return DSP3D_INVALID_ARGUMENT;
    memset(c, 0, sizeof(*c));
    c->width = w;
    c->height = h;
    c->color_stride = c->depth_stride = w;
    c->viewport = c->scissor = (dsp3d_rect){0, 0, w, h};
    c->color = color;
    c->depth = depth;
    c->scratch = scratch;
    c->scratch_count = n;
    c->camera = (dsp3d_camera){{0, 0, 4}, {0, 0, 0}, {0, 1, 0}, 0.85f, 0.05f, 100.0f};
    c->light_direction = normalized((dsp3d_vec3){-0.4f, 0.7f, 1});
    c->ambient = 0.2f;
    c->backface_culling = 1;
    c->shininess = 32;
    return DSP3D_OK;
}
static void clip_bounds(const dsp3d_context *c, uint32_t *x0, uint32_t *y0, uint32_t *x1,
                        uint32_t *y1) {
    *x0 = c->viewport.x > c->scissor.x ? c->viewport.x : c->scissor.x;
    *y0 = c->viewport.y > c->scissor.y ? c->viewport.y : c->scissor.y;
    uint32_t vx = c->viewport.x + c->viewport.width, sx = c->scissor.x + c->scissor.width;
    uint32_t vy = c->viewport.y + c->viewport.height, sy = c->scissor.y + c->scissor.height;
    *x1 = vx < sx ? vx : sx;
    *y1 = vy < sy ? vy : sy;
}
static int valid_rect(const dsp3d_context *c, dsp3d_rect r) {
    return r.width && r.height && r.x < c->width && r.y < c->height && r.width <= c->width - r.x &&
           r.height <= c->height - r.y;
}
dsp3d_result dsp3d_bind_target(dsp3d_context *c, uint32_t w, uint32_t h, uint16_t *color,
                               uint32_t cs, float *depth, uint32_t ds) {
    if (!c || !w || !h || w > 2047 || h > 2047 || cs < w || ds < w || !color || !depth ||
        ((uintptr_t)color & 1u) || ((uintptr_t)depth & 3u) ||
        (uint64_t)cs * h > SIZE_MAX / sizeof(*color) ||
        (uint64_t)ds * h > SIZE_MAX / sizeof(*depth))
        return DSP3D_INVALID_ARGUMENT;
    c->width = w;
    c->height = h;
    c->color = color;
    c->depth = depth;
    c->color_stride = cs;
    c->depth_stride = ds;
    c->viewport = c->scissor = (dsp3d_rect){0, 0, w, h};
    c->depth_initialized = 0;
    return DSP3D_OK;
}
dsp3d_result dsp3d_set_viewport(dsp3d_context *c, dsp3d_rect r) {
    if (!c || !valid_rect(c, r))
        return DSP3D_INVALID_ARGUMENT;
    c->viewport = r;
    return DSP3D_OK;
}
dsp3d_result dsp3d_set_scissor(dsp3d_context *c, dsp3d_rect r) {
    if (!c || !valid_rect(c, r))
        return DSP3D_INVALID_ARGUMENT;
    c->scissor = r;
    return DSP3D_OK;
}
void dsp3d_begin_frame(dsp3d_context *c, uint16_t background) {
    if (!c)
        return;
    uint32_t x0, y0, x1, y1;
    clip_bounds(c, &x0, &y0, &x1, &y1);
    uint32_t pair = (uint32_t)background | ((uint32_t)background << 16);
    if (x0 < x1)
        for (uint32_t y = y0; y < y1; y++) {
            uint16_t *row = c->color + (size_t)y * c->color_stride;
            uint32_t x = x0;
            for (; x + 1 < x1; x += 2)
                memcpy(row + x, &pair, 4);
            if (x < x1)
                row[x] = background;
        }
    dsp3d_clear_depth(c);
}
dsp3d_result dsp3d_scale_rgb565(const uint16_t *s, uint32_t sw, uint32_t sh, uint32_t ss,
                                uint16_t *d, uint32_t dw, uint32_t dh, uint32_t ds) {
    if (!s || !d || !sw || !sh || !dw || !dh || ss < sw || ds < dw || sw > 2047 || sh > 2047 ||
        dw > 2047 || dh > 2047 || ((uintptr_t)s & 1u) || ((uintptr_t)d & 1u) ||
        (uint64_t)ss * sh > SIZE_MAX / 2 || (uint64_t)ds * dh > SIZE_MAX / 2)
        return DSP3D_INVALID_ARGUMENT;
    if (dw == sw * 2 && dh == sh * 2) {
        for (uint32_t y = 0; y < sh; y++)
            for (uint32_t x = 0; x < sw; x++) {
                uint32_t pair = (uint32_t)s[y * ss + x] * 0x10001u;
                memcpy(d + (size_t)(y * 2) * ds + x * 2, &pair, 4);
                memcpy(d + (size_t)(y * 2 + 1) * ds + x * 2, &pair, 4);
            }
    } else
        for (uint32_t y = 0; y < dh; y++)
            for (uint32_t x = 0; x < dw; x++)
                d[(size_t)y * ds + x] = s[(size_t)(y * sh / dh) * ss + x * sw / dw];
    return DSP3D_OK;
}
/* Volatile paired stores prevent GCC from replacing this with nano-libc's
 * byte-at-a-time memset. This also remains useful with other compact C runtimes. */
static void zero_depth(volatile float *p, size_t n) {
    while (n >= 8) {
        p[0] = 0;
        p[1] = 0;
        p[2] = 0;
        p[3] = 0;
        p[4] = 0;
        p[5] = 0;
        p[6] = 0;
        p[7] = 0;
        p += 8;
        n -= 8;
    }
    while (n--)
        *p++ = 0;
}
void dsp3d_clear_depth(dsp3d_context *c) {
    if (!c)
        return;
    if (!c->depth_initialized) {
        for (uint32_t y = 0; y < c->height; y++)
            zero_depth(c->depth + (size_t)y * c->depth_stride, c->width);
        c->depth_initialized = 1;
    } else if (c->dirty_x0 < c->dirty_x1 && c->dirty_y0 < c->dirty_y1) {
        for (uint32_t y = c->dirty_y0; y < c->dirty_y1; y++)
            zero_depth(c->depth + (size_t)y * c->depth_stride + c->dirty_x0,
                       c->dirty_x1 - c->dirty_x0);
    }
    c->dirty_x0 = c->width;
    c->dirty_y0 = c->height;
    c->dirty_x1 = c->dirty_y1 = 0;
    memset(&c->stats, 0, sizeof(c->stats));
}
static void dirty(dsp3d_context *c, unsigned x0, unsigned y0, unsigned x1, unsigned y1) {
    if (x0 < c->dirty_x0)
        c->dirty_x0 = x0;
    if (y0 < c->dirty_y0)
        c->dirty_y0 = y0;
    if (x1 > c->dirty_x1)
        c->dirty_x1 = x1;
    if (y1 > c->dirty_y1)
        c->dirty_y1 = y1;
}
typedef dsp3d_cached_vertex cv;
typedef struct {
    float x, y, z, light, u, v, specular;
} screen_vertex;
static float plane(cv v, int p, float kx, float ky, float nz, float fz) {
    switch (p) {
    case 0:
        return v.z - nz;
    case 1:
        return fz - v.z;
    case 2:
        return v.x + kx * v.z;
    case 3:
        return kx * v.z - v.x;
    case 4:
        return v.y + ky * v.z;
    default:
        return ky * v.z - v.y;
    }
}
static cv mix(cv a, cv b, float t) {
    return (cv){a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t,
                a.z + (b.z - a.z) * t,
                a.light + (b.light - a.light) * t,
                a.u + (b.u - a.u) * t,
                a.v + (b.v - a.v) * t,
                a.specular + (b.specular - a.specular) * t};
}
static int clip(const cv *in, int n, cv *out, int p, float kx, float ky, float nz, float fz) {
    int count = 0;
    cv a = in[n - 1];
    float da = plane(a, p, kx, ky, nz, fz);
    for (int i = 0; i < n; i++) {
        cv b = in[i];
        float db = plane(b, p, kx, ky, nz, fz);
        if ((da >= 0) != (db >= 0))
            out[count++] = mix(a, b, da / (da - db));
        if (db >= 0)
            out[count++] = b;
        a = b;
        da = db;
    }
    return count;
}
static screen_vertex project(cv v, float fx, float fy, float hx, float hy) {
    float iz = 1.0f / v.z;
    return (screen_vertex){hx + v.x * fx * iz, hy - v.y * fy * iz, iz,        v.light,
                           v.u * iz,           v.v * iz,           v.specular};
}
static int32_t edge_fixed(int ax, int ay, int bx, int by, int px, int py) {
#ifdef DSP3D_CMSIS
    /* Signed halfword multiply/add: dx*ry + (-dy)*rx, exact on the subpixel grid. */
    uint32_t a = (uint16_t)(bx - ax) | ((uint32_t)(uint16_t)(ay - by) << 16);
    uint32_t b = (uint16_t)(py - ay) | ((uint32_t)(uint16_t)(px - ax) << 16);
    return (int32_t)__SMUAD(a, b);
#else
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
#endif
}
static void pixel(dsp3d_context *c, int x, int y, float z, uint16_t color) {
    if ((unsigned)x >= c->width || (unsigned)y >= c->height)
        return;
    uint32_t x0, y0, x1, y1;
    clip_bounds(c, &x0, &y0, &x1, &y1);
    if ((unsigned)x < x0 || (unsigned)x >= x1 || (unsigned)y < y0 || (unsigned)y >= y1)
        return;
    size_t i = (size_t)y * c->depth_stride + (unsigned)x;
    if (z >= c->depth[i]) {
        c->depth[i] = z;
        c->color[(size_t)y * c->color_stride + (unsigned)x] = color;
        c->stats.shaded_pixels++;
        dirty(c, (unsigned)x, (unsigned)y, (unsigned)x + 1, (unsigned)y + 1);
    }
}
static void line(dsp3d_context *c, screen_vertex a, screen_vertex b, uint16_t color) {
    float dx = b.x - a.x, dy = b.y - a.y;
    int steps = (int)ceilf(fmaxf(fabsf(dx), fabsf(dy)));
    if (!steps) {
        pixel(c, (int)a.x, (int)a.y, a.z, color);
        return;
    }
    float step = 1.0f / (float)steps;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i * step;
        pixel(c, (int)(a.x + dx * t), (int)(a.y + dy * t), a.z + (b.z - a.z) * t, color);
    }
}
static uint16_t blend565(uint16_t a, uint16_t b, unsigned opacity) {
    unsigned inv = 255 - opacity;
    unsigned r = (((a >> 11) * opacity + (b >> 11) * inv) + 127) / 255;
    unsigned g = ((((a >> 5) & 63) * opacity + ((b >> 5) & 63) * inv) + 127) / 255;
    unsigned blue = (((a & 31) * opacity + (b & 31) * inv) + 127) / 255;
    return (uint16_t)((r << 11) | (g << 5) | blue);
}
static uint16_t shade_texture(uint16_t tex, uint32_t base, unsigned l) {
    unsigned r = (tex >> 11) * ((base >> 16) & 255u) * l / 65025u;
    unsigned g = ((tex >> 5) & 63) * ((base >> 8) & 255u) * l / 65025u;
    unsigned b = (tex & 31) * (base & 255u) * l / 65025u;
    return (uint16_t)((r << 11) | (g << 5) | b);
}
static float specular_at(dsp3d_vec3 normal, dsp3d_vec3 light, dsp3d_vec3 position,
                         const dsp3d_context *c) {
    dsp3d_vec3 view = normalized(sub(c->camera.eye, position));
    dsp3d_vec3 h = normalized((dsp3d_vec3){view.x + light.x, view.y + light.y, view.z + light.z});
    return dot(normal, light) > 0 ? 255.0f * powf(fmaxf(0, dot(normal, h)), c->shininess) : 0;
}
static void raster(dsp3d_context *c, screen_vertex a, screen_vertex b, screen_vertex d,
                   dsp3d_mode mode, float flat, float flat_spec, const dsp3d_material *material,
                   const uint16_t *shades, uint32_t base) {
    int ax = (int)(fmaxf(0, fminf((float)c->width, a.x)) * 16.0f + 0.5f);
    int ay = (int)(fmaxf(0, fminf((float)c->height, a.y)) * 16.0f + 0.5f);
    int bx = (int)(fmaxf(0, fminf((float)c->width, b.x)) * 16.0f + 0.5f);
    int by = (int)(fmaxf(0, fminf((float)c->height, b.y)) * 16.0f + 0.5f);
    int dx = (int)(fmaxf(0, fminf((float)c->width, d.x)) * 16.0f + 0.5f);
    int dy = (int)(fmaxf(0, fminf((float)c->height, d.y)) * 16.0f + 0.5f);
    int32_t area = edge_fixed(ax, ay, bx, by, dx, dy);
    if (!area)
        return;
    if (c->backface_culling && area >= 0)
        return;
    c->stats.rasterized_triangles++;
    if (mode == DSP3D_WIREFRAME) {
        uint16_t color = shades[255];
        line(c, a, b, color);
        line(c, b, d, color);
        line(c, d, a, color);
        return;
    }
    if (area < 0) {
        screen_vertex tmp = b;
        b = d;
        d = tmp;
        int q = bx;
        bx = dx;
        dx = q;
        q = by;
        by = dy;
        dy = q;
        area = -area;
    }
    int x0 = (int)fmaxf(0, ceilf(fminf(a.x, fminf(b.x, d.x)) - 0.5f));
    int x1 = (int)fminf((float)c->width - 1, floorf(fmaxf(a.x, fmaxf(b.x, d.x)) - 0.5f));
    int y0 = (int)fmaxf(0, ceilf(fminf(a.y, fminf(b.y, d.y)) - 0.5f));
    int y1 = (int)fminf((float)c->height - 1, floorf(fmaxf(a.y, fmaxf(b.y, d.y)) - 0.5f));
    uint32_t cx0, cy0, cx1, cy1;
    clip_bounds(c, &cx0, &cy0, &cx1, &cy1);
    x0 = MAX_INT(x0, (int)cx0);
    y0 = MAX_INT(y0, (int)cy0);
    x1 = MIN_INT(x1, (int)cx1 - 1);
    y1 = MIN_INT(y1, (int)cy1 - 1);
    if (x0 > x1 || y0 > y1)
        return;
    dirty(c, (unsigned)x0, (unsigned)y0, (unsigned)x1 + 1, (unsigned)y1 + 1);
    float inv = 1.0f / (float)area;
    int32_t e0 = edge_fixed(bx, by, dx, dy, x0 * 16 + 8, y0 * 16 + 8),
            e1 = edge_fixed(dx, dy, ax, ay, x0 * 16 + 8, y0 * 16 + 8),
            e2 = edge_fixed(ax, ay, bx, by, x0 * 16 + 8, y0 * 16 + 8);
    int32_t ex0 = (by - dy) * 16, ex1 = (dy - ay) * 16, ex2 = (ay - by) * 16, ey0 = (dx - bx) * 16,
            ey1 = (ax - dx) * 16, ey2 = (bx - ax) * 16;
    float zrow = ((float)e0 * a.z + (float)e1 * b.z + (float)e2 * d.z) * inv,
          zx = ((float)ex0 * a.z + (float)ex1 * b.z + (float)ex2 * d.z) * inv,
          zy = ((float)ey0 * a.z + (float)ey1 * b.z + (float)ey2 * d.z) * inv;
    float lrow = ((float)e0 * a.light + (float)e1 * b.light + (float)e2 * d.light) * inv,
          lx = ((float)ex0 * a.light + (float)ex1 * b.light + (float)ex2 * d.light) * inv,
          ly = ((float)ey0 * a.light + (float)ey1 * b.light + (float)ey2 * d.light) * inv;
    uint16_t flat_color = shades[intensity(flat)];
    const dsp3d_texture *texture = mode == DSP3D_TEXTURED && material ? material->texture : NULL;
    unsigned opacity = material ? material->opacity : 255;
    float emission = material ? material->emissive : 0;
    float spec_scale = material ? fmaxf(material->specular, c->specular) : c->specular;
    int unlit = material && (material->flags & DSP3D_MATERIAL_UNLIT);
    float ur = (e0 * a.u + e1 * b.u + e2 * d.u) * inv,
          ux = (ex0 * a.u + ex1 * b.u + ex2 * d.u) * inv,
          uy = (ey0 * a.u + ey1 * b.u + ey2 * d.u) * inv;
    float vr = (e0 * a.v + e1 * b.v + e2 * d.v) * inv,
          vx = (ex0 * a.v + ex1 * b.v + ex2 * d.v) * inv,
          vy = (ey0 * a.v + ey1 * b.v + ey2 * d.v) * inv;
    float sr = (e0 * a.specular + e1 * b.specular + e2 * d.specular) * inv,
          sx = (ex0 * a.specular + ex1 * b.specular + ex2 * d.specular) * inv,
          sy = (ey0 * a.specular + ey1 * b.specular + ey2 * d.specular) * inv;
    /* Top-left ownership avoids shading shared edges twice. */
    int tl0 = (dy < by) || (dy == by && dx > bx), tl1 = (ay < dy) || (ay == dy && ax > dx),
        tl2 = (by < ay) || (by == ay && bx > ax);
    for (int y = y0; y <= y1; y++) {
        int32_t w0 = e0, w1 = e1, w2 = e2;
        float z = zrow, l = lrow, u = ur, v = vr, spec = sr;
        size_t index = (size_t)y * c->depth_stride + (unsigned)x0;
        size_t color_index = (size_t)y * c->color_stride + (unsigned)x0;
        for (int x = x0; x <= x1; x++, index++, color_index++) {
            if ((w0 > 0 || (w0 == 0 && tl0)) && (w1 > 0 || (w1 == 0 && tl1)) &&
                (w2 > 0 || (w2 == 0 && tl2)) && z >= c->depth[index]) {
                uint16_t color;
                unsigned alpha = opacity;
                float light_value = unlit ? 255.0f : (mode == DSP3D_FLAT ? flat : l);
                unsigned lit = intensity(light_value + (255.0f - light_value) * emission);
                if (texture) {
                    float reciprocal = 1.0f / z, tu = u * reciprocal, tv = v * reciprocal;
                    if (texture->repeat) {
                        tu -= floorf(tu);
                        tv -= floorf(tv);
                    }
                    unsigned tx =
                        (unsigned)(fmaxf(0, fminf(1, tu)) * (float)(texture->width - 1) + 0.5f);
                    unsigned ty =
                        (unsigned)(fmaxf(0, fminf(1, tv)) * (float)(texture->height - 1) + 0.5f);
                    color = shade_texture(texture->pixels[(size_t)ty * texture->stride + tx], base,
                                          lit);
                    if (texture->alpha)
                        alpha = alpha * texture->alpha[(size_t)ty * texture->width + tx] / 255;
                } else if (mode == DSP3D_FLAT && !unlit && emission == 0)
                    color = flat_color;
                else
                    color = shades[lit];
                if (spec_scale > 0) {
                    unsigned shine =
                        intensity((mode == DSP3D_FLAT ? flat_spec : spec) * spec_scale);
                    color = blend565(0xffff, color, shine);
                }
                if (alpha == 255) {
                    c->depth[index] = z;
                    c->color[color_index] = color;
                } else if (alpha)
                    c->color[color_index] = blend565(color, c->color[color_index], alpha);
                c->stats.shaded_pixels++;
            }
            w0 += ex0;
            w1 += ex1;
            w2 += ex2;
            z += zx;
            l += lx;
            if (texture) {
                u += ux;
                v += vx;
            }
            if (spec_scale > 0)
                spec += sx;
        }
        e0 += ey0;
        e1 += ey1;
        e2 += ey2;
        zrow += zy;
        lrow += ly;
        if (texture) {
            ur += uy;
            vr += vy;
        }
        if (spec_scale > 0)
            sr += sy;
    }
}
static dsp3d_result draw_internal(dsp3d_context *c, const dsp3d_mesh *m, const dsp3d_transform *t,
                                  dsp3d_mode mode, const dsp3d_geometry *geometry,
                                  const dsp3d_mat4 *world) {
    if (!c || !m || !t || !m->vertices || !m->triangles || !m->vertex_count ||
        (unsigned)mode > DSP3D_TEXTURED || !finite3(t->rotation) || !finite3(t->position) ||
        !isfinite(t->scale) || t->scale <= 0 || !finite3(c->camera.eye) ||
        !finite3(c->camera.target) || !finite3(c->camera.up) || !finite3(c->light_direction) ||
        !isfinite(c->ambient) || c->ambient < 0 || c->ambient > 1 ||
        !isfinite(c->camera.vertical_fov) || c->camera.vertical_fov < 0.05f ||
        c->camera.vertical_fov > 3.0f || !isfinite(c->camera.near_plane) ||
        !isfinite(c->camera.far_plane) || c->camera.near_plane <= 0 ||
        c->camera.far_plane <= c->camera.near_plane || !isfinite(c->specular) || c->specular < 0 ||
        c->specular > 1 || !isfinite(c->shininess) || c->shininess < 1 || c->shininess > 256)
        return DSP3D_INVALID_ARGUMENT;
    if (m->vertex_count > c->scratch_count)
        return DSP3D_INSUFFICIENT_MEMORY;
    dsp3d_vec3 forward = normalized(sub(c->camera.target, c->camera.eye));
    dsp3d_vec3 right = normalized(cross(forward, c->camera.up)), up = cross(right, forward);
    if (dot(right, right) < 0.5f)
        return DSP3D_INVALID_ARGUMENT;
    dsp3d_vec3 light = normalized(c->light_direction);
    float r[9];
    rotation(t->rotation, r);
    float ky = tanf(c->camera.vertical_fov * 0.5f),
          kx = ky * (float)c->viewport.width / (float)c->viewport.height;
    float fx = (float)c->viewport.width * 0.5f / kx, fy = (float)c->viewport.height * 0.5f / ky;
    float hx = (float)c->viewport.x + (float)c->viewport.width * 0.5f,
          hy = (float)c->viewport.y + (float)c->viewport.height * 0.5f;
    dsp3d_mat4 inverse = dsp3d_identity(), normal_matrix = dsp3d_identity();
    if (world) {
        if (dsp3d_matrix_inverse(world, &inverse) != DSP3D_OK)
            return DSP3D_INVALID_ARGUMENT;
        for (unsigned y = 0; y < 3; y++)
            for (unsigned x = 0; x < 3; x++)
                normal_matrix.m[y * 4 + x] = inverse.m[x * 4 + y];
    }
    int want_spec = c->specular > 0;
    if (geometry) {
        for (uint32_t i = 0; i < geometry->material_count; i++)
            want_spec |= geometry->materials[i].specular > 0;
        if (geometry->bounds_valid) {
            dsp3d_vec3 center = world ? dsp3d_matrix_point(world, geometry->bounds.center)
                                      : rotate(geometry->bounds.center, r);
            float radius = geometry->bounds.radius * t->scale;
            if (world) {
                float sum = 0;
                for (unsigned y = 0; y < 3; y++)
                    for (unsigned x = 0; x < 3; x++)
                        sum += world->m[y * 4 + x] * world->m[y * 4 + x];
                radius = geometry->bounds.radius * sqrtf(sum);
            } else
                center = (dsp3d_vec3){center.x * t->scale + t->position.x,
                                      center.y * t->scale + t->position.y,
                                      center.z * t->scale + t->position.z};
            dsp3d_vec3 delta = sub(center, c->camera.eye);
            float x = dot(delta, right), y = dot(delta, up), z = dot(delta, forward),
                  rx = radius * sqrtf(1 + kx * kx), ry = radius * sqrtf(1 + ky * ky);
            if (z + radius < c->camera.near_plane || z - radius > c->camera.far_plane ||
                x + kx * z < -rx || kx * z - x < -rx || y + ky * z < -ry || ky * z - y < -ry) {
                c->stats.rejected_objects++;
                return DSP3D_OK;
            }
        }
    }
    for (uint32_t i = 0; i < m->vertex_count; i++) {
        dsp3d_vec3 v = rotate(m->vertices[i].position, r);
        v = (dsp3d_vec3){v.x * t->scale + t->position.x, v.y * t->scale + t->position.y,
                         v.z * t->scale + t->position.z};
        if (world)
            v = dsp3d_matrix_point(world, m->vertices[i].position);
        dsp3d_vec3 position_world = v;
        v = sub(v, c->camera.eye);
        float l = 1;
        if (mode == DSP3D_GOURAUD || mode == DSP3D_TEXTURED || want_spec) {
            dsp3d_vec3 n =
                world ? normalized(dsp3d_matrix_vector(&normal_matrix, m->vertices[i].normal))
                      : rotate(m->vertices[i].normal, r);
            l = c->ambient + (1.0f - c->ambient) * fmaxf(0, dot(n, light));
        }
        dsp3d_uv uv = geometry && geometry->uv ? geometry->uv[i] : (dsp3d_uv){0, 0};
        dsp3d_vec3 normal =
            want_spec
                ? (world ? normalized(dsp3d_matrix_vector(&normal_matrix, m->vertices[i].normal))
                         : rotate(m->vertices[i].normal, r))
                : (dsp3d_vec3){0, 0, 1};
        c->scratch[i] = (cv){dot(v, right),
                             dot(v, up),
                             dot(v, forward),
                             l * 255.0f,
                             uv.u,
                             uv.v,
                             want_spec ? specular_at(normal, light, position_world, c) : 0};
        if (!isfinite(c->scratch[i].x) || !isfinite(c->scratch[i].y) || !isfinite(c->scratch[i].z))
            return DSP3D_INVALID_ARGUMENT;
    }
    c->stats.transformed_vertices += m->vertex_count;
    if (mode == DSP3D_POINTS) {
        for (uint32_t i = 0; i < m->vertex_count; i++) {
            cv v = c->scratch[i];
            int inside = 1;
            for (int p = 0; p < 6; p++)
                if (plane(v, p, kx, ky, c->camera.near_plane, c->camera.far_plane) < 0)
                    inside = 0;
            if (inside) {
                screen_vertex s = project(v, fx, fy, hx, hy);
                pixel(c, (int)s.x, (int)s.y, s.z, 0xffff);
            }
        }
        return DSP3D_OK;
    }
    for (uint32_t i = 0; i < m->triangle_count; i++) {
        const dsp3d_triangle *f = m->triangles + i;
        if (f->index[0] >= m->vertex_count || f->index[1] >= m->vertex_count ||
            f->index[2] >= m->vertex_count)
            return DSP3D_INVALID_MODEL;
        const dsp3d_material *material = NULL;
        if (geometry && geometry->material_count) {
            uint32_t mi = geometry->material_indices ? geometry->material_indices[i] : 0;
            if (mi >= geometry->material_count)
                return DSP3D_INVALID_MODEL;
            material = geometry->materials + mi;
            if (!material->opacity)
                continue;
        }
        cv buffers[2][12];
        int n = 3, src = 0;
        for (int j = 0; j < 3; j++)
            buffers[0][j] = c->scratch[f->index[j]];
        if (c->backface_culling) {
            cv a = buffers[0][0], b = buffers[0][1], d = buffers[0][2];
            float facing = a.x * (b.y * d.z - b.z * d.y) + a.y * (b.z * d.x - b.x * d.z) +
                           a.z * (b.x * d.y - b.y * d.x);
            if (facing <= 0) {
                c->stats.submitted_triangles++;
                continue;
            }
        }
        float flat = 255, flat_spec = 0;
        if (mode == DSP3D_FLAT) {
            dsp3d_vec3 p0 = m->vertices[f->index[0]].position,
                       p1 = m->vertices[f->index[1]].position,
                       p2 = m->vertices[f->index[2]].position;
            dsp3d_vec3 normal = geometry && geometry->face_normals
                                    ? geometry->face_normals[i]
                                    : normalized(cross(sub(p1, p0), sub(p2, p0)));
            normal =
                world ? normalized(dsp3d_matrix_vector(&normal_matrix, normal)) : rotate(normal, r);
            if (want_spec) {
                dsp3d_vec3 center = {(p0.x + p1.x + p2.x) / 3, (p0.y + p1.y + p2.y) / 3,
                                     (p0.z + p1.z + p2.z) / 3};
                center = world ? dsp3d_matrix_point(world, center) : rotate(center, r);
                if (!world)
                    center = (dsp3d_vec3){center.x * t->scale + t->position.x,
                                          center.y * t->scale + t->position.y,
                                          center.z * t->scale + t->position.z};
                flat_spec = specular_at(normal, light, center, c);
            }
            flat = 255.0f * (c->ambient + (1.0f - c->ambient) * fmaxf(0, dot(normal, light)));
        }
        c->stats.submitted_triangles++;
        /* Trivial accept/reject avoids clipping copies for interior triangles. */
        for (int p = 0; p < 6 && n; p++) {
            int inside = 0;
            for (int j = 0; j < n; j++)
                inside += plane(buffers[src][j], p, kx, ky, c->camera.near_plane,
                                c->camera.far_plane) >= 0;
            if (!inside) {
                n = 0;
                break;
            }
            if (inside != n) {
                n = clip(buffers[src], n, buffers[1 - src], p, kx, ky, c->camera.near_plane,
                         c->camera.far_plane);
                src = 1 - src;
            }
        }
        if (n < 3)
            continue;
        uint32_t base =
            material && !(material->flags & DSP3D_MATERIAL_FACE_COLOR) ? material->color : f->color;
        if (c->tint_enabled)
            base = c->tint;
        const uint16_t *shades = palette(c, base);
        screen_vertex a = project(buffers[src][0], fx, fy, hx, hy);
        for (int j = 1; j < n - 1; j++)
            raster(c, a, project(buffers[src][j], fx, fy, hx, hy),
                   project(buffers[src][j + 1], fx, fy, hx, hy), mode, flat, flat_spec, material,
                   shades, base);
    }
    return DSP3D_OK;
}

dsp3d_result dsp3d_draw(dsp3d_context *c, const dsp3d_mesh *m, const dsp3d_transform *t,
                        dsp3d_mode mode) {
    return draw_internal(c, m, t, mode, NULL, NULL);
}
dsp3d_result dsp3d_draw_geometry(dsp3d_context *c, const dsp3d_geometry *g,
                                 const dsp3d_transform *t, dsp3d_mode mode) {
    if (!g)
        return DSP3D_INVALID_ARGUMENT;
    return draw_internal(c, &g->mesh, t, mode, g, NULL);
}
dsp3d_result dsp3d_draw_matrix(dsp3d_context *c, const dsp3d_geometry *g, const dsp3d_mat4 *world,
                               dsp3d_mode mode) {
    if (!g || !world)
        return DSP3D_INVALID_ARGUMENT;
    const dsp3d_transform identity = {{0, 0, 0}, {0, 0, 0}, 1};
    return draw_internal(c, &g->mesh, &identity, mode, g, world);
}
