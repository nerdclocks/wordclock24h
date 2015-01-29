/*-------------------------------------------------------------------------------------------------------------------------------------------
 * esp8266.h - declarations of ESP8266 routines
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef ESP8266_H
#define ESP8266_H

#include <string.h>
#include <stdint.h>
#include <time.h>

#define ESP8266_MAX_CMD_LEN     80
#define ESP8266_MAX_ANSWER_LEN  64

/*--------------------------------------------------------------------------------------------------------------------------------------
 * possible return values of esp8266_get_answer():
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
#define ESP8266_TIMEOUT         0               // must be 0
#define ESP8266_OK              1
#define ESP8266_ERROR           2
#define ESP8266_LINKED          3
#define ESP8266_UNLINK          4
#define ESP8266_BUSY            5
#define ESP8266_READY           6
#define ESP8266_NO_CHANGE       7
#define ESP8266_IPD             8
#define ESP8266_UNKNOWN         0xFF

typedef struct
{
    uint_fast8_t    channel;
    uint_fast8_t    length;
    uint8_t *       data;
} ESP8266_LISTEN_DATA;

typedef struct
{
    char *          accesspoint;
    char *          ipaddress;
} ESP8266_CONNECTION_INFO;

extern uint_fast8_t             esp8266_is_up;
extern uint_fast8_t             esp8266_is_online;

extern uint_fast8_t             esp8266_get_answer (char *, uint_fast8_t, uint_fast8_t, uint_fast16_t);
uint_fast8_t                    esp8266_listen (ESP8266_LISTEN_DATA *);
extern uint_fast8_t             esp8266_send_cmd (char *);
extern uint_fast8_t             esp8266_reset (void);
extern void                     esp8266_powerdown (void);
extern void                     esp8266_powerup (void);
extern uint_fast8_t             esp8266_connect_to_access_point (char *, char *);
extern uint_fast8_t             esp8266_disconnect_from_access_point (void);
extern uint_fast8_t             esp8266_init (void);
extern uint_fast8_t             esp8266_check_online_status (ESP8266_CONNECTION_INFO *);
extern uint_fast8_t             esp8266_get_time (char *, time_t *);
extern void                     esp8266_ISR (void);                 // should be called every 10 ms

#endif
