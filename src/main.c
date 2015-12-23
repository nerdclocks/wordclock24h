/*-------------------------------------------------------------------------------------------------------------------------------------------
 * main.c - main routines of wclock24h
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * System Clocks configured on STM32F401 and STM32F411 Nucleo Board (see changes in system_stm32f4xx.c):
 *
 *    External 8MHz crystal
 *    Main clock:   84MHz ((HSE_VALUE / PLL_M) * PLL_N) / PLL_P = ((8MHz / 8) * 336) / 4 = 84MHz
 *    AHB clock:    84MHz (Prescaler = 1)
 *    APB1 clock:   42MHz (Prescaler = 2)
 *    APB2 clock:   84MHz (Prescaler = 1)
 *    Timer clock:  84MHz
 *    SDIO clock:   48MHz ((HSE_VALUE / PLL_M) * PLL_N) / PLL_Q = ((8MHz / 8) * 336) / 7 = 48MHz
 *
 * On STM32F401/STM32F411 Nucleo Board, make sure that
 *
 *    - SB54 and SB55 are OFF
 *    - SB16 and SB50 are OFF
 *    - R35 and R37 are soldered (0R or simple wire)
 *    - C33 and C34 are soldered with 22pF capacitors
 *    - X3 is soldered with 8MHz crystal
 *
 * Internal devices used:
 *
 *    +-------------------------+---------------------------+---------------------------+
 *    | Device                  | STM32F4x1 Nucleo          | STM32F103C8T6             |
 *    +-------------------------+---------------------------+---------------------------+
 *    | User button             | GPIO:   PC13              | GPIO:   PA6               |
 *    | Green LED               | GPIO:   PA5               | GPIO:   PC13              |
 *    +-------------------------+---------------------------+---------------------------+
 *
 * External devices:
 *
 *    +-------------------------+---------------------------+---------------------------+
 *    | Device                  | STM32F4x1 Nucleo          | STM32F103C8T6             |
 *    +-------------------------+---------------------------+---------------------------+
 *    | TSOP31238 (IRMP)        | GPIO:   PC10              | GPIO:   PA7|              |
 *    | DCF77                   | GPIO:   PC11              | GPIO:   PA4               |
 *    | MCURSES terminal (USB)  | USART2: TX=PA2  RX=PA3    | USART2: TX=PA2  RX=PA3    |
 *    | ESP8266 USART           | USART6: TX=PA11 RX=PA12   | USART3: TX=PB10 RX=PB11   |
 *    | ESP8266 GPIO            | GPIO:   RST=PA7 CH_PD=PA6 | GPIO:   RST=PA0 CH_PD=PA1 |
 *    | I2C DS3231 & EEPROM     | I2C3:   SCL=PA8 SDA=PC9   | I2C1:   SCL=PB6 SDA=PB7   |
 *    | WS2812                  | DMA1:   PC6               | DMA1:   PA8               |
 *    | DS18xx (OneWire)        | GPIO:   PD2               | GPIO:   PA9               |
 *    | LDR                     | ADC:    ACD1_IN14=PC4     | ADC:    ADC12_IN5=PA5     |
 *    +-------------------------+---------------------------+---------------------------+
 *
 * Timers:
 *
 *    +-------------------------+---------------------------+---------------------------+
 *    | Device                  | STM32F4x1 Nucleo          | STM32F103C8T6             |
 *    +-------------------------+---------------------------+---------------------------+
 *    | General (IRMP etc.)     | TIM2                      | TIM2                      |
 *    | WS2812                  | TIM3                      | TIM1                      |
 *    | DS18xx (OneWire)        | Systick (see delay.c)     | Systick (see delay.c)     |
 *    +-------------------------+---------------------------+---------------------------+
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "wclock24h-config.h"
#include "mcurses.h"
#include "monitor.h"
#include "dsp.h"
#include "dcf77.h"
#include "timeserver.h"
#include "listener.h"
#include "esp8266.h"
#include "led-green.h"
#include "irmp.h"
#include "remote-ir.h"
#include "cmd.h"
#include "button.h"
#include "eeprom.h"
#include "eeprom-data.h"
#include "tempsensor.h"
#include "ds18xx.h"
#include "rtc.h"
#include "ldr.h"
#include "delay.h"

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * global variables
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static volatile uint_fast8_t    animation_flag              = 0;        // flag: animate LEDs
static volatile uint_fast8_t    dcf77_flag                  = 0;        // flag: check DCF77 signal
static volatile uint_fast8_t    ds3231_flag                 = 0;        // flag: read date/time from RTC DS3231
static volatile uint_fast8_t    net_time_flag               = 0;        // flag: read date/time from time server
static volatile uint_fast8_t    ldr_conversion_flag         = 0;        // flag: read LDR value
static volatile uint_fast8_t    show_time_flag              = 0;        // flag: update time on display
static volatile uint_fast8_t    short_isr                   = 0;        // flag: run TIM2_IRQHandler() in short version
static volatile uint32_t        uptime                      = 0;        // uptime in seconds

static ESP8266_CONNECTION_INFO  esp8266_connection_info;                // ESP8266 connection info: SSID & IP address
static uint_fast8_t             brightness_control_per_ldr  = 0;        // flag: LDR controls brightness

static uint_fast8_t             display_mode                = 0;        // display mode
static uint_fast8_t             animation_mode              = 0;        // animation mode
static volatile uint_fast8_t    hour                        = 0;        // current hour
static volatile uint_fast8_t    minute                      = 0;        // current minute
static volatile uint_fast8_t    second                      = 0;        // current second

static int_fast16_t             hour_night_off              = -1;       // time when clock should be powered off during night
static int_fast16_t             minute_night_off            = -1;
static int_fast16_t             hour_night_on               = -1;       // time when clock should be powered on after night
static int_fast16_t             minute_night_on             = -1;

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * timer definitions:
 *
 *      F_INTERRUPTS    = TIM_CLK / (TIM_PRESCALER + 1) / (TIM_PERIOD + 1)
 * <==> TIM_PRESCALER   = TIM_CLK / F_INTERRUPTS / (TIM_PERIOD + 1) - 1
 *
 * STM32F4x1:
 *      TIM_PERIOD      =   8 - 1 =   7
 *      TIM_PRESCALER   = 700 - 1 = 699
 *      F_INTERRUPTS    = 84000000 / 700 / 8 = 15000
 * STM32F103:
 *      TIM_PERIOD      =   6 - 1 =   5
 *      TIM_PRESCALER   = 800 - 1 = 799
 *      F_INTERRUPTS    = 72000000 / 800 / 6 = 15000
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#if defined (STM32F401RE) || defined (STM32F411RE)              // STM32F401/STM32F411 Nucleo Board PC13
#define TIM_CLK                 84000000L                       // timer clock, 84MHz on STM32F401/411 Nucleo Board
#define TIM_PERIOD              7

#elif defined (STM32F103)
#define TIM_CLK                 72000000L                       // timer clock, 72MHz on STM32F103
#define TIM_PERIOD              5
#else
#error STM32 unknown
#endif

#define TIM_PRESCALER           ((TIM_CLK / F_INTERRUPTS) / (TIM_PERIOD + 1) - 1)


/*-------------------------------------------------------------------------------------------------------------------------------------------
 * EEPROM version
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint32_t                 eeprom_version = 0xFFFFFFFF;

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * init timer2 for IRMP
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
timer2_init (void)
{
    TIM_TimeBaseInitTypeDef     tim;
    NVIC_InitTypeDef            nvic;

    TIM_TimeBaseStructInit (&tim);
    RCC_APB1PeriphClockCmd (RCC_APB1Periph_TIM2, ENABLE);

    tim.TIM_ClockDivision   = TIM_CKD_DIV1;
    tim.TIM_CounterMode     = TIM_CounterMode_Up;
    tim.TIM_Period          = TIM_PERIOD;
    tim.TIM_Prescaler       = TIM_PRESCALER;
    TIM_TimeBaseInit (TIM2, &tim);

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    nvic.NVIC_IRQChannel                    = TIM2_IRQn;
    nvic.NVIC_IRQChannelCmd                 = ENABLE;
    nvic.NVIC_IRQChannelPreemptionPriority  = 0x0F;
    nvic.NVIC_IRQChannelSubPriority         = 0x0F;
    NVIC_Init (&nvic);

    TIM_Cmd(TIM2, ENABLE);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * timer2 IRQ handler for IRMP
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
TIM2_IRQHandler (void)
{
    static uint_fast16_t ldr_cnt;
    static uint_fast16_t animation_cnt;
    static uint_fast16_t clk_cnt;
    static uint_fast16_t dcf77_cnt;
    static uint_fast16_t net_time_cnt;
    static uint_fast16_t eeprom_cnt;

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

    if (short_isr)                                              // run this handler in short version?
    {
        clk_cnt++;

        if (clk_cnt == F_INTERRUPTS)                            // increment internal clock every second
        {
            clk_cnt = 0;

            uptime++;

            second++;

            if (second == 60)
            {
                second = 0;
                minute++;

                if (minute == 60)
                {
                    minute = 0;
                    hour++;

                    if (hour == 24)
                    {
                        hour = 0;
                    }
                }
            }

            // show_time_flag = 1;
        }
    }
    else
    {                                                               // no short version, run all
        (void) irmp_ISR ();                                         // call irmp ISR


        if (uptime > 1)
        {
            ldr_cnt++;

            if (ldr_cnt == F_INTERRUPTS / 4)                      // start LDR conversion every 1/4 seconds
            {
                ldr_conversion_flag = 1;
                ldr_cnt = 0;
            }
        }

        animation_cnt++;

        if (animation_cnt == F_INTERRUPTS / 40)                     // set animation_flag every 1/40 of a second
        {
            animation_flag = 1;
            animation_cnt = 0;
        }

        dcf77_cnt++;

        if (dcf77_cnt == F_INTERRUPTS / 100)                        // set dcf77_flag every 1/100 of a second
        {
            dcf77_flag = 1;
            dcf77_cnt = 0;
        }

        net_time_cnt++;

        if (net_time_cnt == F_INTERRUPTS / 100)                     // set esp8266_ten_ms_tick every 1/100 of a second
        {
            esp8266_ten_ms_tick = 1;
            net_time_cnt = 0;
        }

        eeprom_cnt++;

        if (eeprom_cnt == F_INTERRUPTS / 1000)                      // set eeprom_ms_tick every 1/1000 of a second
        {
            eeprom_ms_tick = 1;
            eeprom_cnt = 0;
        }

        clk_cnt++;

        if (clk_cnt == F_INTERRUPTS)                                // increment internal clock every second
        {
            clk_cnt = 0;

            uptime++;

            second++;

            if (second == 30)
            {
                ds3231_flag = 1;                                    // check RTC every hh:mm:30
            }
            else if (second == 60)
            {
                second = 0;
                minute++;

                if (minute == 60)
                {
                    minute = 0;
                    hour++;

                    if (hour == 24)
                    {
                        hour = 0;
                    }
                }
            }

            show_time_flag = 1;

            if (esp8266_is_online)
            {
                if ((minute % 10) == 0 && second == 17)             // net time update every 10 minutes, at hh:m0:17
                {
                    net_time_flag = 1;
                }
            }
        }
    }
}


/*-------------------------------------------------------------------------------------------------------------------------------------------
 * read version from EEPROM
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
read_version_from_eeprom (void)
{
    uint_fast8_t    rtc = 0;

    if (eeprom_is_up)
    {
        rtc = eeprom_read (EEPROM_DATA_OFFSET_VERSION, (uint8_t *) &eeprom_version, sizeof(uint32_t));
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * write version to EEPROM
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
write_version_to_eeprom (void)
{
    uint_fast8_t    rtc = 0;

    if (eeprom_is_up)
    {
        rtc = eeprom_write (EEPROM_DATA_OFFSET_VERSION, (uint8_t *) &eeprom_version, sizeof(uint32_t));
    }

    return rtc;
}


static void
repaint_screen (void)
{
    if (mcurses_is_up)
    {
        monitor_show_screen ();                                                     // show clear screen and LEDs
        monitor_show_modes (display_mode, animation_mode);                          // show current modes
        monitor_show_status (&esp8266_connection_info, brightness_control_per_ldr); // show current states
        monitor_show_colors ();                                                     // show colors & brightness
        monitor_show_clock (display_mode, hour, minute, second);                    // show clock
    }
}
/*-------------------------------------------------------------------------------------------------------------------------------------------
 * main function
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
int
main ()
{
    static uint_fast8_t     last_ldr_value = 0xFF;
    struct tm               tm;
    LISTENER_DATA           lis;
    IRMP_DATA               irmp_data;
    uint_fast8_t            status_led_cnt              = 0;
    uint_fast8_t            do_display                  = 1;
    uint_fast8_t            show_temperature            = 0;
    uint_fast8_t            update_leds_only            = 0;
    uint_fast8_t            time_changed                = 0;
    uint_fast8_t            power_is_on                 = 1;
    uint_fast8_t            night_power_is_on           = 1;
    uint_fast8_t            temperature_index           = 0xFF;
    uint32_t                stop_time;
    uint_fast8_t            cmd;
    uint_fast8_t            ldr_value;
    uint8_t                 ch;

    SystemInit ();
    SystemCoreClockUpdate();                                    // needed for Nucleo board

    if (initscr () == OK)                                       // initialize mcurses
    {
        nodelay (TRUE);
    }

    irmp_init ();                                               // initialize IRMP
    timer2_init ();                                             // initialize timer2 for IRMP, DCF77 & EEPROM
    delay_init (DELAY_RESOLUTION_1_US);                         // initialize delay functions with granularity of 1 us
    led_green_init ();                                          // initialize GPIO for green LED on disco or nucleo board
    button_init ();                                             // initialize GPIO for user button on disco or nucleo board
    rtc_init ();                                                // initialize I2C RTC
    eeprom_init ();                                             // initialize I2C EEPROM

    if (eeprom_is_up)
    {
        read_version_from_eeprom ();

        if (eeprom_version == EEPROM_VERSION)
        {
            remote_ir_read_codes_from_eeprom ();
            dsp_read_config_from_eeprom ();
            timeserver_read_data_from_eeprom ();
        }
    }

    ldr_init ();                                                // initialize LDR (ADC)
    dsp_init ();                                                // initialize display
    dcf77_init ();                                              // initialize DCF77

    short_isr = 1;
    temp_init ();                                               // initialize DS18xx
    short_isr = 0;

    reset_led_states ();
    display_mode                = dsp_get_display_mode ();
    animation_mode              = dsp_get_animation_mode ();
    brightness_control_per_ldr  = dsp_get_automatic_brightness_control ();

    if (eeprom_is_up)
    {
        if (eeprom_version != EEPROM_VERSION)
        {
            if (mcurses_is_up)
            {
                mvaddstr (LOG_LINE, LOG_COL, "Initializing EEPROM...");
                refresh ();
            }

            eeprom_version = EEPROM_VERSION;
            write_version_to_eeprom ();
            remote_ir_write_codes_to_eeprom ();
            dsp_write_config_to_eeprom ();
            timeserver_write_data_to_eeprom ();

            if (mcurses_is_up)
            {
                move (LOG_LINE, LOG_COL);
                clrtoeol ();
            }
        }
    }

    ds3231_flag = 1;

    repaint_screen ();

    stop_time = uptime + 3;                                     // wait 3 seconds for IR signal...
    dsp_set_status_led (1, 1, 1);                               // show white status LED

    while (uptime < stop_time)
    {
        if (irmp_get_data (&irmp_data))                         // got IR signal?
        {
            dsp_set_status_led (1, 0, 0);                       // yes, show red status LED
            delay_sec (1);                                      // and wait 1 second
            (void) irmp_get_data (&irmp_data);                  // flush input of IRMP now
            dsp_set_status_led (0, 0, 0);                       // and switch status LED off

            if (remote_ir_learn ())                             // learn IR commands
            {
                remote_ir_write_codes_to_eeprom ();             // if successful, save them in EEPROM
            }
            break;                                              // and break the loop
        }
    }

    dsp_set_status_led (0, 0, 0);                               // switch off status LED

    while (1)
    {
        if (status_led_cnt)
        {
            status_led_cnt--;

            if (! status_led_cnt)
            {
                dsp_set_status_led (0, 0, 0);
            }
        }

        if (ldr_is_up && brightness_control_per_ldr && ldr_poll_brightness (&ldr_value))
        {
            if (ldr_value < last_ldr_value - 1 || ldr_value > last_ldr_value + 1)           // difference greater than 2
            {
                last_ldr_value = ldr_value;
                dsp_set_brightness (ldr_value);
                monitor_show_ldr_value (ldr_value);
                monitor_show_colors ();
            }
        }

        if (! mcurses_is_up)
        {
            if (getch () == '\r')
            {
                initscr ();
                nodelay (TRUE);
                repaint_screen ();
            }
        }

        if (! esp8266_is_up)                                            // esp8266 not up yet?
        {
            static int timeserver_init_already_called = 0;

            if (! timeserver_init_already_called && uptime >= 2)        // 2 seconds gone after boot?
            {
                timeserver_init ();                                     // yes, initialize ESP8266
                timeserver_init_already_called = 1;

                if (esp8266_is_up)                                      // now up?
                {
                    monitor_show_status (&esp8266_connection_info, brightness_control_per_ldr);
                }
            }
        }
        else
        {                                                               // esp8266 is up...
            if (! esp8266_is_online)                                    // but not online yet...
            {
                if ((uptime % 60) == 25)                                // check online status every minute at xx:25 after boot
                {
                    esp8266_check_online_status (&esp8266_connection_info);

                    if (esp8266_is_online)                              // now online?
                    {
                        monitor_show_status (&esp8266_connection_info, brightness_control_per_ldr);
                        net_time_flag = 1;
                    }
                }
            }
            else
            {
                uint_fast8_t    code;

                if ((code = listener (&lis)) != 0)
                {
                    dsp_set_status_led (1, 0, 0);               // got net command, light red status LED
                    status_led_cnt = 50;

                    switch (code)
                    {
                        case 'C':                               // set color
                        {
                            dsp_set_colors (&(lis.rgb));
                            monitor_show_colors ();
                            break;
                        }

                        case 'P':                               // power on/off
                        {
                            if (power_is_on != lis.power)
                            {
                                power_is_on = lis.power;

                                do_display          = 1;
                                update_leds_only    = 1;
                            }
                            break;
                        }

                        case 'D':                               // set display mode
                        {
                            if (display_mode != lis.mode)
                            {
                                display_mode = dsp_set_display_mode (lis.mode);
                                monitor_show_modes (display_mode, animation_mode);
                                do_display = 1;
                            }
                            break;
                        }

                        case 'A':                               // set animation mode
                        {
                            if (animation_mode != lis.mode)
                            {
                                animation_mode = dsp_set_animation_mode (lis.mode);
                                monitor_show_modes (display_mode, animation_mode);
                                do_display = 1;
                            }
                            break;
                        }

                        case 'B':                               // set brightness
                        {
                            if (brightness_control_per_ldr)
                            {
                                brightness_control_per_ldr = 0;
                                last_ldr_value = 0xFF;
                                dsp_set_automatic_brightness_control (brightness_control_per_ldr);
                                monitor_show_status (&esp8266_connection_info, brightness_control_per_ldr);
                            }
                            dsp_set_brightness (lis.brightness);
                            monitor_show_colors ();
                            break;
                        }

                        case 'L':                               // automatic brightness control on/off
                        {
                            if (lis.automatic_brightness_control)
                            {
                                if (ldr_is_up)
                                {
                                    brightness_control_per_ldr = 1;
                                }
                            }
                            else
                            {
                                brightness_control_per_ldr = 0;
                            }

                            last_ldr_value = 0xFF;
                            dsp_set_automatic_brightness_control (brightness_control_per_ldr);
                            monitor_show_status (&esp8266_connection_info, brightness_control_per_ldr);
                            break;
                        }

                        case 'T':                               // set date/time
                        {
                            if (rtc_is_up)
                            {
                                rtc_set_date_time (&(lis.tm));
                            }

                            hour        = lis.tm.tm_hour;
                            minute      = lis.tm.tm_min;
                            second      = lis.tm.tm_sec;
                            do_display  = 1;
                        }

                        case 'N':                               // Get net time
                        {
                            net_time_flag = 1;
                            break;
                        }

                        case 'S':                               // save configuration
                        {
                            dsp_write_config_to_eeprom ();
                            break;
                        }
                    }
                }
            }
        }

        if (dcf77_time(&tm))
        {
            dsp_set_status_led (1, 1, 0);                       // got DCF77 time, light yellow = green + red LED
            status_led_cnt = 50;

            if (rtc_is_up)
            {
                rtc_set_date_time (&tm);
            }

            hour        = tm.tm_hour;
            minute      = tm.tm_min;
            second      = tm.tm_sec;
            do_display  = 1;

            if (mcurses_is_up)
            {
                mvprintw (DCF_TIME_LINE, DCF_TIME_COL, "%02d.%02d.%04d %02d:%02d",
                          tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min);
            }
        }

        if (ds3231_flag)
        {
            if (rtc_is_up)
            {
                if (rtc_get_date_time (&tm))
                {
                    hour            = tm.tm_hour;
                    minute          = tm.tm_min;
                    second          = tm.tm_sec;
                    do_display      = 1;

                    if (mcurses_is_up)
                    {
                        mvprintw (RTC_TIME_LINE, RTC_TIME_COL, "%02d.%02d.%04d %02d:%02d:%02d",
                                  tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
                        clrtoeol ();
                    }
                }
            }

            ds3231_flag = 0;
        }

        if (ldr_is_up && brightness_control_per_ldr && ldr_conversion_flag)
        {
            ldr_start_conversion ();
            ldr_conversion_flag = 0;
        }

        if (net_time_flag)
        {
            if (esp8266_is_online)
            {
                dsp_set_status_led (0, 0, 1);                   // got NET time, light blue status LED
                status_led_cnt = 50;

                if (timeserver_get_time (&tm))
                {
                    if (rtc_is_up)
                    {
                        rtc_set_date_time (&tm);
                    }

                    hour    = tm.tm_hour;
                    minute  = tm.tm_min;
                    second  = tm.tm_sec;

                    do_display  = 1;

                    if (mcurses_is_up)
                    {
                        mvprintw (NET_TIME_LINE, NET_TIME_COL, "%02d.%02d.%04d %02d:%02d:%02d",
                                  tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
                        clrtoeol ();
                    }
                }
            }

            net_time_flag = 0;
        }

        if (show_time_flag)
        {
            if (second == 0)                                            // full minute
            {
                do_display = 1;
            }

            if (mcurses_is_up)
            {
                mvprintw (TIM_TIME_LINE, TIM_TIME_COL, "%02d:%02d:%02d", hour, minute, second);
            }

            show_time_flag = 0;
        }

        if (button_pressed ())                                          // if user pressed user button, learn IR commands
        {
            if (remote_ir_learn ())
            {
                remote_ir_write_codes_to_eeprom ();
            }
            do_display = 1;
            update_leds_only = 1;
        }

        if (power_is_on)
        {
            if (night_power_is_on)
            {
                if (hour_night_off >= (int_fast16_t) hour && minute_night_off >= (int_fast16_t) minute)
                {
                    night_power_is_on = 0;
                    do_display = 1;
                }
            }
            else
            {
                if (hour_night_on >= (int_fast16_t) hour && minute_night_on >= (int_fast16_t) minute)
                {
                    night_power_is_on = 1;
                    do_display = 1;
                }
            }
        }

        if (show_temperature)
        {
            show_temperature = 0;

            if (ds18xx_is_up)
            {
                uint32_t stop_time;
                uint_fast8_t on = power_is_on ? night_power_is_on : power_is_on;

                short_isr = 1;
                temperature_index = temp_read_temp_index ();
                short_isr = 0;

                monitor_show_temperature (temperature_index);
                dsp_temperature (on, temperature_index);

                stop_time = uptime + 5;

                while (uptime < stop_time)
                {
                    if (animation_flag)
                    {
                        animation_flag = 0;
                        dsp_animation ();
                    }
                }

                do_display = 1;                                         // force update
            }
        }

        if (do_display)                                                 // refresh display (time/mode changed)
        {
            uint_fast8_t on = power_is_on ? night_power_is_on : power_is_on;

            dsp_clock (on, hour, minute);                               // show new time

            if (! update_leds_only)
            {
                monitor_show_clock (display_mode, hour, minute, second);
            }

            do_display          = 0;
            update_leds_only    = 0;
        }

        if (animation_flag)
        {
            animation_flag = 0;
            dsp_animation ();
        }

        if (dcf77_flag)
        {
            dcf77_flag = 0;
            dcf77_tick ();
        }

        cmd = remote_ir_get_cmd ();                             // get IR command

        if (cmd != CMD_INVALID)                                 // got IR command, light green LED
        {
            dsp_set_status_led (1, 0, 0);
            status_led_cnt = 50;
        }

        if (mcurses_is_up)
        {
            if (cmd != CMD_INVALID)                             // if command valid, print command code (mcurses)
            {
                move (LOG_LINE, 0);

                switch (cmd)
                {
                    case CMD_POWER:                         addstr ("IRMP: POWER key");                     break;
                    case CMD_OK:                            addstr ("IRMP: OK key");                        break;
                    case CMD_DECREMENT_DISPLAY_MODE:        addstr ("IRMP: decrement display mode");        break;
                    case CMD_INCREMENT_DISPLAY_MODE:        addstr ("IRMP: increment display mode");        break;
                    case CMD_DECREMENT_ANIMATION_MODE:      addstr ("IRMP: decrement animation mode");      break;
                    case CMD_INCREMENT_ANIMATION_MODE:      addstr ("IRMP: increment animation mode");      break;
                    case CMD_DECREMENT_HOUR:                addstr ("IRMP: decrement minute");              break;
                    case CMD_INCREMENT_HOUR:                addstr ("IRMP: increment hour");                break;
                    case CMD_DECREMENT_MINUTE:              addstr ("IRMP: decrement minute");              break;
                    case CMD_INCREMENT_MINUTE:              addstr ("IRMP: increment minute");              break;
                    case CMD_DECREMENT_BRIGHTNESS_RED:      addstr ("IRMP: decrement red brightness");      break;
                    case CMD_INCREMENT_BRIGHTNESS_RED:      addstr ("IRMP: increment red brightness");      break;
                    case CMD_DECREMENT_BRIGHTNESS_GREEN:    addstr ("IRMP: decrement green brightness");    break;
                    case CMD_INCREMENT_BRIGHTNESS_GREEN:    addstr ("IRMP: increment green brightness");    break;
                    case CMD_DECREMENT_BRIGHTNESS_BLUE:     addstr ("IRMP: decrement blue brightness");     break;
                    case CMD_INCREMENT_BRIGHTNESS_BLUE:     addstr ("IRMP: increment blue brightness");     break;
                    case CMD_DECREMENT_BRIGHTNESS:          addstr ("IRMP: decrement brightness");          break;
                    case CMD_INCREMENT_BRIGHTNESS:          addstr ("IRMP: increment brightness");          break;
                    case CMD_GET_TEMPERATURE:               addstr ("IRMP: get temperature");               break;
                }

                clrtoeol ();
            }
            else
            {                                                   // no valid IR command, read terminal keyboard
                ch = getch ();

                switch (ch)                                     // map keys to commands
                {
                    case 'p':   cmd = CMD_POWER;                        break;
                    case 's':   cmd = CMD_OK;                           break;
                    case 'D':   cmd = CMD_DECREMENT_DISPLAY_MODE;       break;
                    case 'd':   cmd = CMD_INCREMENT_DISPLAY_MODE;       break;
                    case 'A':   cmd = CMD_DECREMENT_ANIMATION_MODE;     break;
                    case 'a':   cmd = CMD_INCREMENT_ANIMATION_MODE;     break;
                    case 'H':   cmd = CMD_DECREMENT_HOUR;               break;
                    case 'h':   cmd = CMD_INCREMENT_HOUR;               break;
                    case 'M':   cmd = CMD_DECREMENT_MINUTE;             break;
                    case 'm':   cmd = CMD_INCREMENT_MINUTE;             break;
                    case 'R':   cmd = CMD_DECREMENT_BRIGHTNESS_RED;     break;
                    case 'r':   cmd = CMD_INCREMENT_BRIGHTNESS_RED;     break;
                    case 'G':   cmd = CMD_DECREMENT_BRIGHTNESS_GREEN;   break;
                    case 'g':   cmd = CMD_INCREMENT_BRIGHTNESS_GREEN;   break;
                    case 'B':   cmd = CMD_DECREMENT_BRIGHTNESS_BLUE;    break;
                    case 'b':   cmd = CMD_INCREMENT_BRIGHTNESS_BLUE;    break;
                    case 'W':   cmd = CMD_DECREMENT_BRIGHTNESS;         break;
                    case 'w':   cmd = CMD_INCREMENT_BRIGHTNESS;         break;
                    case 'q':   cmd = CMD_AUTO_BRIGHTNESS_CONTROL;      break;
                    case 't':   cmd = CMD_GET_TEMPERATURE;              break;

                    case 'l':
                    {
                        if (mcurses_is_up)
                        {
                            clear ();
                            mvaddstr (21, 0, "You are logged out. You can close your terminal now.");
                            mvaddstr (22, 0, "Press ENTER / CR to login.");
                            endwin ();
                        }
                        break;
                    }

                    case 'i':
                    {
                        if (remote_ir_learn ())
                        {
                            remote_ir_write_codes_to_eeprom ();
                        }

                        do_display = 1;
                        update_leds_only = 1;
                        break;
                    }

                    case 'T':
                    {
                        dsp_test ();
                        break;
                    }

                    case 'c':
                    {
                        uint_fast8_t next_line;
                        clear ();

                        mvaddstr (3, 10, "1. Configure Network Module ESP8266");
                        mvaddstr (4, 10, "2. Configure LDR");
                        mvaddstr (6, 10, "0. Exit");

                        next_line = 8;
                        move (next_line, 10);
                        refresh ();

                        while ((ch = getch()) < '0' || ch > '2')
                        {
                            ;
                        }

                        if (ch == '1')
                        {
                            timeserver_configure (next_line, &esp8266_connection_info);

                            if (esp8266_is_online)
                            {
                                net_time_flag = 1;                                  // get time now!
                            }
                        }
                        else if (ch == '2')
                        {
                            ldr_configure (next_line);

                            if (! ldr_is_up)
                            {
                                brightness_control_per_ldr = 1;
                                dsp_set_automatic_brightness_control (brightness_control_per_ldr);
                                monitor_show_status (&esp8266_connection_info, brightness_control_per_ldr);
                                dsp_set_brightness (MAX_BRIGHTNESS);
                                monitor_show_colors ();
                            }
                        }

                        repaint_screen ();
                        break;
                    }

                    case 'x':
                        monitor_show_help ();
                        repaint_screen ();
                        break;

                    case 'e':
                    {
                        eeprom_dump ();
                        repaint_screen ();
                        break;

                    }

                    case 'C':
                    {
                        timeserver_cmd ();
                        repaint_screen ();
                        break;
                    }

                    case 'n':                                                   // get net time
                    {
                        if (esp8266_is_online)
                        {
                            net_time_flag = 1;
                        }
                        break;
                    }

                    case KEY_CR:                                                // refresh screen
                    {
                        initscr ();
                        nodelay (TRUE);
                        repaint_screen ();
                        break;
                    }
                }
            }
        }

        switch (cmd)
        {
            case CMD_POWER:
            {
                power_is_on = power_is_on ? 0 : 1;

                do_display          = 1;
                update_leds_only    = 1;
                break;
            }

            case CMD_OK:
            {
                dsp_write_config_to_eeprom ();
                break;
            }

            case CMD_DECREMENT_DISPLAY_MODE:                    // decrement display mode
            {
                display_mode = dsp_decrement_display_mode ();
                monitor_show_modes (display_mode, animation_mode);
                do_display = 1;
                break;
            }

            case CMD_INCREMENT_DISPLAY_MODE:                    // increment display mode
            {
                display_mode = dsp_increment_display_mode ();
                monitor_show_modes (display_mode, animation_mode);
                do_display = 1;
                break;
            }

            case CMD_DECREMENT_ANIMATION_MODE:                  // decrement display mode
            {
                animation_mode = dsp_decrement_animation_mode ();
                monitor_show_modes (display_mode, animation_mode);
                do_display = 1;
                break;
            }

            case CMD_INCREMENT_ANIMATION_MODE:                  // increment display mode
            {
                animation_mode = dsp_increment_animation_mode ();
                monitor_show_modes (display_mode, animation_mode);
                do_display = 1;
                break;
            }

            case CMD_DECREMENT_HOUR:                            // decrement hour
            {
                if (hour > 0)
                {
                    hour--;
                }
                else
                {
                    hour = 23;
                }

                second          = 0;
                do_display      = 1;
                time_changed    = 1;
                break;
            }

            case CMD_INCREMENT_HOUR:                            // increment hour
            {
                if (hour < 23)
                {
                     hour++;
                }
                else
                {
                    hour =  0;
                }

                second          = 0;
                do_display      = 1;
                time_changed    = 1;
                break;
            }

            case CMD_DECREMENT_MINUTE:                          // decrement minute
            {
                if (minute > 0)
                {
                    minute--;
                }
                else
                {
                    minute = 59;
                }

                second          = 0;
                do_display      = 1;
                time_changed    = 1;
                break;
            }

            case CMD_INCREMENT_MINUTE:                          // increment minute
            {
                if (minute < 59)
                {
                    minute++;
                }
                else
                {
                    minute = 0;
                }

                second          = 0;
                do_display      = 1;
                time_changed    = 1;
                break;
            }

            case CMD_DECREMENT_BRIGHTNESS_RED:                  // decrement red brightness
            {
                dsp_decrement_color_red ();
                do_display          = 1;
                update_leds_only    = 1;
                monitor_show_colors ();
                break;
            }

            case CMD_INCREMENT_BRIGHTNESS_RED:                  // increment red brightness
            {
                dsp_increment_color_red ();
                do_display          = 1;
                update_leds_only    = 1;
                monitor_show_colors ();
                break;
            }

            case CMD_DECREMENT_BRIGHTNESS_GREEN:                // decrement green brightness
            {
                dsp_decrement_color_green ();
                do_display          = 1;
                update_leds_only    = 1;
                monitor_show_colors ();
                break;
            }

            case CMD_INCREMENT_BRIGHTNESS_GREEN:                // increment green brightness
            {
                dsp_increment_color_green ();
                do_display          = 1;
                update_leds_only    = 1;
                monitor_show_colors ();
                break;
            }

            case CMD_DECREMENT_BRIGHTNESS_BLUE:                 // decrement blue brightness
            {
                dsp_decrement_color_blue ();
                do_display          = 1;
                update_leds_only    = 1;
                monitor_show_colors ();
                break;
            }

            case CMD_INCREMENT_BRIGHTNESS_BLUE:                 // increment blue brightness
            {
                dsp_increment_color_blue ();
                do_display          = 1;
                update_leds_only    = 1;
                monitor_show_colors ();
                break;
            }

            case CMD_AUTO_BRIGHTNESS_CONTROL:
            {
                if (brightness_control_per_ldr)
                {
                    brightness_control_per_ldr = 0;
                }
                else if (ldr_is_up)
                {
                    brightness_control_per_ldr = 1;
                }

                last_ldr_value = 0xFF;
                dsp_set_automatic_brightness_control (brightness_control_per_ldr);
                monitor_show_status (&esp8266_connection_info, brightness_control_per_ldr);
                break;
            }

            case CMD_DECREMENT_BRIGHTNESS:                      // decrement brightness
            {
                if (brightness_control_per_ldr)
                {
                    brightness_control_per_ldr = 0;
                    last_ldr_value = 0xFF;
                    dsp_set_automatic_brightness_control (brightness_control_per_ldr);
                    monitor_show_status (&esp8266_connection_info, brightness_control_per_ldr);
                }
                dsp_decrement_brightness ();
                monitor_show_colors ();
                break;
            }

            case CMD_INCREMENT_BRIGHTNESS:                      // increment brightness
            {
                if (brightness_control_per_ldr)
                {
                    brightness_control_per_ldr = 0;
                    last_ldr_value = 0xFF;
                    dsp_set_automatic_brightness_control (brightness_control_per_ldr);
                    monitor_show_status (&esp8266_connection_info, brightness_control_per_ldr);
                }

                dsp_increment_brightness ();
                monitor_show_colors ();
                break;
            }

            case CMD_GET_TEMPERATURE:                           // get net time
            {
                show_temperature    = 1;
                break;
            }

            default:
            {
                break;
            }
        }

        if (time_changed)
        {
            if (rtc_is_up)
            {
                tm.tm_hour = hour;
                tm.tm_min  = minute;
                tm.tm_sec  = second;
                rtc_set_date_time (&tm);
            }

            time_changed = 0;
        }
    }

    return 0;
}

