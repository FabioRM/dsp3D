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
The dsp3D_ll provides low level interface to the hardware.
The following takes advantage of the ST HAL libraris, specifically the ones
for ST's 32F746-Discovery board.
******************************************************************************/

#include "dsp3d_ll.h"

uint32_t LCD_ActiveLayer = 1;

void dsp3D_LL_drawPoint(int32_t x, int32_t y, color32_t color)
{
	BSP_LCD_DrawPixel(x, y, color);
}

void dsp3D_LL_clearScreen(color32_t color)
{
	BSP_LCD_Clear(color);
}

/******************************************************************************
This function comes from the work of Clemente Di Caprio on Github
https://github.com/cledic/STM32F7
******************************************************************************/
void dsp3D_LL_switchScreen(void)
{
	while (!(LTDC->CDSR & LTDC_CDSR_VSYNCS));
	BSP_LCD_SetLayerVisible( !LCD_ActiveLayer, DISABLE);
	while (!(LTDC->CDSR & LTDC_CDSR_VSYNCS));
	BSP_LCD_SetLayerVisible( LCD_ActiveLayer, ENABLE);
	BSP_LCD_SelectLayer(!LCD_ActiveLayer);
	while (!(LTDC->CDSR & LTDC_CDSR_VSYNCS));
	BSP_LCD_SetLayerVisible( !LCD_ActiveLayer, DISABLE);
	
	LCD_ActiveLayer = !LCD_ActiveLayer;
}

void dsp3D_LL_writeToDepthBuffer(uint32_t pos, float32_t value)
{
	*(__IO float32_t*) (DEPTHBUFFER_ADDRESS + pos) = value;
}

float32_t dsp3D_LL_readFromDepthBuffer(uint32_t pos)
{
	return *(__IO float32_t*) (DEPTHBUFFER_ADDRESS + pos);
}

void dsp3D_LL_clearDepthBuffer(void)
{
	int32_t x, y;

	for(x = 0; x < SCREEN_WIDTH; x++)
		for(y = 0;y < SCREEN_HEIGHT; y++)
			dsp3D_LL_writeToDepthBuffer((x + y * SCREEN_WIDTH) * 4, FLT_MAX);
}

void dsp3D_LL_printMatrix(float32_t *data, uint32_t length)
{
	int32_t x, y;
	
	for(x = 0; x < length; x++)
	{
		for(y = 0; y < length; y++)
		{
			printf("%f\t", data[x * length + y]);
		}
		printf("\r\n");
	}
	printf("\r\n\r\n");
}

void dsp3D_LL_printVector(float32_t *data, uint32_t length)
{
	int32_t x;
	
	for(x = 0; x < length; x++)
	{
		printf("%f\t", data[x]);
		printf("\r\n");
	}
	printf("\r\n\r\n");
}

