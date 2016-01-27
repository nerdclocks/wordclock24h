/*-------------------------------------------------------------------------------------------------------------------------------------------
 * ldr.h - adc functions
 *
 * Copyright (c) 2015-2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef LDR_H
#define LDR_H

#if defined (STM32F10X)
#  include "stm32f10x.h"
#  include "stm32f10x_gpio.h"
#  include "stm32f10x_rcc.h"
#  include "stm32f10x_adc.h"
#elif defined (STM32F4XX)
#  include "stm32f4xx.h"
#  include "stm32f4xx_gpio.h"
#  include "stm32f4xx_rcc.h"
#  include "stm32f4xx_adc.h"
#endif

extern uint_fast8_t     ldr_is_up;

extern void             ldr_start_conversion (void);
extern uint_fast8_t     ldr_poll_brightness (uint_fast8_t *);
extern uint_fast8_t     ldr_get_ldr_status (void);
extern void             ldr_set_ldr_status (uint_fast8_t);
extern void             ldr_init (void);

#endif // LDR_H
