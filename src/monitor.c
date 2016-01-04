/*-------------------------------------------------------------------------------------------------------------------------------------------
 * monitor.c - routines for monitor (mcurses)
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "wclock24h-config.h"
#include "mcurses.h"
#include "monitor.h"
#include "dcf77.h"
#include "esp8266.h"
#include "eeprom.h"
#include "ds18xx.h"
#include "rtc.h"
#include "ldr.h"
#include "dsp.h"

#if WCLOCK24H == 1
#include "tables.h"
#else
#include "tables12h.h"
#endif


/*-------------------------------------------------------------------------------------------------------------------------------------------
 * show one letter
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_letter (uint_fast8_t y, uint_fast8_t x, uint_fast8_t onoff)
{
    if (mcurses_is_up)
    {
        if (onoff)
        {
            attrset (A_REVERSE);
        }
        else
        {
            attrset (A_NORMAL);
        }

#if WCLOCK24H == 1
        mvaddch (y, x, display[0][y][x]);
#else
        mvaddch (y, x, display[y][x]);
#endif
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * show all letters off
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_all_letters_off (void)
{
    uint_fast8_t y;
    uint_fast8_t x;

    if (mcurses_is_up)
    {
        for (y = 0; y < WC_ROWS; y++)
        {
            for (x = 0; x < WC_COLUMNS; x++)
            {
                monitor_show_letter (y, x, 0);
            }
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * show one word
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
monitor_show_word (uint_fast8_t idx)
{
#if WCLOCK24H == 1
    uint_fast8_t y = illumination[0][idx].row;
    uint_fast8_t x = illumination[0][idx].col;
    uint_fast8_t l = illumination[0][idx].len;
#else
    uint_fast8_t y = illumination[idx].row;
    uint_fast8_t x = illumination[idx].col;
    uint_fast8_t l = illumination[idx].len;
#endif

    if (mcurses_is_up)
    {
        while (l--)
        {
            monitor_show_letter (y, x, 1);
            x++;
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * display temperature
 *
 *   index ==   0  ->   0°C
 *   index == 250  -> 125°C
 *
 * the first temperature we can show is 10,0°C (index =  0, temperature_index ==  20)
 * the last  temperature we can show is 39,5°C (index = 79, temperature_index == 159)

 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_temperature (uint_fast8_t temperature_index)
{
    if (mcurses_is_up)
    {
        move (TEMP_DEGREE_LINE, TEMP_DEGREE_COL);

        if (temperature_index != 0xFF)
        {
            printw ("%d", temperature_index / 2);

            if (temperature_index & 1)
            {
                addstr (".5");
            }
            addstr ("°C      ");
        }
        else
        {
            addstr ("<unknown>");
        }

#if WCLOCK24H == 1
        if (temperature_index >= 20 && temperature_index < 80)
        {
            uint_fast8_t                    temp_mode = MODES_COUNT - 1;
            uint_fast8_t                    idx = 0;
            uint8_t                         minute_mode;
            const struct MinuteDisplay *    tbl_minute;

            temperature_index -= 20;                                            // subtract 10°C (20 units)

            minute_mode = tbl_modes[temp_mode].minute_txt;
            tbl_minute  = &tbl_minutes[minute_mode][temperature_index];

            monitor_show_all_letters_off ();
            monitor_show_word (WP_ES);
            monitor_show_word (WP_IST);

            for (idx = 0; idx < MAX_MINUTE_WORDS && tbl_minute->wordIdx[idx] != 0; idx++)
            {
                monitor_show_word (tbl_minute->wordIdx[idx]);
            }

            attrset (A_NORMAL);
            refresh ();
        }
#endif
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * show clock time
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_clock (uint_fast8_t mode, uint_fast8_t hour, uint_fast8_t minute, uint_fast8_t second)
{
    static uint8_t                  words[WP_COUNT];
    uint_fast8_t                    hour_mode;
    uint_fast8_t                    minute_mode;
#if WCLOCK24H == 1
    const struct MinuteDisplay *    tbl_minute;
#else
    const struct minute_display *   tbl_minute;
#endif
    const uint8_t *                 word_idx_p;
    uint_fast8_t                    idx = 0;

    if (mcurses_is_up)
    {
#if WCLOCK24H == 1
        minute_mode = tbl_modes[mode].minute_txt;
        tbl_minute  = &tbl_minutes[minute_mode][minute];
        hour_mode = tbl_modes[mode].hour_txt;
#else
        minute_mode = mode;
        tbl_minute  = &tbl_minutes[minute_mode][minute];
        hour_mode = tbl_minute->hour_mode;
#endif

        memset (words, 0, WP_COUNT);
        monitor_show_all_letters_off ();

        mvprintw (TIM_TIME_LINE, TIM_TIME_COL, "%02d:%02d:%02d", hour, minute, second);

        words[WP_ES] = 1;
        words[WP_IST] = 1;

        for (idx = 0; idx < MAX_MINUTE_WORDS && tbl_minute->wordIdx[idx] != 0; idx++)
        {
            words[tbl_minute->wordIdx[idx]] = 1;
        }

#if WCLOCK24H == 0                                                  // WC12h: we have only 12 hours
        if (hour >= HOUR_COUNT)
        {
            hour -= HOUR_COUNT;
        }
#endif

        hour += tbl_minute->hourOffset;                             // correct the hour offset from the minutes

        if (hour >= HOUR_COUNT)
        {
            hour -= HOUR_COUNT;
        }

        word_idx_p = tbl_hours[hour_mode][hour];                    // get the hour words from hour table

        for (idx = 0; idx < MAX_HOUR_WORDS && word_idx_p[idx] != 0; idx++)
        {
            words[word_idx_p[idx]] = 1;
        }

        for (idx = 0; idx < WP_COUNT; idx++)
        {
            if (words[idx])
            {
                monitor_show_word (idx);
            }
        }

        attrset (A_NORMAL);
        refresh ();
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * show LDR value
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_ldr_value (uint_fast8_t brightness)
{
    if (mcurses_is_up)
    {
        mvprintw (LDR_BRIGHTNESS_LINE, LDR_BRIGHTNESS_COL, "%02u", brightness);
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * show mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_modes (uint_fast8_t display_mode, uint_fast8_t animation_mode)
{
    if (mcurses_is_up)
    {
        mvprintw (ANIMATION_MODE_LINE, ANIMATION_MODE_COL, "Anim. Mode: %2d: ", animation_mode);
        addstr (animation_modes[animation_mode]);
        clrtoeol ();
        mvprintw (DISPLAY_MODE_LINE, DISPLAY_MODE_COL,     "Disp. Mode: %2d: ", display_mode);
#if WCLOCK24H == 1
        addstr (tbl_modes[display_mode].description);
#else
        addstr (tbl_modes[display_mode]);
#endif
        clrtoeol ();
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * display menu
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_menu (void)
{
    if (mcurses_is_up)
    {
        attrset (A_REVERSE);
        //                                       1         2         3         4         5         6         7         8
        //                              12345678901234567890123456789012345678901234567890123456789012345678901234567890
        mvaddstr (MENU_LINE, MENU_COL, "                                                                         x=Help ");
        attrset (A_NORMAL);
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * display menu
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_help (void)
{
    if (mcurses_is_up)
    {
        clear ();
        mvaddstr ( 1, 1, "h/H   increment/decrement hour");
        mvaddstr ( 2, 1, "m/M   increment/decrement minute");
        mvaddstr ( 3, 1, "a/A   increment/decrement animation");
        mvaddstr ( 4, 1, "d/D   increment/decrement display mode");
        mvaddstr ( 5, 1, "r/R   increment/decrement red color brightness");
        mvaddstr ( 6, 1, "g/G   increment/decrement green color brightness");
        mvaddstr ( 7, 1, "b/B   increment/decrement blue color brightness");
        mvaddstr ( 8, 1, "w/W   increment/decrement global brightness");
        mvaddstr ( 9, 1, "q     automatic brightness on/off");
        mvaddstr (10, 1, "t     get temperature");
        mvaddstr (11, 1, "n     get network time");
        mvaddstr (12, 1, "i     learn IR");
        mvaddstr (13, 1, "c     configure");
        mvaddstr (14, 1, "s     save settings");
        mvaddstr (15, 1, "e     eeprom dump");
        mvaddstr (16, 1, "T     test LEDs");
        mvaddstr (17, 1, "p     power on/off");
        mvaddstr (18, 1, "l     logout");

        mvaddstr (LINES - 1, 1, "Press ENTER to return.");

        while (getch () != KEY_CR)
        {
            ;
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * show status of device
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
monitor_show_device_status (uint_fast8_t line, uint_fast8_t column, char * device, char * status)
{
    uint_fast8_t x;

    if (mcurses_is_up)
    {
        mvaddstr (line,  column, device);
        mvaddstr (line,  column + 10, status);
        x = column + 10 + strlen (status);

        while (x != COL3)
        {
            addch (' ');
            x++;
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * show status
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_status (ESP8266_CONNECTION_INFO * esp8266_connection_infop, uint_fast8_t auto_brightness)
{
    char * ds18xx_type;
    char * esp_status;

    if (mcurses_is_up)
    {
        if (esp8266_is_up)
        {
            char * fw = esp8266_get_firmware_version ();

            if (fw)
            {
                move (ESP_FW_LINE, ESP_FW_COL);
                addstr ("ESP8266   FW");
                move (ESP_FW_VALUE_LINE, ESP_FW_VALUE_COL);
                addstr (fw);
            }
        }

        if (esp8266_is_online)
        {
            move (ESP_SSID_LINE, ESP_SSID_COL);
            addstr ("ESP8266   SSID");
            move (ESP_SSID_VALUE_LINE, ESP_SSID_VALUE_COL);
            addstr (esp8266_connection_infop->accesspoint);

            move (ESP_IP_LINE, ESP_IP_COL);
            addstr ("ESP8266   IP");
            move (ESP_IP_VALUE_LINE, ESP_IP_VALUE_COL);
            addstr (esp8266_connection_infop->ipaddress);
            clrtoeol ();
        }

        if (ds18xx_is_up)
        {
            switch (ds18xx_get_family_code())
            {
                case DS1822_FAMILY_CODE:    ds18xx_type = "DS1822";     break;
                case DS18B20_FAMILY_CODE:   ds18xx_type = "DS18B20";    break;
                default:                    ds18xx_type = "DS18(S)20";  break;
            }
        }
        else
        {
            ds18xx_type = "DS18xx";
        }

        if (esp8266_is_up)
        {
            if (esp8266_is_online)
            {
                esp_status = "online";
            }
            else
            {
                esp_status = "up";
            }
        }
        else
        {
            esp_status = "off";
        }

        monitor_show_device_status (EEPROM_STAT_LINE,  EEPROM_STAT_COL,  "EEPROM",    eeprom_is_up    ? "up" : "off");
        monitor_show_device_status (DS18XX_STAT_LINE,  DS18XX_STAT_COL,  ds18xx_type, ds18xx_is_up    ? "up" : "off");
        monitor_show_device_status (DS3231_STAT_LINE,  DS3231_STAT_COL,  "RTC",       rtc_is_up       ? "up" : "off");
        monitor_show_device_status (ESP8266_STAT_LINE, ESP8266_STAT_COL, "ESP8266",   esp_status);
        monitor_show_device_status (DCF77_STAT_LINE,   DCF77_STAT_COL,   "DCF77",     dcf77_is_up     ? "up" : "off");
        monitor_show_device_status (LDR_STAT_LINE,     LDR_STAT_COL,     "LDR",       ldr_is_up       ? "up" : "off");
        monitor_show_device_status (AUTO_STAT_LINE,    AUTO_STAT_COL,    "AUTO BR.",  auto_brightness ? "on" : "off");
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * show color values
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_colors (void)
{
    uint_fast8_t    brightness;
    DSP_COLORS      rgb;

    if (mcurses_is_up)
    {
        dsp_get_colors (&rgb);
        brightness = dsp_get_brightness ();
        mvprintw (COLORS_LINE, COLORS_COL, "RGB       %02d %02d %02d", rgb.red, rgb.green, rgb.blue);
        mvprintw (BRIGHTNESS_LINE, BRIGHTNESS_COL, "Brightness: %02d", brightness);
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * show complete screen
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_screen (void)
{
    if (mcurses_is_up)
    {
        clear ();
        monitor_show_all_letters_off ();                        // show display off
        monitor_show_menu ();                                   // show menu
    }
}
