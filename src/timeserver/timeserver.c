/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * timeserver.c - timeserver routines
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include "timeserver.h"
#include "esp8266.h"
#include "eeprom.h"
#include "eeprom-data.h"
#include "log.h"

#define MAX_SSID_LEN            32
#define MAX_KEY_LEN             32
#define MAX_IPADDR_LEN          16

#define GMT                      0                              // GMT offset


/*--------------------------------------------------------------------------------------------------------------------------------------
 * Time server (RFC 868) or NTP server
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
static uint8_t          is_ntp_server;                          // flag if time server is ntp server
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
#if 0 // TODO
    static char     cmd[ESP8266_MAX_CMD_LEN];
    uint_fast8_t    line;

    clear ();

    do
    {
        line = 0;
        move (line, 0);
        addstr ("Command: ");
        clrtoeol ();
        getnstr (cmd, ESP8266_MAX_CMD_LEN - 2);                 // we want to append \r\0, below, see strcat() call
        line++;

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

                    move (line, 0);

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
                esp8266_send_cmd (cmd);

                nodelay (TRUE);

                while (line < 23)
                {
                    if (esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, 500) != ESP8266_TIMEOUT)
                    {
                        clrtoeol ();
                        refresh ();
                        line++;
                    }

                    if (getch() == KEY_ESCAPE)
                    {
                        break;
                    }
                }
            }
        }
    } while (*cmd);
#endif
}

uint_fast8_t
timeserver_set_timezone (int_fast16_t newtimezone)
{
    uint_fast8_t rtc = 0;

    if (newtimezone >= -12 && newtimezone >= 12)
    {
        if (timezone != newtimezone)
        {
            timezone = newtimezone;
            timeserver_write_data_to_eeprom ();
        }

        rtc = 1;
    }
    return rtc;
}

int_fast16_t
timeserver_get_timezone (void)
{
    return timezone;
}

unsigned char *
timeserver_get_timeserver (void)
{
    return timeserver;
}
/*--------------------------------------------------------------------------------------------------------------------------------------
 * Set new timeserver
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
timeserver_set_timeserver (char * new_timeserver)
{
    uint_fast8_t    ntp_success = 0;
    uint_fast8_t    time_success = 0;
    uint_fast8_t    new_is_ntp_server = 0;
    uint_fast8_t    rtc = 0;

    if (*new_timeserver)
    {
        time_t          check_time;

        log_msg ("Checking NTP protocol ... ");

        if (esp8266_get_ntp_time (new_timeserver, &check_time))
        {
            log_msg ("successful!");
            ntp_success = 1;
            new_is_ntp_server = 1;
        }
        else
        {
            log_msg ("failed!");
        }

        log_msg ("Checking TIME protocol (RFC 868) ... ");

        if (esp8266_get_time (new_timeserver, &check_time))
        {
            log_msg ("successful!");
            time_success = 1;
        }
        else
        {
            log_msg ("failed!");
        }

        if (ntp_success || time_success)
        {
            log_msg ("Timeserver successfully changed.");

            if (strcmp ((char *) timeserver, new_timeserver) || is_ntp_server != new_is_ntp_server)
            {
                is_ntp_server = new_is_ntp_server;
                strcpy ((char *) timeserver, new_timeserver);
                timeserver_write_data_to_eeprom ();
            }
            rtc = 1;
        }
        else
        {
            log_msg ("Timeserver NOT changed.");
        }
    }
    return rtc;
}
#if 0 // TODO
uint_fast8_t
timeserver_configure (uint_fast8_t next_line, ESP8266_CONNECTION_INFO * esp8266_connection_infop)
{
    uint_fast8_t    rtc = 0;
    static char     ssid[MAX_SSID_LEN + 1];
    static char     key[MAX_KEY_LEN + 1];
    static char     new_timeserver[MAX_IPADDR_LEN + 1];
    static char     answer[ESP8266_MAX_ANSWER_LEN + 1];
    uint8_t         ch;
    uint_fast8_t    i;


    mvaddstr (next_line, 10, "1. Configure access to AP");  next_line++;
    mvaddstr (next_line, 10, "2. Configure time server");   next_line++;
    mvaddstr (next_line, 10, "3. Configure time zone");     next_line++;
    mvaddstr (next_line, 10, "0. Exit");                    next_line += 2;

    move (next_line, 10);
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
                clear ();

                esp8266_reset ();
                esp8266_get_answer (answer, ESP8266_MAX_ANSWER_LEN, 100);
                esp8266_disconnect_from_access_point ();
                esp8266_connect_to_access_point (ssid, key);
                esp8266_reset ();
                esp8266_get_answer (answer, ESP8266_MAX_ANSWER_LEN, 100);

                mvaddstr (next_line + 3, 10, "Checking ESP8266 online status, please wait...");
                refresh ();

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
        uint_fast8_t    ntp_success = 0;
        uint_fast8_t    time_success = 0;
        uint_fast8_t    new_is_ntp_server = 0;

        move (next_line, 10);
        addstr ("current time server: ");
        addstr ((char *) timeserver);
        move (next_line + 1, 10);
        addstr ("new time server:     ");
        getnstr (new_timeserver, MAX_IPADDR_LEN);

        if (*new_timeserver)
        {
            uint_fast8_t    y;
            uint_fast8_t    x;
            time_t          check_time;

            move (next_line + 3, 10);
            addstr ("Checking NTP protocol ... ");
            getyx (y, x);

            if (esp8266_get_ntp_time (new_timeserver, &check_time))
            {
                mvaddstr (y, x, "successful!");
                ntp_success = 1;
                new_is_ntp_server = 1;
            }
            else
            {
                mvaddstr (y, x, "failed!");
            }

            move (next_line + 4, 10);
            addstr ("Checking TIME protocol (RFC 868) ... ");
            getyx (y, x);

            if (esp8266_get_time (new_timeserver, &check_time))
            {
                mvaddstr (y, x, "successful!");
                time_success = 1;
            }
            else
            {
                mvaddstr (y, x, "failed!");
            }

            move (next_line + 6, 10);

            if (ntp_success || time_success)
            {
                addstr ("Timeserver successfully changed. Press ENTER");

                if (strcmp ((char *) timeserver, new_timeserver) || is_ntp_server != new_is_ntp_server)
                {
                    is_ntp_server = new_is_ntp_server;
                    strcpy ((char *) timeserver, new_timeserver);
                    timeserver_write_data_to_eeprom ();
                }
            }
            else
            {
                addstr ("Timeserver NOT changed. Press ENTER");
            }

            while (getch() != KEY_CR)
            {
                ;
            }
        }
        rtc = 1;
    }
    else if (ch == '3')
    {
        uint8_t         tz[4];

        move (next_line, 10);
        addstr ("current time zone: GMT");

        if (timezone >= 0)
        {
            addch ('+');
        }
        printw ("%d", timezone);

        move (next_line + 1, 10);
        addstr ("new time zone:     GMT");
        getnstr ((char *) tz, 3);

        if (*tz)
        {
            timezone = atoi ((char *) tz);
            timeserver_write_data_to_eeprom ();
        }
        rtc = 1;
    }
    return rtc;
}
#endif

/*--------------------------------------------------------------------------------------------------------------------------------------
 * get time from time server via TCP (see RFC 868)
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
timeserver_get_time (struct tm * tmp)
{
    time_t          curtime;
    uint_fast8_t    rtc = 0;

    if (is_ntp_server)
    {
        rtc = esp8266_get_ntp_time ((char *) timeserver, &curtime);
    }
    else
    {
        rtc = esp8266_get_time ((char *) timeserver, &curtime);
    }

    if (rtc)
    {
        struct tm *	    mytm;
        uint_fast8_t    summertime = 0;

        curtime -= 2208988800U;
        curtime += 3600 * timezone;                                             // localtime() on STM32 returns GMT, but we want ME(S)Z

        mytm = localtime (&curtime);

        // calculate summer time:
        if (mytm->tm_mon >= 3 && mytm->tm_mon <= 8)             // april to september
        {
            summertime = 1;
        }
        else if (mytm->tm_mon == 2)                             // march
        {
            if (mytm->tm_mday - mytm->tm_wday >= 25)            // after or equal last sunday in march
            {
                if (mytm->tm_wday == 0)                         // today last sunday?
                {
                    if (mytm->tm_hour >= 2)                     // after 02:00 we have summer time
                    {
                        summertime = 1;
                    }
                }
                else
                {
                    summertime = 1;
                }
            }
        }
        else if (mytm->tm_mon == 9)                             // it's october
        {
            summertime = 1;

            if (mytm->tm_mday - mytm->tm_wday >= 25)            // it's after or equal last sunday in october...
            {
                if (mytm->tm_wday == 0)                         // today last sunday?
                {
                    if (mytm->tm_hour >= 3)                     // after 03:00 we have winter time
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
