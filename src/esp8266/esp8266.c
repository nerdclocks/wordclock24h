/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * esp8266.c - ESP8266 WLAN routines
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 *    +-------------------------+---------------------------+---------------------------+
 *    | Device                  | STM32F4 Discovery         | STM32F4x1 Nucleo          |
 *    +-------------------------+---------------------------+---------------------------+
 *    | ESP8266 USART           | USART2: TX=PA2  RX=PA3    | USART6: TX=PA11 RX=PA12   |
 *    | ESP8266 GPIO            | GPIO:   RST=PC5 CH_PD=PC4 | GPIO:   RST=PA7 CH_PD=PA6 |
 *    +-------------------------+---------------------------+---------------------------+
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
#include "esp8266.h"
#include "mcurses.h"
#include "monitor.h"
#include "uart.h"

/*--------------------------------------------------------------------------------------------------------------------------------------
 * globals:
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t                    esp8266_is_up       = 0;
uint_fast8_t                    esp8266_is_online   = 0;

static time_t                   curtime;
static volatile uint_fast8_t    ten_ms_tick;

#if defined (STM32F407VG)                                       // STM32F4 Discovery Board PD12

#define ESP8266_RST_PERIPH_CLOCK_CMD    RCC_AHB1PeriphClockCmd
#define ESP8266_RST_PERIPH              RCC_AHB1Periph_GPIOC
#define ESP8266_RST_PORT                GPIOC
#define ESP8266_RST_PIN                 GPIO_Pin_5

#define ESP8266_CH_PD_PERIPH_CLOCK_CMD  RCC_AHB1PeriphClockCmd
#define ESP8266_CH_PD_PERIPH            RCC_AHB1Periph_GPIOC
#define ESP8266_CH_PD_PORT              GPIOC
#define ESP8266_CH_PD_PIN               GPIO_Pin_4

#elif defined (STM32F401RE) || defined (STM32F411RE)            // STM32F401/STM32F411 Nucleo Board

#define ESP8266_RST_PERIPH_CLOCK_CMD    RCC_AHB1PeriphClockCmd
#define ESP8266_RST_PERIPH              RCC_AHB1Periph_GPIOA
#define ESP8266_RST_PORT                GPIOA
#define ESP8266_RST_PIN                 GPIO_Pin_7

#define ESP8266_CH_PD_PERIPH_CLOCK_CMD  RCC_AHB1PeriphClockCmd
#define ESP8266_CH_PD_PERIPH            RCC_AHB1Periph_GPIOA
#define ESP8266_CH_PD_PORT              GPIOA
#define ESP8266_CH_PD_PIN               GPIO_Pin_6

#else
#error STM32 unknown
#endif

static void
esp8266_gpio_init (void)
{
    GPIO_InitTypeDef gpio;

    ESP8266_RST_PERIPH_CLOCK_CMD (ESP8266_RST_PERIPH, ENABLE);      // enable clock for ESP8266 RST

    gpio.GPIO_Pin   = ESP8266_RST_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(ESP8266_RST_PORT, &gpio);

    ESP8266_RST_PERIPH_CLOCK_CMD (ESP8266_CH_PD_PERIPH, ENABLE);    // enable clock for ESP8266 CH_PD

    gpio.GPIO_Pin   = ESP8266_CH_PD_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(ESP8266_CH_PD_PORT, &gpio);

    GPIO_WriteBit(ESP8266_RST_PORT, ESP8266_RST_PIN, SET);
    GPIO_WriteBit(ESP8266_CH_PD_PORT, ESP8266_CH_PD_PIN, SET);
}
/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: poll ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
esp8266_poll (uint8_t * chp, uint_fast16_t ten_ms)
{
    uint_fast16_t  cnt = 0;

    while (1)
    {
        if (uart_poll (chp))
        {
            return 1;
        }

        if (ten_ms_tick)
        {
            ten_ms_tick = 0;

            cnt++;

            if (cnt >= ten_ms)
            {
                break;
            }
        }
    }
    return 0;
}


#define ESP8266_MAX_CHANNELS     5
#define ESP8266_MAX_BUF_LEN     32
#define ESP8266_MAX_DATA_LEN    32

/*--------------------------------------------------------------------------------------------------------------------------------------
 * exp8266_listen ()
 *
 * Return values:
 *   0: no data
 *   1: data received
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_listen (ESP8266_LISTEN_DATA * lp)
{
    static uint8_t      buf[ESP8266_MAX_BUF_LEN];
    static uint8_t      data[ESP8266_MAX_DATA_LEN];
    static uint_fast8_t bufpos = 0;
    static uint_fast8_t indata_channel = 0xff;
    static uint_fast8_t indata_idx;
    static uint_fast8_t indata_len;

    uint8_t             ch;
    uint_fast8_t        rtc = 0;

    if (uart_poll (&ch))
    {
        if (indata_channel == 0xff)
        {
            if (bufpos < ESP8266_MAX_BUF_LEN - 1)
            {
                if (ch != '\r')
                {
                    if (ch == ':')
                    {
                        buf[bufpos] = '\0';

                        if (!strncmp ((char *) buf, "+IPD,", 5))
                        {
                            uint_fast8_t channel = buf[5] - '0';

                            if (buf[6] == ',' && channel < ESP8266_MAX_CHANNELS)
                            {
                                indata_channel = channel;
                                indata_idx = 0;
                                indata_len = atoi ((char *) buf + 7);
                            }
                        }
                    }
                    else
                    {
                        if (ch == '\n')
                        {
                            buf[bufpos] = '\0';
                            bufpos = 0;
                        }
                        else
                        {
                            buf[bufpos] = ch;
                            bufpos++;
                        }
                    }
                }
            }
        }
        else
        {
            if (indata_idx < ESP8266_MAX_DATA_LEN)
            {
                data[indata_idx] = ch;
            }

            indata_idx++;

            if (indata_idx == indata_len)
            {
                rtc = 1;

                lp->channel = indata_channel;
                lp->data    = data;
                lp->length  = (indata_len < ESP8266_MAX_DATA_LEN) ? indata_len : ESP8266_MAX_DATA_LEN;
                indata_channel = 0xff;
            }
        }
    }
    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: get answer from ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_get_answer (char * answer, uint_fast8_t max_len, uint_fast8_t line, uint_fast16_t timeout_cnt)
{
    static char     esp_answer[ESP8266_MAX_ANSWER_LEN + 1];
    uint_fast8_t    in_data = 0;
    uint_fast8_t    l = 0;
    uint8_t         ch;
    uint_fast8_t    x = 0;

    if (! answer)
    {
        answer = esp_answer;
    }

    if (mcurses_is_up)
    {
        move (line, 0);
        clrtoeol();
        refresh ();
    }

    uart_flush ();

    while (1)
    {
        if (esp8266_poll (&ch, timeout_cnt) == 0)
        {
            if (mcurses_is_up)
            {
                mvaddstr (ESP_MSG_LINE, ESP_MSG_COL, "Timeout!");
            }
            return ESP8266_TIMEOUT;
        }

        if (mcurses_is_up)
        {
            if (ch >= 32 && ch < 127)
            {
                addch (ch);
                x++;
            }
            else
            {
                printw ("<%02x>", ch);
                x += 4;
            }
            refresh ();

            if (x >= 75)
            {
                move (line, 0);
                clrtoeol ();
                refresh ();
            }
        }

        if (l < max_len - 1 && (in_data || (ch != '\r' && ch != '\n')))
        {
            answer[l++] = ch;

            if (in_data)
            {
                in_data--;
            }
            else if (l == 9 && ! strncmp ((char *) answer, "+IPD,1,4:", 9))
            {
                in_data = 4;
            }
        }
        else if (ch == '\n')
        {
            answer[l] = '\0';

            if (l > 0)
            {
                if (! strncmp ((char *) answer, "+IPD,1,4:", 9))
                {
                    unsigned char * p = (unsigned char *) answer + 9;
                    curtime =   ((unsigned long) p[3] <<  0) |
                                ((unsigned long) p[2] <<  8) |
                                ((unsigned long) p[1] << 16) |
                                ((unsigned long) p[0] << 24);
                    if (mcurses_is_up)
                    {
                        mvaddstr (ESP_MSG_LINE, ESP_MSG_COL, "IPD Timeserver!");
                        clrtoeol();
                    }
                    return ESP8266_IPD;
                }
                else if (! stricmp (answer, "linked"))
                {
                    if (mcurses_is_up)
                    {
                        mvaddstr (ESP_MSG_LINE, ESP_MSG_COL, "LINKED!");
                        clrtoeol();
                    }
                    return ESP8266_LINKED;
                }
                else if (! stricmp (answer, "unlink"))
                {
                    if (mcurses_is_up)
                    {
                        mvaddstr (ESP_MSG_LINE, ESP_MSG_COL, "UNLINK!");
                        clrtoeol();
                    }
                    return ESP8266_UNLINK;
                }
                else if (! stricmp (answer, "ok"))
                {
                    if (mcurses_is_up)
                    {
                        mvaddstr (ESP_MSG_LINE, ESP_MSG_COL, "OK");
                        clrtoeol();
                    }
                    return ESP8266_OK;
                }
                else if (! stricmp (answer, "error"))
                {
                    if (mcurses_is_up)
                    {
                        mvaddstr (ESP_MSG_LINE, ESP_MSG_COL, "ERROR!");
                        clrtoeol();
                    }
                    return ESP8266_ERROR;
                }
                else if (! strnicmp (answer, "busy", 4))
                {
                    if (mcurses_is_up)
                    {
                        mvaddstr (ESP_MSG_LINE, ESP_MSG_COL, "BUSY!");
                        clrtoeol();
                    }
                    return ESP8266_BUSY;
                }
                else if (! stricmp (answer, "ready"))
                {
                    if (mcurses_is_up)
                    {
                        mvaddstr (ESP_MSG_LINE, ESP_MSG_COL, "READY!");
                        clrtoeol();
                    }
                    return ESP8266_READY;
                }
                else if (! stricmp ((char *) answer, "no change"))
                {
                    if (mcurses_is_up)
                    {
                        mvaddstr (ESP_MSG_LINE, ESP_MSG_COL, "NO CHANGE!");
                        clrtoeol();
                    }
                    return ESP8266_NO_CHANGE;
                }
                else
                {
                    if (mcurses_is_up)
                    {
                        mvaddstr (ESP_MSG_LINE, ESP_MSG_COL, "UNKNOWN!");
                        clrtoeol();
                    }
                    return ESP8266_UNKNOWN;
                }
            }
            else
            {
                move (line, 0);
                clrtoeol ();
                refresh ();
            }

        }
    }
    return 1;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: send a command to ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_send_cmd (char * cmd)
{
    uint8_t         ch;
    uint_fast8_t    length;
    uint_fast8_t    i;

    length = strlen (cmd);

    if (mcurses_is_up)
    {
        move (ESP_MSG_LINE, ESP_MSG_COL);
        clrtoeol ();

        move (ESP_SND_LINE, ESP_SND_COL);
        clrtoeol ();

        addstr ("Sending: ");

        for (i = 0; i < length; i++)
        {
            ch = cmd[i];

            if (ch >= 32 && ch < 127)
            {
                addch (ch);
            }
            else
            {
                printw ("<%02x>", ch);
            }
        }
        refresh ();
    }

    uart_flush();

    while (esp8266_poll (&ch, 20))                      // eat characters from input
    {
        ;
    }

    if (mcurses_is_up)
    {
        move (LOG_LINE, LOG_COL);
        clrtoeol();
        refresh ();
    }

    for (i = 0; i < length; i++)
    {
        uart_putc (cmd[i]);
        uart_flush ();

        do
        {
            if (! esp8266_poll (&ch, 20))                   // read echo
            {
                if (mcurses_is_up)
                {
                    mvaddstr (ESP_MSG_LINE, ESP_MSG_COL, "NO ECHO!");
                    clrtoeol();
                }
                return 0;
            }
        } while (ch == 0xd0);

        if (mcurses_is_up)
        {
            if (ch >= 32 && ch < 127)
            {
                addch (ch);
            }
            else
            {
                printw ("<%02x>", ch);
            }
            refresh ();
        }

        if (ch != cmd[i])
        {
            if (mcurses_is_up)
            {
                mvprintw (ESP_MSG_LINE, ESP_MSG_COL, "wrong echo, got '%c' <%02x>, expected: '%c' <%02x>", ch, ch, cmd[i], cmd[i]);
                clrtoeol();
                refresh ();
            }
            return 0;
        }
    }

    return 1;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: wait some time (in ticks of 10 msec)
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static void
esp8266_wait (uint_fast16_t ten_ms)
{
    uint_fast16_t  cnt = 0;

    if (ten_ms < 2)
    {
        ten_ms = 2;                                             // it is nonsense to wait for next tick, it could arrive very soon
    }

    while (1)
    {
        if (ten_ms_tick)
        {
            ten_ms_tick = 0;

            cnt++;

            if (cnt >= ten_ms)
            {
                break;
            }
        }
    }
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * reset ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_reset (void)
{
    uint_fast8_t    rtc;

    GPIO_WriteBit(ESP8266_RST_PORT, ESP8266_RST_PIN, RESET);
    esp8266_wait (1);
    GPIO_WriteBit(ESP8266_RST_PORT, ESP8266_RST_PIN, SET);

    while ((rtc = esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 500)) != ESP8266_TIMEOUT && rtc != ESP8266_READY)
    {
        ;
    }

    if (rtc == ESP8266_READY)
    {
        rtc = 1;
    }
    else
    {
        rtc = 0;
    }
    return rtc;
}

void
esp8266_powerdown (void)
{
    GPIO_WriteBit(ESP8266_CH_PD_PORT, ESP8266_RST_PIN, RESET);
}

void
esp8266_powerup (void)
{
    GPIO_WriteBit(ESP8266_CH_PD_PORT, ESP8266_RST_PIN, SET);
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: get SSID of connected access point
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static char *
esp8266_get_access_point_connected (void)
{
    static char     esp_answer[ESP8266_MAX_ANSWER_LEN + 1];
    char *          p;
    uint_fast8_t    len;

    esp8266_send_cmd("AT+CWJAP?\r");

    if (esp8266_get_answer (esp_answer, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 100) != ESP8266_TIMEOUT)
    {
        if (esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 100) == ESP8266_OK)
        {
            if (! strncmp (esp_answer, "+CWJAP:\"", 8))
            {
                p = esp_answer + 8;
                len = strlen (p);

                if (len > 1)                                            // more than one char?
                {
                    p[len - 1] = '\0';                                  // strip double quote
                    return p;
                }
            }
        }
    }
    return (char *) NULL;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: get IP address of ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static char *
esp8266_get_ip_address (void)
{
    static char     esp_answer[ESP8266_MAX_ANSWER_LEN + 1];

    esp8266_send_cmd("AT+CIFSR\r");

    if (esp8266_get_answer (esp_answer, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 100) == ESP8266_UNKNOWN)
    {
        return esp_answer;
    }
    return (char *) NULL;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: set MUX off
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
esp8266_set_mux_on_off (uint_fast8_t on)
{
    char *          cmd;
    uint_fast8_t    rtc = 0;

    if (on)
    {
        cmd = "AT+CIPMUX=1\r";
    }
    else
    {
        cmd = "AT+CIPMUX=0\r";
    }


    if (esp8266_send_cmd(cmd) &&
        esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 100) != ESP8266_TIMEOUT)
    {
        rtc = 1;
    }
    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: close any connection
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
esp8266_close_connection (void)
{
    uint_fast8_t    rtc = 0;

    if (esp8266_send_cmd("AT+CIPCLOSE\r") &&
        esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 100) != ESP8266_TIMEOUT)
    {
        rtc = 1;
    }
    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: start server listening on port
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
esp8266_server (uint_fast16_t port)
{
    static char     cmd[32];
    uint_fast8_t    rtc = 0;

    sprintf (cmd, "AT+CIPSERVER=1,%d\r", port);

    if (esp8266_send_cmd(cmd) &&
        esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 100) != ESP8266_TIMEOUT)
    {
        rtc = 1;
    }
    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: connect to access point
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_connect_to_access_point (char * ssid, char * key)
{
    static char     cmd[ESP8266_MAX_CMD_LEN];
    uint_fast8_t    rtc = 0;

    if (esp8266_send_cmd("AT+CWMODE=1\r"))
    {
        if (esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 200) != ESP8266_TIMEOUT)
        {
            strcpy (cmd, "AT+CWJAP=\"");
            strcat (cmd, ssid);
            strcat (cmd, "\",\"");
            strcat (cmd, key);
            strcat (cmd, "\"\r");

            if (esp8266_send_cmd(cmd))
            {
                while (esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 500) != ESP8266_TIMEOUT)     // wait min 30 seconds for connect
                {
                    ;
                }
                rtc = 1;
            }
        }
    }
    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: disconnect from access point
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_disconnect_from_access_point (void)
{
    uint_fast8_t    rtc = 0;

    if (esp8266_send_cmd("AT+CWQAP\r"))
    {
        while (esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 500) != ESP8266_TIMEOUT)
        {
            // wait min. 30 seconds for disconnect
        }
        rtc = 1;
    }
    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * check online status
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_check_online_status (ESP8266_CONNECTION_INFO * infop)
{
    char *      accesspoint;
    char *      ipaddress;

    if ((accesspoint = esp8266_get_access_point_connected ()) != (char *) NULL &&
        (ipaddress = esp8266_get_ip_address ()) != (char *) NULL &&
        esp8266_set_mux_on_off (1) &&
        esp8266_close_connection () &&
        esp8266_server (2424))
    {
        infop->accesspoint  = accesspoint;
        infop->ipaddress    = ipaddress;
        esp8266_is_online = 1;
    }
    else
    {
        esp8266_is_online = 0;
    }

    return esp8266_is_online;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * check if ESP8266 is up
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
esp8266_check_up_status ()
{
    uint_fast8_t    rtc = 0;

    if (esp8266_send_cmd("AT\r"))
    {
        if (esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 100) == ESP8266_OK)
        {
            rtc = 1;
        }
    }

    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * initialize UART and ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_init (void)
{
    static uint_fast8_t already_called;
    uint_fast8_t        rtc;

    if (! already_called)
    {
        already_called = 1;

        uart_init (115200);
        esp8266_gpio_init ();

        while ((rtc = esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 100)) != ESP8266_TIMEOUT && rtc != ESP8266_READY)
        {
            ;
        }

        esp8266_is_up = esp8266_check_up_status ();
    }

    return esp8266_is_up;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * get time from time server via TCP (see RFC 868)
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_get_time (char * timeserver, time_t * curtime_p)
{
    static char     buf[64];
    uint_fast8_t    rtc = 0;

    strcpy (buf, "AT+CIPSTART=1,\"TCP\",\"");
    strcat (buf, timeserver);
    strcat (buf, "\",37\r");

    if (esp8266_send_cmd(buf))
    {
        if (esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 300) == ESP8266_OK &&
            esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 300) == ESP8266_LINKED &&
            esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 300) == ESP8266_IPD &&
            esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 300) == ESP8266_OK &&
            esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 300) == ESP8266_OK &&
            esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, LOG_LINE, 300) == ESP8266_UNLINK)
        {
            *curtime_p = curtime;
            rtc = 1;
        }
    }
	return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * this function should be called every 10 msec, see IRQ in main.c
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
esp8266_ISR (void)
{
    ten_ms_tick = 1;
}
