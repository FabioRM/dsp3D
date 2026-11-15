/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef DSP3D_SCENES_H
#define DSP3D_SCENES_H
#include "dsp3D.h"
extern const dsp3d_mesh dsp3d_example_triangle;
extern const dsp3d_mesh dsp3d_example_cube;
/* Draw two independently transformed objects into the same depth buffer. */
dsp3d_result dsp3d_example_scene(dsp3d_context *ctx, float angle, dsp3d_mode mode);
#endif
