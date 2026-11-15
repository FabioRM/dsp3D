/* SPDX-License-Identifier: BSD-3-Clause
 * Touch model gallery: embedded assets and read-only microSD models. */
#include "board.h"
#include "metamask.h"
#include "suzanne.h"
#include "ff.h"
#include "fonts.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#define TOP 30
#define BOTTOM 46
#define FILE_LIMIT 32
static dsp3d_context renderer;
static const dsp3d_mesh *current = &metamask;
static dsp3d_mesh loaded;
static dsp3d_transform transform = {{0, 0, 0}, {0, 0, 0}, 1.05f};
static dsp3d_mode mode = DSP3D_FLAT;
static FATFS filesystem;
static char names[FILE_LIMIT][64];
static unsigned file_count, page, loaded_slot, browser, autorotate = 1;
static char model_name[64] = "MetaMask Fox",
            status[96] = "Embedded models ready. Tap SD to browse /models.";
static const char *const modes[] = {"Flat", "Gouraud", "Wireframe", "Points"};
volatile uint32_t viewer_command;
static void rect(int x, int y, int w, int h, uint32_t rgb) {
    uint16_t color = dsp3d_rgb565(rgb);
    for (int j = y; j < y + h && j < (int)BOARD_HEIGHT; j++)
        for (int i = x; i < x + w && i < (int)BOARD_WIDTH; i++)
            if (i >= 0 && j >= 0)
                renderer.color[j * BOARD_WIDTH + i] = color;
}
static void text(int x, int y, const char *s, uint32_t rgb) {
    uint16_t color = dsp3d_rgb565(rgb);
    sFONT *font = &Font12;
    while (*s && x + (int)font->Width <= (int)BOARD_WIDTH) {
        unsigned ch = (unsigned char)*s++;
        if (ch < 32 || ch > 126)
            ch = '?';
        const uint8_t *glyph = font->table + (ch - 32) * font->Height * ((font->Width + 7) / 8);
        unsigned bytes = (font->Width + 7) / 8;
        for (unsigned row = 0; row < font->Height; row++)
            for (unsigned col = 0; col < font->Width; col++) {
                if ((glyph[row * bytes + col / 8] & (0x80u >> (col & 7))) && x + (int)col >= 0 &&
                    y + (int)row >= 0 && y + row < BOARD_HEIGHT)
                    renderer.color[(y + row) * BOARD_WIDTH + x + col] = color;
            }
        x += font->Width;
    }
}
static void embedded(unsigned n) {
    current = n ? &suzanne : &metamask;
    snprintf(model_name, sizeof(model_name), "%s", n ? "Suzanne" : "MetaMask Fox");
    mode = n ? DSP3D_GOURAUD : DSP3D_FLAT;
    browser = 0;
    transform.rotation = (dsp3d_vec3){0, 0, 0};
    snprintf(status, sizeof(status), "Embedded asset | Drag to rotate | Pinch or +/- to zoom");
}
static unsigned rows(void) {
    return (BOARD_HEIGHT - TOP - BOTTOM - 25) / 28;
}
static void scan(void) {
    file_count = page = 0;
    browser = 1;
    f_mount(NULL, "0:", 0);
    FRESULT result = f_mount(&filesystem, "0:", 1);
    if (result != FR_OK) {
        snprintf(status, sizeof(status),
                 "SD unavailable (%u). Insert a FAT32 card and tap Refresh.", (unsigned)result);
        return;
    }
    DIR dir;
    result = f_opendir(&dir, "0:/models");
    if (result != FR_OK) {
        snprintf(status, sizeof(status), "Cannot open /models (%u).", (unsigned)result);
        return;
    }
    FILINFO info;
    while (file_count < FILE_LIMIT && (result = f_readdir(&dir, &info)) == FR_OK && info.fname[0]) {
        size_t len = strlen(info.fname);
        if (!(info.fattrib & AM_DIR) && len > 4 && len < sizeof(names[0]) &&
            tolower((unsigned char)info.fname[len - 3]) == 'd' &&
            tolower((unsigned char)info.fname[len - 2]) == '3' &&
            tolower((unsigned char)info.fname[len - 1]) == 'm' && info.fname[len - 4] == '.') {
            memcpy(names[file_count++], info.fname, len + 1);
        }
    }
    f_closedir(&dir);
    if (result != FR_OK)
        snprintf(status, sizeof(status), "SD directory read failed (%u).", (unsigned)result);
    else
        snprintf(status, sizeof(status), "%u models found%s. Tap a row to load.", file_count,
                 file_count == FILE_LIMIT ? " (list limit)" : "");
}
static void load(unsigned i) {
    if (i >= file_count)
        return;
    char path[80];
    snprintf(path, sizeof(path), "0:/models/%s", names[i]);
    FIL file;
    FRESULT result = f_open(&file, path, FA_READ);
    if (result != FR_OK) {
        snprintf(status, sizeof(status), "Open failed (%u). Tap Refresh after reinserting SD.",
                 (unsigned)result);
        return;
    }
    FSIZE_t size = f_size(&file);
    unsigned slot = loaded_slot ^ 1u;
    void *buffer = board_model_buffer(slot);
    if (size > BOARD_MODEL_BYTES || size < 32) {
        f_close(&file);
        snprintf(status, sizeof(status), "Unsupported file size (maximum 2 MiB).");
        return;
    }
    UINT received = 0;
    result = f_read(&file, buffer, (UINT)size, &received);
    f_close(&file);
    if (result != FR_OK || received != size) {
        snprintf(status, sizeof(status), "SD read failed (%u). Previous model retained.",
                 (unsigned)result);
        return;
    }
    dsp3d_mesh candidate;
    dsp3d_result r = dsp3d_model_open(buffer, size, &candidate);
    if (r == DSP3D_OK && candidate.vertex_count > BOARD_MAX_VERTICES)
        r = DSP3D_INSUFFICIENT_MEMORY;
    if (r != DSP3D_OK) {
        snprintf(status, sizeof(status), "Load failed: %s", dsp3d_result_string(r));
        return;
    }
    loaded = candidate;
    current = &loaded;
    loaded_slot = slot;
    browser = 0;
    snprintf(model_name, sizeof(model_name), "%s", names[i]);
    transform.rotation = (dsp3d_vec3){0, 0, 0};
    snprintf(status, sizeof(status), "Loaded from SD | %lu bytes | Model stays in RAM",
             (unsigned long)size);
    board_log("Loaded model from microSD.\r\n");
}
static void benchmark(void) {
    char log[220];
    board_log("BENCH,backend,board,width,height,clock_hz,model,mode,frames,mean_render_us,min_"
              "render_us,max_render_us,mean_clear_us\r\n");
    for (unsigned model = 0; model < 2; model++)
        for (int rendering = 0; rendering < 4; rendering++) {
            uint64_t sum = 0, clear_sum = 0;
            uint32_t minimum = UINT32_MAX, maximum = 0;
            for (unsigned frame = 0; frame < 120; frame++) {
                dsp3d_transform pose = {{0.2f, (float)frame * 0.025f, 0}, {0, 0, 0}, 1.05f};
                uint32_t a = board_cycles();
                board_frame_begin(&renderer, 0);
                uint32_t b = board_cycles();
                dsp3d_draw(&renderer, model ? &suzanne : &metamask, &pose, (dsp3d_mode)rendering);
                uint32_t cycles = board_cycles() - b;
                sum += cycles;
                clear_sum += (uint32_t)(b - a);
                if (cycles < minimum)
                    minimum = cycles;
                if (cycles > maximum)
                    maximum = cycles;
            }
            unsigned divisor = SystemCoreClock / 1000000u;
#ifdef DSP3D_CMSIS
            const char *backend = "cmsis-dsp";
#else
            const char *backend = "scalar";
#endif
            snprintf(log, sizeof(log), "BENCH,%s,%s,%u,%u,%lu,%s,%s,120,%lu,%lu,%lu,%lu\r\n",
                     backend, BOARD_NAME, BOARD_WIDTH, BOARD_HEIGHT, (unsigned long)SystemCoreClock,
                     model ? "suzanne" : "metamask", modes[rendering],
                     (unsigned long)(sum / 120 / divisor), (unsigned long)(minimum / divisor),
                     (unsigned long)(maximum / divisor),
                     (unsigned long)(clear_sum / 120 / divisor));
            board_log(log);
        }
    board_log("BENCH complete\r\n");
}
static void command(int ch) {
    switch (ch) {
    case '1':
        embedded(0);
        break;
    case '2':
        embedded(1);
        break;
    case 'm':
        mode = (dsp3d_mode)((mode + 1) % 4);
        break;
    case 'r':
        autorotate ^= 1;
        break;
    case 's':
        scan();
        board_log(status);
        board_log("\r\n");
        break;
    case '+':
        transform.scale = fminf(2.5f, transform.scale * 1.15f);
        break;
    case '-':
        transform.scale = fmaxf(0.3f, transform.scale / 1.15f);
        break;
    case 'c':
        renderer.backface_culling ^= 1;
        break;
    case 'b':
        benchmark();
        break;
    default:
        break;
    }
}
static void tap(unsigned x, unsigned y) {
    if (y < TOP) {
        if (x < BOARD_WIDTH / 3)
            embedded(0);
        else if (x < BOARD_WIDTH * 2 / 3)
            embedded(1);
        else
            scan();
        return;
    }
    if (browser) {
        if (y >= BOARD_HEIGHT - BOTTOM) {
            if (x < BOARD_WIDTH / 3) {
                if (page)
                    page--;
            } else if (x < BOARD_WIDTH * 2 / 3) {
                if ((page + 1) * rows() < file_count)
                    page++;
            } else
                scan();
        } else if (y >= TOP + 25) {
            unsigned row = (y - TOP - 25) / 28;
            if (row < rows())
                load(page * rows() + row);
        }
        return;
    }
    if (y >= BOARD_HEIGHT - BOTTOM) {
        unsigned cell = x / (BOARD_WIDTH / 5);
        if (cell == 0)
            command('m');
        else if (cell == 1)
            command('r');
        else if (cell == 2)
            command('-');
        else if (cell == 3)
            command('+');
        else
            command('c');
    }
}
static void ui(unsigned render_us, unsigned fps10) {
    rect(0, 0, BOARD_WIDTH, TOP, 0x14253b);
    text(10, 9, "MetaMask Fox", 0xffbb64);
    text(BOARD_WIDTH / 3 + 10, 9, "Suzanne", 0x91d1ff);
    text(BOARD_WIDTH * 2 / 3 + 10, 9, "SD Models", 0xb0f0bc);
    rect(0, BOARD_HEIGHT - BOTTOM, BOARD_WIDTH, BOTTOM, 0x14253b);
    char line[100];
    if (browser) {
        rect(0, TOP, BOARD_WIDTH, BOARD_HEIGHT - TOP - BOTTOM, 0x0b1423);
        text(12, TOP + 6, "MICROSD /models - read only", 0xffffff);
        for (unsigned i = 0; i < rows() && page * rows() + i < file_count; i++) {
            int y = TOP + 25 + (int)i * 28;
            rect(8, y, BOARD_WIDTH - 16, 25, 0x1d334b);
            text(16, y + 6, names[page * rows() + i], 0xffffff);
        }
        snprintf(line, sizeof(line), "Previous          Next          Refresh   Page %u", page + 1);
        text(10, BOARD_HEIGHT - BOTTOM + 8, line, 0xffffff);
    } else {
        unsigned cell = BOARD_WIDTH / 5;
        text(8, BOARD_HEIGHT - BOTTOM + 6, modes[mode], 0xffffff);
        text(cell + 4, BOARD_HEIGHT - BOTTOM + 6, autorotate ? "Pause" : "Rotate", 0xffffff);
        text(cell * 2 + 8, BOARD_HEIGHT - BOTTOM + 6, "Zoom -", 0xffffff);
        text(cell * 3 + 8, BOARD_HEIGHT - BOTTOM + 6, "Zoom +", 0xffffff);
        text(cell * 4 + 4, BOARD_HEIGHT - BOTTOM + 6,
             renderer.backface_culling ? "Cull ON" : "Cull OFF", 0xffffff);
        snprintf(line, sizeof(line), "%s | %lu tris | %u.%u FPS | render %u us", model_name,
                 (unsigned long)current->triangle_count, fps10 / 10, fps10 % 10, render_us);
        rect(0, TOP, BOARD_WIDTH, 17, 0x0b1423);
        text(6, TOP + 2, line, 0xc6d8eb);
    }
    text(6, BOARD_HEIGHT - 17, status, 0xaabdd1);
}
int main(void) {
    board_init();
    board_renderer_init(&renderer);
    if (dsp3d_validate_mesh(&metamask) != DSP3D_OK || dsp3d_validate_mesh(&suzanne) != DSP3D_OK)
        board_panic(15);
    uint32_t last = HAL_GetTick(), last_log = last, last_frame = last;
    unsigned fps10 = 0;
    TS_StateTypeDef touch = {0};
    unsigned previous_touch = 0, previous_button = 0;
    int previous_x = 0, previous_y = 0;
    float previous_span = 0;
    board_log("Controls: 1/2 model, m mode, r rotate, +/- zoom, c culling, s SD browser.\r\n");
    for (;;) {
        uint32_t now = HAL_GetTick();
        float dt = (float)(now - last) * 0.001f;
        last = now;
        if (dt > 0.1f)
            dt = 0.1f;
        unsigned button = BSP_PB_GetState(BOARD_BUTTON) != RESET;
        if (button && !previous_button)
            embedded(current == &metamask ? 1 : 0);
        previous_button = button;
        int ch;
        while ((ch = board_read_char()) >= 0)
            command(ch);
        if (viewer_command) {
            command((int)viewer_command);
            viewer_command = 0;
        }
        if (BSP_TS_GetState(&touch) == TS_OK) {
            unsigned down = touch.touchDetected;
            if (down) {
                int x = touch.touchX[0], y = touch.touchY[0];
                if (!previous_touch)
                    tap((unsigned)x, (unsigned)y);
                else if (!browser && y > TOP + 17 && y < (int)BOARD_HEIGHT - BOTTOM &&
                         previous_y > TOP + 17 && previous_y < (int)BOARD_HEIGHT - BOTTOM) {
                    if (down == 1) {
                        transform.rotation.y += (float)(x - previous_x) * 0.012f;
                        transform.rotation.x += (float)(y - previous_y) * 0.012f;
                        autorotate = 0;
                    } else {
                        float dx = (float)x - touch.touchX[1], dy = (float)y - touch.touchY[1],
                              span = sqrtf(dx * dx + dy * dy);
                        if (previous_touch > 1 && previous_span > 10)
                            transform.scale =
                                fmaxf(0.3f, fminf(2.5f, transform.scale * span / previous_span));
                        previous_span = span;
                    }
                }
                previous_x = x;
                previous_y = y;
            }
            previous_touch = down;
        }
        if (autorotate && !browser && !previous_touch)
            transform.rotation.y += dt * 0.45f;
        transform.rotation.y = remainderf(transform.rotation.y, 6.2831853f);
        transform.rotation.x = remainderf(transform.rotation.x, 6.2831853f);
        uint32_t clear_start = board_cycles();
        board_frame_begin(&renderer, dsp3d_rgb565(0x0b1423));
        uint32_t start = board_cycles();
        if (!browser) {
            dsp3d_result result = dsp3d_draw(&renderer, current, &transform, mode);
            if (result != DSP3D_OK)
                snprintf(status, sizeof(status), "Render failed: %s", dsp3d_result_string(result));
        }
        board_render_cycles = board_cycles() - start;
        unsigned render_us = board_render_cycles / (SystemCoreClock / 1000000u);
        ui(render_us, fps10);
        board_present(&renderer);
        now = HAL_GetTick();
        uint32_t elapsed = now - last_frame;
        last_frame = now;
        if (elapsed)
            fps10 = 10000 / elapsed;
        if (now - last_log >= 1000) {
            char log[180];
            snprintf(log, sizeof(log),
                     "frame=%lu model=%s mode=%s render_us=%u clear_us=%lu fps10=%u tris=%lu "
                     "pixels=%lu\r\n",
                     (unsigned long)board_frame_count, model_name, modes[mode], render_us,
                     (unsigned long)((start - clear_start) / (SystemCoreClock / 1000000u)), fps10,
                     (unsigned long)renderer.stats.rasterized_triangles,
                     (unsigned long)renderer.stats.shaded_pixels);
            board_log(log);
            last_log = now;
            if (BSP_SD_IsDetected() != SD_PRESENT && current == &loaded)
                snprintf(status, sizeof(status),
                         "SD removed. Loaded model remains available in RAM.");
        }
    }
}
