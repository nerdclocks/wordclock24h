/*-------------------------------------------------------------------------------------------------------------------------------------------
 * dsp.c - routines for display (LEDs & mcurses)
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
#include <stdint.h>
#include "wclock24h-config.h"
#include "display.h"
#include "tables.h"
#include "dsp.h"
#include "irmp.h"
#include "ws2812.h"
#include "eeprom.h"
#include "eeprom-data.h"

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

#define STEPS       (32)            // CHANGE HERE
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

#define MAX_BRIGHTNESS      32

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * 8-Bit PWM with 64 different settings:
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static const uint16_t pwmtable8[MAX_BRIGHTNESS]  =
{
        0,     1,     2,     3,     4,     5,     7,    10,
       13,    17,    21,    26,    32,    38,    44,    52,
       60,    68,    77,    87,    97,   108,   120,   132,
      145,   159,   173,   188,   204,   220,   237,   255
};

static RGB_BRIGHTNESS   current_brightness =
{
    MAX_BRIGHTNESS / 2, 0, 0
};

static uint_fast8_t     mode;

static RGB_BRIGHTNESS   current_brightness_up;
static RGB_BRIGHTNESS   current_brightness_down;

#define CURRENT_STATE   0x01
#define TARGET_STATE    0x02

static uint8_t          led_state[WS2812_LEDS];
static uint_fast8_t     red_step;
static uint_fast8_t     green_step;
static uint_fast8_t     blue_step;

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * switch all LEDs off
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_all_leds_off (void)
{
    uint_fast16_t     idx;

    for (idx = 0; idx < WS2812_LEDS; idx++)
    {
        if (led_state[idx] & TARGET_STATE)
        {
            led_state[idx] = CURRENT_STATE;
        }
        else
        {
            led_state[idx] = 0;
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
    uint_fast16_t     n;

    if (y & 0x01)                                       // odd row: count from right to left
    {
        n = y * WC_COLUMNS + (WC_COLUMNS - x);
    }
    else                                                // even row: count from left to right
    {
        n = y * WC_COLUMNS + x;
    }

    led_state[n] |= TARGET_STATE;
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
 * display clock time
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_clock (uint_fast8_t power_is_on, uint_fast8_t hour, uint_fast8_t minute)
{
    static uint8_t                  words[WP_COUNT];
    WS2812_RGB                      rgb_on;
    WS2812_RGB                      rgb_off;
    uint8_t                         hour_mode;
    uint8_t                         minute_mode;
    const struct MinuteDisplay *    tbl_minute;
    const uint8_t *                 word_idx_p;
    uint_fast16_t                   idx;

    // Perhaps not all LEDs have reached the desired brightness yet.
    // This happens when we change the time faster than LED fading is.
    // Then we have to flush the target states:
    rgb_on.red      = pwmtable8[current_brightness.red];
    rgb_on.green    = pwmtable8[current_brightness.green];
    rgb_on.blue     = pwmtable8[current_brightness.blue];

    rgb_off.red     = 0;
    rgb_off.green   = 0;
    rgb_off.blue    = 0;

    if (current_brightness_down.red     != rgb_off.red      ||
        current_brightness_down.green   != rgb_off.green    ||
        current_brightness_down.blue    != rgb_off.blue     ||
        current_brightness_up.red       != rgb_on.red       ||
        current_brightness_up.green     != rgb_on.green     ||
        current_brightness_up.blue      != rgb_on.blue)
    {
        for (idx = 0; idx < WS2812_LEDS; idx++)
        {
            if (led_state[idx] == TARGET_STATE)                           // up
            {
                ws2812_set_led (idx, &rgb_on, 0);
            }
            else if (led_state[idx] == CURRENT_STATE)                     // down
            {
                ws2812_set_led (idx, &rgb_off, 0);
            }
        }

        ws2812_refresh();
    }

    // Now all LEDs have the desired brightness of last time.
    // We can now set the new values:
    hour_mode   = tbl_modes[mode].hour_txt;
    minute_mode = tbl_modes[mode].minute_txt;
    tbl_minute  = &tbl_minutes[minute_mode][minute];

    memset (words, 0, WP_COUNT);
    dsp_all_leds_off ();

    if (power_is_on)
    {
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

    red_step   = current_brightness.red / 5;

    if (red_step == 0 && current_brightness.red > 0)
    {
        red_step = 1;
    }

    green_step = current_brightness.green / 5;

    if (green_step == 0 && current_brightness.green > 0)
    {
        green_step = 1;
    }

    blue_step  = current_brightness.blue / 5;

    if (blue_step == 0 && current_brightness.blue > 0)
    {
        blue_step = 1;
    }

    current_brightness_up.red       = 0;
    current_brightness_up.green     = 0;
    current_brightness_up.blue      = 0;

    current_brightness_down.red     = current_brightness.red;
    current_brightness_down.green   = current_brightness.green;
    current_brightness_down.blue    = current_brightness.blue;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * fade
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_fade (uint_fast8_t changed)
{
    uint_fast16_t    idx;

    if (current_brightness_down.red >= red_step)
    {
        current_brightness_down.red -= red_step;
        changed = 1;
    }
    else if (current_brightness_down.red != 0)
    {
        current_brightness_down.red = 0;
        changed = 1;
    }

    if (current_brightness_down.green >= green_step)
    {
        current_brightness_down.green -= green_step;
        changed = 1;
    }
    else if (current_brightness_down.green != 0)
    {
        current_brightness_down.green = 0;
        changed = 1;
    }

    if (current_brightness_down.blue >= blue_step)
    {
        current_brightness_down.blue -= blue_step;
        changed = 1;
    }
    else if (current_brightness_down.blue != 0)
    {
        current_brightness_down.blue = 0;
        changed = 1;
    }

    if (current_brightness_up.red + red_step <= current_brightness.red)
    {
        current_brightness_up.red += red_step;
        changed = 1;
    }
    else if (current_brightness_up.red != current_brightness.red)
    {
        current_brightness_up.red = current_brightness.red;
        changed = 1;
    }

    if (current_brightness_up.green + green_step <= current_brightness.green)
    {
        current_brightness_up.green += green_step;
        changed = 1;
    }
    else if (current_brightness_up.green != current_brightness.green)
    {
        current_brightness_up.green = current_brightness.green;
        changed = 1;
    }

    if (current_brightness_up.blue + blue_step <= current_brightness.blue)
    {
        current_brightness_up.blue += blue_step;
        changed = 1;
    }
    else if (current_brightness_up.blue != current_brightness.blue)
    {
        current_brightness_up.blue = current_brightness.blue;
        changed = 1;
    }

    if (changed)
    {
        WS2812_RGB  rgb;
        WS2812_RGB  rgb_up;
        WS2812_RGB  rgb_down;

        rgb.red         = pwmtable8[current_brightness.red];
        rgb.green       = pwmtable8[current_brightness.green];
        rgb.blue        = pwmtable8[current_brightness.blue];

        rgb_up.red      = pwmtable8[current_brightness_up.red];
        rgb_up.green    = pwmtable8[current_brightness_up.green];
        rgb_up.blue     = pwmtable8[current_brightness_up.blue];

        rgb_down.red    = pwmtable8[current_brightness_down.red];
        rgb_down.green  = pwmtable8[current_brightness_down.green];
        rgb_down.blue   = pwmtable8[current_brightness_down.blue];

        for (idx = 0; idx < WS2812_LEDS; idx++)
        {
            if (led_state[idx] == TARGET_STATE)                           // up
            {
                ws2812_set_led (idx, &rgb_up, 0);
            }
            else if (led_state[idx] == CURRENT_STATE)                     // down
            {
                ws2812_set_led (idx, &rgb_down, 0);
            }
            else if (led_state[idx] == (CURRENT_STATE | TARGET_STATE))    // on, but no change
            {
                ws2812_set_led (idx, &rgb, 0);
            }
        }

        ws2812_refresh();
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * get display mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_get_mode (void)
{
    return mode;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set display mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_set_mode (uint_fast8_t new_mode)
{
    if (new_mode < MODES_COUNT)
    {
        mode = new_mode;
    }
    return mode;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * increment display mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_increment_mode (void)
{
    if (mode < MODES_COUNT - 1)
    {
        mode++;
    }
    else
    {
        mode = 0;
    }

    return mode;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * increment display mode
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_decrement_mode (void)
{
    if (mode == 0)
    {
        mode = MODES_COUNT - 1;
    }
    else
    {
        mode--;
    }

    return mode;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * increment red brightness
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_increment_brightness_red (void)
{
    if (current_brightness.red < MAX_BRIGHTNESS - 1)
    {
        current_brightness.red++;
        dsp_fade (1);
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * decrement red brightness
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_decrement_brightness_red (void)
{
    if (current_brightness.red > 0)
    {
        current_brightness.red--;
        dsp_fade (1);
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * increment green brightness
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_increment_brightness_green (void)
{
    if (current_brightness.green < MAX_BRIGHTNESS - 1)
    {
        current_brightness.green++;
        dsp_fade (1);
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * decrement green brightness
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_decrement_brightness_green (void)
{
    if (current_brightness.green > 0)
    {
        current_brightness.green--;
        dsp_fade (1);
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * increment blue brightness
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_increment_brightness_blue (void)
{
    if (current_brightness.blue < MAX_BRIGHTNESS - 1)
    {
        current_brightness.blue++;
        dsp_fade (1);
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * decrement blue brightness
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_decrement_brightness_blue (void)
{
    if (current_brightness.blue > 0)
    {
        current_brightness.blue--;
        dsp_fade (1);
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set brightness
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_set_brightness (RGB_BRIGHTNESS * rgb)
{
    current_brightness.red      = (rgb->red < MAX_BRIGHTNESS)   ? rgb->red : MAX_BRIGHTNESS;
    current_brightness.green    = (rgb->green < MAX_BRIGHTNESS) ? rgb->green : MAX_BRIGHTNESS;
    current_brightness.blue     = (rgb->blue < MAX_BRIGHTNESS)  ? rgb->blue : MAX_BRIGHTNESS;

    dsp_fade (1);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * read configuration from EEPROM
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_read_config_from_eeprom (void)
{
    uint_fast8_t            rtc = 0;
    PACKED_RGB_BRIGHTNESS   packed_rgb_brightness;
    uint8_t                 mode8;

    if (eeprom_is_up)
    {
        if (eeprom_read (EEPROM_DATA_OFFSET_RGB_BRIGHTNESS, (uint8_t *) &packed_rgb_brightness, sizeof(PACKED_RGB_BRIGHTNESS)) &&
            eeprom_read (EEPROM_DATA_OFFSET_MODE, &mode8, sizeof(mode8)))
        {
            if (mode8 >= MODES_COUNT)
            {
                mode8 = 0;
            }

            current_brightness.red      = packed_rgb_brightness.red;
            current_brightness.green    = packed_rgb_brightness.green;
            current_brightness.blue     = packed_rgb_brightness.blue;
            mode = mode8;

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
    PACKED_RGB_BRIGHTNESS   packed_rgb_brightness;
    uint8_t                 mode8;

    packed_rgb_brightness.red   = current_brightness.red;
    packed_rgb_brightness.green = current_brightness.green;
    packed_rgb_brightness.blue  = current_brightness.blue;

    mode8 = mode;

    if (eeprom_is_up)
    {
        if (eeprom_write (EEPROM_DATA_OFFSET_RGB_BRIGHTNESS, (uint8_t *) &packed_rgb_brightness, sizeof(PACKED_RGB_BRIGHTNESS)) &&
            eeprom_write (EEPROM_DATA_OFFSET_MODE, &mode8, sizeof(mode8)))
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
