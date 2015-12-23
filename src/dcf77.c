/*-------------------------------------------------------------------------------------------------------------------------------------------
 * dcf77.c - dcf77 routines
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#include "wclock24h-config.h"
#include "dcf77.h"
#include "led-green.h"
#include "mcurses.h"
#include "monitor.h"

#if defined (STM32F407VG)                                   // STM32F4 Discovery Board PC15

#  define DCF77_PERIPH_CLOCK_CMD  RCC_AHB1PeriphClockCmd
#  define DCF77_PERIPH            RCC_AHB1Periph_GPIOC
#  define DCF77_PORT              GPIOC
#  define DCF77_PIN               GPIO_Pin_15

#elif defined (STM32F401RE) || defined (STM32F411RE)        // STM32F401 / STM32F411 Nucleo Board PC11

#  define DCF77_PERIPH_CLOCK_CMD  RCC_AHB1PeriphClockCmd
#  define DCF77_PERIPH            RCC_AHB1Periph_GPIOC
#  define DCF77_PORT              GPIOC
#  define DCF77_PIN               GPIO_Pin_11

#elif defined (STM32F103)

#  define DCF77_PERIPH_CLOCK_CMD  RCC_APB2PeriphClockCmd
#  define DCF77_PERIPH            RCC_APB2Periph_GPIOA
#  define DCF77_PORT              GPIOA
#  define DCF77_PIN               GPIO_Pin_4

#else
#  error STM32 unknown
#endif

uint_fast8_t                    dcf77_is_up = 0;

static uint_fast8_t             time_is_valid = 0;

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * DCF77 init
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void dcf77_init (void)
{
    GPIO_InitTypeDef gpio;

    GPIO_StructInit (&gpio);
    DCF77_PERIPH_CLOCK_CMD (DCF77_PERIPH, ENABLE);

    gpio.GPIO_Pin   = DCF77_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;

#if defined (STM32F10X)
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;     // use pin as input, no pullup
#elif defined (STM32F4XX)
    gpio.GPIO_Mode  = GPIO_Mode_IN;              // use pin as input, no pullup
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;          // possible values: GPIO_PuPd__NOPULL  GPIO_PuPd__UP   GPIO_PuPd__DOWN
#endif

    GPIO_Init(DCF77_PORT, &gpio);
}

#define STATE_UNKNOWN   0
#define STATE_WAIT      1
#define STATE_0         2
#define STATE_1         3

static uint_fast8_t     isdst;
static uint_fast8_t     minute;
static uint_fast8_t     hour;
static uint_fast8_t     wday;
static uint_fast8_t     mday;
static uint_fast8_t     month;
static uint_fast16_t    year;

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * DCF77 tick - called every 1/100 of a second
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dcf77_tick (void)
{
    static uint_fast8_t cnt         = 0;
    static uint_fast8_t state       = STATE_UNKNOWN;
    static uint_fast8_t last_pin    = 0;
    static uint_fast8_t bitno       = 0xff;
    static uint_fast8_t p1          = 0;
    static uint_fast8_t p2          = 0;
    static uint_fast8_t p3          = 0;
    uint_fast16_t       pin;

    if (GPIO_ReadInputDataBit(DCF77_PORT, DCF77_PIN) == Bit_RESET)
    {
        led_green_off ();
        pin = 0;
    }
    else
    {
        led_green_on();
        pin = 1;
    }

    if (state == STATE_UNKNOWN)
    {
        state = STATE_WAIT;
        last_pin = pin;
    }
    else if (state == STATE_WAIT)
    {
        if (last_pin != pin)
        {
            state = (pin == 0) ? STATE_0 : STATE_1;
            last_pin = pin;
            bitno = 0xff;
            cnt = 0;
        }
    }
    else if (state == STATE_0)
    {
        if (pin == last_pin)
        {
            cnt++;
        }
        else
        {
            dcf77_is_up = 1;
            state = STATE_1;
            last_pin = pin;

            if (mcurses_is_up)
            {
                mvprintw (DCF77_LOG_LINE, DCF77_LOG_COL, "LO: %3d   ", cnt);
            }

            if (cnt > 120)
            {
                bitno = 0;

                p1      = 0;
                p2      = 0;
                p3      = 0;

                isdst   = 0;
                minute  = 0;
                hour    = 0;
                wday    = 0;
                mday    = 0;
                month   = 0;
                year    = 0;
            }
            else if (cnt < 75)
            {
                if (mcurses_is_up)
                {
                    mvaddstr (DCF_MSG_LINE, DCF_MSG_COL, "DCF77 Error: Low pulse < 75");
                    clrtoeol ();
                }

                state = STATE_UNKNOWN;
            }

            cnt = 0;
        }
    }
    else // if (state == STATE_1)
    {
        if (pin == last_pin)
        {
            cnt++;
        }
        else
        {
            state = STATE_0;
            last_pin = pin;

            if (mcurses_is_up)
            {
                mvprintw (DCF77_LOG_LINE, DCF77_LOG_COL, "HI: %3d BIT: %3d isdst: %d time: %02d.%02d.%02d %02d:%02d",
                          cnt, bitno, isdst, mday, month, year, hour, minute);
            }

            if (cnt <= 5 || cnt >= 25)
            {
                if (mcurses_is_up)
                {
                    if (cnt <= 5)
                    {
                        mvaddstr (DCF_MSG_LINE, DCF_MSG_COL, "DCF77 Error: High pulse <= 5");
                    }
                    else
                    {
                        mvaddstr (DCF_MSG_LINE, DCF_MSG_COL, "DCF77 Error: High pulse >= 25");
                    }
                    clrtoeol ();
                }

                state = STATE_UNKNOWN;
            }
            else if (cnt > 15)
            {
                switch (bitno)
                {
                    case 17: isdst   =  1; break;

                    case 21: minute +=  1; p1++; break;
                    case 22: minute +=  2; p1++; break;
                    case 23: minute +=  4; p1++; break;
                    case 24: minute +=  8; p1++; break;
                    case 25: minute += 10; p1++; break;
                    case 26: minute += 20; p1++; break;
                    case 27: minute += 40; p1++; break;

                    case 28:
                    {
                        if (! (p1 & 0x01))
                        {
                            if (mcurses_is_up)
                            {
                                mvaddstr (DCF_MSG_LINE, DCF_MSG_COL, "DCF77 Error: parity P1 incorrect");
                                clrtoeol ();
                            }
                            state = STATE_UNKNOWN;
                        }
                        break;
                    }

                    case 29: hour   +=  1; p2++; break;
                    case 30: hour   +=  2; p2++; break;
                    case 31: hour   +=  4; p2++; break;
                    case 32: hour   +=  8; p2++; break;
                    case 33: hour   += 10; p2++; break;
                    case 34: hour   += 20; p2++; break;

                    case 35:
                    {
                        if (! (p2 & 0x01))
                        {
                            if (mcurses_is_up)
                            {
                                mvaddstr (DCF_MSG_LINE, DCF_MSG_COL, "DCF77 Error: parity P2 incorrect");
                                clrtoeol ();
                            }
                            state = STATE_UNKNOWN;
                        }
                        break;
                    }

                    case 36: mday   +=  1; p3++; break;
                    case 37: mday   +=  2; p3++; break;
                    case 38: mday   +=  4; p3++; break;
                    case 39: mday   +=  8; p3++; break;
                    case 40: mday   += 10; p3++; break;
                    case 41: mday   += 20; p3++; break;

                    case 42: wday   +=  1; p3++; break;
                    case 43: wday   +=  2; p3++; break;
                    case 44: wday   +=  4; p3++; break;

                    case 45: month  +=  1; p3++; break;
                    case 46: month  +=  2; p3++; break;
                    case 47: month  +=  4; p3++; break;
                    case 48: month  +=  8; p3++; break;
                    case 49: month  += 10; p3++; break;

                    case 50: year   +=  1; p3++; break;
                    case 51: year   +=  2; p3++; break;
                    case 52: year   +=  4; p3++; break;
                    case 53: year   +=  8; p3++; break;
                    case 54: year   += 10; p3++; break;
                    case 55: year   += 20; p3++; break;
                    case 56: year   += 40; p3++; break;
                    case 57: year   += 80; p3++; break;

                    case 58:
                    {
                        if (! (p3 & 0x01))
                        {
                            if (mcurses_is_up)
                            {
                                mvaddstr (DCF_MSG_LINE, DCF_MSG_COL, "DCF77 Error: parity P3 incorrect");
                                clrtoeol ();
                            }
                            state = STATE_UNKNOWN;
                        }
                        break;
                    }
                }
            }
            else
            {
                switch (bitno)
                {
                    case 28:
                    {
                        if (p1 & 0x01)
                        {
                            if (mcurses_is_up)
                            {
                                mvaddstr (DCF_MSG_LINE, DCF_MSG_COL, "DCF77 Error: parity P1 incorrect");
                                clrtoeol ();
                            }
                            state = STATE_UNKNOWN;                        }
                        break;
                    }

                    case 35:
                    {
                        if (p2 & 0x01)
                        {
                            if (mcurses_is_up)
                            {
                                mvaddstr (DCF_MSG_LINE, DCF_MSG_COL, "DCF77 Error: parity P2 incorrect");
                                clrtoeol ();
                            }
                            state = STATE_UNKNOWN;
                        }
                        break;
                    }

                    case 58:
                    {
                        if (p3 & 0x01)
                        {
                            if (mcurses_is_up)
                            {
                                mvaddstr (DCF_MSG_LINE, DCF_MSG_COL, "DCF77 Error: parity P3 incorrect");
                                clrtoeol ();
                            }
                            state = STATE_UNKNOWN;
                        }
                        break;
                    }
                }

            }

            if (bitno != 0xff)
            {
                bitno++;

                if (bitno == 59)
                {
                    time_is_valid = 1;
                }
                else if (bitno >= 60)
                {
                    state = STATE_UNKNOWN;
                }
            }

            cnt = 0;
        }
    }
    return;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * DCF77 get time
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dcf77_time (struct tm * tm)
{
    uint_fast8_t    rtc = time_is_valid;

    if (time_is_valid)
    {
        tm->tm_year     = year + 100;                   // tm_year begins with 1900
        tm->tm_mon      = month - 1;                    // tm_month begins with 0
        tm->tm_mday     = mday;
        tm->tm_isdst    = 0;
        tm->tm_hour     = hour;
        tm->tm_min      = minute;
        tm->tm_sec      = 0;

        time_is_valid   = 0;
    }
    return rtc;
}

