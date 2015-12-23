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
#define ESP8266_MAX_ANSWER_LEN  80

/*--------------------------------------------------------------------------------------------------------------------------------------
 * possible return values of esp8266_get_answer():
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
#define ESP8266_TIMEOUT         0               // must be 0
#define ESP8266_OK              1
#define ESP8266_ERROR           2
#define ESP8266_ACCESS_POINT    3
#define ESP8266_LINKED          4
#define ESP8266_UNLINK          5
#define ESP8266_CONNECT         6               // firmware AT version:0.21.version:0.9.5:: 1,CONNECT instead of LINK
#define ESP8266_CLOSED          7               // firmware AT version:0.21.version:0.9.5:: 1,CLOSED instead of UNLINK
#define ESP8266_BUSY            8
#define ESP8266_ALREADY_CONNECT 9
#define ESP8266_LINK_TYPE_ERROR 10
#define ESP8266_READY           11
#define ESP8266_NO_CHANGE       12
#define ESP8266_IPD             13
#define ESP8266_ANSWER          14
#define ESP8266_UNSPECIFIED   0xFF

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
extern volatile uint_fast8_t    esp8266_ten_ms_tick;

extern uint_fast8_t             esp8266_get_answer (char *, uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast16_t);
extern uint_fast8_t             esp8266_send_cmd (char *);
extern uint_fast8_t             esp8266_reset (void);
extern void                     esp8266_powerdown (void);
extern void                     esp8266_powerup (void);
extern uint_fast8_t             esp8266_connect_to_access_point (char *, char *);
extern uint_fast8_t             esp8266_disconnect_from_access_point (void);
extern uint_fast8_t             esp8266_listen (ESP8266_LISTEN_DATA *);
extern char *                   esp8266_get_firmware_version ();
extern uint_fast8_t             esp8266_check_online_status (ESP8266_CONNECTION_INFO *);
extern uint_fast8_t             esp8266_get_time (char *, time_t *);
extern uint_fast8_t             esp8266_init (void);
#endif
