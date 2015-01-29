/*-------------------------------------------------------------------------------------------------------------------------------------------
 * remote-ir.c - remote IR routines using IRMP
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#include "wclock24h-config.h"
#include "irmp.h"
#include "remote-ir.h"
#include "cmd.h"
#include "mcurses.h"
#include "monitor.h"
#include "dsp.h"
#include "eeprom.h"
#include "eeprom-data.h"

static  IRMP_DATA   irmp_data_array[N_CMDS];

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * search for a previously stored IR command
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
search_cmd (IRMP_DATA * ip, uint_fast8_t max_cmds)
{
    uint_fast8_t    rtc = CMD_INVALID;
    uint_fast8_t    i;

    for (i = 0; i < max_cmds; i++)
    {
        if (ip->protocol == irmp_data_array[i].protocol &&
            ip->address  == irmp_data_array[i].address  &&
            ip->command  == irmp_data_array[i].command)
        {
            rtc = i;
            break;
        }
    }
    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * read an IR command
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
remote_ir_get_cmd (void)
{
    static uint_fast16_t    frame;
    static uint_fast8_t     last_repetition_flag;
    IRMP_DATA               irmp_data;
    uint_fast8_t            rtc = CMD_INVALID;

    if (irmp_get_data (&irmp_data))
    {
        frame++;

        if (mcurses_is_up)
        {
            mvprintw (IRMP_FRAME_LINE, IRMP_FRAME_COL, "%3d", frame);
        }

        if (last_repetition_flag == 0 && (irmp_data.flags & IRMP_FLAG_REPETITION))  // ignore first repetition frame
        {
            last_repetition_flag = 1;
            return rtc;
        }

        last_repetition_flag = (irmp_data.flags & IRMP_FLAG_REPETITION) ? 1: 0;
        rtc = search_cmd (&irmp_data, N_CMDS);
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * learn remote IR control
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
remote_ir_learn (void)
{
    uint_fast8_t rtc = 1;
    uint_fast8_t i;

    for (i = 0; i < N_CMDS; i++)
    {
        dsp_all_leds_off ();                                    // switch LEDs off
        dsp_led_on (0, i);                                      // switch one LED on

        if (mcurses_is_up)
        {
            monitor_show_all_letters_off ();                    // show table off
            monitor_show_letter (0, i, 1);                      // switch on letter on
            attrset (A_NORMAL);
            move (LOG_LINE, LOG_COL);

            switch (i)
            {
                case CMD_POWER:                         addstr ("IRMP: press key for power off/on");                break;
                case CMD_OK:                            addstr ("IRMP: press key for OK");                          break;
                case CMD_DECREMENT_MODE:                addstr ("IRMP: press key for decrement mode");              break;
                case CMD_INCREMENT_MODE:                addstr ("IRMP: press key for increment mode");              break;
                case CMD_DECREMENT_HOUR:                addstr ("IRMP: press key for decrement hour");              break;
                case CMD_INCREMENT_HOUR:                addstr ("IRMP: press key for increment hour");              break;
                case CMD_DECREMENT_MINUTE:              addstr ("IRMP: press key for decrement minute");            break;
                case CMD_INCREMENT_MINUTE:              addstr ("IRMP: press key for increment minute");            break;
                case CMD_DECREMENT_BRIGHTNESS_RED:      addstr ("IRMP: press key for decrement red brightness");    break;
                case CMD_INCREMENT_BRIGHTNESS_RED:      addstr ("IRMP: press key for increment red brightness");    break;
                case CMD_DECREMENT_BRIGHTNESS_GREEN:    addstr ("IRMP: press key for decrement green brightness");  break;
                case CMD_INCREMENT_BRIGHTNESS_GREEN:    addstr ("IRMP: press key for increment green brightness");  break;
                case CMD_DECREMENT_BRIGHTNESS_BLUE:     addstr ("IRMP: press key for decrement blue brightness");   break;
                case CMD_INCREMENT_BRIGHTNESS_BLUE:     addstr ("IRMP: press key for increment blue brightness");   break;
                case CMD_GET_NET_TIME:                  addstr ("IRMP: press key for get net time");                break;
            }

            clrtoeol ();
            refresh ();
        }

        while (1)
        {
            dsp_fade (0);

            if (irmp_get_data (&irmp_data_array[i]))                                            // read ir data
            {
                if ((irmp_data_array[i].flags & IRMP_FLAG_REPETITION) == 0)                     // no repetition
                {
                    if (i == 0 || search_cmd (&irmp_data_array[i], i) == CMD_INVALID)           // not stored previously
                    {
                        break;                                                                  // it's okay, next command...
                    }
                }
            }
        }
    }

    dsp_all_leds_off ();                                        // switch LEDs off
    dsp_fade (0);

    if (mcurses_is_up)
    {
        monitor_show_all_letters_off ();
        move (LOG_LINE, LOG_COL);
        clrtoeol ();
        refresh ();
    }
    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * read IR codes from EEPROM
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
remote_ir_read_codes_from_eeprom (void)
{
    PACKED_IRMP_DATA    packed_irmp_data;
    uint_fast8_t        rtc = 0;

    if (eeprom_is_up)
    {
        uint_fast8_t    i;
        uint_fast16_t   start_addr = EEPROM_DATA_OFFSET_IRMP_DATA;

        for (i = 0; i < N_CMDS; i++)
        {
            rtc = eeprom_read (start_addr, (uint8_t *) &packed_irmp_data, sizeof(PACKED_IRMP_DATA));

            irmp_data_array[i].protocol = packed_irmp_data.protocol;
            irmp_data_array[i].address  = packed_irmp_data.address;
            irmp_data_array[i].command  = packed_irmp_data.command;

            start_addr += sizeof(PACKED_IRMP_DATA);
        }
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * write IR codes to EEPROM
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast8_t
remote_ir_write_codes_to_eeprom (void)
{
    PACKED_IRMP_DATA    packed_irmp_data;
    uint_fast8_t        rtc = 0;

    if (eeprom_is_up)
    {
        uint_fast8_t    i;
        uint_fast16_t   start_addr = EEPROM_DATA_OFFSET_IRMP_DATA;

        for (i = 0; i < N_CMDS; i++)
        {
            packed_irmp_data.protocol   = irmp_data_array[i].protocol;
            packed_irmp_data.address    = irmp_data_array[i].address;
            packed_irmp_data.command    = irmp_data_array[i].command;

            rtc = eeprom_write (start_addr, (uint8_t *) &packed_irmp_data, sizeof(PACKED_IRMP_DATA));
            start_addr += sizeof(PACKED_IRMP_DATA);
        }
    }

    return rtc;
}
