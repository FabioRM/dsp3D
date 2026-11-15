# Performance measurements

Measured on the attached STM32F746G-Discovery at 200 MHz, 480 x 272, with SDRAM,
I-cache and D-cache enabled, RGB565 double buffering and the LCD scanning during
the benchmark. Compiler: ARM GNU 14.3.1, `-O3`, hard single-precision FPU,
`-ffp-contract=fast`, without `-ffast-math` or LTO.

Each case draws 120 deterministic poses: X rotation 0.2 radians, Y rotation
`frame * 0.025`, scale 1.05, camera (0,0,4), default light, and backface culling.
The DWT cycle counter measures clear and draw independently. Clear includes
DMA2D color fill and dirty-region float-depth clearing. Render excludes UI,
buffer presentation/vblank waits, serial logging and clearing. Minimum/maximum
values vary with pose, interrupts, SDRAM contention and cold caches. This is not
a measurement of the original 2016 engine.

## Final library and assets

Mean time in microseconds. Fox: 188 triangles / 521 vertices. Suzanne: 968
triangles / 505 smooth vertices. Both backends use the same source and assets;
the scalar build replaces CMSIS trigonometry and DSP intrinsics with FPU/C code.

| Model | Mode | CMSIS/DSP render | Scalar render | CMSIS/DSP clear |
| --- | --- | ---: | ---: | ---: |
| Fox | Flat | 6,449 | 6,431 | 2,667 |
| Fox | Gouraud | 7,183 | 8,358 | 2,646 |
| Fox | Wireframe | 3,367 | 3,357 | 2,656 |
| Fox | Points | 1,048 | 1,045 | 2,661 |
| Suzanne | Flat | 7,768 | 7,732 | 2,455 |
| Suzanne | Gouraud | 7,884 | 8,555 | 2,454 |
| Suzanne | Wireframe | 7,644 | 7,655 | 2,461 |
| Suzanne | Points | 1,086 | 1,088 | 2,463 |

Gouraud is about 14% faster for the fox and 8% faster for Suzanne with the DSP
backend. Other modes are close; small differences should be treated as noise.
The assembly was checked for `SMUAD`, `USAT`, and hardware floating-point
operations. Keeping a scalar implementation provides a useful reference and
avoids claiming that every CMSIS call is inherently faster.

The interactive viewer was observed around 59 FPS on the attached LCD for the
tested poses. Display synchronization and UI work affect frame rate; reciprocal
render-only timings must not be advertised as interactive FPS. No F769 hardware
performance is claimed: that project is compile/link verified only.

Raw final results, including per-case minima and maxima:
[DSP](measurements/f746-dsp.csv), [scalar](measurements/f746-scalar.csv).

## Optimization progression

Early v2 measurements used a 65-degree Suzanne crease setting with 1,143 vertices:

| Change | Before | After |
| --- | ---: | ---: |
| Full depth clear to unrolled dirty-region clear | about 7.3 ms clear | about 2.5-2.7 ms clear |
| Early geometric culling / mode-specific vertex work, Suzanne flat | about 11.4 ms draw | about 8.7 ms draw |
| Smooth Suzanne sharing, final setup improvements, Suzanne flat | about 8.7 ms draw | about 7.8 ms draw |

The final Suzanne conversion changes the smoothing policy as well as reducing
vertex work: 505 shared smooth vertices instead of 1,143 crease-split vertices.
The triangle geometry is unchanged; normals and Gouraud appearance improve.
Its binary asset shrinks from 42,952 to 27,640 bytes. The largest gains came from
reducing memory traffic and repeated work, alongside the per-pixel DSP path.

## Reproduce

```powershell
./tools/build.ps1 -Board f746
./tools/flash.ps1 -Board f746
./tools/benchmark.ps1 -Port COM14 -Output build/f746/dsp.csv
./tools/build.ps1 -Board f746 -Scalar
./tools/flash.ps1 -Board f746 -Scalar
./tools/benchmark.ps1 -Port COM14 -Output build/f746/scalar.csv
```

Restore the default firmware with `./tools/flash.ps1 -Board f746` afterward.
Keep culling enabled and do not operate touch controls while starting a benchmark.
The benchmark completes synchronously before the interactive event loop resumes.
`tools/test.ps1 -Benchmark` additionally writes a native reference benchmark for
both resolutions; host times are not predictions of microcontroller performance.
