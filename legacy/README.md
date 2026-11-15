# Legacy dsp3D (2016)

This directory preserves the original engine, low-level interface template, and
Blender 2.67-era exporter. It is reference material and is not compiled into the
new library. The v2 API in `inc/dsp3D.h` uses typed indexed meshes, caller-owned
buffers, and an explicit context instead of global renderer state.

Existing v1 applications can retain this implementation while migrating their
hardware adapter to the v2 RGB565 framebuffer interface. See `docs/API.md`.
