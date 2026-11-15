/* SPDX-License-Identifier: BSD-3-Clause */
#include "dsp3D_asset.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    long length = ftell(file);
    assert(length > 64);
    rewind(file);
    unsigned char *data = malloc((size_t)length);
    unsigned char *workspace = malloc(1024 * 1024);
    assert(data && workspace && fread(data, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    dsp3d_asset asset;
    assert(dsp3d_asset_open(data, (size_t)length, workspace, 1024 * 1024, &asset) == DSP3D_OK);
    assert(asset.geometry.mesh.vertex_count && asset.geometry.mesh.triangle_count);
    assert(asset.geometry.bounds_valid && asset.thumbnail_width == 32);
    assert(dsp3d_validate_geometry(&asset.geometry) == DSP3D_OK);
    for (size_t n = 0; n < (size_t)length; n += 37)
        assert(dsp3d_asset_open(data, n, workspace, 1024 * 1024, &asset) != DSP3D_OK);
    data[length - 1] ^= 1;
    assert(dsp3d_asset_open(data, (size_t)length, workspace, 1024 * 1024, &asset) == DSP3D_CHECKSUM_ERROR);
    data[length - 1] ^= 1;
    /* A valid CRC must not let overlapping chunks through. */
    memcpy(data + 64 + 16 + 4, data + 64 + 4, 4);
    uint32_t crc = dsp3d_crc32(data + 64, (size_t)length - 64);
    memcpy(data + 12, &crc, 4);
    assert(dsp3d_asset_open(data, (size_t)length, workspace, 1024 * 1024, &asset) == DSP3D_INVALID_MODEL);
    free(data);
    free(workspace);
    puts("PASS: extended assets, compact decoding, truncation, CRC and chunk overlap");
    return 0;
}
