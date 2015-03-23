/*-------------------------------------------------------------------------------------------------------------------------------------------
 * green-led.c - LED routines
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */

#if defined (STM32F10X)
#include "stm32f10x.h"
#elif defined (STM32F4XX)
#include "stm32f4xx.h"
#endif
#include "led-green.h"

#if defined (STM32F407VG)                                       // STM32F4 Discovery Board PD12
#define LED_GREEN_PERIPH_CLOCK_CMD  RCC_AHB1PeriphClockCmd
#define LED_GREEN_PERIPH            RCC_AHB1Periph_GPIOD
#define LED_GREEN_PORT              GPIOD
#define LED_GREEN_LED               GPIO_Pin_12
#define LED_GREEN_ON                SET
#define LED_GREEN_OFF               RESET

#elif defined (STM32F401RE) || defined (STM32F411RE)            // STM32F401/STM32F411 Nucleo Board PA5
#define LED_GREEN_PERIPH_CLOCK_CMD  RCC_AHB1PeriphClockCmd
#define LED_GREEN_PERIPH            RCC_AHB1Periph_GPIOA
#define LED_GREEN_PORT              GPIOA
#define LED_GREEN_LED               GPIO_Pin_5
#define LED_GREEN_ON                SET
#define LED_GREEN_OFF               RESET

#elif defined (STM32F103)
#define LED_GREEN_PERIPH_CLOCK_CMD  RCC_APB2PeriphClockCmd
#define LED_GREEN_PERIPH            RCC_APB2Periph_GPIOC
#define LED_GREEN_PORT              GPIOC
#define LED_GREEN_LED               GPIO_Pin_13
#define LED_GREEN_ON                RESET
#define LED_GREEN_OFF               SET

#else
#error STM32 unknown
#endif

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * initialize LED port
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
led_green_init (void)
{
    GPIO_InitTypeDef gpio;

    GPIO_StructInit (&gpio);
    LED_GREEN_PERIPH_CLOCK_CMD (LED_GREEN_PERIPH, ENABLE);     // enable clock for LED Port

    gpio.GPIO_Pin   = LED_GREEN_LED;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;

#if defined (STM32F10X)
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
#elif defined (STM32F4XX)
    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
#endif

    GPIO_Init(LED_GREEN_PORT, &gpio);
    led_green_off ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * green LED on
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
led_green_on (void)
{
    GPIO_WriteBit(LED_GREEN_PORT, LED_GREEN_LED, LED_GREEN_ON);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * green LED off
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
led_green_off (void)
{
    GPIO_WriteBit(LED_GREEN_PORT, LED_GREEN_LED, LED_GREEN_OFF);
}
