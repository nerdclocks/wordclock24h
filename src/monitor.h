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
#define IRMP_FRAME_COL      70

#define DCF_MSG_LINE        0
#define DCF_MSG_COL         24

#define ESP_SSID_LINE       0
#define ESP_SND_LINE        1
#define ESP_MSG_LINE        2

#define ESP_SSID_COL        24
#define ESP_SND_COL         24
#define ESP_MSG_COL         24

#define EEPROM_STAT_LINE    4
#define DS18XX_STAT_LINE    5
#define DS3231_STAT_LINE    6
#define ESP8266_STAT_LINE   7
#define DCF77_STAT_LINE     8
#define LDR_STAT_LINE       9
#define AUTO_STAT_LINE      10

#define EEPROM_STAT_COL     24
#define DS18XX_STAT_COL     24
#define DS3231_STAT_COL     24
#define ESP8266_STAT_COL    24
#define DCF77_STAT_COL      24
#define LDR_STAT_COL        24
#define AUTO_STAT_COL       24

#define COLORS_LINE         11
#define BRIGHTNESS_LINE     COLORS_LINE
#define COLORS_COL          24
#define BRIGHTNESS_COL      42

#define TEMP_DEGREE_LINE    DS18XX_STAT_LINE
#define RTC_TIME_LINE       DS3231_STAT_LINE
#define NET_TIME_LINE       ESP8266_STAT_LINE
#define DCF_TIME_LINE       DCF77_STAT_LINE
#define LDR_BRIGHTNESS_LINE LDR_STAT_LINE

#define TIM_TIME_LINE       4
#define TIM_TIME_COL        42

#define TEMP_DEGREE_COL     42
#define RTC_TIME_COL        42
#define NET_TIME_COL        42
#define DCF_TIME_COL        42
#define LDR_BRIGHTNESS_COL  42

#define ANIMATION_MODE_COL  0
#define DISPLAY_MODE_COL    0

#define ANIMATION_MODE_LINE 16
#define DISPLAY_MODE_LINE   17

#define MENU_1_LINE         18
#define MENU_2_LINE         19
#define MENU_3_LINE         20
#define MENU_4_LINE         21
#define MENU_5_LINE         22

#define MENU_1_COL          0
#define MENU_2_COL          0
#define MENU_3_COL          0
#define MENU_4_COL          0
#define MENU_5_COL          0

#define LOG_LINE            23
#define LOG_COL             0

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * declarations of monitor functions
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
extern void         monitor_show_letter (uint_fast8_t, uint_fast8_t, uint_fast8_t);
extern void         monitor_show_all_letters_off (void);
extern void         monitor_show_temperature (uint_fast8_t);
extern void         monitor_show_clock (uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t);
extern void         monitor_show_ldr_value (uint_fast8_t);
extern void         monitor_show_modes (uint_fast8_t, uint_fast8_t);
extern void         monitor_show_menu (void);
extern void         monitor_show_status (ESP8266_CONNECTION_INFO *, uint_fast8_t);
extern void         monitor_show_colors (void);
extern void         monitor_show_screen ();

#endif
