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
#include "eeprom-data.h"
#include "rtc.h"
#include "listener.h"
#include "log.h"
#include "delay.h"
#include "http.h"

#define MAX_LINE_LEN            256                                 // max. length of line_buffer
#define MAX_PATH_LEN            20

#define NO_METHOD               0
#define GET_METHOD              1
#define POST_METHOD             2

#define MAX_PARAMS              5

typedef struct
{
    char *  name;
    char *  value;
} HTTP_PARAMETERS;

static HTTP_PARAMETERS          http_parameters[MAX_PARAMS];
static int                      bgcolor_cnt;

#define TIMEOUT_MSEC(x)         (x/10)

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * flush output buffer
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
http_flush (void)
{
    esp8266_send_data ((unsigned char *) ".\r\n", 3);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * send string
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
http_send (char * s)
{
    int l = strlen (s);

    esp8266_send_data ((unsigned char *) s, l);
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
http_header (char * title)
{
    char * p = "HTTP/1.0 200 OK";

    http_send (p);
    http_send ("\r\n\r\n");
    http_send ("<html>\r\n");
    http_send ("<head>\r\n");
    http_send ("<title>");
    http_send (title);
    http_send ("</title>\r\n");
    http_send ("</head>\r\n");
    http_send ("<body>\r\n");
    http_send ("<H1>\r\n");
    http_send (title);
    http_send ("</H1>\r\n");
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * send html trailer
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
http_trailer (void)
{
    http_send ("</body>\r\n");
    http_send ("</html>\r\n");
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * send table header
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_header (char * col1, char * col2, char * col3)
{
    http_send ("<table border=0>\r\n");
    http_send ("<tr><th>");
    http_send (col1);
    http_send ("</th><th>");
    http_send (col2);
    http_send ("</th>");

    if (*col3)
    {
        http_send ("<th>");
        http_send (col3);
        http_send ("</th></tr>\r\n");
    }
    bgcolor_cnt = 0;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * begin table row
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
begin_table_row (void)
{
    bgcolor_cnt++;

    if (bgcolor_cnt & 0x01)
    {
        http_send ("<tr bgcolor=#f0f0f0>\r\n");
    }
    else
    {
        http_send ("<tr>\r\n");
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * end table row
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
end_table_row (void)
{
    http_send ("</tr>\r\n");
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * dump eeprom
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static void
http_eeprom_dump (void)
{
    uint8_t         buffer[16 + 1];
    char            buf[8];
    uint_fast16_t   start_addr = 0;
    uint_fast8_t    c;

    http_send ("<PRE>");

    while (start_addr < EEPROM_DATA_END)
    {
        sprintf (buf, "%04x", start_addr);
        http_send (buf);
        http_send ("  ");

        if (eeprom_read (start_addr, buffer, 16))
        {
            for (c = 0; c < 16; c++)
            {
                sprintf (buf, "%02x", buffer[c]);
                http_send (buf);
                http_send (" ");
            }

            http_send (" ");

            for (c = 0; c < 16; c++)
            {
                if (buffer[c] < 32 || buffer[c] >= 127)
                {
                    buffer[c] = '.';
                }
            }

            buffer[c] = '\0';
            http_send ((char *) buffer);
            http_send ("\r\n");

            start_addr += 16;
        }
        else
        {
            break;
        }
    }

    http_send ("</PRE>");
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row (char * col1, char * col2)
{
    begin_table_row ();
    http_send ("<td>");
    http_send (col1);
    http_send ("</td><td>");
    http_send (col2);
    http_send ("</td><td></td>");
    end_table_row ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row with input
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row_input (char * page, char * col1, char * id, char * col2, int maxlength)
{
    char maxlength_buf[8];

    sprintf (maxlength_buf, "%d", maxlength);

    begin_table_row ();
    http_send ("<form method=\"GET\" action=\"/");
    http_send (page);
    http_send ("\">\r\n");

    http_send ("<td>");
    http_send (col1);
    http_send ("</td>\r\n");

    http_send ("<td>");
    http_send ("<input type=\"text\" id=\"");
    http_send (id);
    http_send ("\" name=\"");
    http_send (id);
    http_send ("\" value=\"");
    http_send (col2);
    http_send ("\" maxlength=\"");
    http_send (maxlength_buf);
    http_send ("\">");

    http_send ("</td><td><button type=\"submit\" name=\"action\" value=\"save");
    http_send (id);
    http_send ("\">Save</button>\r\n");

    http_send ("</td>\r\n");
    http_send ("</form>\r\n");
    end_table_row ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row with multiple inputs
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row_inputs (char * page, char * col1, char * id, int n, char ** ids, char ** desc2, char ** col2, int * maxlength, int * maxsize)
{
    int     i;
    char    maxlength_buf[8];
    char    maxsize_buf[8];

    begin_table_row ();
    http_send ("<form method=\"GET\" action=\"/");
    http_send (page);
    http_send ("\">\r\n");

    http_send ("<td>");
    http_send (col1);
    http_send ("</td>\r\n");

    http_send ("<td>");

    for (i = 0; i < n; i++)
    {
        sprintf (maxlength_buf, "%d", maxlength[i]);
        sprintf (maxsize_buf, "%d", maxsize[i]);

        http_send (desc2[i]);
        http_send ("&nbsp;<input type=\"text\" id=\"");
        http_send (ids[i]);
        http_send ("\" name=\"");
        http_send (ids[i]);
        http_send ("\" value=\"");
        http_send (col2[i]);
        http_send ("\" maxlength=\"");
        http_send (maxlength_buf);
        http_send ("\" size=\"");
        http_send (maxsize_buf);
        http_send ("\">");
        http_send ("&nbsp;&nbsp;&nbsp;");
    }

    http_send ("</td><td><button type=\"submit\" name=\"action\" value=\"save");
    http_send (id);
    http_send ("\">Save</button>\r\n");

    http_send ("</td>\r\n");
    http_send ("</form>\r\n");
    end_table_row ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row with checkbox
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row_checkbox (char * page, char * col1, char * id, char * desc, int checked)
{
    begin_table_row ();
    http_send ("<form method=\"GET\" action=\"/");
    http_send (page);
    http_send ("\">\r\n");
    http_send ("<td>");
    http_send (col1);
    http_send ("</td><td>");
    http_send (desc);
    http_send ("&nbsp;<input type=\"checkbox\" name=\"");
    http_send (id);
    http_send ("\" value=\"active\" ");

    if (checked)
    {
        http_send ("checked");
    }

    http_send (">");
    http_send ("</td><td><button type=\"submit\" name=\"action\" value=\"save");
    http_send (id);
    http_send ("\">Save</button>\r\n");

    http_send ("</td>\r\n");
    http_send ("</form>\r\n");
    end_table_row ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row with selection
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row_select (char * page, char * col1, char * id, const char ** col2, int selected_value, int max_values)
{
    char buf[3];
    int i;

    begin_table_row ();
    http_send ("<form method=\"GET\" action=\"/");
    http_send (page);
    http_send ("\">\r\n");

    http_send ("<td>");
    http_send (col1);
    http_send ("</td>\r\n");

    http_send ("<td>");
    http_send ("<select id=\"");
    http_send (id);
    http_send ("\" name=\"");
    http_send (id);
    http_send ("\">\r\n");

    for (i = 0; i < max_values; i++)
    {
        sprintf (buf, "%d", i);
        http_send ("<option value=\"");
        http_send (buf);
        http_send ("\"");

        if (i == selected_value)
        {
            http_send (" selected");
        }

        http_send (">");
        http_send ((char *) col2[i]);
        http_send ("</option>\r\n");
    }

    http_send ("</select>\r\n");

    http_send ("</td><td><button type=\"submit\" name=\"action\" value=\"save");
    http_send (id);
    http_send ("\">Save</button>\r\n");

    http_send ("</td>\r\n");
    http_send ("</form>\r\n");
    end_table_row ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row with slider
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row_slider (char * page, char * col1, char * id, char * col2, char * min, char * max)
{
    begin_table_row ();
    http_send ("<form method=\"GET\" action=\"/");
    http_send (page);
    http_send ("\">\r\n");

    http_send ("<td>");
    http_send (col1);
    http_send ("</td>\r\n");

    http_send ("<td>");
    http_send ("<input type=\"range\" id=\"");
    http_send (id);
    http_send ("\" name=\"");
    http_send (id);
    http_send ("\" value=\"");
    http_send (col2);
    http_send ("\" min=\"");
    http_send (min);
    http_send ("\" max=\"");
    http_send (max);
    http_send ("\">");

    http_send ("</td><td><button type=\"submit\" name=\"action\" value=\"save");
    http_send (id);
    http_send ("\">Save</button>\r\n");

    http_send ("</td>\r\n");
    http_send ("</form>\r\n");
    end_table_row ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table row with slider
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_row_sliders (char * page, char * col1, char * id, int n, char ** ids, char ** desc2, char ** col2, char ** min, char ** max)
{
    int i;

    begin_table_row ();
    http_send ("<form method=\"GET\" action=\"/");
    http_send (page);
    http_send ("\">\r\n");

    http_send ("<td>");
    http_send (col1);
    http_send ("</td>\r\n");
    http_send ("<td>\r\n");

    for (i = 0; i < n; i++)
    {
        http_send (desc2[i]);
        http_send ("&nbsp;<input type=\"range\" id=\"");
        http_send (ids[i]);
        http_send ("\" name=\"");
        http_send (ids[i]);
        http_send ("\" value=\"");
        http_send (col2[i]);
        http_send ("\" min=\"");
        http_send (min[i]);
        http_send ("\" max=\"");
        http_send (max[i]);
        http_send ("\" style=\"width:64px;\">");
        http_send ("&nbsp;&nbsp;&nbsp;\r\n");
    }

    http_send ("</td><td><button type=\"submit\" name=\"action\" value=\"save");
    http_send (id);
    http_send ("\">Save</button>\r\n");

    http_send ("</td>\r\n");
    http_send ("</form>\r\n");
    end_table_row ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * table trailer
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
table_trailer (void)
{
    http_send ("</table>\r\n");
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

static int display_mode;

static void
http_menu (void)
{
    http_send ("<a href=\"/\">Main</a>&nbsp;&nbsp;\r\n");
    http_send ("<a href=\"/network\">Network</a>&nbsp;&nbsp;\r\n");
    http_send ("<a href=\"/display\">Display</a>&nbsp;&nbsp;\r\n");
    http_send ("<P>\r\n");
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * main page
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
http_main (LISTENER_DATA * ld)
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

    http_header ("WordClock");

    http_menu ();

    table_header ("Device", "Value", "");
    table_row ("RTC temperature", rtc_temp);
    table_row ("DS18xx", ds18xx_temp);
    table_row ("EEPROM", eeprom_is_up ? "online" : "offline");
    table_trailer ();

    http_send ("<form method=\"GET\" action=\"/\">\r\n");
    http_send ("<button type=\"submit\" name=\"action\" value=\"poweron\">Power On</button>\r\n");
    http_send ("<button type=\"submit\" name=\"action\" value=\"poweroff\">Power Off</button>\r\n");
    http_send ("<button type=\"submit\" name=\"action\" value=\"eepromdump\">EEPROM dump</button>\r\n");
    http_send ("</form>\r\n");

    http_send ("<form method=\"GET\" action=\"/\">\r\n");
    http_send ("<button type=\"submit\" name=\"action\" value=\"displaytemperature\">Display temperature</button>\r\n");
    http_send ("<button type=\"submit\" name=\"action\" value=\"learnir\">Learn IR remote control</button>\r\n");
    http_send ("</form>\r\n");

    if (message)
    {
        http_send ("<P>\r\n<font color=green>");
        http_send (message);
        http_send ("</font>\r\n");
    }

    if (! strcmp (action, "eepromdump"))
    {
        http_eeprom_dump ();
    }

    http_trailer ();
    http_flush ();

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * network page
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
http_network (void)
{
    char *                      action;
    char *                      message         = (char *) 0;
    ESP8266_INFO *   infop;
    char *                      esp_firmware_version;
    char *                      ids[2]          = { "ap", "pw" };
    char *                      desc[2]         = { "AP", "Key" };
    int                         maxlen[2]       = { MAX_AP_LEN, MAX_PW_LEN };
    int                         maxsize[2]      = { 15, 20 };
    char *                      col2[2];
    char                        timezone_str[MAX_TIMEZONE_LEN+1];
    uint_fast8_t                rtc             = 0;

    infop                   = esp8266_get_info ();
    esp_firmware_version    = infop->firmware;

    col2[0] = infop->accesspoint;
    col2[1] = "";

    action = http_get_param ("action");

    if (action)
    {
        if (! strcmp (action, "saveaccesspoint"))
        {
            extern uint_fast8_t esp8266_is_online;                          // see main.c

            char * ssid = http_get_param ("ap");
            char * key  = http_get_param ("pw");
            http_header ("WordClock Network");
            http_send ("<B>Connecting to Access Point, try again later.</B>");
            http_trailer ();
            http_flush ();
            delay_msec (100);

            esp8266_is_online = 0;
            infop->is_online = 0;
            infop->ipaddress[0] = '\0';

            esp8266_connect_to_access_point (ssid, key);
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
            int_fast16_t tz = atoi (http_get_param ("timezone"));

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

    sprintf (timezone_str, "%d", timeserver_get_timezone());

    http_header ("WordClock Network");
    http_menu ();

    table_header ("Device", "Value", "Action");
    table_row ("IP address", infop->ipaddress);
    table_row ("ESP8266 firmware", esp_firmware_version);
    table_row_inputs ("network", "Access Point", "accesspoint", 2, ids, desc, col2, maxlen, maxsize);
    table_row_input ("network", "Time server", "timeserver", (char *) timeserver_get_timeserver (), MAX_IP_LEN);
    table_row_input ("network", "Time zone (GMT +)", "timezone", timezone_str, MAX_TIMEZONE_LEN);
    table_trailer ();

    http_send ("<form method=\"GET\" action=\"/network\">\r\n");
    http_send ("<button type=\"submit\" name=\"action\" value=\"nettime\">Get net time</button>\r\n");
    http_send ("</form>\r\n");

    if (message)
    {
        http_send ("<P>\r\n<font color=green>");
        http_send (message);
        http_send ("</font>\r\n");
    }

    http_trailer ();
    http_flush ();

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * display page
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
http_display (LISTENER_DATA * ld)
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

    use_ldr                 = ldr_get_ldr_status ();
    display_mode            = display_get_display_mode ();
    animation_mode          = display_get_animation_mode ();
    brightness              = display_get_brightness ();
    auto_brightness_active  = display_get_automatic_brightness_control ();
    display_get_colors (&rgb);

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
            brightness = atoi (http_get_param ("brightness"));
            ld->brightness = brightness;
            rtc = LISTENER_SET_BRIGHTNESS_CODE;
        }
        else if (! strcmp (action, "savecolors"))
        {
            rgb.red     = atoi (http_get_param ("red"));
            rgb.green   = atoi (http_get_param ("green"));
            rgb.blue    = atoi (http_get_param ("blue"));
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
            display_mode = atoi (http_get_param ("displaymode"));
            ld->mode = display_mode;
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

    sprintf (brbuf,     "%d", brightness);

    sprintf (red_buf,   "%d", rgb.red);
    sprintf (green_buf, "%d", rgb.green);
    sprintf (blue_buf,  "%d", rgb.blue);

    rgb_buf[0] = red_buf;
    rgb_buf[1] = green_buf;
    rgb_buf[2] = blue_buf;

    http_header ("WordClock Display");
    http_menu ();

    table_header ("Device", "Value", "Action");

    table_row_select ("display", "Animation", "animation", animation_modes, animation_mode, ANIMATION_MODES);
    table_row_select ("display", "Display Mode", "displaymode", tbl_mode_names, display_mode, MODES_COUNT);
    table_row_checkbox ("display", "LDR", "ldrconnected", "LDR connected", use_ldr);

    if (use_ldr)
    {
        table_row ("LDR", ldr_buf);
        table_row_checkbox ("display", "LDR", "auto", "Automatic brightness", auto_brightness_active);
    }

    if (! use_ldr || ! auto_brightness_active)
    {
        table_row_slider ("display", "Brightness (1-15)", "brightness", brbuf, "0", "15");
    }

    table_row_sliders ("display", "Colors", "colors", 3, ids, desc, rgb_buf, minval, maxval);
    table_trailer ();

    http_send ("<form method=\"GET\" action=\"/display\">\r\n");
    http_send ("<button type=\"submit\" name=\"action\" value=\"poweron\">Power On</button>\r\n");
    http_send ("<button type=\"submit\" name=\"action\" value=\"poweroff\">Power Off</button>\r\n");
    http_send ("<button type=\"submit\" name=\"action\" value=\"testdisplay\">Test display</button>\r\n");
    http_send ("<button type=\"submit\" name=\"action\" value=\"saveall\">Save all</button>\r\n");
    http_send ("</form>\r\n");

    if (message)
    {
        http_send ("<P>\r\n<font color=green>");
        http_send (message);
        http_send ("</font>\r\n");
    }

    http_trailer ();
    http_flush ();

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * hex to integer
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint16_t
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
http_server (char * path, char * param, LISTENER_DATA * ld)
{
    char *      p;
    char *      s;
    char *      t;
    int         rtc = 0;

    log_printf ("http path: '%s'\r\n", path);

    for (p = param; *p; p++)
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

    log_printf ("http parameters: '%s'\r\n", param);

    if (param)
    {
        http_set_params (param);
    }
    else
    {
        http_set_params ("");
    }

    if (! strcmp (path, "/"))
    {
        rtc = http_main (ld);
    }
    else if (! strcmp (path, "/network"))
    {
        rtc = http_network ();
    }
    else if (! strcmp (path, "/display"))
    {
        rtc = http_display (ld);
    }
    else
    {
        char * p = "HTTP/1.0 404 Not Found";
        http_send (p);
        http_send ("\r\n\r\n");
        http_send ("404 Not Found\r\n");
        http_flush ();
    }

    return rtc;
}
