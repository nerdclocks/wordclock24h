/*-------------------------------------------------------------------------------------------------------------------------------------------
 * eeprom-data.h - data structure of EEPROM
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef EEPROM_DATA_H
#define EEPROM_DATA_H

#define EEPROM_VERSION                      0x00010100          // version 1.1.0

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * Some packed structures to minimize used EEPROM space
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
typedef struct __attribute__ ((__packed__))                     // packed IRMP structure without origin member "flags"
{
    uint8_t     protocol;
    uint16_t    address;
    uint16_t    command;
} PACKED_IRMP_DATA;

typedef struct __attribute__ ((__packed__))
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} PACKED_RGB_BRIGHTNESS;


/*-------------------------------------------------------------------------------------------------------------------------------------------
 * Sizes:
 *
 *      EEPROM Version        4 Bytes   ( 1 *  4)
 *      IRMP data           160 Bytes   (32 *  5)
 *      RGB brightness        3 Bytes   ( 3 *  1)
 *      Display mode          1 Byte    ( 1 *  1)
 *      Animation mode        1 Byte    ( 1 *  1)
 *      NTP protocol flag     1 Byte    ( 1 *  1)
 *      Time server          16 Bytes   ( 1 * 16)
 *      Time zone             2 Bytes   ( 1 *  2)
 *      =========================================
 *      Sum                 187 Bytes
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */

#define EEPROM_MAX_IR_CODES                 32                  // max IR codes, should be greater or equal than N_CMDS
#if EEPROM_MAX_IR_CODES < N_CMDS
#error value for EEPROM_MAX_IR_CODES is too low
#endif
#define EEPROM_MAX_IPADDR_LEN               16                  // max length of IP address xxx.xxx.xxx.xxx + '\0'
#define EEPROM_MAX_TIMEZONE_LEN              2                  // max length of timezone: one byte for +/-, one byte for hour offset

#define EEPROM_DATA_SIZE_VERSION            sizeof (uint32_t)
#define EEPROM_DATA_SIZE_IRMP_DATA          (EEPROM_MAX_IR_CODES * sizeof (PACKED_IRMP_DATA))
#define EEPROM_DATA_SIZE_RGB_BRIGHTNESS     sizeof (PACKED_RGB_BRIGHTNESS)
#define EEPROM_DATA_SIZE_DISPLAY_MODE       sizeof (uint8_t)
#define EEPROM_DATA_SIZE_ANIMATION_MODE     sizeof (uint8_t)
#define EEPROM_DATA_SIZE_NTP_PROTOCOL       sizeof (uint8_t)
#define EEPROM_DATA_SIZE_TIMESERVER         (EEPROM_MAX_IPADDR_LEN)
#define EEPROM_DATA_SIZE_TIMEZONE           (EEPROM_MAX_TIMEZONE_LEN)
#define EEPROM_DATA_SIZE_DS18XX_TYPE        sizeof (uint8_t)

#define EEPROM_DATA_OFFSET_VERSION          0
#define EEPROM_DATA_OFFSET_IRMP_DATA        (EEPROM_DATA_OFFSET_VERSION         + EEPROM_DATA_SIZE_VERSION)
#define EEPROM_DATA_OFFSET_RGB_BRIGHTNESS   (EEPROM_DATA_OFFSET_IRMP_DATA       + EEPROM_DATA_SIZE_IRMP_DATA)
#define EEPROM_DATA_OFFSET_DISPLAY_MODE     (EEPROM_DATA_OFFSET_RGB_BRIGHTNESS  + EEPROM_DATA_SIZE_RGB_BRIGHTNESS)
#define EEPROM_DATA_OFFSET_ANIMATION_MODE   (EEPROM_DATA_OFFSET_DISPLAY_MODE    + EEPROM_DATA_SIZE_DISPLAY_MODE)
#define EEPROM_DATA_OFFSET_NTP_PROTOCOL     (EEPROM_DATA_OFFSET_ANIMATION_MODE  + EEPROM_DATA_SIZE_ANIMATION_MODE)
#define EEPROM_DATA_OFFSET_TIMESERVER       (EEPROM_DATA_OFFSET_NTP_PROTOCOL    + EEPROM_DATA_SIZE_NTP_PROTOCOL)
#define EEPROM_DATA_OFFSET_TIMEZONE         (EEPROM_DATA_OFFSET_TIMESERVER      + EEPROM_DATA_SIZE_TIMESERVER)

#endif
