/*-------------------------------------------------------------------------------------------------------------------------------------------
 * dsp.c - routines for LED display
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "wclock24h-config.h"
#include "tables.h"
#include "dsp.h"
#include "irmp.h"
#include "ws2812.h"
#include "eeprom.h"
#include "eeprom-data.h"
#include "delay.h"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

HERE IS A SHORT PROGRAM TO CALCULATE THE PWM TABLE:

//-----------------------------------------------------------------------------------------------------------------------------
// pwmtable[i] = floor(maxvalue * ((i / (steps - 1)) ^ (1/gamma))w + 0.5)
//
// See also:
// http://www.mikrocontroller.net/articles/Diskussion:LED-Fading#Diskussion_wissenschaftl.-technischer_Hintergrund
// http://www.maxim-ic.com/app-notes/index.mvp/id/3667
//
// Compile it with: cc gamma.c -o gamma -lm
//-----------------------------------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <math.h>

#define STEPS       (64)            // CHANGE HERE
#define PWMBITS     (8)             // CHANGE HERE

// GAMMA:
// 0.5   für punktförmige oder aufblitzende Helligkeiten
// 0.33  für Lichtquellen bei 5° Blickwinkel
// 1/2.2 für diffus strahlende LEDs - entspricht der Gammakorrektur von VGA-Bildschirmen
#define GAMMA       (1/2.2)

int main ()
{
    int     maxvalue;
    double  value;
    int     i;

    maxvalue = (1 << PWMBITS) - 1;

    for (i = 0; i < STEPS; i++)
    {
        value = floor ((double) maxvalue * pow ((double) i / (double) (STEPS - 1), 1/(GAMMA)) + 0.5);
        if (i > 0 && value < 1)
        {
            value = 1;
        }

        printf ("%5.0f", value);

        if (i < STEPS - 1)
        {
            putchar (',');
            putchar (' ');
        }

        if (!((i + 1) % 8))
        {
            putchar ('\n');
        }
    }
}

 *+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#define MAX_COLOR_STEPS     64


/*-------------------------------------------------------------------------------------------------------------------------------------------
 * 8-Bit PWM with 64 different settings:
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static const uint16_t pwmtable8[MAX_COLOR_STEPS]  =
{
    0,     1,     1,     2,     2,     3,     3,     4,
    4,     5,     5,     6,     7,     8,     9,    11,
   13,    14,    16,    18,    20,    23,    25,    28,
   31,    33,    36,    40,    43,    46,    50,    54,
   57,    61,    66,    70,    74,    79,    84,    89,
   94,    99,   105,   110,   116,   122,   128,   134,
  140,   147,   153,   160,   167,   174,   182,   189,
  197,   205,   213,   221,   229,   238,   246,   255
};

static DSP_COLORS   current_colors =
{
    MAX_COLOR_STEPS / 2, 0, 0
};

static DSP_COLORS   dimmed_colors =
{
    MAX_COLOR_STEPS / 2, 0, 0
};

static uint_fast8_t                 display_mode;

static DSP_COLORS                   dimmed_colors_up;
static DSP_COLORS                   dimmed_colors_down;

#define CURRENT_STATE               0x01
#define TARGET_STATE                0x02
#define NEW_STATE                   0x04
#define CALC_STATE                  0x08

#define ANIMATION_MODE_NONE         0
#define ANIMATION_MODE_FADE         1
#define ANIMATION_MODE_ROLL         2
#define ANIMATION_MODE_EXPLODE      3
#define ANIMATION_MODE_RANDOM       4
#define ANIMATION_MODES             5

char *                              animation_modes[ANIMATION_MODES] =
{
    "None",
    "Fade",
    "Roll",
    "Explode",
    "Random"
};

static uint_fast8_t                 last_display_mode   = 0xff;

static uint_fast8_t                 animation_mode = ANIMATION_MODE_FADE;
static uint_fast8_t                 animation_start_flag;
static uint_fast8_t                 animation_stop_flag;

union
{
    uint8_t                         state[WS2812_LEDS];
    uint8_t                         matrix[WC_ROWS][WC_COLUMNS];
} led;

static uint_fast8_t                 red_step;
static uint_fast8_t                 green_step;
static uint_fast8_t                 blue_step;

void
dsp_set_led0 (uint_fast8_t r_flag, uint_fast8_t g_flag, uint_fast8_t b_flag)
{
    WS2812_RGB rgb;

    rgb.red     = r_flag ? pwmtable8[MAX_COLOR_STEPS - 1] : 0;
    rgb.green   = g_flag ? pwmtable8[MAX_COLOR_STEPS - 1] : 0;
    rgb.blue    = b_flag ? pwmtable8[MAX_COLOR_STEPS - 1] : 0;

    ws2812_set_led (0, &rgb, 1);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set LED to RGB
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t    brightness = 15;

static void
dsp_set_led (uint_fast16_t n, WS2812_RGB * rgb, uint_fast8_t refresh)
{
	if (n < WS2812_LEDS)
	{
        uint8_t y;
        uint8_t x;

        y = n / WC_COLUMNS;

        if (y & 0x01)                                       // snake: odd row: count from right to left
        {
            x = n % WC_COLUMNS;
            n = y * WC_COLUMNS + (WC_COLUMNS - 1 - x);
        }

        ws2812_set_led (n + 1, rgb, refresh);
	}
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * switch all LEDs off
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
reset_led_states (void)
{
    uint_fast16_t     idx;

    for (idx = 0; idx < WS2812_LEDS; idx++)
    {
        if (led.state[idx] & TARGET_STATE)
        {
            led.state[idx] = CURRENT_STATE;
        }
        else
        {
            led.state[idx] = 0;
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * display one LED
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_led_on (uint_fast8_t y, uint_fast8_t x)
{
    led.matrix[y][x] |= TARGET_STATE;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * display one word
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_word_on (uint_fast8_t idx)
{
    uint_fast8_t y = illumination[0][idx].row;
    uint_fast8_t x = illumination[0][idx].col;
    uint_fast8_t l = illumination[0][idx].len;

    while (l--)
    {
        dsp_led_on (y, x);
        x++;
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * flush animation
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_animation_flush (void)
{
    WS2812_RGB      rgb;
    WS2812_RGB      rgb0;
    uint_fast16_t    idx;

    rgb.red         = pwmtable8[dimmed_colors.red];
    rgb.green       = pwmtable8[dimmed_colors.green];
    rgb.blue        = pwmtable8[dimmed_colors.blue];

    rgb0.red        = 0;
    rgb0.green      = 0;
    rgb0.blue       = 0;

    for (idx = 0; idx < WS2812_LEDS; idx++)
    {
        if (led.state[idx] & TARGET_STATE)                          // fm: == ???
        {
            dsp_set_led (idx, &rgb, 0);
            led.state[idx] |= CURRENT_STATE;                        // we are in sync
        }
        else
        {
            dsp_set_led (idx, &rgb0, 0);
            led.state[idx] &= ~CURRENT_STATE;                       // we are in sync
        }
    }

    ws2812_refresh();
    animation_stop_flag = 1;
}

static void
dsp_animation_none (void)
{
    if (animation_start_flag)
    {
        animation_start_flag = 0;
        animation_stop_flag = 0;
        dsp_animation_flush ();
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * fade
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_animation_fade (void)
{
    uint_fast16_t    idx;
    uint_fast8_t     changed = 0;

    if (animation_start_flag)
    {
        animation_start_flag = 0;
        animation_stop_flag = 0;

        red_step = dimmed_colors.red / 5;

        if (red_step == 0 && dimmed_colors.red > 0)
        {
            red_step = 1;
        }

        green_step = dimmed_colors.green / 5;

        if (green_step == 0 && dimmed_colors.green > 0)
        {
            green_step = 1;
        }

        blue_step = dimmed_colors.blue / 5;

        if (blue_step == 0 && dimmed_colors.blue > 0)
        {
            blue_step = 1;
        }

        dimmed_colors_up.red       = 0;
        dimmed_colors_up.green     = 0;
        dimmed_colors_up.blue      = 0;

        dimmed_colors_down.red     = dimmed_colors.red;
        dimmed_colors_down.green   = dimmed_colors.green;
        dimmed_colors_down.blue    = dimmed_colors.blue;
    }

    if (! animation_stop_flag)
    {
        if (dimmed_colors_down.red >= red_step)
        {
            dimmed_colors_down.red -= red_step;
            changed = 1;
        }
        else if (dimmed_colors_down.red != 0)
        {
            dimmed_colors_down.red = 0;
            changed = 1;
        }

        if (dimmed_colors_down.green >= green_step)
        {
            dimmed_colors_down.green -= green_step;
            changed = 1;
        }
        else if (dimmed_colors_down.green != 0)
        {
            dimmed_colors_down.green = 0;
            changed = 1;
        }

        if (dimmed_colors_down.blue >= blue_step)
        {
            dimmed_colors_down.blue -= blue_step;
            changed = 1;
        }
        else if (dimmed_colors_down.blue != 0)
        {
            dimmed_colors_down.blue = 0;
            changed = 1;
        }

        if (dimmed_colors_up.red + red_step <= dimmed_colors.red)
        {
            dimmed_colors_up.red += red_step;
            changed = 1;
        }
        else if (dimmed_colors_up.red != dimmed_colors.red)
        {
            dimmed_colors_up.red = dimmed_colors.red;
            changed = 1;
        }

        if (dimmed_colors_up.green + green_step <= dimmed_colors.green)
        {
            dimmed_colors_up.green += green_step;
            changed = 1;
        }
        else if (dimmed_colors_up.green != dimmed_colors.green)
        {
            dimmed_colors_up.green = dimmed_colors.green;
            changed = 1;
        }

        if (dimmed_colors_up.blue + blue_step <= dimmed_colors.blue)
        {
            dimmed_colors_up.blue += blue_step;
            changed = 1;
        }
        else if (dimmed_colors_up.blue != dimmed_colors.blue)
        {
            dimmed_colors_up.blue = dimmed_colors.blue;
            changed = 1;
        }

        if (changed)
        {
            WS2812_RGB  rgb;
            WS2812_RGB  rgb_up;
            WS2812_RGB  rgb_down;

            rgb.red         = pwmtable8[dimmed_colors.red];
            rgb.green       = pwmtable8[dimmed_colors.green];
            rgb.blue        = pwmtable8[dimmed_colors.blue];

            rgb_up.red      = pwmtable8[dimmed_colors_up.red];
            rgb_up.green    = pwmtable8[dimmed_colors_up.green];
            rgb_up.blue     = pwmtable8[dimmed_colors_up.blue];

            rgb_down.red    = pwmtable8[dimmed_colors_down.red];
            rgb_down.green  = pwmtable8[dimmed_colors_down.green];
            rgb_down.blue   = pwmtable8[dimmed_colors_down.blue];

            for (idx = 0; idx < WS2812_LEDS; idx++)
            {
                if (led.state[idx] == TARGET_STATE)                           // up
                {
                    dsp_set_led (idx, &rgb_up, 0);
                }
                else if (led.state[idx] == CURRENT_STATE)                     // down
                {
                    dsp_set_led (idx, &rgb_down, 0);
                }
                else if (led.state[idx] == (CURRENT_STATE | TARGET_STATE))    // on, but no change
                {
                    dsp_set_led (idx, &rgb, 0);
                }
            }

            ws2812_refresh();
        }
    }
}

static void
dsp_show_new_display (void)
{
    WS2812_RGB          rgb;
    WS2812_RGB          rgb0;
    uint_fast16_t       idx;

    rgb.red         = pwmtable8[dimmed_colors.red];
    rgb.green       = pwmtable8[dimmed_colors.green];
    rgb.blue        = pwmtable8[dimmed_colors.blue];

    rgb0.red        = 0;
    rgb0.green      = 0;
    rgb0.blue       = 0;

    for (idx = 0; idx < WS2812_LEDS; idx++)
    {
        if (led.state[idx] & NEW_STATE)                                // on
        {
            dsp_set_led (idx, &rgb, 0);
        }
        else
        {
            dsp_set_led (idx, &rgb0, 0);
        }
    }

    ws2812_refresh();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * roll right
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_animation_roll_right (void)
{
 #if 0
    static uint_fast8_t cnt;
    uint_fast8_t        y;
    uint_fast8_t        x;
#else
    static uint_fast16_t    cnt;
    uint_fast16_t           y;
    uint_fast16_t           x;
#endif

    if (animation_start_flag)
    {
        animation_start_flag = 0;
        animation_stop_flag = 0;
        cnt = 0;
    }

    if (! animation_stop_flag)
    {
        if (cnt < WC_COLUMNS)
        {
            cnt++;                                                  // 1...WC_COLUMNS
#if 0
            for (y = 0; y < WC_ROWS; y++)
            {
                for (x = 0; x < WC_COLUMNS; x++)
                {
                    if (x >= cnt)
                    {
                        if (led.matrix[y][x - cnt] & CURRENT_STATE)
                        {
                            led.matrix[y][x] |= NEW_STATE;
                        }
                        else
                        {
                            led.matrix[y][x] &= ~NEW_STATE;
                        }
                    }
                    else
                    {
                        if (led.matrix[y][x + WC_COLUMNS - cnt] & TARGET_STATE)
                        {
                            led.matrix[y][x] |= NEW_STATE;
                        }
                        else
                        {
                            led.matrix[y][x] &= ~NEW_STATE;
                        }
                    }
                }
            }
#else
            for (y = 0; y < WC_ROWS * WC_COLUMNS; y += WC_COLUMNS)
            {
                for (x = 0; x < WC_COLUMNS; x++)
                {
                    if (x >= cnt)
                    {
                        if (led.state[y + x - cnt] & CURRENT_STATE)
                        {
                            led.state[y + x] |= NEW_STATE;
                        }
                        else
                        {
                            led.state[y + x] &= ~NEW_STATE;
                        }
                    }
                    else
                    {
                        if (led.state[y + x + WC_COLUMNS - cnt] & TARGET_STATE)
                        {
                            led.state[y + x] |= NEW_STATE;
                        }
                        else
                        {
                            led.state[y + x] &= ~NEW_STATE;
                        }
                    }
                }
            }
#endif
            dsp_show_new_display ();
        }
        else
        {
            animation_stop_flag = 1;
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * roll left
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_animation_roll_left ()
{
#if 0
    static uint_fast8_t cnt;
    uint_fast8_t        y;
    uint_fast8_t        x;
#else
    static uint_fast16_t    cnt;
    uint_fast16_t           y;
    uint_fast16_t           x;
#endif

    if (animation_start_flag)
    {
        animation_start_flag = 0;
        animation_stop_flag = 0;
        cnt = 0;
    }

    if (! animation_stop_flag)
    {
        if (cnt < WC_COLUMNS)
        {
            cnt++;                                                  // 1...WC_COLUMNS

#if 0
            for (y = 0; y < WC_ROWS; y++)
            {
                for (x = 0; x < WC_COLUMNS; x++)
                {
                    if (x + cnt < WC_COLUMNS)
                    {
                        if (led.matrix[y][x + cnt] & CURRENT_STATE)
                        {
                            led.matrix[y][x] |= NEW_STATE;
                        }
                        else
                        {
                            led.matrix[y][x] &= ~NEW_STATE;
                        }
                    }
                    else
                    {
                        if (led.matrix[y][x + cnt - WC_COLUMNS] & TARGET_STATE)
                        {
                            led.matrix[y][x] |= NEW_STATE;
                        }
                        else
                        {
                            led.matrix[y][x] &= ~NEW_STATE;
                        }
                    }
                }
            }
#else
            for (y = 0; y < WC_ROWS * WC_COLUMNS; y += WC_COLUMNS)
            {
                for (x = 0; x < WC_COLUMNS; x++)
                {
                    if (x + cnt < WC_COLUMNS)
                    {
                        if (led.state[y + x + cnt] & CURRENT_STATE)
                        {
                            led.state[y + x] |= NEW_STATE;
                        }
                        else
                        {
                            led.state[y + x] &= ~NEW_STATE;
                        }
                    }
                    else
                    {
                        if (led.state[y + x + cnt - WC_COLUMNS] & TARGET_STATE)
                        {
                            led.state[y + x] |= NEW_STATE;
                        }
                        else
                        {
                            led.state[y + x] &= ~NEW_STATE;
                        }
                    }
                }
            }
#endif
            dsp_show_new_display ();
        }
        else
        {
            animation_stop_flag = 1;
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * roll down
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_animation_roll_down ()
{
#if 0
    static uint_fast8_t cnt;
    uint_fast8_t        y;
    uint_fast8_t        x;

    if (animation_start_flag)
    {
        animation_start_flag = 0;
        animation_stop_flag = 0;
        cnt = 0;
    }

    if (! animation_stop_flag)
    {
        if (cnt < WC_ROWS)
        {
            cnt++;                                                  // 1...WC_ROWS

            for (y = 0; y < WC_ROWS; y++)
            {
                for (x = 0; x < WC_COLUMNS; x++)
                {
                    if (y >= cnt)
                    {
                        if (led.matrix[y - cnt][x] & CURRENT_STATE)
                        {
                            led.matrix[y][x] |= NEW_STATE;
                        }
                        else
                        {
                            led.matrix[y][x] &= ~NEW_STATE;
                        }
                    }
                    else
                    {
                        if (led.matrix[y + WC_ROWS - cnt][x] & TARGET_STATE)
                        {
                            led.matrix[y][x] |= NEW_STATE;
                        }
                        else
                        {
                            led.matrix[y][x] &= ~NEW_STATE;
                        }
                    }
                }
            }
            dsp_show_new_display ();
        }
        else
        {
            animation_stop_flag = 1;
        }
    }
#else
    static uint_fast16_t    cnt;
    uint_fast16_t           y;
    uint_fast16_t           x;

    if (animation_start_flag)
    {
        animation_start_flag = 0;
        animation_stop_flag = 0;
        cnt = 0;
    }

    if (! animation_stop_flag)
    {
        if (cnt < WC_ROWS * WC_COLUMNS)
        {
            cnt += WC_COLUMNS;                                  // (1...WC_ROWS) * WC_COLUMNS

            for (y = 0; y < WC_ROWS * WC_COLUMNS; y += WC_COLUMNS)
            {
                for (x = 0; x < WC_COLUMNS; x++)
                {
                    if (y >= cnt)
                    {
                        if (led.state[y - cnt + x] & CURRENT_STATE)
                        {
                            led.state[y + x] |= NEW_STATE;
                        }
                        else
                        {
                            led.state[y + x] &= ~NEW_STATE;
                        }
                    }
                    else
                    {
                        if (led.state[y + (WC_ROWS * WC_COLUMNS - cnt) + x] & TARGET_STATE)
                        {
                            led.state[y + x] |= NEW_STATE;
                        }
                        else
                        {
                            led.state[y + x] &= ~NEW_STATE;
                        }
                    }
                }
            }
            dsp_show_new_display ();
        }
        else
        {
            animation_stop_flag = 1;
        }
    }
#endif
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * roll up
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_animation_roll_up ()
{
#if 0
    static uint_fast8_t cnt;
    uint_fast8_t        y;
    uint_fast8_t        x;

    if (animation_start_flag)
    {
        animation_start_flag = 0;
        animation_stop_flag = 0;
        cnt = 0;
    }

    if (! animation_stop_flag)
    {
        if (cnt < WC_ROWS)
        {
            cnt++;                                                  // 1...WC_ROWS

            for (y = 0; y < WC_ROWS; y++)
            {
                for (x = 0; x < WC_COLUMNS; x++)
                {
                    if (y + cnt < WC_ROWS)
                    {
                        if (led.matrix[y + cnt][x] & CURRENT_STATE)
                        {
                            led.matrix[y][x] |= NEW_STATE;
                        }
                        else
                        {
                            led.matrix[y][x] &= ~NEW_STATE;
                        }
                    }
                    else
                    {
                        if (led.matrix[y + cnt - WC_ROWS][x] & TARGET_STATE)
                        {
                            led.matrix[y][x] |= NEW_STATE;
                        }
                        else
                        {
                            led.matrix[y][x] &= ~NEW_STATE;
                        }
                    }
                }
            }
            dsp_show_new_display ();
        }
        else
        {
            animation_stop_flag = 1;
        }
    }
#else
    static uint_fast16_t    cnt;
    uint_fast16_t           y;
    uint_fast16_t           x;

    if (animation_start_flag)
    {
        animation_start_flag = 0;
        animation_stop_flag = 0;
        cnt = 0;
    }

    if (! animation_stop_flag)
    {
        if (cnt < WC_ROWS * WC_COLUMNS)
        {
            cnt += WC_COLUMNS;                                  // (1...WC_ROWS) * WC_COLUMNS

            for (y = 0; y < WC_ROWS * WC_COLUMNS; y += WC_COLUMNS)
            {
                for (x = 0; x < WC_COLUMNS; x++)
                {
                    if (y + cnt < WC_ROWS * WC_COLUMNS)
                    {
                        if (led.state[y + cnt + x] & CURRENT_STATE)
                        {
                            led.state[y + x] |= NEW_STATE;
                        }
                        else
                        {
                            led.state[y + x] &= ~NEW_STATE;
                        }
                    }
                    else
                    {
                        if (led.state[y + (cnt - WC_ROWS * WC_COLUMNS) + x] & TARGET_STATE)
                        {
                            led.state[y + x] |= NEW_STATE;
                        }
                        else
                        {
                            led.state[y + x] &= ~NEW_STATE;
                        }
                    }
                }
            }
            dsp_show_new_display ();
        }
        else
        {
            animation_stop_flag = 1;
        }
    }
#endif
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * roll
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_animation_roll ()
{
    static uint32_t        x;

    if (animation_start_flag)
    {
        x = rand () % 4;
    }

    switch (x)
    {
        case 0:     dsp_animation_roll_right ();       break;
        case 1:     dsp_animation_roll_left  ();       break;
        case 2:     dsp_animation_roll_down  ();       break;
        case 3:     dsp_animation_roll_up    ();       break;
    }
}

static void
dsp_animation_calc_implode (int n)
{
    uint_fast8_t    y;
    uint_fast16_t   yi;
    uint_fast8_t    x;
    uint_fast16_t   xi;
    uint_fast8_t    ny;
    uint_fast8_t    nx;

    for (yi = 0; yi < WC_ROWS * WC_COLUMNS; yi += WC_COLUMNS)
    {
        for (xi = 0; xi < WC_COLUMNS; xi++)
        {
            led.state[yi + xi] &= ~CALC_STATE;
        }
    }

    for (y = 0; y < WC_ROWS; y++)
    {
        for (x = 0; x < WC_COLUMNS; x++)
        {
            if (led.matrix[y][x] & TARGET_STATE)
            {
                if (y < WC_ROWS / 2)
                {
                    if (x < WC_COLUMNS / 2)
                    {
                        ny = y + n;
                        nx = x + n;

                        if (ny > WC_ROWS / 2 - 1)
                        {
                            ny = WC_ROWS / 2 - 1;
                        }

                        if (nx > WC_COLUMNS / 2 - 1)
                        {
                            nx = WC_COLUMNS / 2 - 1;
                        }
                    }
                    else
                    {
                        ny = y + n;
                        nx = x - n;

                        if (ny > WC_ROWS / 2 - 1)
                        {
                            ny = WC_ROWS / 2 - 1;
                        }

                        if (nx < WC_COLUMNS / 2)
                        {
                            nx = WC_COLUMNS / 2;
                        }
                    }
                }
                else
                {
                    if (x < WC_COLUMNS / 2)
                    {
                        ny = y - n;
                        nx = x + n;

                        if (ny < WC_ROWS / 2)
                        {
                            ny = WC_ROWS / 2;
                        }

                        if (nx > WC_COLUMNS / 2 - 1)
                        {
                            nx = WC_COLUMNS / 2 - 1;
                        }
                    }
                    else
                    {
                        ny = y - n;
                        nx = x - n;

                        if (ny < WC_ROWS / 2)
                        {
                            ny = WC_ROWS / 2;
                        }

                        if (nx < WC_COLUMNS / 2)
                        {
                            nx = WC_COLUMNS / 2;
                        }
                    }
                }

                led.matrix[ny][nx] |= CALC_STATE;
            }
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * explode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_animation_explode (void)
{
    static uint_fast8_t cnt;
    uint_fast8_t        y;
    uint_fast16_t       yi;
    uint_fast8_t        x;

    if (animation_start_flag)
    {
        animation_start_flag = 0;
        animation_stop_flag = 0;
        cnt = 0;
    }

    if (! animation_stop_flag)
    {
        if (cnt < WC_COLUMNS / 2)
        {
            for (y = 0; y < WC_ROWS; y++)
            {
                for (x = 0; x < WC_COLUMNS; x++)
                {
                    led.matrix[y][x] &= ~NEW_STATE;
                }
            }

            cnt++;

            dsp_animation_calc_implode (WC_COLUMNS / 2 - cnt);

            for (y = 0, yi = 0; y < WC_ROWS; y++, yi += WC_COLUMNS)
            {
                for (x = 0; x < WC_COLUMNS; x++)
                {
                    if (led.matrix[y][x] & CURRENT_STATE)
                    {
                        if (y < WC_ROWS / 2)
                        {
                            if (x < WC_COLUMNS / 2)
                            {
                                if (y >= cnt && x >= cnt)
                                {
                                    led.matrix[y - cnt][x - cnt] |= NEW_STATE;
                                }
                            }
                            else
                            {
                                if (y >= cnt && x + cnt < WC_COLUMNS)
                                {
                                    led.matrix[y - cnt][x + cnt] |= NEW_STATE;
                                }
                            }
                        }
                        else
                        {
                            if (x < WC_COLUMNS / 2)
                            {
                                if (y + cnt < WC_ROWS && x >= cnt)
                                {
                                    led.matrix[y + cnt][x - cnt] |= NEW_STATE;
                                }
                            }
                            else
                            {
                                if (y + cnt < WC_ROWS && x + cnt < WC_COLUMNS)
                                {
                                    led.matrix[y + cnt][x + cnt] |= NEW_STATE;
                                }
                            }
                        }
                    }

                    if (led.state[yi + x] & CALC_STATE)
                    {
                        led.state[yi + x] |= NEW_STATE;        // matrix leds |= calculated leds
                    }
                }
            }

            dsp_show_new_display ();
        }
        else
        {
            animation_stop_flag = 1;
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * random animation
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_animation_random ()
{
    static uint32_t x;

    if (animation_start_flag)
    {
        x = rand () % 6;
    }

    switch (x)
    {
        case 0:     dsp_animation_fade ();          break;
        case 1:     dsp_animation_roll_right ();    break;
        case 2:     dsp_animation_roll_left ();     break;
        case 3:     dsp_animation_roll_down ();     break;
        case 4:     dsp_animation_roll_up ();       break;
        case 5:     dsp_animation_explode ();       break;
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * animation
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_animation (void)
{
    switch (animation_mode)
    {
        case ANIMATION_MODE_FADE:       dsp_animation_fade ();      break;
        case ANIMATION_MODE_ROLL:       dsp_animation_roll ();      break;
        case ANIMATION_MODE_EXPLODE:    dsp_animation_explode ();   break;
        case ANIMATION_MODE_RANDOM:     dsp_animation_random ();    break;
        default:                        dsp_animation_none ();      break;
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * display temperature
 *
 *   index ==   0  ->   0°C
 *   index == 250  -> 125°C
 *
 * the first temperature we can show is 10,0°C (index =  0, temperature_index ==  20)
 * the last  temperature we can show is 39,5°C (index = 79, temperature_index == 159)
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_temperature (uint_fast8_t power_is_on, uint_fast8_t temperature_index)
{
    uint_fast8_t   temp_mode = MODES_COUNT - 1;

    if (temperature_index >= 20 && temperature_index < 80)
    {
        uint8_t                         minute_mode;
        const struct MinuteDisplay *    tbl_minute;
        uint_fast16_t                   idx;

        last_display_mode = temp_mode;

        temperature_index -= 20;                                            // subtract 10°C (20 units)

        // Perhaps not all LEDs have reached the desired colors yet.
        // This happens when we change the time faster than LED fading is.
        // Then we have to flush the target states:

        dsp_animation_flush ();

        // Now all LEDs have the desired colors of last time.
        // We can now set the new values:

        minute_mode = tbl_modes[temp_mode].minute_txt;
        tbl_minute  = &tbl_minutes[minute_mode][temperature_index];

        reset_led_states ();

        if (power_is_on)
        {
            dsp_word_on (WP_ES);
            dsp_word_on (WP_IST);

            for (idx = 0; idx < MAX_MINUTE_WORDS && tbl_minute->wordIdx[idx] != 0; idx++)
            {
                dsp_word_on (tbl_minute->wordIdx[idx]);
            }
        }

        animation_start_flag = 1;
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * display clock time
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_clock (uint_fast8_t power_is_on, uint_fast8_t hour, uint_fast8_t minute)
{
    static uint8_t                  words[WP_COUNT];
    static uint_fast8_t             last_hour           = 0xff;
    static uint_fast8_t             last_minute         = 0xff;
    static uint_fast8_t             last_power_is_on    = 0xff;
    uint8_t                         hour_mode;
    uint8_t                         minute_mode;
    const struct MinuteDisplay *    tbl_minute;
    const uint8_t *                 word_idx_p;
    uint_fast16_t                   idx;

    if (last_display_mode != display_mode || last_hour != hour || last_minute != minute || last_power_is_on != power_is_on)
    {
        last_display_mode       = display_mode;
        last_hour               = hour;
        last_minute             = minute;
        last_power_is_on        = power_is_on;

        // Perhaps not all LEDs have reached the desired colors yet.
        // This happens when we change the time faster than LED fading is.
        // Then we have to flush the target states:

        dsp_animation_flush ();

        // Now all LEDs have the desired colors of last time.
        // We can now set the new values:

        hour_mode   = tbl_modes[display_mode].hour_txt;
        minute_mode = tbl_modes[display_mode].minute_txt;
        tbl_minute  = &tbl_minutes[minute_mode][minute];

        memset (words, 0, WP_COUNT);
        reset_led_states ();

        if (power_is_on)
        {
            words[WP_ES] = 1;
            words[WP_IST] = 1;

            for (idx = 0; idx < MAX_MINUTE_WORDS && tbl_minute->wordIdx[idx] != 0; idx++)
            {
                words[tbl_minute->wordIdx[idx]] = 1;
            }

            hour += tbl_minute->hourOffset;                             // correct the hour offset from the minutes
            word_idx_p = tbl_hours[hour_mode][hour];                    // get the hour words from hour table

            for (idx = 0; idx < MAX_HOUR_WORDS && word_idx_p[idx] != 0; idx++)
            {
                words[word_idx_p[idx]] = 1;
            }

            for (idx = 0; idx < WP_COUNT; idx++)
            {
                if (words[idx])
                {
                    dsp_word_on (idx);
                }
            }
        }

        animation_start_flag = 1;
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * get display mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_get_display_mode (void)
{
    return display_mode;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set display mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_set_display_mode (uint_fast8_t new_mode)
{
    if (new_mode < MODES_COUNT)
    {
        display_mode = new_mode;
        animation_start_flag = 1;
    }
    return display_mode;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * increment display mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_increment_display_mode (void)
{
    if (display_mode < MODES_COUNT - 1)
    {
        display_mode++;
    }
    else
    {
        display_mode = 0;
    }

    animation_start_flag = 1;
    return display_mode;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * decrement display mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_decrement_display_mode (void)
{
    if (display_mode == 0)
    {
        display_mode = MODES_COUNT - 1;
    }
    else
    {
        display_mode--;
    }

    animation_start_flag = 1;
    return display_mode;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * get animation mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_get_animation_mode (void)
{
    return animation_mode;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set animation mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_set_animation_mode (uint_fast8_t new_mode)
{
    if (new_mode < ANIMATION_MODES)
    {
        animation_mode = new_mode;
    }
    return animation_mode;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * increment animation mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_increment_animation_mode (void)
{
    if (animation_mode < ANIMATION_MODES - 1)
    {
        animation_mode++;
    }
    else
    {
        animation_mode = 0;
    }

    return animation_mode;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * decrement animation mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_decrement_animation_mode (void)
{
    if (animation_mode == 0)
    {
        animation_mode = ANIMATION_MODES - 1;
    }
    else
    {
        animation_mode--;
    }

    return animation_mode;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * calculate dimmed colors
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_calc_dimmed_colors ()
{
    uint_fast8_t    factor;

    factor = (brightness & 0x0F);

    dimmed_colors.red = (current_colors.red * factor) / 15;

    if (current_colors.red > 0 && dimmed_colors.red == 0)
    {
        dimmed_colors.red = 1;
    }

    dimmed_colors.green = (current_colors.green * factor) / 15;

    if (current_colors.green > 0 && dimmed_colors.green == 0)
    {
        dimmed_colors.green = 1;
    }


    dimmed_colors.blue = (current_colors.blue * factor) / 15;

    if (current_colors.blue > 0 && dimmed_colors.blue == 0)
    {
        dimmed_colors.blue = 1;
    }


}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * increment red color by 2
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_increment_color_red (void)
{
    if (current_colors.red < MAX_COLOR_STEPS - 1)
    {
        current_colors.red++;

        if (current_colors.red < MAX_COLOR_STEPS - 1)
        {
            current_colors.red++;
        }

        dsp_calc_dimmed_colors ();
        dsp_animation_flush ();
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * decrement red color by 2
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_decrement_color_red (void)
{
    if (current_colors.red > 0)
    {
        current_colors.red--;

        if (current_colors.red > 0)
        {
            current_colors.red--;
        }

        dsp_calc_dimmed_colors ();
        dsp_animation_flush ();
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * increment green color by 2
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_increment_color_green (void)
{
    if (current_colors.green < MAX_COLOR_STEPS - 1)
    {
        current_colors.green++;

        if (current_colors.green < MAX_COLOR_STEPS - 1)
        {
            current_colors.green++;
        }

        dsp_calc_dimmed_colors ();
        dsp_animation_flush ();
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * decrement green color by 2
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_decrement_color_green (void)
{
    if (current_colors.green > 0)
    {
        current_colors.green--;

        if (current_colors.green > 0)
        {
            current_colors.green--;
        }

        dsp_calc_dimmed_colors ();
        dsp_animation_flush ();
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * increment blue color by 2
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_increment_color_blue (void)
{
    if (current_colors.blue < MAX_COLOR_STEPS - 1)
    {
        current_colors.blue++;

        if (current_colors.blue < MAX_COLOR_STEPS - 1)
        {
            current_colors.blue++;
        }

        dsp_calc_dimmed_colors ();
        dsp_animation_flush ();
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * decrement blue color by 2
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_decrement_color_blue (void)
{
    if (current_colors.blue > 0)
    {
        current_colors.blue--;

        if (current_colors.blue > 0)
        {
            current_colors.blue--;
        }

        dsp_calc_dimmed_colors ();
        dsp_animation_flush ();
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set colors
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_set_colors  (DSP_COLORS * rgb)
{
    current_colors.red      = (rgb->red < MAX_COLOR_STEPS)   ? rgb->red : MAX_COLOR_STEPS;
    current_colors.green    = (rgb->green < MAX_COLOR_STEPS) ? rgb->green : MAX_COLOR_STEPS;
    current_colors.blue     = (rgb->blue < MAX_COLOR_STEPS)  ? rgb->blue : MAX_COLOR_STEPS;

    dsp_calc_dimmed_colors ();
    dsp_animation_flush ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set brightness
 *  15 = full brightness
 *   0 = low brigtness
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_set_brightness  (uint_fast8_t new_brightness)
{
    brightness = new_brightness;

    dsp_calc_dimmed_colors ();
    dsp_animation_flush ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * Test all LEDs
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_test (void)
{
    WS2812_RGB      rgb;
    uint_fast16_t   i;
    uint_fast16_t   j;

    for (i = 1; i < 8; i++)
    {
        rgb.red     = (i & 0x01) ? pwmtable8[MAX_COLOR_STEPS / 2] : 0;
        rgb.green   = (i & 0x02) ? pwmtable8[MAX_COLOR_STEPS / 2] : 0;
        rgb.blue    = (i & 0x04) ? pwmtable8[MAX_COLOR_STEPS / 2] : 0;

        for (j = 0; j < WS2812_LEDS; j++)
        {
            ws2812_set_led (j, &rgb, 0);
        }

        ws2812_refresh ();
        delay_sec (3);
    }
    dsp_animation_flush ();
    dsp_set_led0 (0, 0, 0);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * read configuration from EEPROM
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_read_config_from_eeprom (void)
{
    uint_fast8_t            rtc = 0;
    PACKED_DSP_COLORS       packed_rgb_color;
    uint8_t                 display_mode8;
    uint8_t                 animation_mode8;

    if (eeprom_is_up)
    {
        if (eeprom_read (EEPROM_DATA_OFFSET_DSP_COLORS, (uint8_t *) &packed_rgb_color, sizeof(PACKED_DSP_COLORS)) &&
            eeprom_read (EEPROM_DATA_OFFSET_DISPLAY_MODE, &display_mode8, sizeof(display_mode8)) &&
            eeprom_read (EEPROM_DATA_OFFSET_ANIMATION_MODE, &animation_mode8, sizeof(animation_mode8)))
        {
            if (display_mode8 >= MODES_COUNT)
            {
                display_mode8 = 0;
            }

            if (animation_mode8 >= ANIMATION_MODES)
            {
                animation_mode8 = 0;
            }

            current_colors.red      = packed_rgb_color.red;
            current_colors.green    = packed_rgb_color.green;
            current_colors.blue     = packed_rgb_color.blue;
            display_mode            = display_mode8;
            animation_mode          = animation_mode8;

            rtc = 1;
        }
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * write configuration to EEPROM
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_write_config_to_eeprom (void)
{
    uint_fast8_t            rtc = 0;
    PACKED_DSP_COLORS       packed_rgb_color;
    uint8_t                 display_mode8;
    uint8_t                 animation_mode8;

    packed_rgb_color.red   = current_colors.red;
    packed_rgb_color.green = current_colors.green;
    packed_rgb_color.blue  = current_colors.blue;

    display_mode8               = display_mode;
    animation_mode8             = animation_mode;

    if (eeprom_is_up)
    {
        if (eeprom_write (EEPROM_DATA_OFFSET_DSP_COLORS, (uint8_t *) &packed_rgb_color, sizeof(PACKED_DSP_COLORS)) &&
            eeprom_write (EEPROM_DATA_OFFSET_DISPLAY_MODE, &display_mode8, sizeof(display_mode8)) &&
            eeprom_write (EEPROM_DATA_OFFSET_ANIMATION_MODE, &animation_mode8, sizeof(animation_mode8)))
        {
            rtc = 1;
        }
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * initialize LED display
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_init (void)
{
    ws2812_init ();
}
