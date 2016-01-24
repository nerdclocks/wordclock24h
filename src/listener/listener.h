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

typedef union
{
    uint_fast8_t    mode;
    uint_fast8_t    brightness;
    uint_fast8_t    automatic_brightness_control;
    DSP_COLORS      rgb;
    struct tm       tm;
    uint_fast8_t    power;
} LISTENER_DATA;

extern uint_fast8_t         listener (LISTENER_DATA *);
extern uint_fast8_t         listener_init (void);

#endif