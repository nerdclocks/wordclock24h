/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * esp8266.c - ESP8266 WLAN routines
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
#include "log.h"
#include "esp8266.h"
#include "esp8266-config.h"

#undef UART_PREFIX
#define UART_PREFIX         esp8266
#include "uart.h"

#define NTP_PACKET_SIZE     48
#define LISTEN_PORT         2424
#define HTTP_PORT           80

#define MSEC(x)             (x/10)

/*--------------------------------------------------------------------------------------------------------------------------------------
 * globals:
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
volatile uint_fast8_t                   esp8266_ten_ms_tick;            // should be set every 10 msec to 1, see IRQ in main.c

static ESP8266_INFO                     esp8266_info;
static char                             cmd[ESP8266_MAX_CMD_LEN];

#if defined (STM32F407VG)                                               // STM32F4 Discovery Board PD12

#define ESP8266_RST_PERIPH_CLOCK_CMD    RCC_AHB1PeriphClockCmd
#define ESP8266_RST_PERIPH              RCC_AHB1Periph_GPIOC
#define ESP8266_RST_PORT                GPIOC
#define ESP8266_RST_PIN                 GPIO_Pin_5

#define ESP8266_CH_PD_PERIPH_CLOCK_CMD  RCC_AHB1PeriphClockCmd
#define ESP8266_CH_PD_PERIPH            RCC_AHB1Periph_GPIOC
#define ESP8266_CH_PD_PORT              GPIOC
#define ESP8266_CH_PD_PIN               GPIO_Pin_4

#define ESP8266_FLASH_PERIPH_CLOCK_CMD  RCC_AHB1PeriphClockCmd
#define ESP8266_FLASH_PERIPH            RCC_AHB1Periph_GPIOC
#define ESP8266_FLASH_PORT              GPIOC
#define ESP8266_FLASH_PIN               GPIO_Pin_3

#elif defined (STM32F401RE) || defined (STM32F411RE)            // STM32F401/STM32F411 Nucleo Board

#define ESP8266_RST_PERIPH_CLOCK_CMD    RCC_AHB1PeriphClockCmd
#define ESP8266_RST_PERIPH              RCC_AHB1Periph_GPIOA
#define ESP8266_RST_PORT                GPIOA
#define ESP8266_RST_PIN                 GPIO_Pin_7

#define ESP8266_CH_PD_PERIPH_CLOCK_CMD  RCC_AHB1PeriphClockCmd
#define ESP8266_CH_PD_PERIPH            RCC_AHB1Periph_GPIOA
#define ESP8266_CH_PD_PORT              GPIOA
#define ESP8266_CH_PD_PIN               GPIO_Pin_6

#define ESP8266_FLASH_PERIPH_CLOCK_CMD  RCC_AHB1PeriphClockCmd
#define ESP8266_FLASH_PERIPH            RCC_AHB1Periph_GPIOA
#define ESP8266_FLASH_PORT              GPIOA
#define ESP8266_FLASH_PIN               GPIO_Pin_4

#elif defined (STM32F103)

#define ESP8266_RST_PERIPH_CLOCK_CMD    RCC_APB2PeriphClockCmd
#define ESP8266_RST_PERIPH              RCC_APB2Periph_GPIOA
#define ESP8266_RST_PORT                GPIOA
#define ESP8266_RST_PIN                 GPIO_Pin_0

#define ESP8266_CH_PD_PERIPH_CLOCK_CMD  RCC_APB2PeriphClockCmd
#define ESP8266_CH_PD_PERIPH            RCC_APB2Periph_GPIOA
#define ESP8266_CH_PD_PORT              GPIOA
#define ESP8266_CH_PD_PIN               GPIO_Pin_1

#define ESP8266_FLASH_PERIPH_CLOCK_CMD  RCC_APB2PeriphClockCmd
#define ESP8266_FLASH_PERIPH            RCC_APB2Periph_GPIOA
#define ESP8266_FLASH_PORT              GPIOA
#define ESP8266_FLASH_PIN               GPIO_Pin_4

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

    ESP8266_RST_PERIPH_CLOCK_CMD (ESP8266_FLASH_PERIPH, ENABLE);    // enable clock for ESP8266 FLASH

    gpio.GPIO_Pin   = ESP8266_FLASH_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;

#if defined (STM32F10X)
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
#elif defined (STM32F4XX)
    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
#endif

    GPIO_Init(ESP8266_FLASH_PORT, &gpio);

    GPIO_WriteBit(ESP8266_RST_PORT, ESP8266_RST_PIN, SET);
    GPIO_WriteBit(ESP8266_CH_PD_PORT, ESP8266_CH_PD_PIN, SET);
    GPIO_WriteBit(ESP8266_FLASH_PORT, ESP8266_FLASH_PIN, SET);
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

/*--------------------------------------------------------------------------------------------------------------------------------------
 * get message from ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_get_message (uint_fast16_t timeout_ten_ms)
{
    static char     answer[ESP8266_MAX_ANSWER_LEN + 1];
    uint_fast8_t    l = 0;
    uint_fast8_t    ch;
    uint_fast8_t    rtc = ESP8266_TIMEOUT;

    log_flush ();
    esp8266_uart_flush ();

    if (esp8266_uart_char_available ())
    {
        while (1)
        {
            if (esp8266_poll (&ch, timeout_ten_ms) == 0)
            {
                log_message ("Timeout");
                rtc = ESP8266_TIMEOUT;
                break;
            }

            esp8266_info.is_up = 1;

            if (ch == '\n')
            {
                answer[l] = '\0';

                if (l > 0)
                {
                    log_message (answer);

                    if (! strncmp (answer, "OK", 2))
                    {
                        rtc = ESP8266_OK;
                        break;
                    }
                    else if (! strncmp (answer, "ERROR", 5))
                    {
                        rtc = ESP8266_ERROR;
                        break;
                    }
                    else if (! strncmp (answer, "- ", 2))
                    {
                        rtc = ESP8266_DEBUGMSG;
                        break;
                    }
                    else if (! strncmp (answer, "IPADDRESS ", 10))
                    {
                        strncpy (esp8266_info.ipaddress, answer + 10, ESP8266_MAX_IPADDRESS_SIZE - 1);
                        esp8266_info.is_online = 1;
                        rtc = ESP8266_IPADDRESS;
                        break;
                    }
                    else if (! strncmp (answer, "AP ", 3))
                    {
                        strncpy (esp8266_info.accesspoint, answer + 3, ESP8266_MAX_ACCESSPOINT_SIZE - 1);
                        rtc = ESP8266_ACCESSPOINT;
                        break;
                    }
                    else if (! strncmp (answer, "TIME ", 5))
                    {
                        strncpy (esp8266_info.time, answer + 5, ESP8266_MAX_TIME_SIZE - 1);
                        rtc = ESP8266_TIME;
                        break;
                    }
                    else if (! strncmp (answer, "FIRMWARE ", 9))
                    {
                        strncpy (esp8266_info.firmware, answer + 9, ESP8266_MAX_FIRMWARE_SIZE - 1);
                        rtc = ESP8266_FIRMWARE;
                        break;
                    }
                    else if (! strncmp (answer, "CMD ", 4))
                    {
                        strncpy (esp8266_info.cmd, answer + 4, ESP8266_MAX_CMD_SIZE - 1);
                        rtc = ESP8266_CMD;
                        break;
                    }
                    else if (! strncmp (answer, "HTTP GET ", 9))
                    {
                        strncpy (esp8266_info.http_get_param, answer + 9, ESP8266_MAX_HTTP_GET_PARAM_SIZE - 1);
                        rtc = ESP8266_HTTP_GET;
                        break;
                    }
                    else
                    {
                        rtc = ESP8266_UNSPECIFIED;
                        break;
                    }
                }
                else // l == 0
                {
                    log_flush ();
                }
            }
            else if (ch == '\r')
            {
                ;
            }
            else if (l < ESP8266_MAX_ANSWER_LEN)
            {
                answer[l++] = ch;
            }
        }
    }
    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * send a command to ESP8266, wait for OK or ERROR
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_send_cmd (char * cmd)
{
    uint_fast8_t    length;
    uint_fast8_t    esp8266_rtc;
    uint_fast8_t    ch;
    uint_fast8_t    i;
    uint_fast8_t    rtc = 0;

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

    log_puts ("<0d><0a>\r\n");
    log_flush ();

    esp8266_uart_puts (cmd);
    esp8266_uart_puts ("\r\n");
    esp8266_uart_flush();

    while (1)                                                                       // wait for OK or ERROR
    {
        esp8266_rtc = esp8266_get_message (MSEC(100));

        if (esp8266_rtc == ESP8266_OK)
        {
            rtc = 1;
            break;
        }
        else if (esp8266_rtc == ESP8266_ERROR)
        {
            break;
        }
        else if (esp8266_rtc == ESP8266_TIMEOUT)
        {
            break;
        }
    }

    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * send (http) data to ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
esp8266_send_data (unsigned char * data, uint_fast8_t len)
{
    uint_fast8_t    i;

    for (i = 0; i < len; i++)
    {
        esp8266_uart_putc (data[i]);
    }

    esp8266_uart_flush ();

    return;
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
 * get all esp8266 infos
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
ESP8266_INFO *
esp8266_get_info (void)
{
    return &esp8266_info;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * get ESP8266 up status
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_get_up_status (void)
{
    return esp8266_info.is_up;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * get online status
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_get_online_status (void)
{
    return esp8266_info.is_online;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * get SSID of connected access point
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
char *
esp8266_get_access_point_connected (void)
{
    return esp8266_info.accesspoint;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * get IP address of ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
char *
esp8266_get_ip_address (void)
{
    return esp8266_info.ipaddress;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * get firmware version of ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
char *
esp8266_get_firmware_version (void)
{
    return esp8266_info.firmware;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * reset ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
esp8266_reset (void)
{
    uint_fast8_t    ch;

    GPIO_WriteBit(ESP8266_RST_PORT, ESP8266_RST_PIN, RESET);
    esp8266_wait (MSEC(50));                                                // wait 50 msec
    GPIO_WriteBit(ESP8266_RST_PORT, ESP8266_RST_PIN, SET);

    while (esp8266_poll (&ch, MSEC(500)))                                   // eat boot message stuff
    {
        ;
    }
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * power down ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
esp8266_powerdown (void)
{
    GPIO_WriteBit(ESP8266_CH_PD_PORT, ESP8266_CH_PD_PIN, RESET);
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * power up ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
esp8266_powerup (void)
{
    GPIO_WriteBit(ESP8266_CH_PD_PORT, ESP8266_CH_PD_PIN, SET);
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * connect to access point
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_connect_to_access_point (char * ssid, char * key)
{
    uint_fast8_t    rtc;

    sprintf (cmd, "cap \"%s\",\"%s\"", ssid, key);
    rtc = esp8266_send_cmd (cmd);

    if (rtc)
    {
        esp8266_info.ipaddress[0] = '\0';
        esp8266_info.is_online = 0;
    }

    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * start as access point
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
esp8266_accesspoint (char * ssid, char * key)
{
    uint_fast8_t    rtc;

    sprintf (cmd, "ap \"%s\",\"%s\"", ssid, key);
    rtc = esp8266_send_cmd (cmd);

    if (rtc)
    {
        esp8266_info.ipaddress[0] = '\0';
        esp8266_info.is_online = 0;
    }

    return rtc;
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * start timeserver request from timeserver
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
esp8266_start_timeserver_request (char * timeserver)
{
    if (timeserver && *timeserver)
    {
        sprintf (cmd, "time \"%s\"", timeserver);
        esp8266_send_cmd (cmd);
    }
    else
    {
        esp8266_send_cmd ("time");                                                      // use default timeserver
    }
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * set ESP8266 into flash mode and copy all characters from extern logger UART to ESP8266 UART
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
esp8266_flash (void)
{
    uint_fast8_t    ch;

    log_init (115200);
    esp8266_gpio_init ();
    esp8266_uart_init (115200);

    GPIO_WriteBit(ESP8266_FLASH_PORT, ESP8266_FLASH_PIN, RESET);
    GPIO_WriteBit(ESP8266_RST_PORT, ESP8266_RST_PIN, RESET);
    esp8266_wait (MSEC(50));                                                                   // wait 50 msec
    GPIO_WriteBit(ESP8266_RST_PORT, ESP8266_RST_PIN, SET);
    esp8266_wait (MSEC(500));                                                                  // wait 500 msec
    GPIO_WriteBit(ESP8266_FLASH_PORT, ESP8266_FLASH_PIN, SET);

    while (1)
    {
        if (esp8266_uart_poll (&ch))
        {
            log_uart_putc (ch);
        }

        if (log_uart_poll (&ch))
        {
            esp8266_uart_putc (ch);
        }
    }
}

/*--------------------------------------------------------------------------------------------------------------------------------------
 * initialize UART and ESP8266
 *--------------------------------------------------------------------------------------------------------------------------------------
 */
void
esp8266_init (void)
{
    static uint_fast8_t     already_called;

    if (! already_called)
    {
        already_called = 1;

        log_init (115200);
        log_puts ("ESP8266 LOGGER\r\n");

        esp8266_gpio_init ();
        esp8266_uart_init (115200);
        esp8266_reset ();
    }

    return;
}
