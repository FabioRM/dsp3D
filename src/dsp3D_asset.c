/* SPDX-License-Identifier: BSD-3-Clause */
#include "dsp3D_asset.h"
#include <math.h>
#include <string.h>
typedef struct {
    const uint8_t *data;
    uint32_t count, stride, offset;
} chunk;
static uint32_t rd(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static float rf(const uint8_t *p) {
    uint32_t bits = rd(p);
    float value;
    memcpy(&value, &bits, 4);
    return value;
}
static chunk find(const uint8_t *p, uint32_t count, const char *name) {
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *d = p + 64 + i * 16;
        if (!memcmp(d, name, 4))
            return (chunk){p + rd(d + 4), rd(d + 8), rd(d + 12), rd(d + 4)};
    }
    return (chunk){0};
}
static int shape(chunk c, uint32_t count, uint32_t stride) {
    return c.data && c.count == count && c.stride == stride;
}
static void *take(uint8_t *workspace, size_t capacity, size_t *used, size_t bytes,
                  size_t alignment) {
    if (!workspace)
        return NULL;
    size_t padding = (alignment - (((uintptr_t)workspace + *used) % alignment)) % alignment;
    if (*used > capacity || padding > capacity - *used || bytes > capacity - *used - padding)
        return NULL;
    void *out = workspace + *used + padding;
    *used += padding + bytes;
    return out;
}
dsp3d_result dsp3d_asset_open(const void *data, size_t size, void *workspace, size_t capacity,
                              dsp3d_asset *asset) {
    if (!asset)
        return DSP3D_INVALID_ARGUMENT;
    memset(asset, 0, sizeof(*asset));
    if (!data || ((uintptr_t)data & 3u) || size < 8)
        return DSP3D_INVALID_ARGUMENT;
    const uint8_t *p = data;
    const uint16_t endian = 1;
    if (*(const uint8_t *)&endian != 1)
        return DSP3D_UNSUPPORTED_VERSION;
    if (memcmp(p, "DSP3", 4))
        return DSP3D_INVALID_MODEL;
    if (rd(p + 4) == 1) {
        dsp3d_mesh m;
        dsp3d_result r = dsp3d_model_open(data, size, &m);
        return r == DSP3D_OK ? dsp3d_geometry_init(&asset->geometry, &m, NULL) : r;
    }
    if (rd(p + 4) != 2)
        return DSP3D_UNSUPPORTED_VERSION;
    if (size < 64 || size != rd(p + 8))
        return DSP3D_INVALID_MODEL;
    uint32_t nv = rd(p + 16), nf = rd(p + 20), nc = rd(p + 24);
    if ((uint64_t)nv * sizeof(dsp3d_vertex) > SIZE_MAX)
        return DSP3D_INVALID_MODEL;
    if (!nv || !nf || !nc || nc > 32 || rd(p + 28) || (uint64_t)64 + nc * 16u > size)
        return DSP3D_INVALID_MODEL;
    if (dsp3d_crc32(p + 64, size - 64) != rd(p + 12))
        return DSP3D_CHECKSUM_ERROR;
    static const char known[][5] = {"VRTX", "VQ16", "TRIS", "UV00", "FNOR", "MIDX", "MATL", "TEXI",
                                    "TEXD", "ALPH", "NAME", "SKIN", "IBND", "JNOD", "THMB"};
    for (uint32_t i = 0; i < nc; i++) {
        const uint8_t *d = p + 64 + i * 16;
        uint32_t offset = rd(d + 4), count = rd(d + 8), stride = rd(d + 12);
        uint64_t bytes = (uint64_t)count * stride;
        int recognized = 0;
        for (unsigned k = 0; k < sizeof(known) / sizeof(known[0]); k++)
            recognized |= !memcmp(d, known[k], 4);
        if (!recognized)
            return DSP3D_UNSUPPORTED_VERSION;
        if (!count || !stride || (offset & 3u) || offset < 64 + nc * 16u || offset > size ||
            bytes > size - offset)
            return DSP3D_INVALID_MODEL;
        for (uint32_t j = 0; j < i; j++) {
            const uint8_t *other = p + 64 + j * 16;
            uint64_t begin = rd(other + 4), end = begin + (uint64_t)rd(other + 8) * rd(other + 12);
            if (!memcmp(d, other, 4) || (offset < end && (uint64_t)offset + bytes > begin))
                return DSP3D_INVALID_MODEL;
        }
    }
    chunk vertices = find(p, nc, "VRTX"), compact = find(p, nc, "VQ16"),
          triangles = find(p, nc, "TRIS"), uv = find(p, nc, "UV00"), normals = find(p, nc, "FNOR"),
          indices = find(p, nc, "MIDX"), materials = find(p, nc, "MATL"),
          texinfo = find(p, nc, "TEXI"), texels = find(p, nc, "TEXD"), alpha = find(p, nc, "ALPH"),
          names = find(p, nc, "NAME"), skin = find(p, nc, "SKIN"), bind = find(p, nc, "IBND"),
          joints = find(p, nc, "JNOD"), thumb = find(p, nc, "THMB");
    if (!shape(triangles, nf, 16) || (!!vertices.data + !!compact.data) != 1 ||
        (vertices.data && !shape(vertices, nv, 24)) || (compact.data && !shape(compact, nv, 12)) ||
        (uv.data && !shape(uv, nv, 8)) || (normals.data && !shape(normals, nf, 12)) ||
        (indices.data && !shape(indices, nf, 4)))
        return DSP3D_INVALID_MODEL;
    dsp3d_asset candidate = {0};
    dsp3d_geometry *g = &candidate.geometry;
    size_t used = 0;
    g->mesh = (dsp3d_mesh){(const dsp3d_vertex *)vertices.data,
                           (const dsp3d_triangle *)triangles.data, nv, nf};
    dsp3d_bounds bounds = {0};
    bounds.min = (dsp3d_vec3){rf(p + 32), rf(p + 36), rf(p + 40)};
    bounds.max = (dsp3d_vec3){rf(p + 44), rf(p + 48), rf(p + 52)};
    if (compact.data) {
        dsp3d_vertex *decoded =
            take(workspace, capacity, &used, (size_t)nv * sizeof(*decoded), _Alignof(dsp3d_vertex));
        if (!decoded)
            return DSP3D_INSUFFICIENT_MEMORY;
        dsp3d_result r = dsp3d_unpack_vertices((const dsp3d_compact_vertex *)compact.data, nv,
                                               &bounds, decoded, nv);
        if (r != DSP3D_OK)
            return r;
        g->mesh.vertices = decoded;
    }
    g->uv = (const dsp3d_uv *)uv.data;
    g->face_normals = (const dsp3d_vec3 *)normals.data;
    g->material_indices = (const uint32_t *)indices.data;
    if (materials.data && (!shape(materials, materials.count, 32) || materials.count > 256))
        return DSP3D_INVALID_MODEL;
    if (texinfo.data && (!shape(texinfo, texinfo.count, 24) || texinfo.count > 64 || !texels.data ||
                         texels.stride != 2))
        return DSP3D_INVALID_MODEL;
    if ((texels.data && !texinfo.data) || (alpha.data && (!texinfo.data || alpha.stride != 1)))
        return DSP3D_INVALID_MODEL;
    dsp3d_texture *textures = NULL;
    if (texinfo.data) {
        textures = take(workspace, capacity, &used, (size_t)texinfo.count * sizeof(*textures),
                        _Alignof(dsp3d_texture));
        if (!textures)
            return DSP3D_INSUFFICIENT_MEMORY;
        for (uint32_t i = 0; i < texinfo.count; i++) {
            const uint8_t *t = texinfo.data + i * 24;
            uint32_t w = rd(t), h = rd(t + 4), first = rd(t + 8), afirst = rd(t + 12),
                     flags = rd(t + 16);
            uint64_t count = (uint64_t)w * h;
            if (!w || !h || w > 2047 || h > 2047 || first > texels.count ||
                count > texels.count - first || flags > 1 || rd(t + 20))
                return DSP3D_INVALID_MODEL;
            if (afirst != UINT32_MAX &&
                (!alpha.data || afirst > alpha.count || count > alpha.count - afirst))
                return DSP3D_INVALID_MODEL;
            textures[i] = (dsp3d_texture){(const uint16_t *)texels.data + first,
                                          afirst == UINT32_MAX ? NULL : alpha.data + afirst,
                                          w,
                                          h,
                                          w,
                                          (uint8_t)flags};
        }
        candidate.texture_count = texinfo.count;
    }
    if (materials.data) {
        dsp3d_material *m = take(workspace, capacity, &used, (size_t)materials.count * sizeof(*m),
                                 _Alignof(dsp3d_material));
        if (!m)
            return DSP3D_INSUFFICIENT_MEMORY;
        for (uint32_t i = 0; i < materials.count; i++) {
            const uint8_t *v = materials.data + i * 32;
            uint32_t texture = rd(v + 4), opacity = rd(v + 16), flags = rd(v + 20);
            if ((texture != UINT32_MAX && texture >= candidate.texture_count) || opacity > 255 ||
                flags > 7 || rd(v + 24) || rd(v + 28))
                return DSP3D_INVALID_MODEL;
            m[i] = (dsp3d_material){rd(v),
                                    texture == UINT32_MAX ? NULL : textures + texture,
                                    rf(v + 8),
                                    rf(v + 12),
                                    (uint8_t)opacity,
                                    (uint8_t)flags};
        }
        g->materials = m;
        g->material_count = materials.count;
    }
    if (names.data) {
        if (!shape(names, g->material_count, 32))
            return DSP3D_INVALID_MODEL;
        for (uint32_t i = 0; i < names.count; i++)
            if (!memchr(names.data + i * 32, 0, 32))
                return DSP3D_INVALID_MODEL;
        candidate.material_names = (const char (*)[32])names.data;
    }
    if (skin.data || bind.data || joints.data) {
        if (!shape(skin, nv, 24) || !joints.count || joints.count > DSP3D_SKIN_MAX_JOINTS ||
            !shape(joints, joints.count, 4) || !shape(bind, joints.count, 64))
            return DSP3D_INVALID_MODEL;
        candidate.skin = (const dsp3d_skin_vertex *)skin.data;
        candidate.inverse_bind = (const dsp3d_mat4 *)bind.data;
        candidate.joint_nodes = (const uint32_t *)joints.data;
        candidate.joint_count = joints.count;
        for (uint32_t i = 0; i < nv; i++) {
            float sum = 0;
            for (unsigned j = 0; j < 4; j++) {
                float w = candidate.skin[i].weights[j];
                if (!isfinite(w) || w < 0 || w > 1 || candidate.skin[i].joints[j] >= joints.count)
                    return DSP3D_INVALID_MODEL;
                sum += w;
            }
            if (sum < 0.99f || sum > 1.01f)
                return DSP3D_INVALID_MODEL;
        }
        for (uint32_t i = 0; i < joints.count; i++) {
            dsp3d_mat4 inverse;
            if (dsp3d_matrix_inverse(candidate.inverse_bind + i, &inverse) != DSP3D_OK)
                return DSP3D_INVALID_MODEL;
        }
    }
    if (thumb.data) {
        uint32_t w = rd(p + 56), h = rd(p + 60);
        if (!w || !h || w > 64 || h > 64 || !shape(thumb, w * h, 2))
            return DSP3D_INVALID_MODEL;
        candidate.thumbnail = (const uint16_t *)thumb.data;
        candidate.thumbnail_width = w;
        candidate.thumbnail_height = h;
    } else if (rd(p + 56) || rd(p + 60))
        return DSP3D_INVALID_MODEL;
    dsp3d_result r = dsp3d_prepare_mesh(&g->mesh, NULL, &g->bounds);
    if (r != DSP3D_OK)
        return r;
    g->bounds_valid = 1;
    r = dsp3d_validate_geometry(g);
    if (r != DSP3D_OK)
        return r;
    *asset = candidate;
    return DSP3D_OK;
}
