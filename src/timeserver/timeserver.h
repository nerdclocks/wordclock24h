/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * timeserver.h - declarations of ESP8266 routines
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef TIMESERVER_H
#define TIMESERVER_H

#include <string.h>
#include <stdint.h>
#include <time.h>
#include "esp8266.h"

extern uint_fast8_t     timeserver_read_data_from_eeprom (void);
extern uint_fast8_t     timeserver_write_data_to_eeprom (void);
extern void             timeserver_cmd (void);
extern uint_fast8_t     timeserver_set_timezone (int_fast16_t);
extern int_fast16_t     timeserver_get_timezone (void);
extern uint_fast8_t     timeserver_set_timeserver (char *);
extern unsigned char *  timeserver_get_timeserver (void);
extern void             timeserver_start_timeserver_request (void);
extern void             timeserver_convert_time (struct tm *, uint32_t);

#endif
