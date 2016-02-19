/*-------------------------------------------------------------------------------------------------------------------------------------------
 * esp8266.h - declarations of ESP8266 routines
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
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
#define ESP8266_MAX_ANSWER_LEN  256                             // max length of ESP answer length, could be very long if "HTTP GET ..."

/*--------------------------------------------------------------------------------------------------------------------------------------
 * possible return values of esp8266_get_answer():
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
#define ESP8266_TIMEOUT         0
#define ESP8266_OK              1
#define ESP8266_ERROR           2
#define ESP8266_DEBUGMSG        3
#define ESP8266_IPADDRESS       4
#define ESP8266_ACCESSPOINT     5
#define ESP8266_TIME            6
#define ESP8266_FIRMWARE        7
#define ESP8266_CMD             8
#define ESP8266_HTTP_GET        9
#define ESP8266_UNSPECIFIED   0xFF

#define ESP8266_MAX_FIRMWARE_SIZE        16
#define ESP8266_MAX_ACCESSPOINT_SIZE     32
#define ESP8266_MAX_IPADDRESS_SIZE       16
#define ESP8266_MAX_HTTP_GET_PARAM_SIZE 256
#define ESP8266_MAX_CMD_SIZE             32
#define ESP8266_MAX_TIME_SIZE            16

typedef struct
{
    uint_fast8_t    is_up;
    uint_fast8_t    is_online;
    char            firmware[ESP8266_MAX_FIRMWARE_SIZE];
    char            accesspoint[ESP8266_MAX_ACCESSPOINT_SIZE];
    char            ipaddress[ESP8266_MAX_IPADDRESS_SIZE];
    char            http_get_param[ESP8266_MAX_HTTP_GET_PARAM_SIZE];
    char            cmd[ESP8266_MAX_CMD_SIZE];
    char            time[ESP8266_MAX_TIME_SIZE];
} ESP8266_INFO;

extern volatile uint_fast8_t        esp8266_ten_ms_tick;

extern uint_fast8_t                 esp8266_get_message (uint_fast16_t);
extern uint_fast8_t                 esp8266_send_cmd (char *);
extern void                         esp8266_send_data (unsigned char *, uint_fast8_t);
extern ESP8266_INFO *               esp8266_get_info (void);
extern uint_fast8_t                 esp8266_get_up_status (void);
extern uint_fast8_t                 esp8266_get_online_status (void);
extern char *                       esp8266_get_access_point_connected (void);
extern char *                       esp8266_get_ip_address (void);
extern char *                       esp8266_get_firmware_version (void);
extern void                         esp8266_reset (void);
extern void                         esp8266_powerdown (void);
extern void                         esp8266_powerup (void);
extern uint_fast8_t                 esp8266_connect_to_access_point (char *, char *);
extern uint_fast8_t                 esp8266_accesspoint (char *, char *);
extern void                         esp8266_start_timeserver_request (char *);
extern void                         esp8266_flash (void);
extern void                         esp8266_init (void);
#endif
