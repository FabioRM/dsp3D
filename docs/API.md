# Library API and integration

The C11 library is `inc/dsp3D.h`, `src/dsp3D.c`, and `src/dsp3D_model.c`.
It has no HAL, display, filesystem, operating-system, heap, or global renderer
dependency. Each context owns its camera, light, statistics, palette, and scratch
state; separate contexts can be used independently. A context is not safe for
simultaneous calls from multiple threads.

## Buffers and frame lifetime

Call `dsp3d_init` with width/height (1..2047), an RGB565 color buffer, a float depth
buffer, and scratch storage for at least the largest mesh's vertex count. The
buffers are tightly packed. Color requires two-byte alignment; depth and scratch
require four-byte alignment. Dimensions are bounded so the Cortex-M DSP backend's
signed halfword edge products cannot overflow. The context itself contains a
512-byte lighting palette and should normally be static or application-owned.

Call `dsp3d_begin_frame`, draw one or more meshes, then present the color buffer
using your display backend. Only the color pointer may change between frames for
double buffering. Keep the depth and scratch allocation stable. After replacing
the depth allocation, set `depth_initialized` to zero before beginning a frame.
The first frame clears the complete depth buffer; later frames clear the previous
frame's conservative dirty rectangle, including point and line rendering.

An accelerated backend can clear the color buffer itself and call
`dsp3d_clear_depth` to start a frame. The supplied STM32 port uses DMA2D, aligned
cache maintenance, and an LTDC vertical-blank reload before reusing the old front
buffer. CPU drawing must finish before the display takes ownership.

## Meshes and transforms

Mesh vertices contain position and unit normal. Triangles contain integer indices
and an RGB888 face color. The mesh data is immutable during rendering. Call
`dsp3d_validate_mesh` once for a caller-created mesh; `dsp3d_model_open` validates
binary models automatically. Do not modify validated vertices or indices without
validating again. Render transforms support finite Euler rotations in radians,
translation, and positive uniform scale. Normalize large rotations periodically
for the CMSIS fast-math approximation.

The world is right-handed and Y-up. Positive rotations use Rz * Ry * Rx. The
default camera is at (0,0,4), looking toward the origin. The camera field of view
is vertical, in radians; near and far are positive camera distances. Degenerate
camera/up configurations return an error. For stable numeric results, center and
normalize imported coordinates with the converter rather than passing extreme
floating-point magnitudes.

Normals rotate independently of translation. Lighting is ambient plus a
directional Lambert term. Flat mode computes the geometric face normal; Gouraud
mode shades each shared vertex once and interpolates intensity in screen space.
Gouraud intensity uses a 256-entry RGB565 palette. Wireframe interpolates depth;
point mode uses white vertices. Backface culling is enabled by default and can be
disabled for open or inconsistently wound meshes. There is no texture mapping.

Clipping covers all six view-frustum planes before perspective division.
Rasterization uses an exact 1/16-pixel grid and the top-left ownership rule;
reciprocal camera depth is interpolated across each triangle. Larger depth values
are closer. No division or hardware callback occurs in the filled-pixel loop.

## Integration examples

`examples/scenes/scenes.c` contains a minimal triangle, a hard-edged colored cube,
and a scene with independently transformed objects sharing one depth buffer.
`examples/desktop/preview.c` demonstrates host buffer ownership and BMP output.
`examples/viewer/main.c` is the complete interactive embedded application.

The native CMake target is `dsp3d`; applications link it and include `dsp3D.h`.
The board builds also produce `build/<board>/libdsp3d.a`. A Cortex-M7 build defines
`DSP3D_CMSIS`, `ARM_MATH_CM7`, and `__FPU_PRESENT=1U`, includes the vendored CMSIS
headers, and links the supplied single-precision CMSIS fast-math archive. DSP
instructions accelerate exact edge products and lighting saturation. Matrix and
vector operations are specialized, inline single-precision FPU arithmetic.
`-ffp-contract=fast` permits FMA; `-ffast-math` is deliberately avoided so model
validation and finite-value checks retain their specified meaning.

## Migrating from v1

Version 2 replaces global `dsp3D_set...` state and float-encoded mesh indices with
an explicit context, a typed mesh, and a transform argument to `dsp3d_draw`.
Map old render calls to the `DSP3D_FLAT`, `DSP3D_GOURAUD`, `DSP3D_WIREFRAME`, and
`DSP3D_POINTS` enum values. Camera and light settings live in the context.
Display presentation is the application's responsibility. This is a source API
change; the original v1 code and interface remain under `legacy/` for reference
and for applications that need the original implementation during migration.
