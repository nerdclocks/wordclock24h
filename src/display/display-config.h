/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * display-config.h - configuration of display driver
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */

#if WCLOCK24H == 1                                                                              // WC24H:
#  define DSP_USE_APA102            0                                                           // change here: 1: APA102, 0: WS2812
#else                                                                                           // WC12H:
#  define DSP_USE_APA102            0                                                           // change here: 1: APA102, 0: WS2812
#endif

#if DSP_USE_APA102 == 1
#  define DSP_USE_WS2812            0                                                           // don't change
#else
#  define DSP_USE_WS2812            1                                                           // don't change
#endif

#if WCLOCK24H == 1
#  define DSP_STATUS_LEDS           1                                                           // 1 status LED
#  define DSP_MINUTE_LEDS           0                                                           // 0 minute LEDs
#  define DSP_DISPLAY_LEDS          288                                                         // 288 display LEDs
#  define DSP_AMBILIGHT_LEDS        100                                                         // max. 100 ambilight LEDs
#else // WC12H
#  define DSP_STATUS_LEDS           0                                                           // no status LED
#  define DSP_MINUTE_LEDS           4                                                           // 4 minute LEDs
#  define DSP_DISPLAY_LEDS          110                                                         // 110 display LEDs
#  define DSP_AMBILIGHT_LEDS        100                                                         // max. 100 ambilight LEDs
#endif

#define DSP_STATUS_LED_OFFSET       0                                                           // offset in LED chain
#define DSP_MINUTE_LED_OFFSET       (DSP_STATUS_LED_OFFSET + DSP_STATUS_LEDS)                   // offset of minute LEDs
#define DSP_DISPLAY_LED_OFFSET      (DSP_MINUTE_LED_OFFSET + DSP_MINUTE_LEDS)                   // offset of display LEDs
#define DSP_AMBILIGHT_LED_OFFSET    (DSP_DISPLAY_LED_OFFSET + DSP_DISPLAY_LEDS)                 // offset of ambilight LEDs
#define DSP_MAX_LEDS                (DSP_STATUS_LEDS + DSP_MINUTE_LEDS + DSP_DISPLAY_LEDS + DSP_AMBILIGHT_LEDS) // maximum number of LEDs
