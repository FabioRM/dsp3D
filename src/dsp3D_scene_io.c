/* SPDX-License-Identifier: BSD-3-Clause */
#include "dsp3D_scene.h"
#include <math.h>
#include <string.h>
static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void write32(uint8_t *p, uint32_t n) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(n >> (i * 8));
}
static float readf(const uint8_t *p) {
    uint32_t bits = read32(p);
    float f;
    memcpy(&f, &bits, 4);
    return f;
}
static void writef(uint8_t *p, float f) {
    uint32_t bits;
    memcpy(&bits, &f, 4);
    write32(p, bits);
}
static void write_pose(uint8_t *p, dsp3d_vec3 t, dsp3d_quat q, dsp3d_vec3 s) {
    float v[] = {t.x, t.y, t.z, q.x, q.y, q.z, q.w, s.x, s.y, s.z};
    for (unsigned i = 0; i < 10; i++)
        writef(p + i * 4, v[i]);
}
static void read_pose(const uint8_t *p, dsp3d_vec3 *t, dsp3d_quat *q, dsp3d_vec3 *s) {
    *t = (dsp3d_vec3){readf(p), readf(p + 4), readf(p + 8)};
    *q = (dsp3d_quat){readf(p + 12), readf(p + 16), readf(p + 20), readf(p + 24)};
    *s = (dsp3d_vec3){readf(p + 28), readf(p + 32), readf(p + 36)};
}
dsp3d_result dsp3d_scene_write(const dsp3d_scene *s, const dsp3d_animation *a, void *data,
                               size_t capacity, size_t *written) {
    if (written)
        *written = 0;
    if (!s || !s->nodes || !s->count || s->count > DSP3D_SCENE_MAX_NODES || !data || !written)
        return DSP3D_INVALID_ARGUMENT;
    uint32_t tracks = a ? a->track_count : 0, keys = a ? a->key_count : 0;
    if (a &&
        ((tracks && !a->tracks) || (keys && !a->keys) || !isfinite(a->duration) || a->duration < 0))
        return DSP3D_INVALID_ARGUMENT;
    uint64_t bytes = (uint64_t)s->count * 56 + (uint64_t)tracks * 16 + (uint64_t)keys * 44;
    if (bytes > UINT32_MAX - 40 || bytes + 40 > capacity)
        return DSP3D_INSUFFICIENT_MEMORY;
    uint8_t *p = data;
    memset(p, 0, 40);
    memcpy(p, "D3SC", 4);
    write32(p + 4, 1);
    write32(p + 8, s->count);
    write32(p + 12, tracks);
    write32(p + 16, keys);
    write32(p + 20, (uint32_t)bytes);
    writef(p + 28, a ? a->duration : 0);
    write32(p + 32, a ? !!a->loop : 0);
    uint8_t *out = p + 40;
    for (uint32_t i = 0; i < s->count; i++, out += 56) {
        const dsp3d_node *n = s->nodes + i;
        write32(out, (uint32_t)n->parent);
        write32(out + 4, n->mesh_id);
        write_pose(out + 8, n->translation, n->rotation, n->scale);
        write32(out + 48, !!n->visible);
        write32(out + 52, 0);
    }
    for (uint32_t i = 0; i < tracks; i++, out += 16) {
        const dsp3d_track *t = a->tracks + i;
        write32(out, t->node);
        write32(out + 4, t->first_key);
        write32(out + 8, t->key_count);
        write32(out + 12, !!t->step);
    }
    for (uint32_t i = 0; i < keys; i++, out += 44) {
        const dsp3d_keyframe *k = a->keys + i;
        writef(out, k->time);
        write_pose(out + 4, k->translation, k->rotation, k->scale);
    }
    write32(p + 24, dsp3d_crc32(p + 40, (size_t)bytes));
    *written = (size_t)bytes + 40;
    return DSP3D_OK;
}
dsp3d_result dsp3d_scene_read(const void *data, size_t size, dsp3d_scene *s, uint32_t nc,
                              dsp3d_animation *a, uint32_t tc, uint32_t kc,
                              const dsp3d_geometry *const *catalog, uint32_t catalog_count) {
    if (!s || !a)
        return DSP3D_INVALID_ARGUMENT;
    s->count = a->track_count = a->key_count = 0;
    if (!data || size < 40 || !s->nodes)
        return DSP3D_INVALID_ARGUMENT;
    const uint8_t *p = data;
    if (memcmp(p, "D3SC", 4))
        return DSP3D_INVALID_MODEL;
    if (read32(p + 4) != 1)
        return DSP3D_UNSUPPORTED_VERSION;
    uint32_t n = read32(p + 8), t = read32(p + 12), k = read32(p + 16);
    uint64_t bytes = (uint64_t)n * 56 + (uint64_t)t * 16 + (uint64_t)k * 44;
    if (!n || n > DSP3D_SCENE_MAX_NODES || bytes != read32(p + 20) || bytes != size - 40 ||
        read32(p + 32) > 1 || read32(p + 36))
        return DSP3D_INVALID_MODEL;
    if (dsp3d_crc32(p + 40, size - 40) != read32(p + 24))
        return DSP3D_CHECKSUM_ERROR;
    if (n > nc || t > tc || k > kc || (t && !a->tracks) || (k && !a->keys))
        return DSP3D_INSUFFICIENT_MEMORY;
    const uint8_t *in = p + 40;
    for (uint32_t i = 0; i < n; i++, in += 56) {
        dsp3d_node *node = s->nodes + i;
        memset(node, 0, sizeof(*node));
        node->parent = (int32_t)read32(in);
        node->mesh_id = read32(in + 4);
        if (node->mesh_id != UINT32_MAX) {
            if (!catalog || node->mesh_id >= catalog_count || !catalog[node->mesh_id])
                return DSP3D_INVALID_MODEL;
            node->geometry = catalog[node->mesh_id];
        }
        read_pose(in + 8, &node->translation, &node->rotation, &node->scale);
        if (read32(in + 48) > 1 || read32(in + 52))
            return DSP3D_INVALID_MODEL;
        node->visible = (uint8_t)read32(in + 48);
    }
    for (uint32_t i = 0; i < t; i++, in += 16) {
        a->tracks[i] = (dsp3d_track){read32(in), read32(in + 4), read32(in + 8), read32(in + 12)};
        if (a->tracks[i].step > 1)
            return DSP3D_INVALID_MODEL;
    }
    for (uint32_t i = 0; i < k; i++, in += 44) {
        dsp3d_keyframe *key = a->keys + i;
        key->time = readf(in);
        read_pose(in + 4, &key->translation, &key->rotation, &key->scale);
    }
    s->count = n;
    a->track_count = t;
    a->key_count = k;
    a->duration = readf(p + 28);
    a->loop = (uint8_t)read32(p + 32);
    dsp3d_result r = t ? dsp3d_scene_animate(s, a, 0) : dsp3d_scene_update(s);
    if (r != DSP3D_OK) {
        s->count = a->track_count = a->key_count = 0;
    }
    return r;
}
