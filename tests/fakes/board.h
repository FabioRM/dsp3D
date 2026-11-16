/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef DSP3D_FAKE_BOARD_H
#define DSP3D_FAKE_BOARD_H
#include "dsp3D.h"
#define BOARD_WIDTH 800u
#define BOARD_HEIGHT 480u
#define BOARD_NAME "Native viewer fixture"
#define BOARD_MODEL_BYTES (2u*1024u*1024u)
#define BOARD_WORKSPACE_BYTES (512u*1024u)
#define BOARD_APP_BYTES (512u*1024u)
#define BOARD_MAX_VERTICES 16384u
#define BOARD_BUTTON 0
#define RESET 0
#define TS_OK 0
#define SD_PRESENT 1
typedef struct { unsigned touchDetected, touchX[5], touchY[5]; } TS_StateTypeDef;
extern uint32_t SystemCoreClock;
extern volatile uint32_t board_frame_count,board_render_cycles;
void board_init(void);
void board_renderer_init(dsp3d_context *c);
void board_frame_begin(dsp3d_context *c,uint16_t background);
void board_present(dsp3d_context *c);
void *board_model_buffer(unsigned slot);
void *board_asset_workspace(unsigned slot);
void *board_app_buffer(void);
uint16_t *board_render_buffer(void);
void board_log(const char *text);
int board_read_char(void);
uint32_t board_cycles(void);
void board_panic(uint32_t code);
uint32_t HAL_GetTick(void);
int BSP_PB_GetState(unsigned button);
int BSP_TS_GetState(TS_StateTypeDef *state);
unsigned BSP_SD_IsDetected(void);
int board_preferences_save(const void *data,size_t bytes);
int board_preferences_load(void *data,size_t bytes);
#endif
