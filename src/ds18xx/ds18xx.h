/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * ds18xx.c - DS18xx routines
 *
 * Copyright (c) 2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */

#ifndef DS18XX_H
#define DS18XX_H

#include "stm32f4xx.h"
#include "onewire.h"

#define DS_RESOLUTION_9_BIT     9
#define DS_RESOLUTION_10_BIT    10
#define DS_RESOLUTION_11_BIT    11
#define DS_RESOLUTION_12_BIT    12

extern uint_fast8_t             ds18xx_is_up;

extern uint_fast8_t             ds18xx_read_raw_temp (uint_fast8_t *, uint_fast8_t *, uint_fast16_t *);
extern uint_fast8_t             ds18xx_read_temp (float *);
extern uint_fast8_t             ds18xx_init (uint_fast8_t);

#endif
