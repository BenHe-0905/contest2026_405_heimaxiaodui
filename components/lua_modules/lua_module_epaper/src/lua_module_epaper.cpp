/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Lua binding for the Waveshare 3.5" 4-color e-paper display (bit-bang SPI).
 * The panel is slow to refresh (~seconds per full update, tens of seconds for
 * a 4-color clear), so it is intended for static information the AI wants to
 * leave on screen.
 */
#include "lua_module_epaper.h"

#include <stdint.h>
#include <string.h>

#include "cap_lua.h"

extern "C" {
#include "lauxlib.h"
}

#include "DEV_Config.h"
#include "EPD_3in5g_V2.h"
#include "epaper_text.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lua_image.h"

#include <limits.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "epaper_display.h"

static const char *TAG = "lua_module_epaper";

/* Internal frame buffer + init guard. */
static uint8_t s_fb[EPAPER_FB_SIZE];
static bool s_initialized = false;

/*
 * Weather-screen cache. The LLM generates the weather screen in the background
 * (via show_weather.lua → epaper.save_weather_cache()) without disturbing the
 * screen the user is currently viewing; the BOOT-key switcher later reads this
 * cache through epaper_get_weather_cache() and refreshes the panel instantly.
 */
static uint8_t s_weather_cache[EPAPER_FB_SIZE];
static bool s_weather_cache_valid = false;

/*
 * Schedule/todo-screen cache. Mirror of the weather cache above: show_schedule.lua
 * renders today's schedules in the background and stores the frame here via
 * epaper.save_schedule_cache(); the BOOT-key switcher reads it through
 * epaper_get_schedule_cache() to refresh the 待办 screen (index 2) instantly.
 */
static uint8_t s_schedule_cache[EPAPER_FB_SIZE];
static bool s_schedule_cache_valid = false;

/*
 * EPD access mutex. The panel + s_fb are shared between the Lua API below and
 * the BOOT-key screen switcher (which goes through epaper_display_raw). A full
 * 4-color refresh takes seconds, so a plain FreeRTOS mutex (not a spinlock) is
 * required to avoid watchdog timeouts. Lazily created on first use.
 */
static SemaphoreHandle_t s_epaper_mutex = NULL;

static SemaphoreHandle_t epaper_ensure_mutex(void)
{
    if (s_epaper_mutex == NULL) {
        s_epaper_mutex = xSemaphoreCreateMutex();
    }
    return s_epaper_mutex;
}

esp_err_t epaper_display_raw(const uint8_t *data, size_t len)
{
    if (data == NULL || len != EPAPER_FB_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    SemaphoreHandle_t m = epaper_ensure_mutex();
    if (xSemaphoreTake(m, pdMS_TO_TICKS(10000)) != pdTRUE) {
        ESP_LOGW(TAG, "epaper_display_raw: mutex timeout, skip");
        return ESP_ERR_TIMEOUT;
    }

    memcpy(s_fb, data, EPAPER_FB_SIZE);
    DEV_Module_Init();          /* idempotent: configures EPD GPIO if not yet done */
    EPD_3IN5G_V2_Init();        /* wake from Sleep */
    EPD_3IN5G_V2_Display(s_fb);
    EPD_3IN5G_V2_Sleep();
    xSemaphoreGive(m);
    return ESP_OK;
}

esp_err_t epaper_get_weather_cache(uint8_t *out, size_t out_size, bool *out_valid)
{
    if (out_valid) {
        *out_valid = false;
    }
    if (out == NULL || out_size < EPAPER_FB_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    SemaphoreHandle_t m = epaper_ensure_mutex();
    if (xSemaphoreTake(m, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(out, s_weather_cache, EPAPER_FB_SIZE);
    if (out_valid) {
        *out_valid = s_weather_cache_valid;
    }
    xSemaphoreGive(m);
    return ESP_OK;
}

esp_err_t epaper_get_schedule_cache(uint8_t *out, size_t out_size, bool *out_valid)
{
    if (out_valid) {
        *out_valid = false;
    }
    if (out == NULL || out_size < EPAPER_FB_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    SemaphoreHandle_t m = epaper_ensure_mutex();
    if (xSemaphoreTake(m, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(out, s_schedule_cache, EPAPER_FB_SIZE);
    if (out_valid) {
        *out_valid = s_schedule_cache_valid;
    }
    xSemaphoreGive(m);
    return ESP_OK;
}

static sFONT *epaper_font_for_size(int size)
{
    if (size <= 8) {
        return &Font8;
    } else if (size <= 16) {
        return &Font16;
    }
    return &Font24;
}

static int lua_epaper_init(lua_State *L)
{
    (void)L;
    SemaphoreHandle_t m = epaper_ensure_mutex();
    if (xSemaphoreTake(m, pdMS_TO_TICKS(5000)) == pdTRUE) {
        if (!s_initialized) {
            DEV_Module_Init();
            EPD_3IN5G_V2_Init();
            epaper_fb_clear(s_fb, EPAPER_COLOR_WHITE);
            s_initialized = true;
        }
        xSemaphoreGive(m);
    }
    return 0;
}

static int lua_epaper_clear(lua_State *L)
{
    int color = (int)luaL_optinteger(L, 1, EPAPER_COLOR_WHITE) & 0x03;
    /* Fill the frame buffer only; call epaper.display() to flush to the panel. */
    SemaphoreHandle_t m = epaper_ensure_mutex();
    if (xSemaphoreTake(m, pdMS_TO_TICKS(5000)) == pdTRUE) {
        epaper_fb_clear(s_fb, (uint8_t)color);
        xSemaphoreGive(m);
    }
    return 0;
}

/* Lua: epaper.fill_rect(x, y, w, h, [color=BLACK]) — fill an axis-aligned
 * rectangle in the frame buffer (no panel refresh; call display() to flush).
 * Reuses epaper_fb_set_pixel which packs 4px/byte and clips OOB silently. */
static int lua_epaper_fill_rect(lua_State *L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    int color = (int)luaL_optinteger(L, 5, EPAPER_COLOR_BLACK) & 0x03;
    luaL_argcheck(L, w >= 0 && h >= 0, 3, "w/h must be >= 0");

    SemaphoreHandle_t m = epaper_ensure_mutex();
    if (xSemaphoreTake(m, pdMS_TO_TICKS(5000)) == pdTRUE) {
        for (int yy = y; yy < y + h; yy++) {
            for (int xx = x; xx < x + w; xx++) {
                epaper_fb_set_pixel(s_fb, xx, yy, (uint8_t)color);
            }
        }
        xSemaphoreGive(m);
    }
    return 0;
}

static int lua_epaper_text(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    int size = (int)luaL_optinteger(L, 4, 24);
    int color = (int)luaL_optinteger(L, 5, EPAPER_COLOR_BLACK) & 0x03;

    SemaphoreHandle_t m = epaper_ensure_mutex();
    if (xSemaphoreTake(m, pdMS_TO_TICKS(5000)) == pdTRUE) {
        epaper_fb_draw_string(s_fb, x, y, str, epaper_font_for_size(size), (uint8_t)color);
        xSemaphoreGive(m);
    }
    return 0;
}

static int lua_epaper_display(lua_State *L)
{
    (void)L;
    SemaphoreHandle_t m = epaper_ensure_mutex();
    if (xSemaphoreTake(m, pdMS_TO_TICKS(5000)) == pdTRUE) {
        EPD_3IN5G_V2_Display(s_fb);
        xSemaphoreGive(m);
    }
    return 0;
}

/*
 * Lua: epaper.save_weather_cache() — snapshot the current frame buffer into the
 * weather cache WITHOUT refreshing the panel. The LLM draws the weather screen
 * in the background and stores it here; the BOOT-key switcher displays it later.
 */
static int lua_epaper_save_weather_cache(lua_State *L)
{
    (void)L;
    SemaphoreHandle_t m = epaper_ensure_mutex();
    if (xSemaphoreTake(m, pdMS_TO_TICKS(5000)) == pdTRUE) {
        memcpy(s_weather_cache, s_fb, EPAPER_FB_SIZE);
        s_weather_cache_valid = true;
        xSemaphoreGive(m);
    }
    return 0;
}

/*
 * Lua: epaper.save_schedule_cache() — snapshot the current frame buffer into the
 * schedule cache WITHOUT refreshing the panel. show_schedule.lua draws today's
 * schedules in the background and stores it here; the BOOT-key switcher displays
 * it later on the 待办 screen.
 */
static int lua_epaper_save_schedule_cache(lua_State *L)
{
    (void)L;
    SemaphoreHandle_t m = epaper_ensure_mutex();
    if (xSemaphoreTake(m, pdMS_TO_TICKS(5000)) == pdTRUE) {
        memcpy(s_schedule_cache, s_fb, EPAPER_FB_SIZE);
        s_schedule_cache_valid = true;
        xSemaphoreGive(m);
    }
    return 0;
}

static int lua_epaper_sleep(lua_State *L)
{
    (void)L;
    SemaphoreHandle_t m = epaper_ensure_mutex();
    if (xSemaphoreTake(m, pdMS_TO_TICKS(5000)) == pdTRUE) {
        EPD_3IN5G_V2_Sleep();
        s_initialized = false;
        xSemaphoreGive(m);
    }
    return 0;
}

static int lua_epaper_width(lua_State *L)
{
    lua_pushinteger(L, EPAPER_WIDTH);
    return 1;
}

static int lua_epaper_height(lua_State *L)
{
    lua_pushinteger(L, EPAPER_HEIGHT);
    return 1;
}

/* ---------------------------------------------------------------------------
 * Image drawing: 4-color quantization + optional Floyd-Steinberg dithering.
 * Direct C port of vela-esp32/tools/img2epd.py:convert_image() — same palette,
 * same Euclidean nearest-color, same error-diffusion coefficients
 * (7/16 · 3/16 · 5/16 · 1/16), same MSB-first packing via epaper_fb_set_pixel.
 * ------------------------------------------------------------------------- */

static uint8_t epaper_nearest_color(int r, int g, int b)
{
    static const int pal[4][3] = {
        {   0,   0,   0 },  /* EPAPER_COLOR_BLACK  0 */
        { 255, 255, 255 },  /* EPAPER_COLOR_WHITE  1 */
        { 255, 255,   0 },  /* EPAPER_COLOR_YELLOW 2 */
        { 255,   0,   0 },  /* EPAPER_COLOR_RED    3 */
    };
    int best = 0;
    int best_diff = INT_MAX;
    for (int i = 0; i < 4; i++) {
        int dr = r - pal[i][0];
        int dg = g - pal[i][1];
        int db = b - pal[i][2];
        int diff = dr * dr + dg * dg + db * db;
        if (diff < best_diff) {
            best_diff = diff;
            best = i;
        }
    }
    return (uint8_t)best;
}

/* Python `a // 16` floor division (divisor positive) — img2epd.py uses this
 * exact semantics for error diffusion, so negative errors round toward -inf. */
static int32_t epaper_fs_div16(int32_t a)
{
    int32_t q = a / 16;
    if (a % 16 != 0 && a < 0) {
        q -= 1;
    }
    return q;
}

static void epaper_rgb565_to_rgb888(uint16_t px, int *r, int *g, int *b)
{
    /* Mirrors lua_image_rgb565_to_rgb888() in lua_module_image. */
    *r = (int)(((px >> 8) & 0xf8) | ((px >> 13) & 0x07));
    *g = (int)(((px >> 3) & 0xfc) | ((px >> 9) & 0x03));
    *b = (int)(((px << 3) & 0xf8) | ((px >> 2) & 0x07));
}

static void epaper_draw_rgb565_no_dither(const lua_image_view_t *view, int w, int h)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t o = ((size_t)y * view->width + x) * 2;
            uint16_t px = (uint16_t)view->data[o] | ((uint16_t)view->data[o + 1] << 8);
            int r, g, b;
            epaper_rgb565_to_rgb888(px, &r, &g, &b);
            epaper_fb_set_pixel(s_fb, x, y, epaper_nearest_color(r, g, b));
        }
    }
}

static void epaper_draw_rgb565_dither(const lua_image_view_t *view, int w, int h)
{
    size_t npx = (size_t)w * h;
    int32_t *work = (int32_t *)heap_caps_malloc(npx * 3 * sizeof(int32_t), MALLOC_CAP_SPIRAM);
    if (work == NULL) {
        ESP_LOGW(TAG, "draw_image: dither buffer alloc failed (%u bytes); using no-dither",
                 (unsigned)(npx * 3 * sizeof(int32_t)));
        epaper_draw_rgb565_no_dither(view, w, h);
        return;
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t o = ((size_t)y * view->width + x) * 2;
            uint16_t px = (uint16_t)view->data[o] | ((uint16_t)view->data[o + 1] << 8);
            int r, g, b;
            epaper_rgb565_to_rgb888(px, &r, &g, &b);
            int32_t *p = &work[((size_t)y * w + x) * 3];
            p[0] = r;
            p[1] = g;
            p[2] = b;
        }
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int32_t *p = &work[((size_t)y * w + x) * 3];
            int r = p[0];
            int g = p[1];
            int b = p[2];
            uint8_t idx = epaper_nearest_color(r, g, b);
            epaper_fb_set_pixel(s_fb, x, y, idx);

            int pr, pg, pb;
            switch (idx) {
            case 0:  pr = 0;   pg = 0;   pb = 0;   break;
            case 1:  pr = 255; pg = 255; pb = 255; break;
            case 2:  pr = 255; pg = 255; pb = 0;   break;
            default: pr = 255; pg = 0;   pb = 0;   break;
            }
            int er = r - pr;
            int eg = g - pg;
            int eb = b - pb;

            if (x + 1 < w) {
                int32_t *q = &work[((size_t)y * w + (x + 1)) * 3];
                q[0] += epaper_fs_div16(er * 7);
                q[1] += epaper_fs_div16(eg * 7);
                q[2] += epaper_fs_div16(eb * 7);
            }
            if (y + 1 < h && x - 1 >= 0) {
                int32_t *q = &work[((size_t)(y + 1) * w + (x - 1)) * 3];
                q[0] += epaper_fs_div16(er * 3);
                q[1] += epaper_fs_div16(eg * 3);
                q[2] += epaper_fs_div16(eb * 3);
            }
            if (y + 1 < h) {
                int32_t *q = &work[((size_t)(y + 1) * w + x) * 3];
                q[0] += epaper_fs_div16(er * 5);
                q[1] += epaper_fs_div16(eg * 5);
                q[2] += epaper_fs_div16(eb * 5);
            }
            if (y + 1 < h && x + 1 < w) {
                int32_t *q = &work[((size_t)(y + 1) * w + (x + 1)) * 3];
                q[0] += epaper_fs_div16(er * 1);
                q[1] += epaper_fs_div16(eg * 1);
                q[2] += epaper_fs_div16(eb * 1);
            }
        }
    }

    heap_caps_free(work);
}

static int lua_epaper_draw_image(lua_State *L)
{
    bool dither = lua_toboolean(L, 2) != 0;

    lua_image_view_t view;
    esp_err_t err = lua_image_require_format(L, 1, LUA_IMAGE_FORMAT_RGB565LE, &view);
    if (err != ESP_OK) {
        return luaL_error(L, "draw_image: argument #1 must be an image.frame (decode failed, err=0x%x)",
                          (unsigned)err);
    }

    int w = view.width  < EPAPER_WIDTH  ? view.width  : EPAPER_WIDTH;
    int h = view.height < EPAPER_HEIGHT ? view.height : EPAPER_HEIGHT;

    SemaphoreHandle_t m = epaper_ensure_mutex();
    if (xSemaphoreTake(m, pdMS_TO_TICKS(5000)) == pdTRUE) {
        /* White first so any uncovered region (frame smaller than panel) is white. */
        epaper_fb_clear(s_fb, EPAPER_COLOR_WHITE);

        if (dither) {
            epaper_draw_rgb565_dither(&view, w, h);
        } else {
            epaper_draw_rgb565_no_dither(&view, w, h);
        }
        xSemaphoreGive(m);
    }

    lua_image_release_view(&view);
    return 0; /* caller then calls epaper.display() */
}

int luaopen_epaper(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, lua_epaper_init);
    lua_setfield(L, -2, "init");
    lua_pushcfunction(L, lua_epaper_clear);
    lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, lua_epaper_fill_rect);
    lua_setfield(L, -2, "fill_rect");
    lua_pushcfunction(L, lua_epaper_text);
    lua_setfield(L, -2, "text");
    lua_pushcfunction(L, lua_epaper_display);
    lua_setfield(L, -2, "display");
    lua_pushcfunction(L, lua_epaper_save_weather_cache);
    lua_setfield(L, -2, "save_weather_cache");
    lua_pushcfunction(L, lua_epaper_save_schedule_cache);
    lua_setfield(L, -2, "save_schedule_cache");
    lua_pushcfunction(L, lua_epaper_sleep);
    lua_setfield(L, -2, "sleep");
    lua_pushcfunction(L, lua_epaper_width);
    lua_setfield(L, -2, "width");
    lua_pushcfunction(L, lua_epaper_height);
    lua_setfield(L, -2, "height");
    lua_pushcfunction(L, lua_epaper_draw_image);
    lua_setfield(L, -2, "draw_image");
    return 1;
}

esp_err_t lua_module_epaper_register(void)
{
    return cap_lua_register_module("epaper", luaopen_epaper);
}
