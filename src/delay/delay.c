/*-------------------------------------------------------------------------------------------------------------------------------------------
 * delay.c - delay functions
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#include <stdint.h>
#include "delay.h"

static uint_fast8_t         resolution;
static uint32_t             usec_divider    = 1;
static uint32_t             msec_factor     = 1000;

volatile uint32_t           delay_counter;

void SysTick_Handler(void)
{
    if (delay_counter > 0)
    {
        delay_counter--;
    }
}

void
delay_usec (uint32_t usec)
{
    if (resolution == DELAY_RESOLUTION_1_US)
    {
        delay_counter = usec;
    }
    else
    {
        delay_counter = usec / usec_divider;
    }

    while (delay_counter != 0)
    {
        ;
    }
}

void
delay_msec (uint32_t msec)
{
    delay_counter = msec * msec_factor;

    while (delay_counter != 0)
    {
        ;
    }
}

void
delay_sec (uint32_t sec)
{
    while (sec--)
    {
        delay_msec (1000);
    }
}

void
delay_init (uint_fast8_t res)
{
    uint32_t    divider;

    switch (res)
    {
        case DELAY_RESOLUTION_1_US:
            divider         = 1000000;                          // one Tick = 1 / 1000000 =   1us
            usec_divider    = 1;
            msec_factor     = 1000;
            break;
        case DELAY_RESOLUTION_10_US:
            divider         = 100000;                           // one Tick = 1 /  100000 =  10us
            usec_divider    = 10;
            msec_factor     = 100;
            break;
        default:
            res = DELAY_RESOLUTION_100_US;
            // no break!
        case DELAY_RESOLUTION_100_US:
            divider         = 10000;                           // one Tick = 1 /   10000 = 100us
            usec_divider    = 100;
            msec_factor     = 10;
            break;
    }

    SysTick_Config (SystemCoreClock / divider);
    resolution = res;
}
