/*-------------------------------------------------------------------------------------------------------------------------------------------
 * dcf77.h - declaration of dcf77 routines
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef DCF77_H
#define DCF77_H

#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"

#include <time.h>

extern uint_fast8_t dcf77_is_up;

extern void         dcf77_init (void);
extern void         dcf77_tick (void);
extern uint_fast8_t dcf77_time (struct tm *);

#endif
