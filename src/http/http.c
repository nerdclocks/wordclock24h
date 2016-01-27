/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * http.c - simple HTTP server via ESP8266
 *
 * Copyright (c) 2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp8266.h"
#include "timeserver.h"
#include "ldr.h"
#include "remote-ir.h"
#include "display.h"
#include "tempsensor.h"
#include "ds18xx.h"
#include "eeprom.h"
#include "rtc.h"
#include "listener.h"
#include "log.h"
#include "delay.h"
#include "http.h"

#if defined (STM32F401RE) || defined (STM32F411RE)                  // STM32F401/STM32F411 Nucleo Board
#  define SEND_BUF_LEN          1024                                // we have enough RAM for buffering
#elif defined (STM32F103)                                           // STM32F103
#  define SEND_BUF_LEN          512                                 // save RAM
#endif

#define MAX_LINE_LEN            256                                 // max. length of line_buffer
#define MAX_PATH_LEN            20

#define NO_METHOD               0
#define GET_METHOD              1
#define POST_METHOD             2

#define MAX_PARAMS              5

static char                     send_buf[SEND_BUF_LEN + 1];
static int                      send_len;

typedef struct
{
    char *  name;
    char *  value;
} HTTP_PARAMETERS;

static HTTP_PARAMETERS          http_parameters[MAX_PARAMS];
static int                      bgcolor_cnt;

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * flush output buffer
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
http_flush (int fd)
{
    if (send_len)
    {
        esp8266_send_data (fd, (unsigned char *) send_buf, send_len);
        send_len = 0;
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * send string
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
http_send (int fd, char * s)
{
    int l = strlen (s);

    if (l + send_len > SEND_BUF_LEN)
    {
        http_flush (fd);
    }

    strcpy (send_buf + send_len, s);
    send_len += l;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set parameters from list
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
http_set_params (char * paramlist)
{
    char *  p;                                              // ap=access&pw=secret&action=saveap
    int     idx = 0;

    if (*paramlist)
    {
        http_parameters[idx].name = paramlist;

        for (p = paramlist; idx < MAX_PARAMS - 1 && *p; p++)
        {
            if (*p == '=')
            {
                *p = '\0';
                http_parameters[idx].value = p + 1;
            }
            else if (*p == '&')
            {
                *p = '\0';
                idx++;
                http_parameters[idx].name = p + 1;
            }
        }
        idx++;
    }

    while (idx < MAX_PARAMS)
    {
        http_parameters[idx].name = (char *) 0;
        idx++;
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * get a parameter
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static char *
http_get_param (char * name)
{
    int     idx;

    for (idx = 0; idx < MAX_PARAMS && http_parameters[idx].name != (char *) 0; idx++)
    {
        if (! strcmp (http_parameters[idx].name, name))
        {
            return http_parameters[idx].value;
        }
    }

    return "";
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * send http and html header
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
http_header (int fd, char * title)
{
    char * p = "HTTP/1.0 200 OK";

    http_send (fd, p);
    http_send (fd, "\r\n\r\n");
    http_send (fd, "<html>\r\n");
    http_send (fd, "<head>\r\n");
    http_send (fd, "<title>");
    http_send (fd, title);
    http_send (fd, "</title>\r\n");
    http_send (fd, "</head>\r\n");
    http_send (fd, "<body>\r\n");
    http_send (fd, "<H1>\r\n");
    http_send (fd, title);
    http_send (fd, "</H1>\r\n");
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * send html trailer
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
http_trailer (int fd)
{
    http_send (fd, "</body>\r\n");
    http_send (fd, "</html>\r\n");
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * send table header
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_header (int fd, char * col1, char * col2, char * col3)
{
    http_send (fd, "<table border=0>\r\n");
    http_send (fd, "<tr><th>");
    http_send (fd, col1);
    http_send (fd, "</th><th>");
    http_send (fd, col2);
    http_send (fd, "</th>");

    if (*col3)
    {
        http_send (fd, "<th>");
        http_send (fd, col3);
        http_send (fd, "</th></tr>\r\n");
    }
    bgcolor_cnt = 0;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * begin table row
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
begin_table_row (int fd)
{
    bgcolor_cnt++;

    if (bgcolor_cnt & 0x01)
    {
        http_send (fd, "<tr bgcolor=#f0f0f0>\r\n");
    }
    else
    {
        http_send (fd, "<tr>\r\n");
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * end table row
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
end_table_row (int fd)
{
    http_send (fd, "</tr>\r\n");
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row (int fd, char * col1, char * col2)
{
    begin_table_row (fd);
    http_send (fd, "<td>");
    http_send (fd, col1);
    http_send (fd, "</td><td>");
    http_send (fd, col2);
    http_send (fd, "</td><td></td>");
    end_table_row (fd);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row with input
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row_input (int fd, char * page, char * col1, char * id, char * col2, int maxlength)
{
    char maxlength_buf[8];

    sprintf (maxlength_buf, "%d", maxlength);

    begin_table_row (fd);
    http_send (fd, "<form method=\"POST\" action=\"/");
    http_send (fd, page);
    http_send (fd, "\">\r\n");

    http_send (fd, "<td>");
    http_send (fd, col1);
    http_send (fd, "</td>\r\n");

    http_send (fd, "<td>");
    http_send (fd, "<input type=\"text\" id=\"");
    http_send (fd, id);
    http_send (fd, "\" name=\"");
    http_send (fd, id);
    http_send (fd, "\" value=\"");
    http_send (fd, col2);
    http_send (fd, "\" maxlength=\"");
    http_send (fd, maxlength_buf);
    http_send (fd, "\">");

    http_send (fd, "</td><td><button type=\"submit\" name=\"action\" value=\"save");
    http_send (fd, id);
    http_send (fd, "\">Save</button>\r\n");

    http_send (fd, "</td>\r\n");
    http_send (fd, "</form>\r\n");
    end_table_row (fd);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row with multiple inputs
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row_inputs (int fd, char * page, char * col1, char * id, int n, char ** ids, char ** desc2, char ** col2, int * maxlength, int * maxsize)
{
    int     i;
    char    maxlength_buf[8];
    char    maxsize_buf[8];

    begin_table_row (fd);
    http_send (fd, "<form method=\"POST\" action=\"/");
    http_send (fd, page);
    http_send (fd, "\">\r\n");

    http_send (fd, "<td>");
    http_send (fd, col1);
    http_send (fd, "</td>\r\n");

    http_send (fd, "<td>");

    for (i = 0; i < n; i++)
    {
        sprintf (maxlength_buf, "%d", maxlength[i]);
        sprintf (maxsize_buf, "%d", maxsize[i]);

        http_send (fd, desc2[i]);
        http_send (fd, "&nbsp;<input type=\"text\" id=\"");
        http_send (fd, ids[i]);
        http_send (fd, "\" name=\"");
        http_send (fd, ids[i]);
        http_send (fd, "\" value=\"");
        http_send (fd, col2[i]);
        http_send (fd, "\" maxlength=\"");
        http_send (fd, maxlength_buf);
        http_send (fd, "\" size=\"");
        http_send (fd, maxsize_buf);
        http_send (fd, "\">");
        http_send (fd, "&nbsp;&nbsp;&nbsp;");
    }

    http_send (fd, "</td><td><button type=\"submit\" name=\"action\" value=\"save");
    http_send (fd, id);
    http_send (fd, "\">Save</button>\r\n");

    http_send (fd, "</td>\r\n");
    http_send (fd, "</form>\r\n");
    end_table_row (fd);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row with checkbox
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row_checkbox (int fd, char * page, char * col1, char * id, char * desc, int checked)
{
    begin_table_row (fd);
    http_send (fd, "<form method=\"POST\" action=\"/");
    http_send (fd, page);
    http_send (fd, "\">\r\n");
    http_send (fd, "<td>");
    http_send (fd, col1);
    http_send (fd, "</td><td>");
    http_send (fd, desc);
    http_send (fd, "&nbsp;<input type=\"checkbox\" name=\"");
    http_send (fd, id);
    http_send (fd, "\" value=\"active\" ");

    if (checked)
    {
        http_send (fd, "checked");
    }

    http_send (fd, ">");
    http_send (fd, "</td><td><button type=\"submit\" name=\"action\" value=\"save");
    http_send (fd, id);
    http_send (fd, "\">Save</button>\r\n");

    http_send (fd, "</td>\r\n");
    http_send (fd, "</form>\r\n");
    end_table_row (fd);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row with selection
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row_select (int fd, char * page, char * col1, char * id, const char ** col2, int selected_value, int max_values)
{
    char buf[3];
    int i;

    begin_table_row (fd);
    http_send (fd, "<form method=\"POST\" action=\"/");
    http_send (fd, page);
    http_send (fd, "\">\r\n");

    http_send (fd, "<td>");
    http_send (fd, col1);
    http_send (fd, "</td>\r\n");

    http_send (fd, "<td>");
    http_send (fd, "<select id=\"");
    http_send (fd, id);
    http_send (fd, "\" name=\"");
    http_send (fd, id);
    http_send (fd, "\">\r\n");

    for (i = 0; i < max_values; i++)
    {
        sprintf (buf, "%d", i);
        http_send (fd, "<option value=\"");
        http_send (fd, buf);
        http_send (fd, "\"");

        if (i == selected_value)
        {
            http_send (fd, " selected");
        }

        http_send (fd, ">");
        http_send (fd, (char *) col2[i]);
        http_send (fd, "</option>\r\n");
    }

    http_send (fd, "</select>\r\n");

    http_send (fd, "</td><td><button type=\"submit\" name=\"action\" value=\"save");
    http_send (fd, id);
    http_send (fd, "\">Save</button>\r\n");

    http_send (fd, "</td>\r\n");
    http_send (fd, "</form>\r\n");
    end_table_row (fd);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row with slider
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row_slider (int fd, char * page, char * col1, char * id, char * col2, char * min, char * max)
{
    begin_table_row (fd);
    http_send (fd, "<form method=\"POST\" action=\"/");
    http_send (fd, page);
    http_send (fd, "\">\r\n");

    http_send (fd, "<td>");
    http_send (fd, col1);
    http_send (fd, "</td>\r\n");

    http_send (fd, "<td>");
    http_send (fd, "<input type=\"range\" id=\"");
    http_send (fd, id);
    http_send (fd, "\" name=\"");
    http_send (fd, id);
    http_send (fd, "\" value=\"");
    http_send (fd, col2);
    http_send (fd, "\" min=\"");
    http_send (fd, min);
    http_send (fd, "\" max=\"");
    http_send (fd, max);
    http_send (fd, "\">");

    http_send (fd, "</td><td><button type=\"submit\" name=\"action\" value=\"save");
    http_send (fd, id);
    http_send (fd, "\">Save</button>\r\n");

    http_send (fd, "</td>\r\n");
    http_send (fd, "</form>\r\n");
    end_table_row (fd);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row with slider
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row_sliders (int fd, char * page, char * col1, char * id, int n, char ** ids, char ** desc2, char ** col2, char ** min, char ** max)
{
    int i;

    begin_table_row (fd);
    http_send (fd, "<form method=\"POST\" action=\"/");
    http_send (fd, page);
    http_send (fd, "\">\r\n");

    http_send (fd, "<td>");
    http_send (fd, col1);
    http_send (fd, "</td>\r\n");
    http_send (fd, "<td>");

    for (i = 0; i < n; i++)
    {
        http_send (fd, desc2[i]);
        http_send (fd, "&nbsp;<input type=\"range\" id=\"");
        http_send (fd, ids[i]);
        http_send (fd, "\" name=\"");
        http_send (fd, ids[i]);
        http_send (fd, "\" value=\"");
        http_send (fd, col2[i]);
        http_send (fd, "\" min=\"");
        http_send (fd, min[i]);
        http_send (fd, "\" max=\"");
        http_send (fd, max[i]);
        http_send (fd, "\" style=\"width:64px;\">");
        http_send (fd, "&nbsp;&nbsp;&nbsp;");
    }

    http_send (fd, "</td><td><button type=\"submit\" name=\"action\" value=\"save");
    http_send (fd, id);
    http_send (fd, "\">Save</button>\r\n");

    http_send (fd, "</td>\r\n");
    http_send (fd, "</form>\r\n");
    end_table_row (fd);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table trailer
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_trailer (int fd)
{
    http_send (fd, "</table>\r\n");
}

#define MAX_AP_LEN                  32
#define MAX_PW_LEN                  64
#define MAX_IP_LEN                  15
#define MAX_TIMEZONE_LEN            3
#define MAX_DATE_LEN                10
#define MAX_TIME_LEN                5
#define MAX_BRIGHTNESS_LEN          2
#define MAX_COLOR_VALUE_LEN         2

#define ANIMATION_MODE_NONE         0
#define ANIMATION_MODE_FADE         1
#define ANIMATION_MODE_ROLL         2
#define ANIMATION_MODE_EXPLODE      3
#define ANIMATION_MODE_RANDOM       4
#define ANIMATION_MODES             5

static int animation_mode = 1;

// TODO: Use definitions from tables.c & tables12h.c
#if WCLOCK24H == 1
#include "tables.h"
#else
#include "tables12h.h"
#endif

static const char * tbl_mode_names[MODES_COUNT];

static int display_mode = 10;

static void
http_menu (int fd)
{
    http_send (fd, "<a href=\"/\">Main</a>&nbsp;&nbsp;\r\n");
    http_send (fd, "<a href=\"/network\">Network</a>&nbsp;&nbsp;\r\n");
    http_send (fd, "<a href=\"/display\">Display</a>&nbsp;&nbsp;\r\n");
    http_send (fd, "<P>\r\n");
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * main page
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
http_main (int fd, LISTENER_DATA * ld)
{
    char *          action;
    char *          message         = (char *) 0;
    uint_fast8_t    rtc             = 0;
    char            rtc_temp[16];
    char            ds18xx_temp[16];
    uint_fast8_t    temp_index;

    if (rtc_is_up)
    {
        temp_index = rtc_get_temperature_index ();
        sprintf (rtc_temp, "%d", temp_index / 2);

        if (temp_index % 2)
        {
            strcat (rtc_temp, ".5");
        }

        strcat (rtc_temp, "&deg;C");
    }
    else
    {
        strcpy (rtc_temp, "offline");
    }

    if (ds18xx_is_up)
    {
        temp_index = temp_read_temp_index ();
        sprintf (ds18xx_temp, "%d", temp_index / 2);

        if (temp_index % 2)
        {
            strcat (ds18xx_temp, ".5");
        }

        strcat (ds18xx_temp, "&deg;C");
    }
    else
    {
        strcpy (ds18xx_temp, "offline");
    }

    action = http_get_param ("action");

    if (action)
    {
        if (! strcmp (action, "displaytemperature"))
        {
            message = "Displaying temperature...";
            rtc = LISTENER_DISPLAY_TEMPERATURE_CODE;
        }
        else if (! strcmp (action, "learnir"))
        {
            message = "Learning IR remote control...";
            rtc = LISTENER_IR_LEARN_CODE;
        }
        else if (! strcmp (action, "poweron"))
        {
            message = "Switching power on...";
            ld->power = 1;
            rtc = LISTENER_POWER_CODE;
        }
        else if (! strcmp (action, "poweroff"))
        {
            message = "Switching power off...";
            ld->power = 0;
            rtc = LISTENER_POWER_CODE;
        }
    }

    http_header (fd, "WordClock");

    http_menu (fd);

    table_header (fd, "Device", "Value", "");
    table_row (fd, "RTC temperature", rtc_temp);
    table_row (fd, "DS18xx", ds18xx_temp);
    table_row (fd, "EEPROM", eeprom_is_up ? "online" : "offline");
    table_trailer (fd);

    http_send (fd, "<form method=\"POST\" action=\"/\">\r\n");
    http_send (fd, "<button type=\"submit\" name=\"action\" value=\"poweron\">Power On</button>\r\n");
    http_send (fd, "<button type=\"submit\" name=\"action\" value=\"poweroff\">Power Off</button>\r\n");
    http_send (fd, "</form>\r\n");

    http_send (fd, "<form method=\"POST\" action=\"/\">\r\n");
    http_send (fd, "<button type=\"submit\" name=\"action\" value=\"displaytemperature\">Display temperature</button>\r\n");
    http_send (fd, "<button type=\"submit\" name=\"action\" value=\"learnir\">Learn IR remote control</button>\r\n");
    http_send (fd, "</form>\r\n");

    if (message)
    {
        http_send (fd, "<P>\r\n<font color=green>");
        http_send (fd, message);
        http_send (fd, "</font>\r\n");
    }

    http_trailer (fd);

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * network page
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
http_network (int fd)
{
    char *                      action;
    char *                      message         = (char *) 0;
    ESP8266_CONNECTION_INFO *   infop;
    char *                      esp_firmware_version;
    char *                      ids[2]          = { "ap", "pw" };
    char *                      desc[2]         = { "AP", "Key" };
    int                         maxlen[2]       = { MAX_AP_LEN, MAX_PW_LEN };
    int                         maxsize[2]      = { 15, 20 };
    char *                      col2[2];
    char                        timezone_str[MAX_TIMEZONE_LEN+1];
    uint_fast8_t                rtc             = 0;

    infop                   = esp8266_get_connection_info ();
    esp_firmware_version    = esp8266_get_firmware_version ();

    col2[0] = infop->accesspoint;
    col2[1] = "";

    sprintf (timezone_str, "%d", timeserver_get_timezone());

    action = http_get_param ("action");

    if (action)
    {
        if (! strcmp (action, "saveaccesspoint"))
        {
            char * ssid = http_get_param ("ap");
            char * key  = http_get_param ("pw");
            http_header (fd, "WordClock Network");
            http_send (fd, "<B>Connecting to Access Point, try again later.</B>");
            http_trailer (fd);

            esp8266_disconnect_from_access_point ();

            if (esp8266_connect_to_access_point (ssid, key))
            {
                char ipbuf[32];

                if (esp8266_check_online_status (0))
                {
                    if (infop->ipaddress && *(infop->ipaddress))
                    {
                        sprintf (ipbuf, "  IP %s", infop->ipaddress);
                        display_banner (ipbuf);
                    }
                }
            }
            return 0;
        }
        else if (! strcmp (action, "savetimeserver"))
        {
            char * newtimeserver = http_get_param ("timeserver");

            if (timeserver_set_timeserver (newtimeserver))
            {
                message = "Timeserver successfully changed.";
            }
            else
            {
                message = "Setting of new timeserver failed.";
            }
        }
        else if (! strcmp (action, "savetimezone"))
        {
            int tz = atoi (http_get_param ("timezone"));

            if (timeserver_set_timezone (tz))
            {
                message = "Timezone successfully changed.";
            }
            else
            {
                message = "Setting of new timezone failed.";
            }
        }
        else if (! strcmp (action, "nettime"))
        {
            message = "Getting net time";
            rtc = LISTENER_GET_NET_TIME_CODE;
        }
    }

    http_header (fd, "WordClock Network");
    http_menu (fd);

    table_header (fd, "Device", "Value", "Action");
    table_row (fd, "IP address", infop->ipaddress);
    table_row (fd, "ESP8266 firmware", esp_firmware_version);
    table_row_inputs (fd, "network", "Access Point", "accesspoint", 2, ids, desc, col2, maxlen, maxsize);
    table_row_input (fd, "network", "Time server", "timeserver", (char *) timeserver_get_timeserver (), MAX_IP_LEN);
    table_row_input (fd, "network", "Time zone (GMT +)", "timezone", timezone_str, MAX_TIMEZONE_LEN);
    table_trailer (fd);

    http_send (fd, "<form method=\"POST\" action=\"/network\">\r\n");
    http_send (fd, "<button type=\"submit\" name=\"action\" value=\"nettime\">Get net time</button>\r\n");
    http_send (fd, "</form>\r\n");

    if (message)
    {
        http_send (fd, "<P>\r\n<font color=green>");
        http_send (fd, message);
        http_send (fd, "</font>\r\n");
    }

    http_trailer (fd);

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * display page
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
http_display (int fd, LISTENER_DATA * ld)
{
    char *          action;
    char *          message         = (char *) 0;
    DSP_COLORS      rgb;
    char *          ids[3]          = { "red", "green", "blue" };
    char *          desc[3]         = { "R", "G", "B" };
    char *          rgb_buf[3];
    char *          minval[3]       = { "0",   "0",  "0" };
    char *          maxval[3]       = { "63", "63", "63" };
    uint_fast8_t    brightness;
    uint_fast8_t    auto_brightness_active;
    char            brbuf[MAX_BRIGHTNESS_LEN + 1];
    char            red_buf[MAX_COLOR_VALUE_LEN + 1];
    char            green_buf[MAX_COLOR_VALUE_LEN + 1];
    char            blue_buf[MAX_COLOR_VALUE_LEN + 1];
    uint_fast8_t    use_ldr;
    uint_fast8_t    ldr_value;
    char            ldr_buf[MAX_BRIGHTNESS_LEN + 1];
    uint_fast8_t    i;
    uint_fast8_t    rtc             = 0;

    use_ldr = ldr_get_ldr_status ();

    *ldr_buf = '\0';

    if (use_ldr)
    {
        ldr_start_conversion ();

        while (! ldr_poll_brightness (&ldr_value))
        {
            delay_msec (10);
        }
        sprintf (ldr_buf, "%d", ldr_value);
    }

    animation_mode = display_get_animation_mode ();
    brightness = display_get_brightness ();
    sprintf (brbuf, "%d", brightness);

    auto_brightness_active = display_get_automatic_brightness_control ();

    display_get_colors (&rgb);
    sprintf (red_buf, "%d", rgb.red);
    sprintf (green_buf, "%d", rgb.green);
    sprintf (blue_buf, "%d", rgb.blue);

    for (i = 0; i < MODES_COUNT; i++)
    {
#if WCLOCK24H == 1
        tbl_mode_names[i] = tbl_modes[i].description;
#else
        tbl_mode_names[i] = tbl_modes[i];
#endif
    }

    action = http_get_param ("action");

    if (action)
    {
        if (! strcmp (action, "saveldrconnected"))
        {
            if (! strcmp (http_get_param ("ldrconnected"), "active"))
            {
                use_ldr = 1;
                ld->automatic_brightness_control = 0;
            }
            else
            {
                use_ldr = 0;
                ld->automatic_brightness_control = 0;
            }

            ldr_set_ldr_status (use_ldr);

            rtc = LISTENER_SET_AUTOMATIC_BRIHGHTNESS_CODE;
        }
        else if (! strcmp (action, "saveauto"))
        {
            if (! strcmp (http_get_param ("auto"), "active"))
            {
                auto_brightness_active = 1;
            }
            else
            {
                auto_brightness_active = 0;
            }

            ld->automatic_brightness_control = auto_brightness_active;
            rtc = LISTENER_SET_AUTOMATIC_BRIHGHTNESS_CODE;
        }
        else if (! strcmp (action, "savebrightness"))
        {
            strncpy (brbuf, http_get_param ("brightness"), MAX_BRIGHTNESS_LEN);
            ld->brightness = atoi (brbuf);
            rtc = LISTENER_SET_BRIGHTNESS_CODE;
        }
        else if (! strcmp (action, "savecolors"))
        {
            strncpy (red_buf, http_get_param ("red"), MAX_COLOR_VALUE_LEN);
            strncpy (green_buf, http_get_param ("green"), MAX_COLOR_VALUE_LEN);
            strncpy (blue_buf, http_get_param ("blue"), MAX_COLOR_VALUE_LEN);

            rgb.red = atoi (red_buf);
            rgb.green = atoi (green_buf);
            rgb.blue = atoi (blue_buf);

            ld->rgb = rgb;
            rtc = LISTENER_SET_COLOR_CODE;
        }
        else if (! strcmp (action, "saveanimation"))
        {
            animation_mode = atoi (http_get_param ("animation"));
            ld->mode = animation_mode;
            rtc = LISTENER_ANIMATION_MODE_CODE;
        }
        else if (! strcmp (action, "savedisplaymode"))
        {
            ld->mode = atoi (http_get_param ("displaymode"));
            rtc = LISTENER_DISPLAY_MODE_CODE;
        }
        else if (! strcmp (action, "testdisplay"))
        {
            message = "Testing display...";
            rtc = LISTENER_TEST_DISPLAY_CODE;
        }
        else if (! strcmp (action, "poweron"))
        {
            message = "Switching power on...";
            ld->power = 1;
            rtc = LISTENER_POWER_CODE;
        }
        else if (! strcmp (action, "poweroff"))
        {
            message = "Switching power off...";
            ld->power = 0;
            rtc = LISTENER_POWER_CODE;
        }
        else if (! strcmp (action, "saveall"))
        {
            message = "Saving settings permanently...";
            rtc = LISTENER_SAVE_DISPLAY_CONFIGURATION;
        }
    }

    rgb_buf[0] = red_buf;
    rgb_buf[1] = green_buf;
    rgb_buf[2] = blue_buf;

    http_header (fd, "WordClock Display");
    http_menu (fd);

    table_header (fd, "Device", "Value", "Action");

    table_row_checkbox (fd, "display", "LDR", "ldrconnected", "LDR connected", use_ldr);

    if (use_ldr)
    {
        table_row (fd, "LDR", ldr_buf);
        table_row_checkbox (fd, "display", "LDR", "auto", "Automatic brightness", auto_brightness_active);
    }

    table_row_slider (fd, "display", "Brightness (1-15)", "brightness", brbuf, "0", "15");
    table_row_sliders (fd, "display", "Colors", "colors", 3, ids, desc, rgb_buf, minval, maxval);
    table_row_select (fd, "display", "Animation", "animation", animation_modes, animation_mode, ANIMATION_MODES);
    table_row_select (fd, "display", "Display Mode", "displaymode", tbl_mode_names, display_mode, MODES_COUNT);
    table_trailer (fd);

    http_send (fd, "<form method=\"POST\" action=\"/display\">\r\n");
    http_send (fd, "<button type=\"submit\" name=\"action\" value=\"poweron\">Power On</button>\r\n");
    http_send (fd, "<button type=\"submit\" name=\"action\" value=\"poweroff\">Power Off</button>\r\n");
    http_send (fd, "<button type=\"submit\" name=\"action\" value=\"testdisplay\">Test display</button>\r\n");
    http_send (fd, "<button type=\"submit\" name=\"action\" value=\"saveall\">Save all</button>\r\n");
    http_send (fd, "</form>\r\n");

    if (message)
    {
        http_send (fd, "<P>\r\n<font color=green>");
        http_send (fd, message);
        http_send (fd, "</font>\r\n");
    }

    http_trailer (fd);

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * hex to integer
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint16_t
htoi (char * buf, uint8_t max_digits)
{
    uint8_t     i;
    uint8_t     x;
    uint16_t    sum = 0;

    for (i = 0; i < max_digits && *buf; i++)
    {
        x = buf[i];

        if (x >= '0' && x <= '9')
        {
            x -= '0';
        }
        else if (x >= 'A' && x <= 'F')
        {
            x -= 'A' - 10;
        }
        else if (x >= 'a' && x <= 'f')
        {
            x -= 'a' - 10;
        }
        else
        {
            x = 0;
        }
        sum <<= 4;
        sum += x;
    }

    return (sum);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * http server
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
http_server (int fd, unsigned char * buf, int buflen, LISTENER_DATA * ld)
{
    char        line_buf[MAX_LINE_LEN];
    char        path[MAX_PATH_LEN];
    char *      p;
    char *      s;
    char *      t;
    int         idx = 0;
    int         at_first_line = 1;
    int         content_length = 0;
    int         at_eol = 0;
    int         method = NO_METHOD;
    int         ch;
    int         i;
    int         rtc = 0;

    line_buf[0] = '\0';
    path[0]     = '\0';

    while (buflen > 0 && *buf)
    {
        if (method != GET_METHOD && *buf != '\r' && *buf != '\n' && idx < MAX_LINE_LEN - 1)
        {
            line_buf[idx++] = buf[0];
        }

        ch = *buf++;
        buflen--;

        if (ch == '\r')
        {
            ;
        }
        else if (ch == '\n')
        {
            if (method != GET_METHOD)
            {
                line_buf[idx] = '\0';
            }

            idx = 0;

            if (at_first_line)
            {
                if (! strncmp (line_buf, "POST /", 6))
                {
                    method = POST_METHOD;

                    s = line_buf + 5;                                                   // 5: copy with slash
                    t = path;

                    for (i = 0; *s && *s != ' ' && i < MAX_PATH_LEN - 1; idx++)
                    {
                        *t++ = *s++;
                    }
                    *t = '\0';
                }
                else if (! strncmp (line_buf, "GET /", 5))
                {
                    method = GET_METHOD;

                    s = line_buf + 4;                                                   // 4: copy with slash
                    t = path;

                    for (i = 0; *s && *s != ' ' && *s != '&' && i < MAX_PATH_LEN - 1; idx++)
                    {
                        *t++ = *s++;
                    }
                    *t = '\0';

                    if (*s == '?')
                    {
                        char * t = line_buf;
                        s++;

                        while (*s && *s != ' ')
                        {
                            *t++ = *s++;
                        }
                        *t = '\0';

                    }
                    else if (*s == ' ')
                    {
                        line_buf[0] = '\0';
                    }
                }
                else
                {
                    method = NO_METHOD;
                }
            }
            else
            {
                if (! strnicmp (line_buf, "Content-Length: ", 16))
                {
                    content_length = atoi (line_buf + 16);
                }
            }

            at_first_line = 0;

            if (at_eol)
            {
                break;
            }
            at_eol = 1;
        }
        else
        {
            at_eol = 0;
        }
    }

    log_printf ("http: Path: '%s'\r\n", path);

    if (method == POST_METHOD)
    {
        log_printf ("http: content-length: '%d'\r\n", content_length);

        for (i = 0; i < content_length && i < MAX_LINE_LEN - 1 && buflen > 0 && *buf; i++)
        {
            line_buf[i] = *buf++;
            buflen--;
        }

        line_buf[i] = '\0';
    }

    for (p = line_buf; *p; p++)
    {
        if (*p == '%')
        {
            *p = htoi (p + 1, 2);

            t = p + 1;
            s = p + 3;

            while (*s)
            {
                *t++ = *s++;
            }
            *t = '\0';
        }
    }

    log_printf ("Data: '%s'\r\n", line_buf);

    if (method != NO_METHOD)
    {
        http_set_params (line_buf);

        if (! strcmp (path, "/"))
        {
            rtc = http_main (fd, ld);
        }
        else if (! strcmp (path, "/network"))
        {
            rtc = http_network (fd);
        }
        else if (! strcmp (path, "/display"))
        {
            rtc = http_display (fd, ld);
        }
        else
        {
            char * p = "HTTP/1.0 404 Not Found";
            log_msg (p);
            http_send (fd, p);
            http_send (fd, "\r\n\r\n");
            http_send (fd, "404 Not Found\r\n");
        }
    }
    else
    {
            char * p = "HTTP/1.0 404 Not Found";
            log_msg (p);
            http_send (fd, p);
            http_send (fd, "\r\n\r\n");
            http_send (fd, "404 Not Found\r\n");
    }

    http_flush (fd);

    sprintf (line_buf, "AT+CIPCLOSE=%d", fd);
    esp8266_send_cmd (line_buf);

    return rtc;
}
