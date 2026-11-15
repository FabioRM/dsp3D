# dsp3D implementation plan

The project language is English, including source, comments, tools, UI, and documentation.

## Delivery sequence

1. Establish a self-contained source tree, preserve the original renderer, move source assets, and vendor the required STM32CubeF7 1.17.4 components with their licenses.
2. Build a board-independent C library with caller-owned memory, indexed meshes, safe clipping, correct lighting, four rendering modes, and a host reference backend.
3. Add deterministic STL conversion, an explicitly versioned binary format, embedded assets, and checked model loading.
4. Complete the STM32F746G-Discovery project: SDRAM, RGB565 double buffering, touch model viewer, diagnostics, and microSD browser.
5. Profile and optimize the Cortex-M7 implementation; record actual measurements and retain correctness tests.
6. Add the STM32F769I-Discovery project at 200 MHz / 800 x 480 using the same library and application.
7. Complete examples, English documentation, licensing, offline relocation checks, and commit each validated milestone. Push the work branch when ready.

## Acceptance criteria

- Firmware builds use only files within this directory, including the local compiler. No build-time network requests or references to the external STM32Cube repository.
- Both supplied meshes are selectable from embedded assets; FAT32 microSD models are optional and read-only.
- Rendering does not allocate memory or access storage per frame.
- Both board projects compile and link. Hardware tests are distinguished from compile/host checks; the F769 board is not assumed to be attached.
- Performance claims include model, resolution, mode, clock, and measurement method.
- Third-party licenses remain intact; supplied mesh provenance is documented without inventing licensing rights.

## Progress

- Initial inspection: original engine and Blender exporter present; no complete board firmware. MetaMask STL: 188 triangles (binary). Suzanne STL: 968 triangles (ASCII). F746 ST-Link enumerates, with VCP on COM14. Installed ARM GCC and CubeProgrammer available for making local tool copies.
