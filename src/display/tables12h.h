/*-------------------------------------------------------------------------------------------------------------------------------------------
 * tables12h.h - extern declarations for WordClock12h
 *
 * Copyright (c) 2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#include "wclock24h-config.h"
#if WCLOCK24H == 0

#ifndef TABLES12H_H
#define TABLES12H_H

#include <stdint.h>

#define WC_ROWS                 10
#define WC_COLUMNS              11

#define MODES_COUNT             4
#define MINUTE_COUNT            12

#define HOUR_MODES_COUNT        2
#define HOUR_COUNT              12
#define MAX_HOUR_WORDS          2

#define MAX_MINUTE_WORDS        4                               // 3 + 1 for termination

struct MinuteDisplay12
{
    uint8_t hour_mode;
    uint8_t hourOffset;
    uint8_t wordIdx[MAX_MINUTE_WORDS];
};

struct WordIllu
{
    uint8_t row;
    uint8_t col;
    uint8_t len;
};

extern const char * tbl_modes[MODES_COUNT];

enum wc12h_words
{
    WP_END_OF_WORDS,
    WP_ES,
    WP_IST,
    WP_FUENF_1,
    WP_ZEHN_1,
    WP_ZWANZIG,
    WP_DREI_1,
    WP_VIER,
    WP_VIERTEL,
    WP_DREIVIERTEL,
    WP_NACH,
    WP_VOR,
    WP_HALB,
    WP_ZWOELF,
    WP_ZWEI,
    WP_EIN,
    WP_EINS,
    WP_SIEBEN,
    WP_DREI_2,
    WP_FUENF_2,
    WP_ELF,
    WP_NEUN,
    WP_VIER_2,
    WP_ACHT,
    WP_ZEHN_2,
    WP_SECHS,
    WP_UHR,
    WP_COUNT
};

extern const char *     tbl_modes[MODES_COUNT];
extern const uint8_t    tbl_hours[HOUR_MODES_COUNT][HOUR_COUNT][MAX_HOUR_WORDS];
extern const struct     MinuteDisplay12 tbl_minutes[MODES_COUNT][MINUTE_COUNT];
extern const struct     WordIllu illumination[WP_COUNT];
extern const char *     display[WC_ROWS];

#endif // TABLES12H_H

#endif
