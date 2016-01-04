/*-------------------------------------------------------------------------------------------------------------------------------------------
 * tables12h.c - tables for WordClock12h
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

#include "tables12h.h"

const char * tbl_modes[MODES_COUNT] =
{
    "WESSI",
    "OSSI",
    "RHEIN-RUHR",
    "SCHWABEN"
};

const uint8_t it_is[2] = { WP_ES, WP_IST };

const uint8_t tbl_hours[HOUR_MODES_COUNT][HOUR_COUNT][MAX_HOUR_WORDS] =
{
    {                                                               // tbl_hours[0][] = hh:00
        {WP_ZWOELF,   WP_UHR},                                      // 00:00
        {WP_EIN,      WP_UHR},                                      // 01:00
        {WP_ZWEI,     WP_UHR},                                      // 02:00
        {WP_DREI_2,   WP_UHR},                                      // 03:00
        {WP_VIER_2,   WP_UHR},                                      // 04:00
        {WP_FUENF_2,  WP_UHR},                                      // 05:00
        {WP_SECHS,    WP_UHR},                                      // 06:00
        {WP_SIEBEN,   WP_UHR},                                      // 07:00
        {WP_ACHT,     WP_UHR},                                      // 08:00
        {WP_NEUN,     WP_UHR},                                      // 09:00
        {WP_ZEHN_2,   WP_UHR},                                      // 10:00
        {WP_ELF,      WP_UHR}                                       // 11:00
    },
    {                                                               // tbl_hours[1][] = hh:mm
        {WP_ZWOELF},                                                // 00:mm
        {WP_EINS},                                                  // 01:mm
        {WP_ZWEI},                                                  // 02:mm
        {WP_DREI_2},                                                // 03:mm
        {WP_VIER_2},                                                // 04:mm
        {WP_FUENF_2},                                               // 05:mm
        {WP_SECHS},                                                 // 06:mm
        {WP_SIEBEN},                                                // 07:mm
        {WP_ACHT},                                                  // 08:mm
        {WP_NEUN},                                                  // 09:mm
        {WP_ZEHN_2},                                                // 10:mm
        {WP_ELF}                                                    // 11:mm
    }
};

const struct minute_display tbl_minutes[MODES_COUNT][MINUTE_COUNT] =
{
    {                                                               // tbl_minutes[0][] = WESSI
        {0},                                                        // 00
        {1, 0, {WP_FUENF_1, WP_NACH          }},                    // 05
        {1, 0, {WP_ZEHN_1,  WP_NACH          }},                    // 10
        {1, 0, {WP_VIERTEL, WP_NACH          }},                    // 15
        {1, 1, {WP_ZEHN_1,  WP_VOR,  WP_HALB }},                    // 20
        {1, 1, {WP_FUENF_1, WP_VOR,  WP_HALB }},                    // 25
        {1, 1, {WP_HALB                      }},                    // 30
        {1, 1, {WP_FUENF_1, WP_NACH, WP_HALB }},                    // 35
        {1, 1, {WP_ZEHN_1,  WP_NACH, WP_HALB }},                    // 40
        {1, 1, {WP_VIERTEL, WP_VOR           }},                    // 45
        {1, 1, {WP_ZEHN_1,  WP_VOR           }},                    // 50
        {1, 1, {WP_FUENF_1, WP_VOR           }},                    // 55
    },

    {                                                               // tbl_minutes[1][] = OSSI
        {0},                                                        // 00
        {1, 0, {WP_FUENF_1,     WP_NACH          }},                // 05
        {1, 0, {WP_ZEHN_1,      WP_NACH          }},                // 10
        {1, 1, {WP_VIERTEL,                      }},                // 45
        {1, 1, {WP_ZEHN_1,      WP_VOR,  WP_HALB }},                // 20
        {1, 1, {WP_FUENF_1,     WP_VOR,  WP_HALB }},                // 25
        {1, 1, {WP_HALB                          }},                // 30
        {1, 1, {WP_FUENF_1,     WP_NACH, WP_HALB }},                // 35
        {1, 1, {WP_ZEHN_1,      WP_NACH, WP_HALB }},                // 40
        {1, 1, {WP_DREIVIERTEL                   }},                // 15
        {1, 1, {WP_ZEHN_1,      WP_VOR           }},                // 50
        {1, 1, {WP_FUENF_1,     WP_VOR           }},                // 55
    },

    {                                                               // tbl_minutes[2][] = RHEIN-RUHR
        {0},                                                        // 00
        {1, 0, {WP_FUENF_1,     WP_NACH          }},                // 05
        {1, 0, {WP_ZEHN_1,      WP_NACH          }},                // 10
        {1, 0, {WP_VIERTEL,     WP_NACH          }},                // 15
        {1, 0, {WP_ZWANZIG,     WP_NACH          }},                // 20
        {1, 1, {WP_FUENF_1,     WP_VOR,  WP_HALB }},                // 25
        {1, 1, {WP_HALB                          }},                // 30
        {1, 1, {WP_FUENF_1,     WP_NACH, WP_HALB }},                // 35
        {1, 1, {WP_ZWANZIG,     WP_VOR           }},                // 40
        {1, 1, {WP_VIERTEL,     WP_VOR           }},                // 45
        {1, 1, {WP_ZEHN_1,      WP_VOR           }},                // 50
        {1, 1, {WP_FUENF_1,     WP_VOR           }},                // 55
    },

    {                                                               // tbl_minutes[3][] = SCHWABEN
        {0},                                                        // 00
        {1, 0, {WP_FUENF_1,     WP_NACH          }},                // 05
        {1, 0, {WP_ZEHN_1,      WP_NACH          }},                // 10
        {1, 1, {WP_VIERTEL,                      }},                // 45
        {1, 0, {WP_ZWANZIG,     WP_NACH          }},                // 20
        {1, 1, {WP_FUENF_1,     WP_VOR,  WP_HALB }},                // 25
        {1, 1, {WP_HALB                          }},                // 30
        {1, 1, {WP_FUENF_1,     WP_NACH, WP_HALB }},                // 35
        {1, 1, {WP_ZWANZIG,     WP_VOR           }},                // 40
        {1, 1, {WP_DREIVIERTEL,                  }},                // 15
        {1, 1, {WP_ZEHN_1,      WP_VOR           }},                // 50
        {1, 1, {WP_FUENF_1,     WP_VOR           }},                // 55
    }
};

const struct WordIllu illumination[WP_COUNT] =
{
    {0,0,0},                                                        //  0 = WP_END_OF_WORDS = ""
    {0,0,2},                                                        //  1 = WP_ES           = "ES"
    {0,3,3},                                                        //  2 = WP_IST          = "IST"
    {0,7,4},                                                        //  3 = WP_FUENF_1      = "FÜNF"
    {1,0,4},                                                        //  4 = WP_ZEHN_1       = "ZEHN"
    {1,4,7},                                                        //  5 = WP_ZWANZIG      = "ZWANZIG"
    {2,0,4},                                                        //  6 = WP_DREI_1       = "DREI"
    {2,4,4},                                                        //  7 = WP_VIER         = "VIER"
    {2,4,7},                                                        //  8 = WP_VIERTEL      = "VIERTEL"
    {2,0,11},                                                       //  9 = WP_DREIVIERTEL  = "DREIVIERTEL"
    {3,2,4},                                                        // 10 = WP_NACH         = "NACH"
    {3,6,3},                                                        // 11 = WP_VOR          = "VOR"
    {4,0,4},                                                        // 12 = WP_HALB         = "HALB"
    {4,5,5},                                                        // 13 = WP_ZWOELF       = "ZWÖLF"
    {5,0,4},                                                        // 14 = WP_ZWEI         = "ZWEI"
    {5,2,3},                                                        // 15 = WP_EIN          = "EIN"
    {5,2,4},                                                        // 16 = WP_EINS         = "EINS"
    {5,5,6},                                                        // 17 = WP_SIEBEN       = "SIEBEN"
    {6,1,4},                                                        // 18 = WP_DREI_2       = "DREI"
    {6,7,4},                                                        // 19 = WP_FUENF_2      = "FÜNF"
    {7,0,3},                                                        // 20 = WP_ELF          = "ELF"
    {7,3,4},                                                        // 21 = WP_NEUN         = "NEUN"
    {7,7,4},                                                        // 22 = WP_VIER_2       = "VIER"
    {8,1,4},                                                        // 23 = WP_ACHT         = "ACHT"
    {8,5,4},                                                        // 24 = WP_ZEHN_2       = "ZEHN"
    {9,1,5},                                                        // 25 = WP_SECHS        = "SECHS"
    {9,8,3},                                                        // 26 = WP_UHR          = "UHR"
};

const char * display[WC_ROWS] =
{
    "ESKISTLFÜNF",
    "ZEHNZWANZIG",
    "DREIVIERTEL",
    "TGNACHVORJM",
    "HALBQZWÖLFP",
    "ZWEINSIEBEN",
    "KDREIRHFÜNF",
    "ELFNEUNVIER",
    "WACHTZEHNRS",
    "BSECHSFMUHR"
};

#endif
