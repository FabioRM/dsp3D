# dsp3D
The dsp3D is a powerful 3D rendering engine designed for ARM Cortex-M processor based devices. It takes full advantage of the CMSIS DSP library to provide a fast operation. A device equipped also with a hardware floating point unit is recommended.

Four rendering methods are available:
	- Gouraud rendering
	- Flat surface rendering
	- Wireframe rendering
	- Point rendering

# Test with ST's 32F746-Discovery board (MCU: STM32F746)
It is capable to render the "Suzanne" head (968 faces, 507 vertices) in 100ms with Gouraud including the screen plotting.
The math operations required less than 20ms, the sub-optimal performance is caused by the time required to access the external SDRAM embedded on the board (for drawing on the screen and depth buffer r/w).
