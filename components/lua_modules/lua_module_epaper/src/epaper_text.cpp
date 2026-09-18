/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Self-contained ASCII text renderer for the 2-bit-per-pixel e-paper frame
 * buffer. Font tables use the STMicroelectronics layout (row-major, MSB =
 * leftmost pixel), identical to GUI_Paint.cpp in the vela-esp32 project.
 */
#include "epaper_text.h"

void epaper_fb_clear(uint8_t *fb, uint8_t color)
{
    uint8_t packed = (uint8_t)((color << 6) | (color << 4) | (color << 2) | color);
    for (int i = 0; i < EPAPER_FB_SIZE; i++) {
        fb[i] = packed;
    }
}

void epaper_fb_set_pixel(uint8_t *fb, int x, int y, uint8_t color)
{
    if (x < 0 || x >= EPAPER_WIDTH || y < 0 || y >= EPAPER_HEIGHT) {
        return;
    }
    int byte_idx = y * EPAPER_BYTES_PER_ROW + (x / 4);
    int shift = 6 - (x % 4) * 2;
    uint8_t mask = (uint8_t)(0x03 << shift);
    fb[byte_idx] = (uint8_t)((fb[byte_idx] & ~mask) | (color << shift));
}

void epaper_fb_draw_char(uint8_t *fb, int x, int y, char ch, const sFONT *font, uint8_t color)
{
    if (ch < ' ' || ch > '~') {
        return; /* only printable ASCII */
    }

    uint16_t bytes_per_row = (uint16_t)(font->Width / 8 + (font->Width % 8 ? 1 : 0));
    uint32_t char_offset = (uint32_t)(ch - ' ') * font->Height * bytes_per_row;
    const uint8_t *ptr = &font->table[char_offset];

    for (int page = 0; page < font->Height; page++) {
        for (int column = 0; column < font->Width; column++) {
            if (*ptr & (0x80 >> (column % 8))) {
                epaper_fb_set_pixel(fb, x + column, y + page, color);
            }
            if (column % 8 == 7) {
                ptr++;
            }
        }
        if (font->Width % 8 != 0) {
            ptr++;
        }
    }
}

void epaper_fb_draw_string(uint8_t *fb, int x, int y, const char *str,
                           const sFONT *font, uint8_t color)
{
    int x0 = x;
    int cx = x;
    int cy = y;

    for (const char *p = str; *p != '\0'; p++) {
        if (*p == '\n') {
            cx = x0;
            cy += font->Height;
            continue;
        }
        epaper_fb_draw_char(fb, cx, cy, *p, font, color);
        cx += font->Width;
    }
}
