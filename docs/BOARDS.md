# Discovery board projects

Both projects compile the same library and viewer. HAL/BSP files remain under
`third_party/STM32CubeF7`; application-specific startup and memory decisions are
in `ports/common/board.c` and each board's linker/configuration directory.

| Property | STM32F746G-Discovery | STM32F769I-Discovery |
| --- | --- | --- |
| MCU define | STM32F746xx | STM32F769xx |
| CPU / SDRAM clock | 200 / 100 MHz | 200 / 100 MHz |
| Display | RGB LTDC, 480 x 272 | DSI video, 800 x 480 |
| Framebuffer | RGB565, two buffers | RGB565, two buffers |
| FPU compiler target | fpv5-sp-d16 | fpv5-d16 |
| Flash linker region | 1 MiB | 2 MiB |
| Internal data / stack | 64 KiB DTCM | 64 KiB DTCM |
| Physical SDRAM | 8 MiB, 16-bit bus | 16 MiB, 32-bit bus |
| Touch component | FT5336 | FT6x06 family (BSP panel detection) |
| Console | USART1 PA9/PB7 via BSP | USART1 PA9/PA10 |
| User button | BUTTON_KEY | BUTTON_USER |
| Verification | Compiled, flashed, display/touch confirmed by owner | Compiled and linked; board not attached |

Both builds currently use the first 8 MiB of SDRAM, with a final reserved probe
page. Layout is calculated and checked at startup: two color buffers, one float
depth buffer, scratch for 16,384 vertices, and two 2 MiB model arenas. F746 uses
about 5.5 MiB and F769 about 7.2 MiB, leaving the probe page unallocated. The
remaining F769 SDRAM is available to future applications, outside this demo's MPU
region. External SDRAM is initialized by the BSP before any application buffer
is accessed; startup does not zero uninitialized external memory.

Application state and the reserved 16 KiB stack use DTCM. DMA2D only accesses
aligned SDRAM framebuffers. SDMMC uses polling and a CPU-accessed sector buffer,
so it does not attempt DMA into DTCM. The MPU configures SDRAM as cacheable,
write-back, non-shareable and non-executable. Old back-buffer cache lines are
invalidated before complete DMA2D overwrite; rendered cache lines are cleaned
before the LTDC reload. The CPU waits for the vertical-blank reload callback
before reusing the previous front buffer.

The F746 LCD PLL override preserves a 9.6 MHz pixel clock and a 48 MHz SDMMC
kernel. The F769 retains the BSP's DSI video timing and 384 MHz PLLSAI VCO,
with its /8 48 MHz domain configured before LCD initialization. The library's
CMSIS routines are single precision on both devices; enabling the F769's double
FPU does not introduce double precision in the renderer's hot paths.

Build with `./tools/build.ps1 -Board f769`, then flash that board with
`./tools/flash.ps1 -Board f769`. The flash tool rejects the F746 device ID when
F769 firmware is selected. F769 hardware behavior, display revisions and touch
orientation must be verified on the physical second board.

Hardware references: the checked-in BSP source and
[ST UM2033](https://www.st.com.cn/resource/en/user_manual/um2033-discovery-kit-with-stm32f769ni-mcu-stmicroelectronics.pdf).
