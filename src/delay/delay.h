/*-------------------------------------------------------------------------------------------------------------------------------------------
 * delay.h - declaration of delay functions
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#if defined (STM32F10X)
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#elif defined (STM32F4XX)
#include "stm32f4xx.h"
#include "stm32f4xx_rcc.h"
#endif

#define DELAY_RESOLUTION_1_US           1
#define DELAY_RESOLUTION_10_US          2
#define DELAY_RESOLUTION_100_US         3

extern volatile uint32_t                delay_counter;

extern void delay_usec (uint32_t);
extern void delay_msec (uint32_t);
extern void delay_sec  (uint32_t);
extern void delay_init (uint_fast8_t);
