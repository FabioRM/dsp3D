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
#ifdef BOARD_F769
#define TOP 64
#define CONTROLS 64
#define STATUS_HEIGHT 40
#define INFO_HEIGHT 32
#define LIST_HEADER 40
#define LIST_ROW 56
#define TAB_FONT Font24
#define CONTROL_FONT Font20
#define BODY_FONT Font16
#define FILE_FONT Font20
#else
#define TOP 30
#define CONTROLS 29
#define STATUS_HEIGHT 17
#define INFO_HEIGHT 17
#define LIST_HEADER 25
#define LIST_ROW 28
#define TAB_FONT Font12
#define CONTROL_FONT Font12
#define BODY_FONT Font12
#define FILE_FONT Font12
#endif
#define BOTTOM (CONTROLS + STATUS_HEIGHT)
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
static void text_font(int x, int y, const char *s, uint32_t rgb, const sFONT *font, int right) {
    uint16_t color = dsp3d_rgb565(rgb);
    if (right > (int)BOARD_WIDTH)
        right = BOARD_WIDTH;
    while (*s && x + (int)font->Width <= right) {
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
static void text(int x, int y, const char *s, uint32_t rgb) {
    text_font(x, y, s, rgb, &BODY_FONT, BOARD_WIDTH);
}
static void button(unsigned column, unsigned columns, int y, int height, const char *label,
                   uint32_t accent, int active, const sFONT *font) {
    int left = (int)(column * BOARD_WIDTH / columns);
    int right = (int)((column + 1) * BOARD_WIDTH / columns);
#ifdef BOARD_F769
    int x = left + 5, width = right - left - 10;
    rect(x, y + 5, width, height - 10, active ? accent : 0x36516c);
    rect(x + 2, y + 7, width - 4, height - 14, active ? 0x274868 : 0x1c334b);
    unsigned length = (unsigned)strlen(label);
    int text_x = x + (width - (int)(length * font->Width)) / 2;
    text_font(text_x, y + (height - font->Height) / 2, label, accent, font, right - 7);
#else
    (void)active;
    text_font(left + 8, y + (height - font->Height) / 2, label, accent, font, right - 2);
#endif
}
static void status_text(void) {
#ifdef BOARD_F769
    /* Wrap status messages rather than shrinking the font or hiding their ending. */
    const unsigned capacity = (BOARD_WIDTH - 24) / BODY_FONT.Width;
    const char *remaining = status;
    for (unsigned row = 0; row < 2 && *remaining; row++) {
        unsigned count = (unsigned)strlen(remaining);
        if (count > capacity) {
            count = capacity;
            while (count && remaining[count] != ' ')
                count--;
            if (!count)
                count = capacity;
        }
        char line[96];
        memcpy(line, remaining, count);
        line[count] = 0;
        text(12, BOARD_HEIGHT - STATUS_HEIGHT + 3 + (int)row * 18, line, 0xb7cce0);
        remaining += count;
        while (*remaining == ' ')
            remaining++;
    }
#else
    text(6, BOARD_HEIGHT - STATUS_HEIGHT, status, 0xaabdd1);
#endif
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
    return (BOARD_HEIGHT - TOP - BOTTOM - LIST_HEADER) / LIST_ROW;
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
    if (x >= BOARD_WIDTH || y >= BOARD_HEIGHT)
        return;
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
        if (y >= BOARD_HEIGHT - BOTTOM && y < BOARD_HEIGHT - STATUS_HEIGHT) {
            if (x < BOARD_WIDTH / 3) {
                if (page)
                    page--;
            } else if (x < BOARD_WIDTH * 2 / 3) {
                if ((page + 1) * rows() < file_count)
                    page++;
            } else
                scan();
        } else if (y >= TOP + LIST_HEADER && y < BOARD_HEIGHT - BOTTOM) {
            unsigned row = (y - TOP - LIST_HEADER) / LIST_ROW;
            if (row < rows())
                load(page * rows() + row);
        }
        return;
    }
    if (y >= BOARD_HEIGHT - BOTTOM && y < BOARD_HEIGHT - STATUS_HEIGHT) {
        unsigned cell = x * 5 / BOARD_WIDTH;
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
    button(0, 3, 0, TOP, "MetaMask Fox", 0xffbb64, !browser && current == &metamask, &TAB_FONT);
    button(1, 3, 0, TOP, "Suzanne", 0x91d1ff, !browser && current == &suzanne, &TAB_FONT);
    button(2, 3, 0, TOP, "SD Models", 0xb0f0bc, browser, &TAB_FONT);
    rect(0, BOARD_HEIGHT - BOTTOM, BOARD_WIDTH, BOTTOM, 0x14253b);
    char line[100];
    if (browser) {
        rect(0, TOP, BOARD_WIDTH, BOARD_HEIGHT - TOP - BOTTOM, 0x0b1423);
        for (unsigned i = 0; i < rows() && page * rows() + i < file_count; i++) {
            int y = TOP + LIST_HEADER + (int)i * LIST_ROW;
            rect(8, y + 2, BOARD_WIDTH - 16, LIST_ROW - 4, 0x1d334b);
            char label[64];
            snprintf(label, sizeof(label), "%s", names[page * rows() + i]);
            unsigned capacity = (BOARD_WIDTH - 40) / FILE_FONT.Width;
            if (strlen(label) > capacity && capacity > 3) {
                memcpy(label + capacity - 3, "...", 4);
            }
            text_font(20, y + (LIST_ROW - FILE_FONT.Height) / 2, label, 0xffffff, &FILE_FONT,
                      BOARD_WIDTH - 20);
        }
        snprintf(line, sizeof(line), "MICROSD /models | Page %u", page + 1);
        rect(0, TOP, BOARD_WIDTH, LIST_HEADER, 0x0b1423);
        text(12, TOP + (LIST_HEADER - BODY_FONT.Height) / 2, line, 0xffffff);
        button(0, 3, BOARD_HEIGHT - BOTTOM, CONTROLS, "Previous", 0xffffff, 0, &CONTROL_FONT);
        button(1, 3, BOARD_HEIGHT - BOTTOM, CONTROLS, "Next", 0xffffff, 0, &CONTROL_FONT);
        button(2, 3, BOARD_HEIGHT - BOTTOM, CONTROLS, "Refresh", 0xffffff, 0, &CONTROL_FONT);
    } else {
        button(0, 5, BOARD_HEIGHT - BOTTOM, CONTROLS, modes[mode], 0xffffff, 0, &CONTROL_FONT);
        button(1, 5, BOARD_HEIGHT - BOTTOM, CONTROLS, autorotate ? "Pause" : "Rotate", 0xffffff, 0,
               &CONTROL_FONT);
        button(2, 5, BOARD_HEIGHT - BOTTOM, CONTROLS, "Zoom -", 0xffffff, 0, &CONTROL_FONT);
        button(3, 5, BOARD_HEIGHT - BOTTOM, CONTROLS, "Zoom +", 0xffffff, 0, &CONTROL_FONT);
        button(4, 5, BOARD_HEIGHT - BOTTOM, CONTROLS,
               renderer.backface_culling ? "Cull ON" : "Cull OFF", 0xffffff, 0, &CONTROL_FONT);
        snprintf(line, sizeof(line), "%s | %lu tris | %u.%u FPS | render %u us", model_name,
                 (unsigned long)current->triangle_count, fps10 / 10, fps10 % 10, render_us);
        rect(0, TOP, BOARD_WIDTH, INFO_HEIGHT, 0x0b1423);
        text(6, TOP + (INFO_HEIGHT - BODY_FONT.Height) / 2, line, 0xc6d8eb);
    }
    status_text();
}
int main(void) {
    board_init();
    board_renderer_init(&renderer);
    if (dsp3d_validate_mesh(&metamask) != DSP3D_OK || dsp3d_validate_mesh(&suzanne) != DSP3D_OK)
        board_panic(15);
    uint32_t last = HAL_GetTick(), last_log = last, last_frame = last;
    unsigned fps10 = 0;
    TS_StateTypeDef touch = {0};
    unsigned previous_touch = 0, previous_button = 0, gesture_active = 0;
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
                if (!previous_touch) {
                    gesture_active =
                        !browser && y >= TOP + INFO_HEIGHT && y < (int)BOARD_HEIGHT - BOTTOM;
                    tap((unsigned)x, (unsigned)y);
                } else if (gesture_active && !browser && y >= TOP + INFO_HEIGHT &&
                           y < (int)BOARD_HEIGHT - BOTTOM && previous_y >= TOP + INFO_HEIGHT &&
                           previous_y < (int)BOARD_HEIGHT - BOTTOM) {
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
