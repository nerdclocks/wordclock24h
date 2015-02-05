// Word-Arrays for 24h Wordclock, see http://www.mikrocontroller.net/articles/WordClock24h
// Von-Neumann-Variant, Data from Wc24h1816_08-30-5.Feb.2015 CodeGen v0.14
// Code-Generator see https://gist.github.com/TorstenC/aec0724be4afcd1d7545
// tbl_modes[MODES_COUNT]: 793 bytes
// tbl_hours[HOUR_MODES_COUNT][HOUR_COUNT][MAX_HOUR_WORDS]: 800 bytes
// tbl_minutes[MINUTE_MODES_COUNT][MINUTE_COUNT]: 5280 bytes
// illumination[1][WP_COUNT]: 225 bytes
// total (tbl_minutes + tbl_hours + tbl_modes + illumination): 7098 bytes

#ifndef TABLES_H
#define TABLES_H

#include <stdint.h>

#define MODES_COUNT 18 // count of different display modes
#define MAX_HOUR_WORDS 4 // how many words for hour display (no end token)
#define HOUR_COUNT 25 // 24 plus one to distinguish between before and after full hour
#define MAX_MINUTE_WORDS 7 // how many words for minute display (no end token)
#define MINUTE_COUNT 60
#define WC_COLUMNS 18
#define WC_ROWS 16

struct WordIllu {
uint8_t row;
uint8_t col;
uint8_t len;
};

struct Modes {
uint8_t minute_txt; // we don´t need minute_txt_last
uint8_t hour_txt;
const char* description;
};
struct MinuteDisplay {
uint8_t hourOffset; // offset for hour display
uint8_t wordIdx[MAX_MINUTE_WORDS];
};
enum HourMode {
HM_0, // 0 = Mode 0: "leer, überspringen"
HM_1, // 1 = Mode 1: "MM - HH (12)"
HM_2, // 2 = Mode 2: "MM - HH UHR (12)"
HM_3, // 3 = Mode 3: "MM - HH UHR (12) NACHTS"
HM_4, // 4 = Mode 4: "MM - HH UHR (24)"
HM_5, // 5 = Mode 5: "HH UHR (12) - MM"
HM_6, // 6 = Mode 6: "HH UHR 24) - MM"
HM_7, // 7 = Mode 7: "MITTERNACHT (0 UHR)"
HOUR_MODES_COUNT // Number of HourModes
};

enum MinuteMode {
MM_1, // 0 = Mode 1: "MM NACH"
MM_2, // 1 = Mode 2: "MM MINUTEN NACH"
MM_3, // 2 = Mode 3: "OSSI - MM MINUTEN NACH (VIERTEL NACH, HALB, VIERTEL VOR)"
MM_4, // 3 = Mode 4: "OESI - MINUTEN NACH (VIERTEL NACH, HALB, DREIVIERTEL)"
MM_5, // 4 = Mode 5: "RHEIN/ RUHR - MINUTEN NACH (VIERTEL, HALB, DREIVIERTEL)"
MM_6, // 5 = Mode 6: "SCHWABEN - MM MINUTEN NACH (VIERTEL NACH, HALB, DREIVIERTEL)"
MM_7, // 6 = Mode 7: "WESSI - MM MINUTEN NACH (VIERTEL, HALB, DREIVIERTEL)"
MM_8, // 7 = Mode 8: "MM"
MM_9, // 8 = Mode 9: "UND MM MINUTEN"
MM_10, // 9 = Mode 10: "MM MINUTEN VOR"
MM_11, // 10 = Mode 11: "TEMPERATUR "CC GRAD""
MINUTE_MODES_COUNT // Number of MinuteModes
};
enum WordPos {
WP_END_OF_WORDS, // 0 = "0_END_OF_WORDS" = ""
WP_ES, // 1 = "1_ES" = "ES"
WP_IST, // 2 = "2_IST" = "IST"
WP_VIERTEL_1, // 3 = "3_VIERTEL" = "VIERTEL"
WP_EIN_1, // 4 = "4_EIN" = "EIN"
WP_EINS_1, // 5 = "5_EINS" = "EINS"
WP_IN, // 6 = "6_IN" = "IN"
WP_DREI_1, // 7 = "7_DREI" = "DREI"
WP_EIN_2, // 8 = "8_EIN" = "EIN"
WP_EINE_1, // 9 = "9_EINE" = "EINE"
WP_EINER, // 10 = "10_EINER" = "EINER"
WP_SECH_1, // 11 = "11_SECH" = "SECH"
WP_SECHS_1, // 12 = "12_SECHS" = "SECHS"
WP_SIEB_1, // 13 = "13_SIEB" = "SIEB"
WP_SIEBEN_1, // 14 = "14_SIEBEN" = "SIEBEN"
WP_ELF_1, // 15 = "15_ELF" = "ELF"
WP_FUENF_1, // 16 = "16_FUENF" = "FÜNF"
WP_NEUN_1, // 17 = "17_NEUN" = "NEUN"
WP_VIER_1, // 18 = "18_VIER" = "VIER"
WP_ACHT_1, // 19 = "19_ACHT" = "ACHT"
WP_NULL_1, // 20 = "20_NULL" = "NULL"
WP_ZWEI_1, // 21 = "21_ZWEI" = "ZWEI"
WP_ZWOELF_1, // 22 = "22_ZWOELF" = "ZWÖLF"
WP_ZEHN_1, // 23 = "23_ZEHN" = "ZEHN"
WP_UND_1, // 24 = "24_UND" = "UND"
WP_ZWANZIG_1, // 25 = "25_ZWANZIG" = "ZWANZIG"
WP_VIERZIG_1, // 26 = "26_VIERZIG" = "VIERZIG"
WP_DREISSIG_1, // 27 = "27_DREISSIG" = "DREISSIG"
WP_FUENFZIG_1, // 28 = "28_FUENFZIG" = "FÜNFZIG"
WP_UHR_1, // 29 = "29_UHR" = "UHR"
WP_MINUTE_1, // 30 = "30_MINUTE" = "MINUTE"
WP_MINUTEN_1, // 31 = "31_MINUTEN" = "MINUTEN"
WP_VOR_1, // 32 = "32_VOR" = "VOR"
WP_UND_2, // 33 = "33_UND" = "UND"
WP_NACH_1, // 34 = "34_NACH" = "NACH"
WP_EIN_3, // 35 = "35_EIN" = "EIN"
WP_DREIVIERTEL, // 36 = "36_DREIVIERTEL" = "DREIVIERTEL"
WP_VIERTEL_2, // 37 = "37_VIERTEL" = "VIERTEL"
WP_HALB, // 38 = "38_HALB" = "HALB"
WP_SIEB_2, // 39 = "39_SIEB" = "SIEB"
WP_SIEBEN_2, // 40 = "40_SIEBEN" = "SIEBEN"
WP_NEUN_2, // 41 = "41_NEUN" = "NEUN"
WP_NULL_2, // 42 = "42_NULL" = "NULL"
WP_ZWEI_2, // 43 = "43_ZWEI" = "ZWEI"
WP_EIN_4, // 44 = "44_EIN" = "EIN"
WP_EINE_2, // 45 = "45_EINE" = "EINE"
WP_FUENF_2, // 46 = "46_FUENF" = "FÜNF"
WP_SECH_2, // 47 = "47_SECH" = "SECH"
WP_SECHS_2, // 48 = "48_SECHS" = "SECHS"
WP_ACHT_2, // 49 = "49_ACHT" = "ACHT"
WP_VIER_2, // 50 = "50_VIER" = "VIER"
WP_DREI_2, // 51 = "51_DREI" = "DREI"
WP_EIN_5, // 52 = "52_EIN" = "EIN"
WP_EINS_2, // 53 = "53_EINS" = "EINS"
WP_UND_3, // 54 = "54_UND" = "UND"
WP_ELF_2, // 55 = "55_ELF" = "ELF"
WP_ZEHN_2, // 56 = "56_ZEHN" = "ZEHN"
WP_ZWANZIG_2, // 57 = "57_ZWANZIG" = "ZWANZIG"
WP_GRAD, // 58 = "58_GRAD" = "GRAD"
WP_DREISSIG_2, // 59 = "59_DREISSIG" = "DREISSIG"
WP_VIERZIG_2, // 60 = "60_VIERZIG" = "VIERZIG"
WP_ZWOELF_2, // 61 = "61_ZWÖLF" = "ZWÖLF"
WP_FUENFZIG_2, // 62 = "62_FUENFZIG" = "FÜNFZIG"
WP_MINUTE_2, // 63 = "63_MINUTE" = "MINUTE"
WP_MINUTEN_2, // 64 = "64_MINUTEN" = "MINUTEN"
WP_UHR_2, // 65 = "65_UHR" = "UHR"
WP_FRUEH, // 66 = "66_FRUEH" = "FRÜH"
WP_VOR_2, // 67 = "67_VOR" = "VOR"
WP_ABENDS, // 68 = "68_ABENDS" = "ABENDS"
WP_MITTERNACHT, // 69 = "69_MITTERNACHT" = "MITTERNACHT"
WP_NACH_2, // 70 = "70_NACH" = "NACH"
WP_NACHTS, // 71 = "71_NACHTS" = "NACHTS"
WP_MORGENS, // 72 = "72_MORGENS" = "MORGENS"
WP_WARM, // 73 = "73_WARM" = "WARM"
WP_MITTAGS, // 74 = "74_MITTAGS" = "MITTAGS"
WP_COUNT, // number of words
};
extern const struct Modes tbl_modes[MODES_COUNT];
extern const uint8_t it_is[2];
extern const uint8_t tbl_hours[HOUR_MODES_COUNT][HOUR_COUNT][MAX_HOUR_WORDS];
extern const struct MinuteDisplay tbl_minutes[MINUTE_MODES_COUNT][MINUTE_COUNT];
extern const struct WordIllu illumination[1][WP_COUNT]; // dummy-dimension for screen-saver-variant with capital ß
extern const char* display[1][WC_ROWS]; // dummy-dimension for screen-saver-variant with capital ß
#endif // TABLES_H
