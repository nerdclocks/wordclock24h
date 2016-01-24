/*-------------------------------------------------------------------------------------------------------------------------------------------
 * display.h - interface declaration of LED display routines
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef DSP_H
#define DSP_H

#define MAX_BRIGHTNESS                      15

// display flags:
#define DISPLAY_FLAG_NONE                   0                       // nothing to do
#define DISPLAY_FLAG_UPDATE_MINUTES         1                       // update minute LEDs
#define DISPLAY_FLAG_UPDATE_NO_ANIMATION    2                       // update without animation, e.g. colors
#define DISPLAY_FLAG_UPDATE_ALL             3                       // update complete display

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} DSP_COLORS;

extern char *       animation_modes[];

void                display_set_status_led (uint_fast8_t, uint_fast8_t, uint_fast8_t);
extern void         display_reset_led_states (void);
extern void         display_led_on (uint_fast8_t, uint_fast8_t);
extern void         display_temperature (uint_fast8_t, uint_fast8_t);
extern void         display_clock (uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t);
extern void         display_animation (void);
extern uint_fast8_t display_get_display_mode (void);
extern uint_fast8_t display_set_display_mode (uint_fast8_t);
extern uint_fast8_t display_increment_display_mode (void);
extern uint_fast8_t display_decrement_display_mode (void);
extern uint_fast8_t display_get_automatic_brightness_control (void);
extern void         display_set_automatic_brightness_control (uint_fast8_t);
extern uint_fast8_t display_get_animation_mode (void);
extern uint_fast8_t display_set_animation_mode (uint_fast8_t);
extern uint_fast8_t display_increment_animation_mode (void);
extern uint_fast8_t display_decrement_animation_mode (void);
extern void         display_increment_color_red (void);
extern void         display_decrement_color_red (void);
extern void         display_increment_color_green (void);
extern void         display_decrement_color_green (void);
extern void         display_increment_color_blue (void);
extern void         display_decrement_color_blue (void);
extern void         display_get_colors (DSP_COLORS *);
extern void         display_set_colors (DSP_COLORS *);
extern uint_fast8_t display_get_brightness (void);
extern void         display_set_brightness  (uint_fast8_t);
extern void         display_decrement_brightness (void);
extern void         display_increment_brightness (void);
extern void         display_test (void);
extern void         display_message (char *);
extern void         display_banner (char *);
extern uint_fast8_t display_read_config_from_eeprom (void);
extern uint_fast8_t display_write_config_to_eeprom (void);
extern void         display_init (void);

#endif
