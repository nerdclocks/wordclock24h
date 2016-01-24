/*-------------------------------------------------------------------------------------------------------------------------------------------
 * monitor.h - interface declaration of wclock12/24 monitor
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
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

#define COL1                 0
#define COL2                24
#define COL3                46
#define COL4                70

#define TIM_TIME_LINE       0
#define TIM_TIME_COL        COL3

#define IRMP_FRAME_LINE     0
#define IRMP_FRAME_COL      COL4

#define EEPROM_STAT_LINE    1
#define EEPROM_STAT_COL     COL2

#define DS18XX_STAT_LINE    2
#define DS18XX_STAT_COL     COL2
#define DS18XX_TEMP_LINE    2
#define DS18XX_TEMP_COL     COL4

#define DS3231_STAT_LINE    3
#define DS3231_STAT_COL     COL2
#define RTC_TIME_LINE       3
#define RTC_TIME_COL        COL3
#define RTC_TEMP_LINE       3
#define RTC_TEMP_COL        COL4

#define ESP8266_STAT_LINE   4
#define ESP8266_STAT_COL    COL2
#define NET_TIME_LINE       4
#define NET_TIME_COL        COL3

#define ESP_FW_LINE         5
#define ESP_FW_COL          COL2
#define ESP_FW_VALUE_LINE   5
#define ESP_FW_VALUE_COL    COL3

#define ESP_SSID_LINE       6
#define ESP_SSID_COL        COL2
#define ESP_SSID_VALUE_LINE 6
#define ESP_SSID_VALUE_COL  COL3

#define ESP_IP_LINE         7
#define ESP_IP_COL          COL2
#define ESP_IP_VALUE_LINE   7
#define ESP_IP_VALUE_COL    COL3

#define DCF77_STAT_LINE     8
#define DCF77_STAT_COL      COL2
#define DCF_TIME_LINE       8
#define DCF_TIME_COL        COL3

#define LDR_STAT_LINE       9
#define LDR_STAT_COL        COL2
#define LDR_BRIGHTNESS_LINE 9
#define LDR_BRIGHTNESS_COL  COL3

#define AUTO_STAT_LINE      10
#define AUTO_STAT_COL       COL2
#define COLORS_LINE         11
#define COLORS_COL          COL2
#define BRIGHTNESS_LINE     11
#define BRIGHTNESS_COL      COL3

#define DCF_MSG_LINE        12
#define DCF_MSG_COL         COL2

#define ANIMATION_MODE_LINE 17
#define ANIMATION_MODE_COL  COL1
#define DISPLAY_MODE_LINE   18
#define DISPLAY_MODE_COL    COL1

#define DCF77_LOG_LINE      20
#define DCF77_LOG_COL       COL1
#define ESP_SND_LINE        21
#define ESP_SND_COL         COL1
#define ESP_LOG_LINE        22
#define ESP_LOG_COL         COL1

#define LOG_LINE            ESP_LOG_LINE
#define LOG_COL             ESP_LOG_COL

#define MENU_LINE           23
#define MENU_COL            COL1

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * declarations of monitor functions
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
extern void         monitor_show_letter (uint_fast8_t, uint_fast8_t, uint_fast8_t);
extern void         monitor_show_all_letters_off (void);
extern void         monitor_show_temperature_ds (uint_fast8_t);
extern void         monitor_show_temperature_rtc (uint_fast8_t);
extern void         monitor_show_temperature_on_display (uint_fast8_t);
extern void         monitor_show_clock (uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t);
extern void         monitor_show_ldr_value (uint_fast8_t);
extern void         monitor_show_modes (uint_fast8_t, uint_fast8_t);
extern void         monitor_show_help (void);
extern void         monitor_show_menu (void);
extern void         monitor_show_status (ESP8266_CONNECTION_INFO *, uint_fast8_t);
extern void         monitor_show_colors (void);
extern void         monitor_show_screen ();

#endif
