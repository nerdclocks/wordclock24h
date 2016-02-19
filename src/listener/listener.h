/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * listener.h - declarations of ESP8266 routines
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef LISTENER_H
#define LISTENER_H

#include <string.h>
#include <stdint.h>
#include <time.h>
#include "display.h"

#define LISTENER_SET_COLOR_CODE                 'C'                             // set color
#define LISTENER_POWER_CODE                     'P'                             // power on/off
#define LISTENER_DISPLAY_MODE_CODE              'D'                             // set display mode
#define LISTENER_ANIMATION_MODE_CODE            'A'                             // set animation mode
#define LISTENER_DISPLAY_TEMPERATURE_CODE       'W'                             // display temperature
#define LISTENER_SET_BRIGHTNESS_CODE            'B'                             // set brightness
#define LISTENER_SET_AUTOMATIC_BRIHGHTNESS_CODE 'L'                             // automatic brightness control on/off
#define LISTENER_TEST_DISPLAY_CODE              'X'                             // test display
#define LISTENER_SET_DATE_TIME_CODE             'T'                             // set date/time
#define LISTENER_GET_NET_TIME_CODE              'N'                             // Get net time
#define LISTENER_IR_LEARN_CODE                  'I'                             // IR learn
#define LISTENER_SET_NIGHT_TIME                 'J'                             // set night off time
#define LISTENER_SAVE_DISPLAY_CONFIGURATION     'S'                             // save display configuration

typedef union
{
    uint_fast8_t        mode;
    uint_fast8_t        brightness;
    uint_fast8_t        automatic_brightness_control;
    DSP_COLORS          rgb;
    struct tm           tm;
    uint_fast8_t        power;
} LISTENER_DATA;

extern uint_fast8_t         listener (LISTENER_DATA *);

#endif
