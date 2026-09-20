/**
  * @file    fonts.h
  * @brief   Header for the ASCII bitmap fonts used by the e-paper text renderer.
  *
  * Trimmed from the STMicroelectronics fonts.h: only the sFONT ASCII fonts
  * (Font8 / Font16 / Font24) are kept. The GB2312 (cFONT) Chinese fonts are
  * omitted to keep the e-paper component self-contained.
  */

#ifndef __EPAPER_FONTS_H
#define __EPAPER_FONTS_H

#include <stdint.h>

#ifdef __cplusplus
 extern "C" {
#endif

/* ASCII font: row-major bitmap, MSB = leftmost pixel. */
typedef struct _tFont
{
  const uint8_t *table;
  uint16_t Width;
  uint16_t Height;
} sFONT;

extern sFONT Font24;
extern sFONT Font16;
extern sFONT Font8;

#ifdef __cplusplus
}
#endif

#endif /* __EPAPER_FONTS_H */
