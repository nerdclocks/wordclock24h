/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * ws2812-config.h - configuration of WS2812 driver
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */

#define WS2812_STATUS_LED_OFFSET    0                                                                       // offset in LED chain

#if WCLOCK24H == 1

#  define WS2812_STATUS_LEDS          1                                                                     //   1 status LED

#  define WS2812_DISPLAY_LED_OFFSET   (WS2812_STATUS_LED_OFFSET + WS2812_STATUS_LEDS)                       // offset of display LEDs
#  define WS2812_DISPLAY_LEDS         288                                                                   // 288 display LEDs

#  define WS2812_AMBILIGHT_LED_OFFSET (WS2812_DISPLAY_LED_OFFSET + WS2812_DISPLAY_LEDS)                     // offset of ambilight LEDs

#ifdef STM32F103
#  define WS2812_AMBILIGHT_LEDS       0                                                                     // no ambilight LEDs
#else
#  define WS2812_AMBILIGHT_LEDS       100                                                                   // max. 100 ambilight LEDs
#endif

#  define WS2812_MAX_LEDS             (WS2812_STATUS_LEDS + WS2812_DISPLAY_LEDS + WS2812_AMBILIGHT_LEDS)    // maximum needed LEDs

#else // WC12H

#  define WS2812_STATUS_LEDS          1                                                                     //   1 status LED

#  define WS2812_MINUTE_LED_OFFSET    (WS2812_STATUS_LED_OFFSET + WS2812_STATUS_LEDS)                       // offset of minute LEDs
#  define WS2812_MINUTE_LEDS          4                                                                     //   4 minute LEDs

#  define WS2812_DISPLAY_LED_OFFSET   (WS2812_STATUS_LED_OFFSET + WS2812_MINUTE_LEDS)                       // offset of display LEDs
#  define WS2812_DISPLAY_LEDS         110                                                                   // 110 display LEDs

#  define WS2812_AMBILIGHT_LED_OFFSET (WS2812_DISPLAY_LED_OFFSET + WS2812_DISPLAY_LEDS)                     // offset of ambilight LEDs
#  define WS2812_AMBILIGHT_LEDS       100                                                                   // max. 100 ambilight LEDs

#  define WS2812_MAX_LEDS             (WS2812_STATUS_LEDS + WS2812_MINUTE_LEDS + WS2812_DISPLAY_LEDS + WS2812_AMBILIGHT_LEDS)
                                                                                                            // maximum needed LEDs

#endif
