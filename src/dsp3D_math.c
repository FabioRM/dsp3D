/* SPDX-License-Identifier: BSD-3-Clause */
#include "dsp3D_math.h"
#include <math.h>
dsp3d_vec3 dsp3d_vadd(dsp3d_vec3 a, dsp3d_vec3 b) {
    return (dsp3d_vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}
dsp3d_vec3 dsp3d_vsub(dsp3d_vec3 a, dsp3d_vec3 b) {
    return (dsp3d_vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}
dsp3d_vec3 dsp3d_vscale(dsp3d_vec3 v, float s) {
    return (dsp3d_vec3){v.x * s, v.y * s, v.z * s};
}
float dsp3d_vdot(dsp3d_vec3 a, dsp3d_vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
dsp3d_vec3 dsp3d_vcross(dsp3d_vec3 a, dsp3d_vec3 b) {
    return (dsp3d_vec3){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
dsp3d_vec3 dsp3d_vunit(dsp3d_vec3 v) {
    float n = dsp3d_vdot(v, v);
    return isfinite(n) && n > 1e-20f ? dsp3d_vscale(v, 1.0f / sqrtf(n)) : (dsp3d_vec3){0, 0, 0};
}
static dsp3d_quat q_unit(dsp3d_quat q) {
    float d = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (!isfinite(d) || d < 1e-20f)
        return (dsp3d_quat){0, 0, 0, 1};
    float s = 1 / sqrtf(d);
    return (dsp3d_quat){q.x * s, q.y * s, q.z * s, q.w * s};
}
dsp3d_quat dsp3d_quat_euler(dsp3d_vec3 a) {
    float x = a.x * 0.5f, y = a.y * 0.5f, z = a.z * 0.5f, sx = sinf(x), sy = sinf(y), sz = sinf(z),
          cx = cosf(x), cy = cosf(y), cz = cosf(z);
    return (dsp3d_quat){sx * cy * cz - cx * sy * sz, cx * sy * cz + sx * cy * sz,
                        cx * cy * sz - sx * sy * cz, cx * cy * cz + sx * sy * sz};
}
dsp3d_quat dsp3d_quat_slerp(dsp3d_quat a, dsp3d_quat b, float t) {
    a = q_unit(a);
    b = q_unit(b);
    t = fmaxf(0, fminf(1, t));
    float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (d < 0) {
        b = (dsp3d_quat){-b.x, -b.y, -b.z, -b.w};
        d = -d;
    }
    float x = 1 - t, y = t;
    if (d < 0.9995f) {
        float theta = acosf(fminf(1, d)), s = sinf(theta);
        x = sinf((1 - t) * theta) / s;
        y = sinf(t * theta) / s;
    }
    return q_unit(
        (dsp3d_quat){a.x * x + b.x * y, a.y * x + b.y * y, a.z * x + b.z * y, a.w * x + b.w * y});
}
dsp3d_mat4 dsp3d_identity(void) {
    return (dsp3d_mat4){{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
}
dsp3d_mat4 dsp3d_matrix_trs(dsp3d_vec3 p, dsp3d_quat q, dsp3d_vec3 s) {
    q = q_unit(q);
    float x = q.x, y = q.y, z = q.z, w = q.w;
    return (dsp3d_mat4){{(1 - 2 * (y * y + z * z)) * s.x, 2 * (x * y - z * w) * s.y,
                         2 * (x * z + y * w) * s.z, p.x, 2 * (x * y + z * w) * s.x,
                         (1 - 2 * (x * x + z * z)) * s.y, 2 * (y * z - x * w) * s.z, p.y,
                         2 * (x * z - y * w) * s.x, 2 * (y * z + x * w) * s.y,
                         (1 - 2 * (x * x + y * y)) * s.z, p.z, 0, 0, 0, 1}};
}
dsp3d_mat4 dsp3d_matrix_multiply(const dsp3d_mat4 *a, const dsp3d_mat4 *b) {
    dsp3d_mat4 out = {0};
    for (unsigned y = 0; y < 4; y++)
        for (unsigned x = 0; x < 4; x++)
            for (unsigned k = 0; k < 4; k++)
                out.m[y * 4 + x] += a->m[y * 4 + k] * b->m[k * 4 + x];
    return out;
}
dsp3d_vec3 dsp3d_matrix_point(const dsp3d_mat4 *m, dsp3d_vec3 v) {
    return (dsp3d_vec3){m->m[0] * v.x + m->m[1] * v.y + m->m[2] * v.z + m->m[3],
                        m->m[4] * v.x + m->m[5] * v.y + m->m[6] * v.z + m->m[7],
                        m->m[8] * v.x + m->m[9] * v.y + m->m[10] * v.z + m->m[11]};
}
dsp3d_vec3 dsp3d_matrix_vector(const dsp3d_mat4 *m, dsp3d_vec3 v) {
    return (dsp3d_vec3){m->m[0] * v.x + m->m[1] * v.y + m->m[2] * v.z,
                        m->m[4] * v.x + m->m[5] * v.y + m->m[6] * v.z,
                        m->m[8] * v.x + m->m[9] * v.y + m->m[10] * v.z};
}
dsp3d_result dsp3d_matrix_inverse(const dsp3d_mat4 *a, dsp3d_mat4 *out) {
    if (!a || !out)
        return DSP3D_INVALID_ARGUMENT;
    for (unsigned i = 0; i < 16; i++)
        if (!isfinite(a->m[i]))
            return DSP3D_INVALID_ARGUMENT;
    if (fabsf(a->m[12]) + fabsf(a->m[13]) + fabsf(a->m[14]) + fabsf(a->m[15] - 1) > 1e-6f)
        return DSP3D_INVALID_ARGUMENT;
    const float *m = a->m;
    float det = m[0] * (m[5] * m[10] - m[6] * m[9]) - m[1] * (m[4] * m[10] - m[6] * m[8]) +
                m[2] * (m[4] * m[9] - m[5] * m[8]);
    if (!isfinite(det) || fabsf(det) < 1e-12f)
        return DSP3D_INVALID_ARGUMENT;
    float d = 1 / det;
    dsp3d_mat4 r = dsp3d_identity();
    r.m[0] = (m[5] * m[10] - m[6] * m[9]) * d;
    r.m[1] = (m[2] * m[9] - m[1] * m[10]) * d;
    r.m[2] = (m[1] * m[6] - m[2] * m[5]) * d;
    r.m[4] = (m[6] * m[8] - m[4] * m[10]) * d;
    r.m[5] = (m[0] * m[10] - m[2] * m[8]) * d;
    r.m[6] = (m[2] * m[4] - m[0] * m[6]) * d;
    r.m[8] = (m[4] * m[9] - m[5] * m[8]) * d;
    r.m[9] = (m[1] * m[8] - m[0] * m[9]) * d;
    r.m[10] = (m[0] * m[5] - m[1] * m[4]) * d;
    dsp3d_vec3 t = dsp3d_matrix_vector(&r, (dsp3d_vec3){-m[3], -m[7], -m[11]});
    r.m[3] = t.x;
    r.m[7] = t.y;
    r.m[11] = t.z;
    *out = r;
    return DSP3D_OK;
}
void dsp3d_orbit_apply(const dsp3d_orbit *o, dsp3d_camera *c) {
    if (!o || !c)
        return;
    float p = fmaxf(-1.5f, fminf(1.5f, o->pitch)), d = fmaxf(0.01f, o->distance);
    c->target = o->target;
    c->up = (dsp3d_vec3){0, 1, 0};
    c->eye = dsp3d_vadd(o->target, (dsp3d_vec3){sinf(o->yaw) * cosf(p) * d, sinf(p) * d,
                                                cosf(o->yaw) * cosf(p) * d});
}
void dsp3d_camera_pan(dsp3d_camera *c, float h, float v) {
    if (!c || !isfinite(h) || !isfinite(v))
        return;
    dsp3d_vec3 f = dsp3d_vunit(dsp3d_vsub(c->target, c->eye)),
               r = dsp3d_vunit(dsp3d_vcross(f, c->up)), u = dsp3d_vcross(r, f);
    dsp3d_vec3 t = dsp3d_vadd(dsp3d_vscale(r, h), dsp3d_vscale(u, v));
    c->eye = dsp3d_vadd(c->eye, t);
    c->target = dsp3d_vadd(c->target, t);
}
dsp3d_result dsp3d_camera_fit(dsp3d_camera *c, const dsp3d_bounds *b, float aspect, float margin) {
    if (!c || !b || !isfinite(b->radius) || b->radius <= 0 || !isfinite(aspect) || aspect <= 0 ||
        !isfinite(margin) || margin < 1 || !isfinite(c->vertical_fov) || c->vertical_fov < 0.05f ||
        c->vertical_fov > 3)
        return DSP3D_INVALID_ARGUMENT;
    float a = fminf(c->vertical_fov * 0.5f, atanf(tanf(c->vertical_fov * 0.5f) * aspect));
    float distance = b->radius / sinf(a) * margin;
    dsp3d_vec3 direction = dsp3d_vunit(dsp3d_vsub(c->eye, c->target));
    if (dsp3d_vdot(direction, direction) < 0.5f)
        direction = (dsp3d_vec3){0, 0, 1};
    c->target = b->center;
    c->eye = dsp3d_vadd(b->center, dsp3d_vscale(direction, distance));
    c->near_plane = fmaxf(0.01f, distance - b->radius * 1.5f);
    c->far_plane = fmaxf(c->near_plane + 1, distance + b->radius * 2);
    return DSP3D_OK;
}
dsp3d_result dsp3d_pick_mesh(const dsp3d_camera *c, dsp3d_rect vp, float x, float y,
                             const dsp3d_mesh *mesh, const dsp3d_mat4 *world, dsp3d_hit *hit) {
    if (!c || !mesh || !world || !hit || !vp.width || !vp.height || !isfinite(x) || !isfinite(y) ||
        x < vp.x || y < vp.y || x >= vp.x + vp.width || y >= vp.y + vp.height)
        return DSP3D_INVALID_ARGUMENT;
    dsp3d_vec3 f = dsp3d_vunit(dsp3d_vsub(c->target, c->eye)),
               r = dsp3d_vunit(dsp3d_vcross(f, c->up)), u = dsp3d_vcross(r, f);
    if (dsp3d_vdot(r, r) < 0.5f)
        return DSP3D_INVALID_ARGUMENT;
    float ky = tanf(c->vertical_fov * 0.5f),
          sx = ((x - vp.x) / (float)vp.width * 2 - 1) * ky * (float)vp.width / vp.height,
          sy = (1 - (y - vp.y) / (float)vp.height * 2) * ky;
    dsp3d_vec3 ray =
        dsp3d_vunit(dsp3d_vadd(f, dsp3d_vadd(dsp3d_vscale(r, sx), dsp3d_vscale(u, sy))));
    hit->distance = INFINITY;
    hit->triangle = UINT32_MAX;
    for (uint32_t i = 0; i < mesh->triangle_count; i++) {
        const dsp3d_triangle *t = mesh->triangles + i;
        for (unsigned j = 0; j < 3; j++)
            if (t->index[j] >= mesh->vertex_count)
                return DSP3D_INVALID_MODEL;
        dsp3d_vec3 a = dsp3d_matrix_point(world, mesh->vertices[t->index[0]].position),
                   b = dsp3d_matrix_point(world, mesh->vertices[t->index[1]].position),
                   d = dsp3d_matrix_point(world, mesh->vertices[t->index[2]].position);
        dsp3d_vec3 e1 = dsp3d_vsub(b, a), e2 = dsp3d_vsub(d, a), p = dsp3d_vcross(ray, e2);
        float det = dsp3d_vdot(e1, p);
        if (fabsf(det) < 1e-8f)
            continue;
        float inv = 1 / det;
        dsp3d_vec3 s = dsp3d_vsub(c->eye, a);
        float bu = dsp3d_vdot(s, p) * inv;
        if (bu < 0 || bu > 1)
            continue;
        dsp3d_vec3 q = dsp3d_vcross(s, e1);
        float bv = dsp3d_vdot(ray, q) * inv;
        if (bv < 0 || bu + bv > 1)
            continue;
        float distance = dsp3d_vdot(e2, q) * inv, view_depth = distance * dsp3d_vdot(ray, f);
        if (view_depth >= c->near_plane && view_depth <= c->far_plane && distance < hit->distance) {
            hit->distance = distance;
            hit->triangle = i;
            hit->position = dsp3d_vadd(c->eye, dsp3d_vscale(ray, distance));
        }
    }
    return DSP3D_OK;
}
