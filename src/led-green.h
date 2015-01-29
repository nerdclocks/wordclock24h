/*-------------------------------------------------------------------------------------------------------------------------------------------
 * green-led.h - declarations of LED routines
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef LED_GREEN_H
#define LED_GREEN_H

#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"

extern void         led_green_init (void);
extern void         led_green_on (void);
extern void         led_green_off (void);

#endif
