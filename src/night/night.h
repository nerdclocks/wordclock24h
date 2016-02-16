/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * night.h - declarations of night time routines
 *
 * Copyright (c) 2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef NIGHT_H
#define NIGHT_H

#include <string.h>
#include <stdint.h>

typedef struct
{
    uint_fast8_t        night_time_active;
    uint_fast16_t       night_time;                                         // time in minutes 0 - 1439
} NIGHT_TIME;

extern uint_fast8_t     night_read_data_from_eeprom (void);
extern uint_fast8_t     night_write_data_to_eeprom (void);
extern NIGHT_TIME *     night_get_night_time_off (void);
extern NIGHT_TIME *     night_get_night_time_on (void);
extern void             night_set_night_time_off (NIGHT_TIME *);
extern void             night_set_night_time_on (NIGHT_TIME *);
extern void             night_init (void);

#endif
