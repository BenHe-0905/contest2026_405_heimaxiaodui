/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "fonts.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Waveshare 3.5" e-Paper (G) panel: 184 x 384, 2 bits per pixel. */
#define EPAPER_WIDTH          184
#define EPAPER_HEIGHT         384
#define EPAPER_BYTES_PER_ROW  (EPAPER_WIDTH / 4)   /* 46 bytes/row, 4 px/byte */
#define EPAPER_FB_SIZE        (EPAPER_BYTES_PER_ROW * EPAPER_HEIGHT)

/* Panel colors (2-bit values). */
#define EPAPER_COLOR_BLACK    0x0
#define EPAPER_COLOR_WHITE    0x1
#define EPAPER_COLOR_YELLOW   0x2
#define EPAPER_COLOR_RED      0x3

/**
 * Fill the whole frame buffer with a single 2-bit color.
 */
void epaper_fb_clear(uint8_t *fb, uint8_t color);

/**
 * Set a single pixel to a 2-bit color. Pixels are packed 4-per-byte, MSB
 * first (byte_idx = y*46 + x/4, shift = 6 - (x%4)*2). Out-of-range
 * coordinates are silently ignored.
 *
 * Exposed (non-static) so lua_module_epaper's draw_image path can write
 * decoded pixels into the frame buffer with the same packing as text.
 */
void epaper_fb_set_pixel(uint8_t *fb, int x, int y, uint8_t color);

/**
 * Draw a single ASCII character into the frame buffer at (x, y).
 * Only the foreground pixels are written; background is left untouched so
 * text can be layered (draw over an already-cleared white buffer).
 * Pixels falling outside the panel are silently clipped.
 */
void epaper_fb_draw_char(uint8_t *fb, int x, int y, char ch, const sFONT *font, uint8_t color);

/**
 * Draw an ASCII string. '\n' advances to the next line (back to x0 at
 * y + font->Height). No automatic wrapping; characters past the right edge
 * are clipped.
 */
void epaper_fb_draw_string(uint8_t *fb, int x, int y, const char *str,
                           const sFONT *font, uint8_t color);

#ifdef __cplusplus
}
#endif
