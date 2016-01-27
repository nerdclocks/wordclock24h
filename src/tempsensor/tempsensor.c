/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * tempsensor.c - temperature sensor routines
 *
 * Copyright (c) 2015-2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */

#include "ds18xx.h"
#include "tempsensor.h"
#include "eeprom.h"
#include "eeprom-data.h"

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * temp_read_temp_index () - read temperature
 *
 *    temperature_index =   0 ->   0°C
 *    temperature_index = 250 -> 125°C
 *    temperature_index = 255 -> Error
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
temp_read_temp_index (void)
{
    uint_fast8_t    resolution;
    uint_fast8_t    is_negative;
    uint_fast16_t   raw_temp;
    uint_fast8_t    temperature_index = 0xFF;

    if (ds18xx_read_raw_temp (&resolution, &is_negative, &raw_temp))
    {
        if (! is_negative)
        {
            temperature_index = raw_temp;
        }
    }
    return temperature_index;
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * temp_init () - initialize temperature sensor routines
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
temp_init (void)
{
    uint_fast8_t    rtc;

    rtc = ds18xx_init (DS_RESOLUTION_9_BIT);

    return rtc;
}

