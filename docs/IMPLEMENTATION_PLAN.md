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
- Completed: portable C11 library, typed meshes, six-plane clipping, exact subpixel rasterization, DSP and scalar backends, checked binary loader, standalone STL converter and native tests.
- Completed: local SDK/tool bundles, F746 firmware build/programming, SDRAM test, RGB565 double buffering, touch controls, embedded models, serial diagnostics and on-board DWT benchmarks. The owner confirmed that display and touch work.
- Completed: read-only microSD browser and transactional loader. Host parser/CRC/error tests pass. No physical microSD is available; card/SDMMC testing is pending.
- Completed: F769 200 MHz / 800 x 480 project compiles and links against the same library/viewer. Subsequently flashed through its SEGGER J-Link interface; SDRAM, live rendering and serial controls verified, with display/touch confirmed by the owner. All eight DSP benchmark cases are recorded.
- Completed: triangle/cube/multi-object examples, portable native preview/benchmark, framebuffer capture, English API/model/board/tool documentation, BSD copyright update and third-party/asset notices.
- Completed: relocated builds in a path containing spaces, with PATH restricted to Windows System32; both firmware binaries match the original builds byte for byte. Native CMake build/test/install also passes.
- Completed: final F746 DSP/scalar benchmarks, on-board absent-card recovery, actual LCD framebuffer captures, and UART overrun recovery after debugger pauses. Final firmware is running on the F746.
- Delivery: progress commits published on `codex/standalone-cortex-m-library`. Both boards are hardware verified. Physical microSD testing remains pending because no card is available.
