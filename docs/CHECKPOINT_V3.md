# Development checkpoint — 2026-11-16

Development stopped at the owner's request. All current work is retained on
`main`; the final review and documentation pass is unfinished.

Implemented and committed:

- Strided render targets, viewports/scissors and RGB565 scaling; selectable
  full/half/quarter resolution and adaptive quality with hysteresis.
- Prepared face normals/bounds, conservative object rejection, compact vertices,
  cached material palettes and three embedded Suzanne LOD levels.
- Affine/quaternion math, camera orbit/pan/fit/reset and geometric picking.
- Perspective-correct textures, alpha materials, specular highlights and
  directional planar shadows. Transparency sorts objects; it is not an
  order-independent renderer or a shadow-map implementation.
- Scene hierarchies, serialization, TRS keyframes and four-weight skeletal skinning.
- Extended v2 assets with materials, UVs, textures, skin data and thumbnails;
  legacy v1 model compatibility.
- OBJ/MTL and glTF/GLB conversion, with explicit rejection of unsupported input
  encodings/features. glTF base color/roughness is approximated by the renderer's
  lighting model; this is not a full PBR implementation.
- Ten embedded gallery examples, large F769 touch controls, material/light
  presets and explicit preference saves to the reserved last flash sector.
- Sorted SD folders, thumbnails, progress, transactional model/scene replacement,
  scene catalogs, animated/skinned playback and media recovery.
- Separate clear/render/scale/UI/presentation telemetry; a 400 x 240 benchmark
  selected with UART `t` or `tools/benchmark.ps1 -EqualResolution`.
- J-Link framebuffer capture without resetting the running application.

## Evidence collected

`tools/test.ps1` passed native renderer/asset/importer/gallery tests and actual
viewer integration tests with host-backed storage, including interrupted reads,
corrupt assets/scenes, rollback, folders and skinned scene playback.
Both board firmware builds passed. Latest build sizes:

| Board | Flash used | DTCM data |
| --- | ---: | ---: |
| F746 | 197,824 bytes | 8,392 bytes |
| F769 | 210,144 bytes | 8,680 bytes |

The latest F769 image was flashed and verified through J-Link. All ten gallery
entries ran and produced telemetry. The owner exercised gallery examples and
settings. The F746 was unavailable for flashing this revision. No physical
microSD card was available; host storage tests do not validate SDMMC hardware.

The F769 200 MHz DSP benchmark uses 120 deterministic poses per case, at
400 x 240, with CPU clearing of the offscreen target. Render times exclude
clearing, scaling, GUI and display synchronization:

| Model | Flat mean | Gouraud mean | Clear mean, flat |
| --- | ---: | ---: | ---: |
| Fox | 5,575 us | 6,032 us | 983 us |
| Suzanne | 7,612 us | 7,452 us | 897 us |

Full data: [400 x 240 DSP CSV](measurements/f769-v3-400x240-dsp.csv).
These are not directly comparable to earlier full-screen/DMA-clear results.
[Gallery telemetry](measurements/f769-gallery-v3.log) records all ten examples
at an 800 x 280 render area. That capture used paused scenes; further animation
validation should use playback enabled. The owner subsequently exercised the
interactive gallery. Initial on-board optimization reduced native-size copy
from roughly 15.9 ms to 4.6 ms in the observed case; the static fox subsequently
rendered around 7.7 ms with approximately 31 interactive FPS. These observations
are not a controlled before/after benchmark.

## Remaining work before declaring the full expansion complete

- Finish the code review and update API/format/browser/board documentation and
  the older roadmap, which still describes the pre-expansion code.
- Integrate all new public headers and examples into CMake installation/tests;
  add CI and stable image regression references.
- Complete final controlled gallery/animation/adaptive-resolution/picking tests,
  verify preference persistence across reboot, and collect final performance
  tables. A same-resolution F746 measurement requires reconnecting that board.
- Repeat relocation validation with the final sources; prepare a final release
  version and documentation cross-check.
- Flash the updated F746 when available and test a physical FAT32 microSD.

The complete local folder retains portable tools under ignored `tools/local/`.
All firmware source dependencies and generated example assets are tracked.
