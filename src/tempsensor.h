/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * tempsensor.h - declaration of temperature sensor routines
 *
 * Copyright (c) 2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */

#ifndef TEMPSENSOR_H
#define TEMPSENSOR_H

#include "stm32f4xx.h"

extern uint_fast8_t             temp_read_temp_index (void);
extern uint_fast8_t             temp_init (void);

#endif
