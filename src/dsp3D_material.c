/* SPDX-License-Identifier: BSD-3-Clause */
#include "dsp3D_material.h"
#include <math.h>
#include <string.h>
dsp3d_result dsp3d_geometry_init(dsp3d_geometry *g, const dsp3d_mesh *m, dsp3d_vec3 *normals) {
    if (!g || !m)
        return DSP3D_INVALID_ARGUMENT;
    memset(g, 0, sizeof(*g));
    dsp3d_result r = dsp3d_prepare_mesh(m, normals, &g->bounds);
    if (r == DSP3D_OK) {
        g->mesh = *m;
        g->face_normals = normals;
        g->bounds_valid = 1;
    }
    return r;
}
dsp3d_result dsp3d_validate_geometry(const dsp3d_geometry *g) {
    if (!g || dsp3d_validate_mesh(&g->mesh) != DSP3D_OK)
        return DSP3D_INVALID_MODEL;
    if (g->material_count && !g->materials)
        return DSP3D_INVALID_MODEL;
    for (uint32_t i = 0; i < g->mesh.vertex_count; i++)
        if (g->uv && (!isfinite(g->uv[i].u) || !isfinite(g->uv[i].v)))
            return DSP3D_INVALID_MODEL;
    for (uint32_t i = 0; i < g->mesh.triangle_count; i++) {
        if (g->material_indices && g->material_indices[i] >= g->material_count)
            return DSP3D_INVALID_MODEL;
        if (g->face_normals) {
            dsp3d_vec3 n = g->face_normals[i];
            float length = dsp3d_vdot(n, n);
            if (!isfinite(length) || length < 0.99f || length > 1.01f)
                return DSP3D_INVALID_MODEL;
        }
    }
    for (uint32_t i = 0; i < g->material_count; i++) {
        const dsp3d_material *m = g->materials + i;
        if (!isfinite(m->specular) || m->specular < 0 || m->specular > 1 ||
            !isfinite(m->emissive) || m->emissive < 0 || m->emissive > 1 || (m->flags & ~3u))
            return DSP3D_INVALID_MODEL;
        if (m->texture) {
            const dsp3d_texture *t = m->texture;
            if (!g->uv || !t->pixels || ((uintptr_t)t->pixels & 1u) || !t->width || !t->height ||
                t->width > 2047 || t->height > 2047 || t->stride < t->width ||
                (uint64_t)t->stride * t->height > SIZE_MAX / 2)
                return DSP3D_INVALID_MODEL;
        }
    }
    if (g->bounds_valid) {
        const dsp3d_bounds *b = &g->bounds;
        if (!isfinite(b->radius) || b->radius < 0 || !isfinite(b->center.x) ||
            !isfinite(b->center.y) || !isfinite(b->center.z))
            return DSP3D_INVALID_MODEL;
        for (uint32_t i = 0; i < g->mesh.vertex_count; i++) {
            dsp3d_vec3 d = dsp3d_vsub(g->mesh.vertices[i].position, b->center);
            if (dsp3d_vdot(d, d) > (b->radius + 0.0001f) * (b->radius + 0.0001f))
                return DSP3D_INVALID_MODEL;
        }
    }
    return DSP3D_OK;
}
dsp3d_result dsp3d_planar_shadow(const dsp3d_mesh *m, const dsp3d_mat4 *world, dsp3d_vec3 light,
                                 float plane, dsp3d_vertex *out, uint32_t capacity) {
    if (!m || !world || !out || !isfinite(plane) || !isfinite(light.x) || !isfinite(light.y) ||
        !isfinite(light.z) || fabsf(light.y) < 1e-6f)
        return DSP3D_INVALID_ARGUMENT;
    if (capacity < m->vertex_count)
        return DSP3D_INSUFFICIENT_MEMORY;
    for (uint32_t i = 0; i < m->vertex_count; i++) {
        dsp3d_vec3 p = dsp3d_matrix_point(world, m->vertices[i].position);
        float distance = (p.y - plane) / light.y;
        p = (dsp3d_vec3){p.x - light.x * distance, plane + 0.002f, p.z - light.z * distance};
        if (!isfinite(p.x) || !isfinite(p.z))
            return DSP3D_INVALID_ARGUMENT;
        out[i] = (dsp3d_vertex){p, {0, 1, 0}};
    }
    return DSP3D_OK;
}
