/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "lua.h"

int luaopen_epaper(lua_State *L);
esp_err_t lua_module_epaper_register(void);

#ifdef __cplusplus
}
#endif
