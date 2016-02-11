/*-------------------------------------------------------------------------------------------------------------------------------------------
 * main.c - main routines of wclock24h
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
 *
 * System Clocks configured on STM32F103 Mini development board:
 *
 *    External 8MHz crystal
 *    SYSCLK:       72MHz ((HSE_VALUE / HSE_Prediv) * PLL_Mul) = ((8MHz / 1) * 9)  = 72MHz
 *    AHB clock:    72MHz (AHB  Prescaler = 1)
 *    APB1 clock:   36MHz (APB1 Prescaler = 2)
 *    APB2 clock:   72MHz (APB2 Prescaler = 1)
 *    Timer clock:  72MHz (APB1 Prescaler = 2, Timer Multiplier 2)
 *
 * System Clocks configured on STM32F401 and STM32F411 Nucleo Board (see changes in system_stm32f4xx.c):
 *
 *    External 8MHz crystal
 *    Main clock:   84MHz ((HSE_VALUE / PLL_M) * PLL_N) / PLL_P = ((8MHz / 8) * 336) / 4 = 84MHz
 *    AHB clock:    84MHz (AHB  Prescaler = 1)
 *    APB1 clock:   42MHz (APB1 Prescaler = 2)
 *    APB2 clock:   84MHz (APB2 Prescaler = 1)
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
 *    +-------------------------+-------------------------------+-------------------------------+
 *    | Device                  | STM32F4x1 Nucleo              | STM32F103C8T6                 |
 *    +-------------------------+-------------------------------+-------------------------------+
 *    | User button             | GPIO:   PC13                  | GPIO:   PA6                   |
 *    | Board LED               | GPIO:   PA5                   | GPIO:   PC13                  |
 *    +-------------------------+-------------------------------+-------------------------------+
 *
 * External devices:
 *
 *    +-------------------------+-------------------------------+-------------------------------+
 *    | Device                  | STM32F4x1 Nucleo              | STM32F103C8T6                 |
 *    +-------------------------+-------------------------------+-------------------------------+
 *    | TSOP31238 (IRMP)        | GPIO:      PC10               | GPIO:      PB3                |
 *    | DS18xx (OneWire)        | GPIO:      PD2                | GPIO:      PB5                |
 *    | Logger (Nucleo: USB)    | USART2:    TX=PA2  RX=PA3     | USART1:    TX=PA9  RX=PA10    |
 *    | ESP8266 USART           | USART6:    TX=PA11 RX=PA12    | USART2:    TX=PA2  RX=PA3     |
 *    | ESP8266 GPIO            | GPIO:      RST=PA7 CH_PD=PA6  | GPIO:      RST=PA0 CH_PD=PA1  |
 *    | ESP8266 GPIO (FLASH)    | GPIO:      FLASH=PA4          | GPIO:      FLASH=PA4          |
 *    | I2C DS3231 & EEPROM     | I2C3:      SCL=PA8 SDA=PC9    | I2C1:      SCL=PB6 SDA=PB7    |
 *    | LDR                     | ADC:       ACD1_IN14=PC4      | ADC:       ADC12_IN5=PA5      |
 *    | WS2812                  | TIM3/DMA1: PC6                | TIM1/DMA1: PA8                |
 *    | APA102                  | SPI2/DMA1: SCK=PB13 MOSI=PB15 | SPI2/DMA1: SCK=PB13 MOSI=PB15 |
 *    +-------------------------+-------------------------------+-------------------------------+
 *
 * Timers:
 *
 *    +-------------------------+-------------------------------+-------------------------------+
 *    | Device                  | STM32F4x1 Nucleo              | STM32F103C8T6                 |
 *    +-------------------------+-------------------------------+-------------------------------+
 *    | General (IRMP etc.)     | TIM2                          | TIM2                          |
 *    | WS2812                  | TIM3                          | TIM1                          |
 *    | DS18xx (OneWire)        | Systick (see delay.c)         | Systick (see delay.c)         |
 *    +-------------------------+-------------------------------+-------------------------------+
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
#include "display.h"
#include "timeserver.h"
#include "listener.h"
#include "esp8266.h"
#include "board-led.h"
#include "irmp.h"
#include "remote-ir.h"
#include "button.h"
#include "eeprom.h"
#include "eeprom-data.h"
#include "tempsensor.h"
#include "ds18xx.h"
#include "rtc.h"
#include "ldr.h"
#include "delay.h"

#include "log.h"

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * global variables
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static volatile uint_fast8_t    animation_flag              = 0;        // flag: animate LEDs
static volatile uint_fast8_t    ds3231_flag                 = 0;        // flag: read date/time from RTC DS3231
static volatile uint_fast8_t    net_time_flag               = 0;        // flag: read date/time from time server
static volatile uint_fast16_t   net_time_countdown          = 3800;     // counter: if it counts to 0, then net_time_flag will be triggered
static volatile uint_fast8_t    ldr_conversion_flag         = 0;        // flag: read LDR value
static volatile uint_fast8_t    show_time_flag              = 0;        // flag: update time on display, set every full minute
static volatile uint_fast8_t    short_isr                   = 0;        // flag: run TIM2_IRQHandler() in short version
static volatile uint32_t        uptime                      = 0;        // uptime in seconds
static volatile uint_fast8_t    hour                        = 0;        // current hour
static volatile uint_fast8_t    minute                      = 0;        // current minute
static volatile uint_fast8_t    second                      = 0;        // current second

static uint_fast8_t             brightness_control_per_ldr  = 0;        // flag: LDR controls brightness
static uint_fast8_t             display_mode                = 0;        // display mode
static uint_fast8_t             animation_mode              = 0;        // animation mode

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
#if defined (STM32F401RE) || defined (STM32F411RE)                          // STM32F401/STM32F411 Nucleo Board PC13
#define TIM_CLK                 84000000L                                   // timer clock, 84MHz on STM32F401/411 Nucleo Board
#define TIM_PERIOD              7

#elif defined (STM32F103)
#define TIM_CLK                 72000000L                                   // timer clock, 72MHz on STM32F103
#define TIM_PERIOD              5
#else
#error STM32 unknown
#endif

#define TIM_PRESCALER           ((TIM_CLK / F_INTERRUPTS) / (TIM_PERIOD + 1) - 1)

#define STATUS_LED_FLASH_TIME   50                                          // status LED: time of flash


/*-------------------------------------------------------------------------------------------------------------------------------------------
 * EEPROM version
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint32_t                 eeprom_version = 0xFFFFFFFF;

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * initialize timer2
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
 * timer2 IRQ handler for IRMP, soft clock, dcf77, animations, several timeouts
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
TIM2_IRQHandler (void)
{
    static uint_fast16_t ldr_cnt;
    static uint_fast16_t animation_cnt;
    static uint_fast16_t clk_cnt;
    static uint_fast16_t net_time_cnt;
    static uint_fast16_t eeprom_cnt;
    static uint_fast16_t ds3231_cnt;

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

    if (short_isr)                                                          // run this handler in short version
    {
        clk_cnt++;

        if (clk_cnt == F_INTERRUPTS)                                        // increment internal clock every second
        {
            clk_cnt = 0;

            uptime++;

            second++;

            if (second == 60)
            {
                second = 0;
                minute++;

                show_time_flag = 1;

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
        }
    }
    else
    {                                                                       // no short version, run all
#if SAVE_RAM == 0
        (void) irmp_ISR ();                                                 // call irmp ISR
#endif

        if (uptime > 1)
        {
            ldr_cnt++;

            if (ldr_cnt == F_INTERRUPTS / 4)                                // start LDR conversion every 1/4 seconds
            {
                ldr_conversion_flag = 1;
                ldr_cnt = 0;
            }
        }

        animation_cnt++;

        if (animation_cnt == F_INTERRUPTS / 40)                             // set animation_flag every 1/40 of a second
        {
            animation_flag = 1;
            animation_cnt = 0;
        }

        net_time_cnt++;

        if (net_time_cnt == F_INTERRUPTS / 100)                             // set esp8266_ten_ms_tick every 1/100 of a second
        {
            esp8266_ten_ms_tick = 1;
            net_time_cnt = 0;
        }

        eeprom_cnt++;

        if (eeprom_cnt == F_INTERRUPTS / 1000)                              // set eeprom_ms_tick every 1/1000 of a second
        {
            eeprom_ms_tick = 1;
            eeprom_cnt = 0;
        }

        clk_cnt++;

        if (clk_cnt == F_INTERRUPTS)                                        // increment internal clock every second
        {
            clk_cnt = 0;

            uptime++;

            second++;

            ds3231_cnt++;

            if (ds3231_cnt >= 70)                                           // get rtc time every 70 seconds
            {
                ds3231_flag = 1;
                ds3231_cnt = 0;
            }

            if (second == 60)
            {
                second = 0;
                minute++;

                show_time_flag = 1;

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

            if (net_time_countdown)
            {
                net_time_countdown--;

                if (net_time_countdown == 0)                                // net time update every 3800 minutes
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

uint_fast8_t            esp8266_is_online = 0;

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
    ESP8266_INFO *          esp8266_infop;
    uint_fast8_t            esp8266_is_up = 0;
    uint_fast8_t            code;

#if SAVE_RAM == 0
    IRMP_DATA               irmp_data;
    uint32_t                stop_time;
    uint_fast8_t            cmd;
#endif
    uint_fast8_t            status_led_cnt              = 0;
    uint_fast8_t            display_flag                = DISPLAY_FLAG_UPDATE_ALL;
    uint_fast8_t            show_temperature            = 0;
    uint_fast8_t            time_changed                = 0;
    uint_fast8_t            power_is_on                 = 1;
    uint_fast8_t            night_power_is_on           = 1;
    uint_fast8_t            ldr_value;
    uint_fast8_t            ap_mode = 0;

    SystemInit ();
    SystemCoreClockUpdate();                                                // needed for Nucleo board

    log_init ();                                                            // initilize logger on uart

#if SAVE_RAM == 0
    irmp_init ();                                                           // initialize IRMP
#endif
    timer2_init ();                                                         // initialize timer2 for IRMP, EEPROM
    delay_init (DELAY_RESOLUTION_1_US);                                     // initialize delay functions with granularity of 1 us
    board_led_init ();                                                      // initialize GPIO for green LED on disco or nucleo board
    button_init ();                                                         // initialize GPIO for user button on disco or nucleo board
    rtc_init ();                                                            // initialize I2C RTC
    eeprom_init ();                                                         // initialize I2C EEPROM

    if (button_pressed ())                                                  // set ESP8266 into flash mode
    {
        board_led_on ();
        esp8266_flash ();
    }

    log_msg ("\r\nWelcome to WordClock Logger!");
    log_msg ("----------------------------");

    if (rtc_is_up)
    {
        log_msg ("rtc is online");
    }
    else
    {
        log_msg ("rtc is offline");
    }

    if (eeprom_is_up)
    {
        log_msg ("eeprom is online");
        read_version_from_eeprom ();

        if (eeprom_version == EEPROM_VERSION)
        {
#if SAVE_RAM == 0
            remote_ir_read_codes_from_eeprom ();
#endif
            display_read_config_from_eeprom ();
            timeserver_read_data_from_eeprom ();
        }
    }
    else
    {
        log_msg ("eeprom is offline");
    }

    ldr_init ();                                                            // initialize LDR (ADC)
    display_init ();                                                        // initialize display

    short_isr = 1;
    temp_init ();                                                           // initialize DS18xx
    short_isr = 0;

    display_reset_led_states ();
    display_mode                = display_get_display_mode ();
    animation_mode              = display_get_animation_mode ();
    brightness_control_per_ldr  = display_get_automatic_brightness_control ();

    if (eeprom_is_up)
    {
        if (eeprom_version != EEPROM_VERSION)
        {
            log_msg ("Initializing EEPROM...");

            eeprom_version = EEPROM_VERSION;
            write_version_to_eeprom ();
#if SAVE_RAM == 0
            remote_ir_write_codes_to_eeprom ();
#endif
            display_write_config_to_eeprom ();
            timeserver_write_data_to_eeprom ();
        }
    }

    ds3231_flag = 1;

#if SAVE_RAM == 0
    stop_time = uptime + 3;                                                 // wait 3 seconds for IR signal...
    display_set_status_led (1, 1, 1);                                       // show white status LED

    while (uptime < stop_time)
    {
        if (irmp_get_data (&irmp_data))                                     // got IR signal?
        {
            display_set_status_led (1, 0, 0);                               // yes, show red status LED
            delay_sec (1);                                                  // and wait 1 second
            (void) irmp_get_data (&irmp_data);                              // flush input of IRMP now
            display_set_status_led (0, 0, 0);                               // and switch status LED off

            if (remote_ir_learn ())                                         // learn IR commands
            {
                remote_ir_write_codes_to_eeprom ();                         // if successful, save them in EEPROM
            }
            break;                                                          // and break the loop
        }
    }
#endif

    display_set_status_led (0, 0, 0);                                       // switch off status LED

    esp8266_init ();
    esp8266_infop = esp8266_get_info ();

    while (1)
    {
        if (! ap_mode && esp8266_is_up && button_pressed ())                // if user pressed user button, set ESP8266 to AP mode
        {
            ap_mode = 1;
            log_msg ("User button pressed: configuring ESP8266 as access point");
            esp8266_is_online = 0;
            esp8266_infop->is_online = 0;
            esp8266_infop->ipaddress[0] = '\0';
            esp8266_accesspoint ("wordclock", "1234567890");
        }


        if (status_led_cnt)
        {
            status_led_cnt--;

            if (! status_led_cnt)
            {
                display_set_status_led (0, 0, 0);
            }
        }

        if ((code = listener (&lis)) != 0)
        {
            display_set_status_led (1, 0, 0);                               // got net command, light red status LED
            status_led_cnt = STATUS_LED_FLASH_TIME;

            switch (code)
            {
                case LISTENER_SET_COLOR_CODE:                               // set color
                {
                    display_set_colors (&(lis.rgb));
                    log_printf ("command: set colors to %d %d %d\r\n", lis.rgb.red, lis.rgb.green, lis.rgb.blue);
                    break;
                }

                case LISTENER_POWER_CODE:                                   // power on/off
                {
                    if (power_is_on != lis.power)
                    {
                        power_is_on = lis.power;
                        display_flag = DISPLAY_FLAG_UPDATE_ALL;
                        log_msg ("command: set power");
                    }
                    break;
                }

                case LISTENER_DISPLAY_MODE_CODE:                            // set display mode
                {
                    if (display_mode != lis.mode)
                    {
                        display_mode = display_set_display_mode (lis.mode);
                        display_flag = DISPLAY_FLAG_UPDATE_ALL;
                        log_printf ("command: set display mode to %d\r\n", display_mode);
                    }
                    break;
                }

                case LISTENER_ANIMATION_MODE_CODE:                          // set animation mode
                {
                    if (animation_mode != lis.mode)
                    {
                        animation_mode = display_set_animation_mode (lis.mode);
                        animation_flag = 0;
                        display_flag = DISPLAY_FLAG_UPDATE_ALL;
                        log_printf ("command: set animation mode to %d\r\n", animation_flag);
                    }
                    break;
                }

                case LISTENER_DISPLAY_TEMPERATURE_CODE:                     // set animation mode
                {
                    show_temperature = 1;
                    log_msg ("command: show temperature");
                    break;
                }

                case LISTENER_SET_BRIGHTNESS_CODE:                          // set brightness
                {
                    if (brightness_control_per_ldr)
                    {
                        brightness_control_per_ldr = 0;
                        last_ldr_value = 0xFF;
                        display_set_automatic_brightness_control (brightness_control_per_ldr);
                    }
                    display_set_brightness (lis.brightness);
                    display_flag = DISPLAY_FLAG_UPDATE_NO_ANIMATION;
                    log_printf ("command: set brightness to %d, disable autmomatic brightness control per LDR\r\n", lis.brightness);
                    break;
                }

                case LISTENER_SET_AUTOMATIC_BRIHGHTNESS_CODE:               // automatic brightness control on/off
                {
                    if (lis.automatic_brightness_control)
                    {
                        if (ldr_is_up)
                        {
                            brightness_control_per_ldr = 1;
                            log_msg ("command: enable automatic brightness control");
                        }
                        else
                        {
                            log_msg ("command: enable automatic brightness control, but will fail: LDR is off");
                        }
                    }
                    else
                    {
                        brightness_control_per_ldr = 0;
                        log_msg ("command: disable automatic brightness control");
                    }

                    last_ldr_value = 0xFF;
                    display_set_automatic_brightness_control (brightness_control_per_ldr);
                    break;
                }

                case LISTENER_TEST_DISPLAY_CODE:                            // test display
                {
                    log_msg ("command: start display test");
                    display_test ();
                    break;
                }

                case LISTENER_SET_DATE_TIME_CODE:                           // set date/time
                {
                    if (rtc_is_up)
                    {
                        rtc_set_date_time (&(lis.tm));
                    }

                    if (hour != (uint_fast8_t) lis.tm.tm_hour || minute != (uint_fast8_t) lis.tm.tm_min)
                    {
                        display_flag = DISPLAY_FLAG_UPDATE_ALL;
                    }

                    hour   = lis.tm.tm_hour;
                    minute = lis.tm.tm_min;
                    second = lis.tm.tm_sec;

                    log_printf ("command: set time to %4d-%02d-%02d %02d:%02d:%02d\r\n",
                                lis.tm.tm_year + 1900, lis.tm.tm_mon + 1, lis.tm.tm_mday,
                                lis.tm.tm_hour, lis.tm.tm_min, lis.tm.tm_sec);
                    break;
                }

                case LISTENER_GET_NET_TIME_CODE:                            // get net time
                {
                    net_time_flag = 1;
                    log_msg ("command: start net time request");
                    break;
                }

                case LISTENER_IR_LEARN_CODE:                                // IR learn
                {
#if SAVE_RAM == 0
                    log_msg ("command: learn IR codes");

                    if (remote_ir_learn ())
                    {
                        remote_ir_write_codes_to_eeprom ();
                    }
#endif
                    break;
                }

                case LISTENER_SAVE_DISPLAY_CONFIGURATION:                   // save display configuration
                {
                    display_write_config_to_eeprom ();
                    log_msg ("command: save display settings");
                    break;
                }
            }
        }

        if (ldr_is_up && brightness_control_per_ldr && ldr_poll_brightness (&ldr_value))
        {
            if (ldr_value + 1 < last_ldr_value || ldr_value > last_ldr_value + 1)           // difference greater than 2
            {
                log_printf ("ldr: old brightnes: %d new brightness: %d\r\n", last_ldr_value, ldr_value);
                last_ldr_value = ldr_value;
                display_set_brightness (ldr_value);
                display_flag = DISPLAY_FLAG_UPDATE_NO_ANIMATION;
            }
        }

        if (!esp8266_is_up)                                                 // esp8266 up yet?
        {
            if (esp8266_infop->is_up)
            {
                esp8266_is_up = 1;
                log_msg ("ESP8266 now up");
            }
        }
        else
        {                                                                   // esp8266 is up...
            if (! esp8266_is_online)                                        // but not online yet...
            {
                if (esp8266_infop->is_online)                               // now online?
                {
                    char buf[32];
                    esp8266_is_online = 1;

                    log_msg ("ESP8266 now online");
                    sprintf (buf, "  IP %s", esp8266_infop->ipaddress);
                    display_banner (buf);
                    display_flag = DISPLAY_FLAG_UPDATE_ALL;

                    net_time_flag = 1;
                }
            }
        }

        if (ds3231_flag)
        {
            if (rtc_is_up)
            {
                if (rtc_get_date_time (&tm))
                {
                    if (hour != (uint_fast8_t) tm.tm_hour || minute != (uint_fast8_t) tm.tm_min)
                    {
                        display_flag = DISPLAY_FLAG_UPDATE_ALL;
                    }

                    hour            = tm.tm_hour;
                    minute          = tm.tm_min;
                    second          = tm.tm_sec;

                    log_printf ("read rtc: %4d-%02d-%02d %02d:%02d:%02d\r\n",
                                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
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
            if (esp8266_infop->is_online)
            {
                display_set_status_led (0, 0, 1);                       // light blue status LED
                status_led_cnt = STATUS_LED_FLASH_TIME;
                timeserver_start_timeserver_request ();                 // start a timeserver request, answer follows...
            }

            net_time_flag = 0;
            net_time_countdown = 3800;                                  // next net time after 3800 sec
        }

        if (show_time_flag)                                             // set every full minute
        {
#if WCLOCK24H == 1
            display_flag = DISPLAY_FLAG_UPDATE_ALL;
#else
            if (minute % 5)
            {
                display_flag = DISPLAY_FLAG_UPDATE_MINUTES;             // only update minute LEDs
            }
            else
            {
                display_flag = DISPLAY_FLAG_UPDATE_ALL;
            }
#endif
            show_time_flag = 0;
        }

        if (power_is_on)
        {
            if (night_power_is_on)
            {
                if (hour_night_off >= (int_fast16_t) hour && minute_night_off >= (int_fast16_t) minute)
                {
                    night_power_is_on = 0;
                    display_flag = DISPLAY_FLAG_UPDATE_ALL;
                }
            }
            else
            {
                if (hour_night_on >= (int_fast16_t) hour && minute_night_on >= (int_fast16_t) minute)
                {
                    night_power_is_on = 1;
                    display_flag = DISPLAY_FLAG_UPDATE_ALL;
                }
            }
        }

        if (show_temperature)
        {
            uint_fast8_t temperature_index_ds;
            uint_fast8_t temperature_index_rtc;
            uint_fast8_t temperature_index = 0xFF;
            show_temperature = 0;

            log_msg ("show temperature");

            if (ds18xx_is_up)
            {
                short_isr = 1;
                temperature_index_ds = temp_read_temp_index ();
                temperature_index = temperature_index_ds;
                short_isr = 0;
            }
            else
            {
                temperature_index_ds = 0xFF;
            }

            if (rtc_is_up)
            {
                temperature_index_rtc = rtc_get_temperature_index ();

                if (temperature_index != 0xFF)
                {
                    temperature_index = temperature_index_rtc;
                }
            }
            else
            {
                temperature_index_rtc = 0xFF;
            }

            if (temperature_index != 0xFF)
            {
                if (temperature_index_ds != 0xFF)
                {
                    log_msg ("got temperature from DS18xxx");
                }

                if (temperature_index_rtc != 0xFF)
                {
                    log_msg ("got temperature from RTC");
                }

                display_temperature (power_is_on ? night_power_is_on : power_is_on, temperature_index);

#if WCLOCK24H == 1                                                          // WC24H shows temperature with animation, WC12H rolls itself
                uint32_t        stop_time;

                stop_time = uptime + 5;

                while (uptime < stop_time)
                {
                    if (animation_flag)
                    {
                        animation_flag = 0;
                        display_animation ();
                    }
                }
#endif
                display_flag = DISPLAY_FLAG_UPDATE_ALL;                     // force update
            }
        }

        if (display_flag)                                                   // refresh display (time/mode changed)
        {
            log_msg ("update display");

            uint_fast8_t on = power_is_on ? night_power_is_on : power_is_on;

            display_clock (on, hour, minute, display_flag);                 // show new time
            display_flag        = DISPLAY_FLAG_NONE;
        }

        if (animation_flag)
        {
            animation_flag = 0;
            display_animation ();
        }

#if SAVE_RAM == 0
        cmd = remote_ir_get_cmd ();                             // get IR command

        if (cmd != REMOTE_IR_CMD_INVALID)                       // got IR command, light green LED
        {
            display_set_status_led (1, 0, 0);
            status_led_cnt = STATUS_LED_FLASH_TIME;
        }

        if (cmd != REMOTE_IR_CMD_INVALID)                       // if command valid, print command code (mcurses)
        {
            switch (cmd)
            {
                case REMOTE_IR_CMD_POWER:                         log_msg ("IRMP: POWER key");                     break;
                case REMOTE_IR_CMD_OK:                            log_msg ("IRMP: OK key");                        break;
                case REMOTE_IR_CMD_DECREMENT_DISPLAY_MODE:        log_msg ("IRMP: decrement display mode");        break;
                case REMOTE_IR_CMD_INCREMENT_DISPLAY_MODE:        log_msg ("IRMP: increment display mode");        break;
                case REMOTE_IR_CMD_DECREMENT_ANIMATION_MODE:      log_msg ("IRMP: decrement animation mode");      break;
                case REMOTE_IR_CMD_INCREMENT_ANIMATION_MODE:      log_msg ("IRMP: increment animation mode");      break;
                case REMOTE_IR_CMD_DECREMENT_HOUR:                log_msg ("IRMP: decrement minute");              break;
                case REMOTE_IR_CMD_INCREMENT_HOUR:                log_msg ("IRMP: increment hour");                break;
                case REMOTE_IR_CMD_DECREMENT_MINUTE:              log_msg ("IRMP: decrement minute");              break;
                case REMOTE_IR_CMD_INCREMENT_MINUTE:              log_msg ("IRMP: increment minute");              break;
                case REMOTE_IR_CMD_DECREMENT_BRIGHTNESS_RED:      log_msg ("IRMP: decrement red brightness");      break;
                case REMOTE_IR_CMD_INCREMENT_BRIGHTNESS_RED:      log_msg ("IRMP: increment red brightness");      break;
                case REMOTE_IR_CMD_DECREMENT_BRIGHTNESS_GREEN:    log_msg ("IRMP: decrement green brightness");    break;
                case REMOTE_IR_CMD_INCREMENT_BRIGHTNESS_GREEN:    log_msg ("IRMP: increment green brightness");    break;
                case REMOTE_IR_CMD_DECREMENT_BRIGHTNESS_BLUE:     log_msg ("IRMP: decrement blue brightness");     break;
                case REMOTE_IR_CMD_INCREMENT_BRIGHTNESS_BLUE:     log_msg ("IRMP: increment blue brightness");     break;
                case REMOTE_IR_CMD_DECREMENT_BRIGHTNESS:          log_msg ("IRMP: decrement brightness");          break;
                case REMOTE_IR_CMD_INCREMENT_BRIGHTNESS:          log_msg ("IRMP: increment brightness");          break;
                case REMOTE_IR_CMD_GET_TEMPERATURE:               log_msg ("IRMP: get temperature");               break;
            }
        }

        switch (cmd)
        {
            case REMOTE_IR_CMD_POWER:
            {
                power_is_on = power_is_on ? 0 : 1;

                display_flag        = DISPLAY_FLAG_UPDATE_ALL;
                break;
            }

            case REMOTE_IR_CMD_OK:
            {
                display_write_config_to_eeprom ();
                break;
            }

            case REMOTE_IR_CMD_DECREMENT_DISPLAY_MODE:                      // decrement display mode
            {
                display_mode = display_decrement_display_mode ();
                display_flag = DISPLAY_FLAG_UPDATE_ALL;
                break;
            }

            case REMOTE_IR_CMD_INCREMENT_DISPLAY_MODE:                      // increment display mode
            {
                display_mode = display_increment_display_mode ();
                display_flag = DISPLAY_FLAG_UPDATE_ALL;
                break;
            }

            case REMOTE_IR_CMD_DECREMENT_ANIMATION_MODE:                    // decrement display mode
            {
                animation_mode = display_decrement_animation_mode ();
                display_flag = DISPLAY_FLAG_UPDATE_ALL;
                break;
            }

            case REMOTE_IR_CMD_INCREMENT_ANIMATION_MODE:                    // increment display mode
            {
                animation_mode = display_increment_animation_mode ();
                display_flag = DISPLAY_FLAG_UPDATE_ALL;
                break;
            }

            case REMOTE_IR_CMD_DECREMENT_HOUR:                              // decrement hour
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
                display_flag    = DISPLAY_FLAG_UPDATE_ALL;
                time_changed    = 1;
                break;
            }

            case REMOTE_IR_CMD_INCREMENT_HOUR:                              // increment hour
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
                display_flag    = DISPLAY_FLAG_UPDATE_ALL;
                time_changed    = 1;
                break;
            }

            case REMOTE_IR_CMD_DECREMENT_MINUTE:                            // decrement minute
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
                display_flag    = DISPLAY_FLAG_UPDATE_ALL;
                time_changed    = 1;
                break;
            }

            case REMOTE_IR_CMD_INCREMENT_MINUTE:                            // increment minute
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
                display_flag    = DISPLAY_FLAG_UPDATE_ALL;
                time_changed    = 1;
                break;
            }

            case REMOTE_IR_CMD_DECREMENT_BRIGHTNESS_RED:                    // decrement red brightness
            {
                display_decrement_color_red ();
                display_flag        = DISPLAY_FLAG_UPDATE_NO_ANIMATION;
                break;
            }

            case REMOTE_IR_CMD_INCREMENT_BRIGHTNESS_RED:                    // increment red brightness
            {
                display_increment_color_red ();
                display_flag        = DISPLAY_FLAG_UPDATE_NO_ANIMATION;
                break;
            }

            case REMOTE_IR_CMD_DECREMENT_BRIGHTNESS_GREEN:                  // decrement green brightness
            {
                display_decrement_color_green ();
                display_flag        = DISPLAY_FLAG_UPDATE_NO_ANIMATION;
                break;
            }

            case REMOTE_IR_CMD_INCREMENT_BRIGHTNESS_GREEN:                  // increment green brightness
            {
                display_increment_color_green ();
                display_flag        = DISPLAY_FLAG_UPDATE_NO_ANIMATION;
                break;
            }

            case REMOTE_IR_CMD_DECREMENT_BRIGHTNESS_BLUE:                   // decrement blue brightness
            {
                display_decrement_color_blue ();
                display_flag        = DISPLAY_FLAG_UPDATE_NO_ANIMATION;
                break;
            }

            case REMOTE_IR_CMD_INCREMENT_BRIGHTNESS_BLUE:                   // increment blue brightness
            {
                display_increment_color_blue ();
                display_flag        = DISPLAY_FLAG_UPDATE_NO_ANIMATION;
                break;
            }

            case REMOTE_IR_CMD_AUTO_BRIGHTNESS_CONTROL:
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
                display_set_automatic_brightness_control (brightness_control_per_ldr);
                display_flag = DISPLAY_FLAG_UPDATE_NO_ANIMATION;
                break;
            }

            case REMOTE_IR_CMD_DECREMENT_BRIGHTNESS:                            // decrement brightness
            {
                if (brightness_control_per_ldr)
                {
                    brightness_control_per_ldr = 0;
                    last_ldr_value = 0xFF;
                    display_set_automatic_brightness_control (brightness_control_per_ldr);
                    display_flag = DISPLAY_FLAG_UPDATE_NO_ANIMATION;
                }

                display_decrement_brightness ();
                display_flag = DISPLAY_FLAG_UPDATE_NO_ANIMATION;
                break;
            }

            case REMOTE_IR_CMD_INCREMENT_BRIGHTNESS:                        // increment brightness
            {
                if (brightness_control_per_ldr)
                {
                    brightness_control_per_ldr = 0;
                    last_ldr_value = 0xFF;
                    display_set_automatic_brightness_control (brightness_control_per_ldr);
                    display_flag = DISPLAY_FLAG_UPDATE_NO_ANIMATION;
                }

                display_increment_brightness ();
                display_flag = DISPLAY_FLAG_UPDATE_NO_ANIMATION;
                break;
            }

            case REMOTE_IR_CMD_GET_TEMPERATURE:                             // get temperature
            {
                show_temperature    = 1;
                break;
            }

            default:
            {
                break;
            }
        }
#endif // SAVE_RAM == 0

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

