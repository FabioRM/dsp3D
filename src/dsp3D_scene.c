/* SPDX-License-Identifier: BSD-3-Clause */
#include "dsp3D_scene.h"
#include <math.h>
#include <string.h>
static int finite3(dsp3d_vec3 v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}
static int valid_pose(dsp3d_vec3 p, dsp3d_quat q, dsp3d_vec3 s) {
    float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    return finite3(p) && finite3(s) && fabsf(s.x) > 1e-8f && fabsf(s.y) > 1e-8f &&
           fabsf(s.z) > 1e-8f && isfinite(n) && n > 1e-10f;
}
dsp3d_result dsp3d_scene_update(dsp3d_scene *scene) {
    if (!scene || !scene->nodes || !scene->count || scene->count > DSP3D_SCENE_MAX_NODES)
        return DSP3D_INVALID_ARGUMENT;
    uint8_t ready[DSP3D_SCENE_MAX_NODES] = {0};
    uint32_t done = 0;
    for (uint32_t i = 0; i < scene->count; i++) {
        dsp3d_node *n = scene->nodes + i;
        if (n->parent < -1 || n->parent >= (int32_t)scene->count || n->parent == (int32_t)i ||
            !valid_pose(n->translation, n->rotation, n->scale))
            return DSP3D_INVALID_MODEL;
    }
    for (uint32_t pass = 0; pass < scene->count && done < scene->count; pass++) {
        uint32_t before = done;
        for (uint32_t i = 0; i < scene->count; i++) {
            dsp3d_node *n = scene->nodes + i;
            if (!ready[i] && (n->parent < 0 || ready[n->parent])) {
                dsp3d_mat4 local = dsp3d_matrix_trs(n->translation, n->rotation, n->scale);
                n->world = n->parent < 0
                               ? local
                               : dsp3d_matrix_multiply(&scene->nodes[n->parent].world, &local);
                ready[i] = 1;
                done++;
            }
        }
        if (done == before)
            return DSP3D_INVALID_MODEL;
    }
    return done == scene->count ? DSP3D_OK : DSP3D_INVALID_MODEL;
}
static int transparent(const dsp3d_geometry *g) {
    for (uint32_t i = 0; i < g->material_count; i++)
        if (g->materials[i].opacity < 255 ||
            (g->materials[i].texture && g->materials[i].texture->alpha))
            return 1;
    return 0;
}
static int visible(const dsp3d_scene *s, uint32_t i) {
    for (uint32_t visited = 0; visited < s->count; visited++) {
        if (!s->nodes[i].visible)
            return 0;
        if (s->nodes[i].parent < 0)
            return 1;
        i = (uint32_t)s->nodes[i].parent;
        if (i >= s->count)
            return 0;
    }
    return 0;
}
dsp3d_result dsp3d_scene_render(dsp3d_context *c, dsp3d_scene *s, dsp3d_mode mode) {
    if (!c)
        return DSP3D_INVALID_ARGUMENT;
    dsp3d_result r = dsp3d_scene_update(s);
    if (r != DSP3D_OK)
        return r;
    uint32_t order[DSP3D_SCENE_MAX_NODES], count = 0;
    float distance[DSP3D_SCENE_MAX_NODES];
    uint8_t old_pass = c->material_pass;
    c->material_pass = 1;
    dsp3d_vec3 forward = dsp3d_vunit(dsp3d_vsub(c->camera.target, c->camera.eye));
    for (uint32_t i = 0; i < s->count; i++) {
        dsp3d_node *n = s->nodes + i;
        if (!visible(s, i) || !n->geometry)
            continue;
        r = dsp3d_draw_matrix(c, n->geometry, &n->world, mode);
        if (r != DSP3D_OK)
            goto finish;
        if (transparent(n->geometry)) {
            dsp3d_vec3 center =
                dsp3d_matrix_point(&n->world, n->geometry->bounds_valid ? n->geometry->bounds.center
                                                                        : (dsp3d_vec3){0, 0, 0});
            float z = dsp3d_vdot(dsp3d_vsub(center, c->camera.eye), forward);
            uint32_t j = count;
            while (j && distance[j - 1] < z) {
                distance[j] = distance[j - 1];
                order[j] = order[j - 1];
                j--;
            }
            order[j] = i;
            distance[j] = z;
            count++;
        }
    }
    c->material_pass = 2;
    for (uint32_t i = 0; i < count; i++) {
        dsp3d_node *n = s->nodes + order[i];
        r = dsp3d_draw_matrix(c, n->geometry, &n->world, mode);
        if (r != DSP3D_OK)
            break;
    }
finish:
    c->material_pass = old_pass;
    return r;
}
dsp3d_result dsp3d_scene_animate(dsp3d_scene *s, const dsp3d_animation *a, float time) {
    if (!s || !a || !isfinite(time) || !isfinite(a->duration) || a->duration < 0 ||
        (a->track_count && !a->tracks) || (a->key_count && !a->keys))
        return DSP3D_INVALID_ARGUMENT;
    if (a->duration > 0) {
        if (a->loop) {
            time = fmodf(time, a->duration);
            if (time < 0)
                time += a->duration;
        } else
            time = fmaxf(0, fminf(a->duration, time));
    }
    for (uint32_t i = 0; i < a->track_count; i++) {
        dsp3d_track tr = a->tracks[i];
        if (tr.node >= s->count || !tr.key_count || tr.first_key > a->key_count ||
            tr.key_count > a->key_count - tr.first_key)
            return DSP3D_INVALID_MODEL;
        for (uint32_t j = 0; j < tr.key_count; j++) {
            const dsp3d_keyframe *k = a->keys + tr.first_key + j;
            if (!isfinite(k->time) || k->time < 0 || k->time > a->duration ||
                !valid_pose(k->translation, k->rotation, k->scale) ||
                (j && k->time < a->keys[tr.first_key + j - 1].time))
                return DSP3D_INVALID_MODEL;
        }
    }
    for (uint32_t i = 0; i < a->track_count; i++) {
        dsp3d_track tr = a->tracks[i];
        const dsp3d_keyframe *keys = a->keys + tr.first_key;
        uint32_t j = 0;
        while (j + 1 < tr.key_count && keys[j + 1].time <= time)
            j++;
        const dsp3d_keyframe *x = keys + j, *y = j + 1 < tr.key_count ? keys + j + 1 : x;
        float t =
            y->time > x->time ? fmaxf(0, fminf(1, (time - x->time) / (y->time - x->time))) : 0;
        if (tr.step)
            t = 0;
        dsp3d_node *n = s->nodes + tr.node;
        n->translation =
            dsp3d_vadd(x->translation, dsp3d_vscale(dsp3d_vsub(y->translation, x->translation), t));
        n->scale = dsp3d_vadd(x->scale, dsp3d_vscale(dsp3d_vsub(y->scale, x->scale), t));
        n->rotation = dsp3d_quat_slerp(x->rotation, y->rotation, t);
    }
    return dsp3d_scene_update(s);
}
dsp3d_result dsp3d_scene_pick(const dsp3d_context *c, const dsp3d_scene *s, float x, float y,
                              uint32_t *node, dsp3d_hit *hit) {
    if (!c || !s || !node || !hit)
        return DSP3D_INVALID_ARGUMENT;
    *node = UINT32_MAX;
    hit->distance = INFINITY;
    hit->triangle = UINT32_MAX;
    for (uint32_t i = 0; i < s->count; i++)
        if (visible(s, i) && s->nodes[i].geometry) {
            dsp3d_hit candidate;
            dsp3d_result r =
                dsp3d_pick_mesh(&c->camera, c->viewport, x, y, &s->nodes[i].geometry->mesh,
                                &s->nodes[i].world, &candidate);
            if (r != DSP3D_OK)
                return r;
            if (candidate.distance < hit->distance) {
                *node = i;
                *hit = candidate;
            }
        }
    return DSP3D_OK;
}
dsp3d_result dsp3d_skin_palette(const dsp3d_scene *s, const uint32_t *joints, const dsp3d_mat4 *ib,
                                uint32_t count, const dsp3d_mat4 *mesh_inverse,
                                dsp3d_joint_palette *palette) {
    if (!s || !joints || !ib || !mesh_inverse || !palette || !count ||
        count > DSP3D_SKIN_MAX_JOINTS)
        return DSP3D_INVALID_ARGUMENT;
    for (uint32_t i = 0; i < count; i++) {
        if (joints[i] >= s->count)
            return DSP3D_INVALID_MODEL;
        dsp3d_mat4 world = dsp3d_matrix_multiply(&s->nodes[joints[i]].world, ib + i);
        palette[i].position = dsp3d_matrix_multiply(mesh_inverse, &world);
        dsp3d_mat4 inverse;
        dsp3d_result r = dsp3d_matrix_inverse(&palette[i].position, &inverse);
        if (r != DSP3D_OK)
            return r;
        palette[i].normal = dsp3d_identity();
        for (unsigned y = 0; y < 3; y++)
            for (unsigned x = 0; x < 3; x++)
                palette[i].normal.m[y * 4 + x] = inverse.m[x * 4 + y];
    }
    return DSP3D_OK;
}
dsp3d_result dsp3d_skin_vertices(const dsp3d_vertex *source, const dsp3d_skin_vertex *skin,
                                 uint32_t count, const dsp3d_joint_palette *palette,
                                 uint32_t joints, dsp3d_vertex *out, uint32_t capacity) {
    if (!source || !skin || !palette || !out || !joints || joints > DSP3D_SKIN_MAX_JOINTS)
        return DSP3D_INVALID_ARGUMENT;
    if (count > capacity)
        return DSP3D_INSUFFICIENT_MEMORY;
    for (uint32_t i = 0; i < count; i++) {
        float sum = 0;
        dsp3d_vec3 p = {0, 0, 0}, n = {0, 0, 0};
        for (unsigned j = 0; j < 4; j++) {
            float w = skin[i].weights[j];
            if (!isfinite(w) || w < 0 || w > 1 || skin[i].joints[j] >= joints)
                return DSP3D_INVALID_MODEL;
            if (w) {
                const dsp3d_joint_palette *bone = palette + skin[i].joints[j];
                p = dsp3d_vadd(
                    p, dsp3d_vscale(dsp3d_matrix_point(&bone->position, source[i].position), w));
                n = dsp3d_vadd(
                    n, dsp3d_vscale(dsp3d_matrix_vector(&bone->normal, source[i].normal), w));
                sum += w;
            }
        }
        if (sum < 1e-6f || !finite3(p) || !finite3(n))
            return DSP3D_INVALID_MODEL;
        out[i] = (dsp3d_vertex){dsp3d_vscale(p, 1 / sum), dsp3d_vunit(n)};
    }
    return DSP3D_OK;
}
