/*-------------------------------------------------------------------------------------------------------------------------------------------
 * ldr.c - ldr functions
 *
 * Copyright (c) 2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */

#include "mcurses.h"
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
    uint_fast8_t    rtc = 0;
    uint8_t         tmp_use_ldr;

    if (eeprom_is_up && eeprom_read (EEPROM_DATA_OFFSET_USE_LDR, &tmp_use_ldr, EEPROM_DATA_SIZE_USE_LDR))
    {
        use_ldr = tmp_use_ldr;
        rtc = 1;
    }
    else
    {
        use_ldr = 0;
    }

    return rtc;
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
 * ask user for SSID and key of access point, then configure ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
ldr_configure (uint_fast8_t next_line)
{
    uint8_t         ch;

    mvaddstr (next_line, 10, "Current configuration: ");

    if (use_ldr)
    {
        addstr ("LDR connected");
    }
    else
    {
        addstr ("LDR not connected");
    }

    next_line += 2;

    mvaddstr (next_line, 10, "1. LDR connected");       next_line++;
    mvaddstr (next_line, 10, "2. LDR not connected");   next_line++;
    mvaddstr (next_line, 10, "0. Exit");                next_line += 2;

    move (next_line, 10);
    refresh ();

    while ((ch = getch()) < '0' || ch > '2')
    {
        ;
    }

    if (ch == '1')
    {
        if (! use_ldr)
        {
            use_ldr = 1;
            ldr_write_data_to_eeprom ();
            ldr_init ();
        }
    }
    else if (ch == '2')
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
