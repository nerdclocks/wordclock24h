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
#include "dsp.h"
#include "tables.h"


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

        mvaddch (y, x, display[0][y][x]);
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
    uint_fast8_t y = illumination[0][idx].row;
    uint_fast8_t x = illumination[0][idx].col;
    uint_fast8_t l = illumination[0][idx].len;

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
 *   index ==   0  ->   0�C
 *   index == 250  -> 125�C
 *
 * the first temperature we can show is 10,0�C (index =  0, temperature_index ==  20)
 * the last  temperature we can show is 39,5�C (index = 79, temperature_index == 159)

 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_temperature (uint_fast8_t temperature_index)
{
    if (mcurses_is_up)
    {
        uint_fast8_t   temp_mode = MODES_COUNT - 1;
        uint_fast8_t   idx = 0;

        move (TEMP_DEGREE_LINE, TEMP_DEGREE_COL);

        if (temperature_index != 0xFF)
        {
            printw ("%d", temperature_index / 2);

            if (temperature_index & 1)
            {
                addstr (".5");
            }
            addstr ("�C      ");
        }
        else
        {
            addstr ("<unknown>");
        }

        if (temperature_index >= 20 && temperature_index < 80)
        {
            uint8_t                         minute_mode;
            const struct MinuteDisplay *    tbl_minute;

            temperature_index -= 20;                                            // subtract 10�C (20 units)

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
    const struct MinuteDisplay *    tbl_minute;
    const uint8_t *                 word_idx_p;
    uint_fast8_t                    idx = 0;

    if (mcurses_is_up)
    {
        hour_mode   = tbl_modes[mode].hour_txt;
        minute_mode = tbl_modes[mode].minute_txt;
        tbl_minute  = &tbl_minutes[minute_mode][minute];

        memset (words, 0, WP_COUNT);
        monitor_show_all_letters_off ();

        mvprintw (TIM_TIME_LINE, TIM_TIME_COL, "%02d:%02d:%02d", hour, minute, second);

        words[WP_ES] = 1;
        words[WP_IST] = 1;

        for (idx = 0; idx < MAX_MINUTE_WORDS && tbl_minute->wordIdx[idx] != 0; idx++)
        {
            words[tbl_minute->wordIdx[idx]] = 1;
        }

        hour += tbl_minute->hourOffset;                         // correct the hour offset from the minutes
        word_idx_p = tbl_hours[hour_mode][hour];                // get the hour words from hour table

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
 * show mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_modes (uint_fast8_t display_mode, uint_fast8_t animation_mode)
{
    if (mcurses_is_up)
    {
        mvprintw (ANIMATION_MODE_LINE, ANIMATION_MODE_COL, "Animation Mode: %2d: ", animation_mode);
        addstr (animation_modes[animation_mode]);
        clrtoeol ();
        mvprintw (DISPLAY_MODE_LINE, DISPLAY_MODE_COL,     "Display   Mode: %2d: ", display_mode);
        addstr (tbl_modes[display_mode].description);
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
        //                                           1         2         3         4         5         6         7         8
        //                                  12345678901234567890123456789012345678901234567890123456789012345678901234567890
        mvaddstr (MENU_1_LINE, MENU_1_COL, " h/H: inc/dec hour   m/M: inc/dec min    a/A: inc/dec anim   d/D: inc/dec mode  ");
        mvaddstr (MENU_2_LINE, MENU_2_COL, " r/R: inc/dec red    g/G: inc/dec green  b/B: inc/dec blue   p:   power on/off  ");
        mvaddstr (MENU_3_LINE, MENU_3_COL, " t:   get temp       n: get net time     c:   configure      s:   save          ");
        mvaddstr (MENU_4_LINE, MENU_4_COL, " e:   eeprom dump    l: logout                                                  ");
        attrset (A_NORMAL);
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * show status
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_status (ESP8266_CONNECTION_INFO * esp8266_connection_infop)
{
    if (mcurses_is_up)
    {
        if (esp8266_is_online)
        {
            move (ESP_SSID_LINE, ESP_SSID_COL);
            addstr ("SSID: ");
            addstr (esp8266_connection_infop->accesspoint);
            addstr ("   IP address: ");
            addstr (esp8266_connection_infop->ipaddress);
            clrtoeol ();
        }
        mvaddstr (EEPROM_STAT_LINE, EEPROM_STAT_COL, eeprom_is_up    ? "EEPROM  up    " : "EEPROM  off   ");
        mvaddstr (DS18XX_STAT_LINE, DS18XX_STAT_COL, ds18xx_is_up    ? "DS18xx  up    " : "DS18xx  off   ");
        mvaddstr (DS3231_STAT_LINE, DS3231_STAT_COL, rtc_is_up       ? "RTC     up    " : "RTC     off   ");
        mvaddstr (ESP8266_STAT_LINE, ESP8266_STAT_COL, esp8266_is_up ? (esp8266_is_online ? "ESP8266 online" : "ESP8266 up    ") : "ESP8266 off   ");
        mvaddstr (DCF77_STAT_LINE, DCF77_STAT_COL, dcf77_is_up       ? "DCF77   up    " : "DCF77   off   ");
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * show complete screen
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
monitor_show_screen (uint_fast8_t display_mode, uint_fast8_t animation_mode, ESP8266_CONNECTION_INFO * esp8266_connection_infop)
{
    if (mcurses_is_up)
    {
        clear ();
        monitor_show_all_letters_off ();                        // show display off
        monitor_show_modes (display_mode, animation_mode);      // show current mode
        monitor_show_menu ();                                   // show menu
        monitor_show_status (esp8266_connection_infop);
    }
}
