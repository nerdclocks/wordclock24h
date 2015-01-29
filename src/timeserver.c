/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * timeserver.c - timeserver routines
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include "wclock24h-config.h"
#include "timeserver.h"
#include "esp8266.h"
#include "mcurses.h"
#include "monitor.h"
#include "eeprom.h"
#include "irmp.h"
#include "dsp.h"
#include "eeprom-data.h"

#define MAX_SSID_LEN            32
#define MAX_KEY_LEN             32
#define MAX_IPADDR_LEN          16

#define GMT                      0                              // GMT offset


/*--------------------------------------------------------------------------------------------------------------------------------------
 * Time server (RFC 868)
 *
 * Default time server is ntp3.ptb.de (192.53.103.103)
 * You can also use uclock.de (88.198.64.6) or any other RFC 868 time server
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
#define NET_TIME_HOST           "192.53.103.103"                // IP address of RFC 868 time server

/*--------------------------------------------------------------------------------------------------------------------------------------
 * Default GMT offset in seconds. For MEZ or MESZ, set it to 3600.
 * Summer time will be calculated automatically
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
#define NET_TIME_GMT_OFFSET     (GMT+1)                         // GMT offset, e.g. (GMT+1) for MEZ or MESZ

/*--------------------------------------------------------------------------------------------------------------------------------------
 * globals:
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static uint8_t          timeserver[MAX_IPADDR_LEN + 1] = NET_TIME_HOST;
static uint8_t          is_ntp_server;                          // flag if time server is ntp server, yet not used
static int_fast16_t     timezone = NET_TIME_GMT_OFFSET;         // from -12 to +12

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * read configuration data from EEPROM
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
timeserver_read_data_from_eeprom (void)
{
    uint_fast8_t    rtc = 0;
    uint8_t         tz[EEPROM_DATA_SIZE_TIMEZONE];

    if (eeprom_is_up &&
        eeprom_read (EEPROM_DATA_OFFSET_TIMESERVER, timeserver, MAX_IPADDR_LEN) &&
        eeprom_read (EEPROM_DATA_OFFSET_NTP_PROTOCOL, &is_ntp_server, EEPROM_DATA_SIZE_NTP_PROTOCOL) &&
        eeprom_read (EEPROM_DATA_OFFSET_TIMEZONE, tz, EEPROM_DATA_SIZE_TIMEZONE))
    {
        timezone = tz[1];

        if (tz[0] == '-')
        {
            timezone = -timezone;
        }

        rtc = 1;
    }
    else
    {
        strncpy ((char *) timeserver, NET_TIME_HOST, MAX_IPADDR_LEN);
        timezone = NET_TIME_GMT_OFFSET;
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * write configuration data to EEPROM
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
timeserver_write_data_to_eeprom (void)
{
    uint8_t         tz[EEPROM_MAX_TIMEZONE_LEN];
    uint_fast8_t    rtc = 0;

    if (timezone >= 0)
    {
        tz[0] = '+';
        tz[1] = timezone;
    }
    else
    {
        tz[0] = '-';
        tz[1] = -timezone;
    }

    if (eeprom_is_up &&
        eeprom_write (EEPROM_DATA_OFFSET_TIMESERVER, timeserver, EEPROM_DATA_SIZE_TIMESERVER) &&
        eeprom_write (EEPROM_DATA_OFFSET_NTP_PROTOCOL, &is_ntp_server, EEPROM_DATA_SIZE_NTP_PROTOCOL) &&
        eeprom_write (EEPROM_DATA_OFFSET_TIMEZONE, tz, EEPROM_DATA_SIZE_TIMEZONE))
    {
        rtc = 1;
    }

    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * ask user for a command and send this to ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
timeserver_cmd (void)
{
    static char     cmd[ESP8266_MAX_CMD_LEN];
    static char     answer[ESP8266_MAX_ANSWER_LEN + 1];
    uint_fast8_t    line;

    clear ();

    do
    {
        move (5, 0);
        addstr ("Command: ");
        clrtoeol ();
        getnstr (cmd, ESP8266_MAX_CMD_LEN - 2);                 // we want to append \r\0, below, see strcat() call

        if (*cmd)
        {
            if (*cmd == '!')                                    // only listen
            {
                ESP8266_LISTEN_DATA     l;
                uint32_t                cnt = 0;
                uint_fast8_t            rtc;

                while ((rtc = esp8266_listen (& l)) == 0)
                {
                    cnt++;

                    if (cnt == 8000000)
                    {
                        break;
                    }
                }

                if (rtc)
                {
                    uint_fast8_t i;

                    move (6, 0);

                    printw ("channel=%d, length=%d, data=", l.channel, l.length);

                    for (i = 0; i < l.length; i++)
                    {
                        if (l.data[i] >= 32 && l.data[i] < 127)
                        {
                            addch (l.data[i]);
                        }
                        else
                        {
                            printw ("<%02x>", l.data[i]);
                        }
                    }
                }
            }
            else
            {
                strcat (cmd, "\r");
                esp8266_send_cmd (cmd);

                for (line = 6; line < 23 && esp8266_get_answer (answer, ESP8266_MAX_ANSWER_LEN,  line, 1000) != ESP8266_TIMEOUT; line++)
                {
                    ;
                }
            }
        }
    } while (*cmd);
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * ask user for SSID and key of access point, then configure ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
timeserver_configure (ESP8266_CONNECTION_INFO * esp8266_connection_infop)
{
    static char     ssid[MAX_SSID_LEN + 1];
    static char     key[MAX_KEY_LEN + 1];
    static char     new_timeserver[MAX_IPADDR_LEN + 1];
    static char     answer[ESP8266_MAX_ANSWER_LEN + 1];
    uint8_t         ch;
    uint_fast8_t    i;
    uint_fast8_t    next_line;
    uint_fast8_t    rtc = 0;

    clear ();

    mvaddstr (3, 10, "1. Configure access to AP");
    mvaddstr (4, 10, "2. Configure time server");
    mvaddstr (5, 10, "3. Configure time zone");
    mvaddstr (6, 10, "0. Exit");
    move (7, 10);
    next_line = 9;
    refresh ();

    while ((ch = getch()) < '0' || ch > '3')
    {
        ;
    }

    if (ch == '1')
    {
        move (next_line, 10);
        addstr ("SSID: ");
        getnstr (ssid, MAX_SSID_LEN);

        if (*ssid)
        {
            move (next_line + 1, 10);
            addstr ("KEY:  ");
            getnstr (key, MAX_KEY_LEN);

            if (*key)
            {
                esp8266_reset ();
                esp8266_get_answer (answer, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 100);
                esp8266_disconnect_from_access_point ();
                esp8266_connect_to_access_point (ssid, key);
                esp8266_reset ();
                esp8266_get_answer (answer, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 100);

                for (i = 0; i < 64; i++)
                {
                    rtc = esp8266_check_online_status (esp8266_connection_infop);

                    if (rtc)
                    {
                        break;
                    }
                }
            }
        }
    }
    else if (ch == '2')
    {
        move (next_line, 10);
        addstr ("current time server: ");
        addstr ((char *) timeserver);
        move (next_line + 1, 10);
        addstr ("new time server:     ");
        getnstr (new_timeserver, MAX_IPADDR_LEN);

        if (*new_timeserver)
        {
            strcpy ((char *) timeserver, new_timeserver);
            timeserver_write_data_to_eeprom ();
        }
        rtc = 1;
    }
    else if (ch == '3')
    {
        uint8_t         tz[EEPROM_DATA_SIZE_TIMEZONE];

        move (next_line, 10);
        addstr ("current time zone: GMT");

        if (timezone >= 0)
        {
            addch ('+');
        }
        printw ("%d", timezone);

        move (next_line + 1, 10);
        addstr ("new time zone:     GMT");
        getnstr ((char *) tz, EEPROM_DATA_SIZE_TIMEZONE);

        if (*tz)
        {
            timezone = atoi ((char *) tz);
            timeserver_write_data_to_eeprom ();
        }
        rtc = 1;
    }
    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * get time from time server via TCP (see RFC 868)
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
timeserver_get_time (struct tm * tmp)
{
    time_t          curtime;
    uint_fast8_t    rtc = 0;

    rtc = esp8266_get_time ((char *) timeserver, &curtime);

    if (rtc)
    {
        struct tm *	    mytm;
        uint_fast8_t    summertime = 0;

        curtime -= 2208988800U;
        curtime += 3600 * timezone;                                             // localtime() on STM32 returns GMT, but we want ME(S)Z

        mytm = localtime (&curtime);

        // calculate summer time:
        if (mytm->tm_mon >= 2 && mytm->tm_mon <= 9)                             // month: march - october
        {
            if (mytm->tm_mon == 2 && mytm->tm_mday - mytm->tm_wday >= 25)       // after or equal last sunday in march
            {
                if (mytm->tm_wday == 0)                                         // today last sunday?
                {
                    if (mytm->tm_hour >= 2)                                     // after 02:00 we have summer time
                    {
                        summertime = 1;
                    }
                }
                else
                {
                    summertime = 1;
                }
            }
            else if (mytm->tm_mon == 9)                                         // it's october
            {
                summertime = 1;

                if (mytm->tm_mday - mytm->tm_wday >= 25)                        // it's after or equal last sunday in october...
                {
                    if (mytm->tm_wday == 0)                                     // today last sunday?
                    {
                        if (mytm->tm_hour >= 3)                                 // after 03:00 we have winter time
                        {
                            summertime = 0;
                        }
                    }
                    else
                    {
                        summertime = 0;
                    }
                }
            }
            else
            {
                summertime = 1;
            }
        }

        if (summertime)
        {
            curtime += 3600;                                                    // add one hour more for MESZ
            mytm = localtime (&curtime);
        }

        tmp->tm_year     = mytm->tm_year;
        tmp->tm_mon      = mytm->tm_mon;
        tmp->tm_mday     = mytm->tm_mday;
        tmp->tm_isdst    = mytm->tm_isdst;
        tmp->tm_hour     = mytm->tm_hour;
        tmp->tm_min      = mytm->tm_min;
        tmp->tm_sec      = mytm->tm_sec;
    }

	return rtc;
}


/*--------------------------------------------------------------------------------------------------------------------------------------
 * initialize timeserver routines
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
timeserver_init (void)
{
    uint_fast8_t    rtc;

    rtc = esp8266_init ();

    return rtc;
}

