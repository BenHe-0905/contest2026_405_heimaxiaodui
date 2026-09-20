/*****************************************************************************
* | File      	:   Debug.h
* | Author      :   Waveshare team (ESP-IDF port)
* | Function    :   debug with printf
* | Info        :   Converted from Arduino Serial to ESP-IDF
******************************************************************************/
#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdio.h>

#define USE_DEBUG 1
#if USE_DEBUG
    #define Debug(__info) printf("%s", __info)
#else
    #define Debug(__info)
#endif

#endif
