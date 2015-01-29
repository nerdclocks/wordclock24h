// Mock-up arrays for 24h Wordclock, see http://www.mikrocontroller.net/articles/WordClock24h
// Von-Neumann-Variant, Data from WC24h_18x16_V2_2010_2003_15_Dez_2014, CodeGen v0.09
// Code-Generator see https://gist.github.com/TorstenC/aec0724be4afcd1d7545

#define DisplayX 18 // deprecated
#define WC_COLUMNS 18
#define DisplayY 16 // deprecated
#define WC_ROWS 16

struct WordIllu {
uint8_t row;
uint8_t col;
uint8_t len;
};

#ifdef TABLES_EXTERN
extern const char* display[1][WC_ROWS];
#else
const char* display[1][WC_ROWS]= {{ // dummy-dimension for screen-saver-variant with capital ß
"ES#IST#VIERTELEINS",
"DREINERSECHSIEBEN#",
"ELFÜNFNEUNVIERACHT",
"NULLZWEI#ZWÖLFZEHN",
"UND#ZWANZIGVIERZIG",
"DREISSIGFÜNFZIGUHR",
"MINUTEN#VORBISNACH",
"UNDHALBDREIVIERTEL",
"SIEBENEUNULLZWEINE",
"FÜNFSECHSACHTVIER#",
"DREINSUND#ELF#ZEHN",
"ZWANZIG###DREISSIG",
"VIERZIGZWÖLFÜNFZIG",
"MINUTENUHR#FRÜHVOR",
"ABENDSMITTERNACHTS",
"MORGENS....MITTAGS"
}};
#endif

enum WordPos {
WP_DUMMY, // 0 = "0_DUMMY" = ""
WP_ES, // 1 = "1_ES" = "ES"
WP_IST, // 2 = "2_IST" = "IST"
WP_VIERTEL_1, // 3 = "3_VIERTEL" = "VIERTEL"
WP_DUMMY_1, // 4 = "4_DUMMY" = ""
WP_DUMMY_2, // 5 = "5_DUMMY" = ""
WP_EIN_1, // 6 = "6_EIN" = "EIN"
WP_EINS_1, // 7 = "7_EINS" = "EINS"
WP_IN, // 8 = "8_IN" = "IN"
WP_DREI_1, // 9 = "9_DREI" = "DREI"
WP_EIN_2, // 10 = "10_EIN" = "EIN"
WP_EINE_1, // 11 = "11_EINE" = "EINE"
WP_EINER, // 12 = "12_EINER" = "EINER"
WP_SECH_1, // 13 = "13_SECH" = "SECH"
WP_SECHS_1, // 14 = "14_SECHS" = "SECHS"
WP_SIEB_1, // 15 = "15_SIEB" = "SIEB"
WP_SIEBEN_1, // 16 = "16_SIEBEN" = "SIEBEN"
WP_ELF_1, // 17 = "17_ELF" = "ELF"
WP_FUENF_1, // 18 = "18_FUENF" = "FÜNF"
WP_NEUN_1, // 19 = "19_NEUN" = "NEUN"
WP_VIER_1, // 20 = "20_VIER" = "VIER"
WP_ACHT_1, // 21 = "21_ACHT" = "ACHT"
WP_NULL_1, // 22 = "22_NULL" = "NULL"
WP_ZWEI_1, // 23 = "23_ZWEI" = "ZWEI"
WP_ZWOELF_1, // 24 = "24_ZWOELF" = "ZWÖLF"
WP_ZEHN_1, // 25 = "25_ZEHN" = "ZEHN"
WP_UND_1, // 26 = "26_UND" = "UND"
WP_ZWANZIG_1, // 27 = "27_ZWANZIG" = "ZWANZIG"
WP_VIERZIG_1, // 28 = "28_VIERZIG" = "VIERZIG"
WP_DREISSIG_1, // 29 = "29_DREISSIG" = "DREISSIG"
WP_FUENFZIG_1, // 30 = "30_FUENFZIG" = "FÜNFZIG"
WP_UHR_1, // 31 = "31_UHR" = "UHR"
WP_MINUTE_1, // 32 = "32_MINUTE" = "MINUTE"
WP_MINUTEN_1, // 33 = "33_MINUTEN" = "MINUTEN"
WP_VOR_1, // 34 = "34_VOR" = "VOR"
WP_BIS, // 35 = "35_BIS" = "BIS"
WP_NACH_1, // 36 = "36_NACH" = "NACH"
WP_UND_2, // 37 = "37_UND" = "UND"
WP_HALB, // 38 = "38_HALB" = "HALB"
WP_DREIVIERTEL, // 39 = "39_DREIVIERTEL" = "DREIVIERTEL"
WP_VIERTEL_2, // 40 = "40_VIERTEL" = "VIERTEL"
WP_SIEB_2, // 41 = "41_SIEB" = "SIEB"
WP_SIEBEN_2, // 42 = "42_SIEBEN" = "SIEBEN"
WP_NEUN_2, // 43 = "43_NEUN" = "NEUN"
WP_NULL_2, // 44 = "44_NULL" = "NULL"
WP_ZWEI_2, // 45 = "45_ZWEI" = "ZWEI"
WP_EIN_3, // 46 = "46_EIN" = "EIN"
WP_EINE_2, // 47 = "47_EINE" = "EINE"
WP_FUENF_2, // 48 = "48_FUENF" = "FÜNF"
WP_SECH_2, // 49 = "49_SECH" = "SECH"
WP_SECHS_2, // 50 = "50_SECHS" = "SECHS"
WP_ACHT_2, // 51 = "51_ACHT" = "ACHT"
WP_VIER_2, // 52 = "52_VIER" = "VIER"
WP_DREI_2, // 53 = "53_DREI" = "DREI"
WP_EIN_4, // 54 = "54_EIN" = "EIN"
WP_EINS_2, // 55 = "55_EINS" = "EINS"
WP_UND_3, // 56 = "56_UND" = "UND"
WP_ELF_2, // 57 = "57_ELF" = "ELF"
WP_ZEHN_2, // 58 = "58_ZEHN" = "ZEHN"
WP_ZWANZIG_2, // 59 = "59_ZWANZIG" = "ZWANZIG"
WP_DREISSIG_2, // 60 = "60_DREISSIG" = "DREISSIG"
WP_VIERZIG_2, // 61 = "61_VIERZIG" = "VIERZIG"
WP_ZWOELF_2, // 62 = "62_ZWOELF" = "ZWÖLF"
WP_FUENFZIG_2, // 63 = "63_FUENFZIG" = "FÜNFZIG"
WP_MINUTE_2, // 64 = "64_MINUTE" = "MINUTE"
WP_MINUTEN_2, // 65 = "65_MINUTEN" = "MINUTEN"
WP_UHR_2, // 66 = "66_UHR" = "UHR"
WP_FRUEH, // 67 = "67_FRUEH" = "FRÜH"
WP_VOR_2, // 68 = "68_VOR" = "VOR"
WP_ABENDS, // 69 = "69_ABENDS" = "ABENDS"
WP_MITTERNACHT, // 70 = "70_MITTERNACHT" = "MITTERNACHT"
WP_NACH_2, // 71 = "71_NACH" = "NACH"
WP_NACHTS, // 72 = "72_NACHTS" = "NACHTS"
WP_MORGENS, // 73 = "73_MORGENS" = "MORGENS"
WP_LED_1, // 74 = "74_LED1" = "._1"
WP_LED_2, // 75 = "75_LED2" = "._2"
WP_LED_3, // 76 = "76_LED3" = "._3"
WP_LED_4, // 77 = "77_LED4" = "._4"
WP_MITTAG_S, // 78 = "78_MITTAGS" = "MITTAGS"
WP_COUNT, // number of words
WpCount = WP_COUNT // deprecated
};

#ifdef TABLES_EXTERN
extern const struct WordIllu illumination[1][WP_COUNT];
#else
const struct WordIllu illumination[1][WP_COUNT]= {{ // dummy-dimension for screen-saver-variant with capital ß
{0,0,0}, // 0 = WP_DUMMY = ""
{0,0,2}, // 1 = WP_ES = "ES"
{0,3,3}, // 2 = WP_IST = "IST"
{0,7,7}, // 3 = WP_VIERTEL_1 = "VIERTEL"
{0,0,0}, // 4 = WP_DUMMY_1 = ""
{0,0,0}, // 5 = WP_DUMMY_2 = ""
{0,14,3}, // 6 = WP_EIN_1 = "EIN"
{0,14,4}, // 7 = WP_EINS_1 = "EINS"
{0,15,2}, // 8 = WP_IN = "IN"
{1,0,4}, // 9 = WP_DREI_1 = "DREI"
{1,2,3}, // 10 = WP_EIN_2 = "EIN"
{1,2,4}, // 11 = WP_EINE_1 = "EINE"
{1,2,5}, // 12 = WP_EINER = "EINER"
{1,7,4}, // 13 = WP_SECH_1 = "SECH"
{1,7,5}, // 14 = WP_SECHS_1 = "SECHS"
{1,11,4}, // 15 = WP_SIEB_1 = "SIEB"
{1,11,6}, // 16 = WP_SIEBEN_1 = "SIEBEN"
{2,0,3}, // 17 = WP_ELF_1 = "ELF"
{2,2,4}, // 18 = WP_FUENF_1 = "FÜNF"
{2,6,4}, // 19 = WP_NEUN_1 = "NEUN"
{2,10,4}, // 20 = WP_VIER_1 = "VIER"
{2,14,4}, // 21 = WP_ACHT_1 = "ACHT"
{3,0,4}, // 22 = WP_NULL_1 = "NULL"
{3,4,4}, // 23 = WP_ZWEI_1 = "ZWEI"
{3,9,5}, // 24 = WP_ZWOELF_1 = "ZWÖLF"
{3,14,4}, // 25 = WP_ZEHN_1 = "ZEHN"
{4,0,3}, // 26 = WP_UND_1 = "UND"
{4,4,7}, // 27 = WP_ZWANZIG_1 = "ZWANZIG"
{4,11,7}, // 28 = WP_VIERZIG_1 = "VIERZIG"
{5,0,8}, // 29 = WP_DREISSIG_1 = "DREISSIG"
{5,8,7}, // 30 = WP_FUENFZIG_1 = "FÜNFZIG"
{5,15,3}, // 31 = WP_UHR_1 = "UHR"
{6,0,6}, // 32 = WP_MINUTE_1 = "MINUTE"
{6,0,7}, // 33 = WP_MINUTEN_1 = "MINUTEN"
{6,8,3}, // 34 = WP_VOR_1 = "VOR"
{6,11,3}, // 35 = WP_BIS = "BIS"
{6,14,4}, // 36 = WP_NACH_1 = "NACH"
{7,0,3}, // 37 = WP_UND_2 = "UND"
{7,3,4}, // 38 = WP_HALB = "HALB"
{7,7,11}, // 39 = WP_DREIVIERTEL = "DREIVIERTEL"
{7,11,7}, // 40 = WP_VIERTEL_2 = "VIERTEL"
{8,0,4}, // 41 = WP_SIEB_2 = "SIEB"
{8,0,6}, // 42 = WP_SIEBEN_2 = "SIEBEN"
{8,5,4}, // 43 = WP_NEUN_2 = "NEUN"
{8,8,4}, // 44 = WP_NULL_2 = "NULL"
{8,12,4}, // 45 = WP_ZWEI_2 = "ZWEI"
{8,14,3}, // 46 = WP_EIN_3 = "EIN"
{8,14,4}, // 47 = WP_EINE_2 = "EINE"
{9,0,4}, // 48 = WP_FUENF_2 = "FÜNF"
{9,4,4}, // 49 = WP_SECH_2 = "SECH"
{9,4,5}, // 50 = WP_SECHS_2 = "SECHS"
{9,9,4}, // 51 = WP_ACHT_2 = "ACHT"
{9,13,4}, // 52 = WP_VIER_2 = "VIER"
{10,0,4}, // 53 = WP_DREI_2 = "DREI"
{10,2,3}, // 54 = WP_EIN_4 = "EIN"
{10,2,4}, // 55 = WP_EINS_2 = "EINS"
{10,6,3}, // 56 = WP_UND_3 = "UND"
{10,10,3}, // 57 = WP_ELF_2 = "ELF"
{10,14,4}, // 58 = WP_ZEHN_2 = "ZEHN"
{11,0,7}, // 59 = WP_ZWANZIG_2 = "ZWANZIG"
{11,10,8}, // 60 = WP_DREISSIG_2 = "DREISSIG"
{12,0,7}, // 61 = WP_VIERZIG_2 = "VIERZIG"
{12,7,5}, // 62 = WP_ZWOELF_2 = "ZWÖLF"
{12,11,7}, // 63 = WP_FUENFZIG_2 = "FÜNFZIG"
{13,0,6}, // 64 = WP_MINUTE_2 = "MINUTE"
{13,0,7}, // 65 = WP_MINUTEN_2 = "MINUTEN"
{13,7,3}, // 66 = WP_UHR_2 = "UHR"
{13,11,4}, // 67 = WP_FRUEH = "FRÜH"
{13,15,3}, // 68 = WP_VOR_2 = "VOR"
{14,0,6}, // 69 = WP_ABENDS = "ABENDS"
{14,6,11}, // 70 = WP_MITTERNACHT = "MITTERNACHT"
{14,12,4}, // 71 = WP_NACH_2 = "NACH"
{14,12,6}, // 72 = WP_NACHTS = "NACHTS"
{15,0,7}, // 73 = WP_MORGENS = "MORGENS"
{15,7,1}, // 74 = WP_LED_1 = "._1"
{15,8,1}, // 75 = WP_LED_2 = "._2"
{15,9,1}, // 76 = WP_LED_3 = "._3"
{15,10,1}, // 77 = WP_LED_4 = "._4"
{15,11,7}, // 78 = WP_MITTAG_S = "MITTAGS"
}};
#endif
