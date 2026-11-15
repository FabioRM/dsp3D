# Extended engine implementation

All work remains on `main`. Commit each verified feature increment; push only
after the implementation and available hardware validation are complete.

## Accepted scope

- [ ] Strided render targets, viewport and scissor; protected framebuffer regions.
- [ ] Selectable/adaptive resolution, measured upscaling and offline/runtime LOD.
- [ ] Bounds, prepared face normals, compact geometry and material palette reuse.
- [ ] OBJ/MTL and glTF conversion with explicit supported-format validation.
- [ ] Versioned extended assets retaining v1 compatibility, materials and UVs.
- [ ] Perspective-correct textures, alpha blending, specular lighting and shadows.
- [ ] Scene hierarchy, serialization, keyframes, skeletal deformation and picking.
- [ ] Orbit/pan/fit/reset, material/light controls and persistent preferences.
- [ ] Sorted hierarchical SD browser, thumbnails, metadata, load progress and media recovery.
- [ ] Embedded demonstration gallery covering every feature on both board builds.
- [ ] Separate clear/render/UI/presentation metrics and equal-resolution benchmarks.
- [ ] Native regression/image checks, converter tests and offline board builds.
- [ ] Flash all available boards, exercise examples and record actual measurements.
- [ ] Update English documentation and publish all commits at the end.

Physical microSD tests require a card; hardware absence is recorded separately
from software implementation and host validation. Importers reject unsupported
encodings/extensions rather than silently omitting their data.
