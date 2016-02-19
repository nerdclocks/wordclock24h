/*-------------------------------------------------------------------------------------------------------------------------------------------
 * ldr.c - ldr functions
 *
 * Copyright (c) 2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */

#include "adc.h"
#include "ldr.h"
#include "eeprom.h"
#include "eeprom-data.h"

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * start conversion
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
ldr_start_conversion (void)
{
    adc_start_single_conversion ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * poll LDR value
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
ldr_poll_brightness  (uint_fast8_t * brightness_p)
{
    uint16_t        adc_value;
    uint_fast8_t    rtc;

    rtc = adc_poll_conversion_value (&adc_value);

    if (rtc)
    {
        *brightness_p = (adc_value >> 8) & 0x0F;    // adc_value has 12 bits, but we need only 4 bits (16 values) for brightness
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * initialize LDR
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
ldr_init (void)
{
    adc_init ();
}
