/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * night.c - night time routines
 *
 * Copyright (c) 2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include "eeprom.h"
#include "eeprom-data.h"
#include "night.h"
#include "log.h"

/*--------------------------------------------------------------------------------------------------------------------------------------
 * globals:
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static NIGHT_TIME       night_time_off;
static NIGHT_TIME       night_time_on;

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * read configuration data from EEPROM
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
night_read_data_from_eeprom (void)
{
    unsigned char   night_time_buf[EEPROM_MAX_NIGHT_TIME_LEN];
    uint_fast8_t    rtc = 0;

    if (eeprom_is_up &&
        eeprom_read (EEPROM_DATA_OFFSET_NIGHT_TIME, night_time_buf, EEPROM_MAX_NIGHT_TIME_LEN))
    {
        night_time_off.night_time_active    = night_time_buf[0];
        night_time_off.night_time           = night_time_buf[1] * 60 + night_time_buf[2];
        night_time_on.night_time_active     = night_time_buf[3];
        night_time_on.night_time            = night_time_buf[4] * 60 + night_time_buf[5];

        rtc = 1;
    }
    else
    {
        night_time_off.night_time_active    = 0;
        night_time_off.night_time           = 0;
        night_time_on.night_time_active     = 0;
        night_time_on.night_time            = 0;
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * write configuration data to EEPROM
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
night_write_data_to_eeprom (void)
{
    unsigned char   night_time_buf[EEPROM_MAX_NIGHT_TIME_LEN];
    uint_fast8_t    rtc = 0;

    night_time_buf[0] = night_time_off.night_time_active;
    night_time_buf[1] = night_time_off.night_time / 60;                     // hh
    night_time_buf[2] = night_time_off.night_time % 60;                     // mm
    night_time_buf[3] = night_time_on.night_time_active;
    night_time_buf[4] = night_time_on.night_time / 60;                      // hh
    night_time_buf[5] = night_time_on.night_time % 60;                      // mm

    if (eeprom_is_up &&
        eeprom_write (EEPROM_DATA_OFFSET_NIGHT_TIME, night_time_buf, EEPROM_MAX_NIGHT_TIME_LEN))
    {
        rtc = 1;
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * get night time off
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
NIGHT_TIME *
night_get_night_time_off (void)
{
    return &night_time_off;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * get night time on
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
NIGHT_TIME *
night_get_night_time_on (void)
{
    return &night_time_on;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set night time off
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
night_set_night_time_off (NIGHT_TIME * new_night_time_p)
{
    memcpy (&night_time_off, new_night_time_p, sizeof (NIGHT_TIME));
    night_write_data_to_eeprom ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set night time on
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
night_set_night_time_on (NIGHT_TIME * new_night_time_p)
{
    memcpy (&night_time_on, new_night_time_p, sizeof (NIGHT_TIME));
    night_write_data_to_eeprom ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * init night times
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
night_init (void)
{
    return;                                                                 // nothing to do
}
