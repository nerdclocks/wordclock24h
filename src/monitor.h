/*-------------------------------------------------------------------------------------------------------------------------------------------
 * monitor.h - interface declaration of monitor routines
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef MONITOR_H
#define MONITOR_H

#include "esp8266.h"

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * cursor positions for monitor messages
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#define IRMP_FRAME_LINE     0
#define DCF_MSG_LINE        0
#define ESP_SSID_LINE       0
#define ESP_SND_LINE        1
#define ESP_MSG_LINE        2
#define EEPROM_STAT_LINE    3
#define DS3231_STAT_LINE    4
#define ESP8266_STAT_LINE   5
#define DCF77_STAT_LINE     6
#define RTC_TIME_LINE       7
#define NET_TIME_LINE       8
#define DCF_TIME_LINE       9
#define TIM_TIME_LINE       10

#define MODE_LINE           18

#define MENU_1_LINE         20
#define MENU_2_LINE         21
#define MENU_3_LINE         22
#define LOG_LINE            23

#define IRMP_FRAME_COL      70
#define DCF_MSG_COL         24
#define ESP_SSID_COL        24
#define ESP_SND_COL         24
#define ESP_MSG_COL         24
#define EEPROM_STAT_COL     65
#define DS3231_STAT_COL     65
#define ESP8266_STAT_COL    65
#define DCF77_STAT_COL      65
#define RTC_TIME_COL        24
#define NET_TIME_COL        24
#define DCF_TIME_COL        24
#define TIM_TIME_COL        24

#define MODE_COL            0

#define MENU_1_COL          0
#define MENU_2_COL          0
#define MENU_3_COL          0
#define LOG_COL             0

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * declarations of monitor functions
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
extern void         monitor_show_letter (uint_fast8_t, uint_fast8_t, uint_fast8_t);
extern void         monitor_show_all_letters_off (void);
extern void         monitor_show_clock (uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t);
extern void         monitor_show_mode (uint_fast8_t);
extern void         monitor_show_menu (void);
extern void         monitor_show_status (ESP8266_CONNECTION_INFO *);
extern void         monitor_show_screen (uint_fast8_t, ESP8266_CONNECTION_INFO *);

#endif
