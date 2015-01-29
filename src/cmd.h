/*-------------------------------------------------------------------------------------------------------------------------------------------
 * cmd.h - declaration of interactive commands
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef CMD_H
#define CMD_H

#define CMD_INVALID                     0xFF

#define CMD_POWER                        0
#define CMD_OK                           1

#define CMD_DECREMENT_MODE               2
#define CMD_INCREMENT_MODE               3

#define CMD_DECREMENT_HOUR               4
#define CMD_INCREMENT_HOUR               5

#define CMD_DECREMENT_MINUTE             6
#define CMD_INCREMENT_MINUTE             7

#define CMD_DECREMENT_BRIGHTNESS_RED     8
#define CMD_INCREMENT_BRIGHTNESS_RED     9

#define CMD_DECREMENT_BRIGHTNESS_GREEN  10
#define CMD_INCREMENT_BRIGHTNESS_GREEN  11

#define CMD_DECREMENT_BRIGHTNESS_BLUE   12
#define CMD_INCREMENT_BRIGHTNESS_BLUE   13

#define CMD_GET_NET_TIME                14

#define N_CMDS                          15

#endif
