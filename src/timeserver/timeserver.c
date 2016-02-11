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
#include "http.h"
#include "esp8266.h"
#include "eeprom.h"
#include "eeprom-data.h"
#include "log.h"

#define MAX_IPADDR_LEN          16
#define GMT                      0                              // GMT offset


/*--------------------------------------------------------------------------------------------------------------------------------------
 * NTP server
 *
 * Default time server is ntp3.ptb.de (192.53.103.103)
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
#define NET_TIME_HOST           "192.53.103.103"                // IP address of default timeserver

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

uint_fast8_t
timeserver_set_timezone (int_fast16_t newtimezone)
{
    uint_fast8_t rtc = 0;

    log_printf ("New timezone: %d\r\n", newtimezone);

    if (newtimezone >= -12 && newtimezone <= 12)
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
    uint_fast8_t    rtc;

    is_ntp_server = 0;                                                      // not used anymore
    strcpy ((char *) timeserver, new_timeserver);
    rtc = timeserver_write_data_to_eeprom ();
    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * start timeserver request
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
timeserver_start_timeserver_request (void)
{
    esp8266_start_timeserver_request ((char *) timeserver);
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * convert time
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
timeserver_convert_time (struct tm * tmp, uint32_t seconds_since_1900)
{
    time_t          curtime;

    struct tm *	    mytm;
    uint_fast8_t    summertime = 0;

    curtime = (time_t) (seconds_since_1900 - 2208988800U);
    curtime += 3600 * timezone;                                             // localtime() on STM32 returns GMT, but we want ME(S)Z

    mytm = localtime (&curtime);

    // calculate summer time:
    if (mytm->tm_mon >= 3 && mytm->tm_mon <= 8)                             // april to september
    {
        summertime = 1;
    }
    else if (mytm->tm_mon == 2)                                             // march
    {
        if (mytm->tm_mday - mytm->tm_wday >= 25)                            // after or equal last sunday in march
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
    }
    else if (mytm->tm_mon == 9)                                             // it's october
    {
        summertime = 1;

        if (mytm->tm_mday - mytm->tm_wday >= 25)                            // it's after or equal last sunday in october...
        {
            if (mytm->tm_wday == 0)                                         // today last sunday?
            {
                if (mytm->tm_hour >= 3)                                     // after 03:00 we have winter time
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
