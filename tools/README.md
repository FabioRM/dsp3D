# Local tools and offline operation

The supplied working directory contains private portable copies in `tools/local`:

| Directory | Purpose |
| --- | --- |
| `arm-gnu` | ARM GNU toolchain 14.3.1 from the installed STM32CubeIDE distribution |
| `openocd` | ST OpenOCD executable, DLLs, licenses and matching `scripts` |
| `programmer` | STM32CubeProgrammer copy (optional fallback) |
| `host-gnu` | Native Windows GCC toolchain for tests/reference rendering |
| `python` | Python standard-library runtime for conversion and framebuffer capture |
| `clang-format` | Optional source formatting tool |

`build.ps1`, `flash.ps1`, `test.ps1`, and asset/capture tools resolve executables
inside this directory. They do not download packages, query the network, or read
the external STM32Cube repository. Windows, its USB driver, and PowerShell remain
host prerequisites. PowerShell 7 is recommended. Paths may contain spaces.

Portable tools are ignored by Git because they are large workstation-specific
binary distributions. **This complete working folder is self-contained; a fresh
Git clone needs these local tool bundles supplied separately.** Copy this entire
folder, including `tools/local`, when moving it to another offline workstation.
Source SDK dependencies, generated assets, and build instructions are tracked.
Tool licenses are retained with each local distribution; redistribution of the
executables follows their original terms, independently of the dsp3D code license.

To populate another checkout manually, put complete portable distributions in the
directories above. For ARM GCC, retain `bin`, `lib`, and `arm-none-eabi` together.
For OpenOCD, retain the matching executable/DLLs and scripts from the same ST
distribution. The scripts use the native `stlink-dap` driver, compatible with the
attached ST-Link V2J24M11; CubeProgrammer 2.23 refuses that old probe firmware.
No ST-Link firmware update was required for the verified OpenOCD workflow.

## Commands

Run from the repository root:

```powershell
./tools/build.ps1 -Board f746
./tools/flash.ps1 -Board f746
./tools/monitor.ps1 -Port COM14 -Seconds 10
./tools/test.ps1 -Benchmark
./tools/regenerate-assets.ps1
./tools/capture.ps1 -Board f746 -Output build/capture.bmp
```

Build output includes `viewer.elf`, `.bin`, `.hex`, `.map`, and `libdsp3d.a`.
The flash script checks the MCU device ID before erasing firmware sectors.
It connects under reset so sleeping applications can be reprogrammed reliably.
The original F746 flash backup is in the ignored `build/original-f746-flash.bin`.

Serial commands (115200 baud, 8N1): `1` fox, `2` Suzanne, `m` mode, `r` automatic
rotation, `+`/`-` zoom, `c` culling, `s` SD browser, `b` deterministic benchmark.
The benchmark logs all eight model/mode combinations, 120 poses per case, using
the DWT cycle counter. Allow 30 seconds when collecting serial output. Build
with `-Scalar` and flash with `-Scalar` for the equivalent FPU scalar backend.

`capture.ps1` pauses the CPU, reads the displayed RGB565 framebuffer through SWD,
resumes execution, and writes a BMP. A capture takes several seconds and pauses
animation. The library also has an optional native CMake build; the PowerShell
workflow does not require CMake or Ninja.
