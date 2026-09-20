/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Export for non-Lua C/C++ tasks (e.g. the BOOT-key screen switcher) that need
 * to push a full pre-rendered frame to the e-paper panel. Internally this
 * serializes with the Lua-side access to the shared frame buffer via a mutex.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Copy a complete 2-bit/px (4-color) frame buffer into the epaper module's
 * internal buffer and refresh the panel (Init -> Display -> Sleep). The buffer
 * must be exactly the module's frame-buffer size and use the same byte packing
 * as epaper_text.h (4 px/byte, MSB-first, palette black0/white1/yellow2/red3).
 */
esp_err_t epaper_display_raw(const uint8_t *data, size_t len);

/*
 * Copy the LLM-generated weather-screen cache into the caller's buffer.
 * The cache is populated by the Lua API `epaper.save_weather_cache()` (which
 * snapshots the internal frame buffer without refreshing the panel) and read
 * here by the BOOT-key screen switcher so it can instantly show the cached
 * weather screen without waiting for the next LLM generation. `out_valid`
 * (optional) reports whether a cache has actually been saved; the buffer is
 * only meaningful when it is true.
 */
esp_err_t epaper_get_weather_cache(uint8_t *out, size_t out_size, bool *out_valid);

/*
 * Copy the LLM-generated schedule/todo-screen cache into the caller's buffer.
 * Same pattern as epaper_get_weather_cache(): populated by the Lua API
 * `epaper.save_schedule_cache()` (snapshots the internal frame buffer without
 * refreshing the panel), read here by the BOOT-key switcher for the 待办 screen
 * (index 2). `out_valid` (optional) reports whether a cache has been saved.
 */
esp_err_t epaper_get_schedule_cache(uint8_t *out, size_t out_size, bool *out_valid);

#ifdef __cplusplus
}
#endif
