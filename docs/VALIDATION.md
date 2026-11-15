# Validation record

Development and validation dates: 15-16 November 2026 (Europe/Rome).

## Passed

- Native GCC build with `-Wall -Wextra -Wpedantic -Werror`, assertions enabled.
- Native renderer tests: checked model opening, CRC reference vector, truncated
  data, bad alignment/version/checksum, non-finite model coordinates, depth,
  culling, all four rendering modes, translation-invariant normals, insufficient
  scratch, degenerate camera, exact shared-edge ownership, dirty/empty frame
  clears, and 1,000 deterministic clipping-stress cases with color/depth guards.
- Python converter tests: exact reproduction of checked-in binary assets,
  source triangle counts, finite/unit normals, malformed STL rejection, binary
  STL detection even with a misleading `solid` header.
- Independent native CMake/Ninja build and CTest run.
- Both board projects compile and link with ARM GCC 14.3.1. ELF program headers
  separate executable flash and writable non-executable data segments.
- Relocation check: copied build inputs and compiler to `build/relocation check`,
  restricted PATH to Windows System32, rebuilt both targets, and compared SHA-256
  hashes. Both firmware binaries matched the original builds exactly.
- F746 flash write and read-back verification through local OpenOCD, MCU ID
  0x449, ST-Link V2J24M11. Original 1 MiB flash preserved locally in
  `build/original-f746-flash.bin` before programming.
- F746 SDRAM probe passed. LCD updates, buffer swaps, both embedded models,
  serial commands, and all render modes exercised. The owner independently
  confirmed that the display and touch work.
- F746 deterministic on-board benchmarks captured for both scalar and DSP
  builds. See [performance](PERFORMANCE.md).
- F746 absent-card path returns FatFs result 3 (`FR_NOT_READY`), displays the
  recovery instructions, and continues presenting frames.
- Hardware RGB565 framebuffers read through SWD and visually inspected. These
  images show actual board output, not host mockups.
- Serial model switching retested after a long SWD halt. Explicit UART overrun
  recovery keeps commands working when the debugger pauses the receive loop.
- Native CMake library installation checked: static archive and public header
  install successfully into an independent prefix.
- F769 flash write/read-back verification through local CubeProgrammer and the
  onboard SEGGER J-Link interface, MCU ID 0x451. Original 2 MiB flash backed up
  locally in `build/original-f769-flash.bin` before programming.
- F769 application start, SDRAM probe, frame presentation, both models and serial
  controls verified on COM15. The owner confirmed that display and touch work.
- All eight F769 CMSIS/DSP benchmark cases captured at 200 MHz / 800 x 480.
  See [performance](PERFORMANCE.md) and the checked-in measurement CSV.

## Remaining physical checks

- No microSD is available. SDMMC transfers, FAT32 enumeration/loading with a real
  card, removal/reinsertion and physical-media errors are implemented but have
  not been exercised on hardware. Host binary-loader checks do not replace this.
- The supplied Windows native GCC distribution does not include the sanitizer
  runtime/spec files, so an attempted UBSan build was unavailable. Guard-buffer,
  exact coverage and deterministic stress tests passed without a sanitizer.

## F769 touch layout update (16 November)

- F769 tabs and bottom controls now occupy 64-pixel-high touch areas. Tab text
  uses Font24, controls and file rows Font20, and metadata/status Font16.
- SD rows are 56 pixels high; filenames are shortened with ellipses and status
  text wraps across two lines. Four rows fit per F769 browser page.
- Hit areas follow the layout constants, status text is not interactive, and
  object gestures must start in the 3D area instead of a toolbar.
- Both F769 and F746 firmware compile. The F769 firmware was flashed through
  J-Link, boot/SDRAM/frame activity checked on COM15, and the owner confirmed
  that the new controls are comfortably sized.
- Native visual checks reused the firmware UI code and BSP fonts for the model
  view, a populated browser with a long filename, and absent-card messaging.
  Those previews use illustrative counters, not new performance measurements.

## Hardware captures

![F746 fox](images/f746-fox.png)

![F746 Suzanne](images/f746-suzanne.png)

![F746 absent microSD recovery](images/f746-sd-absent.png)
