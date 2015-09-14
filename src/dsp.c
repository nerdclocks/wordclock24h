/*-------------------------------------------------------------------------------------------------------------------------------------------
 * dsp.c - routines for LED display 16x18
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
#include "ldr.h"

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

static uint_fast8_t                 brightness = MAX_BRIGHTNESS;
static uint_fast8_t                 automatic_brightness_control = 0;

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
    uint8_t                         state[WS2812_DISPLAY_LEDS];
    uint8_t                         matrix[WC_ROWS][WC_COLUMNS];
} led;

static uint_fast8_t                 red_step;
static uint_fast8_t                 green_step;
static uint_fast8_t                 blue_step;

static uint_fast8_t                 ambi_state;

#define FONT_LINES                  12
#define FONT_COLS                    7

const char font[256][FONT_LINES] =
{
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x00
    {0x00,0x00,0x00,0x1C,0x63,0x55,0x7F,0x63,0x1C,0x00,0x00,0x00},  // 0x01
    {0x00,0x00,0x00,0x1C,0x7F,0x6B,0x7F,0x63,0x1C,0x00,0x00,0x00},  // 0x02
    {0x00,0x36,0x7F,0x7F,0x7F,0x3E,0x3E,0x1C,0x08,0x00,0x00,0x00},  // 0x03
    {0x00,0x08,0x1C,0x3E,0x7F,0x3E,0x1C,0x08,0x08,0x00,0x00,0x00},  // 0x04
    {0x00,0x1C,0x1C,0x1C,0x7F,0x7F,0x7F,0x0C,0x0C,0x00,0x00,0x00},  // 0x05
    {0x00,0x08,0x1C,0x3E,0x7F,0x7F,0x7F,0x0C,0x0C,0x00,0x00,0x00},  // 0x06
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x07
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x08
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x09
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x0A
    {0x00,0x06,0x03,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00},  // 0x0B
    {0x00,0x3C,0x42,0x42,0x42,0x3C,0x08,0x1C,0x08,0x00,0x00,0x00},  // 0x0C
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x0D
    {0x00,0x02,0x0E,0x1A,0x12,0x12,0x12,0x1E,0x1C,0x70,0x60,0x00},  // 0x0E
    {0x00,0x00,0x08,0x3E,0x22,0x63,0x22,0x3E,0x08,0x00,0x00,0x00},  // 0x0F
    {0x00,0x00,0x40,0x70,0x7C,0x7F,0x7C,0x70,0x40,0x00,0x00,0x00},  // 0x10
    {0x00,0x00,0x01,0x07,0x1F,0x7F,0x1F,0x07,0x01,0x00,0x00,0x00},  // 0x11
    {0x00,0x08,0x08,0x1C,0x08,0x08,0x08,0x08,0x08,0x1C,0x08,0x00},  // 0x12
    {0x00,0x24,0x24,0x24,0x24,0x24,0x24,0x00,0x24,0x00,0x00,0x00},  // 0x13
    {0x00,0x3E,0x3A,0x3A,0x1A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x00},  // 0x14
    {0x00,0x1E,0x20,0x30,0x1C,0x22,0x32,0x1C,0x02,0x02,0x3C,0x00},  // 0x15
    {0x00,0x00,0x00,0x7F,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x16
    {0x08,0x08,0x1C,0x08,0x08,0x08,0x08,0x1C,0x08,0x00,0x1C,0x00},  // 0x17
    {0x00,0x08,0x08,0x1C,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x00},  // 0x18
    {0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x1C,0x08,0x00},  // 0x19
    {0x00,0x00,0x00,0x00,0x08,0x3E,0x08,0x00,0x00,0x00,0x00,0x00},  // 0x1A
    {0x00,0x00,0x00,0x00,0x10,0x3E,0x10,0x00,0x00,0x00,0x00,0x00},  // 0x1B
    {0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x20,0x3E,0x00,0x00,0x00},  // 0x1C
    {0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x1D
    {0x00,0x00,0x08,0x08,0x1C,0x1C,0x3E,0x3E,0x7F,0x00,0x00,0x00},  // 0x1E
    {0x00,0x00,0x7F,0x3E,0x3E,0x1C,0x1C,0x08,0x08,0x00,0x00,0x00},  // 0x1F
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x20
    {0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x00,0x08,0x00,0x00,0x00},  // 0x21
    {0x24,0x24,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x22
    {0x00,0x0A,0x0A,0x3F,0x14,0x14,0x7E,0x28,0x28,0x00,0x00,0x00},  // 0x23
    {0x08,0x1E,0x28,0x28,0x18,0x0C,0x0A,0x0A,0x3C,0x08,0x00,0x00},  // 0x24
    {0x00,0x31,0x4A,0x4C,0x38,0x0E,0x19,0x29,0x46,0x00,0x00,0x00},  // 0x25
    {0x00,0x0E,0x12,0x16,0x18,0x69,0x45,0x66,0x3F,0x00,0x00,0x00},  // 0x26
    {0x08,0x08,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x27
    {0x06,0x0C,0x08,0x10,0x10,0x10,0x10,0x10,0x08,0x0C,0x06,0x00},  // 0x28
    {0x20,0x18,0x08,0x04,0x04,0x04,0x04,0x04,0x08,0x18,0x30,0x00},  // 0x29
    {0x00,0x08,0x36,0x18,0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x2A
    {0x00,0x00,0x00,0x08,0x08,0x7F,0x08,0x08,0x08,0x00,0x00,0x00},  // 0x2B
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x08,0x10,0x00},  // 0x2C
    {0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x2D
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00},  // 0x2E
    {0x01,0x02,0x02,0x04,0x04,0x08,0x10,0x10,0x20,0x20,0x40,0x00},  // 0x2F
    {0x00,0x3C,0x24,0x42,0x42,0x42,0x42,0x24,0x3C,0x00,0x00,0x00},  // 0x30
    {0x00,0x08,0x38,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00},  // 0x31
    {0x00,0x3C,0x02,0x02,0x06,0x08,0x10,0x20,0x3E,0x00,0x00,0x00},  // 0x32
    {0x00,0x3C,0x04,0x04,0x18,0x04,0x04,0x04,0x38,0x00,0x00,0x00},  // 0x33
    {0x00,0x04,0x0C,0x14,0x24,0x44,0x7E,0x04,0x04,0x00,0x00,0x00},  // 0x34
    {0x00,0x3C,0x20,0x20,0x38,0x04,0x04,0x04,0x38,0x00,0x00,0x00},  // 0x35
    {0x00,0x0E,0x10,0x20,0x2C,0x32,0x22,0x22,0x1C,0x00,0x00,0x00},  // 0x36
    {0x00,0x3E,0x02,0x04,0x08,0x10,0x10,0x20,0x20,0x00,0x00,0x00},  // 0x37
    {0x00,0x1C,0x22,0x22,0x1C,0x24,0x22,0x22,0x1C,0x00,0x00,0x00},  // 0x38
    {0x00,0x1C,0x22,0x22,0x22,0x1E,0x02,0x04,0x38,0x00,0x00,0x00},  // 0x39
    {0x00,0x00,0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00,0x00,0x00},  // 0x3A
    {0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x08,0x10,0x00},  // 0x3B
    {0x00,0x00,0x00,0x02,0x0C,0x10,0x10,0x0C,0x02,0x00,0x00,0x00},  // 0x3C
    {0x00,0x00,0x00,0x00,0x3E,0x00,0x3E,0x00,0x00,0x00,0x00,0x00},  // 0x3D
    {0x00,0x00,0x00,0x20,0x18,0x04,0x04,0x18,0x20,0x00,0x00,0x00},  // 0x3E
    {0x00,0x3C,0x22,0x02,0x04,0x08,0x08,0x00,0x08,0x00,0x00,0x00},  // 0x3F
    {0x00,0x1C,0x22,0x4E,0x52,0x56,0x5B,0x20,0x1C,0x00,0x00,0x00},  // 0x40
    {0x00,0x08,0x14,0x14,0x14,0x22,0x3E,0x22,0x41,0x00,0x00,0x00},  // 0x41
    {0x00,0x3C,0x22,0x22,0x3C,0x22,0x22,0x22,0x3C,0x00,0x00,0x00},  // 0x42
    {0x00,0x1E,0x20,0x40,0x40,0x40,0x40,0x20,0x1E,0x00,0x00,0x00},  // 0x43
    {0x00,0x78,0x44,0x42,0x42,0x42,0x42,0x44,0x78,0x00,0x00,0x00},  // 0x44
    {0x00,0x3E,0x20,0x20,0x20,0x3C,0x20,0x20,0x3E,0x00,0x00,0x00},  // 0x45
    {0x00,0x3E,0x20,0x20,0x20,0x3C,0x20,0x20,0x20,0x00,0x00,0x00},  // 0x46
    {0x00,0x1E,0x20,0x40,0x40,0x4E,0x42,0x22,0x1E,0x00,0x00,0x00},  // 0x47
    {0x00,0x22,0x22,0x22,0x3E,0x22,0x22,0x22,0x22,0x00,0x00,0x00},  // 0x48
    {0x00,0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00},  // 0x49
    {0x00,0x3C,0x04,0x04,0x04,0x04,0x04,0x04,0x78,0x00,0x00,0x00},  // 0x4A
    {0x00,0x22,0x24,0x28,0x30,0x30,0x28,0x24,0x22,0x00,0x00,0x00},  // 0x4B
    {0x00,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x3E,0x00,0x00,0x00},  // 0x4C
    {0x00,0x66,0x66,0x6A,0x5A,0x5A,0x52,0x42,0x42,0x00,0x00,0x00},  // 0x4D
    {0x00,0x22,0x32,0x32,0x2A,0x2A,0x26,0x26,0x22,0x00,0x00,0x00},  // 0x4E
    {0x00,0x3C,0x24,0x42,0x42,0x42,0x42,0x24,0x3C,0x00,0x00,0x00},  // 0x4F
    {0x00,0x3C,0x22,0x22,0x22,0x3C,0x20,0x20,0x20,0x00,0x00,0x00},  // 0x50
    {0x00,0x3C,0x24,0x42,0x42,0x42,0x42,0x24,0x1C,0x04,0x03,0x00},  // 0x51
    {0x00,0x3C,0x22,0x22,0x24,0x38,0x28,0x24,0x22,0x00,0x00,0x00},  // 0x52
    {0x00,0x1E,0x20,0x20,0x18,0x04,0x02,0x02,0x3C,0x00,0x00,0x00},  // 0x53
    {0x00,0x7F,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00},  // 0x54
    {0x00,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x1C,0x00,0x00,0x00},  // 0x55
    {0x00,0x41,0x22,0x22,0x22,0x14,0x14,0x1C,0x08,0x00,0x00,0x00},  // 0x56
    {0x00,0x41,0x41,0x49,0x49,0x35,0x36,0x36,0x22,0x00,0x00,0x00},  // 0x57
    {0x00,0x41,0x22,0x14,0x08,0x08,0x14,0x22,0x41,0x00,0x00,0x00},  // 0x58
    {0x00,0x41,0x22,0x14,0x14,0x08,0x08,0x08,0x08,0x00,0x00,0x00},  // 0x59
    {0x00,0x7E,0x02,0x04,0x08,0x10,0x20,0x40,0x7E,0x00,0x00,0x00},  // 0x5A
    {0x0F,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x0F,0x00},  // 0x5B
    {0x40,0x20,0x20,0x10,0x10,0x08,0x04,0x04,0x02,0x02,0x01,0x00},  // 0x5C
    {0x3C,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x3C,0x00},  // 0x5D
    {0x08,0x08,0x18,0x14,0x14,0x24,0x22,0x00,0x00,0x00,0x00,0x00},  // 0x5E
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0x00,0x00},  // 0x5F
    {0x10,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0x60
    {0x00,0x00,0x00,0x1C,0x02,0x1E,0x22,0x22,0x1F,0x00,0x00,0x00},  // 0x61
    {0x20,0x20,0x20,0x2C,0x32,0x22,0x22,0x22,0x3C,0x00,0x00,0x00},  // 0x62
    {0x00,0x00,0x00,0x1E,0x20,0x20,0x20,0x20,0x1E,0x00,0x00,0x00},  // 0x63
    {0x02,0x02,0x02,0x1E,0x22,0x22,0x22,0x26,0x1A,0x00,0x00,0x00},  // 0x64
    {0x00,0x00,0x00,0x1C,0x22,0x3E,0x20,0x20,0x1E,0x00,0x00,0x00},  // 0x65
    {0x0E,0x10,0x10,0x7E,0x10,0x10,0x10,0x10,0x10,0x00,0x00,0x00},  // 0x66
    {0x00,0x00,0x00,0x1E,0x22,0x22,0x22,0x22,0x1E,0x02,0x1C,0x00},  // 0x67
    {0x20,0x20,0x20,0x2E,0x32,0x22,0x22,0x22,0x22,0x00,0x00,0x00},  // 0x68
    {0x08,0x00,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00},  // 0x69
    {0x04,0x00,0x00,0x3C,0x04,0x04,0x04,0x04,0x04,0x04,0x38,0x00},  // 0x6A
    {0x20,0x20,0x20,0x22,0x24,0x38,0x28,0x24,0x22,0x00,0x00,0x00},  // 0x6B
    {0x38,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00},  // 0x6C
    {0x00,0x00,0x00,0x5B,0x6D,0x49,0x49,0x49,0x49,0x00,0x00,0x00},  // 0x6D
    {0x00,0x00,0x00,0x2E,0x32,0x22,0x22,0x22,0x22,0x00,0x00,0x00},  // 0x6E
    {0x00,0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00},  // 0x6F
    {0x00,0x00,0x00,0x2C,0x32,0x22,0x22,0x22,0x3C,0x20,0x20,0x00},  // 0x70
    {0x00,0x00,0x00,0x1E,0x22,0x22,0x22,0x22,0x1E,0x02,0x02,0x00},  // 0x71
    {0x00,0x00,0x00,0x2E,0x32,0x20,0x20,0x20,0x20,0x00,0x00,0x00},  // 0x72
    {0x00,0x00,0x00,0x1E,0x20,0x38,0x06,0x02,0x3C,0x00,0x00,0x00},  // 0x73
    {0x00,0x10,0x10,0x7E,0x10,0x10,0x10,0x10,0x0E,0x00,0x00,0x00},  // 0x74
    {0x00,0x00,0x00,0x22,0x22,0x22,0x22,0x26,0x3A,0x00,0x00,0x00},  // 0x75
    {0x00,0x00,0x00,0x41,0x22,0x22,0x14,0x14,0x08,0x00,0x00,0x00},  // 0x76
    {0x00,0x00,0x00,0x41,0x49,0x55,0x55,0x36,0x22,0x00,0x00,0x00},  // 0x77
    {0x00,0x00,0x00,0x42,0x24,0x18,0x18,0x24,0x42,0x00,0x00,0x00},  // 0x78
    {0x00,0x00,0x00,0x42,0x24,0x24,0x18,0x18,0x10,0x10,0x60,0x00},  // 0x79
    {0x00,0x00,0x00,0x7E,0x04,0x08,0x10,0x20,0x7E,0x00,0x00,0x00},  // 0x7A
    {0x0E,0x08,0x08,0x08,0x08,0x30,0x08,0x08,0x08,0x08,0x06,0x00},  // 0x7B
    {0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x00},  // 0x7C
    {0x30,0x08,0x08,0x08,0x08,0x06,0x08,0x08,0x08,0x08,0x38,0x00},  // 0x7D
    {0x00,0x00,0x00,0x00,0x00,0x39,0x4E,0x00,0x00,0x00,0x00,0x00},  // 0x7E
    {0x00,0x00,0x08,0x14,0x36,0x22,0x22,0x22,0x3E,0x00,0x00,0x00},  // 0x7F
    {0x00,0x1E,0x20,0x40,0x40,0x40,0x40,0x20,0x1E,0x04,0x02,0x06},  // 0x80
    {0x00,0x14,0x00,0x22,0x22,0x22,0x22,0x26,0x3A,0x00,0x00,0x00},  // 0x81
    {0x04,0x08,0x00,0x1C,0x22,0x3E,0x20,0x20,0x1E,0x00,0x00,0x00},  // 0x82
    {0x0C,0x12,0x00,0x1C,0x02,0x1E,0x22,0x22,0x1F,0x00,0x00,0x00},  // 0x83
    {0x00,0x14,0x00,0x1C,0x02,0x1E,0x22,0x22,0x1F,0x00,0x00,0x00},  // 0x84
    {0x10,0x08,0x00,0x1C,0x02,0x1E,0x22,0x22,0x1F,0x00,0x00,0x00},  // 0x85
    {0x08,0x14,0x08,0x1C,0x02,0x1E,0x22,0x22,0x1F,0x00,0x00,0x00},  // 0x86
    {0x00,0x00,0x00,0x1E,0x20,0x20,0x20,0x20,0x1E,0x08,0x04,0x0C},  // 0x87
    {0x0C,0x12,0x00,0x1C,0x22,0x3E,0x20,0x20,0x1E,0x00,0x00,0x00},  // 0x88
    {0x00,0x14,0x00,0x1C,0x22,0x3E,0x20,0x20,0x1E,0x00,0x00,0x00},  // 0x89
    {0x10,0x08,0x00,0x1C,0x22,0x3E,0x20,0x20,0x1E,0x00,0x00,0x00},  // 0x8A
    {0x00,0x14,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00},  // 0x8B
    {0x0C,0x12,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00},  // 0x8C
    {0x10,0x08,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00},  // 0x8D
    {0x24,0x08,0x14,0x14,0x14,0x22,0x3E,0x22,0x41,0x00,0x00,0x00},  // 0x8E
    {0x1C,0x08,0x14,0x14,0x14,0x22,0x3E,0x22,0x41,0x00,0x00,0x00},  // 0x8F
    {0x04,0x3E,0x20,0x20,0x20,0x3C,0x20,0x20,0x3E,0x00,0x00,0x00},  // 0x90
    {0x00,0x00,0x00,0x76,0x09,0x3F,0x48,0x48,0x37,0x00,0x00,0x00},  // 0x91
    {0x00,0x0F,0x0C,0x14,0x14,0x26,0x3C,0x24,0x47,0x00,0x00,0x00},  // 0x92
    {0x0C,0x12,0x00,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00},  // 0x93
    {0x00,0x14,0x00,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00},  // 0x94
    {0x10,0x08,0x00,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00},  // 0x95
    {0x0C,0x12,0x00,0x22,0x22,0x22,0x22,0x26,0x3A,0x00,0x00,0x00},  // 0x96
    {0x20,0x10,0x00,0x22,0x22,0x22,0x22,0x26,0x3A,0x00,0x00,0x00},  // 0x97
    {0x00,0x14,0x00,0x42,0x24,0x24,0x18,0x18,0x10,0x10,0x60,0x00},  // 0x98
    {0x24,0x3C,0x24,0x42,0x42,0x42,0x42,0x24,0x3C,0x00,0x00,0x00},  // 0x99
    {0x24,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x1C,0x00,0x00,0x00},  // 0x9A
    {0x00,0x00,0x00,0x3E,0x46,0x4A,0x52,0x62,0x7C,0x00,0x00,0x00},  // 0x9B
    {0x00,0x06,0x08,0x08,0x1C,0x08,0x08,0x10,0x1E,0x00,0x00,0x00},  // 0x9C
    {0x00,0x3E,0x24,0x46,0x4A,0x52,0x62,0x24,0x7C,0x00,0x00,0x00},  // 0x9D
    {0x00,0x00,0x00,0x42,0x24,0x18,0x18,0x24,0x42,0x00,0x00,0x00},  // 0x9E
    {0x07,0x08,0x08,0x1E,0x08,0x08,0x08,0x08,0x08,0x08,0x70,0x00},  // 0x9F
    {0x04,0x08,0x00,0x1C,0x02,0x1E,0x22,0x22,0x1F,0x00,0x00,0x00},  // 0xA0
    {0x04,0x08,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00},  // 0xA1
    {0x04,0x08,0x00,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00},  // 0xA2
    {0x04,0x08,0x00,0x22,0x22,0x22,0x22,0x26,0x3A,0x00,0x00,0x00},  // 0xA3
    {0x0A,0x14,0x00,0x2E,0x32,0x22,0x22,0x22,0x22,0x00,0x00,0x00},  // 0xA4
    {0x0A,0x36,0x32,0x32,0x2A,0x2A,0x26,0x26,0x22,0x00,0x00,0x00},  // 0xA5
    {0x00,0x3C,0x04,0x1C,0x24,0x3E,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xA6
    {0x00,0x1C,0x22,0x22,0x22,0x1C,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xA7
    {0x00,0x00,0x00,0x08,0x00,0x08,0x08,0x10,0x20,0x22,0x1E,0x00},  // 0xA8
    {0x00,0x3C,0x42,0x5A,0x5A,0x42,0x3C,0x00,0x00,0x00,0x00,0x00},  // 0xA9
    {0x00,0x00,0x00,0x00,0x7E,0x02,0x02,0x00,0x00,0x00,0x00,0x00},  // 0xAA
    {0x00,0x62,0x24,0x28,0x2F,0x11,0x12,0x24,0x47,0x00,0x00,0x00},  // 0xAB
    {0x00,0x62,0x24,0x28,0x2A,0x16,0x1A,0x2F,0x42,0x00,0x00,0x00},  // 0xAC
    {0x00,0x00,0x00,0x08,0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x00},  // 0xAD
    {0x00,0x00,0x00,0x0A,0x14,0x28,0x14,0x0A,0x00,0x00,0x00,0x00},  // 0xAE
    {0x00,0x00,0x00,0x28,0x14,0x0A,0x14,0x28,0x00,0x00,0x00,0x00},  // 0xAF
    {0x52,0x00,0x52,0x00,0x00,0x52,0x00,0x52,0x00,0x00,0x52,0x00},  // 0xB0
    {0x25,0x52,0x25,0x00,0x52,0x25,0x52,0x25,0x52,0x00,0x25,0x52},  // 0xB1
    {0x7F,0x52,0x7F,0x52,0x52,0x7F,0x52,0x7F,0x52,0x52,0x7F,0x52},  // 0xB2
    {0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08},  // 0xB3
    {0x08,0x08,0x08,0x08,0x08,0x78,0x08,0x08,0x08,0x08,0x08,0x08},  // 0xB4
    {0x08,0x18,0x14,0x14,0x14,0x22,0x3E,0x22,0x41,0x00,0x00,0x00},  // 0xB5
    {0x18,0x2C,0x14,0x14,0x14,0x22,0x3E,0x22,0x41,0x00,0x00,0x00},  // 0xB6
    {0x20,0x18,0x14,0x14,0x14,0x22,0x3E,0x22,0x41,0x00,0x00,0x00},  // 0xB7
    {0x00,0x1C,0x22,0x4D,0x51,0x51,0x4D,0x22,0x1C,0x00,0x00,0x00},  // 0xB8
    {0x14,0x14,0x14,0x14,0x74,0x04,0x74,0x14,0x14,0x14,0x14,0x14},  // 0xB9
    {0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14},  // 0xBA
    {0x00,0x00,0x00,0x00,0x7C,0x04,0x74,0x14,0x14,0x14,0x14,0x14},  // 0xBB
    {0x14,0x14,0x14,0x14,0x74,0x04,0x7C,0x00,0x00,0x00,0x00,0x00},  // 0xBC
    {0x00,0x04,0x1E,0x34,0x24,0x24,0x34,0x1E,0x04,0x00,0x00,0x00},  // 0xBD
    {0x00,0x41,0x22,0x14,0x08,0x3E,0x08,0x3E,0x08,0x00,0x00,0x00},  // 0xBE
    {0x00,0x00,0x00,0x00,0x00,0x78,0x08,0x08,0x08,0x08,0x08,0x08},  // 0xBF
    {0x08,0x08,0x08,0x08,0x08,0x0F,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xC0
    {0x08,0x08,0x08,0x08,0x08,0x7F,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xC1
    {0x00,0x00,0x00,0x00,0x00,0x7F,0x08,0x08,0x08,0x08,0x08,0x08},  // 0xC2
    {0x08,0x08,0x08,0x08,0x08,0x0F,0x08,0x08,0x08,0x08,0x08,0x08},  // 0xC3
    {0x00,0x00,0x00,0x00,0x00,0x7F,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xC4
    {0x08,0x08,0x08,0x08,0x08,0x7F,0x08,0x08,0x08,0x08,0x08,0x08},  // 0xC5
    {0x0A,0x14,0x00,0x1C,0x02,0x1E,0x22,0x22,0x1F,0x00,0x00,0x00},  // 0xC6
    {0x0A,0x1C,0x14,0x14,0x14,0x22,0x3E,0x22,0x41,0x00,0x00,0x00},  // 0xC7
    {0x14,0x14,0x14,0x14,0x17,0x10,0x1F,0x00,0x00,0x00,0x00,0x00},  // 0xC8
    {0x00,0x00,0x00,0x00,0x1F,0x10,0x17,0x14,0x14,0x14,0x14,0x14},  // 0xC9
    {0x14,0x14,0x14,0x14,0x77,0x00,0x7F,0x00,0x00,0x00,0x00,0x00},  // 0xCA
    {0x00,0x00,0x00,0x00,0x7F,0x00,0x77,0x14,0x14,0x14,0x14,0x14},  // 0xCB
    {0x14,0x14,0x14,0x14,0x17,0x10,0x17,0x14,0x14,0x14,0x14,0x14},  // 0xCC
    {0x00,0x00,0x00,0x00,0x7F,0x00,0x7F,0x00,0x00,0x00,0x00,0x00},  // 0xCD
    {0x14,0x14,0x14,0x14,0x77,0x00,0x77,0x14,0x14,0x14,0x14,0x14},  // 0xCE
    {0x00,0x41,0x3E,0x22,0x22,0x22,0x3E,0x41,0x00,0x00,0x00,0x00},  // 0xCF
    {0x68,0x18,0x2C,0x3E,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00},  // 0xD0
    {0x00,0x78,0x44,0x42,0x62,0x42,0x42,0x44,0x78,0x00,0x00,0x00},  // 0xD1
    {0x18,0x3E,0x20,0x20,0x20,0x3C,0x20,0x20,0x3E,0x00,0x00,0x00},  // 0xD2
    {0x24,0x3E,0x20,0x20,0x20,0x3C,0x20,0x20,0x3E,0x00,0x00,0x00},  // 0xD3
    {0x10,0x3E,0x20,0x20,0x20,0x3C,0x20,0x20,0x3E,0x00,0x00,0x00},  // 0xD4
    {0x00,0x00,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00},  // 0xD5
    {0x04,0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00},  // 0xD6
    {0x18,0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00},  // 0xD7
    {0x24,0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00},  // 0xD8
    {0x08,0x08,0x08,0x08,0x08,0x78,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xD9
    {0x00,0x00,0x00,0x00,0x00,0x0F,0x08,0x08,0x08,0x08,0x08,0x08},  // 0xDA
    {0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F},  // 0xDB
    {0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F},  // 0xDC
    {0x08,0x08,0x08,0x08,0x00,0x00,0x00,0x08,0x08,0x08,0x08,0x00},  // 0xDD
    {0x10,0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0x00,0x00,0x00},  // 0xDE
    {0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xDF
    {0x04,0x3C,0x24,0x42,0x42,0x42,0x42,0x24,0x3C,0x00,0x00,0x00},  // 0xE0
    {0x3C,0x24,0x24,0x28,0x28,0x24,0x22,0x21,0x2E,0x00,0x00,0x00},  // 0xE1
    {0x18,0x3C,0x24,0x42,0x42,0x42,0x42,0x24,0x3C,0x00,0x00,0x00},  // 0xE2
    {0x10,0x3C,0x24,0x42,0x42,0x42,0x42,0x24,0x3C,0x00,0x00,0x00},  // 0xE3
    {0x0A,0x14,0x00,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x00},  // 0xE4
    {0x0A,0x3C,0x24,0x42,0x42,0x42,0x42,0x24,0x3C,0x00,0x00,0x00},  // 0xE5
    {0x00,0x00,0x00,0x22,0x22,0x22,0x22,0x26,0x3A,0x20,0x20,0x00},  // 0xE6
    {0x20,0x20,0x20,0x2C,0x32,0x22,0x22,0x22,0x3C,0x20,0x20,0x00},  // 0xE7
    {0x00,0x20,0x3C,0x22,0x22,0x22,0x22,0x3C,0x20,0x00,0x00,0x00},  // 0xE8
    {0x04,0x2A,0x22,0x22,0x22,0x22,0x22,0x22,0x1C,0x00,0x00,0x00},  // 0xE9
    {0x18,0x26,0x22,0x22,0x22,0x22,0x22,0x22,0x1C,0x00,0x00,0x00},  // 0xEA
    {0x10,0x2A,0x22,0x22,0x22,0x22,0x22,0x22,0x1C,0x00,0x00,0x00},  // 0xEB
    {0x04,0x08,0x00,0x42,0x24,0x24,0x18,0x18,0x10,0x10,0x60,0x00},  // 0xEC
    {0x04,0x49,0x22,0x14,0x14,0x08,0x08,0x08,0x08,0x00,0x00,0x00},  // 0xED
    {0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xEE
    {0x08,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xEF
    {0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xF0
    {0x00,0x00,0x08,0x08,0x3E,0x08,0x08,0x00,0x3E,0x00,0x00,0x00},  // 0xF1
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0x00,0x7F},  // 0xF2
    {0x00,0x71,0x72,0x14,0x7A,0x0E,0x1A,0x2F,0x42,0x00,0x00,0x00},  // 0xF3
    {0x00,0x3E,0x3A,0x3A,0x1A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x00},  // 0xF4
    {0x00,0x1E,0x20,0x30,0x1C,0x22,0x32,0x1C,0x02,0x02,0x3C,0x00},  // 0xF5
    {0x00,0x00,0x00,0x10,0x00,0x7E,0x00,0x00,0x10,0x00,0x00,0x00},  // 0xF6
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x04,0x0C},  // 0xF7
    {0x00,0x08,0x14,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xF8
    {0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xF9
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00},  // 0xFA
    {0x00,0x18,0x08,0x08,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xFB
    {0x00,0x1E,0x0E,0x02,0x1E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xFC
    {0x00,0x1E,0x02,0x0C,0x1E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 0xFD
    {0x00,0x00,0x00,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x00,0x00,0x00},  // 0xFE
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}   // 0xFF
};

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * refresh status LED
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_refresh_status_led (void)
{
    ws2812_refresh (WS2812_STATUS_LEDS);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * refresh display LEDs
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_refresh_display_leds (void)
{
    ws2812_refresh (WS2812_STATUS_LEDS + WS2812_DISPLAY_LEDS);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * refresh ambilight LEDs
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_refresh_ambilight_leds (void)
{
    ws2812_refresh (WS2812_STATUS_LEDS + WS2812_DISPLAY_LEDS + WS2812_AMBILIGHT_LEDS);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set status LED
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_set_status_led (uint_fast8_t r_flag, uint_fast8_t g_flag, uint_fast8_t b_flag)
{
    WS2812_RGB rgb;

    rgb.red     = r_flag ? pwmtable8[MAX_COLOR_STEPS - 1] : 0;
    rgb.green   = g_flag ? pwmtable8[MAX_COLOR_STEPS - 1] : 0;
    rgb.blue    = b_flag ? pwmtable8[MAX_COLOR_STEPS - 1] : 0;

    ws2812_set_led (WS2812_STATUS_LED_OFFSET, &rgb);
    dsp_refresh_status_led ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set display LED to RGB
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_set_display_led (uint_fast16_t n, WS2812_RGB * rgb, uint_fast8_t refresh)
{
	if (n < WS2812_DISPLAY_LEDS)
	{
        uint8_t y;
        uint8_t x;

        y = n / WC_COLUMNS;

        if (y & 0x01)                                       // snake: odd row: count from right to left
        {
            x = n % WC_COLUMNS;
            n = y * WC_COLUMNS + (WC_COLUMNS - 1 - x);
        }

        ws2812_set_led (WS2812_DISPLAY_LED_OFFSET + n, rgb);

        if (refresh)
        {
            dsp_refresh_display_leds ();
        }
	}
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set ambilight LEDs to RGB
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_set_ambilight_led (WS2812_RGB * rgb, uint_fast8_t refresh)
{
    uint_fast16_t   n;

    for (n = 0; n < WS2812_AMBILIGHT_LEDS; n++)
    {
        ws2812_set_led (WS2812_AMBILIGHT_LED_OFFSET + n, rgb);
    }

    if (refresh)
    {
        dsp_refresh_ambilight_leds();
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

    for (idx = 0; idx < WS2812_DISPLAY_LEDS; idx++)
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
dsp_animation_flush (uint_fast8_t flush_ambi)
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

    for (idx = 0; idx < WS2812_DISPLAY_LEDS; idx++)
    {
        if (led.state[idx] & TARGET_STATE)                          // fm: == ???
        {
            dsp_set_display_led (idx, &rgb, 0);
            led.state[idx] |= CURRENT_STATE;                        // we are in sync
        }
        else
        {
            dsp_set_display_led (idx, &rgb0, 0);
            led.state[idx] &= ~CURRENT_STATE;                       // we are in sync
        }
    }

    if (flush_ambi)
    {
        if (ambi_state & TARGET_STATE)
        {
            dsp_set_ambilight_led (&rgb, 0);
            ambi_state |= CURRENT_STATE;
        }
        else
        {
            dsp_set_ambilight_led (&rgb0, 0);
            ambi_state &= ~CURRENT_STATE;
        }

        dsp_refresh_ambilight_leds ();
    }
    else
    {
        dsp_refresh_display_leds ();
    }
    animation_stop_flag = 1;
}

static void
dsp_animation_none (void)
{
    if (animation_start_flag)
    {
        animation_start_flag = 0;
        animation_stop_flag = 0;
        dsp_animation_flush (FALSE);
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
        if (red_step > 0)
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
        }

        if (green_step > 0)
        {
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
        }

        if (blue_step > 0)
        {
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

            for (idx = 0; idx < WS2812_DISPLAY_LEDS; idx++)
            {
                if (led.state[idx] == TARGET_STATE)                           // up
                {
                    dsp_set_display_led (idx, &rgb_up, 0);
                }
                else if (led.state[idx] == CURRENT_STATE)                     // down
                {
                    dsp_set_display_led (idx, &rgb_down, 0);
                }
                else if (led.state[idx] == (CURRENT_STATE | TARGET_STATE))    // on, but no change
                {
                    dsp_set_display_led (idx, &rgb, 0);
                }
            }

            dsp_refresh_display_leds ();
        }
        else
        {
            animation_stop_flag = 1;
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

    for (idx = 0; idx < WS2812_DISPLAY_LEDS; idx++)
    {
        if (led.state[idx] & NEW_STATE)                                // on
        {
            dsp_set_display_led (idx, &rgb, 0);
        }
        else
        {
            dsp_set_display_led (idx, &rgb0, 0);
        }
    }

    dsp_refresh_display_leds ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * roll right
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_animation_roll_right (void)
{
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
        if (cnt < WC_COLUMNS)
        {
            cnt++;                                                  // 1...WC_COLUMNS

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
        if (cnt < WC_COLUMNS)
        {
            cnt++;                                                  // 1...WC_COLUMNS

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
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * roll up
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
dsp_animation_roll_up ()
{
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

        dsp_animation_flush (FALSE);

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

        dsp_animation_flush (FALSE);

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
 * get automatic brightness control flag
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_get_automatic_brightness_control (void)
{
    return automatic_brightness_control;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set automatic brightness control flag
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_set_automatic_brightness_control (uint_fast8_t new_automatic_brightness_control)
{
    automatic_brightness_control = new_automatic_brightness_control;

    if (! automatic_brightness_control)
    {
        dsp_set_brightness (MAX_BRIGHTNESS);
    }
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
    static uint8_t  b[16] = { 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15};
    uint_fast8_t    factor;

    if (brightness == MAX_BRIGHTNESS)
    {
        dimmed_colors.red   = current_colors.red;
        dimmed_colors.green = current_colors.green;
        dimmed_colors.blue  = current_colors.blue;
    }
    else
    {
        factor = b[brightness];

        dimmed_colors.red = (current_colors.red * factor) / MAX_BRIGHTNESS;

        if (current_colors.red > 0 && dimmed_colors.red == 0)
        {
            dimmed_colors.red = 1;
        }

        dimmed_colors.green = (current_colors.green * factor) / MAX_BRIGHTNESS;

        if (current_colors.green > 0 && dimmed_colors.green == 0)
        {
            dimmed_colors.green = 1;
        }

        dimmed_colors.blue = (current_colors.blue * factor) / MAX_BRIGHTNESS;

        if (current_colors.blue > 0 && dimmed_colors.blue == 0)
        {
            dimmed_colors.blue = 1;
        }
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
        dsp_animation_flush (TRUE);
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
        dsp_animation_flush (TRUE);
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
        dsp_animation_flush (TRUE);
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
        dsp_animation_flush (TRUE);
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
        dsp_animation_flush (TRUE);
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
        dsp_animation_flush (TRUE);
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * get colors
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_get_colors (DSP_COLORS * rgb)
{
    rgb->red    = current_colors.red;
    rgb->green  = current_colors.green;
    rgb->blue   = current_colors.blue;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set colors
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_set_colors (DSP_COLORS * rgb)
{
    current_colors.red      = (rgb->red < MAX_COLOR_STEPS)   ? rgb->red : MAX_COLOR_STEPS;
    current_colors.green    = (rgb->green < MAX_COLOR_STEPS) ? rgb->green : MAX_COLOR_STEPS;
    current_colors.blue     = (rgb->blue < MAX_COLOR_STEPS)  ? rgb->blue : MAX_COLOR_STEPS;

    dsp_calc_dimmed_colors ();
    dsp_animation_flush (TRUE);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * get brightness
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
dsp_get_brightness (void)
{
    return brightness;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set brightness
 *  MAX_BRIGHTNESS  = full brightness
 *   0              = lowest brightness
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_set_brightness (uint_fast8_t new_brightness)
{
    if (new_brightness > MAX_BRIGHTNESS)
    {
        brightness = MAX_BRIGHTNESS;
    }
    else
    {
        brightness = new_brightness;
    }

    dsp_calc_dimmed_colors ();
    dsp_animation_flush (TRUE);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * decrement brightness
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_decrement_brightness (void)
{
    if (brightness > 0)
    {
        brightness--;
        dsp_calc_dimmed_colors ();
        dsp_animation_flush (TRUE);
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * increment brightness
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_increment_brightness (void)
{
    if (brightness < MAX_BRIGHTNESS)
    {
        brightness++;
        dsp_calc_dimmed_colors ();
        dsp_animation_flush (TRUE);
    }
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

        for (j = 0; j < WS2812_STATUS_LEDS + WS2812_DISPLAY_LEDS; j++)
        {
            ws2812_set_led (j, &rgb);
        }

        dsp_refresh_display_leds ();
        delay_sec (3);
    }

    dsp_animation_flush (FALSE);
    dsp_set_status_led (0, 0, 0);
}


static void
dsp_show_char (uint_fast8_t y, uint_fast8_t x, unsigned char ch, uint_fast8_t col_offset)
{
    uint_fast8_t    line;
    uint_fast8_t    col;

    for (line = 0; line < FONT_LINES; line++)
    {
        for (col = 0; (col_offset < FONT_COLS); col++, col_offset++)
        {
            if (y + line < WC_ROWS && x + col < WC_COLUMNS)
            {
                if (font[ch][line] & (1<<(FONT_COLS - col_offset)))
                {
                    led.matrix[y + line][x + col] = NEW_STATE;
                }
                else
                {
                    led.matrix[y + line][x + col] = CURRENT_STATE;
                }
            }
        }
    }
}

static void
dsp_message_with_offset (unsigned char * str, uint_fast8_t n)
{
    uint_fast8_t     message_y;
    uint_fast8_t     message_x;

    message_y = (WC_ROWS - FONT_LINES) / 2;
    message_x = 0;

    dsp_show_char (message_y, message_x, *str, n);
    str++;
    message_x = FONT_COLS - n;

    while (*str && message_x < WC_COLUMNS)
    {
        dsp_show_char (message_y, message_x, *str++, 0);
        message_x += FONT_COLS;
    }

    while (message_x < WC_COLUMNS)
    {
        dsp_show_char (message_y, message_x, ' ', 0);
        message_x += FONT_COLS;
    }

    dsp_show_new_display ();
}


/*-------------------------------------------------------------------------------------------------------------------------------------------
 * Display message
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
dsp_message (char * str)
{
    uint_fast8_t    n;
    unsigned char * p;

    reset_led_states ();

    for (p = (unsigned char *) str; *p; p++)
    {
        for (n = 0; n < FONT_COLS; n++)
        {
            dsp_message_with_offset (p, n);
            delay_msec (100);
        }
    }
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
    uint8_t                 brightness8;
    uint8_t                 automatic_brightness_control8;

    if (eeprom_is_up)
    {
        if (eeprom_read (EEPROM_DATA_OFFSET_DSP_COLORS, (uint8_t *) &packed_rgb_color, sizeof(PACKED_DSP_COLORS)) &&
            eeprom_read (EEPROM_DATA_OFFSET_DISPLAY_MODE, &display_mode8, sizeof(display_mode8)) &&
            eeprom_read (EEPROM_DATA_OFFSET_ANIMATION_MODE, &animation_mode8, sizeof(animation_mode8)) &&
            eeprom_read (EEPROM_DATA_OFFSET_BRIGHTNESS, &brightness8, EEPROM_DATA_SIZE_BRIGHTNESS) &&
            eeprom_read (EEPROM_DATA_OFFSET_AUTO_BRIGHTNESS, &automatic_brightness_control8, EEPROM_DATA_SIZE_AUTO_BRIGHTNESS))
        {
            if (display_mode8 >= MODES_COUNT)
            {
                display_mode8 = 0;
            }

            if (animation_mode8 >= ANIMATION_MODES)
            {
                animation_mode8 = 0;
            }

            if (brightness8 > MAX_BRIGHTNESS)
            {
                brightness8 = MAX_BRIGHTNESS;
            }

            if (automatic_brightness_control8 != 0x01)        // only 0x00 or 0x01 allowed, 0xFF means empty EEPROM
            {
                automatic_brightness_control8 = 0;
            }

            current_colors.red              = packed_rgb_color.red;
            current_colors.green            = packed_rgb_color.green;
            current_colors.blue             = packed_rgb_color.blue;
            display_mode                    = display_mode8;
            animation_mode                  = animation_mode8;
            brightness                      = brightness8;
            automatic_brightness_control    = automatic_brightness_control8;

            dsp_calc_dimmed_colors ();

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
    uint8_t                 brightness8;
    uint8_t                 automatic_brightness_control8;

    packed_rgb_color.red            = current_colors.red;
    packed_rgb_color.green          = current_colors.green;
    packed_rgb_color.blue           = current_colors.blue;

    display_mode8                   = display_mode;
    animation_mode8                 = animation_mode;
    brightness8                     = brightness;
    automatic_brightness_control8   = automatic_brightness_control;

    if (eeprom_is_up)
    {
        if (eeprom_write (EEPROM_DATA_OFFSET_DSP_COLORS, (uint8_t *) &packed_rgb_color, sizeof(PACKED_DSP_COLORS)) &&
            eeprom_write (EEPROM_DATA_OFFSET_DISPLAY_MODE, &display_mode8, sizeof(display_mode8)) &&
            eeprom_write (EEPROM_DATA_OFFSET_ANIMATION_MODE, &animation_mode8, sizeof(animation_mode8)) &&
            eeprom_write (EEPROM_DATA_OFFSET_BRIGHTNESS, &brightness8, EEPROM_DATA_SIZE_BRIGHTNESS) &&
            eeprom_write (EEPROM_DATA_OFFSET_AUTO_BRIGHTNESS, &automatic_brightness_control8, EEPROM_DATA_SIZE_AUTO_BRIGHTNESS))

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
