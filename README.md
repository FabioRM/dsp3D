# dsp3D
The dsp3D is a powerful 3D rendering engine designed for ARM Cortex-M processor based devices. It takes full advantage of the CMSIS DSP library to provide a fast operation.The presence of the floating point unit is recommended, better if it is double precision

The engine supports surface colors (one color per surface) and four rendering methods are available:
	- Gouraud rendering
	- Flat surface rendering
	- Wireframe rendering
	- Point rendering

# Tests and results
The engine were successfully tested on:
	- ST's STM32F746G-Discovery board (MCU: STM32F746) - Cortex-M7 @ 216MHz - 480x272 display
	- ST's STM32F769I-Discovery board (MCU: STM32F769) - Cortex-M7 @ 200MHz - 800x480 display
	- ST's STM32F429I-Discovery board (MCU: STM32F429) - Cortex-M4 @ 180MHz - 240x320 display

I made two demos to see on YouTube:
https://www.youtube.com/watch?v=GNKWmLiMpuk
