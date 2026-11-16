/* SPDX-License-Identifier: BSD-3-Clause
 * Board startup and coherent CPU / DMA2D / LTDC ownership. */
#include "board.h"
#include <string.h>
volatile uint32_t board_frame_count, board_render_cycles, board_error_code;
static UART_HandleTypeDef console;
static DMA2D_HandleTypeDef fill;
static uint16_t *buffers[2];
static float *depth_buffer;
static dsp3d_cached_vertex *vertex_cache;
static uint8_t *model_buffers[2];
static uint8_t *asset_workspaces[2], *app_buffer;
static uint16_t *render_buffer;
#ifdef BOARD_F769
#define SDRAM_LIMIT 0xc0fff000u
#define PREF_ADDRESS 0x081c0000u
#define PREF_SECTOR FLASH_SECTOR_11
#else
#define SDRAM_LIMIT 0xc07ff000u
#define PREF_ADDRESS 0x080c0000u
#define PREF_SECTOR FLASH_SECTOR_7
#endif
static unsigned back = 1;
static volatile unsigned reload_pending;
static size_t aligned(size_t n) {
    return (n + 31u) & ~(size_t)31u;
}
void board_panic(uint32_t code) {
    board_error_code = code;
    for (;;) {
        BSP_LED_Toggle(LED1);
        HAL_Delay(150);
    }
}
static void clock_init(void) {
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 25;
    osc.PLL.PLLN = 400;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 8;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK || HAL_PWREx_ActivateOverDrive() != HAL_OK)
        board_panic(1);
    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType =
        RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_6) != HAL_OK)
        board_panic(2);
    /* 48 MHz SDMMC domain independent of the 200 MHz CPU / 100 MHz SDRAM. */
    RCC_PeriphCLKInitTypeDef periph = {0};
    periph.PeriphClockSelection = RCC_PERIPHCLK_CLK48 | RCC_PERIPHCLK_SDMMC1;
    periph.PLLSAI.PLLSAIN = 384;
    periph.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV8;
    periph.PLLSAI.PLLSAIQ = 8;
    periph.PLLSAI.PLLSAIR = 5;
    periph.PLLSAIDivQ = 1;
    periph.PLLSAIDivR = RCC_PLLSAIDIVR_2;
    periph.Clk48ClockSelection = RCC_CLK48SOURCE_PLLSAIP;
    periph.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_CLK48;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK)
        board_panic(3);
}
static void mpu_init(void) {
    MPU_Region_InitTypeDef m = {0};
    HAL_MPU_Disable();
    m.Enable = MPU_REGION_ENABLE;
    m.Number = MPU_REGION_NUMBER0;
    m.BaseAddress = 0xc0000000;
#ifdef BOARD_F769
    m.Size = MPU_REGION_SIZE_16MB;
#else
    m.Size = MPU_REGION_SIZE_8MB;
#endif
    m.AccessPermission = MPU_REGION_FULL_ACCESS;
    m.IsBufferable = MPU_ACCESS_BUFFERABLE;
    m.IsCacheable = MPU_ACCESS_CACHEABLE;
    m.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    m.TypeExtField = MPU_TEX_LEVEL0;
    m.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    HAL_MPU_ConfigRegion(&m);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
#ifndef BOARD_F769
/* Keep the LCD PLL and SDMMC kernel consistent: 192/4 = 48 MHz. */
void BSP_LCD_ClockConfig(LTDC_HandleTypeDef *ltdc, void *params) {
    (void)ltdc;
    (void)params;
    RCC_PeriphCLKInitTypeDef p = {0};
    p.PeriphClockSelection = RCC_PERIPHCLK_LTDC | RCC_PERIPHCLK_CLK48 | RCC_PERIPHCLK_SDMMC1;
    p.PLLSAI.PLLSAIN = 192;
    p.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV4;
    p.PLLSAI.PLLSAIQ = 4;
    p.PLLSAI.PLLSAIR = 5;
    p.PLLSAIDivQ = 1;
    p.PLLSAIDivR = RCC_PLLSAIDIVR_4;
    p.Clk48ClockSelection = RCC_CLK48SOURCE_PLLSAIP;
    p.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_CLK48;
    if (HAL_RCCEx_PeriphCLKConfig(&p) != HAL_OK)
        board_panic(16);
}
#endif
static void console_init(void) {
    console.Init.BaudRate = 115200;
    console.Init.WordLength = UART_WORDLENGTH_8B;
    console.Init.StopBits = UART_STOPBITS_1;
    console.Init.Parity = UART_PARITY_NONE;
    console.Init.Mode = UART_MODE_TX_RX;
    console.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    console.Init.OverSampling = UART_OVERSAMPLING_16;
#ifdef BOARD_F769
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef pins = {0};
    pins.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    pins.Mode = GPIO_MODE_AF_PP;
    pins.Pull = GPIO_PULLUP;
    pins.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    pins.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &pins);
    console.Instance = USART1;
    if (HAL_UART_Init(&console) != HAL_OK)
        board_panic(4);
#else
    BSP_COM_Init(COM1, &console);
#endif
}
void board_log(const char *text) {
    HAL_UART_Transmit(&console, (uint8_t *)text, (uint16_t)strlen(text), 100);
}
int board_read_char(void) {
    /* Debugger pauses and rendering may overrun the single-byte receive register.
     * Clear ORE explicitly; zero-timeout HAL polling can leave it latched. */
    if (__HAL_UART_GET_FLAG(&console, UART_FLAG_ORE))
        __HAL_UART_CLEAR_OREFLAG(&console);
    if (__HAL_UART_GET_FLAG(&console, UART_FLAG_RXNE))
        return (int)(console.Instance->RDR & 255u);
    return -1;
}
void board_init(void) {
    mpu_init();
    SCB_EnableICache();
    SCB_EnableDCache();
    HAL_Init();
    DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP;
    BSP_LED_Init(LED1);
    clock_init();
    console_init();
    BSP_PB_Init(BOARD_BUTTON, BUTTON_MODE_GPIO);
    BSP_SD_Detect_MspInit(NULL,NULL);
    board_log("\r\ndsp3D " DSP3D_VERSION " / " BOARD_NAME " / 200 MHz\r\n");
    if (BSP_LCD_Init() != LCD_OK)
        board_panic(5);
    /* The BSP LCD initialization also initializes SDRAM. Test a reserved final page. */
    volatile uint32_t *probe = (volatile uint32_t *)SDRAM_LIMIT;
    for (unsigned i = 0; i < 1024; i++)
        probe[i] = 0x5a5a0000u ^ i;
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)probe, 4096);
    for (unsigned i = 0; i < 1024; i++)
        if (probe[i] != (0x5a5a0000u ^ i))
            board_panic(6);
    uintptr_t p = 0xc0000000;
    size_t color_bytes = aligned(BOARD_WIDTH * BOARD_HEIGHT * 2u);
    buffers[0] = (uint16_t *)p;
    p += color_bytes;
    buffers[1] = (uint16_t *)p;
    p += color_bytes;
    depth_buffer = (float *)p;
    p += aligned(BOARD_WIDTH * BOARD_HEIGHT * 4u);
    vertex_cache = (dsp3d_cached_vertex *)p;
    p += aligned(BOARD_MAX_VERTICES * sizeof(*vertex_cache));
    model_buffers[0] = (uint8_t *)p;
    p += BOARD_MODEL_BYTES;
    model_buffers[1] = (uint8_t *)p;
    p += BOARD_MODEL_BYTES;
    render_buffer = (uint16_t *)p;
    p += color_bytes;
    asset_workspaces[0] = (uint8_t *)p;
    p += BOARD_WORKSPACE_BYTES;
    asset_workspaces[1] = (uint8_t *)p;
    p += BOARD_WORKSPACE_BYTES;
    app_buffer = (uint8_t *)p;
    p += BOARD_APP_BYTES;
    if (p > SDRAM_LIMIT)
        board_panic(7);
    memset(buffers[0], 0, color_bytes * 2);
    SCB_CleanDCache_by_Addr((uint32_t *)buffers[0], (int32_t)(color_bytes * 2));
    LTDC_LayerCfgTypeDef layer = {0};
    layer.WindowX1 = BOARD_WIDTH;
    layer.WindowY1 = BOARD_HEIGHT;
    layer.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
    layer.FBStartAdress = (uint32_t)buffers[0];
    layer.Alpha = 255;
    layer.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
    layer.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
    layer.ImageWidth = BOARD_WIDTH;
    layer.ImageHeight = BOARD_HEIGHT;
    if (HAL_LTDC_ConfigLayer(&BOARD_LTDC, &layer, 0) != HAL_OK)
        board_panic(8);
    __HAL_LTDC_LAYER_DISABLE(&BOARD_LTDC, 1);
    __HAL_LTDC_RELOAD_CONFIG(&BOARD_LTDC);
    HAL_NVIC_SetPriority(LTDC_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(LTDC_IRQn);
    BSP_LCD_DisplayOn();
    if (BSP_TS_Init(BOARD_WIDTH, BOARD_HEIGHT) != TS_OK)
        board_log("Touch initialization failed; use the user button.\r\n");
    __HAL_RCC_DMA2D_CLK_ENABLE();
    fill.Instance = DMA2D;
    fill.Init.Mode = DMA2D_R2M;
    fill.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    fill.Init.OutputOffset = 0;
    if (HAL_DMA2D_Init(&fill) != HAL_OK)
        board_panic(9);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    /* Cortex-M7 implements the optional CoreSight software lock. */
    *(volatile uint32_t *)0xe0001fb0 = 0xc5acce55;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB();
    __ISB();
    board_log("SDRAM probe passed; LCD and renderer buffers ready.\r\n");
}
void board_renderer_init(dsp3d_context *c) {
    if (dsp3d_init(c, BOARD_WIDTH, BOARD_HEIGHT, buffers[back], depth_buffer, vertex_cache,
                   BOARD_MAX_VERTICES) != DSP3D_OK)
        board_panic(10);
}
void board_frame_begin(dsp3d_context *c, uint16_t background) {
    c->color = buffers[back];
    int32_t bytes = (int32_t)aligned(BOARD_WIDTH * BOARD_HEIGHT * 2u);
    /* Every cache line is overwritten; discard dirty old back-buffer pixels. */
    SCB_InvalidateDCache_by_Addr((uint32_t *)c->color, bytes);
    __DSB();
    uint32_t rgb = ((uint32_t)(background >> 11) * 255u / 31u) << 16;
    rgb |= ((uint32_t)((background >> 5) & 63) * 255u / 63u) << 8;
    rgb |= (uint32_t)(background & 31) * 255u / 31u;
    if (HAL_DMA2D_Start(&fill, rgb, (uint32_t)c->color, BOARD_WIDTH, BOARD_HEIGHT) != HAL_OK ||
        HAL_DMA2D_PollForTransfer(&fill, 100) != HAL_OK)
        board_panic(11);
    SCB_InvalidateDCache_by_Addr((uint32_t *)c->color, bytes);
    if(c->depth) dsp3d_clear_depth(c);
}
void board_present(dsp3d_context *c) {
    SCB_CleanDCache_by_Addr((uint32_t *)c->color,
                            (int32_t)aligned(BOARD_WIDTH * BOARD_HEIGHT * 2u));
    __DSB();
    if (HAL_LTDC_SetAddress_NoReload(&BOARD_LTDC, (uint32_t)c->color, 0) != HAL_OK)
        board_panic(12);
    reload_pending = 1;
    if (HAL_LTDC_Reload(&BOARD_LTDC, LTDC_RELOAD_VERTICAL_BLANKING) != HAL_OK)
        board_panic(13);
    uint32_t start = HAL_GetTick();
    while (reload_pending) {
        if (HAL_GetTick() - start > 100)
            board_panic(14);
        __WFI();
    }
    back ^= 1;
    board_frame_count++;
}
void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *h) {
    (void)h;
    reload_pending = 0;
}
void LTDC_IRQHandler(void) {
    HAL_LTDC_IRQHandler(&BOARD_LTDC);
}
#ifdef BOARD_F769
extern DSI_HandleTypeDef hdsi_discovery;
void DSI_IRQHandler(void) {
    HAL_DSI_IRQHandler(&hdsi_discovery);
}
#endif
void SysTick_Handler(void) {
    HAL_IncTick();
}
void HardFault_Handler(void) {
    board_error_code = 0x48465254;
    for (;;) {
        __NOP();
    }
}
void MemManage_Handler(void) {
    board_error_code = 0x4d454d;
    for (;;) {
        __NOP();
    }
}
void BusFault_Handler(void) {
    board_error_code = 0x425553;
    for (;;) {
        __NOP();
    }
}
void UsageFault_Handler(void) {
    board_error_code = 0x555347;
    for (;;) {
        __NOP();
    }
}
uint32_t board_cycles(void) {
    return DWT->CYCCNT;
}
void *board_model_buffer(unsigned slot) {
    return model_buffers[slot & 1u];
}
void *board_asset_workspace(unsigned slot) { return asset_workspaces[slot & 1u]; }
void *board_app_buffer(void) { return app_buffer; }
uint16_t *board_render_buffer(void) { return render_buffer; }
/* Append-only records. The final flash sector is excluded by the linker.
 * A commit word is programmed last; interrupted writes leave the previous record usable. */
int board_preferences_load(void *data, size_t bytes) {
    if (!data || bytes > 112) return 0;
    const uint32_t *latest = NULL;
    for (uintptr_t p=PREF_ADDRESS; p<PREF_ADDRESS+256u*1024u; p+=128) {
        const uint32_t *record=(const uint32_t *)p;
        if (record[0]==0x46525044 && record[1]==bytes && record[31]==0x434f4d54 &&
            record[30]==dsp3d_crc32(record,120)) latest=record;
    }
    if (!latest) return 0;
    memcpy(data,latest+2,bytes);
    return 1;
}
int board_preferences_save(const void *data, size_t bytes) {
    if (!data || bytes > 112) return 0;
    uint32_t record[32];
    memset(record,255,sizeof(record));
    record[0]=0x46525044;record[1]=(uint32_t)bytes;
    memcpy(record+2,data,bytes);
    record[30]=dsp3d_crc32(record,120);record[31]=0x434f4d54;
    uintptr_t target=0;
    for (uintptr_t p=PREF_ADDRESS; p<PREF_ADDRESS+256u*1024u && !target; p+=128) {
        const uint32_t *old=(const uint32_t *)p;
        unsigned blank=1;
        for(unsigned i=0;i<32;i++) blank &= old[i]==UINT32_MAX;
        if(blank) target=p;
    }
    if(HAL_FLASH_Unlock()!=HAL_OK) return 0;
    int ok=1;
    if(!target) {
        FLASH_EraseInitTypeDef erase={0};
        erase.TypeErase=FLASH_TYPEERASE_SECTORS;erase.Sector=PREF_SECTOR;
        erase.NbSectors=1;erase.VoltageRange=FLASH_VOLTAGE_RANGE_3;
        uint32_t error;
        ok=HAL_FLASHEx_Erase(&erase,&error)==HAL_OK;
        target=PREF_ADDRESS;
    }
    for(unsigned i=0;i<32 && ok;i++)
        ok=HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,(uint32_t)target+i*4,record[i])==HAL_OK;
    HAL_FLASH_Lock();
    SCB_InvalidateDCache_by_Addr((uint32_t *)target,128);
    return ok && !memcmp((const void *)target,record,sizeof(record));
}
