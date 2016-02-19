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
#include "base.h"
#include "listener.h"
#include "timeserver.h"
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

#define MAX_PARAMETERS      10

uint_fast8_t
listener (LISTENER_DATA * ld)
{
    ESP8266_INFO *      info;
    uint_fast8_t        msg_rtc;
    uint_fast8_t        param[MAX_PARAMETERS];
    uint_fast8_t        rtc = 0;

    msg_rtc = esp8266_get_message (10);

    switch (msg_rtc)
    {
        case ESP8266_TIME:
            info = esp8266_get_info ();
            char * endptr;
            uint32_t seconds_since_1900 = strtoul (info->time, &endptr, 10);
            timeserver_convert_time (&(ld->tm), seconds_since_1900);
            rtc = LISTENER_SET_DATE_TIME_CODE;
            break;

        case ESP8266_CMD:
            info = esp8266_get_info ();
            char *  parameters = info->cmd;
            int     n = 0;

            while (n < MAX_PARAMETERS)
            {
                if (*parameters && *(parameters + 1))
                {
                    param[n++] = htoi (parameters, 2);
                    parameters += 2;

                    if (* parameters == ' ')
                    {
                        parameters++;
                    }
                }
                else
                {
                    break;
                }
            }

            if (n > 0)
            {
                switch (param[0])
                {
                    case LISTENER_ANIMATION_MODE_CODE:                  // Set Animation Mode
                    case LISTENER_DISPLAY_MODE_CODE:                    // Set Display Mode
                    {
                        if (n == 2)
                        {
                            rtc         = param[0];
                            ld->mode    = param[1];
                        }
                        break;
                    }

                    case LISTENER_SET_BRIGHTNESS_CODE:                  // Set Brightness
                    {
                        if (n == 2)
                        {
                            rtc             = param[0];
                            ld->brightness  = param[1];
                        }
                        break;
                    }

                    case LISTENER_SET_COLOR_CODE:                       // Set RGB Colors
                    {
                        if (n == 4)
                        {
                            rtc             = param[0];
                            ld->rgb.red     = param[1];
                            ld->rgb.green   = param[2];
                            ld->rgb.blue    = param[3];
                        }
                        break;
                    }

                    case LISTENER_SET_DATE_TIME_CODE:                   // Set Date/Time
                    {
                        if (n == 7)
                        {
                            rtc             = param[0];
                            ld->tm.tm_year  = param[1] + 2000;
                            ld->tm.tm_mon   = param[2] - 1;
                            ld->tm.tm_mday  = param[3];
                            ld->tm.tm_hour  = param[4];
                            ld->tm.tm_min   = param[5];
                            ld->tm.tm_sec   = param[6];
                        }
                        break;
                    }

                    case LISTENER_SET_AUTOMATIC_BRIHGHTNESS_CODE:       // Automatic Brightness control per LDR
                    {
                        if (n == 2)
                        {
                            rtc                                 = param[0];
                            ld->automatic_brightness_control    = param[1];
                        }
                        break;
                    }

                    case LISTENER_POWER_CODE:                           // Power on/off
                    {
                        if (n == 2)
                        {
                            rtc             = param[0];
                            ld->power       = param[1];
                        }
                        break;
                    }

                    case LISTENER_GET_NET_TIME_CODE:                    // Get net time
                    case LISTENER_SAVE_DISPLAY_CONFIGURATION:           // Save display settings
                    {
                        if (n == 1)
                        {
                            rtc = param[0];
                        }
                        break;
                    }
                }
            }
            break;

        case ESP8266_HTTP_GET:
            info = esp8266_get_info ();
            char * path = info->http_get_param;
            char * params = strchr (path, ' ');

            if (params)
            {
                *params = '\0';
                params += 1;
            }

            rtc = http_server (path, params, ld);
            break;
    }

    return rtc;
}
