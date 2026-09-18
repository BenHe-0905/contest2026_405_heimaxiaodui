/*****************************************************************************
* | File      	:   DEV_Config.h
* | Author      :   Waveshare team (ESP-IDF port)
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V2.0
* | Date        :   2026-08-26
* | Info        :   Converted from Arduino to ESP-IDF
******************************************************************************/
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include <stdint.h>
#include <stdio.h>

/**
 * data types
**/
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

/**
 * GPIO config - ESP32-P4 wiring for 3.5" e-Paper (G), Waveshare 4-color panel.
 *
 * NOTE: On ESP32-P4 the SPI flash (MSPI0) and PSRAM (MSPI1) live on dedicated
 * internal buses, NOT on the GPIO matrix. The DBG_FLASH_* / DBG_PSRAM_* pads
 * (GPIO22~54) are only function-4 debug probes and remain usable as ordinary
 * GPIO. So the pins below (SCK=21 MOSI=22 CS=33 DC=32 RST=5 BUSY=20) are safe.
 *
 * These are the pins vela-esp32 already validated on this same EVB.
**/
#define EPD_SCK_PIN  21
#define EPD_MOSI_PIN 22
#define EPD_CS_PIN   33
#define EPD_DC_PIN   32
#define EPD_RST_PIN  5
#define EPD_BUSY_PIN 20

/**
 * PWR pin: directly connected to 3V3 (always on), no GPIO control needed
 */
#define D_9PIN  0

/**
 * GPIO read and write
**/
#include "driver/gpio.h"
#define DEV_Digital_Write(_pin, _value) gpio_set_level((gpio_num_t)(_pin), (_value))
#define DEV_Digital_Read(_pin)          gpio_get_level((gpio_num_t)(_pin))

/**
 * delay x ms
**/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
/* tick=100Hz 时 pdMS_TO_TICKS(<10ms) 会算出 0，vTaskDelay(0) 不阻塞，
 * 导致 ReadBusyH 的 while(!BUSY) 忙循环饿死 idle 任务触发 TWDT。
 * 保证至少阻塞 1 tick（10ms），让 delay 真正让出 CPU。 */
#define DEV_Delay_ms(__xms) do { \
    TickType_t _t = pdMS_TO_TICKS(__xms); \
    if (_t == 0) _t = 1; \
    vTaskDelay(_t); \
} while (0)

/*------------------------------------------------------------------------------------------------------*/
UBYTE DEV_Module_Init(void);
void DEV_SPI_WriteByte(UBYTE data);
UBYTE DEV_SPI_ReadByte(void);
void DEV_SPI_Write_nByte(UBYTE *pData, UDOUBLE len);
void DEV_Module_Exit(void);

#endif
