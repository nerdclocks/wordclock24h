/*-------------------------------------------------------------------------------------------------------------------------------------------
 * rtc.c - DS3231 & DS1307 RTC routines
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#include "rtc.h"
#include "i2c.h"

#define DS1307_OR_DS3231_ADDR   0xD0                            // I2C address
#define FIRST_TIME_REG          0x00                            // address of first time register

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * DS1307 addresses and control registers
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#define DS1307_LAST_RAM_ADDR    0x3F                            // Last ram address of DS1307

#define DS1307_CTRL_OUT         0x80                            // Output Level
#define DS1307_CTRL_SQWE        0x10                            // Square Wave Enable
#define DS1307_CTRL_RS1         0x02                            // Rate Select RS2/RS1: 00 =    1Hz, 01 = 1024Hz,
#define DS1307_CTRL_RS2         0x01                            // Rate Select RS1/RS1: 10 = 4096Hz, 11 = 8182Hz

#define DS1307_CTRL_REG         0x07                            // address of control register
#define DS1307_CTRL_DEFAULT     0x00                            // default value: all bits reset

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * DS3231 addresses and control registers
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#define DS3231_CTRL_EOSC        0x80                            // Enable Oscillator, active low
#define DS3231_CTRL_BBSQW       0x40                            // Battery-Backed Square-Wave Enable
#define DS3231_CTRL_CONV        0x20                            // Convert Temperature
#define DS3231_CTRL_RS2         0x10                            // Rate Select RS2/RS1: 00 =    1Hz, 01 = 1024Hz,
#define DS3231_CTRL_RS1         0x08                            // Rate Select RS1/RS1: 10 = 4096Hz, 11 = 8182Hz
#define DS3231_CTRL_INTCN       0x04                            // Interrupt Control
#define DS3231_CTRL_A2IE        0x02                            // Alarm 2 Interrupt Enable
#define DS3231_CTRL_A1IE        0x01                            // Alarm 1 Interrupt Enable

#define DS3231_CTRL_REG         0x0E                            // address of control register
#define DS3231_CTRL_DEFAULT     0x00                            // default value: all bits reset

#define DS3231_TEMP_REG_HI      0x11                            // 8 upper bytes: integer part
#define DS3231_TEMP_REG_LO      0x12                            // 2 lower bytes: fractional part 0x00=0.00°, 0x01=0.25°, ... 0x03=0.75°

#define INT_TO_BCD(x)           (((x) / 10) << 4) + ((x) % 10)
#define BCD_TO_INT(x)           (10 * ((x) >> 4) + ((x) & 0x0F))

uint_fast8_t                    rtc_is_up = 0;

static uint_fast8_t             is_ds1307;

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: read data
 *
 * Return values:
 *  0   Failed
 *  1   Successful
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
rtc_read (uint_fast8_t start_addr, uint8_t * buffer, uint_fast8_t cnt)
{
    int rtc;

    if (i2c_read (DS1307_OR_DS3231_ADDR, start_addr, 0, buffer, cnt) == I2C_OK)
    {
        rtc = 1;
    }
    else
    {
        rtc = 0;
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: write data
 *
 * Return values:
 *  0   Failed
 *  1   Successful
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
rtc_write (uint_fast8_t start_addr, uint8_t * buffer, uint_fast8_t cnt)
{
    int rtc;

    if (i2c_write (DS1307_OR_DS3231_ADDR, start_addr, 0, buffer, cnt) == I2C_OK)
    {
        rtc = 1;
    }
    else
    {
        rtc = 0;
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * initialize I2C and RTC
 *
 * Return values:
 *  0   Failed
 *  1   Successful
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
rtc_init (void)
{
    int rtc;
    uint8_t value;

    i2c_init ();

    rtc = rtc_read (DS1307_LAST_RAM_ADDR, &value, 1);                   // test read of last ram address of DS1307

    if (rtc)
    {
        is_ds1307 = 1;                                                  // RTC is DS1307

        rtc = rtc_read (DS1307_CTRL_REG, &value, 1);                    // read control register

        if (rtc)
        {
            if (value != DS1307_CTRL_DEFAULT)
            {
                value = DS1307_CTRL_DEFAULT;
                rtc_write (DS1307_CTRL_REG, &value, 1);
            }

            rtc = rtc_read (FIRST_TIME_REG, &value, 1);                 // read first time keeping register

            if (rtc)
            {
                if (value & 0x80)                                       // CH bit set, RTC is stopped
                {
                    value &= ~0x80;                                     // clear CH bit, start RTC
                    rtc_write (FIRST_TIME_REG, &value, 1);
                }

                rtc_is_up = 1;
            }
        }
    }
    else
    {
        rtc = rtc_read (DS3231_CTRL_REG, &value, 1);                    // test read of control register of DS3231

        if (rtc)
        {
            if (value != DS3231_CTRL_DEFAULT)
            {
                value = DS3231_CTRL_DEFAULT;
                rtc_write (DS3231_CTRL_REG, &value, 1);
            }

            rtc_is_up = 1;
        }
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * set date & time
 *
 * Return values:
 *  0   Failed
 *  1   Successful
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
rtc_set_date_time (struct tm * tmp)
{
    uint8_t         buffer[7];
    uint_fast8_t    rtc;

    if (rtc_is_up)
    {
        buffer[0] = INT_TO_BCD(tmp->tm_sec);
        buffer[1] = INT_TO_BCD(tmp->tm_min);
        buffer[2] = INT_TO_BCD(tmp->tm_hour);
        buffer[3] = tmp->tm_wday + 1;
        buffer[4] = INT_TO_BCD(tmp->tm_mday);
        buffer[5] = INT_TO_BCD(tmp->tm_mon + 1);
        buffer[6] = INT_TO_BCD(tmp->tm_year - 100);

        rtc = rtc_write (FIRST_TIME_REG, buffer, 7);
    }
    else
    {
        rtc = 0;
    }
    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * get date & time
 *
 * Return values:
 *  0   Failed
 *  1   Successful
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
rtc_get_date_time (struct tm * tmp)
{
    uint8_t         buffer[7];
    uint_fast8_t    rtc;

    if (rtc_is_up)
    {
        rtc = rtc_read (FIRST_TIME_REG, buffer, 7);

        if (rtc)
        {
            tmp->tm_sec  = BCD_TO_INT(buffer[0]);
            tmp->tm_min  = BCD_TO_INT(buffer[1]);
            tmp->tm_hour = BCD_TO_INT(buffer[2]);
            tmp->tm_wday = buffer[3] - 1;
            tmp->tm_mday = BCD_TO_INT(buffer[4]);
            tmp->tm_mon  = BCD_TO_INT(buffer[5]) - 1;
            tmp->tm_year = BCD_TO_INT(buffer[6]) + 100;
        }
    }
    else
    {
        rtc = 0;
    }
    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * get temperature index
 *
 * Return values:
 *    index =   0 ->   0°C
 *    index = 250 -> 125°C
 *    index = 255 -> Error
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
rtc_get_temperature_index (void)
{
    uint8_t         buffer[2];
    uint_fast8_t    index = 0xFF;

    if (rtc_is_up)
    {
        index = rtc_read (DS3231_TEMP_REG_HI, buffer, 2);

        if (index)
        {
            index = (buffer[0] << 1) | ((buffer[1] & 0x02) >> 1);     // multiply integer part by 2, add 1 if fractional part >= 2
        }
    }
    return index;
}
