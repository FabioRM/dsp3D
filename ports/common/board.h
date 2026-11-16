/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef DSP3D_BOARD_H
#define DSP3D_BOARD_H
#include "dsp3D.h"
#include "stm32f7xx_hal.h"
#ifdef BOARD_F769
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_lcd.h"
#include "stm32f769i_discovery_sdram.h"
#include "stm32f769i_discovery_sd.h"
#include "stm32f769i_discovery_ts.h"
#define BOARD_WIDTH 800u
#define BOARD_HEIGHT 480u
#define BOARD_NAME "STM32F769I Discovery"
#define BOARD_LTDC hltdc_discovery
#define BOARD_BUTTON BUTTON_USER
#else
#include "stm32746g_discovery.h"
#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery_sdram.h"
#include "stm32746g_discovery_sd.h"
#include "stm32746g_discovery_ts.h"
#define BOARD_WIDTH 480u
#define BOARD_HEIGHT 272u
#define BOARD_NAME "STM32F746G Discovery"
#define BOARD_LTDC hLtdcHandler
#define BOARD_BUTTON BUTTON_KEY
#endif
#define BOARD_MAX_VERTICES 16384u
#define BOARD_MODEL_BYTES (2u * 1024u * 1024u)
#define BOARD_WORKSPACE_BYTES (512u * 1024u)
#define BOARD_APP_BYTES (512u * 1024u)
extern LTDC_HandleTypeDef BOARD_LTDC;
extern volatile uint32_t board_frame_count;
extern volatile uint32_t board_render_cycles;
extern volatile uint32_t board_error_code;
void board_init(void);
void board_renderer_init(dsp3d_context *context);
void board_frame_begin(dsp3d_context *context, uint16_t background);
void board_present(dsp3d_context *context);
void *board_model_buffer(unsigned slot);
void *board_asset_workspace(unsigned slot);
void *board_app_buffer(void);
uint16_t *board_render_buffer(void);
int board_preferences_load(void *data, size_t bytes);
int board_preferences_save(const void *data, size_t bytes);
void board_log(const char *text);
int board_read_char(void);
void board_panic(uint32_t code);
uint32_t board_cycles(void);
void SysTick_Handler(void);
#endif
