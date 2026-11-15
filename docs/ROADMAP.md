# Suggested next improvements

This roadmap describes gaps in the current code, not features already delivered
or approved implementation work. Priorities favor useful Cortex-M capabilities
with measurable cost and preserve the board-independent library.

| Priority | Improvement | Current gap and intended benefit |
| --- | --- | --- |
| 1 | Viewport, scissor and framebuffer stride | Rendering currently uses a tightly packed full-screen buffer, including pixels later covered by the UI. Explicit viewport/stride support would isolate the 3D area and enable subwindows and lower-resolution targets. |
| 1 | Selectable/adaptive render resolution and LOD | Large zoom levels increase shaded-pixel cost; there is no automatic quality adjustment or mesh simplification. Measure lower-resolution rendering plus upscaling and offline LOD meshes against a frame-time budget. |
| 2 | OBJ/MTL import and material controls | Face colors already work, but STL does not carry the original artwork palette. Add PC-side OBJ/MTL conversion to existing face colors, named material presets and a color/light editor in the viewer. |
| 2 | Camera navigation and view reset | The camera API exists, but the touch viewer primarily rotates the object. Add orbit/pan, Fit/Reset, consistent zoom reset on model selection, and optional object picking. |
| 2 | A richer model browser | The current browser has a flat 32-file list, 63-character names and manual refresh. Add sorting, subdirectories, thumbnails, mesh metadata and a loading/progress view; validate insertion/removal with a real microSD. |
| 3 | Geometry preprocessing and memory optimization | Precompute face normals and bounds, measure object/frustum rejection, cache material palettes, and evaluate quantized positions/normals before changing the binary format. Retain a scalar reference and compatibility checks. |
| 3 | Scene hierarchy and animation | Multiple independent draws already share a depth buffer. Parent/child transforms, scene serialization, keyframes, skeletal animation and glTF import are not implemented. Start with transform animation and a small scene API. |
| 4 | UVs, textures and additional lighting | There are no UV coordinates, texture sampling, transparency, specular shading or shadows. Add a versioned format extension and perspective-correct UV interpolation before considering more costly effects. |

## Validation work

- Physical microSD/FAT32 tests remain pending; host parser checks are not a substitute.
- Add separate UI and presentation timing to the existing clear/draw/FPS metrics.
- Maintain cross-board build checks and image comparisons for the native renderer.
- Establish a reproducible same-resolution benchmark before comparing F746 and F769 MCU speed.
- Keep imported asset provenance and licenses alongside new model sources.

The first recommended implementation is viewport/stride support, followed by
measured resolution/LOD controls. Material import is the most direct visual
improvement for models such as the MetaMask fox.
