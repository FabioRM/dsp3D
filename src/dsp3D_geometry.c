/* SPDX-License-Identifier: BSD-3-Clause */
#include "dsp3D_geometry.h"
#include <math.h>
static dsp3d_vec3 difference(dsp3d_vec3 a, dsp3d_vec3 b) {
    return (dsp3d_vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}
static dsp3d_vec3 unit(dsp3d_vec3 v) {
    float n = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    return n > 1e-20f ? (dsp3d_vec3){v.x / n, v.y / n, v.z / n} : (dsp3d_vec3){0, 1, 0};
}
dsp3d_result dsp3d_prepare_mesh(const dsp3d_mesh *m, dsp3d_vec3 *normals, dsp3d_bounds *b) {
    if (!b || dsp3d_validate_mesh(m) != DSP3D_OK)
        return DSP3D_INVALID_MODEL;
    b->min = b->max = m->vertices[0].position;
    for (uint32_t i = 1; i < m->vertex_count; i++) {
        dsp3d_vec3 p = m->vertices[i].position;
        b->min = (dsp3d_vec3){fminf(b->min.x, p.x), fminf(b->min.y, p.y), fminf(b->min.z, p.z)};
        b->max = (dsp3d_vec3){fmaxf(b->max.x, p.x), fmaxf(b->max.y, p.y), fmaxf(b->max.z, p.z)};
    }
    b->center = (dsp3d_vec3){(b->min.x + b->max.x) * 0.5f, (b->min.y + b->max.y) * 0.5f,
                             (b->min.z + b->max.z) * 0.5f};
    b->radius = 0;
    for (uint32_t i = 0; i < m->vertex_count; i++) {
        dsp3d_vec3 d = difference(m->vertices[i].position, b->center);
        b->radius = fmaxf(b->radius, sqrtf(d.x * d.x + d.y * d.y + d.z * d.z));
    }
    if (!isfinite(b->radius))
        return DSP3D_INVALID_MODEL;
    if (normals)
        for (uint32_t i = 0; i < m->triangle_count; i++) {
            const dsp3d_triangle *f = m->triangles + i;
            dsp3d_vec3 a =
                difference(m->vertices[f->index[1]].position, m->vertices[f->index[0]].position);
            dsp3d_vec3 c =
                difference(m->vertices[f->index[2]].position, m->vertices[f->index[0]].position);
            normals[i] = unit(
                (dsp3d_vec3){a.y * c.z - a.z * c.y, a.z * c.x - a.x * c.z, a.x * c.y - a.y * c.x});
        }
    return DSP3D_OK;
}
static int16_t pack(float x) {
    x = fmaxf(-1, fminf(1, x));
    return (int16_t)lroundf(x * 32767.0f);
}
dsp3d_result dsp3d_pack_vertices(const dsp3d_mesh *m, dsp3d_compact_vertex *out, uint32_t capacity,
                                 dsp3d_bounds *b) {
    if (!m || !out || !b)
        return DSP3D_INVALID_ARGUMENT;
    if (capacity < m->vertex_count)
        return DSP3D_INSUFFICIENT_MEMORY;
    dsp3d_result r = dsp3d_prepare_mesh(m, NULL, b);
    if (r != DSP3D_OK)
        return r;
    dsp3d_vec3 e = {(b->max.x - b->min.x) * 0.5f, (b->max.y - b->min.y) * 0.5f,
                    (b->max.z - b->min.z) * 0.5f};
    for (uint32_t i = 0; i < m->vertex_count; i++) {
        dsp3d_vertex v = m->vertices[i];
        dsp3d_vec3 p = difference(v.position, b->center);
        out[i] =
            (dsp3d_compact_vertex){{pack(e.x > 0 ? p.x / e.x : 0), pack(e.y > 0 ? p.y / e.y : 0),
                                    pack(e.z > 0 ? p.z / e.z : 0)},
                                   {pack(v.normal.x), pack(v.normal.y), pack(v.normal.z)}};
    }
    return DSP3D_OK;
}
dsp3d_result dsp3d_unpack_vertices(const dsp3d_compact_vertex *p, uint32_t count,
                                   const dsp3d_bounds *b, dsp3d_vertex *out, uint32_t capacity) {
    if (!p || !b || !out || !count || !isfinite(b->min.x) || !isfinite(b->min.y) ||
        !isfinite(b->min.z) || !isfinite(b->max.x) || !isfinite(b->max.y) || !isfinite(b->max.z) ||
        b->max.x < b->min.x || b->max.y < b->min.y || b->max.z < b->min.z)
        return DSP3D_INVALID_ARGUMENT;
    if (capacity < count)
        return DSP3D_INSUFFICIENT_MEMORY;
    dsp3d_vec3 center = {(b->min.x + b->max.x) * 0.5f, (b->min.y + b->max.y) * 0.5f,
                         (b->min.z + b->max.z) * 0.5f};
    dsp3d_vec3 scale = {(b->max.x - b->min.x) / 65534.0f, (b->max.y - b->min.y) / 65534.0f,
                        (b->max.z - b->min.z) / 65534.0f};
    for (uint32_t i = 0; i < count; i++) {
        if (p[i].normal[0] == 0 && p[i].normal[1] == 0 && p[i].normal[2] == 0)
            return DSP3D_INVALID_MODEL;
        out[i].position = (dsp3d_vec3){center.x + p[i].position[0] * scale.x,
                                       center.y + p[i].position[1] * scale.y,
                                       center.z + p[i].position[2] * scale.z};
        out[i].normal = unit((dsp3d_vec3){p[i].normal[0] / 32767.0f, p[i].normal[1] / 32767.0f,
                                          p[i].normal[2] / 32767.0f});
    }
    return DSP3D_OK;
}
const dsp3d_mesh *dsp3d_select_lod(const dsp3d_lod *levels, uint32_t count, float distance) {
    if (!levels || !count || !isfinite(distance) || distance < 0)
        return NULL;
    const dsp3d_mesh *selected = levels[0].mesh;
    for (uint32_t i = 0; i < count; i++) {
        if (!levels[i].mesh || !isfinite(levels[i].minimum_distance) ||
            (i && levels[i].minimum_distance < levels[i - 1].minimum_distance))
            return NULL;
        if (distance >= levels[i].minimum_distance)
            selected = levels[i].mesh;
    }
    return selected;
}
void dsp3d_quality_init(dsp3d_quality *q, uint32_t target) {
    if (q)
        *q = (dsp3d_quality){target ? target : 33333, 0, 0, 1};
}
uint8_t dsp3d_quality_update(dsp3d_quality *q, uint32_t us) {
    if (!q)
        return 1;
    if (!q->target_us)
        dsp3d_quality_init(q, 33333);
    q->filtered_us = q->filtered_us ? (uint32_t)(((uint64_t)q->filtered_us * 7 + us) / 8) : us;
    if (q->stable_frames < 30) {
        q->stable_frames++;
        return q->divisor;
    }
    if ((uint64_t)q->filtered_us * 10 > (uint64_t)q->target_us * 11 && q->divisor < 4) {
        q->divisor *= 2;
        q->stable_frames = 0;
    } else if ((uint64_t)q->filtered_us * 10 < (uint64_t)q->target_us * 4 && q->divisor > 1) {
        q->divisor /= 2;
        q->stable_frames = 0;
    }
    return q->divisor;
}
