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

uint_fast8_t            ldr_is_up;

static uint_fast8_t     use_ldr = 0;

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
 * read configuration data from EEPROM
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
ldr_read_data_from_eeprom (void)
{
    uint8_t         tmp_use_ldr;

    if (eeprom_is_up &&
        eeprom_read (EEPROM_DATA_OFFSET_USE_LDR, &tmp_use_ldr, EEPROM_DATA_SIZE_USE_LDR) &&
        tmp_use_ldr == 0x01)                                            // 0xFF means off
    {
        use_ldr = 1;
    }
    else
    {
        use_ldr = 0;
    }

    return use_ldr;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * write configuration data to EEPROM
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
ldr_write_data_to_eeprom (void)
{
    uint8_t         tmp_use_ldr;
    uint_fast8_t    rtc = 0;

    tmp_use_ldr = use_ldr;

    if (eeprom_is_up && eeprom_write (EEPROM_DATA_OFFSET_USE_LDR, &tmp_use_ldr, EEPROM_DATA_SIZE_USE_LDR))
    {
        rtc = 1;
    }

    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * get LDR status
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
ldr_get_ldr_status (void)
{
    return use_ldr;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * set LDR status
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
ldr_set_ldr_status (uint_fast8_t connected)
{
    if (connected)
    {
        if (! use_ldr)
        {
            use_ldr = 1;
            ldr_write_data_to_eeprom ();
            ldr_init ();
        }
    }
    else
    {
        if (use_ldr)
        {
            use_ldr = 0;
            ldr_write_data_to_eeprom ();
            ldr_is_up = 0;
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * initialize LDR
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
ldr_init (void)
{
    ldr_read_data_from_eeprom ();

    if (use_ldr)
    {
        adc_init ();

        if (adc_is_up)
        {
            ldr_is_up = 1;
        }
    }
}
