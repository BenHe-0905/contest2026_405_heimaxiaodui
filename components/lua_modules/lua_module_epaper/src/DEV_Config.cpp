/*****************************************************************************
* | File      	:   DEV_Config.cpp
* | Author      :   Waveshare team (ESP-IDF port)
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V2.0
* | Date        :   2026-08-26
* | Info        :   Converted from Arduino to ESP-IDF
******************************************************************************/
#include "DEV_Config.h"
#include <string.h>

/******************************************************************************
function:	Configure GPIO
parameter:
Info:
******************************************************************************/
static void GPIO_Config(void)
{
    // Output pins
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;

    // Configure output pins
    uint64_t output_pins = (1ULL << EPD_CS_PIN) |
                           (1ULL << EPD_SCK_PIN) |
                           (1ULL << EPD_MOSI_PIN) |
                           (1ULL << EPD_DC_PIN) |
                           (1ULL << EPD_RST_PIN);
#if D_9PIN
    output_pins |= (1ULL << EPD_PWR_PIN);
#endif
    io_conf.pin_bit_mask = output_pins;
    gpio_config(&io_conf);

    // Configure input pin (BUSY)
    gpio_config_t in_conf = {};
    in_conf.intr_type = GPIO_INTR_DISABLE;
    in_conf.mode = GPIO_MODE_INPUT;
    in_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    in_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    in_conf.pin_bit_mask = (1ULL << EPD_BUSY_PIN);
    gpio_config(&in_conf);

    // Set default levels
    DEV_Digital_Write(EPD_CS_PIN, 1);
    DEV_Digital_Write(EPD_SCK_PIN, 0);
#if D_9PIN
    DEV_Digital_Write(EPD_PWR_PIN, 1);
#endif
}

/******************************************************************************
function:	Module Initialize
parameter:
Info:
******************************************************************************/
UBYTE DEV_Module_Init(void)
{
    GPIO_Config();
    return 0;
}

/******************************************************************************
function:	SPI write byte (bit-bang)
parameter:
Info:
******************************************************************************/
void DEV_SPI_WriteByte(UBYTE data)
{
    DEV_Digital_Write(EPD_CS_PIN, 0);

    for (int i = 0; i < 8; i++)
    {
        if ((data & 0x80) == 0)
            DEV_Digital_Write(EPD_MOSI_PIN, 0);
        else
            DEV_Digital_Write(EPD_MOSI_PIN, 1);

        data <<= 1;
        DEV_Digital_Write(EPD_SCK_PIN, 1);
        DEV_Digital_Write(EPD_SCK_PIN, 0);
    }

    DEV_Digital_Write(EPD_CS_PIN, 1);
}

/******************************************************************************
function:	SPI read byte (bit-bang)
parameter:
Info:
******************************************************************************/
UBYTE DEV_SPI_ReadByte(void)
{
    UBYTE j = 0xff;

    // Switch MOSI to input
    gpio_set_direction((gpio_num_t)EPD_MOSI_PIN, GPIO_MODE_INPUT);

    DEV_Digital_Write(EPD_CS_PIN, 0);
    for (int i = 0; i < 8; i++)
    {
        j = j << 1;
        if (DEV_Digital_Read(EPD_MOSI_PIN))
            j = j | 0x01;
        else
            j = j & 0xfe;

        DEV_Digital_Write(EPD_SCK_PIN, 1);
        DEV_Digital_Write(EPD_SCK_PIN, 0);
    }
    DEV_Digital_Write(EPD_CS_PIN, 1);

    // Switch MOSI back to output
    gpio_set_direction((gpio_num_t)EPD_MOSI_PIN, GPIO_MODE_OUTPUT);

    return j;
}

/******************************************************************************
function:	SPI write n bytes
parameter:
Info:
******************************************************************************/
void DEV_SPI_Write_nByte(UBYTE *pData, UDOUBLE len)
{
    for (UDOUBLE i = 0; i < len; i++)
        DEV_SPI_WriteByte(pData[i]);
}

/******************************************************************************
function:	Module Exit
parameter:
Info:
******************************************************************************/
void DEV_Module_Exit(void)
{
#if D_9PIN
    DEV_Digital_Write(EPD_PWR_PIN, 0);
#endif
    DEV_Digital_Write(EPD_RST_PIN, 0);
}
