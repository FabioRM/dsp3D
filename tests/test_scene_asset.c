/* SPDX-License-Identifier: BSD-3-Clause */
#include "dsp3D_asset.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static void *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    assert(f && fseek(f, 0, SEEK_END) == 0);
    *size = (size_t)ftell(f);
    rewind(f);
    void *p = malloc(*size);
    assert(p && fread(p, 1, *size, f) == *size);
    fclose(f);
    return p;
}
int main(int argc, char **argv) {
    assert(argc == 3);
    size_t ma, sc;
    void *model = read_file(argv[1], &ma), *bytes = read_file(argv[2], &sc);
    unsigned char workspace[8192];
    dsp3d_asset asset;
    assert(dsp3d_asset_open(model, ma, workspace, sizeof(workspace), &asset) == DSP3D_OK);
    const dsp3d_geometry *catalog[] = {&asset.geometry};
    dsp3d_node nodes[4];
    dsp3d_track tracks[4];
    dsp3d_keyframe keys[128];
    dsp3d_scene scene = {nodes, 0};
    dsp3d_animation animation = {tracks, keys, 0, 0, 0, 0};
    assert(dsp3d_scene_read(bytes, sc, &scene, 4, &animation, 4, 128, catalog, 1) == DSP3D_OK);
    assert(dsp3d_scene_animate(&scene, &animation, .5f) == DSP3D_OK);
    assert(fabsf(nodes[0].world.m[3] - .5f) < 1e-5f);
    free(model);
    free(bytes);
    puts("PASS: imported scene hierarchy and baked animation evaluated by C runtime");
}
