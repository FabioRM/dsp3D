/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 Fabio Angeletti */
#include "dsp3D.h"
#include <math.h>
#include <string.h>
_Static_assert(sizeof(dsp3d_vertex) == 24, "Model vertex layout");
_Static_assert(sizeof(dsp3d_triangle) == 16, "Model triangle layout");
_Static_assert(sizeof(float) == 4, "IEEE-754 binary32 required");
static uint32_t u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
uint32_t dsp3d_crc32(const void *data, size_t size) {
    const uint8_t *p = data;
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; i++) {
        crc ^= p[i];
        for (unsigned b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
dsp3d_result dsp3d_validate_mesh(const dsp3d_mesh *m) {
    if (!m || !m->vertices || !m->triangles || !m->vertex_count || !m->triangle_count)
        return DSP3D_INVALID_MODEL;
    for (uint32_t i = 0; i < m->vertex_count; i++) {
        const dsp3d_vertex *v = m->vertices + i;
        if (!isfinite(v->position.x) || !isfinite(v->position.y) || !isfinite(v->position.z) ||
            !isfinite(v->normal.x) || !isfinite(v->normal.y) || !isfinite(v->normal.z))
            return DSP3D_INVALID_MODEL;
        float n = v->normal.x * v->normal.x + v->normal.y * v->normal.y + v->normal.z * v->normal.z;
        if (n < 0.99f || n > 1.01f)
            return DSP3D_INVALID_MODEL;
    }
    for (uint32_t i = 0; i < m->triangle_count; i++)
        for (unsigned j = 0; j < 3; j++)
            if (m->triangles[i].index[j] >= m->vertex_count)
                return DSP3D_INVALID_MODEL;
    return DSP3D_OK;
}
dsp3d_result dsp3d_model_open(const void *data, size_t size, dsp3d_mesh *m) {
    if (m)
        memset(m, 0, sizeof(*m));
    if (!data || !m || ((uintptr_t)data & 3u))
        return DSP3D_INVALID_ARGUMENT;
    const uint8_t *p = data;
    const uint16_t endian = 1;
    if (*(const uint8_t *)&endian != 1)
        return DSP3D_UNSUPPORTED_VERSION;
    if (size < 32 || memcmp(p, "DSP3", 4))
        return DSP3D_INVALID_MODEL;
    if (u32(p + 4) != DSP3D_MODEL_VERSION)
        return DSP3D_UNSUPPORTED_VERSION;
    uint32_t nv = u32(p + 8), nf = u32(p + 12), bytes = u32(p + 16);
    if (!nv || !nf || u32(p + 24) || u32(p + 28) || (uint64_t)nv * 24u + 32u > SIZE_MAX)
        return DSP3D_INVALID_MODEL;
    size_t vbytes = (size_t)nv * 24;
    if ((uint64_t)nf * 16u + vbytes + 32u > SIZE_MAX)
        return DSP3D_INVALID_MODEL;
    size_t expected = vbytes + (size_t)nf * 16;
    if (expected != bytes || expected != size - 32)
        return DSP3D_INVALID_MODEL;
    if (dsp3d_crc32(p + 32, expected) != u32(p + 20))
        return DSP3D_CHECKSUM_ERROR;
    dsp3d_mesh candidate = {(const dsp3d_vertex *)(p + 32),
                            (const dsp3d_triangle *)(p + 32 + vbytes), nv, nf};
    dsp3d_result result = dsp3d_validate_mesh(&candidate);
    if (result == DSP3D_OK)
        *m = candidate;
    return result;
}
const char *dsp3d_result_string(dsp3d_result r) {
    static const char *const names[] = {"OK",
                                        "Invalid argument",
                                        "Invalid model",
                                        "Unsupported model version",
                                        "Checksum mismatch",
                                        "Insufficient memory"};
    return (unsigned)r < sizeof(names) / sizeof(names[0]) ? names[r] : "Unknown error";
}
