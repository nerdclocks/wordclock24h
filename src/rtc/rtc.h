/*-------------------------------------------------------------------------------------------------------------------------------------------
 * rtc.h - delarations of DS3231 RTC routines
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef DS3231_H
#define DS3231_H

#include <time.h>
#if defined (STM32F10X)
#  include "stm32f10x.h"
#elif defined (STM32F4XX)
#  include "stm32f4xx.h"
#endif

extern uint_fast8_t rtc_is_up;

extern uint_fast8_t rtc_init (void);
extern uint_fast8_t rtc_set_date_time (struct tm *);
extern uint_fast8_t rtc_get_date_time (struct tm *);
extern uint_fast8_t rtc_get_temperature_index (void);
#endif
