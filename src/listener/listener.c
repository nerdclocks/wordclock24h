/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * listener.c - ip listener routines
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
#include "wclock24h-config.h"
#include "listener.h"
#include "http.h"
#include "esp8266.h"
#include "eeprom.h"
#include "eeprom-data.h"
#include "log.h"

/*--------------------------------------------------------------------------------------------------------------------------------------
 * listener
 *
 * parses following commands:
 *
 *  Command       Length    description
 *  -----------------------------------------------------
 *  <D>m            2       Set display mode
 *  <M>m            2       Set display mode (deprecated)
 *  <A>m            2       Set animation mode
 *  <C>rgb          4       RGB values (0..31)
 *  <T>ymdhms       7       Date/time
 *  <P>p            2       Power on/off
 *  <N>             1       Get net time
 *  <S>             1       Save
 *--------------------------------------------------------------------------------------------------------------------------------------
 */

uint_fast8_t
listener (LISTENER_DATA * ld)
{
    ESP8266_LISTEN_DATA l;
    uint_fast8_t        rtc;

    rtc = esp8266_listen (& l);

    if (rtc)
    {
        rtc = 0;

        if (l.channel == 0)                                                 // UDP request
        {
            if (l.length > 0)
            {
#if 0 // TODO
                if (mcurses_is_up)
                {
                    mvprintw (ESP_LOG_LINE, ESP_LOG_COL, "channel=%d, length=%d, data=", l.channel, l.length);

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
                    clrtoeol ();
                }
#endif

                switch (l.data[0])
                {
                    case 'A':                                           // Set Animation Mode
                    case 'D':                                           // Set Display Mode
                    {
                        if (l.length == 2)
                        {
                            rtc         = l.data[0];
                            ld->mode    = l.data[1];
                        }
                        break;
                    }

                    case 'B':                                           // Set Brightness
                    {
                        if (l.length == 2)
                        {
                            rtc             = l.data[0];
                            ld->brightness  = l.data[1];
                        }
                        break;
                    }

                    case 'C':                                           // Set RGB Colors
                    {
                        if (l.length == 4)
                        {
                            rtc             = l.data[0];
                            ld->rgb.red     = l.data[1];
                            ld->rgb.green   = l.data[2];
                            ld->rgb.blue    = l.data[3];
                        }
                        break;
                    }

                    case 'T':                                           // Set Date/Time
                    {
                        if (l.length == 7)
                        {
                            rtc             = l.data[0];
                            ld->tm.tm_year  = l.data[1] + 2000;
                            ld->tm.tm_mon   = l.data[2] - 1;
                            ld->tm.tm_mday  = l.data[3];
                            ld->tm.tm_hour  = l.data[4];
                            ld->tm.tm_min   = l.data[5];
                            ld->tm.tm_sec   = l.data[6];
                        }
                        break;
                    }

                    case 'L':                                           // Automatic Brightness control per LDR
                    {
                        if (l.length == 2)
                        {
                            rtc                                 = l.data[0];
                            ld->automatic_brightness_control    = l.data[1];
                        }
                        break;
                    }

                    case 'P':                                           // Power on/off
                    {
                        if (l.length == 2)
                        {
                            rtc             = l.data[0];
                            ld->power       = l.data[1];
                        }
                        break;
                    }

                    case 'N':                                           // Get Net Time
                    case 'S':                                           // Save Settings
                    {
                        if (l.length == 1)
                        {
                            rtc = l.data[0];
                        }
                        break;
                    }
                }
            }
        }
        else // if (l.channel == 1)                                     // channel = 1,2,3,4: browser via TCP
        {
            if (l.length > 0)
            {
#if 0                                                                   // don't log, too much traffic
                unsigned int i;

                for (i = 0; i < l.length; i++)
                {
                    log_char (l.data[i]);
                }
#endif
                rtc = http_server (l.channel, l.data, l.length, ld);
            }
            else
            {
                log_msg ("listen: length = 0");

            }
        }
    }
    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * initialize listener routines
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
listener_init (void)
{
    uint_fast8_t    rtc;

    rtc = esp8266_init ();

    return rtc;
}

