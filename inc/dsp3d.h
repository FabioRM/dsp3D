/******************************************************************************
Copyright (c) 2016 - Fabio Angeletti
e-mail: fabio.angeletti89@gmail.com
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of dsp3D nor the names of its contributors may be used
  to endorse or promote products derived from this software without
  specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This work was inspired by the excellent tutorial series by David Rousset:
"learning how to write a 3D soft engine from scratch in C#, TypeScript
or JavaScript" - available on David's website https://www.davrous.com

******************************************************************************/

/******************************************************************************
The dsp3D is a powerful 3D rendering engine designed for ARM Cortex-M processor
based devices. It takes full advantage of the CMSIS DSP library to provide a
fast operation. A device equipped also with a hardware floating point unit is
recommended.

Four rendering methods are available:
	- Gouraud rendering
	- Flat surface rendering
	- Wireframe rendering
	- Point rendering

After rendering, the screen need to be drawn. Use dsp3D_present

It is easily extensible to support different face colors and maybe textures.
Tested on ST's 32F746-Discovery board
******************************************************************************/

#ifndef __DSP3D_ENGINE__
#define __DSP3D_ENGINE__

#include "stm32f7xx_hal.h"
#include "float.h"
#include "arm_math.h"
#include "dsp3d_ll.h"
#include "genericMesh.h"

#define ABS(x)   		((x) > 0 ? (x) : -(x))
#define MIN(x, y)		((x) > (y) ? (y) : (x))
#define MAX(x, y)		((x) < (y) ? (y) : (x))
#define ROUND(x) 		((x)>=0?(int32_t)((x)+0.5):(int32_t)((x)-0.5))

#define color32_t	uint32_t

void dsp3D_setCameraPosition(float32_t x, float32_t y, float32_t z);
void dsp3D_setCameraTarget(float32_t x, float32_t y, float32_t z);
void dsp3D_setMeshPosition(float32_t x, float32_t y, float32_t z);	
void dsp3D_setMeshRotation(float32_t yaw, float32_t pitch, float32_t roll);
void dsp3D_setLightPosition(float32_t x, float32_t y, float32_t z);	

void dsp3D_init(void);
void dsp3D_clearAll(void);

void dsp3D_renderGouraud(void *meshPointer);
void dsp3D_renderFlat(void *meshPointer);
void dsp3D_renderWireframe(void *meshPointer);
void dsp3D_renderPoints(void *meshPointer);

void dsp3D_present(void);
void dsp3D_presentAndClearDepthBuffer(void);
void dsp3D_generateMatrices(void);

/* Debugging purposes *****************************************************/
void dsp3D_renderPoint(float32_t x, float32_t y, float32_t z);

#endif
