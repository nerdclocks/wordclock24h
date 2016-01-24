/*-------------------------------------------------------------------------------------------------------------------------------------------
 * remote-ir.c - remote IR routines using IRMP
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
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
#include "mcurses.h"
#include "monitor.h"
#include "display.h"
#include "eeprom.h"
#include "eeprom-data.h"

#ifndef DEBUG

static  IRMP_DATA   irmp_data_array[N_REMOTE_IR_CMDS];

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * search for a previously stored IR command
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static uint_fast8_t
search_cmd (IRMP_DATA * ip, uint_fast8_t max_cmds)
{
    uint_fast8_t    rtc = REMOTE_IR_CMD_INVALID;
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
    uint_fast8_t            rtc = REMOTE_IR_CMD_INVALID;

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
        rtc = search_cmd (&irmp_data, N_REMOTE_IR_CMDS);
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
    char *          s;
    char *          dec;
    char *          inc;
    uint_fast8_t    rtc = 1;
    uint_fast8_t    i;

    s = "IRMP: press key for ";
    dec = "decrement ";
    inc = "increment ";

    for (i = 0; i < N_REMOTE_IR_CMDS; i++)
    {
        display_reset_led_states ();                            // switch LEDs off
        display_led_on (0, i);                                  // switch one LED on

        if (mcurses_is_up)
        {
            monitor_show_all_letters_off ();                    // show table off
            monitor_show_letter (0, i, 1);                      // switch on letter on
            attrset (A_NORMAL);
            move (LOG_LINE, LOG_COL);

            switch (i)
            {
                case REMOTE_IR_CMD_POWER:                         addstr (s);                 addstr ("power off/on");            break;
                case REMOTE_IR_CMD_OK:                            addstr (s);                 addstr ("OK");                      break;
                case REMOTE_IR_CMD_DECREMENT_DISPLAY_MODE:        addstr (s); addstr (dec);   addstr ("display mode");            break;
                case REMOTE_IR_CMD_INCREMENT_DISPLAY_MODE:        addstr (s); addstr (inc);   addstr ("display mode");            break;
                case REMOTE_IR_CMD_DECREMENT_ANIMATION_MODE:      addstr (s); addstr (dec);   addstr ("animation mode");          break;
                case REMOTE_IR_CMD_INCREMENT_ANIMATION_MODE:      addstr (s); addstr (inc);   addstr ("animation mode");          break;
                case REMOTE_IR_CMD_DECREMENT_HOUR:                addstr (s); addstr (dec);   addstr ("hour");                    break;
                case REMOTE_IR_CMD_INCREMENT_HOUR:                addstr (s); addstr (inc);   addstr ("hour");                    break;
                case REMOTE_IR_CMD_DECREMENT_MINUTE:              addstr (s); addstr (dec);   addstr ("minute");                  break;
                case REMOTE_IR_CMD_INCREMENT_MINUTE:              addstr (s); addstr (inc);   addstr ("minute");                  break;
                case REMOTE_IR_CMD_DECREMENT_BRIGHTNESS_RED:      addstr (s); addstr (dec);   addstr ("red brightness");          break;
                case REMOTE_IR_CMD_INCREMENT_BRIGHTNESS_RED:      addstr (s); addstr (inc);   addstr ("red brightness");          break;
                case REMOTE_IR_CMD_DECREMENT_BRIGHTNESS_GREEN:    addstr (s); addstr (dec);   addstr ("green brightness");        break;
                case REMOTE_IR_CMD_INCREMENT_BRIGHTNESS_GREEN:    addstr (s); addstr (inc);   addstr ("green brightness");        break;
                case REMOTE_IR_CMD_DECREMENT_BRIGHTNESS_BLUE:     addstr (s); addstr (dec);   addstr ("blue brightness");         break;
                case REMOTE_IR_CMD_INCREMENT_BRIGHTNESS_BLUE:     addstr (s); addstr (inc);   addstr ("blue brightness");         break;
                case REMOTE_IR_CMD_DECREMENT_BRIGHTNESS:          addstr (s); addstr (dec);   addstr ("global brightness");       break;
                case REMOTE_IR_CMD_INCREMENT_BRIGHTNESS:          addstr (s); addstr (inc);   addstr ("global brightness");       break;
                case REMOTE_IR_CMD_AUTO_BRIGHTNESS_CONTROL:       addstr (s);                 addstr ("toggle auto brightness");  break;
                case REMOTE_IR_CMD_GET_TEMPERATURE:               addstr (s);                 addstr ("get temperature");         break;
            }

            clrtoeol ();
            refresh ();
        }

        while (1)
        {
            display_animation ();

            if (irmp_get_data (&irmp_data_array[i]))                                            // read ir data
            {
                if ((irmp_data_array[i].flags & IRMP_FLAG_REPETITION) == 0)                     // no repetition
                {
                    if (i == 0 || search_cmd (&irmp_data_array[i], i) == REMOTE_IR_CMD_INVALID) // not stored previously
                    {
                        break;                                                                  // it's okay, next command...
                    }
                }
            }
        }
    }

    display_reset_led_states ();                                                                // switch LEDs off
    display_animation ();

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

        for (i = 0; i < N_REMOTE_IR_CMDS; i++)
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

        for (i = 0; i < N_REMOTE_IR_CMDS; i++)
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

#endif
