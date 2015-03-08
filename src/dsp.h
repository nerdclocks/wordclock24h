/*-------------------------------------------------------------------------------------------------------------------------------------------
 * dsp.h - interface declaration of display routines
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef DSP_H
#define DSP_H

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RGB_BRIGHTNESS;

extern char *       animation_modes[];

extern void         reset_led_states (void);
extern void         dsp_led_on (uint_fast8_t, uint_fast8_t);
extern void         dsp_temperature (uint_fast8_t, uint_fast8_t);
extern void         dsp_clock (uint_fast8_t, uint_fast8_t, uint_fast8_t);
extern void         dsp_animation (void);
extern uint_fast8_t dsp_get_display_mode (void);
extern uint_fast8_t dsp_set_display_mode (uint_fast8_t);
extern uint_fast8_t dsp_increment_display_mode (void);
extern uint_fast8_t dsp_decrement_display_mode (void);
extern uint_fast8_t dsp_get_animation_mode (void);
extern uint_fast8_t dsp_set_animation_mode (uint_fast8_t);
extern uint_fast8_t dsp_increment_animation_mode (void);
extern uint_fast8_t dsp_decrement_animation_mode (void);
extern void         dsp_increment_brightness_red (void);
extern void         dsp_decrement_brightness_red (void);
extern void         dsp_increment_brightness_green (void);
extern void         dsp_decrement_brightness_green (void);
extern void         dsp_increment_brightness_blue (void);
extern void         dsp_decrement_brightness_blue (void);
extern void         dsp_set_brightness (RGB_BRIGHTNESS *);
extern uint_fast8_t dsp_read_config_from_eeprom (void);
extern uint_fast8_t dsp_write_config_to_eeprom (void);
extern void         dsp_init (void);

#endif
