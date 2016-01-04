/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * esp8266.c - ESP8266 WLAN routines
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
 *
 * Following ESP8266 firmware versions have been successfully checked - received by AT+GMR:
 *
 *    00150900
 *    00170901
 *    0018000902-AI03
 *    0020000903
 *    AT version:0.21.0.0 SDK version:0.9.5
 *    AT version:0.50.0.0 SDK version:1.4.0
 *    AT version:0.51.0.0 SDK version:1.5.0
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include "esp8266.h"
#include "esp8266-config.h"
#include "monitor.h"

#define UART_PREFIX         esp8266
#include "uart.h"

#define NTP_PACKET_SIZE     48
#define LISTEN_PORT         2525

#define TIMEOUT_MSEC(x)     (x/10)
#define TIMEOUT
/*--------------------------------------------------------------------------------------------------------------------------------------
 * globals:
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t                            esp8266_is_up       = 0;
uint_fast8_t                            esp8266_is_online   = 0;
volatile uint_fast8_t                   esp8266_ten_ms_tick;            // should be set every 10 msec to 1, see IRQ in main.c

#define ESP8266_MAX_F_VERSION_LEN       40
static char                             firmware_version[ESP8266_MAX_F_VERSION_LEN + 1];

#if defined (STM32F407VG)                                               // STM32F4 Discovery Board PD12

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

#elif defined (STM32F103)

#define ESP8266_RST_PERIPH_CLOCK_CMD    RCC_APB2PeriphClockCmd
#define ESP8266_RST_PERIPH              RCC_APB2Periph_GPIOA
#define ESP8266_RST_PORT                GPIOA
#define ESP8266_RST_PIN                 GPIO_Pin_0

#define ESP8266_CH_PD_PERIPH_CLOCK_CMD  RCC_APB2PeriphClockCmd
#define ESP8266_CH_PD_PERIPH            RCC_APB2Periph_GPIOA
#define ESP8266_CH_PD_PORT              GPIOA
#define ESP8266_CH_PD_PIN               GPIO_Pin_1

#else
#error STM32 unknown
#endif

#if ESP8266_DEBUG == 1
#undef UART_PREFIX
#define UART_PREFIX     log
#include "uart.h"
#define log_init(b)     do { log_uart_init (b); } while (0)
#define log_putc(c)     do { log_uart_putc (c); } while (0)
#define log_puts(s)     do { log_uart_puts (s); } while (0)
#define log_message(m)  do { log_uart_puts (" ("); log_uart_puts (m); log_uart_puts (")\r\n"); } while (0)
#define log_hex(b)      do { char log_buf[5]; sprintf (log_buf, "<%02x>", b); log_uart_puts (log_buf); } while (0)
#define log_flush()     do { log_uart_flush (); } while (0)
#else
#define log_init(b)
#define log_putc(c)
#define log_puts(s)
#define log_message(m)
#define log_hex(b)
#define log_flush()
#endif

static void
esp8266_gpio_init (void)
{
    GPIO_InitTypeDef gpio;

    GPIO_StructInit (&gpio);

    ESP8266_RST_PERIPH_CLOCK_CMD (ESP8266_RST_PERIPH, ENABLE);      // enable clock for ESP8266 RST

    gpio.GPIO_Pin   = ESP8266_RST_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;

#if defined (STM32F10X)
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
#elif defined (STM32F4XX)
    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
#endif

    GPIO_Init(ESP8266_RST_PORT, &gpio);

    ESP8266_RST_PERIPH_CLOCK_CMD (ESP8266_CH_PD_PERIPH, ENABLE);    // enable clock for ESP8266 CH_PD

    gpio.GPIO_Pin   = ESP8266_CH_PD_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;

#if defined (STM32F10X)
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
#elif defined (STM32F4XX)
    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
#endif

    GPIO_Init(ESP8266_CH_PD_PORT, &gpio);

    GPIO_WriteBit(ESP8266_RST_PORT, ESP8266_RST_PIN, SET);
    GPIO_WriteBit(ESP8266_CH_PD_PORT, ESP8266_CH_PD_PIN, SET);
}
/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: poll ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
esp8266_poll (uint_fast8_t * chp, uint_fast16_t ten_ms)
{
    uint_fast16_t  cnt = 0;

    while (1)
    {
        if (esp8266_uart_poll (chp))
        {
            return 1;
        }

        if (esp8266_ten_ms_tick)
        {
            esp8266_ten_ms_tick = 0;

            cnt++;

            if (cnt >= ten_ms)
            {
                break;
            }
        }
    }
    return 0;
}

static unsigned char    data_buf[NTP_PACKET_SIZE];
static uint_fast8_t     data_buf_len;

/*--------------------------------------------------------------------------------------------------------------------------------------
 * get answer from ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_get_answer (char * answer, uint_fast8_t max_len, uint_fast16_t timeout_ten_ms)
{
    static char     esp_answer[ESP8266_MAX_ANSWER_LEN + 1];
    uint_fast8_t    in_data = 0;
    uint_fast8_t    l = 0;
    uint_fast8_t    ch;

    if (! answer)
    {
        answer = esp_answer;
    }

    log_flush ();

    esp8266_uart_flush ();

    while (1)
    {
        if (esp8266_poll (&ch, timeout_ten_ms) == 0)
        {
            log_message ("Timeout");
            return ESP8266_TIMEOUT;
        }

        if (ch >= 32 && ch < 127)
        {
            log_putc (ch);
        }
        else
        {
            if (l > 0)
            {
                log_hex (ch);
            }
        }

        log_flush ();

        if (l < max_len - 1 && (in_data || (ch != '\r' && ch != '\n')))
        {
            answer[l++] = ch;

            if (in_data)
            {
                in_data--;

                data_buf[data_buf_len++] = ch;

                if (! in_data)
                {
                    log_message ("ESP8266_IPD");
                    return ESP8266_IPD;
                }
            }
            else if (l == 9 && ! strncmp ((char *) answer, "+IPD,1,4:", 9))
            {
                in_data = 4;
                data_buf_len = 0;
            }
            else if (l == 10 && ! strncmp ((char *) answer, "+IPD,1,48:", 10))
            {
                in_data = 48;
                data_buf_len = 0;
            }
        }
        else if (ch == '\n' || ch == '\r')      // fm 2015-10-22: sometimes ESP answers with \r\n, sometimes only with \r
        {
            answer[l] = '\0';

            if (l > 0)
            {
                if (! stricmp (answer, "LINKED"))
                {
                    log_message ("ESP8266_LINKED");
                    return ESP8266_LINKED;
                }
                else if (! stricmp (answer, "UNLINK"))
                {
                    log_message ("ESP8266_UNLINK");
                    return ESP8266_UNLINK;
                }
                else if (l > 1 && ! stricmp (answer + 1, ",CONNECT"))      // firmware AT version:0.21.version:0.9.5: 1,CONNECT instead of LINKED
                {
                    log_message ("ESP8266_CONNECT");
                    return ESP8266_CONNECT;
                }
                else if (l > 1 && ! stricmp (answer + 1, ",CLOSED"))        // firmware AT version:0.21.version:0.9.5: 1,CLOSED instead of UNLINK
                {
                    log_message ("ESP8266_CLOSED");
                    return ESP8266_CLOSED;
                }
                else if (! stricmp (answer, "OK"))
                {
                    log_message ("ESP8266_OK");
                    return ESP8266_OK;
                }
                else if (! stricmp (answer, "SEND OK"))
                {
                    log_message ("ESP8266_SEND_OK");
                    return ESP8266_SEND_OK;
                }
                else if (! stricmp (answer, "ERROR"))
                {
                    log_message ("ESP8266_ERROR");
                    return ESP8266_ERROR;
                }
                else if (! stricmp (answer, "WIFI CONNECTED"))
                {
                    log_message ("ESP8266_WIFI_CONNECTED");
                    return ESP8266_WIFI_CONNECTED;
                }
                else if (! stricmp (answer, "WIFI GOT IP"))
                {
                    log_message ("ESP8266_WIFI_GOT_IP");
                    return ESP8266_WIFI_GOT_IP;
                }
                else if (! strnicmp (answer, "BUSY", 4))
                {
                    log_message ("ESP8266_BUSY");
                    // esp8266_reset ();                                   // if we get busy, we must reset
                    return ESP8266_BUSY;
                }
                else if (! strnicmp (answer, "ALREAY CONNECT", 14) || ! strnicmp (answer, "ALREADY CONNECT", 15))
                {
                    log_message ("ESP8266_ALREADY_CONNECT");
                    // esp8266_reset ();                                   // old buggy software: if we are already connected, we must reset
                    return ESP8266_ALREADY_CONNECT;
                }
                else if (! strnicmp (answer, "Link typ ERROR", 14))
                {
                    log_message ("ESP8266_LINK_TYPE_ERROR");
                    // esp8266_reset ();                                   // old buggy software: if we are already connected, we must reset
                    return ESP8266_LINK_TYPE_ERROR;
                }
                else if (! stricmp (answer, "READY"))
                {
                    log_message ("ESP8266_READY");
                    return ESP8266_READY;
                }
                else if (! stricmp ((char *) answer, "NO CHANGE"))
                {
                    log_message ("ESP8266_NO_CHANGE");
                    return ESP8266_NO_CHANGE;
                }
                else if (! strnicmp (answer, "+CWJAP:", 7))
                {
                    log_message ("ESP8266_ACCESS_POINT");
                    return ESP8266_ACCESS_POINT;
                }
                else if (answer[0] == '+')
                {
                    log_message ("ESP8266_ANSWER");
                    return ESP8266_ANSWER;
                }
                else
                {
                    log_message ("ESP8266_UNSPECIFIED");
                    return ESP8266_UNSPECIFIED;
                }
            }
            else // l == 0
            {
                log_flush ();
            }
        }
    }
    return 1;
}

static uint_fast8_t esp8266_needs_crlf = 1;

/*--------------------------------------------------------------------------------------------------------------------------------------
 * send a command to ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_send_cmd (char * cmd)
{
    uint8_t         send_ch;
    uint_fast8_t    ch;
    uint_fast8_t    length;
    uint_fast8_t    i;

    length = strlen (cmd);

    log_puts ("--> ");

    for (i = 0; i < length; i++)
    {
        ch = cmd[i];

        if (ch >= 32 && ch < 127)
        {
            log_putc (ch);
        }
        else
        {
            log_hex (ch);
        }
    }

    log_puts ("<0d>");

    if (esp8266_needs_crlf)
    {
        log_puts ("<0a>");
    }

    log_puts ("\r\n");
    log_flush ();

    esp8266_uart_flush();

    while (esp8266_poll (&ch, 20))                      // eat characters from input
    {
        ;
    }

    for (i = 0; i < length + 1 + (esp8266_needs_crlf ? 1 : 0); i++)
    {
        if (i < length)
        {
            send_ch = cmd[i];
        }
        else if (i == length)
        {
            send_ch = '\r';
        }
        else
        {
            send_ch = '\n';
        }

        esp8266_uart_putc (send_ch);
        esp8266_uart_flush ();

        do
        {
            if (! esp8266_poll (&ch, 10))                   // read echo
            {
                log_puts ("NO ECHO!\r\n");
                return 0;
            }
        } while (ch == 0x00);

        if (ch == 0x0d && send_ch == 0x0a)      // old ESP8266 (FW 00150900) sends \r\r instead of \r\n or \r alone
        {
            ch = 0x0a;
        }

        if (ch != send_ch)
        {
            log_puts ("\r\n");
            log_puts ("wrong echo, got ");
            log_hex (ch);
            log_puts ("expected: ");
            log_hex (send_ch);
            log_puts ("\r\n");
            log_flush ();

            return 0;
        }
    }

    return 1;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * send data to ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
esp8266_send_data (unsigned char * data, uint_fast8_t len)
{
    char buf[64];
    uint_fast8_t    ch = '\0';
    uint_fast8_t    rtc = 1;

    sprintf (buf, "AT+CIPSEND=1,%d", len);

    if (esp8266_send_cmd (buf))
    {
        while (esp8266_poll (&ch, 20))                                  // wait for prompt "> "
        {
            if (ch == ' ')
            {
                break;
            }
        }

        if (ch == ' ')
        {
            while (len)
            {
                esp8266_uart_putc (*data);
                len--;
                data++;
            }
            esp8266_uart_flush ();
        }
    }

    return rtc;
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
        if (esp8266_ten_ms_tick)
        {
            esp8266_ten_ms_tick = 0;

            cnt++;

            if (cnt >= ten_ms)
            {
                break;
            }
        }
    }
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
    char *          pp;

    esp8266_send_cmd("AT+CWJAP?");

    if (esp8266_get_answer (esp_answer, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100)) == ESP8266_ACCESS_POINT)
    {
        esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100));  // some ESP answer with 2nd line: OK

        p = esp_answer + 7;                                     // skip "+CWJAP:"

        if (*p == '"')                                          // skip following double quote, too
        {
            p++;

            for (pp = p; *pp; pp++)                             // search next double quote and strip string
            {
                if (*pp == '"')
                {
                    *pp = '\0';
                    break;
                }
            }
        }

        return p;
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
        cmd = "AT+CIPMUX=1";
    }
    else
    {
        cmd = "AT+CIPMUX=0";
    }


    if (esp8266_send_cmd(cmd) &&
        esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100)) != ESP8266_TIMEOUT)
    {
        rtc = 1;
    }
    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: close server connection
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
esp8266_close_server_connection (void)
{
    uint_fast8_t    rtc = 0;

    if (esp8266_send_cmd("AT+CIPCLOSE") &&
        esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100)) != ESP8266_TIMEOUT)
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

    sprintf (cmd, "AT+CIPSERVER=1,%d", port);

    if (esp8266_set_mux_on_off (1))
    {
        if (esp8266_send_cmd(cmd) &&
            esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100)) != ESP8266_TIMEOUT)
        {
            rtc = 1;
        }
    }
    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: check if ESP8266 is up
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
esp8266_check_up_status ()
{
    uint_fast8_t    rtc = 0;

    if (esp8266_send_cmd("AT"))
    {
        if (esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100)) == ESP8266_OK)
        {
            rtc = 1;
        }
    }

    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: get IP address of ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
static char *
esp8266_get_ip_address (void)
{
    static char     esp_answer[ESP8266_MAX_ANSWER_LEN + 1];
    static char     ip_address[ESP8266_MAX_ANSWER_LEN + 1];
    uint_fast8_t    len;
    uint_fast8_t    rtc;

    esp8266_send_cmd("AT+CIFSR");

    rtc = esp8266_get_answer (esp_answer, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100));

    if (rtc == ESP8266_UNSPECIFIED)                             // 00150900, 00170901 and 0018000902-AI03
    {                                                           // 0018000902-AI03 sends OK
        rtc = esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100));

        if (rtc == ESP8266_OK || rtc == ESP8266_TIMEOUT)        // but 00150900 + 00170901 sends nothing more
        {
            return esp_answer;
        }
    }
    else if (rtc == ESP8266_ANSWER)                             // 0020000903 and AT version:0.21.version:0.9.5
    {
        do
        {
            if (rtc == ESP8266_ANSWER)
            {
                if (! strnicmp (esp_answer, "+CIFSR:STAIP,\"", 14))
                {
                    strcpy (ip_address, esp_answer + 14);
                    len = strlen (ip_address);

                    if (len > 0)
                    {
                        ip_address[len - 1] = '\0';                    // strip "
                    }
                }
            }

            rtc = esp8266_get_answer (esp_answer, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100));
        } while (rtc != ESP8266_TIMEOUT && rtc != ESP8266_OK);

        if (rtc == ESP8266_OK)
        {
            return ip_address;
        }
    }

    return (char *) NULL;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * reset ESP8266
 *
 * don't rely on rtc, some modules send "invalid" instead of "ready"
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
esp8266_reset (void)
{
    uint_fast8_t    esp8266_rtc;
    uint_fast8_t    cnt;

    GPIO_WriteBit(ESP8266_RST_PORT, ESP8266_RST_PIN, RESET);
    esp8266_wait (5);
    GPIO_WriteBit(ESP8266_RST_PORT, ESP8266_RST_PIN, SET);

    for (cnt = 0; cnt < 50; cnt++)
    {
        esp8266_rtc = esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(500));

        if (esp8266_rtc == ESP8266_TIMEOUT || esp8266_rtc == ESP8266_READY)
        {
            break;
        }
    }
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * power down ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
esp8266_powerdown (void)
{
    GPIO_WriteBit(ESP8266_CH_PD_PORT, ESP8266_RST_PIN, RESET);
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * power up ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
esp8266_powerup (void)
{
    GPIO_WriteBit(ESP8266_CH_PD_PORT, ESP8266_RST_PIN, SET);
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * connect to access point
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_connect_to_access_point (char * ssid, char * key)
{
    static char     cmd[ESP8266_MAX_CMD_LEN];
    uint_fast8_t    esp8266_rtc = 0;
    uint_fast8_t    rtc = 0;

    if (esp8266_send_cmd("AT+CWMODE_DEF=1") &&                                                  // AT 0.50 or higher
        esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100)) == ESP8266_OK)
    {
        strcpy (cmd, "AT+CWJAP_DEF=\"");
        strcat (cmd, ssid);
        strcat (cmd, "\",\"");
        strcat (cmd, key);
        strcat (cmd, "\"");

        if (esp8266_send_cmd(cmd))
        {
            do
            {
                esp8266_rtc = esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, 3000);
            } while (esp8266_rtc != ESP8266_TIMEOUT && esp8266_rtc != ESP8266_OK && esp8266_rtc != ESP8266_ERROR);

            if (esp8266_rtc == ESP8266_OK)
            {
                rtc = 1;
            }
        }
    }
    else if (esp8266_send_cmd("AT+CWMODE=1") &&
             esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100)) != ESP8266_TIMEOUT)
    {
        strcpy (cmd, "AT+CWJAP=\"");
        strcat (cmd, ssid);
        strcat (cmd, "\",\"");
        strcat (cmd, key);
        strcat (cmd, "\"");

        if (esp8266_send_cmd(cmd))
        {
            do
            {
                esp8266_rtc = esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, 3000);
            } while (esp8266_rtc != ESP8266_TIMEOUT && esp8266_rtc != ESP8266_OK && esp8266_rtc != ESP8266_ERROR);

            if (esp8266_rtc == ESP8266_OK)
            {
                rtc = 1;
            }
        }
    }
    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * disconnect from access point
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_disconnect_from_access_point (void)
{
    uint_fast8_t    rtc = 0;

    if (esp8266_send_cmd("AT+CWQAP"))
    {
        while (esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(500)) != ESP8266_TIMEOUT)
        {
            // wait min. 30 seconds for disconnect
        }
        rtc = 1;
    }
    return rtc;
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

    uint_fast8_t        ch;
    uint_fast8_t        rtc = 0;

    if (esp8266_uart_poll (&ch))
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
 * get firmware version of ESP8266
 *
 * Following ESP8266 firmware versions have been successfully checked - received by AT+GMR:
 *
 *    00150900
 *    0018000902-AI03
 *    0020000903
 *    AT version:0.21.0.0 SDK version:0.9.5
 *    AT version:0.51.0.0 SDK version:1.5.0
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
char *
esp8266_get_firmware_version (void)
{
    static char     buf[ESP8266_MAX_F_VERSION_LEN];
    uint_fast8_t    offset;
    uint_fast8_t    rtc;

    if (firmware_version[0])
    {
        return firmware_version;
    }

     esp8266_send_cmd("AT+GMR");

    if (esp8266_get_answer (buf, ESP8266_MAX_F_VERSION_LEN, 20) == ESP8266_UNSPECIFIED)
    {
        if (! strnicmp (buf, "AT version:", 11))                                // AT version:0.21.0.0 SDK version:0.9.5
        {
            offset = 11;
        }
        else                                                                    // 00150900, 00170901, 0018000902-AI03, 0020000903
        {
            offset = 0;
        }

        do
        {
            rtc = esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, 20);
        } while (rtc != ESP8266_TIMEOUT && rtc != ESP8266_OK);

        if (rtc == ESP8266_OK)
        {
            char * p;

            strncpy (firmware_version, buf + offset, ESP8266_MAX_F_VERSION_LEN);

            for (p = firmware_version; *p; p++)         // strip a date behind AT version, e.g. "(Nov 27 2015 13:37:21)"
            {
                if (*p == '(')
                {
                    *p = '\0';
                }
            }
            return firmware_version;
        }
    }
    return (char *) NULL;
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
        esp8266_close_server_connection ()  &&
        esp8266_server (LISTEN_PORT))
    {
        infop->accesspoint  = accesspoint;
        infop->ipaddress    = ipaddress;
        esp8266_is_online   = 1;
    }
    else
    {
        esp8266_is_online = 0;
    }

    return esp8266_is_online;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * get time from time server via TCP (see RFC 868)
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_get_time (char * timeserver, time_t * curtime_p)
{
    static char     buf[ESP8266_MAX_ANSWER_LEN];
    uint_fast8_t    rtc = 0;
    uint_fast8_t    esp8266_rtc;

    if (esp8266_set_mux_on_off (1))
    {
        strcpy (buf, "AT+CIPSTART=1,\"TCP\",\"");
        strcat (buf, timeserver);
        strcat (buf, "\",37");

        if (esp8266_send_cmd(buf))
        {
            do
            {
                esp8266_rtc = esp8266_get_answer (buf, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(200));

                if (esp8266_rtc == ESP8266_IPD)     // check only IPD, all other answers depend from ESP8266 firmware version
                {
                    rtc = 1;
                }
            } while (esp8266_rtc != ESP8266_TIMEOUT);
        }

        if (rtc)
        {
            unsigned char * p = data_buf;
            time_t curtime =    ((unsigned long) p[3] <<  0) |
                                ((unsigned long) p[2] <<  8) |
                                ((unsigned long) p[1] << 16) |
                                ((unsigned long) p[0] << 24);

            *curtime_p = curtime;
        }
    }

	return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * get time from NTP server (UDP)
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_get_ntp_time (char * timeserver, time_t * curtime_p)
{
    static char     buf[ESP8266_MAX_ANSWER_LEN];
    unsigned char   ntp_buf[NTP_PACKET_SIZE];
    uint_fast8_t    esp8266_rtc = 0;
    uint_fast8_t    rtc = 0;

    if (esp8266_set_mux_on_off (1))
    {
        memset (ntp_buf, 0, NTP_PACKET_SIZE);

        ntp_buf[0] = 0b11100011;                                            // LI, version, mode
        ntp_buf[1] = 0;                                                     // stratum, or type of clock
        ntp_buf[2] = 6;                                                     // polling interval
        ntp_buf[3] = 0xEC;                                                  // peer clock precision
                                                                            // 8 bytes of zero for Root Delay & Root Dispersion
        ntp_buf[12] = 49;
        ntp_buf[13] = 0x4E;
        ntp_buf[14] = 49;
        ntp_buf[15] = 52;

        strcpy (buf, "AT+CIPSTART=1,\"UDP\",\"");
        strcat (buf, timeserver);
        strcat (buf, "\",123,8123,0");

        if (esp8266_send_cmd(buf))
        {
            esp8266_rtc = esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(300));

            if (esp8266_rtc == ESP8266_CONNECT)                 // firmware 0020000903 and AT version:0.21.version:0.9.5 or higher
            {                                                   // send CONNECT, then OK
                esp8266_rtc = esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100));
            }

            if (esp8266_rtc == ESP8266_OK)
            {
                esp8266_send_data (ntp_buf, NTP_PACKET_SIZE);

                do
                {
                    esp8266_rtc = esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100));

                    if (esp8266_rtc == ESP8266_IPD)             // check only IPD, all other answers depend from ESP8266 firmware version
                    {
                        rtc = 1;
                    }
                } while (esp8266_rtc != ESP8266_TIMEOUT);

                if (esp8266_send_cmd("AT+CIPCLOSE=1"))
                {
                    do
                    {
                        // ignore answers, they depend from ESP8266 firmware version
                        esp8266_rtc = esp8266_get_answer ((char *) NULL, ESP8266_MAX_ANSWER_LEN, TIMEOUT_MSEC(100));
                    } while (esp8266_rtc != ESP8266_TIMEOUT);
                }
            }
        }

        if (rtc)
        {
            time_t curtime =    data_buf[40] << 24 |
                                data_buf[41] << 16 |
                                data_buf[42] <<  8 |
                                data_buf[43];
            *curtime_p = curtime;
        }
    }

	return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * initialize UART and ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
#define MAX_BAUDRATES   3

uint_fast8_t
esp8266_init (void)
{
    static uint_fast8_t     already_called;
    static uint_fast32_t    baudrates[MAX_BAUDRATES] = { 115200, 57600, 9600 };     // baud rates to check
    uint_fast8_t            i;

    if (! already_called)
    {
        already_called = 1;

        log_init (115200);
        log_puts ("ESP8266 LOGGER\r\n");

        esp8266_gpio_init ();

        for (i = 0; i < MAX_BAUDRATES; i++)
        {
            esp8266_uart_init (baudrates[i]);

            esp8266_reset ();

            esp8266_is_up = esp8266_check_up_status ();

            if (esp8266_is_up)
            {
                if (baudrates[i] != 115200)
                {
                    (void) esp8266_send_cmd ("AT+CIOBAUD=115200");      // change baud rate of ESP8266 to 115200
                    esp8266_uart_init (115200);                                 // change baud rate of UART to 115200
                }
                break;
            }
        }
    }

    return esp8266_is_up;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * Should be called every ten msec
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
esp8266_ISR (void)
{
    esp8266_ten_ms_tick = 1;
}
