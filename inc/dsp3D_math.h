/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef DSP3D_MATH_H
#define DSP3D_MATH_H
#include "dsp3D_geometry.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float x, y, z, w;
} dsp3d_quat;
typedef struct {
    float m[16];
} dsp3d_mat4; /* row-major, column vectors */
typedef struct {
    dsp3d_vec3 target;
    float yaw, pitch, distance;
} dsp3d_orbit;
typedef struct {
    float distance;
    uint32_t triangle;
    dsp3d_vec3 position;
} dsp3d_hit;
dsp3d_vec3 dsp3d_vadd(dsp3d_vec3 a, dsp3d_vec3 b);
dsp3d_vec3 dsp3d_vsub(dsp3d_vec3 a, dsp3d_vec3 b);
dsp3d_vec3 dsp3d_vscale(dsp3d_vec3 v, float s);
float dsp3d_vdot(dsp3d_vec3 a, dsp3d_vec3 b);
dsp3d_vec3 dsp3d_vcross(dsp3d_vec3 a, dsp3d_vec3 b);
dsp3d_vec3 dsp3d_vunit(dsp3d_vec3 v);
dsp3d_quat dsp3d_quat_euler(dsp3d_vec3 radians);
dsp3d_quat dsp3d_quat_slerp(dsp3d_quat a, dsp3d_quat b, float t);
dsp3d_mat4 dsp3d_identity(void);
dsp3d_mat4 dsp3d_matrix_trs(dsp3d_vec3 translation, dsp3d_quat rotation, dsp3d_vec3 scale);
dsp3d_mat4 dsp3d_matrix_multiply(const dsp3d_mat4 *a, const dsp3d_mat4 *b);
dsp3d_vec3 dsp3d_matrix_point(const dsp3d_mat4 *m, dsp3d_vec3 v);
dsp3d_vec3 dsp3d_matrix_vector(const dsp3d_mat4 *m, dsp3d_vec3 v);
dsp3d_result dsp3d_matrix_inverse(const dsp3d_mat4 *m, dsp3d_mat4 *out);
void dsp3d_orbit_apply(const dsp3d_orbit *orbit, dsp3d_camera *camera);
void dsp3d_camera_pan(dsp3d_camera *camera, float horizontal, float vertical);
dsp3d_result dsp3d_camera_fit(dsp3d_camera *camera, const dsp3d_bounds *bounds, float aspect,
                              float margin);
dsp3d_result dsp3d_pick_mesh(const dsp3d_camera *camera, dsp3d_rect viewport, float x, float y,
                             const dsp3d_mesh *mesh, const dsp3d_mat4 *world, dsp3d_hit *hit);
#ifdef __cplusplus
}
#endif
#endif
